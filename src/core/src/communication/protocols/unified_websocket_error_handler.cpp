#include "hydrogen/core/unified_websocket_error_handler.h"

#ifdef HYDROGEN_HAS_WEBSOCKETS
#include <algorithm>
#include <iomanip>
#include <random>
#include <spdlog/spdlog.h>
#include <sstream>

namespace hydrogen {
namespace core {

// EnhancedWebSocketError implementation
json EnhancedWebSocketError::toDetailedJson() const {
  json j = json::object();

  // Base error information
  j["errorId"] = errorId;
  j["errorCode"] = errorCode;
  j["message"] = message;
  j["details"] = details;
  j["category"] = static_cast<int>(category);
  j["severity"] = static_cast<int>(severity);
  j["recommendedAction"] = static_cast<int>(recommendedAction);
  j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                       timestamp.time_since_epoch())
                       .count();
  j["component"] = component;
  j["operation"] = operation;
  j["context"] = context;

  // Enhanced information
  j["connectionContext"] = {
      {"connectionId", connectionContext.connectionId},
      {"componentName", connectionContext.componentName},
      {"endpoint", connectionContext.endpoint},
      {"isClient", connectionContext.isClient},
      {"connectionStartTime",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           connectionContext.connectionStartTime.time_since_epoch())
           .count()},
      {"lastActivityTime",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           connectionContext.lastActivityTime.time_since_epoch())
           .count()},
      {"reconnectAttempts", connectionContext.reconnectAttempts},
      {"metadata", connectionContext.metadata}};

  j["correlationId"] = correlationId;
  j["errorChain"] = errorChain;
  j["isRetryable"] = isRetryable;
  j["suggestedRetryDelay"] = suggestedRetryDelay.count();

  return j;
}

bool EnhancedWebSocketError::shouldTriggerCircuitBreaker() const {
  // Circuit breaker should be triggered for severe connection or network errors
  return (category == WebSocketErrorCategory::CONNECTION &&
          severity >= WebSocketErrorSeverity::HIGH) ||
         (category == WebSocketErrorCategory::NETWORK &&
          severity >= WebSocketErrorSeverity::MEDIUM) ||
         (category == WebSocketErrorCategory::TIMEOUT &&
          severity >= WebSocketErrorSeverity::HIGH);
}

std::string EnhancedWebSocketError::getErrorFingerprint() const {
  // Create a unique fingerprint for error pattern analysis
  std::stringstream ss;
  ss << static_cast<int>(category) << ":" << static_cast<int>(severity) << ":"
     << errorCode << ":" << connectionContext.componentName;
  return ss.str();
}

// WebSocketCircuitBreaker implementation
WebSocketCircuitBreaker::WebSocketCircuitBreaker(
    const std::string &connectionId)
    : connectionId_(connectionId) {
  lastFailureTime_ = std::chrono::system_clock::now();
}

bool WebSocketCircuitBreaker::canAttemptConnection() const {
  std::lock_guard<std::mutex> lock(stateMutex_);

  switch (state_.load()) {
  case State::CLOSED:
    return true;
  case State::OPEN: {
    auto now = std::chrono::system_clock::now();
    auto timeSinceFailure =
        std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                              lastFailureTime_);
    return timeSinceFailure >= recoveryTimeout_;
  }
  case State::HALF_OPEN:
    return true;
  default:
    return false;
  }
}

void WebSocketCircuitBreaker::recordSuccess() {
  std::lock_guard<std::mutex> lock(stateMutex_);

  State currentState = state_.load();
  if (currentState == State::HALF_OPEN) {
    successCount_.fetch_add(1);
    if (successCount_.load() >= successThreshold_) {
      state_.store(State::CLOSED);
      failureCount_.store(0);
      successCount_.store(0);
      spdlog::info(
          "WebSocketCircuitBreaker: Connection {} recovered, circuit closed",
          connectionId_);
    }
  } else if (currentState == State::CLOSED) {
    // Reset failure count on successful operation
    failureCount_.store(0);
  }
}

void WebSocketCircuitBreaker::recordFailure() {
  std::lock_guard<std::mutex> lock(stateMutex_);

  failureCount_.fetch_add(1);
  lastFailureTime_ = std::chrono::system_clock::now();

  State currentState = state_.load();
  if (currentState == State::CLOSED &&
      failureCount_.load() >= failureThreshold_) {
    state_.store(State::OPEN);
    spdlog::warn("WebSocketCircuitBreaker: Connection {} circuit opened due to "
                 "{} failures",
                 connectionId_, failureCount_.load());
  } else if (currentState == State::HALF_OPEN) {
    state_.store(State::OPEN);
    successCount_.store(0);
    spdlog::warn("WebSocketCircuitBreaker: Connection {} circuit reopened "
                 "after failed recovery attempt",
                 connectionId_);
  }
}

void WebSocketCircuitBreaker::reset() {
  std::lock_guard<std::mutex> lock(stateMutex_);
  state_.store(State::CLOSED);
  failureCount_.store(0);
  successCount_.store(0);
  spdlog::info("WebSocketCircuitBreaker: Connection {} circuit manually reset",
               connectionId_);
}

// UnifiedWebSocketErrorHandler implementation
UnifiedWebSocketErrorHandler::UnifiedWebSocketErrorHandler()
    : protocolErrorMapper_(std::make_shared<ProtocolErrorMapper>()) {
  statistics_.lastErrorTime = std::chrono::system_clock::now();
  statistics_.lastRecoveryTime = std::chrono::system_clock::now();
}

void UnifiedWebSocketErrorHandler::registerConnection(
    const WebSocketConnectionContext &context) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);
  connections_[context.connectionId] = context;

  if (circuitBreakerEnabled_) {
    circuitBreakers_[context.connectionId] =
        std::make_shared<WebSocketCircuitBreaker>(context.connectionId);
  }

  spdlog::debug(
      "UnifiedWebSocketErrorHandler: Registered connection {} for component {}",
      context.connectionId, context.componentName);
}

void UnifiedWebSocketErrorHandler::unregisterConnection(
    const std::string &connectionId) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);
  connections_.erase(connectionId);
  circuitBreakers_.erase(connectionId);

  spdlog::debug("UnifiedWebSocketErrorHandler: Unregistered connection {}",
                connectionId);
}

void UnifiedWebSocketErrorHandler::updateConnectionActivity(
    const std::string &connectionId) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);
  auto it = connections_.find(connectionId);
  if (it != connections_.end()) {
    it->second.lastActivityTime = std::chrono::system_clock::now();
  }
}

void UnifiedWebSocketErrorHandler::handleError(const WebSocketError &error) {
  EnhancedWebSocketError enhancedError = enhanceError(error);
  handleEnhancedError(enhancedError);
}

void UnifiedWebSocketErrorHandler::handleEnhancedError(
    const EnhancedWebSocketError &error) {
  spdlog::error(
      "UnifiedWebSocketErrorHandler: Handling error {} in component {}: {}",
      error.errorId, error.component, error.message);

  // Check circuit breaker
  auto circuitBreaker = getCircuitBreaker(error.connectionContext.connectionId);
  if (circuitBreaker && !circuitBreaker->canAttemptConnection()) {
    spdlog::warn("UnifiedWebSocketErrorHandler: Circuit breaker open for "
                 "connection {}, skipping recovery",
                 error.connectionContext.connectionId);
    return;
  }

  // Determine recovery action
  WebSocketRecoveryAction action = determineRecoveryAction(error);

  // Execute recovery action
  bool recoverySuccessful = false;
  if (action != WebSocketRecoveryAction::NONE) {
    recoverySuccessful =
        executeRecoveryAction(error.connectionContext.connectionId, action);

    // Update circuit breaker
    if (circuitBreaker) {
      if (recoverySuccessful) {
        circuitBreaker->recordSuccess();
      } else {
        circuitBreaker->recordFailure();
      }
    }
  }

  // Update statistics
  updateStatistics(error, action, recoverySuccessful);

  // Create and notify error event
  WebSocketErrorEvent event;
  event.error = error;
  event.actionTaken = action;
  event.recoverySuccessful = recoverySuccessful;
  event.eventTime = std::chrono::system_clock::now();
  event.eventId = generateEventId();

  notifyErrorEvent(event);

  // Store for correlation if correlation ID is provided
  if (!error.correlationId.empty()) {
    correlateError(error.correlationId, error);
  }
}

WebSocketRecoveryAction UnifiedWebSocketErrorHandler::determineRecoveryAction(
    const WebSocketError &error) {
  // Use custom recovery strategy if provided
  if (recoveryStrategyCallback_) {
    try {
      EnhancedWebSocketError enhancedError = enhanceError(error);
      return recoveryStrategyCallback_(enhancedError);
    } catch (const std::exception &e) {
      spdlog::error("UnifiedWebSocketErrorHandler: Error in recovery strategy "
                    "callback: {}",
                    e.what());
    }
  }

  // Use recommended action if available
  if (error.recommendedAction != WebSocketRecoveryAction::NONE) {
    return error.recommendedAction;
  }

  // Default recovery logic based on error category and severity
  switch (error.category) {
  case WebSocketErrorCategory::CONNECTION:
    return (error.severity >= WebSocketErrorSeverity::MEDIUM)
               ? WebSocketRecoveryAction::RECONNECT
               : WebSocketRecoveryAction::RETRY;
  case WebSocketErrorCategory::TIMEOUT:
    return WebSocketRecoveryAction::RETRY;
  case WebSocketErrorCategory::PROTOCOL:
    return (error.severity >= WebSocketErrorSeverity::HIGH)
               ? WebSocketRecoveryAction::RECONNECT
               : WebSocketRecoveryAction::RESET;
  case WebSocketErrorCategory::MESSAGE:
    return WebSocketRecoveryAction::NONE;
  case WebSocketErrorCategory::NETWORK:
    return WebSocketRecoveryAction::RECONNECT;
  case WebSocketErrorCategory::AUTHENTICATION:
    return WebSocketRecoveryAction::TERMINATE;
  default:
    return WebSocketRecoveryAction::RETRY;
  }
}

bool UnifiedWebSocketErrorHandler::shouldRetry(const WebSocketError &error,
                                               int attemptCount) {
  RetryPolicy policy = getRetryPolicy(error.component);

  if (attemptCount >= policy.maxAttempts) {
    return false;
  }

  if (!error.isRecoverable()) {
    return false;
  }

  // Don't retry authentication errors
  if (error.category == WebSocketErrorCategory::AUTHENTICATION) {
    return false;
  }

  // Don't retry critical errors
  if (error.severity == WebSocketErrorSeverity::CRITICAL) {
    return false;
  }

  return true;
}

std::chrono::milliseconds
UnifiedWebSocketErrorHandler::getRetryDelay(const WebSocketError &error,
                                            int attemptCount) {
  RetryPolicy policy = getRetryPolicy(error.component);

  if (!policy.exponentialBackoff) {
    return policy.baseDelay;
  }

  // Exponential backoff with jitter
  auto delay = policy.baseDelay * (1 << attemptCount);
  delay = std::min(delay, policy.maxDelay);

  // Add jitter (±25%)
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_real_distribution<> jitter(0.75, 1.25);

  delay = std::chrono::milliseconds(
      static_cast<long long>(delay.count() * jitter(gen)));

  return delay;
}

void UnifiedWebSocketErrorHandler::correlateError(
    const std::string &correlationId, const EnhancedWebSocketError &error) {
  std::lock_guard<std::mutex> lock(correlationMutex_);
  correlatedErrors_[correlationId].push_back(error);

  // Clean up old correlations periodically
  cleanupOldCorrelations();
}

std::vector<EnhancedWebSocketError>
UnifiedWebSocketErrorHandler::getCorrelatedErrors(
    const std::string &correlationId) const {
  std::lock_guard<std::mutex> lock(correlationMutex_);
  auto it = correlatedErrors_.find(correlationId);
  return (it != correlatedErrors_.end())
             ? it->second
             : std::vector<EnhancedWebSocketError>{};
}

std::shared_ptr<WebSocketCircuitBreaker>
UnifiedWebSocketErrorHandler::getCircuitBreaker(
    const std::string &connectionId) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);
  auto it = circuitBreakers_.find(connectionId);
  return (it != circuitBreakers_.end()) ? it->second : nullptr;
}

void UnifiedWebSocketErrorHandler::resetCircuitBreaker(
    const std::string &connectionId) {
  auto circuitBreaker = getCircuitBreaker(connectionId);
  if (circuitBreaker) {
    circuitBreaker->reset();
  }
}

void UnifiedWebSocketErrorHandler::setGlobalRetryPolicy(
    int maxAttempts, std::chrono::milliseconds baseDelay,
    bool exponentialBackoff) {
  globalRetryPolicy_.maxAttempts = maxAttempts;
  globalRetryPolicy_.baseDelay = baseDelay;
  globalRetryPolicy_.exponentialBackoff = exponentialBackoff;
}

void UnifiedWebSocketErrorHandler::setConnectionSpecificRetryPolicy(
    const std::string &connectionId, int maxAttempts,
    std::chrono::milliseconds baseDelay) {
  RetryPolicy policy;
  policy.maxAttempts = maxAttempts;
  policy.baseDelay = baseDelay;
  connectionRetryPolicies_[connectionId] = policy;
}

UnifiedWebSocketErrorHandler::UnifiedErrorStatistics
UnifiedWebSocketErrorHandler::getStatistics() const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  return statistics_;
}

void UnifiedWebSocketErrorHandler::resetStatistics() {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  statistics_ = UnifiedErrorStatistics{};
  statistics_.lastErrorTime = std::chrono::system_clock::now();
  statistics_.lastRecoveryTime = std::chrono::system_clock::now();
}

json UnifiedWebSocketErrorHandler::generateErrorReport(
    const std::string &connectionId) const {
  json report = json::object();

  {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    report["statistics"] = {
        {"totalErrors", statistics_.totalErrors},
        {"connectionErrors", statistics_.connectionErrors},
        {"protocolErrors", statistics_.protocolErrors},
        {"timeoutErrors", statistics_.timeoutErrors},
        {"messageErrors", statistics_.messageErrors},
        {"authenticationErrors", statistics_.authenticationErrors},
        {"networkErrors", statistics_.networkErrors},
        {"unknownErrors", statistics_.unknownErrors},
        {"retriesAttempted", statistics_.retriesAttempted},
        {"successfulRecoveries", statistics_.successfulRecoveries},
        {"failedRecoveries", statistics_.failedRecoveries},
        {"circuitBreakerTrips", statistics_.circuitBreakerTrips},
        {"averageRecoveryTime", statistics_.averageRecoveryTime}};

    if (!connectionId.empty()) {
      auto it = statistics_.errorsByConnection.find(connectionId);
      if (it != statistics_.errorsByConnection.end()) {
        report["connectionSpecific"] = {{"connectionId", connectionId},
                                        {"errorCount", it->second}};
      }
    } else {
      report["errorsByConnection"] = statistics_.errorsByConnection;
      report["errorsByComponent"] = statistics_.errorsByComponent;
    }
  }

  // Add circuit breaker status
  {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    json circuitBreakerStatus = json::array();
    for (const auto &[connId, breaker] : circuitBreakers_) {
      if (connectionId.empty() || connId == connectionId) {
        circuitBreakerStatus.push_back(
            {{"connectionId", connId},
             {"state", static_cast<int>(breaker->getState())},
             {"failureCount", breaker->getFailureCount()},
             {"lastFailureTime",
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  breaker->getLastFailureTime().time_since_epoch())
                  .count()}});
      }
    }
    report["circuitBreakers"] = circuitBreakerStatus;
  }

  return report;
}

std::vector<std::string>
UnifiedWebSocketErrorHandler::getTopErrorPatterns(size_t limit) const {
  std::lock_guard<std::mutex> lock(correlationMutex_);

  std::unordered_map<std::string, size_t> patternCounts;

  for (const auto &[correlationId, errors] : correlatedErrors_) {
    for (const auto &error : errors) {
      std::string pattern = error.getErrorFingerprint();
      patternCounts[pattern]++;
    }
  }

  std::vector<std::pair<std::string, size_t>> sortedPatterns(
      patternCounts.begin(), patternCounts.end());
  std::sort(sortedPatterns.begin(), sortedPatterns.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  std::vector<std::string> topPatterns;
  for (size_t i = 0; i < std::min(limit, sortedPatterns.size()); ++i) {
    topPatterns.push_back(sortedPatterns[i].first);
  }

  return topPatterns;
}

bool UnifiedWebSocketErrorHandler::isConnectionHealthy(
    const std::string &connectionId) const {
  auto circuitBreaker =
      const_cast<UnifiedWebSocketErrorHandler *>(this)->getCircuitBreaker(
          connectionId);
  if (circuitBreaker) {
    return circuitBreaker->getState() == WebSocketCircuitBreaker::State::CLOSED;
  }

  // If no circuit breaker, check error statistics
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  auto it = statistics_.errorsByConnection.find(connectionId);
  if (it != statistics_.errorsByConnection.end()) {
    // Consider connection unhealthy if it has more than 10 errors
    return it->second <= 10;
  }

  return true; // No errors recorded, assume healthy
}

EnhancedWebSocketError
UnifiedWebSocketErrorHandler::enhanceError(const WebSocketError &error) {
  EnhancedWebSocketError enhanced;

  // Copy base error information
  enhanced.errorId = error.errorId;
  enhanced.errorCode = error.errorCode;
  enhanced.message = error.message;
  enhanced.details = error.details;
  enhanced.category = error.category;
  enhanced.severity = error.severity;
  enhanced.recommendedAction = error.recommendedAction;
  enhanced.timestamp = error.timestamp;
  enhanced.component = error.component;
  enhanced.operation = error.operation;
  enhanced.systemErrorCode = error.systemErrorCode;
  enhanced.context = error.context;

  // Add enhanced information
  enhanced.correlationId = generateCorrelationId();
  enhanced.isRetryable = error.isRecoverable();

  // Set suggested retry delay based on error type
  switch (error.category) {
  case WebSocketErrorCategory::TIMEOUT:
    enhanced.suggestedRetryDelay = std::chrono::milliseconds{500};
    break;
  case WebSocketErrorCategory::CONNECTION:
    enhanced.suggestedRetryDelay = std::chrono::milliseconds{2000};
    break;
  case WebSocketErrorCategory::NETWORK:
    enhanced.suggestedRetryDelay = std::chrono::milliseconds{5000};
    break;
  default:
    enhanced.suggestedRetryDelay = std::chrono::milliseconds{1000};
    break;
  }

  // Find connection context
  {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    for (const auto &[connId, context] : connections_) {
      if (context.componentName == error.component) {
        enhanced.connectionContext = context;
        break;
      }
    }
  }

  return enhanced;
}

void UnifiedWebSocketErrorHandler::updateStatistics(
    const EnhancedWebSocketError &error, WebSocketRecoveryAction action,
    bool recoverySuccessful) {
  std::lock_guard<std::mutex> lock(statisticsMutex_);

  statistics_.totalErrors++;
  statistics_.lastErrorTime = error.timestamp;

  // Update category-specific counters
  switch (error.category) {
  case WebSocketErrorCategory::CONNECTION:
    statistics_.connectionErrors++;
    break;
  case WebSocketErrorCategory::PROTOCOL:
    statistics_.protocolErrors++;
    break;
  case WebSocketErrorCategory::TIMEOUT:
    statistics_.timeoutErrors++;
    break;
  case WebSocketErrorCategory::MESSAGE:
    statistics_.messageErrors++;
    break;
  case WebSocketErrorCategory::AUTHENTICATION:
    statistics_.authenticationErrors++;
    break;
  case WebSocketErrorCategory::NETWORK:
    statistics_.networkErrors++;
    break;
  default:
    statistics_.unknownErrors++;
    break;
  }

  // Update recovery statistics
  if (action != WebSocketRecoveryAction::NONE) {
    statistics_.retriesAttempted++;
    if (recoverySuccessful) {
      statistics_.successfulRecoveries++;
      statistics_.lastRecoveryTime = std::chrono::system_clock::now();
    } else {
      statistics_.failedRecoveries++;
    }
  }

  // Update per-connection and per-component statistics
  statistics_.errorsByConnection[error.connectionContext.connectionId]++;
  statistics_.errorsByComponent[error.component]++;

  // Update circuit breaker trip count
  if (error.shouldTriggerCircuitBreaker()) {
    statistics_.circuitBreakerTrips++;
  }
}

void UnifiedWebSocketErrorHandler::cleanupOldCorrelations() {
  auto now = std::chrono::system_clock::now();

  for (auto it = correlatedErrors_.begin(); it != correlatedErrors_.end();) {
    bool hasRecentErrors = false;
    for (const auto &error : it->second) {
      auto errorAge = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - error.timestamp);
      if (errorAge < correlationWindow_) {
        hasRecentErrors = true;
        break;
      }
    }

    if (!hasRecentErrors) {
      it = correlatedErrors_.erase(it);
    } else {
      ++it;
    }
  }
}

std::string UnifiedWebSocketErrorHandler::generateCorrelationId() const {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<uint64_t> dis;

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                       now.time_since_epoch())
                       .count();

  std::stringstream ss;
  ss << "corr_" << timestamp << "_" << std::hex << dis(gen);
  return ss.str();
}

std::string UnifiedWebSocketErrorHandler::generateEventId() const {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<uint64_t> dis;

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                       now.time_since_epoch())
                       .count();

  std::stringstream ss;
  ss << "evt_" << timestamp << "_" << std::hex << dis(gen);
  return ss.str();
}

UnifiedWebSocketErrorHandler::RetryPolicy
UnifiedWebSocketErrorHandler::getRetryPolicy(
    const std::string &connectionId) const {
  auto it = connectionRetryPolicies_.find(connectionId);
  return (it != connectionRetryPolicies_.end()) ? it->second
                                                : globalRetryPolicy_;
}

bool UnifiedWebSocketErrorHandler::executeRecoveryAction(
    const std::string &connectionId, WebSocketRecoveryAction action) {
  if (connectionRecoveryCallback_) {
    try {
      return connectionRecoveryCallback_(connectionId, action);
    } catch (const std::exception &e) {
      spdlog::error("UnifiedWebSocketErrorHandler: Error in connection "
                    "recovery callback: {}",
                    e.what());
      return false;
    }
  }

  // Default recovery actions (placeholder implementations)
  switch (action) {
  case WebSocketRecoveryAction::RETRY:
    spdlog::info(
        "UnifiedWebSocketErrorHandler: Retrying operation for connection {}",
        connectionId);
    return true; // Assume success for now
  case WebSocketRecoveryAction::RECONNECT:
    spdlog::info("UnifiedWebSocketErrorHandler: Reconnecting connection {}",
                 connectionId);
    return true; // Assume success for now
  case WebSocketRecoveryAction::RESET:
    spdlog::info("UnifiedWebSocketErrorHandler: Resetting connection {}",
                 connectionId);
    return true; // Assume success for now
  case WebSocketRecoveryAction::TERMINATE:
    spdlog::warn("UnifiedWebSocketErrorHandler: Terminating connection {}",
                 connectionId);
    return true; // Termination is always "successful"
  default:
    return false;
  }
}

void UnifiedWebSocketErrorHandler::notifyErrorEvent(
    const WebSocketErrorEvent &event) {
  if (errorEventCallback_) {
    try {
      errorEventCallback_(event);
    } catch (const std::exception &e) {
      spdlog::error(
          "UnifiedWebSocketErrorHandler: Error in error event callback: {}",
          e.what());
    }
  }
}

// UnifiedWebSocketErrorHandlerFactory implementation
std::shared_ptr<UnifiedWebSocketErrorHandler>
UnifiedWebSocketErrorHandlerFactory::createHandler() {
  return std::make_shared<UnifiedWebSocketErrorHandler>();
}

std::shared_ptr<UnifiedWebSocketErrorHandler>
UnifiedWebSocketErrorHandlerFactory::createHandlerWithDefaults() {
  auto handler = createHandler();
  handler->setGlobalRetryPolicy(3, std::chrono::milliseconds{1000}, true);
  handler->enableCircuitBreaker(true);
  return handler;
}

std::shared_ptr<UnifiedWebSocketErrorHandler>
UnifiedWebSocketErrorHandlerFactory::createClientHandler() {
  auto handler = createHandlerWithDefaults();
  handler->setGlobalRetryPolicy(5, std::chrono::milliseconds{500}, true);
  handler->setErrorCorrelationWindow(std::chrono::milliseconds{10000});
  return handler;
}

std::shared_ptr<UnifiedWebSocketErrorHandler>
UnifiedWebSocketErrorHandlerFactory::createServerHandler() {
  auto handler = createHandlerWithDefaults();
  handler->setGlobalRetryPolicy(2, std::chrono::milliseconds{2000}, false);
  handler->setErrorCorrelationWindow(std::chrono::milliseconds{30000});
  return handler;
}

std::shared_ptr<UnifiedWebSocketErrorHandler>
UnifiedWebSocketErrorHandlerFactory::createHighAvailabilityHandler() {
  auto handler = createHandlerWithDefaults();
  handler->setGlobalRetryPolicy(10, std::chrono::milliseconds{100}, true);
  handler->enableCircuitBreaker(true);
  handler->setErrorCorrelationWindow(std::chrono::milliseconds{60000});
  return handler;
}

std::shared_ptr<UnifiedWebSocketErrorHandler>
UnifiedWebSocketErrorHandlerFactory::createDevelopmentHandler() {
  auto handler = createHandlerWithDefaults();
  handler->setGlobalRetryPolicy(1, std::chrono::milliseconds{5000}, false);
  handler->enableCircuitBreaker(false);
  handler->setErrorCorrelationWindow(std::chrono::milliseconds{5000});
  return handler;
}

// UnifiedWebSocketErrorRegistry implementation
UnifiedWebSocketErrorRegistry &UnifiedWebSocketErrorRegistry::getInstance() {
  static UnifiedWebSocketErrorRegistry instance;
  return instance;
}

void UnifiedWebSocketErrorRegistry::setGlobalHandler(
    std::shared_ptr<UnifiedWebSocketErrorHandler> handler) {
  std::lock_guard<std::mutex> lock(handlersMutex_);
  globalHandler_ = handler;
  spdlog::info(
      "UnifiedWebSocketErrorRegistry: Set global unified error handler");
}

std::shared_ptr<UnifiedWebSocketErrorHandler>
UnifiedWebSocketErrorRegistry::getGlobalHandler() {
  std::lock_guard<std::mutex> lock(handlersMutex_);
  return globalHandler_;
}

void UnifiedWebSocketErrorRegistry::registerComponentHandler(
    const std::string &component,
    std::shared_ptr<UnifiedWebSocketErrorHandler> handler) {
  std::lock_guard<std::mutex> lock(handlersMutex_);
  componentHandlers_[component] = handler;
  spdlog::info("UnifiedWebSocketErrorRegistry: Registered unified error "
               "handler for component: {}",
               component);
}

void UnifiedWebSocketErrorRegistry::unregisterComponentHandler(
    const std::string &component) {
  std::lock_guard<std::mutex> lock(handlersMutex_);
  auto it = componentHandlers_.find(component);
  if (it != componentHandlers_.end()) {
    componentHandlers_.erase(it);
    spdlog::info("UnifiedWebSocketErrorRegistry: Unregistered unified error "
                 "handler for component: {}",
                 component);
  }
}

std::shared_ptr<UnifiedWebSocketErrorHandler>
UnifiedWebSocketErrorRegistry::getComponentHandler(
    const std::string &component) {
  std::lock_guard<std::mutex> lock(handlersMutex_);
  auto it = componentHandlers_.find(component);
  return (it != componentHandlers_.end()) ? it->second : globalHandler_;
}

void UnifiedWebSocketErrorRegistry::handleError(const WebSocketError &error,
                                                const std::string &component) {
  auto handler =
      getComponentHandler(component.empty() ? error.component : component);
  if (handler) {
    handler->handleError(error);
  } else {
    spdlog::error("UnifiedWebSocketErrorRegistry: No handler available for "
                  "error in component {}: {}",
                  component.empty() ? error.component : component,
                  error.toString());
  }
}

void UnifiedWebSocketErrorRegistry::handleEnhancedError(
    const EnhancedWebSocketError &error, const std::string &component) {
  auto handler =
      getComponentHandler(component.empty() ? error.component : component);
  if (handler) {
    handler->handleEnhancedError(error);
  } else {
    spdlog::error("UnifiedWebSocketErrorRegistry: No handler available for "
                  "enhanced error in component {}: {}",
                  component.empty() ? error.component : component,
                  error.toString());
  }
}

UnifiedWebSocketErrorHandler::UnifiedErrorStatistics
UnifiedWebSocketErrorRegistry::getGlobalStatistics() const {
  std::lock_guard<std::mutex> lock(handlersMutex_);

  UnifiedWebSocketErrorHandler::UnifiedErrorStatistics aggregated;

  // Aggregate statistics from all handlers
  for (const auto &[component, handler] : componentHandlers_) {
    auto stats = handler->getStatistics();
    aggregated.totalErrors += stats.totalErrors;
    aggregated.connectionErrors += stats.connectionErrors;
    aggregated.protocolErrors += stats.protocolErrors;
    aggregated.timeoutErrors += stats.timeoutErrors;
    aggregated.messageErrors += stats.messageErrors;
    aggregated.authenticationErrors += stats.authenticationErrors;
    aggregated.networkErrors += stats.networkErrors;
    aggregated.unknownErrors += stats.unknownErrors;
    aggregated.retriesAttempted += stats.retriesAttempted;
    aggregated.successfulRecoveries += stats.successfulRecoveries;
    aggregated.failedRecoveries += stats.failedRecoveries;
    aggregated.circuitBreakerTrips += stats.circuitBreakerTrips;

    if (stats.lastErrorTime > aggregated.lastErrorTime) {
      aggregated.lastErrorTime = stats.lastErrorTime;
    }
    if (stats.lastRecoveryTime > aggregated.lastRecoveryTime) {
      aggregated.lastRecoveryTime = stats.lastRecoveryTime;
    }

    // Merge error maps
    for (const auto &[connId, count] : stats.errorsByConnection) {
      aggregated.errorsByConnection[connId] += count;
    }
    for (const auto &[comp, count] : stats.errorsByComponent) {
      aggregated.errorsByComponent[comp] += count;
    }
  }

  // Include global handler statistics
  if (globalHandler_) {
    auto stats = globalHandler_->getStatistics();
    aggregated.totalErrors += stats.totalErrors;
    aggregated.connectionErrors += stats.connectionErrors;
    aggregated.protocolErrors += stats.protocolErrors;
    aggregated.timeoutErrors += stats.timeoutErrors;
    aggregated.messageErrors += stats.messageErrors;
    aggregated.authenticationErrors += stats.authenticationErrors;
    aggregated.networkErrors += stats.networkErrors;
    aggregated.unknownErrors += stats.unknownErrors;
    aggregated.retriesAttempted += stats.retriesAttempted;
    aggregated.successfulRecoveries += stats.successfulRecoveries;
    aggregated.failedRecoveries += stats.failedRecoveries;
    aggregated.circuitBreakerTrips += stats.circuitBreakerTrips;

    if (stats.lastErrorTime > aggregated.lastErrorTime) {
      aggregated.lastErrorTime = stats.lastErrorTime;
    }
    if (stats.lastRecoveryTime > aggregated.lastRecoveryTime) {
      aggregated.lastRecoveryTime = stats.lastRecoveryTime;
    }

    // Merge error maps
    for (const auto &[connId, count] : stats.errorsByConnection) {
      aggregated.errorsByConnection[connId] += count;
    }
    for (const auto &[comp, count] : stats.errorsByComponent) {
      aggregated.errorsByComponent[comp] += count;
    }
  }

  return aggregated;
}

json UnifiedWebSocketErrorRegistry::generateGlobalErrorReport() const {
  json report = json::object();

  {
    std::lock_guard<std::mutex> lock(handlersMutex_);

    // Global statistics
    auto globalStats = getGlobalStatistics();
    report["globalStatistics"] = {
        {"totalErrors", globalStats.totalErrors},
        {"connectionErrors", globalStats.connectionErrors},
        {"protocolErrors", globalStats.protocolErrors},
        {"timeoutErrors", globalStats.timeoutErrors},
        {"messageErrors", globalStats.messageErrors},
        {"authenticationErrors", globalStats.authenticationErrors},
        {"networkErrors", globalStats.networkErrors},
        {"unknownErrors", globalStats.unknownErrors},
        {"retriesAttempted", globalStats.retriesAttempted},
        {"successfulRecoveries", globalStats.successfulRecoveries},
        {"failedRecoveries", globalStats.failedRecoveries},
        {"circuitBreakerTrips", globalStats.circuitBreakerTrips},
        {"errorsByConnection", globalStats.errorsByConnection},
        {"errorsByComponent", globalStats.errorsByComponent}};

    // Component-specific reports
    json componentReports = json::object();
    for (const auto &[component, handler] : componentHandlers_) {
      componentReports[component] = handler->generateErrorReport();
    }
    report["componentReports"] = componentReports;

    // Global handler report
    if (globalHandler_) {
      report["globalHandlerReport"] = globalHandler_->generateErrorReport();
    }
  }

  return report;
}

} // namespace core
} // namespace hydrogen

#endif // HYDROGEN_HAS_WEBSOCKETS

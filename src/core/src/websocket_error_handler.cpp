#include "hydrogen/core/websocket_error_handler.h"

#ifdef HYDROGEN_HAS_WEBSOCKETS
#include <sstream>
#include <random>
#include <iomanip>
#include <boost/beast/websocket/error.hpp>
#include <boost/asio/error.hpp>

namespace hydrogen {
namespace core {

// WebSocketError implementation
std::string WebSocketError::toString() const {
    std::stringstream ss;
    ss << "WebSocketError[" << errorId << "]: " << message;
    if (!details.empty()) {
        ss << " (" << details << ")";
    }
    ss << " [Category: " << static_cast<int>(category) 
       << ", Severity: " << static_cast<int>(severity) << "]";
    return ss.str();
}

bool WebSocketError::isRecoverable() const {
    switch (category) {
        case WebSocketErrorCategory::CONNECTION:
        case WebSocketErrorCategory::TIMEOUT:
        case WebSocketErrorCategory::NETWORK:
            return severity != WebSocketErrorSeverity::CRITICAL;
        case WebSocketErrorCategory::PROTOCOL:
        case WebSocketErrorCategory::MESSAGE:
            return severity == WebSocketErrorSeverity::LOW || severity == WebSocketErrorSeverity::MEDIUM;
        case WebSocketErrorCategory::AUTHENTICATION:
        case WebSocketErrorCategory::RESOURCE:
            return false;
        default:
            return false;
    }
}

bool WebSocketError::requiresReconnection() const {
    return category == WebSocketErrorCategory::CONNECTION ||
           (category == WebSocketErrorCategory::PROTOCOL && severity >= WebSocketErrorSeverity::HIGH) ||
           (category == WebSocketErrorCategory::NETWORK && severity >= WebSocketErrorSeverity::MEDIUM);
}

// StandardWebSocketErrorHandler implementation
StandardWebSocketErrorHandler::StandardWebSocketErrorHandler() {
    statistics_.lastErrorTime = std::chrono::system_clock::now();
}

void StandardWebSocketErrorHandler::handleError(const WebSocketError& error) {
    updateStatistics(error);
    
    spdlog::error("WebSocket error in {}: {}", error.component, error.toString());
    
    if (errorCallback_) {
        try {
            errorCallback_(error);
        } catch (const std::exception& e) {
            spdlog::error("Error in WebSocket error callback: {}", e.what());
        }
    }
    
    // Determine and execute recovery action
    WebSocketRecoveryAction action = determineRecoveryAction(error);
    if (action != WebSocketRecoveryAction::NONE && recoveryCallback_) {
        try {
            bool success = recoveryCallback_(error, action);
            if (success) {
                std::lock_guard<std::mutex> lock(statisticsMutex_);
                statistics_.successfulRecoveries++;
            }
        } catch (const std::exception& e) {
            spdlog::error("Error in WebSocket recovery callback: {}", e.what());
        }
    }
}

WebSocketRecoveryAction StandardWebSocketErrorHandler::determineRecoveryAction(const WebSocketError& error) {
    // Check if error is recoverable
    if (!error.isRecoverable()) {
        return WebSocketRecoveryAction::TERMINATE;
    }
    
    // Use recommended action if available
    if (error.recommendedAction != WebSocketRecoveryAction::NONE) {
        return error.recommendedAction;
    }
    
    // Determine action based on category and severity
    return getDefaultRecoveryAction(error.category, error.severity);
}

bool StandardWebSocketErrorHandler::shouldRetry(const WebSocketError& error, int attemptCount) {
    if (attemptCount >= maxRetryAttempts_) {
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

std::chrono::milliseconds StandardWebSocketErrorHandler::getRetryDelay(const WebSocketError& error, int attemptCount) {
    if (!useExponentialBackoff_) {
        return baseRetryDelay_;
    }
    
    // Exponential backoff with jitter
    auto delay = baseRetryDelay_ * (1 << attemptCount);
    if (delay > maxRetryDelay_) {
        delay = maxRetryDelay_;
    }
    
    // Add jitter (±25%)
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.75, 1.25);
    
    delay = std::chrono::milliseconds(static_cast<long long>(delay.count() * dis(gen)));
    
    return delay;
}

StandardWebSocketErrorHandler::ErrorStatistics StandardWebSocketErrorHandler::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    return statistics_;
}

void StandardWebSocketErrorHandler::resetStatistics() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_ = ErrorStatistics{};
    statistics_.lastErrorTime = std::chrono::system_clock::now();
}

void StandardWebSocketErrorHandler::updateStatistics(const WebSocketError& error) {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    statistics_.totalErrors++;
    statistics_.lastErrorTime = error.timestamp;
    
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
        default:
            break;
    }
}

WebSocketRecoveryAction StandardWebSocketErrorHandler::getDefaultRecoveryAction(WebSocketErrorCategory category, WebSocketErrorSeverity severity) {
    switch (category) {
        case WebSocketErrorCategory::CONNECTION:
            return (severity >= WebSocketErrorSeverity::MEDIUM) ? WebSocketRecoveryAction::RECONNECT : WebSocketRecoveryAction::RETRY;
        case WebSocketErrorCategory::TIMEOUT:
            return WebSocketRecoveryAction::RETRY;
        case WebSocketErrorCategory::PROTOCOL:
            return (severity >= WebSocketErrorSeverity::HIGH) ? WebSocketRecoveryAction::RECONNECT : WebSocketRecoveryAction::RESET;
        case WebSocketErrorCategory::MESSAGE:
            return WebSocketRecoveryAction::NONE;
        case WebSocketErrorCategory::NETWORK:
            return WebSocketRecoveryAction::RECONNECT;
        case WebSocketErrorCategory::AUTHENTICATION:
            return WebSocketRecoveryAction::ESCALATE;
        case WebSocketErrorCategory::RESOURCE:
            return WebSocketRecoveryAction::ESCALATE;
        default:
            return WebSocketRecoveryAction::NONE;
    }
}

// WebSocketErrorFactory implementation
WebSocketError WebSocketErrorFactory::createFromBoostError(const boost::system::error_code& ec, 
                                                          const std::string& component,
                                                          const std::string& operation) {
    WebSocketError error;
    error.errorId = generateErrorId();
    error.errorCode = ec.category().name() + std::to_string(ec.value());
    error.message = ec.message();
    error.category = categorizeBoostError(ec);
    error.severity = determineSeverity(error.category, ec);
    error.timestamp = std::chrono::system_clock::now();
    error.component = component;
    error.operation = operation;
    error.systemErrorCode = ec;
    
    // Set recommended action based on error type
    if (ec == websocket::error::closed) {
        error.recommendedAction = WebSocketRecoveryAction::RECONNECT;
        error.details = "WebSocket connection was closed";
    } else if (ec == boost::asio::error::operation_aborted) {
        error.recommendedAction = WebSocketRecoveryAction::NONE;
        error.details = "Operation was aborted";
    } else if (ec == boost::asio::error::timed_out) {
        error.recommendedAction = WebSocketRecoveryAction::RETRY;
        error.details = "Operation timed out";
    } else if (ec == boost::asio::error::connection_refused) {
        error.recommendedAction = WebSocketRecoveryAction::RETRY;
        error.details = "Connection was refused by server";
    } else {
        error.recommendedAction = WebSocketRecoveryAction::NONE;
    }
    
    return error;
}

WebSocketError WebSocketErrorFactory::createFromException(const std::exception& ex,
                                                         const std::string& component,
                                                         const std::string& operation) {
    WebSocketError error;
    error.errorId = generateErrorId();
    error.errorCode = "EXCEPTION";
    error.message = ex.what();
    error.category = WebSocketErrorCategory::UNKNOWN;
    error.severity = WebSocketErrorSeverity::MEDIUM;
    error.recommendedAction = WebSocketRecoveryAction::RETRY;
    error.timestamp = std::chrono::system_clock::now();
    error.component = component;
    error.operation = operation;
    error.details = "Exception thrown during WebSocket operation";
    
    return error;
}

WebSocketError WebSocketErrorFactory::createConnectionError(const std::string& message, const std::string& details) {
    WebSocketError error;
    error.errorId = generateErrorId();
    error.errorCode = "CONNECTION_ERROR";
    error.message = message;
    error.details = details;
    error.category = WebSocketErrorCategory::CONNECTION;
    error.severity = WebSocketErrorSeverity::HIGH;
    error.recommendedAction = WebSocketRecoveryAction::RECONNECT;
    error.timestamp = std::chrono::system_clock::now();
    
    return error;
}

WebSocketError WebSocketErrorFactory::createProtocolError(const std::string& message, const std::string& details) {
    WebSocketError error;
    error.errorId = generateErrorId();
    error.errorCode = "PROTOCOL_ERROR";
    error.message = message;
    error.details = details;
    error.category = WebSocketErrorCategory::PROTOCOL;
    error.severity = WebSocketErrorSeverity::MEDIUM;
    error.recommendedAction = WebSocketRecoveryAction::RESET;
    error.timestamp = std::chrono::system_clock::now();
    
    return error;
}

WebSocketError WebSocketErrorFactory::createTimeoutError(const std::string& operation, std::chrono::milliseconds timeout) {
    WebSocketError error;
    error.errorId = generateErrorId();
    error.errorCode = "TIMEOUT_ERROR";
    error.message = "Operation timed out: " + operation;
    error.details = "Timeout after " + std::to_string(timeout.count()) + "ms";
    error.category = WebSocketErrorCategory::TIMEOUT;
    error.severity = WebSocketErrorSeverity::MEDIUM;
    error.recommendedAction = WebSocketRecoveryAction::RETRY;
    error.timestamp = std::chrono::system_clock::now();
    error.operation = operation;
    
    return error;
}

WebSocketError WebSocketErrorFactory::createMessageError(const std::string& message, const std::string& details) {
    WebSocketError error;
    error.errorId = generateErrorId();
    error.errorCode = "MESSAGE_ERROR";
    error.message = message;
    error.details = details;
    error.category = WebSocketErrorCategory::MESSAGE;
    error.severity = WebSocketErrorSeverity::LOW;
    error.recommendedAction = WebSocketRecoveryAction::NONE;
    error.timestamp = std::chrono::system_clock::now();
    
    return error;
}

WebSocketError WebSocketErrorFactory::createAuthenticationError(const std::string& message, const std::string& details) {
    WebSocketError error;
    error.errorId = generateErrorId();
    error.errorCode = "AUTH_ERROR";
    error.message = message;
    error.details = details;
    error.category = WebSocketErrorCategory::AUTHENTICATION;
    error.severity = WebSocketErrorSeverity::HIGH;
    error.recommendedAction = WebSocketRecoveryAction::ESCALATE;
    error.timestamp = std::chrono::system_clock::now();
    
    return error;
}

WebSocketErrorCategory WebSocketErrorFactory::categorizeBoostError(const boost::system::error_code& ec) {
    if (ec.category() == boost::asio::error::get_system_category() ||
        ec.category() == boost::asio::error::get_netdb_category()) {
        return WebSocketErrorCategory::NETWORK;
    }
    
    if (ec == websocket::error::closed ||
        ec == boost::asio::error::connection_refused ||
        ec == boost::asio::error::connection_reset ||
        ec == boost::asio::error::connection_aborted) {
        return WebSocketErrorCategory::CONNECTION;
    }
    
    if (ec == boost::asio::error::timed_out) {
        return WebSocketErrorCategory::TIMEOUT;
    }
    
    // Check for WebSocket-specific errors (using only valid constants)
    if (ec == websocket::error::closed ||
        ec == websocket::error::buffer_overflow ||
        ec == websocket::error::partial_deflate_block ||
        ec == websocket::error::message_too_big ||
        ec == websocket::error::bad_http_version ||
        ec == websocket::error::bad_method ||
        ec == websocket::error::no_host ||
        ec == websocket::error::bad_size ||
        ec == websocket::error::bad_close_code ||
        ec == websocket::error::bad_close_size ||
        ec == websocket::error::bad_close_payload) {
        return WebSocketErrorCategory::PROTOCOL;
    }
    
    return WebSocketErrorCategory::UNKNOWN;
}

WebSocketErrorSeverity WebSocketErrorFactory::determineSeverity(WebSocketErrorCategory category, const boost::system::error_code& ec) {
    switch (category) {
        case WebSocketErrorCategory::CONNECTION:
            if (ec == boost::asio::error::connection_refused) {
                return WebSocketErrorSeverity::HIGH;
            }
            return WebSocketErrorSeverity::MEDIUM;
        case WebSocketErrorCategory::PROTOCOL:
            return WebSocketErrorSeverity::MEDIUM;
        case WebSocketErrorCategory::TIMEOUT:
            return WebSocketErrorSeverity::MEDIUM;
        case WebSocketErrorCategory::AUTHENTICATION:
            return WebSocketErrorSeverity::HIGH;
        case WebSocketErrorCategory::RESOURCE:
            return WebSocketErrorSeverity::CRITICAL;
        case WebSocketErrorCategory::MESSAGE:
            return WebSocketErrorSeverity::LOW;
        case WebSocketErrorCategory::NETWORK:
            return WebSocketErrorSeverity::MEDIUM;
        default:
            return WebSocketErrorSeverity::MEDIUM;
    }
}

std::string WebSocketErrorFactory::generateErrorId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "ws_err_" << std::hex << timestamp << "_" << std::hex << dis(gen);
    return ss.str();
}

// WebSocketErrorRegistry implementation
WebSocketErrorRegistry& WebSocketErrorRegistry::getInstance() {
    static WebSocketErrorRegistry instance;
    return instance;
}

void WebSocketErrorRegistry::registerHandler(const std::string& component, std::shared_ptr<WebSocketErrorHandler> handler) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    handlers_[component] = handler;
    spdlog::debug("WebSocketErrorRegistry: Registered error handler for component: {}", component);
}

void WebSocketErrorRegistry::unregisterHandler(const std::string& component) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    auto it = handlers_.find(component);
    if (it != handlers_.end()) {
        handlers_.erase(it);
        spdlog::debug("WebSocketErrorRegistry: Unregistered error handler for component: {}", component);
    }
}

std::shared_ptr<WebSocketErrorHandler> WebSocketErrorRegistry::getHandler(const std::string& component) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    auto it = handlers_.find(component);
    if (it != handlers_.end()) {
        return it->second;
    }

    // Return global handler if component-specific handler not found
    return globalHandler_;
}

void WebSocketErrorRegistry::handleGlobalError(const WebSocketError& error) {
    // Try component-specific handler first
    auto handler = getHandler(error.component);
    if (handler) {
        handler->handleError(error);
        return;
    }

    // Fall back to global handler
    if (globalHandler_) {
        globalHandler_->handleError(error);
    } else {
        // Default logging if no handler available
        spdlog::error("Unhandled WebSocket error in {}: {}", error.component, error.toString());
    }
}

void WebSocketErrorRegistry::setGlobalErrorHandler(std::shared_ptr<WebSocketErrorHandler> handler) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    globalHandler_ = handler;
    spdlog::info("WebSocketErrorRegistry: Set global error handler");
}

StandardWebSocketErrorHandler::ErrorStatistics WebSocketErrorRegistry::getGlobalStatistics() const {
    std::lock_guard<std::mutex> lock(handlersMutex_);

    StandardWebSocketErrorHandler::ErrorStatistics aggregated;

    // Aggregate statistics from all handlers
    for (const auto& [component, handler] : handlers_) {
        if (auto standardHandler = std::dynamic_pointer_cast<StandardWebSocketErrorHandler>(handler)) {
            auto stats = standardHandler->getStatistics();
            aggregated.totalErrors += stats.totalErrors;
            aggregated.connectionErrors += stats.connectionErrors;
            aggregated.protocolErrors += stats.protocolErrors;
            aggregated.timeoutErrors += stats.timeoutErrors;
            aggregated.messageErrors += stats.messageErrors;
            aggregated.retriesAttempted += stats.retriesAttempted;
            aggregated.successfulRecoveries += stats.successfulRecoveries;

            if (stats.lastErrorTime > aggregated.lastErrorTime) {
                aggregated.lastErrorTime = stats.lastErrorTime;
            }
        }
    }

    // Include global handler statistics
    if (auto standardGlobalHandler = std::dynamic_pointer_cast<StandardWebSocketErrorHandler>(globalHandler_)) {
        auto stats = standardGlobalHandler->getStatistics();
        aggregated.totalErrors += stats.totalErrors;
        aggregated.connectionErrors += stats.connectionErrors;
        aggregated.protocolErrors += stats.protocolErrors;
        aggregated.timeoutErrors += stats.timeoutErrors;
        aggregated.messageErrors += stats.messageErrors;
        aggregated.retriesAttempted += stats.retriesAttempted;
        aggregated.successfulRecoveries += stats.successfulRecoveries;

        if (stats.lastErrorTime > aggregated.lastErrorTime) {
            aggregated.lastErrorTime = stats.lastErrorTime;
        }
    }

    return aggregated;
}

} // namespace core
} // namespace hydrogen

#endif // HYDROGEN_HAS_WEBSOCKETS

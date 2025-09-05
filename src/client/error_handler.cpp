#include "client/error_handler.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <thread>
#include <random>

namespace hydrogen {
namespace client {

// CircuitBreaker implementation
CircuitBreaker::CircuitBreaker(const Config& config) : config(config) {
    lastFailureTime.store(std::chrono::system_clock::now());
}

bool CircuitBreaker::canExecute() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    CircuitBreakerState currentState = state.load();
    
    switch (currentState) {
        case CircuitBreakerState::CLOSED:
            return true;
            
        case CircuitBreakerState::OPEN:
            return shouldAttemptReset();
            
        case CircuitBreakerState::HALF_OPEN:
            return true;
            
        default:
            return false;
    }
}

void CircuitBreaker::recordSuccess() {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    CircuitBreakerState currentState = state.load();
    
    if (currentState == CircuitBreakerState::HALF_OPEN) {
        size_t currentSuccessCount = successCount.fetch_add(1) + 1;
        if (currentSuccessCount >= config.successThreshold) {
            transitionToClosed();
        }
    } else if (currentState == CircuitBreakerState::CLOSED) {
        // Reset failure count on success
        failureCount.store(0);
    }
}

void CircuitBreaker::recordFailure() {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    lastFailureTime.store(std::chrono::system_clock::now());
    size_t currentFailureCount = failureCount.fetch_add(1) + 1;
    
    CircuitBreakerState currentState = state.load();
    
    if (currentState == CircuitBreakerState::CLOSED && 
        currentFailureCount >= config.failureThreshold) {
        transitionToOpen();
    } else if (currentState == CircuitBreakerState::HALF_OPEN) {
        transitionToOpen();
    }
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(stateMutex);
    transitionToClosed();
}

void CircuitBreaker::transitionToOpen() {
    state.store(CircuitBreakerState::OPEN);
    successCount.store(0);
    spdlog::warn("Circuit breaker transitioned to OPEN state");
}

void CircuitBreaker::transitionToHalfOpen() {
    state.store(CircuitBreakerState::HALF_OPEN);
    successCount.store(0);
    spdlog::info("Circuit breaker transitioned to HALF_OPEN state");
}

void CircuitBreaker::transitionToClosed() {
    state.store(CircuitBreakerState::CLOSED);
    failureCount.store(0);
    successCount.store(0);
    spdlog::info("Circuit breaker transitioned to CLOSED state");
}

bool CircuitBreaker::shouldAttemptReset() const {
    auto now = std::chrono::system_clock::now();
    auto lastFailure = lastFailureTime.load();
    auto timeSinceLastFailure = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFailure);
    
    return timeSinceLastFailure >= config.retryTimeout;
}

// RetryPolicy implementation
RetryPolicy::RetryPolicy(const Config& config) : config(config) {}

bool RetryPolicy::shouldRetry(const ErrorInfo& error, size_t attemptCount) const {
    if (attemptCount >= config.maxRetries) {
        return false;
    }
    
    return isRetryableError(error);
}

std::chrono::milliseconds RetryPolicy::getDelay(size_t attemptCount) const {
    if (attemptCount == 0) {
        return config.initialDelay;
    }
    
    double delay = config.initialDelay.count() * std::pow(config.backoffMultiplier, attemptCount - 1);
    auto delayMs = std::chrono::milliseconds(static_cast<long long>(delay));
    
    return std::min(delayMs, config.maxDelay);
}

void RetryPolicy::reset() {
    // Nothing to reset in current implementation
}

bool RetryPolicy::isRetryableError(const ErrorInfo& error) const {
    return std::find(config.retryableCategories.begin(), 
                    config.retryableCategories.end(), 
                    error.category) != config.retryableCategories.end();
}

// ErrorHandler implementation
ErrorHandler::ErrorHandler(const Config& config) : config(config) {
    // Register default recovery callbacks
    registerRecoveryCallback(RecoveryStrategy::RETRY, 
        [this](const ErrorInfo& error, RecoveryStrategy strategy) {
            return defaultRetryRecovery(error, strategy);
        });
    
    registerRecoveryCallback(RecoveryStrategy::CIRCUIT_BREAKER,
        [this](const ErrorInfo& error, RecoveryStrategy strategy) {
            return defaultCircuitBreakerRecovery(error, strategy);
        });
    
    registerRecoveryCallback(RecoveryStrategy::GRACEFUL_DEGRADATION,
        [this](const ErrorInfo& error, RecoveryStrategy strategy) {
            return defaultGracefulDegradationRecovery(error, strategy);
        });
    
    spdlog::info("ErrorHandler initialized");
}

ErrorHandler::~ErrorHandler() {
    spdlog::info("ErrorHandler destroyed");
}

void ErrorHandler::handleError(const ErrorInfo& error) {
    stats.totalErrors++;
    
    // Add to history
    addToHistory(error);
    
    // Log the error
    switch (error.severity) {
        case ErrorSeverity::LOW:
            spdlog::debug("Low severity error: {} - {}", error.errorCode, error.errorMessage);
            break;
        case ErrorSeverity::MEDIUM:
            spdlog::warn("Medium severity error: {} - {}", error.errorCode, error.errorMessage);
            break;
        case ErrorSeverity::HIGH:
            spdlog::error("High severity error: {} - {}", error.errorCode, error.errorMessage);
            break;
        case ErrorSeverity::CRITICAL:
            spdlog::critical("Critical error: {} - {}", error.errorCode, error.errorMessage);
            stats.criticalErrors++;
            break;
    }
    
    // Execute specific error callback if registered
    auto callbackIt = errorCallbacks.find(error.errorCode);
    if (callbackIt != errorCallbacks.end()) {
        try {
            callbackIt->second(error);
        } catch (const std::exception& e) {
            spdlog::error("Exception in error callback for {}: {}", error.errorCode, e.what());
        }
    }
    
    // Determine and execute recovery strategy
    RecoveryStrategy strategy = determineRecoveryStrategy(error);
    if (executeRecovery(error, strategy)) {
        stats.recoveredErrors++;
    }
    
    // Cleanup old errors periodically
    cleanupOldErrors();
}

void ErrorHandler::registerErrorCallback(const std::string& errorCode, ErrorCallback callback) {
    errorCallbacks[errorCode] = callback;
    spdlog::debug("Registered error callback for: {}", errorCode);
}

void ErrorHandler::registerRecoveryCallback(RecoveryStrategy strategy, RecoveryCallback callback) {
    recoveryCallbacks[strategy] = callback;
    spdlog::debug("Registered recovery callback for strategy: {}", static_cast<int>(strategy));
}

bool ErrorHandler::executeWithRetry(const std::string& operation, 
                                           std::function<bool()> func,
                                           const RetryPolicy::Config& retryConfig) {
    RetryPolicy retryPolicy(retryConfig);
    size_t attemptCount = 0;
    
    while (true) {
        try {
            if (func()) {
                if (attemptCount > 0) {
                    stats.successfulRetries++;
                }
                return true;
            }
        } catch (const std::exception& e) {
            ErrorInfo error("EXECUTION_EXCEPTION", e.what(), ErrorSeverity::HIGH, ErrorCategory::INTERNAL);
            error.context = operation;
            
            if (!retryPolicy.shouldRetry(error, attemptCount)) {
                handleError(error);
                return false;
            }
        }
        
        attemptCount++;
        stats.retryAttempts++;
        
        // Wait before retry
        auto delay = retryPolicy.getDelay(attemptCount);
        std::this_thread::sleep_for(delay);
        
        spdlog::debug("Retrying operation '{}' (attempt {})", operation, attemptCount + 1);
    }
}

bool ErrorHandler::executeWithCircuitBreaker(const std::string& operation,
                                                    std::function<bool()> func,
                                                    const CircuitBreaker::Config& cbConfig) {
    // Get or create circuit breaker for this operation
    auto it = circuitBreakers.find(operation);
    if (it == circuitBreakers.end()) {
        circuitBreakers[operation] = std::make_unique<CircuitBreaker>(cbConfig);
        it = circuitBreakers.find(operation);
    }
    
    auto& circuitBreaker = it->second;
    
    if (!circuitBreaker->canExecute()) {
        ErrorInfo error("CIRCUIT_BREAKER_OPEN", "Circuit breaker is open for operation: " + operation,
                       ErrorSeverity::HIGH, ErrorCategory::RESOURCE);
        handleError(error);
        return false;
    }
    
    try {
        if (func()) {
            circuitBreaker->recordSuccess();
            return true;
        } else {
            circuitBreaker->recordFailure();
            if (circuitBreaker->getState() == CircuitBreakerState::OPEN) {
                stats.circuitBreakerTrips++;
            }
            return false;
        }
    } catch (const std::exception& e) {
        circuitBreaker->recordFailure();
        if (circuitBreaker->getState() == CircuitBreakerState::OPEN) {
            stats.circuitBreakerTrips++;
        }
        
        ErrorInfo error("CIRCUIT_BREAKER_EXCEPTION", e.what(), ErrorSeverity::HIGH, ErrorCategory::INTERNAL);
        error.context = operation;
        handleError(error);
        return false;
    }
}

void ErrorHandler::resetStatistics() {
    stats.totalErrors = 0;
    stats.recoveredErrors = 0;
    stats.criticalErrors = 0;
    stats.circuitBreakerTrips = 0;
    stats.retryAttempts = 0;
    stats.successfulRetries = 0;
    spdlog::info("Error handler statistics reset");
}

std::vector<ErrorInfo> ErrorHandler::getErrorHistory(ErrorCategory category) const {
    std::lock_guard<std::mutex> lock(historyMutex);

    if (category == ErrorCategory::INTERNAL) {
        return errorHistory; // Return all errors
    }

    std::vector<ErrorInfo> filtered;
    std::copy_if(errorHistory.begin(), errorHistory.end(), std::back_inserter(filtered),
                [category](const ErrorInfo& error) { return error.category == category; });

    return filtered;
}

std::vector<ErrorInfo> ErrorHandler::getRecentErrors(std::chrono::minutes duration) const {
    std::lock_guard<std::mutex> lock(historyMutex);

    auto cutoff = std::chrono::system_clock::now() - duration;
    std::vector<ErrorInfo> recent;

    std::copy_if(errorHistory.begin(), errorHistory.end(), std::back_inserter(recent),
                [cutoff](const ErrorInfo& error) { return error.timestamp >= cutoff; });

    return recent;
}

void ErrorHandler::clearErrorHistory() {
    std::lock_guard<std::mutex> lock(historyMutex);
    errorHistory.clear();
    spdlog::info("Error history cleared");
}

bool ErrorHandler::isHealthy() const {
    // Consider system healthy if error rate is below threshold
    double errorRate = getErrorRate(std::chrono::minutes(5));
    return errorRate < 0.1; // Less than 10% error rate
}

double ErrorHandler::getErrorRate(std::chrono::minutes window) const {
    auto recentErrors = getRecentErrors(window);
    if (recentErrors.empty()) {
        return 0.0;
    }

    // Simple error rate calculation - could be enhanced with more sophisticated metrics
    size_t totalOperations = stats.totalErrors.load();
    if (totalOperations == 0) {
        return 0.0;
    }

    return static_cast<double>(recentErrors.size()) / totalOperations;
}

std::unordered_map<ErrorCategory, size_t> ErrorHandler::getErrorCategoryCounts() const {
    std::lock_guard<std::mutex> lock(historyMutex);

    std::unordered_map<ErrorCategory, size_t> counts;
    for (const auto& error : errorHistory) {
        counts[error.category]++;
    }

    return counts;
}

RecoveryStrategy ErrorHandler::determineRecoveryStrategy(const ErrorInfo& error) const {
    // Strategy selection based on error characteristics
    switch (error.category) {
        case ErrorCategory::NETWORK:
        case ErrorCategory::TIMEOUT:
            return (error.severity >= ErrorSeverity::HIGH) ?
                   RecoveryStrategy::CIRCUIT_BREAKER : RecoveryStrategy::EXPONENTIAL_BACKOFF;

        case ErrorCategory::AUTHENTICATION:
            return RecoveryStrategy::ESCALATE;

        case ErrorCategory::RESOURCE:
            return RecoveryStrategy::GRACEFUL_DEGRADATION;

        case ErrorCategory::VALIDATION:
            return RecoveryStrategy::IGNORE; // Usually not recoverable

        case ErrorCategory::PROTOCOL:
            return RecoveryStrategy::RETRY;

        case ErrorCategory::INTERNAL:
        default:
            return (error.severity == ErrorSeverity::CRITICAL) ?
                   RecoveryStrategy::RESTART : RecoveryStrategy::RETRY;
    }
}

bool ErrorHandler::executeRecovery(const ErrorInfo& error, RecoveryStrategy strategy) {
    auto it = recoveryCallbacks.find(strategy);
    if (it != recoveryCallbacks.end()) {
        try {
            return it->second(error, strategy);
        } catch (const std::exception& e) {
            spdlog::error("Exception in recovery callback: {}", e.what());
            return false;
        }
    }

    spdlog::warn("No recovery callback registered for strategy: {}", static_cast<int>(strategy));
    return false;
}

void ErrorHandler::addToHistory(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(historyMutex);

    errorHistory.push_back(error);

    // Limit history size
    if (errorHistory.size() > config.maxErrorHistory) {
        errorHistory.erase(errorHistory.begin(),
                          errorHistory.begin() + (errorHistory.size() - config.maxErrorHistory));
    }
}

void ErrorHandler::cleanupOldErrors() {
    std::lock_guard<std::mutex> lock(historyMutex);

    auto cutoff = std::chrono::system_clock::now() - config.errorHistoryRetention;
    errorHistory.erase(
        std::remove_if(errorHistory.begin(), errorHistory.end(),
                      [cutoff](const ErrorInfo& error) { return error.timestamp < cutoff; }),
        errorHistory.end());
}

bool ErrorHandler::defaultRetryRecovery(const ErrorInfo& error, RecoveryStrategy strategy) {
    spdlog::debug("Executing default retry recovery for error: {}", error.errorCode);
    // Default retry logic would be implemented here
    return true;
}

bool ErrorHandler::defaultCircuitBreakerRecovery(const ErrorInfo& error, RecoveryStrategy strategy) {
    spdlog::debug("Executing default circuit breaker recovery for error: {}", error.errorCode);
    // Default circuit breaker logic would be implemented here
    return true;
}

bool ErrorHandler::defaultGracefulDegradationRecovery(const ErrorInfo& error, RecoveryStrategy strategy) {
    spdlog::debug("Executing default graceful degradation recovery for error: {}", error.errorCode);
    // Default graceful degradation logic would be implemented here
    return true;
}

} // namespace client
} // namespace hydrogen

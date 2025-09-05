#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>

namespace hydrogen {
namespace client {

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    LOW = 0,        // Minor issues, continue operation
    MEDIUM = 1,     // Moderate issues, may affect performance
    HIGH = 2,       // Serious issues, may cause failures
    CRITICAL = 3    // Critical issues, immediate attention required
};

/**
 * @brief Error categories for better classification
 */
enum class ErrorCategory {
    NETWORK,        // Network connectivity issues
    PROTOCOL,       // Protocol-level errors
    AUTHENTICATION, // Authentication/authorization errors
    TIMEOUT,        // Timeout-related errors
    RESOURCE,       // Resource exhaustion errors
    VALIDATION,     // Data validation errors
    INTERNAL        // Internal system errors
};

/**
 * @brief Recovery strategies
 */
enum class RecoveryStrategy {
    IGNORE,         // Ignore the error and continue
    RETRY,          // Retry the operation
    EXPONENTIAL_BACKOFF, // Retry with exponential backoff
    CIRCUIT_BREAKER,     // Use circuit breaker pattern
    FAILOVER,       // Switch to alternative resource
    GRACEFUL_DEGRADATION, // Reduce functionality gracefully
    RESTART,        // Restart the component
    ESCALATE        // Escalate to higher level handler
};

/**
 * @brief Error information structure
 */
struct ErrorInfo {
    std::string errorCode;
    std::string errorMessage;
    ErrorSeverity severity;
    ErrorCategory category;
    std::chrono::system_clock::time_point timestamp;
    std::string context;
    std::unordered_map<std::string, std::string> metadata;
    
    ErrorInfo(const std::string& code, const std::string& message, 
              ErrorSeverity sev = ErrorSeverity::MEDIUM,
              ErrorCategory cat = ErrorCategory::INTERNAL)
        : errorCode(code), errorMessage(message), severity(sev), category(cat),
          timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Circuit breaker states
 */
enum class CircuitBreakerState {
    CLOSED,     // Normal operation
    OPEN,       // Circuit is open, failing fast
    HALF_OPEN   // Testing if service is back
};

/**
 * @brief Circuit breaker for preventing cascading failures
 */
class CircuitBreaker {
public:
    struct Config {
        size_t failureThreshold = 5;
        std::chrono::milliseconds timeout{30000};
        std::chrono::milliseconds retryTimeout{60000};
        size_t successThreshold = 3; // For half-open state
    };

    explicit CircuitBreaker(const Config& config = {});
    
    bool canExecute() const;
    void recordSuccess();
    void recordFailure();
    void reset();
    
    CircuitBreakerState getState() const { return state.load(); }
    size_t getFailureCount() const { return failureCount.load(); }
    size_t getSuccessCount() const { return successCount.load(); }

private:
    Config config;
    std::atomic<CircuitBreakerState> state{CircuitBreakerState::CLOSED};
    std::atomic<size_t> failureCount{0};
    std::atomic<size_t> successCount{0};
    std::atomic<std::chrono::system_clock::time_point> lastFailureTime;
    mutable std::mutex stateMutex;
    
    void transitionToOpen();
    void transitionToHalfOpen();
    void transitionToClosed();
    bool shouldAttemptReset() const;
};

/**
 * @brief Retry policy with exponential backoff
 */
class RetryPolicy {
public:
    struct Config {
        size_t maxRetries = 3;
        std::chrono::milliseconds initialDelay{100};
        double backoffMultiplier = 2.0;
        std::chrono::milliseconds maxDelay{30000};
        std::vector<ErrorCategory> retryableCategories = {
            ErrorCategory::NETWORK, ErrorCategory::TIMEOUT, ErrorCategory::RESOURCE
        };
    };

    explicit RetryPolicy(const Config& config = {});
    
    bool shouldRetry(const ErrorInfo& error, size_t attemptCount) const;
    std::chrono::milliseconds getDelay(size_t attemptCount) const;
    void reset();

private:
    Config config;
    bool isRetryableError(const ErrorInfo& error) const;
};

/**
 * @brief Error handler with multiple recovery strategies
 */
class ErrorHandler {
public:
    using ErrorCallback = std::function<void(const ErrorInfo&)>;
    using RecoveryCallback = std::function<bool(const ErrorInfo&, RecoveryStrategy)>;

    struct Config {
        bool enableCircuitBreaker = true;
        bool enableRetryPolicy = true;
        bool enableGracefulDegradation = true;
        size_t maxErrorHistory = 1000;
        std::chrono::minutes errorHistoryRetention{60};
    };

    explicit ErrorHandler(const Config& config = {});
    ~ErrorHandler();

    // Error handling
    void handleError(const ErrorInfo& error);
    void registerErrorCallback(const std::string& errorCode, ErrorCallback callback);
    void registerRecoveryCallback(RecoveryStrategy strategy, RecoveryCallback callback);

    // Recovery strategies
    bool executeWithRetry(const std::string& operation, 
                         std::function<bool()> func,
                         const RetryPolicy::Config& retryConfig = RetryPolicy::Config{});
    
    bool executeWithCircuitBreaker(const std::string& operation,
                                  std::function<bool()> func,
                                  const CircuitBreaker::Config& cbConfig = CircuitBreaker::Config{});

    // Statistics and monitoring
    struct Statistics {
        std::atomic<size_t> totalErrors{0};
        std::atomic<size_t> recoveredErrors{0};
        std::atomic<size_t> criticalErrors{0};
        std::atomic<size_t> circuitBreakerTrips{0};
        std::atomic<size_t> retryAttempts{0};
        std::atomic<size_t> successfulRetries{0};
    };

    const Statistics& getStatistics() const { return stats; }
    void resetStatistics();

    // Error history and analysis
    std::vector<ErrorInfo> getErrorHistory(ErrorCategory category = ErrorCategory::INTERNAL) const;
    std::vector<ErrorInfo> getRecentErrors(std::chrono::minutes duration) const;
    void clearErrorHistory();

    // Health monitoring
    bool isHealthy() const;
    double getErrorRate(std::chrono::minutes window) const;
    std::unordered_map<ErrorCategory, size_t> getErrorCategoryCounts() const;

private:
    Config config;
    Statistics stats;
    
    // Error callbacks and recovery strategies
    std::unordered_map<std::string, ErrorCallback> errorCallbacks;
    std::unordered_map<RecoveryStrategy, RecoveryCallback> recoveryCallbacks;
    
    // Circuit breakers per operation
    std::unordered_map<std::string, std::unique_ptr<CircuitBreaker>> circuitBreakers;
    
    // Error history
    std::vector<ErrorInfo> errorHistory;
    mutable std::mutex historyMutex;
    
    // Internal methods
    RecoveryStrategy determineRecoveryStrategy(const ErrorInfo& error) const;
    bool executeRecovery(const ErrorInfo& error, RecoveryStrategy strategy);
    void addToHistory(const ErrorInfo& error);
    void cleanupOldErrors();
    
    // Default recovery implementations
    bool defaultRetryRecovery(const ErrorInfo& error, RecoveryStrategy strategy);
    bool defaultCircuitBreakerRecovery(const ErrorInfo& error, RecoveryStrategy strategy);
    bool defaultGracefulDegradationRecovery(const ErrorInfo& error, RecoveryStrategy strategy);
};

} // namespace client
} // namespace hydrogen

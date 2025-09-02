#pragma once

#include "../core/service_registry.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <chrono>
#include <exception>

namespace hydrogen {
namespace server {
namespace infrastructure {

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * @brief Error category enumeration
 */
enum class ErrorCategory {
    SYSTEM,
    NETWORK,
    AUTHENTICATION,
    AUTHORIZATION,
    VALIDATION,
    CONFIGURATION,
    DEVICE,
    PROTOCOL,
    DATABASE,
    EXTERNAL_SERVICE,
    UNKNOWN
};

/**
 * @brief Error recovery strategy
 */
enum class RecoveryStrategy {
    NONE,           // No automatic recovery
    RETRY,          // Retry the operation
    FALLBACK,       // Use fallback mechanism
    RESTART,        // Restart component
    IGNORE,         // Ignore the error
    ESCALATE        // Escalate to higher level
};

/**
 * @brief Error information structure
 */
struct ErrorInfo {
    std::string errorId;
    std::string errorCode;
    std::string message;
    std::string details;
    ErrorSeverity severity;
    ErrorCategory category;
    std::string component;
    std::string operation;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> context;
    std::string stackTrace;
    std::string userId;
    std::string sessionId;
    std::string remoteAddress;
};

/**
 * @brief Error recovery action
 */
struct RecoveryAction {
    std::string actionId;
    std::string name;
    std::string description;
    RecoveryStrategy strategy;
    std::function<bool()> action;
    int maxRetries;
    std::chrono::milliseconds retryDelay;
    std::chrono::seconds timeout;
    bool isAsync;
};

/**
 * @brief Error pattern for matching and handling
 */
struct ErrorPattern {
    std::string patternId;
    std::string name;
    std::string errorCodePattern;
    std::string messagePattern;
    ErrorCategory category;
    ErrorSeverity minSeverity;
    std::string componentPattern;
    std::vector<RecoveryAction> recoveryActions;
    bool enabled;
    int priority;
};

/**
 * @brief Error handler interface
 * 
 * Provides comprehensive error handling, recovery, and reporting capabilities
 * for the server infrastructure.
 */
class IErrorHandler : public core::IService {
public:
    virtual ~IErrorHandler() = default;

    // Error reporting
    virtual std::string reportError(const ErrorInfo& error) = 0;
    virtual std::string reportError(const std::string& errorCode, const std::string& message,
                                  ErrorSeverity severity = ErrorSeverity::MEDIUM,
                                  ErrorCategory category = ErrorCategory::UNKNOWN,
                                  const std::string& component = "") = 0;
    virtual std::string reportException(const std::exception& ex, const std::string& component = "",
                                      const std::string& operation = "") = 0;

    // Error retrieval
    virtual ErrorInfo getError(const std::string& errorId) const = 0;
    virtual std::vector<ErrorInfo> getErrors(ErrorSeverity minSeverity = ErrorSeverity::LOW,
                                           const std::string& component = "",
                                           size_t limit = 100) const = 0;
    virtual std::vector<ErrorInfo> getRecentErrors(std::chrono::minutes timeWindow,
                                                  ErrorSeverity minSeverity = ErrorSeverity::LOW) const = 0;

    // Error pattern management
    virtual bool addErrorPattern(const ErrorPattern& pattern) = 0;
    virtual bool removeErrorPattern(const std::string& patternId) = 0;
    virtual bool updateErrorPattern(const ErrorPattern& pattern) = 0;
    virtual ErrorPattern getErrorPattern(const std::string& patternId) const = 0;
    virtual std::vector<ErrorPattern> getAllErrorPatterns() const = 0;
    virtual bool enableErrorPattern(const std::string& patternId, bool enabled) = 0;

    // Recovery action management
    virtual bool addRecoveryAction(const std::string& patternId, const RecoveryAction& action) = 0;
    virtual bool removeRecoveryAction(const std::string& patternId, const std::string& actionId) = 0;
    virtual std::vector<RecoveryAction> getRecoveryActions(const std::string& patternId) const = 0;

    // Error handling and recovery
    virtual bool handleError(const std::string& errorId) = 0;
    virtual bool executeRecovery(const std::string& errorId, const std::string& actionId = "") = 0;
    virtual std::vector<std::string> getAvailableRecoveryActions(const std::string& errorId) const = 0;
    virtual bool isRecoveryInProgress(const std::string& errorId) const = 0;

    // Error suppression and filtering
    virtual bool suppressError(const std::string& errorCode, std::chrono::minutes duration) = 0;
    virtual bool unsuppressError(const std::string& errorCode) = 0;
    virtual bool isErrorSuppressed(const std::string& errorCode) const = 0;
    virtual std::vector<std::string> getSuppressedErrors() const = 0;

    // Error aggregation and analysis
    virtual std::unordered_map<std::string, size_t> getErrorCountByCode(std::chrono::hours timeWindow) const = 0;
    virtual std::unordered_map<std::string, size_t> getErrorCountByComponent(std::chrono::hours timeWindow) const = 0;
    virtual std::unordered_map<ErrorCategory, size_t> getErrorCountByCategory(std::chrono::hours timeWindow) const = 0;
    virtual std::unordered_map<ErrorSeverity, size_t> getErrorCountBySeverity(std::chrono::hours timeWindow) const = 0;

    // Error rate monitoring
    virtual double getErrorRate(const std::string& component = "", std::chrono::minutes timeWindow = std::chrono::minutes(5)) const = 0;
    virtual bool isErrorRateExceeded(const std::string& component = "", double threshold = 0.1) const = 0;
    virtual void setErrorRateThreshold(const std::string& component, double threshold) = 0;
    virtual std::unordered_map<std::string, double> getErrorRateThresholds() const = 0;

    // Circuit breaker functionality
    virtual bool enableCircuitBreaker(const std::string& component, int failureThreshold, 
                                    std::chrono::seconds timeout) = 0;
    virtual bool disableCircuitBreaker(const std::string& component) = 0;
    virtual bool isCircuitBreakerOpen(const std::string& component) const = 0;
    virtual bool resetCircuitBreaker(const std::string& component) = 0;

    // Error notification and alerting
    virtual bool addNotificationChannel(const std::string& channelId, const std::string& type,
                                      const std::unordered_map<std::string, std::string>& config) = 0;
    virtual bool removeNotificationChannel(const std::string& channelId) = 0;
    virtual bool sendNotification(const std::string& channelId, const ErrorInfo& error) = 0;
    virtual bool setNotificationRule(ErrorSeverity minSeverity, const std::vector<std::string>& channels) = 0;

    // Error reporting and export
    virtual std::string generateErrorReport(std::chrono::hours timeWindow = std::chrono::hours(24)) const = 0;
    virtual bool exportErrors(const std::string& filePath, const std::string& format = "json",
                            std::chrono::hours timeWindow = std::chrono::hours(24)) const = 0;
    virtual std::string getErrorSummary(std::chrono::hours timeWindow = std::chrono::hours(1)) const = 0;

    // Error cleanup and archiving
    virtual bool cleanupOldErrors(std::chrono::hours maxAge) = 0;
    virtual bool archiveErrors(const std::string& archivePath, std::chrono::hours maxAge) = 0;
    virtual size_t getErrorCount() const = 0;
    virtual size_t getArchivedErrorCount() const = 0;

    // Configuration
    virtual void setMaxErrorHistory(size_t maxErrors) = 0;
    virtual void setErrorRetentionPeriod(std::chrono::hours period) = 0;
    virtual void setAutoRecoveryEnabled(bool enabled) = 0;
    virtual bool isAutoRecoveryEnabled() const = 0;

    // Event callbacks
    using ErrorEventCallback = std::function<void(const ErrorInfo& error)>;
    using RecoveryEventCallback = std::function<void(const std::string& errorId, const std::string& actionId, bool success)>;
    using CircuitBreakerEventCallback = std::function<void(const std::string& component, const std::string& event)>;

    virtual void setErrorEventCallback(ErrorEventCallback callback) = 0;
    virtual void setRecoveryEventCallback(RecoveryEventCallback callback) = 0;
    virtual void setCircuitBreakerEventCallback(CircuitBreakerEventCallback callback) = 0;

    // Utility methods
    virtual std::string generateErrorId() const = 0;
    virtual std::string getStackTrace() const = 0;
    virtual bool isKnownError(const std::string& errorCode) const = 0;
    virtual std::vector<std::string> getSimilarErrors(const std::string& errorCode) const = 0;
};

/**
 * @brief Error handler factory
 */
class ErrorHandlerFactory : public core::IServiceFactory {
public:
    std::unique_ptr<core::IService> createService(
        const std::string& serviceName,
        const std::unordered_map<std::string, std::string>& config = {}
    ) override;
    
    std::vector<std::string> getSupportedServices() const override;
    bool isServiceSupported(const std::string& serviceName) const override;
};

/**
 * @brief RAII error scope for automatic error handling
 */
class ErrorScope {
public:
    ErrorScope(std::shared_ptr<IErrorHandler> errorHandler, const std::string& component, const std::string& operation);
    ~ErrorScope();
    
    void reportError(const std::string& errorCode, const std::string& message, ErrorSeverity severity = ErrorSeverity::MEDIUM);
    void reportException(const std::exception& ex);
    void setContext(const std::string& key, const std::string& value);
    
private:
    std::shared_ptr<IErrorHandler> errorHandler_;
    std::string component_;
    std::string operation_;
    std::unordered_map<std::string, std::string> context_;
    std::chrono::steady_clock::time_point startTime_;
};

/**
 * @brief Exception classes for structured error handling
 */
class ServerException : public std::exception {
public:
    ServerException(const std::string& errorCode, const std::string& message, 
                   ErrorSeverity severity = ErrorSeverity::MEDIUM,
                   ErrorCategory category = ErrorCategory::UNKNOWN);
    
    const char* what() const noexcept override;
    const std::string& getErrorCode() const;
    ErrorSeverity getSeverity() const;
    ErrorCategory getCategory() const;
    
private:
    std::string errorCode_;
    std::string message_;
    ErrorSeverity severity_;
    ErrorCategory category_;
};

class ConfigurationException : public ServerException {
public:
    ConfigurationException(const std::string& message);
};

class AuthenticationException : public ServerException {
public:
    AuthenticationException(const std::string& message);
};

class AuthorizationException : public ServerException {
public:
    AuthorizationException(const std::string& message);
};

class ValidationException : public ServerException {
public:
    ValidationException(const std::string& message);
};

class NetworkException : public ServerException {
public:
    NetworkException(const std::string& message);
};

class DeviceException : public ServerException {
public:
    DeviceException(const std::string& message);
};

/**
 * @brief Utility macros for error handling
 */
#define ERROR_SCOPE(handler, component, operation) ErrorScope _error_scope(handler, component, operation)
#define REPORT_ERROR(scope, code, message, severity) scope.reportError(code, message, severity)
#define REPORT_EXCEPTION(scope, ex) scope.reportException(ex)

} // namespace infrastructure
} // namespace server
} // namespace hydrogen

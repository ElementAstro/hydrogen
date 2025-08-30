#pragma once

#include "message.h"
#include "device_health.h"
#include "device_lifecycle.h"

#include <atomic>
#include <functional>
#include <map>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <queue>
#include <unordered_set>

namespace astrocomm {
namespace core {

/**
 * @brief Enhanced error handling strategies with device-specific behaviors
 */
enum class ErrorHandlingStrategy {
    IGNORE,                    // Ignore the error
    RETRY,                     // Simple retry with backoff
    RETRY_EXPONENTIAL,         // Exponential backoff retry
    RETRY_WITH_RESET,          // Retry after device reset
    DEVICE_RESTART,            // Restart the device
    DEVICE_RECONNECT,          // Reconnect the device
    FAILOVER_PRIMARY,          // Switch to primary backup
    FAILOVER_SECONDARY,        // Switch to secondary backup
    ESCALATE_TO_OPERATOR,      // Notify human operator
    QUARANTINE_DEVICE,         // Isolate device from system
    AUTOMATIC_RECOVERY,        // Attempt automatic recovery sequence
    MAINTENANCE_MODE,          // Put device in maintenance mode
    NOTIFY,                    // Notify about the error
    RESTART_DEVICE,            // Restart the specific device
    FAILOVER,                  // General failover strategy
    CUSTOM                     // Use custom recovery logic
};

/**
 * @brief Error severity levels for prioritization
 */
enum class ErrorSeverity {
    LOW,        // Minor issues, continue operation
    MEDIUM,     // Moderate issues, may affect performance
    HIGH,       // Serious issues, may cause failures
    CRITICAL,   // Critical issues, immediate attention required
    FATAL       // Fatal errors, system shutdown may be required
};

/**
 * @brief Enhanced error handling context structure with additional metadata
 */
struct ErrorContext {
    std::string deviceId;
    std::string errorCode;
    std::string errorMessage;
    std::string command;
    nlohmann::json parameters;
    int retryCount{0};
    int maxRetries{3};
    std::chrono::system_clock::time_point errorTime;

    // Enhanced fields
    ErrorSeverity severity = ErrorSeverity::MEDIUM;
    std::string deviceType;
    std::string errorCategory;
    DeviceHealthStatus deviceHealth = DeviceHealthStatus::UNKNOWN;
    DeviceLifecycleState deviceState = DeviceLifecycleState::UNINITIALIZED;
    std::vector<std::string> affectedComponents;
    std::vector<std::string> recoveryActions;
    nlohmann::json diagnosticData;
    std::chrono::milliseconds recoveryTimeout{30000};
    bool requiresOperatorIntervention = false;
    std::string operatorMessage;
    int escalationLevel = 0;
    std::chrono::system_clock::time_point lastRecoveryAttempt;

    // Creates a context initialized from an error message
    static ErrorContext fromErrorMessage(const ErrorMessage &errorMsg);

    // Enhanced methods
    nlohmann::json toJson() const;
    static ErrorContext fromJson(const nlohmann::json& j);
    bool shouldEscalate() const;
    bool isRecoverable() const;
    std::chrono::milliseconds getBackoffDelay() const;
};

/**
 * Custom error handler type
 */
using ErrorHandlerFunc = std::function<bool(const ErrorContext &)>;

/**
 * @brief Enhanced error recovery and exception handling manager
 */
class ErrorRecoveryManager {
public:
    ErrorRecoveryManager();
    ~ErrorRecoveryManager();

    // Basic error handling
    void registerErrorHandler(const std::string &errorCode,
                             ErrorHandlingStrategy strategy,
                             ErrorHandlerFunc customHandler = nullptr);

    void registerDeviceErrorHandler(const std::string &deviceId,
                                   const std::string &errorCode,
                                   ErrorHandlingStrategy strategy,
                                   ErrorHandlerFunc customHandler = nullptr);

    bool handleError(const ErrorMessage &errorMsg);
    bool handleError(const ErrorContext &context);

    void setGlobalErrorHandler(ErrorHandlerFunc handler);
    void setAutoRetryEnabled(bool enabled);
    void setDefaultMaxRetries(int maxRetries);
    void setRetryDelay(int delayMs);

    // Enhanced error handling methods
    void registerErrorHandlerWithSeverity(const std::string &errorCode,
                                         ErrorSeverity severity,
                                         ErrorHandlingStrategy strategy,
                                         ErrorHandlerFunc customHandler = nullptr);

    void registerDeviceTypeErrorHandler(const std::string &deviceType,
                                       const std::string &errorCode,
                                       ErrorHandlingStrategy strategy,
                                       ErrorHandlerFunc customHandler = nullptr);

    void registerCategoryErrorHandler(const std::string &category,
                                     ErrorHandlingStrategy strategy,
                                     ErrorHandlerFunc customHandler = nullptr);

    bool handleErrorWithContext(const ErrorContext &context);
    bool attemptRecovery(const std::string &deviceId, const ErrorContext &context);
    bool escalateError(const ErrorContext &context);

    // Recovery sequence management
    void registerRecoverySequence(const std::string &deviceType,
                                 const std::vector<ErrorHandlingStrategy> &sequence);
    bool executeRecoverySequence(const std::string &deviceId, const ErrorContext &context);

    // Error monitoring and analysis
    void startErrorMonitoring();
    void stopErrorMonitoring();
    bool isErrorMonitoringActive() const { return monitoringActive_.load(); }

  /**
   * Get error statistics
   */
  struct ErrorStats {
    int totalErrors{0};
    int handledErrors{0};
    int retriedErrors{0};
    int ignoredErrors{0};
    std::map<std::string, int> errorCodeCounts;
  };

  ErrorStats getErrorStats() const;

  /**
   * Clear error statistics
   */
  void clearErrorStats();

  /**
   * Start the error recovery manager
   */
  void start();

  /**
   * Stop the error recovery manager
   */
  void stop();

private:
  struct ErrorHandlerInfo {
    ErrorHandlingStrategy strategy;
    ErrorHandlerFunc customHandler;
    int maxRetries{3};
    int retryDelayMs{1000};
  };

  // Error handler mappings
  std::shared_mutex handlersMutex_;
  std::map<std::string, ErrorHandlerInfo> globalHandlers_;
  std::map<std::string, std::map<std::string, ErrorHandlerInfo>> deviceHandlers_;

  // Global settings
  ErrorHandlerFunc globalErrorHandler_;
  bool autoRetryEnabled_{true};
  int defaultMaxRetries_{3};
  int defaultRetryDelayMs_{1000};

  // Statistics
  mutable std::shared_mutex statsMutex_;
  ErrorStats stats_;

  // Thread management
  std::atomic<bool> running_{false};
  std::atomic<bool> monitoringActive_{false};
  std::thread recoveryThread_;

  // Internal methods
  bool executeErrorHandler(const ErrorContext &context, const ErrorHandlerInfo &handlerInfo);
  bool retryOperation(const ErrorContext &context);
  void updateStats(const std::string &errorCode, bool handled);
};

} // namespace core
} // namespace astrocomm

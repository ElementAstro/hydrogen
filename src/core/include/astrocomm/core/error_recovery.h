#pragma once

#include "message.h"

#include <atomic>
#include <functional>
#include <map>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

namespace astrocomm {
namespace core {

/**
 * Error handling strategies enum
 */
enum class ErrorHandlingStrategy {
  IGNORE,         // Ignore the error
  RETRY,          // Retry the operation
  NOTIFY,         // Notify only, but do not handle
  RESTART_DEVICE, // Restart the device
  FAILOVER,       // Switch to a backup device
  CUSTOM          // Custom handling strategy
};

/**
 * Error handling context structure
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

  // Creates a context initialized from an error message
  static ErrorContext fromErrorMessage(const ErrorMessage &errorMsg);
};

/**
 * Custom error handler type
 */
using ErrorHandlerFunc = std::function<bool(const ErrorContext &)>;

/**
 * Error recovery and exception handling manager
 */
class ErrorRecoveryManager {
public:
  ErrorRecoveryManager();
  ~ErrorRecoveryManager();

  /**
   * Register an error handling strategy for a specific error code
   */
  void registerErrorHandler(const std::string &errorCode,
                           ErrorHandlingStrategy strategy,
                           ErrorHandlerFunc customHandler = nullptr);

  /**
   * Register a device-specific error handler
   */
  void registerDeviceErrorHandler(const std::string &deviceId,
                                 const std::string &errorCode,
                                 ErrorHandlingStrategy strategy,
                                 ErrorHandlerFunc customHandler = nullptr);

  /**
   * Handle an error message
   */
  bool handleError(const ErrorMessage &errorMsg);

  /**
   * Handle an error context
   */
  bool handleError(const ErrorContext &context);

  /**
   * Set global error handler (fallback)
   */
  void setGlobalErrorHandler(ErrorHandlerFunc handler);

  /**
   * Enable/disable automatic retry for failed operations
   */
  void setAutoRetryEnabled(bool enabled);

  /**
   * Set default maximum retry count
   */
  void setDefaultMaxRetries(int maxRetries);

  /**
   * Set retry delay in milliseconds
   */
  void setRetryDelay(int delayMs);

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
  std::thread recoveryThread_;

  // Internal methods
  bool executeErrorHandler(const ErrorContext &context, const ErrorHandlerInfo &handlerInfo);
  bool retryOperation(const ErrorContext &context);
  void updateStats(const std::string &errorCode, bool handled);
};

} // namespace core
} // namespace astrocomm

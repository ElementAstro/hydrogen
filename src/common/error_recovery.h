#pragma once

#include "common/message.h"

#include <atomic>
#include <functional>
#include <map>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

namespace astrocomm {

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
 *
 * Responsible for handling error messages in the system and performing
 * recovery operations according to the configured strategy
 */
class ErrorRecoveryManager {
public:
  ErrorRecoveryManager();
  ~ErrorRecoveryManager();

  /**
   * Starts the error handling service
   */
  void start();

  /**
   * Stops the error handling service
   */
  void stop();

  /**
   * Handles an error message
   *
   * @param errorMsg The error message
   * @return Whether the handling was successful
   */
  bool handleError(const ErrorMessage &errorMsg);

  /**
   * Sets the handling strategy for a specific error code
   *
   * @param errorCode The error code
   * @param strategy The handling strategy
   */
  void setErrorStrategy(const std::string &errorCode,
                        ErrorHandlingStrategy strategy);

  /**
   * Sets the handling strategy for a specific error code on a specific device
   *
   * @param deviceId The device ID
   * @param errorCode The error code
   * @param strategy The handling strategy
   */
  void setDeviceErrorStrategy(const std::string &deviceId,
                              const std::string &errorCode,
                              ErrorHandlingStrategy strategy);

  /**
   * Registers a custom error handler
   *
   * @param errorCode The error code
   * @param handler The handler function
   */
  void registerCustomHandler(const std::string &errorCode,
                             ErrorHandlerFunc handler);

  /**
   * Registers a device-specific custom error handler
   *
   * @param deviceId The device ID
   * @param errorCode The error code
   * @param handler The handler function
   */
  void registerDeviceCustomHandler(const std::string &deviceId,
                                   const std::string &errorCode,
                                   ErrorHandlerFunc handler);

  /**
   * Gets the list of unresolved errors
   *
   * @return A JSON array of unresolved errors
   */
  nlohmann::json getPendingErrors() const;

  /**
   * Gets the error handling history
   *
   * @param limit The number of records to return
   * @return A JSON array of error handling history
   */
  nlohmann::json getErrorHistory(int limit = 100) const;

  /**
   * Clears the error history
   */
  void clearErrorHistory();

  /**
   * Manually resolves a specific error
   *
   * @param errorId The error ID
   * @param resolution Description of the solution
   * @return Whether the resolution was successful
   */
  bool resolveError(const std::string &errorId, const std::string &resolution);

private:
  // Error handling thread
  std::thread errorHandlerThread;
  std::atomic<bool> running{false};

  // Error handling strategy mapping
  mutable std::shared_mutex strategiesMutex;
  std::map<std::string, ErrorHandlingStrategy> globalStrategies;
  std::map<std::string, std::map<std::string, ErrorHandlingStrategy>>
      deviceStrategies;

  // Custom error handler mapping
  mutable std::shared_mutex handlersMutex;
  std::map<std::string, ErrorHandlerFunc> customHandlers;
  std::map<std::string, std::map<std::string, ErrorHandlerFunc>>
      deviceCustomHandlers;

  // Unresolved errors
  mutable std::shared_mutex pendingErrorsMutex;
  std::map<std::string, ErrorContext> pendingErrors;

  // Error history records
  mutable std::shared_mutex errorHistoryMutex;
  std::vector<nlohmann::json> errorHistory;

  // Error handling worker thread
  void errorHandlerWorker();

  // Processes the error according to the strategy
  bool processError(const ErrorContext &context,
                    ErrorHandlingStrategy strategy);

  // Finds the strategy applicable to a specific error
  ErrorHandlingStrategy findStrategy(const std::string &deviceId,
                                     const std::string &errorCode);

  // Finds the custom handler applicable to a specific error
  ErrorHandlerFunc findCustomHandler(const std::string &deviceId,
                                     const std::string &errorCode);

  // Logs the error handling result
  void logErrorHandling(const ErrorContext &context, bool resolved,
                        const std::string &action);
};

} // namespace astrocomm
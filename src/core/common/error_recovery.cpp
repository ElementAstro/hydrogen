#include "hydrogen/core/error_recovery.h"
#include "hydrogen/core/utils.h"

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <thread>

namespace hydrogen {
namespace core {

ErrorContext ErrorContext::fromErrorMessage(const ErrorMessage &errorMsg) {
  ErrorContext context;
  context.deviceId = errorMsg.getDeviceId();
  context.errorCode = errorMsg.getErrorCode();
  context.errorMessage = errorMsg.getErrorMessage();
  context.errorTime = std::chrono::system_clock::now();

  // Extract additional details from error message
  auto details = errorMsg.getDetails();
  if (details.contains("command")) {
    context.command = details["command"];
  }
  if (details.contains("parameters")) {
    context.parameters = details["parameters"];
  }
  if (details.contains("retryCount")) {
    context.retryCount = details["retryCount"];
  }
  if (details.contains("maxRetries")) {
    context.maxRetries = details["maxRetries"];
  }

  return context;
}

ErrorRecoveryManager::ErrorRecoveryManager() = default;

ErrorRecoveryManager::~ErrorRecoveryManager() { stop(); }

void ErrorRecoveryManager::registerErrorHandler(
    const std::string &errorCode, ErrorHandlingStrategy strategy,
    ErrorHandlerFunc customHandler) {
  std::lock_guard<std::shared_mutex> lock(handlersMutex_);

  ErrorHandlerInfo info;
  info.strategy = strategy;
  info.customHandler = customHandler;
  info.maxRetries = defaultMaxRetries_;
  info.retryDelayMs = defaultRetryDelayMs_;

  globalHandlers_[errorCode] = info;
}

void ErrorRecoveryManager::registerDeviceErrorHandler(
    const std::string &deviceId, const std::string &errorCode,
    ErrorHandlingStrategy strategy, ErrorHandlerFunc customHandler) {
  std::lock_guard<std::shared_mutex> lock(handlersMutex_);

  ErrorHandlerInfo info;
  info.strategy = strategy;
  info.customHandler = customHandler;
  info.maxRetries = defaultMaxRetries_;
  info.retryDelayMs = defaultRetryDelayMs_;

  deviceHandlers_[deviceId][errorCode] = info;
}

bool ErrorRecoveryManager::handleError(const ErrorMessage &errorMsg) {
  ErrorContext context = ErrorContext::fromErrorMessage(errorMsg);
  return handleError(context);
}

bool ErrorRecoveryManager::handleError(const ErrorContext &context) {
  updateStats(context.errorCode, false);

  // Look for device-specific handler first
  {
    std::shared_lock<std::shared_mutex> lock(handlersMutex_);

    auto deviceIt = deviceHandlers_.find(context.deviceId);
    if (deviceIt != deviceHandlers_.end()) {
      auto errorIt = deviceIt->second.find(context.errorCode);
      if (errorIt != deviceIt->second.end()) {
        bool handled = executeErrorHandler(context, errorIt->second);
        updateStats(context.errorCode, handled);
        return handled;
      }
    }

    // Look for global handler
    auto globalIt = globalHandlers_.find(context.errorCode);
    if (globalIt != globalHandlers_.end()) {
      bool handled = executeErrorHandler(context, globalIt->second);
      updateStats(context.errorCode, handled);
      return handled;
    }
  }

  // Use global error handler as fallback
  if (globalErrorHandler_) {
    bool handled = globalErrorHandler_(context);
    updateStats(context.errorCode, handled);
    return handled;
  }

  // No handler found
  return false;
}

void ErrorRecoveryManager::setGlobalErrorHandler(ErrorHandlerFunc handler) {
  globalErrorHandler_ = handler;
}

void ErrorRecoveryManager::setAutoRetryEnabled(bool enabled) {
  autoRetryEnabled_ = enabled;
}

void ErrorRecoveryManager::setDefaultMaxRetries(int maxRetries) {
  defaultMaxRetries_ = maxRetries;
}

void ErrorRecoveryManager::setRetryDelay(int delayMs) {
  defaultRetryDelayMs_ = delayMs;
}

ErrorRecoveryManager::ErrorStats ErrorRecoveryManager::getErrorStats() const {
  std::shared_lock<std::shared_mutex> lock(statsMutex_);
  return stats_;
}

void ErrorRecoveryManager::clearErrorStats() {
  std::lock_guard<std::shared_mutex> lock(statsMutex_);
  stats_ = ErrorStats{};
}

void ErrorRecoveryManager::start() {
  if (running_.load()) {
    return;
  }

  running_.store(true);
  // Could start a background thread for periodic cleanup or monitoring
}

void ErrorRecoveryManager::stop() {
  if (!running_.load()) {
    return;
  }

  running_.store(false);

  if (recoveryThread_.joinable()) {
    recoveryThread_.join();
  }
}

bool ErrorRecoveryManager::executeErrorHandler(
    const ErrorContext &context, const ErrorHandlerInfo &handlerInfo) {
  switch (handlerInfo.strategy) {
  case ErrorHandlingStrategy::IGNORE:
    return true;

  case ErrorHandlingStrategy::RETRY:
    if (autoRetryEnabled_ && context.retryCount < handlerInfo.maxRetries) {
      return retryOperation(context);
    }
    return false;

  case ErrorHandlingStrategy::NOTIFY:
    // Just log or notify, don't actually handle
    return false;

  case ErrorHandlingStrategy::RESTART_DEVICE:
    // Would need device management integration
    return false;

  case ErrorHandlingStrategy::FAILOVER:
    // Would need device management integration
    return false;

  case ErrorHandlingStrategy::CUSTOM:
    if (handlerInfo.customHandler) {
      return handlerInfo.customHandler(context);
    }
    return false;

  default:
    return false;
  }
}

bool ErrorRecoveryManager::retryOperation(const ErrorContext &context) {
  (void)context; // Suppress unused parameter warning

  // Sleep for retry delay
  std::this_thread::sleep_for(std::chrono::milliseconds(defaultRetryDelayMs_));

  // In a real implementation, this would re-execute the original command
  // For now, just return true to indicate retry was attempted
  return true;
}

void ErrorRecoveryManager::updateStats(const std::string &errorCode,
                                       bool handled) {
  std::lock_guard<std::shared_mutex> lock(statsMutex_);

  stats_.totalErrors++;
  stats_.errorCodeCounts[errorCode]++;

  if (handled) {
    stats_.handledErrors++;
  } else {
    stats_.ignoredErrors++;
  }
}

} // namespace core
} // namespace hydrogen

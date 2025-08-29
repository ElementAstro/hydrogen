#include "common/error_recovery.h"
#include "common/utils.h"
#include <chrono>
#include <spdlog/spdlog.h>


#ifdef _WIN32
#undef IGNORE
#endif

namespace astrocomm {

ErrorContext ErrorContext::fromErrorMessage(const ErrorMessage &errorMsg) {
  ErrorContext context;
  context.deviceId = errorMsg.getDeviceId();
  context.errorCode = errorMsg.getErrorCode();
  context.errorMessage = errorMsg.getErrorMessage();
  context.errorTime = std::chrono::system_clock::now();

  // Try to extract relevant information from details
  auto details = errorMsg.getDetails();
  if (!details.is_null()) {
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
  }

  return context;
}

ErrorRecoveryManager::ErrorRecoveryManager() {
  // Set some default error handling strategies
  std::unique_lock<std::shared_mutex> lock(strategiesMutex);
  globalStrategies["CONNECTION_LOST"] = ErrorHandlingStrategy::RETRY;
  globalStrategies["TIMEOUT"] = ErrorHandlingStrategy::RETRY;
  globalStrategies["DEVICE_NOT_FOUND"] = ErrorHandlingStrategy::NOTIFY;
  globalStrategies["PERMISSION_DENIED"] = ErrorHandlingStrategy::NOTIFY;
  globalStrategies["INVALID_COMMAND"] = ErrorHandlingStrategy::NOTIFY;
  globalStrategies["INTERNAL_ERROR"] = ErrorHandlingStrategy::NOTIFY;
  lock.unlock();

  spdlog::info("[ErrorRecoveryManager] Initialization completed");
}

ErrorRecoveryManager::~ErrorRecoveryManager() { stop(); }

void ErrorRecoveryManager::start() {
  bool expected = false;
  if (!running.compare_exchange_strong(expected, true)) {
    return; // Already running
  }

  errorHandlerThread =
      std::thread(&ErrorRecoveryManager::errorHandlerWorker, this);

  spdlog::info("[ErrorRecoveryManager] 错误处理服务已启动");
}

void ErrorRecoveryManager::stop() {
  bool expected = true;
  if (!running.compare_exchange_strong(expected, false)) {
    return; // 已经停止
  }

  if (errorHandlerThread.joinable()) {
    errorHandlerThread.join();
  }

  spdlog::info("[ErrorRecoveryManager] 错误处理服务已停止");
}

bool ErrorRecoveryManager::handleError(const ErrorMessage &errorMsg) {
  // 创建错误上下文
  ErrorContext context = ErrorContext::fromErrorMessage(errorMsg);

  // 生成错误ID
  std::string errorId = generateUuid();

  // 查找适用的处理策略
  ErrorHandlingStrategy strategy =
      findStrategy(context.deviceId, context.errorCode);

  spdlog::info("[ErrorRecoveryManager] 处理错误 {}: {} (设备: {}, 策略: {})",
               errorId, context.errorCode, context.deviceId,
               static_cast<int>(strategy));

  // 处理错误
  bool resolved = processError(context, strategy);

  // 如果未解决，添加到未解决错误列表
  if (!resolved) {
    std::unique_lock<std::shared_mutex> lock(pendingErrorsMutex);
    pendingErrors[errorId] = context;
    lock.unlock();

    spdlog::warn("[ErrorRecoveryManager] 错误未解决，已添加到待处理列表: {}",
                 errorId);
  }

  // 记录错误处理结果
  std::string action;
  switch (strategy) {
  case ErrorHandlingStrategy::IGNORE:
    action = "忽略";
    break;
  case ErrorHandlingStrategy::RETRY:
    action = "重试";
    break;
  case ErrorHandlingStrategy::NOTIFY:
    action = "通知";
    break;
  case ErrorHandlingStrategy::RESTART_DEVICE:
    action = "重启设备";
    break;
  case ErrorHandlingStrategy::FAILOVER:
    action = "切换到备用设备";
    break;
  case ErrorHandlingStrategy::CUSTOM:
    action = "自定义处理";
    break;
  }

  logErrorHandling(context, resolved, action);

  return resolved;
}

void ErrorRecoveryManager::setErrorStrategy(const std::string &errorCode,
                                            ErrorHandlingStrategy strategy) {
  std::unique_lock<std::shared_mutex> lock(strategiesMutex);
  globalStrategies[errorCode] = strategy;
  lock.unlock();

  spdlog::info("[ErrorRecoveryManager] 设置错误 {} 的处理策略: {}", errorCode,
               static_cast<int>(strategy));
}

void ErrorRecoveryManager::setDeviceErrorStrategy(
    const std::string &deviceId, const std::string &errorCode,
    ErrorHandlingStrategy strategy) {
  std::unique_lock<std::shared_mutex> lock(strategiesMutex);
  deviceStrategies[deviceId][errorCode] = strategy;
  lock.unlock();

  spdlog::info("[ErrorRecoveryManager] 设置设备 {} 错误 {} 的处理策略: {}",
               deviceId, errorCode, static_cast<int>(strategy));
}

void ErrorRecoveryManager::registerCustomHandler(const std::string &errorCode,
                                                 ErrorHandlerFunc handler) {
  std::unique_lock<std::shared_mutex> lock(handlersMutex);
  customHandlers[errorCode] = handler;
  lock.unlock();

  spdlog::info("[ErrorRecoveryManager] 注册错误 {} 的自定义处理器", errorCode);
}

void ErrorRecoveryManager::registerDeviceCustomHandler(
    const std::string &deviceId, const std::string &errorCode,
    ErrorHandlerFunc handler) {
  std::unique_lock<std::shared_mutex> lock(handlersMutex);
  deviceCustomHandlers[deviceId][errorCode] = handler;
  lock.unlock();

  spdlog::info("[ErrorRecoveryManager] 注册设备 {} 错误 {} 的自定义处理器",
               deviceId, errorCode);
}

nlohmann::json ErrorRecoveryManager::getPendingErrors() const {
  std::shared_lock<std::shared_mutex> lock(pendingErrorsMutex);

  nlohmann::json result = nlohmann::json::array();

  for (const auto &[errorId, context] : pendingErrors) {
    nlohmann::json errorInfo = {
        {"errorId", errorId},
        {"deviceId", context.deviceId},
        {"errorCode", context.errorCode},
        {"errorMessage", context.errorMessage},
        {"timestamp", std::chrono::system_clock::to_time_t(context.errorTime)}};

    if (!context.command.empty()) {
      errorInfo["command"] = context.command;
    }

    if (!context.parameters.is_null()) {
      errorInfo["parameters"] = context.parameters;
    }

    result.push_back(errorInfo);
  }

  return result;
}

nlohmann::json ErrorRecoveryManager::getErrorHistory(int limit) const {
  std::shared_lock<std::shared_mutex> lock(errorHistoryMutex);

  nlohmann::json result = nlohmann::json::array();

  int count = 0;
  for (auto it = errorHistory.rbegin();
       it != errorHistory.rend() && count < limit; ++it, ++count) {
    result.push_back(*it);
  }

  return result;
}

void ErrorRecoveryManager::clearErrorHistory() {
  std::unique_lock<std::shared_mutex> lock(errorHistoryMutex);
  errorHistory.clear();
  lock.unlock();

  spdlog::info("[ErrorRecoveryManager] 错误历史已清除");
}

bool ErrorRecoveryManager::resolveError(const std::string &errorId,
                                        const std::string &resolution) {
  ErrorContext context;
  bool found = false;

  {
    std::unique_lock<std::shared_mutex> lock(pendingErrorsMutex);
    auto it = pendingErrors.find(errorId);
    if (it == pendingErrors.end()) {
      lock.unlock();
      spdlog::warn("[ErrorRecoveryManager] 尝试解决不存在的错误: {}", errorId);
      return false;
    }

    // 获取错误上下文
    context = it->second;

    // 从未解决列表中移除
    pendingErrors.erase(it);
    found = true;
  }

  if (found) {
    // 记录解决结果
    logErrorHandling(context, true, "手动解决: " + resolution);
    spdlog::info("[ErrorRecoveryManager] 错误 {} 已手动解决: {}", errorId,
                 resolution);
  }

  return found;
}

void ErrorRecoveryManager::errorHandlerWorker() {
  spdlog::info("[ErrorRecoveryManager] 错误处理工作线程已启动");

  while (running.load(std::memory_order_relaxed)) {
    // 需要处理的错误集合
    std::vector<std::pair<std::string, ErrorContext>> errorsToRetry;
    std::vector<std::string> errorsToRemove;

    // 首先，我们使用共享锁获取需要重试的错误
    {
      std::shared_lock<std::shared_mutex> lock(pendingErrorsMutex);
      for (const auto &[errorId, context] : pendingErrors) {
        if (context.retryCount < context.maxRetries) {
          errorsToRetry.emplace_back(errorId, context);
        } else {
          errorsToRemove.push_back(errorId);
        }
      }
    }

    // 处理达到最大重试次数的错误
    if (!errorsToRemove.empty()) {
      std::unique_lock<std::shared_mutex> lock(pendingErrorsMutex);
      for (const auto &errorId : errorsToRemove) {
        auto it = pendingErrors.find(errorId);
        if (it != pendingErrors.end()) {
          // 记录处理结果
          logErrorHandling(it->second, false, "达到最大重试次数");
          pendingErrors.erase(it);
        }
      }
    }

    // 重试处理
    for (auto &[errorId, context] : errorsToRetry) {
      // 增加重试计数并更新原始记录
      {
        std::unique_lock<std::shared_mutex> lock(pendingErrorsMutex);
        auto it = pendingErrors.find(errorId);
        if (it != pendingErrors.end()) {
          it->second.retryCount++;
          context.retryCount = it->second.retryCount;
        } else {
          continue; // 错误可能已被其他线程处理
        }
      }

      // 重试处理错误
      bool resolved = processError(context, ErrorHandlingStrategy::RETRY);

      // 如果解决了，从未解决列表中移除
      if (resolved) {
        std::unique_lock<std::shared_mutex> lock(pendingErrorsMutex);
        auto it = pendingErrors.find(errorId);
        if (it != pendingErrors.end()) {
          // 记录处理结果
          logErrorHandling(context, true, "重试成功");
          pendingErrors.erase(it);
        }
      }
    }

    // 等待一段时间再次检查
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

  spdlog::info("[ErrorRecoveryManager] 错误处理工作线程已停止");
}

bool ErrorRecoveryManager::processError(const ErrorContext &context,
                                        ErrorHandlingStrategy strategy) {
  bool resolved = false;

  switch (strategy) {
  case ErrorHandlingStrategy::IGNORE:
    // 直接忽略错误
    resolved = true;
    break;

  case ErrorHandlingStrategy::RETRY:
    // 重试操作
    // 注：实际实现中，这里需要调用设备服务或客户端重新执行命令
    spdlog::info("[ErrorRecoveryManager] 重试命令 {} 执行 (设备: {})",
                 context.command, context.deviceId);
    // 模拟重试结果
    resolved = (context.retryCount < 2); // 假设第二次重试会成功
    break;

  case ErrorHandlingStrategy::NOTIFY:
    // 仅通知，不做处理
    spdlog::info("[ErrorRecoveryManager] 通知错误: {} (设备: {})",
                 context.errorCode, context.deviceId);
    // 通知不会解决错误
    resolved = false;
    break;

  case ErrorHandlingStrategy::RESTART_DEVICE:
    // 重启设备
    // 注：实际实现中，这里需要调用设备管理器的重启设备功能
    spdlog::info("[ErrorRecoveryManager] 重启设备: {}", context.deviceId);
    // 模拟重启结果
    resolved = true; // 假设重启总是成功的
    break;

  case ErrorHandlingStrategy::FAILOVER:
    // 切换到备用设备
    // 注：实际实现中，这里需要调用设备管理器查找备用设备并切换
    spdlog::info("[ErrorRecoveryManager] 尝试切换到备用设备 (主设备: {})",
                 context.deviceId);
    // 模拟切换结果
    resolved = false; // 假设没有可用的备用设备
    break;

  case ErrorHandlingStrategy::CUSTOM:
    // 使用自定义处理器
    {
      ErrorHandlerFunc handler =
          findCustomHandler(context.deviceId, context.errorCode);
      if (handler) {
        spdlog::info(
            "[ErrorRecoveryManager] 使用自定义处理器处理错误 {} (设备: {})",
            context.errorCode, context.deviceId);
        resolved = handler(context);
      } else {
        spdlog::warn(
            "[ErrorRecoveryManager] 未找到错误 {} 的自定义处理器 (设备: {})",
            context.errorCode, context.deviceId);
        resolved = false;
      }
    }
    break;
  }

  return resolved;
}

ErrorHandlingStrategy
ErrorRecoveryManager::findStrategy(const std::string &deviceId,
                                   const std::string &errorCode) {
  std::shared_lock<std::shared_mutex> lock(strategiesMutex);

  // 首先查找设备特定的策略
  auto deviceIt = deviceStrategies.find(deviceId);
  if (deviceIt != deviceStrategies.end()) {
    auto strategyIt = deviceIt->second.find(errorCode);
    if (strategyIt != deviceIt->second.end()) {
      return strategyIt->second;
    }
  }

  // 然后查找全局策略
  auto globalIt = globalStrategies.find(errorCode);
  if (globalIt != globalStrategies.end()) {
    return globalIt->second;
  }

  // 默认策略是通知
  return ErrorHandlingStrategy::NOTIFY;
}

ErrorHandlerFunc
ErrorRecoveryManager::findCustomHandler(const std::string &deviceId,
                                        const std::string &errorCode) {
  std::shared_lock<std::shared_mutex> lock(handlersMutex);

  // 首先查找设备特定的处理器
  auto deviceIt = deviceCustomHandlers.find(deviceId);
  if (deviceIt != deviceCustomHandlers.end()) {
    auto handlerIt = deviceIt->second.find(errorCode);
    if (handlerIt != deviceIt->second.end()) {
      return handlerIt->second;
    }
  }

  // 然后查找全局处理器
  auto globalIt = customHandlers.find(errorCode);
  if (globalIt != customHandlers.end()) {
    return globalIt->second;
  }

  // 没有找到处理器
  return nullptr;
}

void ErrorRecoveryManager::logErrorHandling(const ErrorContext &context,
                                            bool resolved,
                                            const std::string &action) {
  nlohmann::json record = {{"timestamp", std::chrono::system_clock::to_time_t(
                                             std::chrono::system_clock::now())},
                           {"deviceId", context.deviceId},
                           {"errorCode", context.errorCode},
                           {"errorMessage", context.errorMessage},
                           {"action", action},
                           {"resolved", resolved}};

  if (!context.command.empty()) {
    record["command"] = context.command;
  }

  {
    std::unique_lock<std::shared_mutex> lock(errorHistoryMutex);
    errorHistory.push_back(record);

    // 限制历史记录数量
    if (errorHistory.size() > 1000) {
      errorHistory.erase(errorHistory.begin()); // 移除最旧的记录
    }
  }
}

} // namespace astrocomm
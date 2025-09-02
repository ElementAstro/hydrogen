#include "client/command_executor.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>

namespace hydrogen {

CommandExecutor::CommandExecutor(MessageProcessor* messageProcessor)
    : messageProcessor(messageProcessor),
      commandsExecuted(0), asyncCommandsExecuted(0), batchCommandsExecuted(0),
      commandErrors(0), timeouts(0) {
  if (!messageProcessor) {
    throw std::invalid_argument("MessageProcessor cannot be null");
  }

  // Initialize message queue manager
  messageQueueManager = std::make_unique<MessageQueueManager>();
  messageQueueManager->setMessageSender(
      [this](const Message& msg) -> bool {
        return this->messageProcessor->sendMessage(msg);
      });
  messageQueueManager->start();

  spdlog::debug("CommandExecutor initialized");
}

CommandExecutor::~CommandExecutor() {
  // Clear pending commands
  clearPendingCommands();

  // Stop message queue manager
  if (messageQueueManager) {
    messageQueueManager->stop();
  }

  spdlog::debug("CommandExecutor destroyed");
}

json CommandExecutor::executeCommand(const std::string& deviceId,
                                    const std::string& command,
                                    const json& parameters,
                                    Message::QoSLevel qosLevel) {
  if (!isValidDeviceId(deviceId)) {
    throw std::invalid_argument("Invalid device ID: " + deviceId);
  }

  if (!isValidCommand(command)) {
    throw std::invalid_argument("Invalid command: " + command);
  }

  // Create command message
  CommandMessage msg(command);
  msg.setDeviceId(deviceId);
  msg.setQoSLevel(qosLevel);

  if (!parameters.is_null()) {
    msg.setParameters(parameters);
  }

  try {
    json response;

    // Use different execution paths based on QoS level
    if (qosLevel != Message::QoSLevel::AT_MOST_ONCE) {
      response = executeWithQoS(msg);
    } else {
      response = messageProcessor->sendAndWaitForResponse(msg);
    }

    updateStats(1, 0, 0, 0, 0);
    spdlog::debug("Command '{}' executed successfully on device '{}'", command, deviceId);
    return response;

  } catch (const std::exception& e) {
    updateStats(0, 0, 0, 1, 0);
    spdlog::error("Command '{}' failed on device '{}': {}", command, deviceId, e.what());
    throw;
  }
}

void CommandExecutor::executeCommandAsync(const std::string& deviceId,
                                         const std::string& command,
                                         const json& parameters,
                                         Message::QoSLevel qosLevel,
                                         AsyncCallback callback) {
  if (!isValidDeviceId(deviceId)) {
    spdlog::error("Invalid device ID for async command: {}", deviceId);
    if (callback) {
      std::thread([cb = std::move(callback)]() {
        cb(json{{"error", "Invalid device ID"}});
      }).detach();
    }
    return;
  }

  if (!isValidCommand(command)) {
    spdlog::error("Invalid command for async execution: {}", command);
    if (callback) {
      std::thread([cb = std::move(callback)]() {
        cb(json{{"error", "Invalid command"}});
      }).detach();
    }
    return;
  }

  // Create command message
  CommandMessage msg(command);
  msg.setDeviceId(deviceId);
  msg.setQoSLevel(qosLevel);

  if (!parameters.is_null()) {
    msg.setParameters(parameters);
  }

  std::string messageId = msg.getMessageId();

  // Register callback if provided
  if (callback) {
    registerAsyncCallback(messageId, callback);
  }

  // Use message queue for sending
  messageQueueManager->sendMessage(
      msg, [this, messageId, command, deviceId, callback](const std::string& /*id*/, bool success) {
        if (!success) {
          spdlog::error("Async message delivery failed for command '{}' on device '{}'",
                        command, deviceId);

          // Call callback with error
          if (callback) {
            std::thread([cb = callback]() {
              cb(json{{"error", "Message delivery failed"}});
            }).detach();
          }

          // Remove from pending callbacks
          unregisterAsyncCallback(messageId);
          updateStats(0, 0, 0, 1, 0);
        } else {
          updateStats(0, 1, 0, 0, 0);
        }
      });

  spdlog::debug("Async command '{}' sent to device '{}'", command, deviceId);
}

json CommandExecutor::executeBatchCommands(
    const std::string& deviceId,
    const std::vector<std::pair<std::string, json>>& commands,
    bool sequential,
    Message::QoSLevel qosLevel) {
  
  if (!isValidDeviceId(deviceId)) {
    throw std::invalid_argument("Invalid device ID: " + deviceId);
  }

  if (commands.empty()) {
    throw std::invalid_argument("Command list cannot be empty");
  }

  // Validate all commands
  for (const auto& cmd : commands) {
    if (!isValidCommand(cmd.first)) {
      throw std::invalid_argument("Invalid command in batch: " + cmd.first);
    }
  }

  CommandMessage msg("BATCH");
  msg.setDeviceId(deviceId);
  msg.setQoSLevel(qosLevel);

  json cmdArray = json::array();
  for (const auto& cmd : commands) {
    json cmdObj = {{"command", cmd.first}};

    if (!cmd.second.is_null()) {
      cmdObj["parameters"] = cmd.second;
    }

    cmdArray.push_back(cmdObj);
  }

  msg.setParameters({
      {"commands", cmdArray},
      {"executionMode", sequential ? "SEQUENTIAL" : "PARALLEL"}
  });

  try {
    json response;
    
    if (qosLevel != Message::QoSLevel::AT_MOST_ONCE) {
      response = executeWithQoS(msg);
    } else {
      response = messageProcessor->sendAndWaitForResponse(msg);
    }

    updateStats(0, 0, 1, 0, 0);
    spdlog::info("Batch of {} commands executed on device '{}'", commands.size(), deviceId);
    return response;

  } catch (const std::exception& e) {
    updateStats(0, 0, 0, 1, 0);
    spdlog::error("Batch command execution failed on device '{}': {}", deviceId, e.what());
    throw;
  }
}

void CommandExecutor::setMessageRetryParams(int maxRetries, int retryIntervalMs) {
  if (messageQueueManager) {
    messageQueueManager->setRetryParams(maxRetries, retryIntervalMs);
    spdlog::info("Message retry parameters updated: maxRetries={}, retryIntervalMs={}",
                 maxRetries, retryIntervalMs);
  } else {
    spdlog::warn("Cannot set message retry params: MessageQueueManager is null");
  }
}

bool CommandExecutor::cancelAsyncCommand(const std::string& messageId) {
  std::lock_guard<std::mutex> lock(callbacksMutex);
  auto it = asyncCallbacks.find(messageId);
  if (it != asyncCallbacks.end()) {
    asyncCallbacks.erase(it);
    spdlog::debug("Cancelled async command with message ID: {}", messageId);
    return true;
  }
  return false;
}

size_t CommandExecutor::getPendingAsyncCount() const {
  std::lock_guard<std::mutex> lock(callbacksMutex);
  return asyncCallbacks.size();
}

json CommandExecutor::getExecutionStats() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  
  json stats;
  stats["commandsExecuted"] = commandsExecuted;
  stats["asyncCommandsExecuted"] = asyncCommandsExecuted;
  stats["batchCommandsExecuted"] = batchCommandsExecuted;
  stats["commandErrors"] = commandErrors;
  stats["timeouts"] = timeouts;
  stats["pendingAsyncCommands"] = getPendingAsyncCount();
  
  return stats;
}

void CommandExecutor::clearPendingCommands() {
  std::lock_guard<std::mutex> lock(callbacksMutex);
  
  // Call all pending callbacks with error
  for (auto& [messageId, callback] : asyncCallbacks) {
    if (callback) {
      spdlog::debug("Notifying async callback for {} about shutdown", messageId);
      std::thread([cb = callback]() {
        cb(json{{"error", "CommandExecutor shutdown"}});
      }).detach();
    }
  }
  
  asyncCallbacks.clear();
  spdlog::debug("Cleared {} pending async commands", asyncCallbacks.size());
}

void CommandExecutor::handleAsyncResponse(const std::string& messageId, const json& response) {
  std::lock_guard<std::mutex> lock(callbacksMutex);
  auto it = asyncCallbacks.find(messageId);
  if (it != asyncCallbacks.end()) {
    if (it->second) {
      std::thread([cb = it->second, response]() {
        cb(response);
      }).detach();
    }
    asyncCallbacks.erase(it);
  }
}

void CommandExecutor::registerAsyncCallback(const std::string& messageId, AsyncCallback callback) {
  std::lock_guard<std::mutex> lock(callbacksMutex);
  asyncCallbacks[messageId] = std::move(callback);
}

void CommandExecutor::unregisterAsyncCallback(const std::string& messageId) {
  std::lock_guard<std::mutex> lock(callbacksMutex);
  asyncCallbacks.erase(messageId);
}

bool CommandExecutor::isValidDeviceId(const std::string& deviceId) const {
  if (deviceId.empty() || deviceId.length() > 256) {
    return false;
  }

  return std::all_of(deviceId.begin(), deviceId.end(), [](char c) {
    return std::isalnum(c) || c == '_' || c == '-' || c == '.';
  });
}

bool CommandExecutor::isValidCommand(const std::string& command) const {
  if (command.empty() || command.length() > 128) {
    return false;
  }

  return std::all_of(command.begin(), command.end(), [](char c) {
    return std::isalnum(c) || c == '_' || c == '-';
  });
}

void CommandExecutor::updateStats(size_t executed, size_t asyncExecuted,
                                 size_t batchExecuted, size_t errors, size_t timeoutCount) {
  std::lock_guard<std::mutex> lock(statsMutex);
  commandsExecuted += executed;
  asyncCommandsExecuted += asyncExecuted;
  batchCommandsExecuted += batchExecuted;
  commandErrors += errors;
  timeouts += timeoutCount;
}

json CommandExecutor::executeWithQoS(const CommandMessage& msg, int /*timeoutSeconds*/) {
  // For now, use a simplified approach without promise/future
  // In a real implementation, this would integrate with the message processor
  // to wait for actual responses

  std::string messageId = msg.getMessageId();
  bool deliverySuccess = false;

  // Send message with QoS and wait for delivery confirmation
  messageQueueManager->sendMessage(msg, [&deliverySuccess, messageId](
                                            const std::string& /*id*/, bool success) {
    deliverySuccess = success;
    if (!success) {
      spdlog::error("Message delivery failed for command (ID: {})", messageId);
    }
  });

  // Simple timeout mechanism - in real implementation this would wait for actual response
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  if (deliverySuccess) {
    return json{{"status", "success", "messageId", messageId}};
  } else {
    updateStats(0, 0, 0, 0, 1);
    throw std::runtime_error("Message delivery failed");
  }
}

} // namespace hydrogen

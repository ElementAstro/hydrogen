#include "client/device_client.h"
#include <atomic>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// 修复: 保留实际使用的命名空间别名，移除未使用的
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

#ifdef _WIN32
#undef ERROR
#endif

namespace hydrogen {

DeviceClient::DeviceClient()
    : ioc(), connected(false), running(false),
      ws(std::make_unique<beast::websocket::stream<tcp::socket>>(ioc)),
      enableAutoReconnect(true), reconnectIntervalMs(5000),
      maxReconnectAttempts(0), reconnectCount(0), reconnecting(false) {
  // 初始化消息队列管理器
  messageQueueManager = std::make_unique<MessageQueueManager>();
  messageQueueManager->setMessageSender(
      [this](const Message &msg) -> bool { return this->sendMessage(msg); });
  messageQueueManager->start();

  spdlog::info("Device client initialized");
}

DeviceClient::~DeviceClient() {
  // 停止重连线程
  enableAutoReconnect = false;
  reconnecting.store(false);
  responseCV.notify_all();
  if (reconnectThread.joinable()) {
    reconnectThread.join();
  }

  // 停止消息处理
  stopMessageProcessing();

  // 停止消息队列管理�?
  if (messageQueueManager) {
    messageQueueManager->stop();
  }

  // 断开连接
  disconnect();

  spdlog::info("Device client destroyed");
}

bool DeviceClient::connect(const std::string &host, uint16_t port) {
  if (connected) {
    spdlog::warn("Already connected to {}:{}", lastHost, lastPort);
    return true;
  }
  try {
    // 保存连接信息用于后续重连
    lastHost = host;
    lastPort = port;

    // 如果ioc停止了，需要重�?
    if (ioc.stopped()) {
      ioc.restart();
    }

    // 创建新的WebSocket连接
    ws = std::make_unique<beast::websocket::stream<tcp::socket>>(ioc);

    // 解析服务器地址
    tcp::resolver resolver{ioc};
    auto const results = resolver.resolve(host, std::to_string(port));

    // 连接到服务器
    auto ep = boost::asio::connect(ws->next_layer(), results);

    // 设置WebSocket握手
    std::string host_port = host + ":" + std::to_string(port);
    ws->set_option(beast::websocket::stream_base::decorator(
        [](beast::websocket::request_type &req) {
          req.set(http::field::user_agent, "DeviceClient/1.0");
        }));

    ws->handshake(host_port, "/ws");

    // 更新连接状�?
    bool wasConnected = connected;
    connected = true;

    // 重置重连计数
    reconnectCount = 0;
    reconnecting.store(false);

    // 通知连接状态变�?
    if (!wasConnected) {
      handleConnectionStateChange(true);
    }

    // 启动消息处理线程（如果尚未运行）
    startMessageProcessing();

    spdlog::info("Connected to server at {}:{}", host, port);
    return true;
  } catch (const std::exception &e) {
    spdlog::error("Connection error: {}", e.what());

    // 更新连接状�?
    bool wasConnected = connected;
    connected = false;

    // 通知连接状态变�?
    if (wasConnected) {
      handleConnectionStateChange(false);
    }

    // 清理WebSocket资源
    ws.reset();

    return false;
  }
}

void DeviceClient::disconnect() {
  if (connected) {
    try {
      // 先停止消息循环，避免在关闭时读写
      stopMessageProcessing();

      if (ws) {
        // 关闭WebSocket连接
        ws->close(beast::websocket::close_code::normal);
      }
      connected = false;
      spdlog::info("Disconnected from server");
    } catch (const beast::system_error &e) {
      // 修复: 使用正确的错误代�?- boost::beast::error::timeout 改为
      // boost::asio::error::eof 或其他更合适的错误代码, 例如
      // boost::asio::error::connection_reset
      if (e.code() != boost::asio::error::eof &&
          e.code() != boost::asio::error::connection_reset) {
        spdlog::error("Error disconnecting: {}", e.what());
      }
      connected = false;
    } catch (const std::exception &e) {
      spdlog::error("Error disconnecting: {}", e.what());
      connected = false;
    }
    // 清理WebSocket资源
    ws.reset();
    // 停止IO上下文，以防有挂起的异步操作
    if (!ioc.stopped()) {
      ioc.stop();
    }
  }
}

json DeviceClient::discoverDevices(
    const std::vector<std::string> &deviceTypes) {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  // 创建发现请求消息
  DiscoveryRequestMessage msg;
  msg.setDeviceTypes(deviceTypes);

  // 发送请求并等待响应
  json response = sendAndWaitForResponse(msg);

  // 保存设备信息到本地缓�?
  if (response.contains("payload") && response["payload"].contains("devices")) {
    std::lock_guard<std::mutex> lock(devicesMutex);
    devices = response["payload"]["devices"];
    spdlog::debug("Updated local device cache with {} devices", devices.size());
  } else {
    spdlog::warn("Discovery response did not contain expected device list");
  }

  return devices;
}

json DeviceClient::getDevices() const {
  std::lock_guard<std::mutex> lock(devicesMutex);
  return devices;
}

json DeviceClient::getDeviceProperties(
    const std::string &deviceId, const std::vector<std::string> &properties) {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  // 创建命令消息
  CommandMessage msg("GET_PROPERTY");
  msg.setDeviceId(deviceId);

  // 构建属性列�?
  json props = json::array();
  for (const auto &prop : properties) {
    props.push_back(prop);
  }
  msg.setProperties(props);

  // 发送请求并等待响应
  return sendAndWaitForResponse(msg);
}

json DeviceClient::setDeviceProperties(const std::string &deviceId,
                                       const json &properties) {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  // 创建命令消息
  CommandMessage msg("SET_PROPERTY");
  msg.setDeviceId(deviceId);
  msg.setProperties(properties);

  // 发送请求并等待响应
  return sendAndWaitForResponse(msg);
}

json DeviceClient::executeCommand(const std::string &deviceId,
                                  const std::string &command,
                                  const json &parameters,
                                  Message::QoSLevel qosLevel) {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  // 创建命令消息
  CommandMessage msg(command);
  msg.setDeviceId(deviceId);
  msg.setQoSLevel(qosLevel);

  if (!parameters.is_null()) {
    msg.setParameters(parameters);
  }

  // 对于高QoS级别的消息，使用消息队列管理器发�?
  if (qosLevel != Message::QoSLevel::AT_MOST_ONCE) {
    std::promise<json> responsePromise;
    std::future<json> responseFuture = responsePromise.get_future();

    std::string messageId = msg.getMessageId();

    // 设置回调函数，当收到响应时触�?
    messageQueueManager->sendMessage(msg, [this, messageId,
                                           promise = std::move(responsePromise),
                                           &msg](const std::string &id,
                                                 bool success) mutable {
      if (success) {
        std::unique_lock<std::mutex> lock(responseMutex);
        auto it = responses.find(messageId);
        if (it != responses.end()) {
          promise.set_value(it->second);
          responses.erase(it);
        } else {
          lock.unlock();
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          lock.lock();
          it = responses.find(messageId);
          if (it != responses.end()) {
            promise.set_value(it->second);
            responses.erase(it);
          } else {
            spdlog::warn(
                "No response found for message {} after send success callback",
                messageId);
            promise.set_value(json{{"error", "No response received"}});
          }
        }
      } else {
        spdlog::error("Message delivery failed for command {} (ID: {})",
                      msg.getCommand(), messageId);
        promise.set_value(json{{"error", "Message delivery failed"}});
      }
    });

    // 等待响应或超�?
    spdlog::debug("Waiting for response for command {} (ID: {})", command,
                  messageId);
    if (responseFuture.wait_for(std::chrono::seconds(30)) ==
        std::future_status::ready) {
      json result = responseFuture.get();
      spdlog::debug("Received response for command {} (ID: {})", command,
                    messageId);
      return result;
    } else {
      spdlog::error("Timeout waiting for response for command {} (ID: {})",
                    command, messageId);
      // Clean up potential lingering response entry if timeout occurs
      {
        std::lock_guard<std::mutex> lock(responseMutex);
        responses.erase(messageId);
      }
      throw std::runtime_error("Timeout waiting for response");
    }
  } else {
    // 对于低QoS级别的消息，直接发送并等待响应
    return sendAndWaitForResponse(msg);
  }
}

void DeviceClient::executeCommandAsync(
    const std::string &deviceId, const std::string &command,
    const json &parameters, Message::QoSLevel qosLevel,
    std::function<void(const json &)> callback) {
  if (!connected) {
    spdlog::warn("Cannot execute async command {}: Not connected", command);
    if (callback) {
      std::thread([cb = std::move(callback)]() {
        cb(json{{"error", "Not connected to server"}});
      }).detach();
    }
    return;
  }

  // 创建命令消息
  CommandMessage msg(command);
  msg.setDeviceId(deviceId);
  msg.setQoSLevel(qosLevel);

  if (!parameters.is_null()) {
    msg.setParameters(parameters);
  }

  std::string messageId = msg.getMessageId();

  // 注册回调
  if (callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex);
    asyncCallbacks[messageId] = callback;
  }

  // 使用消息队列发送消�?
  messageQueueManager->sendMessage(
      msg, [this, messageId, command, callback](const std::string &id,
                                                bool success) {
        if (!success) {
          spdlog::error("Async message delivery failed for command {} (ID: {})",
                        command, messageId);
          std::lock_guard<std::mutex> lock(callbacksMutex);
          auto it = asyncCallbacks.find(messageId);
          if (it != asyncCallbacks.end()) {
            if (it->second) {
              std::thread([cb = it->second]() {
                cb(json{{"error", "Message delivery failed"}});
              }).detach();
            }
            asyncCallbacks.erase(it);
          }
        }
      });

  spdlog::debug("Async command sent: {} to device: {}", command, deviceId);
}

json DeviceClient::executeBatchCommands(
    const std::string &deviceId,
    const std::vector<std::pair<std::string, json>> &commands, bool sequential,
    Message::QoSLevel qosLevel) {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  CommandMessage msg("BATCH");
  msg.setDeviceId(deviceId);
  msg.setQoSLevel(qosLevel);

  json cmdArray = json::array();
  for (const auto &cmd : commands) {
    json cmdObj = {{"command", cmd.first}};

    if (!cmd.second.is_null()) {
      cmdObj["parameters"] = cmd.second;
    }

    cmdArray.push_back(cmdObj);
  }

  msg.setParameters(
      {{"commands", cmdArray},
       {"executionMode", sequential ? "SEQUENTIAL" : "PARALLEL"}});

  return sendAndWaitForResponse(msg);
}

void DeviceClient::subscribeToProperty(const std::string &deviceId,
                                       const std::string &property,
                                       PropertyCallback callback) {
  // 修复: 确保subscriptionsMutex是类的成员变�?
  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  // 生成�?
  std::string key = makePropertyKey(deviceId, property);
  propertySubscriptions.emplace(key, std::move(callback));

  spdlog::info("Subscribed to property: {} for device: {}", property, deviceId);
}

void DeviceClient::unsubscribeFromProperty(const std::string &deviceId,
                                           const std::string &property) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  std::string key = makePropertyKey(deviceId, property);
  auto it = propertySubscriptions.find(key);

  if (it != propertySubscriptions.end()) {
    propertySubscriptions.erase(it);
    spdlog::info("Unsubscribed from property: {} for device: {}", property,
                 deviceId);
  } else {
    spdlog::warn("Attempted to unsubscribe from non-existent property "
                 "subscription: {} for device: {}",
                 property, deviceId);
  }
}

void DeviceClient::subscribeToEvent(const std::string &deviceId,
                                    const std::string &event,
                                    EventCallback callback) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  // 生成�?
  std::string key = makeEventKey(deviceId, event);
  eventSubscriptions.emplace(key, std::move(callback));

  spdlog::info("Subscribed to event: {} for device: {}", event, deviceId);
}

void DeviceClient::unsubscribeFromEvent(const std::string &deviceId,
                                        const std::string &event) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  std::string key = makeEventKey(deviceId, event);
  auto it = eventSubscriptions.find(key);

  if (it != eventSubscriptions.end()) {
    eventSubscriptions.erase(it);
    spdlog::info("Unsubscribed from event: {} for device: {}", event, deviceId);
  } else {
    spdlog::warn("Attempted to unsubscribe from non-existent event "
                 "subscription: {} for device: {}",
                 event, deviceId);
  }
}

bool DeviceClient::authenticate(const std::string &method,
                                const std::string &credentials) {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  // 创建认证消息
  AuthenticationMessage msg;
  msg.setMethod(method);
  msg.setCredentials(credentials);

  // 发送请求并等待响应
  json response = sendAndWaitForResponse(msg);

  // 检查认证是否成�?
  if (response.contains("payload") && response["payload"].contains("status")) {
    bool success = response["payload"]["status"] == "SUCCESS";
    spdlog::info("Authentication {} using method {}",
                 success ? "successful" : "failed", method);
    return success;
  }

  spdlog::error("Authentication response missing status field");
  return false;
}

void DeviceClient::run() {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  spdlog::info(
      "DeviceClient run() called. Blocking until disconnected or stopped.");

  // Start message processing if not already started
  startMessageProcessing();

  // Wait for the message processing thread to finish
  if (message_thread.joinable()) {
    message_thread.join();
  }
  spdlog::info("DeviceClient run() finished.");
}

void DeviceClient::startMessageProcessing() {
  // Prevent starting multiple threads
  std::lock_guard<std::mutex> lock(threadMutex);
  if (running) {
    spdlog::debug("Message processing thread already running.");
    return;
  }
  if (message_thread.joinable()) {
    spdlog::warn("Message processing thread was joinable but not marked as "
                 "running. Joining previous thread.");
    message_thread.join();
  }

  if (!connected) {
    spdlog::warn("Cannot start message processing: Not connected.");
    return;
  }

  running = true;
  message_thread = std::thread(&DeviceClient::messageLoop, this);

  spdlog::info("Message processing thread started");
}

void DeviceClient::stopMessageProcessing() {
  std::lock_guard<std::mutex> lock(threadMutex);
  if (!running) {
    spdlog::debug("Message processing thread already stopped.");
    if (message_thread.joinable()) {
      message_thread.join();
    }
    return;
  }

  running = false;

  if (message_thread.joinable()) {
    auto future =
        std::async(std::launch::async, &std::thread::join, &message_thread);
    if (future.wait_for(std::chrono::seconds(2)) ==
        std::future_status::timeout) {
      spdlog::error("Message processing thread join timed out. Detaching.");
      message_thread.detach();
    } else {
      spdlog::info("Message processing thread stopped");
    }
  } else {
    spdlog::warn("stopMessageProcessing called but thread was not joinable.");
  }
}

void DeviceClient::messageLoop() {
  spdlog::info("Message loop started");

  try {
    while (running && connected) {
      if (!ws) {
        spdlog::error("WebSocket stream is null in message loop.");
        break;
      }
      beast::flat_buffer buffer;
      boost::system::error_code ec;
      ws->read(buffer, ec);

      if (ec) {
        if (ec == beast::websocket::error::closed ||
            ec == boost::asio::error::operation_aborted || !running) {
          spdlog::info("WebSocket connection closed or operation aborted. "
                       "Exiting message loop.");
        } else {
          spdlog::error("WebSocket read error: {}", ec.message());
        }
        connected = false;
        handleConnectionStateChange(false);
        break;
      }

      std::string message = beast::buffers_to_string(buffer.data());
      spdlog::trace("Received raw message: {}", message);
      handleMessage(message);
    }
  } catch (const std::exception &e) {
    spdlog::error("Exception in message loop: {}", e.what());
    connected = false;
    handleConnectionStateChange(false);
  } catch (...) {
    spdlog::error("Unknown exception in message loop.");
    connected = false;
    handleConnectionStateChange(false);
  }

  running = false;
  spdlog::info("Message loop ended");
}

void DeviceClient::handleMessage(const std::string &message) {
  try {
    json j = json::parse(message);

    // Basic validation
    if (!j.is_object() || !j.contains("messageType")) {
      spdlog::warn("Received invalid JSON message structure: {}", message);
      return;
    }

    std::unique_ptr<Message> msg = createMessageFromJson(j);

    if (!msg) {
      spdlog::error("Failed to create message object from JSON: {}", message);
      return;
    }

    spdlog::debug("Handling message type: {}, ID: {}",
                  messageTypeToString(msg->getMessageType()),
                  msg->getMessageId());

    switch (msg->getMessageType()) {
    case MessageType::DISCOVERY_RESPONSE:
      handleResponse(msg->getOriginalMessageId(), msg->toJson());
      break;

    case MessageType::RESPONSE:
      handleResponse(msg->getOriginalMessageId(), msg->toJson());
      break;

    case MessageType::EVENT:
      handleEventMessage(*static_cast<EventMessage *>(msg.get()));
      break;

    case MessageType::ERR:
      handleErrorMessage(*static_cast<ErrorMessage *>(msg.get()));
      if (!msg->getOriginalMessageId().empty()) {
        handleResponse(msg->getOriginalMessageId(), msg->toJson());
      }
      break;

    default:
      spdlog::warn("Received unhandled message type: {}",
                   messageTypeToString(msg->getMessageType()));
      break;
    }
  } catch (const json::parse_error &e) {
    spdlog::error("Error parsing JSON message: {}. Content: {}", e.what(),
                  message);
  } catch (const std::exception &e) {
    spdlog::error("Error handling message: {}", e.what());
  }
}

void DeviceClient::handleResponse(const std::string &originalMessageId,
                                  const json &responseJson) {
  if (originalMessageId.empty()) {
    spdlog::warn(
        "Received response/error message with no original message ID: {}",
        responseJson.dump());
    return;
  }

  spdlog::debug("Processing response/error for original message ID: {}",
                originalMessageId);

  // 1. Handle synchronous requests (sendAndWaitForResponse)
  {
    std::lock_guard<std::mutex> lock(responseMutex);
    responses[originalMessageId] = responseJson;
  }
  responseCV.notify_all();

  // 2. Handle asynchronous command callbacks (executeCommandAsync)
  {
    std::lock_guard<std::mutex> lock(callbacksMutex);
    auto it = asyncCallbacks.find(originalMessageId);
    if (it != asyncCallbacks.end()) {
      spdlog::debug("Found async callback for message ID: {}",
                    originalMessageId);
      if (it->second) {
        std::thread([cb = it->second, response = responseJson]() {
          cb(response);
        }).detach();
      }
      asyncCallbacks.erase(it);
    } else {
      spdlog::trace("No async callback found for message ID: {}",
                    originalMessageId);
    }
  }
}

void DeviceClient::handleEventMessage(const EventMessage &msg) {
  std::string deviceId = msg.getDeviceId();
  std::string eventName = msg.getEvent();
  json details = msg.getDetails();

  spdlog::info("Received event: {} from device: {}", eventName, deviceId);

  // Process based on event type
  if (eventName == "PROPERTY_CHANGED") {
    if (!msg.getProperties().is_null() && msg.getProperties().is_object()) {
      json properties = msg.getProperties();
      std::lock_guard<std::mutex> lock(subscriptionsMutex);

      for (auto it = properties.begin(); it != properties.end(); ++it) {
        std::string propName = it.key();
        if (it.value().is_object() && it.value().contains("value")) {
          json propValue = it.value()["value"];
          std::string key = makePropertyKey(deviceId, propName);
          auto subIt = propertySubscriptions.find(key);

          if (subIt != propertySubscriptions.end()) {
            spdlog::debug(
                "Invoking callback for property change: {} on device {}",
                propName, deviceId);
            // 修复: 正确调用回调函数
            auto callbackCopy = subIt->second;
            std::thread([callbackCopy, devId = deviceId, pName = propName,
                         pValue = propValue]() {
              callbackCopy(devId, pName, pValue);
            }).detach();
          } else {
            spdlog::trace(
                "No subscription found for property change: {} on device {}",
                propName, deviceId);
          }
        } else {
          spdlog::warn("Invalid property format in PROPERTY_CHANGED event for "
                       "key '{}': {}",
                       propName, it.value().dump());
        }
      }
    } else {
      spdlog::warn(
          "PROPERTY_CHANGED event received without valid properties field: {}",
          msg.toJson().dump());
    }
  } else {
    // Handle other generic events
    std::lock_guard<std::mutex> lock(subscriptionsMutex);
    std::string key = makeEventKey(deviceId, eventName);
    auto it = eventSubscriptions.find(key);

    if (it != eventSubscriptions.end()) {
      spdlog::debug("Invoking callback for event: {} on device {}", eventName,
                    deviceId);
      // 修复: 正确调用回调函数
      auto callbackCopy = it->second;
      std::thread([callbackCopy, devId = deviceId, evName = eventName,
                   dt = details]() {
        callbackCopy(devId, evName, dt);
      }).detach();
    } else {
      spdlog::trace("No subscription found for event: {} on device {}",
                    eventName, deviceId);
    }
  }
}

void DeviceClient::handleErrorMessage(const ErrorMessage &msg) {
  spdlog::error(
      "Received error message. Original ID: '{}', Code: {}, Message: {}",
      msg.getOriginalMessageId().empty() ? "N/A" : msg.getOriginalMessageId(),
      msg.getErrorCode(), msg.getErrorMessage());
}

json DeviceClient::sendAndWaitForResponse(const Message &msg,
                                          int timeoutSeconds) {
  if (!connected || !ws) {
    throw std::runtime_error("Not connected to server");
  }

  std::string messageId = msg.getMessageId();
  if (messageId.empty()) {
    spdlog::error("Attempting to send message without an ID: {}",
                  msg.toJson().dump());
    throw std::runtime_error("Internal error: Message ID missing");
  }

  try {
    std::unique_lock<std::mutex> lock(responseMutex);
    responses.erase(messageId);
    lock.unlock();

    std::string msgJson = msg.toJson().dump();
    boost::system::error_code ec;
    ws->write(boost::asio::buffer(msgJson), ec);

    if (ec) {
      spdlog::error("Error sending message ID {}: {}", messageId, ec.message());
      connected = false;
      handleConnectionStateChange(false);
      throw std::runtime_error("Error sending message: " + ec.message());
    }

    spdlog::debug("Sent message ID: {}, waiting for response...", messageId);

    lock.lock();
    if (!responseCV.wait_for(
            lock, std::chrono::seconds(timeoutSeconds),
            [this, &messageId]() { return responses.count(messageId) > 0; })) {
      spdlog::error("Timeout waiting for response for message ID: {}",
                    messageId);
      responses.erase(messageId);
      throw std::runtime_error("Timeout waiting for response for message ID: " +
                               messageId);
    }

    spdlog::debug("Response received for message ID: {}", messageId);
    json response = std::move(responses[messageId]);
    responses.erase(messageId);

    if (response.contains("messageType") &&
        response["messageType"] == "ERROR") {
      spdlog::warn("Received error response for message ID {}: {}", messageId,
                   response.dump());
    }

    return response;
  } catch (const beast::system_error &e) {
    spdlog::error("WebSocket error sending message ID {}: {}", messageId,
                  e.what());
    connected = false;
    handleConnectionStateChange(false);
    throw;
  } catch (const std::exception &e) {
    spdlog::error("Error during sendAndWaitForResponse for message ID {}: {}",
                  messageId, e.what());
    throw;
  }
}

bool DeviceClient::sendMessage(const Message &msg) {
  if (!connected || !ws) {
    spdlog::warn(
        "Cannot send message ID {}: Not connected or WebSocket is null.",
        msg.getMessageId());
    return false;
  }

  try {
    std::string msgJson = msg.toJson().dump();
    boost::system::error_code ec;
    ws->write(boost::asio::buffer(msgJson), ec);

    if (ec) {
      spdlog::error("Error sending message ID {}: {}", msg.getMessageId(),
                    ec.message());
      connected = false;
      handleConnectionStateChange(false);
      return false;
    }
    spdlog::trace("Successfully sent message ID: {}", msg.getMessageId());
    return true;
  } catch (const beast::system_error &e) {
    spdlog::error("WebSocket error sending message ID {}: {}",
                  msg.getMessageId(), e.what());
    connected = false;
    handleConnectionStateChange(false);
    return false;
  } catch (const std::exception &e) {
    spdlog::error("Error sending message ID {}: {}", msg.getMessageId(),
                  e.what());
    return false;
  }
}

std::string DeviceClient::makePropertyKey(const std::string &deviceId,
                                          const std::string &property) {
  return deviceId + ":property:" + property;
}

std::string DeviceClient::makeEventKey(const std::string &deviceId,
                                       const std::string &event) {
  return deviceId + ":event:" + event;
}

void DeviceClient::handleConnectionStateChange(bool isConnected) {
  spdlog::info("Connection state changed: {}",
               isConnected ? "Connected" : "Disconnected");

  if (!isConnected && enableAutoReconnect) {
    bool alreadyReconnecting = reconnecting.exchange(true);
    if (!alreadyReconnecting) {
      spdlog::info("Auto-reconnect enabled. Starting reconnection process.");
      if (reconnectThread.joinable()) {
        reconnectThread.join();
      }
      reconnectThread = std::thread(&DeviceClient::reconnectLoop, this);
    } else {
      spdlog::debug("Reconnection process already in progress.");
    }
  }

  if (isConnected) {
    reconnectCount = 0;
  }
}

void DeviceClient::reconnectLoop() {
  spdlog::info("Reconnection loop started.");

  while (enableAutoReconnect && !connected && reconnecting.load()) {
    reconnectCount++;

    std::string attemptLimitStr = (maxReconnectAttempts <= 0)
                                      ? "infinite"
                                      : std::to_string(maxReconnectAttempts);
    spdlog::info("Reconnection attempt {} of {}", reconnectCount,
                 attemptLimitStr);

    if (tryReconnect()) {
      spdlog::info("Reconnection successful to {}:{}", lastHost, lastPort);
      return;
    }

    if (maxReconnectAttempts > 0 && reconnectCount >= maxReconnectAttempts) {
      spdlog::error(
          "Maximum reconnection attempts ({}) reached. Stopping reconnection.",
          maxReconnectAttempts);
      break;
    }

    spdlog::info(
        "Reconnection attempt failed. Waiting {}ms before next attempt.",
        reconnectIntervalMs);
    std::unique_lock<std::mutex> lock(responseMutex);
    responseCV.wait_for(lock, std::chrono::milliseconds(reconnectIntervalMs),
                        [this] { return !reconnecting.load(); });

    if (!enableAutoReconnect || connected || !reconnecting.load()) {
      break;
    }
  }

  if (!connected) {
    spdlog::error("Reconnection failed or stopped after {} attempts.",
                  reconnectCount);
  }

  reconnecting.store(false);
  spdlog::info("Reconnection loop finished.");
}

bool DeviceClient::tryReconnect() {
  if (lastHost.empty() || lastPort == 0) {
    spdlog::error("Cannot reconnect: No previous connection information "
                  "available.");
    return false;
  }

  spdlog::info("Attempting to reconnect to {}:{}", lastHost, lastPort);

  disconnect();

  return connect(lastHost, lastPort);
}

void DeviceClient::resetState() {
  spdlog::debug("Resetting client state for reconnection.");
  {
    std::lock_guard<std::mutex> lock(responseMutex);
    responses.clear();
    responseCV.notify_all();
  }

  {
    std::lock_guard<std::mutex> lock(callbacksMutex);
    for (auto &[id, callback] : asyncCallbacks) {
      if (callback) {
        spdlog::debug("Notifying async callback for {} about connection loss.",
                      id);
        std::thread([cb = callback]() {
          cb(json{{"error", "Connection lost"}});
        }).detach();
      }
    }
    asyncCallbacks.clear();
  }

  if (messageQueueManager) {
    spdlog::debug("Message queue state reset (if applicable).");
  }
}

void DeviceClient::publishEvent(const std::string &eventName,
                                const json &details,
                                Message::Priority priority) {
  if (!connected) {
    spdlog::error("Cannot publish event '{}': Not connected to server",
                  eventName);
    return;
  }

  EventMessage event(eventName);
  event.setPriority(priority);

  if (!details.is_null()) {
    event.setDetails(details);
  }

  if (messageQueueManager) {
    messageQueueManager->sendMessage(event);
    spdlog::debug("Published event '{}' via message queue", eventName);
  } else {
    spdlog::error("Cannot publish event '{}': MessageQueueManager is null",
                  eventName);
  }
}

void DeviceClient::setMessageRetryParams(int maxRetries, int retryIntervalMs) {
  if (messageQueueManager) {
    messageQueueManager->setRetryParams(maxRetries, retryIntervalMs);
    spdlog::info(
        "Message retry parameters updated: maxRetries={}, retryIntervalMs={}",
        maxRetries, retryIntervalMs);
  } else {
    spdlog::warn(
        "Cannot set message retry params: MessageQueueManager is null");
  }
}

void DeviceClient::setAutoReconnect(bool enable, int intervalMs,
                                    int maxAttempts) {
  enableAutoReconnect = enable;
  reconnectIntervalMs = intervalMs;
  maxReconnectAttempts = maxAttempts;
  spdlog::info("Auto-reconnect settings updated: enabled={}, interval={}ms, "
               "maxAttempts={}",
               enable, intervalMs, maxAttempts);

  if (!enable && reconnecting.load()) {
    reconnecting.store(false);
    responseCV.notify_all();
  }
}

json DeviceClient::getStatusInfo() const {
  json status;

  status["connected"] = connected;
  status["host"] = lastHost;
  status["port"] = lastPort;

  status["autoReconnectEnabled"] = enableAutoReconnect;
  status["reconnecting"] = reconnecting.load();
  status["reconnectCount"] = reconnectCount;
  status["maxReconnectAttempts"] = maxReconnectAttempts;
  status["reconnectIntervalMs"] = reconnectIntervalMs;

  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    status["deviceCount"] = devices.is_null() ? 0 : devices.size();
  }

  {
    std::lock_guard<std::mutex> lock(subscriptionsMutex);
    status["propertySubscriptionCount"] = propertySubscriptions.size();
    status["eventSubscriptionCount"] = eventSubscriptions.size();
  }

  {
    std::lock_guard<std::mutex> lock(callbacksMutex);
    status["pendingAsyncCallbackCount"] = asyncCallbacks.size();
  }

  if (messageQueueManager) {
    status["messageQueueInfoAvailable"] = true;
  } else {
    status["messageQueueInfoAvailable"] = false;
  }

  return status;
}

} // namespace hydrogen
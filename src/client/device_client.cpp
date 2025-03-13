#include "client/device_client.h"
#include "common/logger.h"
#include "common/utils.h"
#include <chrono>
#include <future>

namespace astrocomm {

DeviceClient::DeviceClient() : ws(ioc), connected(false), running(false) {
  logInfo("Device client initialized", "DeviceClient");
}

DeviceClient::~DeviceClient() {
  stopMessageProcessing();
  disconnect();
}

bool DeviceClient::connect(const std::string &host, uint16_t port) {
  try {
    // 创建WebSocket连接
    tcp::resolver resolver{ioc};
    auto const results = resolver.resolve(host, std::to_string(port));

    // 连接到服务器
    auto ep = net::connect(ws.next_layer(), results);

    // 设置WebSocket握手
    std::string host_port = host + ":" + std::to_string(port);
    ws.set_option(
        websocket::stream_base::decorator([](websocket::request_type &req) {
          req.set(http::field::user_agent, "DeviceClient/1.0");
        }));

    ws.handshake(host_port, "/ws");
    connected = true;

    logInfo("Connected to server at " + host + ":" + std::to_string(port),
            "DeviceClient");
    return true;
  } catch (const std::exception &e) {
    logError("Connection error: " + std::string(e.what()), "DeviceClient");
    return false;
  }
}

void DeviceClient::disconnect() {
  if (connected) {
    try {
      ws.close(websocket::close_code::normal);
      connected = false;
      logInfo("Disconnected from server", "DeviceClient");
    } catch (const std::exception &e) {
      logError("Error disconnecting: " + std::string(e.what()), "DeviceClient");
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

  // 保存设备信息到本地缓存
  if (response.contains("payload") && response["payload"].contains("devices")) {
    std::lock_guard<std::mutex> lock(devicesMutex);
    devices = response["payload"]["devices"];
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

  // 构建属性列表
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
                                  const json &parameters) {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  // 创建命令消息
  CommandMessage msg(command);
  msg.setDeviceId(deviceId);

  if (!parameters.is_null()) {
    msg.setParameters(parameters);
  }

  // 发送请求并等待响应
  return sendAndWaitForResponse(msg);
}

json DeviceClient::executeBatchCommands(
    const std::string &deviceId,
    const std::vector<std::pair<std::string, json>> &commands,
    bool sequential) {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  // 创建批处理命令
  CommandMessage msg("BATCH");
  msg.setDeviceId(deviceId);

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

  // 发送请求并等待响应
  return sendAndWaitForResponse(msg);
}

void DeviceClient::subscribeToProperty(const std::string &deviceId,
                                       const std::string &property,
                                       PropertyCallback callback) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  // 生成键
  std::string key = makePropertyKey(deviceId, property);
  propertySubscriptions[key] = callback;

  logInfo("Subscribed to property: " + property + " for device: " + deviceId,
          "DeviceClient");
}

void DeviceClient::unsubscribeFromProperty(const std::string &deviceId,
                                           const std::string &property) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  std::string key = makePropertyKey(deviceId, property);
  auto it = propertySubscriptions.find(key);

  if (it != propertySubscriptions.end()) {
    propertySubscriptions.erase(it);
    logInfo("Unsubscribed from property: " + property +
                " for device: " + deviceId,
            "DeviceClient");
  }
}

void DeviceClient::subscribeToEvent(const std::string &deviceId,
                                    const std::string &event,
                                    EventCallback callback) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  // 生成键
  std::string key = makeEventKey(deviceId, event);
  eventSubscriptions[key] = callback;

  logInfo("Subscribed to event: " + event + " for device: " + deviceId,
          "DeviceClient");
}

void DeviceClient::unsubscribeFromEvent(const std::string &deviceId,
                                        const std::string &event) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  std::string key = makeEventKey(deviceId, event);
  auto it = eventSubscriptions.find(key);

  if (it != eventSubscriptions.end()) {
    eventSubscriptions.erase(it);
    logInfo("Unsubscribed from event: " + event + " for device: " + deviceId,
            "DeviceClient");
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

  // 检查认证是否成功
  if (response.contains("payload") && response["payload"].contains("status")) {
    return response["payload"]["status"] == "SUCCESS";
  }

  return false;
}

void DeviceClient::run() {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  running = true;
  messageLoop();
}

void DeviceClient::startMessageProcessing() {
  if (message_thread.joinable()) {
    return; // 线程已经在运行
  }

  running = true;
  message_thread = std::thread(&DeviceClient::messageLoop, this);

  logInfo("Message processing thread started", "DeviceClient");
}

void DeviceClient::stopMessageProcessing() {
  running = false;

  if (message_thread.joinable()) {
    message_thread.join();
    logInfo("Message processing thread stopped", "DeviceClient");
  }
}

void DeviceClient::messageLoop() {
  logInfo("Message loop started", "DeviceClient");

  while (running && connected) {
    try {
      beast::flat_buffer buffer;
      ws.read(buffer);
      std::string message = beast::buffers_to_string(buffer.data());

      handleMessage(message);
    } catch (beast::system_error const &se) {
      if (se.code() == websocket::error::closed) {
        logInfo("WebSocket connection closed", "DeviceClient");
        connected = false;
        break;
      } else {
        logError("WebSocket error: " + se.code().message(), "DeviceClient");
      }
    } catch (const std::exception &e) {
      logError("Error in message loop: " + std::string(e.what()),
               "DeviceClient");
    }
  }

  logInfo("Message loop ended", "DeviceClient");
}

void DeviceClient::handleMessage(const std::string &message) {
  try {
    json j = json::parse(message);
    std::unique_ptr<Message> msg = createMessageFromJson(j);

    switch (msg->getMessageType()) {
    case MessageType::DISCOVERY_RESPONSE:
      handleDiscoveryResponse(
          *static_cast<DiscoveryResponseMessage *>(msg.get()));
      break;

    case MessageType::RESPONSE:
      handleResponseMessage(*static_cast<ResponseMessage *>(msg.get()));
      break;

    case MessageType::EVENT:
      handleEventMessage(*static_cast<EventMessage *>(msg.get()));
      break;

    case MessageType::ERROR:
      handleErrorMessage(*static_cast<ErrorMessage *>(msg.get()));
      break;

    default:
      logWarning("Received unexpected message type: " +
                     messageTypeToString(msg->getMessageType()),
                 "DeviceClient");
      break;
    }
  } catch (const std::exception &e) {
    logError("Error parsing message: " + std::string(e.what()), "DeviceClient");
  }
}

void DeviceClient::handleDiscoveryResponse(
    const DiscoveryResponseMessage &msg) {
  // 处理设备发现响应
  json devices = msg.getDevices();

  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    this->devices = devices;
  }

  // 将响应存入响应映射
  {
    std::lock_guard<std::mutex> lock(responseMutex);
    responses[msg.getOriginalMessageId()] = msg.toJson();
  }

  // 通知等待线程
  responseCV.notify_all();

  logInfo("Received discovery response with " + std::to_string(devices.size()) +
              " devices",
          "DeviceClient");
}

void DeviceClient::handleResponseMessage(const ResponseMessage &msg) {
  // 保存响应到映射
  {
    std::lock_guard<std::mutex> lock(responseMutex);
    responses[msg.getOriginalMessageId()] = msg.toJson();
  }

  // 通知等待线程
  responseCV.notify_all();

  logDebug("Received response for message: " + msg.getOriginalMessageId(),
           "DeviceClient");
}

void DeviceClient::handleEventMessage(const EventMessage &msg) {
  std::string deviceId = msg.getDeviceId();
  std::string eventName = msg.getEvent();
  json details = msg.getDetails();

  logInfo("Received event: " + eventName + " from device: " + deviceId,
          "DeviceClient");

  // 处理属性变更事件
  if (eventName == "PROPERTY_CHANGED") {
    json properties = msg.getProperties();

    // 通知所有相关的属性订阅者
    std::lock_guard<std::mutex> lock(subscriptionsMutex);

    for (auto it = properties.begin(); it != properties.end(); ++it) {
      std::string propName = it.key();
      json propValue = it.value();

      std::string key = makePropertyKey(deviceId, propName);
      auto subIt = propertySubscriptions.find(key);

      if (subIt != propertySubscriptions.end()) {
        // 调用回调函数
        subIt->second(deviceId, propName, propValue["value"]);
      }
    }
  } else {
    // 通知事件订阅者
    std::lock_guard<std::mutex> lock(subscriptionsMutex);

    std::string key = makeEventKey(deviceId, eventName);
    auto it = eventSubscriptions.find(key);

    if (it != eventSubscriptions.end()) {
      // 调用回调函数
      it->second(deviceId, eventName, details);
    }
  }
}

void DeviceClient::handleErrorMessage(const ErrorMessage &msg) {
  // 打印错误消息
  logError("Received error: " + msg.getErrorCode() + " - " +
               msg.getErrorMessage(),
           "DeviceClient");

  // 如果有原始消息ID，将错误存入响应映射
  if (!msg.getOriginalMessageId().empty()) {
    std::lock_guard<std::mutex> lock(responseMutex);
    responses[msg.getOriginalMessageId()] = msg.toJson();
    responseCV.notify_all();
  }
}

json DeviceClient::sendAndWaitForResponse(const Message &msg,
                                          int timeoutSeconds) {
  if (!connected) {
    throw std::runtime_error("Not connected to server");
  }

  // 确保消息有ID
  std::string messageId = msg.getMessageId();

  try {
    // 发送消息
    std::string msgJson = msg.toJson().dump();
    ws.write(net::buffer(msgJson));

    logDebug("Sent message: " + messageId, "DeviceClient");

    // 等待响应
    std::unique_lock<std::mutex> lock(responseMutex);
    bool success = responseCV.wait_for(
        lock, std::chrono::seconds(timeoutSeconds), [this, &messageId]() {
          return responses.find(messageId) != responses.end();
        });

    if (!success) {
      throw std::runtime_error("Timeout waiting for response");
    }

    // 获取并移除响应
    json response = responses[messageId];
    responses.erase(messageId);

    return response;
  } catch (const std::exception &e) {
    logError("Error sending message: " + std::string(e.what()), "DeviceClient");
    throw;
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

} // namespace astrocomm
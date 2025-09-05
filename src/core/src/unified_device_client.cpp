#include "hydrogen/core/unified_device_client.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <iomanip>
#include <random>
#include <sstream>

namespace hydrogen {
namespace core {

// UnifiedDeviceClient implementation
UnifiedDeviceClient::UnifiedDeviceClient(const ClientConnectionConfig &config)
    : config_(config), errorMapper_(std::make_shared<ProtocolErrorMapper>()) {

  statistics_.connectionTime = std::chrono::system_clock::now();
  statistics_.lastMessageTime = std::chrono::system_clock::now();
  lastDeviceUpdate_ = std::chrono::system_clock::now();
  lastReconnectionAttempt_ = std::chrono::system_clock::now();

  // Initialize error handler if not provided
  if (!errorHandler_) {
    errorHandler_ = std::make_shared<StandardWebSocketErrorHandler>();
  }

  spdlog::info("UnifiedDeviceClient: Initialized with host {}:{}", config_.host,
               config_.port);
}

UnifiedDeviceClient::~UnifiedDeviceClient() { disconnect(); }

bool UnifiedDeviceClient::connect() {
  return connect(config_.host, config_.port);
}

bool UnifiedDeviceClient::connect(const std::string &host, uint16_t port) {
  if (connected_.load()) {
    spdlog::warn("UnifiedDeviceClient: Already connected to {}:{}", host, port);
    return true;
  }

  if (connecting_.load()) {
    spdlog::warn("UnifiedDeviceClient: Connection attempt already in progress");
    return false;
  }

  connecting_.store(true);
  spdlog::info("UnifiedDeviceClient: Connecting to {}:{}", host, port);

  try {
    // Update config if different
    if (host != config_.host || port != config_.port) {
      config_.host = host;
      config_.port = port;
    }

    bool success = establishConnection();

    if (success) {
      connected_.store(true);
      statistics_.isConnected = true;
      statistics_.connectionTime = std::chrono::system_clock::now();
      reconnectionAttempts_.store(0);

      // Start background threads
      startMessageProcessing();

      if (config_.heartbeatInterval.count() > 0) {
        heartbeatThread_ =
            std::thread(&UnifiedDeviceClient::heartbeatLoop, this);
      }

      if (config_.enableAutoReconnect) {
        reconnectionThread_ =
            std::thread(&UnifiedDeviceClient::reconnectionLoop, this);
      }

      notifyConnectionChange(true);
      spdlog::info("UnifiedDeviceClient: Successfully connected to {}:{}", host,
                   port);
    } else {
      spdlog::error("UnifiedDeviceClient: Failed to connect to {}:{}", host,
                    port);
      notifyError("Connection failed");
    }

    connecting_.store(false);
    return success;

  } catch (const std::exception &e) {
    connecting_.store(false);
    spdlog::error("UnifiedDeviceClient: Connection exception: {}", e.what());
    notifyError("Connection exception: " + std::string(e.what()));
    return false;
  }
}

void UnifiedDeviceClient::disconnect() {
  if (!connected_.load()) {
    return;
  }

  spdlog::info("UnifiedDeviceClient: Disconnecting from {}:{}", config_.host,
               config_.port);

  shutdown_.store(true);
  connected_.store(false);

  // Stop message processing
  stopMessageProcessing();

  // Wake up all waiting threads
  {
    std::lock_guard<std::mutex> lock(pendingResponsesMutex_);
    for (auto &[messageId, cv] : responseWaiters_) {
      cv->notify_all();
    }
  }

  // Join threads
  if (messageProcessingThread_.joinable()) {
    messageProcessingThread_.join();
  }
  if (heartbeatThread_.joinable()) {
    heartbeatThread_.join();
  }
  if (reconnectionThread_.joinable()) {
    reconnectionThread_.join();
  }

  // Cleanup connection
  cleanupConnection();

  statistics_.isConnected = false;
  notifyConnectionChange(false);

  spdlog::info("UnifiedDeviceClient: Disconnected from {}:{}", config_.host,
               config_.port);
}

bool UnifiedDeviceClient::isConnected() const {
  return connected_.load() && websocketIsConnected();
}

void UnifiedDeviceClient::updateConfig(const ClientConnectionConfig &config) {
  bool needsReconnect =
      (config.host != config_.host || config.port != config_.port);
  config_ = config;

  if (needsReconnect && connected_.load()) {
    spdlog::info("UnifiedDeviceClient: Configuration changed, reconnecting...");
    disconnect();
    connect();
  }
}

void UnifiedDeviceClient::setAutoReconnect(bool enable,
                                           std::chrono::milliseconds interval,
                                           int maxAttempts) {
  config_.enableAutoReconnect = enable;
  config_.reconnectInterval = interval;
  config_.maxReconnectAttempts = maxAttempts;

  spdlog::debug("UnifiedDeviceClient: Auto-reconnect {}, interval: {}ms, max "
                "attempts: {}",
                enable ? "enabled" : "disabled", interval.count(), maxAttempts);
}

json UnifiedDeviceClient::discoverDevices(
    const std::vector<std::string> &deviceTypes) {
  if (!isConnected()) {
    throw std::runtime_error("Not connected to server");
  }

  auto discoveryRequest = std::make_unique<DiscoveryRequestMessage>();

  json filter;
  if (!deviceTypes.empty()) {
    filter["deviceTypes"] = deviceTypes;
  }
  discoveryRequest->setFilter(filter);

  json response =
      sendMessage(std::move(discoveryRequest), config_.messageTimeout);

  // Update device cache
  {
    std::lock_guard<std::mutex> lock(deviceCacheMutex_);
    if (response.contains("devices")) {
      deviceCache_ = response["devices"];
      lastDeviceUpdate_ = std::chrono::system_clock::now();
    }
  }

  return response;
}

json UnifiedDeviceClient::getDevices() const {
  std::lock_guard<std::mutex> lock(deviceCacheMutex_);
  return deviceCache_;
}

json UnifiedDeviceClient::getDeviceInfo(const std::string &deviceId) const {
  std::lock_guard<std::mutex> lock(deviceCacheMutex_);

  if (deviceCache_.is_array()) {
    for (const auto &device : deviceCache_) {
      if (device.contains("deviceId") && device["deviceId"] == deviceId) {
        return device;
      }
    }
  }

  return json::object();
}

json UnifiedDeviceClient::getDeviceProperties(
    const std::string &deviceId, const std::vector<std::string> &properties) {
  if (!isConnected()) {
    throw std::runtime_error("Not connected to server");
  }

  auto command = std::make_unique<CommandMessage>("get_properties");

  json parameters;
  parameters["deviceId"] = deviceId;
  if (!properties.empty()) {
    parameters["properties"] = properties;
  }
  command->setParameters(parameters);

  return sendMessage(std::move(command), config_.messageTimeout);
}

json UnifiedDeviceClient::setDeviceProperties(const std::string &deviceId,
                                              const json &properties) {
  if (!isConnected()) {
    throw std::runtime_error("Not connected to server");
  }

  auto command = std::make_unique<CommandMessage>("set_properties");

  json parameters;
  parameters["deviceId"] = deviceId;
  parameters["properties"] = properties;
  command->setParameters(parameters);

  return sendMessage(std::move(command), config_.messageTimeout);
}

json UnifiedDeviceClient::executeCommand(const std::string &deviceId,
                                         const std::string &command,
                                         const json &parameters,
                                         Message::QoSLevel qosLevel) {
  if (!isConnected()) {
    throw std::runtime_error("Not connected to server");
  }

  auto commandMsg = std::make_unique<CommandMessage>(command);

  json cmdParameters = parameters;
  cmdParameters["deviceId"] = deviceId;
  commandMsg->setParameters(cmdParameters);
  commandMsg->setQoSLevel(qosLevel);

  return sendMessage(std::move(commandMsg), config_.messageTimeout);
}

void UnifiedDeviceClient::executeCommandAsync(
    const std::string &deviceId, const std::string &command,
    const json &parameters, Message::QoSLevel qosLevel,
    std::function<void(const json &)> callback) {
  if (!isConnected()) {
    if (callback) {
      json error;
      error["error"] = "Not connected to server";
      callback(error);
    }
    return;
  }

  auto commandMsg = std::make_unique<CommandMessage>(command);

  json cmdParameters = parameters;
  cmdParameters["deviceId"] = deviceId;
  commandMsg->setParameters(cmdParameters);
  commandMsg->setQoSLevel(qosLevel);

  sendMessageAsync(std::move(commandMsg), callback);
}

json UnifiedDeviceClient::executeBatchCommands(
    const std::string &deviceId,
    const std::vector<std::pair<std::string, json>> &commands, bool sequential,
    Message::QoSLevel qosLevel) {
  if (!isConnected()) {
    throw std::runtime_error("Not connected to server");
  }

  auto batchCommand = std::make_unique<CommandMessage>("batch_execute");

  json parameters;
  parameters["deviceId"] = deviceId;
  parameters["sequential"] = sequential;

  json commandList = json::array();
  for (const auto &[cmd, params] : commands) {
    json commandEntry;
    commandEntry["command"] = cmd;
    commandEntry["parameters"] = params;
    commandList.push_back(commandEntry);
  }
  parameters["commands"] = commandList;

  batchCommand->setParameters(parameters);
  batchCommand->setQoSLevel(qosLevel);

  return sendMessage(std::move(batchCommand),
                     config_.messageTimeout * commands.size());
}

json UnifiedDeviceClient::sendMessage(std::shared_ptr<Message> message,
                                      std::chrono::milliseconds timeout) {
  if (!isConnected()) {
    throw std::runtime_error("Not connected to server");
  }

  std::string messageId = message->getMessageId();

  // Set up response waiting
  std::condition_variable responseCV;
  {
    std::lock_guard<std::mutex> lock(pendingResponsesMutex_);
    responseWaiters_[messageId] = &responseCV;
  }

  try {
    // Send the message
    auto &transformer = getGlobalMessageTransformer();
    auto result = transformer.transform(*message, protocol_);

    if (!result.success) {
      throw std::runtime_error("Failed to transform message: " +
                               result.errorMessage);
    }

    std::string messageData = result.transformedData.dump();
    if (!websocketSend(messageData)) {
      throw std::runtime_error("Failed to send message over WebSocket");
    }

    updateStatistics(true, false, false);

    // Wait for response
    std::unique_lock<std::mutex> lock(pendingResponsesMutex_);
    bool received = responseCV.wait_for(lock, timeout, [&] {
      return pendingResponses_.find(messageId) != pendingResponses_.end();
    });

    json response;
    if (received) {
      response = pendingResponses_[messageId];
      pendingResponses_.erase(messageId);
    } else {
      response["error"] = "Message timeout";
      response["messageId"] = messageId;
    }

    responseWaiters_.erase(messageId);

    return response;

  } catch (const std::exception &e) {
    // Cleanup on error
    std::lock_guard<std::mutex> lock(pendingResponsesMutex_);
    responseWaiters_.erase(messageId);
    pendingResponses_.erase(messageId);

    updateStatistics(false, false, true);
    throw;
  }
}

void UnifiedDeviceClient::sendMessageAsync(
    std::shared_ptr<Message> message,
    std::function<void(const json &)> callback) {
  if (!isConnected()) {
    if (callback) {
      json error;
      error["error"] = "Not connected to server";
      callback(error);
    }
    return;
  }

  std::string messageId = message->getMessageId();

  // Register callback
  if (callback) {
    std::lock_guard<std::mutex> lock(pendingResponsesMutex_);
    asyncCallbacks_[messageId] = callback;
  }

  try {
    // Send the message
    auto &transformer = getGlobalMessageTransformer();
    auto result = transformer.transform(*message, protocol_);

    if (!result.success) {
      if (callback) {
        json error;
        error["error"] = "Failed to transform message: " + result.errorMessage;
        callback(error);
      }
      return;
    }

    std::string messageData = result.transformedData.dump();
    if (!websocketSend(messageData)) {
      if (callback) {
        json error;
        error["error"] = "Failed to send message over WebSocket";
        callback(error);
      }
      return;
    }

    updateStatistics(true, false, false);

  } catch (const std::exception &e) {
    if (callback) {
      json error;
      error["error"] = e.what();
      callback(error);
    }
    updateStatistics(false, false, true);
  }
}

bool UnifiedDeviceClient::subscribeToProperty(const std::string &deviceId,
                                              const std::string &property) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex_);
  propertySubscriptions_[deviceId].push_back(property);

  // Send subscription request to server
  try {
    auto command = std::make_unique<CommandMessage>("subscribe_property");
    json parameters;
    parameters["deviceId"] = deviceId;
    parameters["property"] = property;
    command->setParameters(parameters);

    json response = sendMessage(std::move(command), config_.messageTimeout);
    return response.value("success", false);
  } catch (const std::exception &e) {
    spdlog::error(
        "UnifiedDeviceClient: Failed to subscribe to property {}.{}: {}",
        deviceId, property, e.what());
    return false;
  }
}

bool UnifiedDeviceClient::unsubscribeFromProperty(const std::string &deviceId,
                                                  const std::string &property) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex_);
  auto &props = propertySubscriptions_[deviceId];
  props.erase(std::remove(props.begin(), props.end(), property), props.end());

  try {
    auto command = std::make_unique<CommandMessage>("unsubscribe_property");
    json parameters;
    parameters["deviceId"] = deviceId;
    parameters["property"] = property;
    command->setParameters(parameters);

    json response = sendMessage(std::move(command), config_.messageTimeout);
    return response.value("success", false);
  } catch (const std::exception &e) {
    spdlog::error(
        "UnifiedDeviceClient: Failed to unsubscribe from property {}.{}: {}",
        deviceId, property, e.what());
    return false;
  }
}

bool UnifiedDeviceClient::subscribeToEvents(const std::string &deviceId,
                                            const std::string &eventType) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex_);
  eventSubscriptions_[deviceId].push_back(eventType);

  try {
    auto command = std::make_unique<CommandMessage>("subscribe_events");
    json parameters;
    parameters["deviceId"] = deviceId;
    if (!eventType.empty()) {
      parameters["eventType"] = eventType;
    }
    command->setParameters(parameters);

    json response = sendMessage(std::move(command), config_.messageTimeout);
    return response.value("success", false);
  } catch (const std::exception &e) {
    spdlog::error(
        "UnifiedDeviceClient: Failed to subscribe to events for {}: {}",
        deviceId, e.what());
    return false;
  }
}

bool UnifiedDeviceClient::unsubscribeFromEvents(const std::string &deviceId,
                                                const std::string &eventType) {
  std::lock_guard<std::mutex> lock(subscriptionsMutex_);
  auto &events = eventSubscriptions_[deviceId];
  events.erase(std::remove(events.begin(), events.end(), eventType),
               events.end());

  try {
    auto command = std::make_unique<CommandMessage>("unsubscribe_events");
    json parameters;
    parameters["deviceId"] = deviceId;
    if (!eventType.empty()) {
      parameters["eventType"] = eventType;
    }
    command->setParameters(parameters);

    json response = sendMessage(std::move(command), config_.messageTimeout);
    return response.value("success", false);
  } catch (const std::exception &e) {
    spdlog::error(
        "UnifiedDeviceClient: Failed to unsubscribe from events for {}: {}",
        deviceId, e.what());
    return false;
  }
}

void UnifiedDeviceClient::startMessageProcessing() {
  if (messageProcessingActive_.load()) {
    return;
  }

  messageProcessingActive_.store(true);
  messageProcessingThread_ =
      std::thread(&UnifiedDeviceClient::messageProcessingLoop, this);
  spdlog::debug("UnifiedDeviceClient: Message processing started");
}

void UnifiedDeviceClient::stopMessageProcessing() {
  if (!messageProcessingActive_.load()) {
    return;
  }

  messageProcessingActive_.store(false);

  if (messageProcessingThread_.joinable()) {
    messageProcessingThread_.join();
  }

  spdlog::debug("UnifiedDeviceClient: Message processing stopped");
}

bool UnifiedDeviceClient::isMessageProcessingActive() const {
  return messageProcessingActive_.load();
}

ClientStatistics UnifiedDeviceClient::getStatistics() const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  return statistics_;
}

void UnifiedDeviceClient::resetStatistics() {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  statistics_ = ClientStatistics{};
  statistics_.isConnected = connected_.load();
  statistics_.connectionTime = std::chrono::system_clock::now();
  statistics_.lastMessageTime = std::chrono::system_clock::now();
}

bool UnifiedDeviceClient::establishConnection() {
  try {
    return websocketConnect();
  } catch (const std::exception &e) {
    spdlog::error("UnifiedDeviceClient: Failed to establish connection: {}",
                  e.what());
    return false;
  }
}

void UnifiedDeviceClient::cleanupConnection() {
  try {
    websocketDisconnect();
  } catch (const std::exception &e) {
    spdlog::error("UnifiedDeviceClient: Error during connection cleanup: {}",
                  e.what());
  }
}

bool UnifiedDeviceClient::attemptReconnection() {
  if (!config_.enableAutoReconnect) {
    return false;
  }

  int attempts = reconnectionAttempts_.load();
  if (config_.maxReconnectAttempts > 0 &&
      attempts >= config_.maxReconnectAttempts) {
    spdlog::error(
        "UnifiedDeviceClient: Maximum reconnection attempts ({}) exceeded",
        config_.maxReconnectAttempts);
    return false;
  }

  auto now = std::chrono::system_clock::now();
  auto timeSinceLastAttempt =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now - lastReconnectionAttempt_);

  if (timeSinceLastAttempt < config_.reconnectInterval) {
    return false; // Too soon to retry
  }

  lastReconnectionAttempt_ = now;
  reconnectionAttempts_.fetch_add(1);

  spdlog::info("UnifiedDeviceClient: Attempting reconnection {} of {}",
               attempts + 1,
               config_.maxReconnectAttempts > 0
                   ? std::to_string(config_.maxReconnectAttempts)
                   : "unlimited");

  {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.reconnectionAttempts++;
  }

  return connect();
}

void UnifiedDeviceClient::messageProcessingLoop() {
  spdlog::debug("UnifiedDeviceClient: Message processing loop started");

  while (messageProcessingActive_.load() && !shutdown_.load()) {
    try {
      if (!isConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      std::string messageData;
      if (websocketReceive(messageData)) {
        try {
          json message = json::parse(messageData);
          handleIncomingMessage(message);
          updateStatistics(false, true, false);
        } catch (const json::parse_error &e) {
          spdlog::error(
              "UnifiedDeviceClient: Failed to parse incoming message: {}",
              e.what());
          updateStatistics(false, false, true);
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

    } catch (const std::exception &e) {
      spdlog::error("UnifiedDeviceClient: Error in message processing loop: {}",
                    e.what());
      updateStatistics(false, false, true);
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
  }

  spdlog::debug("UnifiedDeviceClient: Message processing loop stopped");
}

void UnifiedDeviceClient::heartbeatLoop() {
  spdlog::debug("UnifiedDeviceClient: Heartbeat loop started");

  while (!shutdown_.load()) {
    std::this_thread::sleep_for(config_.heartbeatInterval);

    if (isConnected()) {
      try {
        json heartbeat;
        heartbeat["type"] = "heartbeat";
        heartbeat["timestamp"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();

        websocketSend(heartbeat.dump());
        spdlog::trace("UnifiedDeviceClient: Heartbeat sent");
      } catch (const std::exception &e) {
        spdlog::warn("UnifiedDeviceClient: Failed to send heartbeat: {}",
                     e.what());
      }
    }
  }

  spdlog::debug("UnifiedDeviceClient: Heartbeat loop stopped");
}

void UnifiedDeviceClient::reconnectionLoop() {
  spdlog::debug("UnifiedDeviceClient: Reconnection loop started");

  while (!shutdown_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    if (!isConnected() && !connecting_.load()) {
      attemptReconnection();
    }
  }

  spdlog::debug("UnifiedDeviceClient: Reconnection loop stopped");
}

void UnifiedDeviceClient::handleIncomingMessage(const json &message) {
  try {
    std::string messageType = message.value("type", "");

    if (messageType == "response") {
      handleResponse(message);
    } else if (messageType == "event") {
      handleEvent(message);
    } else if (messageType == "error") {
      handleError(message);
    } else if (messageType == "property_change") {
      handlePropertyChange(message);
    } else if (messageType == "heartbeat") {
      // Heartbeat acknowledgment - no action needed
      spdlog::trace("UnifiedDeviceClient: Heartbeat acknowledged");
    } else {
      // Generic message callback
      if (messageCallback_) {
        messageCallback_(message);
      }
    }

  } catch (const std::exception &e) {
    spdlog::error("UnifiedDeviceClient: Error handling incoming message: {}",
                  e.what());
  }
}

void UnifiedDeviceClient::handleResponse(const json &response) {
  std::string messageId = response.value("messageId", "");
  if (messageId.empty()) {
    spdlog::warn("UnifiedDeviceClient: Received response without messageId");
    return;
  }

  std::lock_guard<std::mutex> lock(pendingResponsesMutex_);

  // Check for synchronous response waiter
  auto waiterIt = responseWaiters_.find(messageId);
  if (waiterIt != responseWaiters_.end()) {
    pendingResponses_[messageId] = response;
    waiterIt->second->notify_one();
    return;
  }

  // Check for async callback
  auto callbackIt = asyncCallbacks_.find(messageId);
  if (callbackIt != asyncCallbacks_.end()) {
    try {
      callbackIt->second(response);
    } catch (const std::exception &e) {
      spdlog::error("UnifiedDeviceClient: Error in async callback: {}",
                    e.what());
    }
    asyncCallbacks_.erase(callbackIt);
    return;
  }

  spdlog::debug(
      "UnifiedDeviceClient: Received response for unknown message: {}",
      messageId);
}

void UnifiedDeviceClient::handleEvent(const json &event) {
  if (eventCallback_) {
    std::string deviceId = event.value("deviceId", "");
    std::string eventType = event.value("eventType", "");
    json eventData = event.value("data", json::object());

    try {
      eventCallback_(deviceId, eventType, eventData);
    } catch (const std::exception &e) {
      spdlog::error("UnifiedDeviceClient: Error in event callback: {}",
                    e.what());
    }
  }
}

void UnifiedDeviceClient::handleError(const json &error) {
  std::string errorMessage = error.value("message", "Unknown error");

  {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.errors++;
    statistics_.lastError = errorMessage;
  }

  spdlog::error("UnifiedDeviceClient: Server error: {}", errorMessage);
  notifyError(errorMessage);
}

void UnifiedDeviceClient::handlePropertyChange(const json &propertyChange) {
  if (propertyChangeCallback_) {
    std::string deviceId = propertyChange.value("deviceId", "");
    std::string property = propertyChange.value("property", "");
    json value = propertyChange.value("value", json{});

    try {
      propertyChangeCallback_(deviceId, property, value);
    } catch (const std::exception &e) {
      spdlog::error(
          "UnifiedDeviceClient: Error in property change callback: {}",
          e.what());
    }
  }
}

std::string UnifiedDeviceClient::generateMessageId() const {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<uint64_t> dis;

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                       now.time_since_epoch())
                       .count();

  std::stringstream ss;
  ss << "msg_" << timestamp << "_" << std::hex << dis(gen);
  return ss.str();
}

void UnifiedDeviceClient::updateStatistics(bool sent, bool received,
                                           bool error) {
  std::lock_guard<std::mutex> lock(statisticsMutex_);

  if (sent)
    statistics_.messagesSent++;
  if (received)
    statistics_.messagesReceived++;
  if (error)
    statistics_.errors++;

  if (sent || received) {
    statistics_.lastMessageTime = std::chrono::system_clock::now();
  }
}

void UnifiedDeviceClient::notifyConnectionChange(bool connected) {
  if (connectionCallback_) {
    try {
      connectionCallback_(connected);
    } catch (const std::exception &e) {
      spdlog::error("UnifiedDeviceClient: Error in connection callback: {}",
                    e.what());
    }
  }
}

void UnifiedDeviceClient::notifyError(const std::string &error) {
  if (errorCallback_) {
    try {
      errorCallback_(error);
    } catch (const std::exception &e) {
      spdlog::error("UnifiedDeviceClient: Error in error callback: {}",
                    e.what());
    }
  }
}

// WebSocket operations (placeholder implementations)
bool UnifiedDeviceClient::websocketConnect() {
  // TODO: Implement actual WebSocket connection
  // For now, simulate successful connection
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  websocketConnection_ = reinterpret_cast<void *>(0x12345678);
  return true;
}

void UnifiedDeviceClient::websocketDisconnect() {
  // TODO: Implement actual WebSocket disconnection
  websocketConnection_ = nullptr;
}

bool UnifiedDeviceClient::websocketSend(const std::string &data) {
  // TODO: Implement actual WebSocket send
  // For now, simulate successful send
  if (!websocketConnection_)
    return false;
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  return true;
}

bool UnifiedDeviceClient::websocketReceive(std::string &data) {
  // TODO: Implement actual WebSocket receive
  // For now, simulate occasional message reception
  if (!websocketConnection_)
    return false;

  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<int> dis(1, 1000);

  if (dis(gen) <= 5) { // 0.5% chance of receiving a message
    json simulatedMessage;
    simulatedMessage["type"] = "heartbeat";
    simulatedMessage["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    data = simulatedMessage.dump();
    return true;
  }

  return false;
}

bool UnifiedDeviceClient::websocketIsConnected() const {
  // TODO: Implement actual WebSocket connection check
  return websocketConnection_ != nullptr;
}

// UnifiedDeviceClientFactory implementation
std::unique_ptr<UnifiedDeviceClient>
UnifiedDeviceClientFactory::createClient(const ClientConnectionConfig &config) {
  return std::make_unique<UnifiedDeviceClient>(config);
}

std::unique_ptr<UnifiedDeviceClient>
UnifiedDeviceClientFactory::createAndConnect(const std::string &host,
                                             uint16_t port) {
  auto client = createClient();
  if (client->connect(host, port)) {
    return client;
  }
  return nullptr;
}

std::unique_ptr<UnifiedDeviceClient>
UnifiedDeviceClientFactory::createWebSocketClient(const std::string &host,
                                                  uint16_t port) {
  ClientConnectionConfig config = getDefaultConfig();
  config.host = host;
  config.port = port;
  config.endpoint = "/ws";
  return createClient(config);
}

std::unique_ptr<UnifiedDeviceClient>
UnifiedDeviceClientFactory::createHttpClient(const std::string &host,
                                             uint16_t port) {
  ClientConnectionConfig config = getDefaultConfig();
  config.host = host;
  config.port = port;
  config.endpoint = "/api";
  return createClient(config);
}

ClientConnectionConfig UnifiedDeviceClientFactory::getDefaultConfig() {
  ClientConnectionConfig config;
  config.host = "localhost";
  config.port = 8080;
  config.endpoint = "/ws";
  config.useTls = false;
  config.connectTimeout = std::chrono::milliseconds{5000};
  config.messageTimeout = std::chrono::milliseconds{5000};
  config.heartbeatInterval = std::chrono::milliseconds{30000};
  config.enableAutoReconnect = true;
  config.reconnectInterval = std::chrono::milliseconds{5000};
  config.maxReconnectAttempts = 0; // Unlimited
  return config;
}

ClientConnectionConfig
UnifiedDeviceClientFactory::getSecureConfig(const std::string &host,
                                            uint16_t port) {
  ClientConnectionConfig config = getDefaultConfig();
  config.host = host;
  config.port = port;
  config.useTls = true;
  return config;
}

ClientConnectionConfig
UnifiedDeviceClientFactory::getHighPerformanceConfig(const std::string &host,
                                                     uint16_t port) {
  ClientConnectionConfig config = getDefaultConfig();
  config.host = host;
  config.port = port;
  config.connectTimeout = std::chrono::milliseconds{2000};
  config.messageTimeout = std::chrono::milliseconds{2000};
  config.heartbeatInterval = std::chrono::milliseconds{10000};
  config.reconnectInterval = std::chrono::milliseconds{1000};
  return config;
}

} // namespace core
} // namespace hydrogen

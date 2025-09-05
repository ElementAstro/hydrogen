#include "hydrogen/core/zmq_queue_client.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>

namespace hydrogen {
namespace core {

// ZmqMessage implementation
json ZmqMessage::toJson() const {
  json j;
  j["id"] = id;
  j["content"] = content;
  j["clientId"] = clientId;
  j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                       timestamp.time_since_epoch())
                       .count();
  j["type"] = static_cast<int>(type);
  j["metadata"] = metadata;
  return j;
}

ZmqMessage ZmqMessage::fromJson(const json &j) {
  ZmqMessage msg;
  msg.id = j.value("id", "");
  msg.content = j.value("content", "");
  msg.clientId = j.value("clientId", "");

  if (j.contains("timestamp")) {
    auto timestamp_ms = j["timestamp"].get<int64_t>();
    msg.timestamp = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp_ms));
  } else {
    msg.timestamp = std::chrono::system_clock::now();
  }

  msg.type = static_cast<ZmqMessageType>(j.value("type", 0));
  msg.metadata = j.value("metadata", json::object());

  return msg;
}

// ZmqQueueClient implementation
ZmqQueueClient::ZmqQueueClient(const ZmqClientConfig &config)
    : config_(config) {
  statistics_.lastMessageTime = std::chrono::steady_clock::now();
  lastReconnectAttempt_ = std::chrono::steady_clock::now();
}

ZmqQueueClient::~ZmqQueueClient() { disconnect(); }

bool ZmqQueueClient::connect() {
  if (connected_.load()) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::warn("ZmqQueueClient: Already connected to {}",
                 config_.serverAddress);
#endif
    return true;
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("ZmqQueueClient: Connecting to {} with socket type {}",
               config_.serverAddress, static_cast<int>(config_.socketType));
#endif

  try {
    if (!initializeSocket()) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::error("ZmqQueueClient: Failed to initialize socket");
#endif
      return false;
    }

    // Start processing threads
    running_.store(true);
    shutdown_.store(false);

    messageProcessorThread_ =
        std::thread(&ZmqQueueClient::messageProcessorLoop, this);
    receiverThread_ = std::thread(&ZmqQueueClient::receiverLoop, this);

    if (config_.enableHeartbeat) {
      heartbeatThread_ = std::thread(&ZmqQueueClient::heartbeatLoop, this);
    }

    connected_.store(true);
    statistics_.isConnected = true;
    statistics_.connectionTime = std::chrono::steady_clock::now();
    statistics_.connectionAttempts++;
    reconnectAttempts_.store(0);

    updateConnectionStatus(true);
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("ZmqQueueClient: Successfully connected to {}",
                 config_.serverAddress);
#endif

    return true;

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("ZmqQueueClient: Connection failed: {}", e.what());
#endif
    handleError("Connection failed: " + std::string(e.what()));
    return false;
  }
}

void ZmqQueueClient::disconnect() {
  if (!connected_.load()) {
    return;
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("ZmqQueueClient: Disconnecting from {}", config_.serverAddress);
#endif

  shutdown_.store(true);
  running_.store(false);

  // Wake up all waiting threads
  outgoingQueueCV_.notify_all();
  incomingQueueCV_.notify_all();

  // Wake up response waiters
  {
    std::lock_guard<std::mutex> lock(responsesMutex_);
    for (auto &[messageId, cv] : responseWaiters_) {
      cv->notify_all();
    }
  }

  // Join threads
  if (messageProcessorThread_.joinable()) {
    messageProcessorThread_.join();
  }
  if (receiverThread_.joinable()) {
    receiverThread_.join();
  }
  if (heartbeatThread_.joinable()) {
    heartbeatThread_.join();
  }

  // Cleanup socket
  cleanupSocket();

  // Clear queues
  clearQueue();

  connected_.store(false);
  statistics_.isConnected = false;

  updateConnectionStatus(false);
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("ZmqQueueClient: Disconnected from {}", config_.serverAddress);
#endif
}

bool ZmqQueueClient::isConnected() const { return connected_.load(); }

bool ZmqQueueClient::reconnect() {
  if (connected_.load()) {
    disconnect();
  }

  auto now = std::chrono::steady_clock::now();
  auto timeSinceLastAttempt =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now - lastReconnectAttempt_)
          .count();

  if (timeSinceLastAttempt < config_.reconnectInterval) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::debug("ZmqQueueClient: Reconnect attempt too soon, waiting...");
#endif
    return false;
  }

  int attempts = reconnectAttempts_.load();
  if (attempts >= config_.maxReconnectAttempts) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("ZmqQueueClient: Maximum reconnect attempts ({}) exceeded",
                  config_.maxReconnectAttempts);
#endif
    return false;
  }

  lastReconnectAttempt_ = now;
  reconnectAttempts_.fetch_add(1);
  statistics_.reconnectionAttempts++;

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("ZmqQueueClient: Reconnection attempt {} of {}", attempts + 1,
               config_.maxReconnectAttempts);
#endif

  return connect();
}

bool ZmqQueueClient::sendMessage(const std::string &content,
                                 ZmqMessageType type) {
  ZmqMessage message;
  message.id = generateMessageId();
  message.content = content;
  message.clientId =
      "client_" + std::to_string(reinterpret_cast<uintptr_t>(this));
  message.timestamp = std::chrono::system_clock::now();
  message.type = type;

  return sendMessage(message);
}

bool ZmqQueueClient::sendMessage(const ZmqMessage &message) {
  if (!connected_.load()) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::warn("ZmqQueueClient: Cannot send message - not connected");
#endif
    return false;
  }

  enqueueMessage(message);
  return true;
}

bool ZmqQueueClient::sendMessageSync(const std::string &content,
                                     std::string &response, int timeoutMs) {
  if (!connected_.load()) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::warn("ZmqQueueClient: Cannot send sync message - not connected");
#endif
    return false;
  }

  ZmqMessage message;
  message.id = generateMessageId();
  message.content = content;
  message.clientId =
      "client_" + std::to_string(reinterpret_cast<uintptr_t>(this));
  message.timestamp = std::chrono::system_clock::now();
  message.type = ZmqMessageType::DATA;

  // Set up response waiting
  std::condition_variable responseCV;
  {
    std::lock_guard<std::mutex> lock(responsesMutex_);
    responseWaiters_[message.id] = &responseCV;
  }

  // Send the message
  enqueueMessage(message);

  // Wait for response
  std::unique_lock<std::mutex> lock(responsesMutex_);
  bool received =
      responseCV.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
        return pendingResponses_.find(message.id) != pendingResponses_.end();
      });

  if (received) {
    response = pendingResponses_[message.id];
    pendingResponses_.erase(message.id);
  }

  responseWaiters_.erase(message.id);

  if (!received) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::warn("ZmqQueueClient: Synchronous message timeout for ID: {}",
                 message.id);
#endif
  }

  return received;
}

void ZmqQueueClient::enqueueMessage(const ZmqMessage &message) {
  {
    std::lock_guard<std::mutex> lock(outgoingQueueMutex_);

    // Check queue size limits
    if (outgoingQueue_.size() >= static_cast<size_t>(config_.highWaterMark)) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::warn(
          "ZmqQueueClient: Outgoing queue full, dropping oldest message");
#endif
      outgoingQueue_.pop();                         // Drop oldest message
      updateStatistics(false, false, false, false); // Count as dropped
    }

    outgoingQueue_.push(message);
  }

  outgoingQueueCV_.notify_one();
  updateStatistics(false, false, true, false);

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug(
      "ZmqQueueClient: Message queued for sending: {} (queue size: {})",
      message.id, getQueueSize());
#endif
}

size_t ZmqQueueClient::getQueueSize() const {
  std::lock_guard<std::mutex> outLock(outgoingQueueMutex_);
  std::lock_guard<std::mutex> inLock(incomingQueueMutex_);
  return outgoingQueue_.size() + incomingQueue_.size();
}

size_t ZmqQueueClient::getOutgoingQueueSize() const {
  std::lock_guard<std::mutex> lock(outgoingQueueMutex_);
  return outgoingQueue_.size();
}

size_t ZmqQueueClient::getIncomingQueueSize() const {
  std::lock_guard<std::mutex> lock(incomingQueueMutex_);
  return incomingQueue_.size();
}

bool ZmqQueueClient::isQueueFull() const {
  return getQueueSize() >= static_cast<size_t>(config_.highWaterMark);
}

void ZmqQueueClient::flushOutgoingQueue() {
  std::lock_guard<std::mutex> lock(outgoingQueueMutex_);

  spdlog::info("ZmqQueueClient: Flushing outgoing queue ({} messages)",
               outgoingQueue_.size());

  while (!outgoingQueue_.empty()) {
    const auto &message = outgoingQueue_.front();
    spdlog::debug("ZmqQueueClient: Dropping queued message: {}", message.id);
    outgoingQueue_.pop();
  }
}

void ZmqQueueClient::clearQueue() {
  size_t outgoingCount = 0;
  size_t incomingCount = 0;

  {
    std::lock_guard<std::mutex> lock(outgoingQueueMutex_);
    outgoingCount = outgoingQueue_.size();
    while (!outgoingQueue_.empty()) {
      outgoingQueue_.pop();
    }
  }

  {
    std::lock_guard<std::mutex> lock(incomingQueueMutex_);
    incomingCount = incomingQueue_.size();
    while (!incomingQueue_.empty()) {
      incomingQueue_.pop();
    }
  }

  // Clear any pending responses
  {
    std::lock_guard<std::mutex> lock(responsesMutex_);
    pendingResponses_.clear();
    responseWaiters_.clear();
  }

  spdlog::info(
      "ZmqQueueClient: All queues cleared (outgoing: {}, incoming: {})",
      outgoingCount, incomingCount);
}

ZmqClientStatistics ZmqQueueClient::getStatistics() const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  ZmqClientStatistics stats = statistics_;
  stats.totalMessagesQueued = getQueueSize();
  return stats;
}

void ZmqQueueClient::resetStatistics() {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  statistics_ = ZmqClientStatistics{};
  statistics_.lastMessageTime = std::chrono::steady_clock::now();
  statistics_.isConnected = connected_.load();
  if (statistics_.isConnected) {
    statistics_.connectionTime = std::chrono::steady_clock::now();
  }
}

bool ZmqQueueClient::setSocketOption(int option, int value) {
  // TODO: Implement actual ZeroMQ socket option setting
  // For now, simulate success
  spdlog::debug("ZmqQueueClient: Setting socket option {} to {}", option,
                value);
  return true;
}

bool ZmqQueueClient::getSocketOption(int option, int &value) const {
  // TODO: Implement actual ZeroMQ socket option getting
  // For now, simulate success
  value = 0;
  spdlog::debug("ZmqQueueClient: Getting socket option {}", option);
  return true;
}

bool ZmqQueueClient::initializeSocket() {
  try {
    // TODO: Implement actual ZeroMQ socket initialization
    // context_ = zmq_ctx_new();
    // if (!context_) {
    //     spdlog::error("ZmqQueueClient: Failed to create ZeroMQ context");
    //     return false;
    // }
    //
    // socket_ = zmq_socket(context_, static_cast<int>(config_.socketType));
    // if (!socket_) {
    //     spdlog::error("ZmqQueueClient: Failed to create ZeroMQ socket");
    //     return false;
    // }
    //
    // // Set socket options
    // zmq_setsockopt(socket_, ZMQ_SNDHWM, &config_.highWaterMark,
    // sizeof(config_.highWaterMark)); zmq_setsockopt(socket_, ZMQ_RCVHWM,
    // &config_.highWaterMark, sizeof(config_.highWaterMark));
    // zmq_setsockopt(socket_, ZMQ_LINGER, &config_.lingerTime,
    // sizeof(config_.lingerTime)); zmq_setsockopt(socket_, ZMQ_RCVTIMEO,
    // &config_.receiveTimeout, sizeof(config_.receiveTimeout));
    // zmq_setsockopt(socket_, ZMQ_SNDTIMEO, &config_.sendTimeout,
    // sizeof(config_.sendTimeout));
    //
    // // Connect to server
    // int rc = zmq_connect(socket_, config_.serverAddress.c_str());
    // if (rc != 0) {
    //     spdlog::error("ZmqQueueClient: Failed to connect to {}: {}",
    //                  config_.serverAddress, zmq_strerror(errno));
    //     return false;
    // }

    // For now, simulate successful initialization
    context_ = reinterpret_cast<void *>(0x12345678);
    socket_ = reinterpret_cast<void *>(0x87654321);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return true;

  } catch (const std::exception &e) {
    spdlog::error("ZmqQueueClient: Socket initialization failed: {}", e.what());
    return false;
  }
}

void ZmqQueueClient::cleanupSocket() {
  // TODO: Implement actual ZeroMQ socket cleanup
  // if (socket_) {
  //     zmq_close(socket_);
  //     socket_ = nullptr;
  // }
  //
  // if (context_) {
  //     zmq_ctx_destroy(context_);
  //     context_ = nullptr;
  // }

  // For now, simulate cleanup
  socket_ = nullptr;
  context_ = nullptr;
}

void ZmqQueueClient::messageProcessorLoop() {
  spdlog::debug("ZmqQueueClient: Message processor loop started");

  while (running_.load()) {
    try {
      std::unique_lock<std::mutex> lock(outgoingQueueMutex_);

      // Wait for messages or shutdown
      outgoingQueueCV_.wait(
          lock, [this] { return !outgoingQueue_.empty() || !running_.load(); });

      // Process queued messages in batches to match server behavior
      std::vector<ZmqMessage> messageBatch;
      size_t batchSize =
          std::min(static_cast<size_t>(10), outgoingQueue_.size());

      for (size_t i = 0; i < batchSize && !outgoingQueue_.empty(); ++i) {
        messageBatch.push_back(outgoingQueue_.front());
        outgoingQueue_.pop();
      }
      lock.unlock();

      // Process batch outside of lock
      for (const auto &message : messageBatch) {
        if (!running_.load())
          break;

        bool sent = sendQueuedMessage(message);
        if (sent) {
          updateStatistics(true, false, false, true);
          spdlog::debug("ZmqQueueClient: Message sent successfully: {}",
                        message.id);
        } else {
          spdlog::warn("ZmqQueueClient: Failed to send message: {}",
                       message.id);
          handleError("Failed to send message: " + message.id);

          // Re-queue failed message if connection is still active
          if (connected_.load()) {
            std::lock_guard<std::mutex> requeueLock(outgoingQueueMutex_);
            // Add to front of queue for retry (priority)
            std::queue<ZmqMessage> tempQueue;
            tempQueue.push(message);
            while (!outgoingQueue_.empty()) {
              tempQueue.push(outgoingQueue_.front());
              outgoingQueue_.pop();
            }
            outgoingQueue_ = tempQueue;
          } else {
            // Attempt reconnection if connection lost
            std::thread([this] { reconnect(); }).detach();
          }
        }

        // Small delay between messages to prevent overwhelming
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      lock.lock();

    } catch (const std::exception &e) {
      spdlog::error("ZmqQueueClient: Error in message processor loop: {}",
                    e.what());
      handleError("Message processor error: " + std::string(e.what()));
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
  }

  spdlog::debug("ZmqQueueClient: Message processor loop stopped");
}

void ZmqQueueClient::receiverLoop() {
  spdlog::debug("ZmqQueueClient: Receiver loop started");

  while (running_.load()) {
    try {
      std::string receivedData;
      if (simulateReceive(receivedData)) {
        try {
          json messageJson = json::parse(receivedData);
          ZmqMessage message = ZmqMessage::fromJson(messageJson);

          // Check incoming queue size limits
          {
            std::lock_guard<std::mutex> lock(incomingQueueMutex_);
            if (incomingQueue_.size() >=
                static_cast<size_t>(config_.highWaterMark)) {
              spdlog::warn("ZmqQueueClient: Incoming queue full, dropping "
                           "oldest message");
              incomingQueue_.pop(); // Drop oldest message
            }
            incomingQueue_.push(message);
          }

          incomingQueueCV_.notify_one();

          // Process message immediately for better responsiveness
          processIncomingMessage(message);

          updateStatistics(false, true, false, true);
          spdlog::debug("ZmqQueueClient: Message received and queued: {} "
                        "(queue size: {})",
                        message.id, getQueueSize());

        } catch (const json::parse_error &e) {
          spdlog::error("ZmqQueueClient: Failed to parse received message: {}",
                        e.what());
          handleError("Message parse error: " + std::string(e.what()));
          updateStatistics(false, false, false, false); // Count as error
        }
      } else {
        // No message received, sleep briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

    } catch (const std::exception &e) {
      spdlog::error("ZmqQueueClient: Error in receiver loop: {}", e.what());
      handleError("Receiver error: " + std::string(e.what()));
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
  }

  spdlog::debug("ZmqQueueClient: Receiver loop stopped");
}

void ZmqQueueClient::heartbeatLoop() {
  spdlog::debug("ZmqQueueClient: Heartbeat loop started");

  while (running_.load()) {
    try {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(config_.heartbeatInterval));

      if (running_.load() && connected_.load()) {
        ZmqMessage heartbeat = createHeartbeatMessage();
        enqueueMessage(heartbeat);
        spdlog::trace("ZmqQueueClient: Heartbeat sent");
      }

    } catch (const std::exception &e) {
      spdlog::error("ZmqQueueClient: Error in heartbeat loop: {}", e.what());
      std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
  }

  spdlog::debug("ZmqQueueClient: Heartbeat loop stopped");
}

bool ZmqQueueClient::sendQueuedMessage(const ZmqMessage &message) {
  if (!connected_.load()) {
    return false;
  }

  try {
    std::string messageData = message.toJson().dump();
    return simulateSend(messageData);

  } catch (const std::exception &e) {
    spdlog::error("ZmqQueueClient: Failed to serialize message {}: {}",
                  message.id, e.what());
    return false;
  }
}

void ZmqQueueClient::processIncomingMessage(const ZmqMessage &message) {
  // Handle different message types appropriately
  switch (message.type) {
  case ZmqMessageType::DATA:
    handleSynchronousResponse(message);
    break;
  case ZmqMessageType::HEARTBEAT:
    handleHeartbeatMessage(message);
    break;
  case ZmqMessageType::CONTROL:
    handleControlMessage(message);
    break;
  case ZmqMessageType::BROADCAST:
    handleBroadcastMessage(message);
    break;
  default:
    spdlog::warn("ZmqQueueClient: Unknown message type: {}",
                 static_cast<int>(message.type));
    break;
  }

  // Call user message handler for all message types
  if (messageHandler_) {
    try {
      messageHandler_(message);
    } catch (const std::exception &e) {
      spdlog::error("ZmqQueueClient: Error in message handler: {}", e.what());
      handleError("Message handler error: " + std::string(e.what()));
    }
  }
}

void ZmqQueueClient::handleHeartbeatMessage(const ZmqMessage &message) {
  spdlog::trace("ZmqQueueClient: Received heartbeat from server: {}",
                message.id);

  // Update last activity time
  lastHeartbeatTime_ = std::chrono::steady_clock::now();

  // Send heartbeat response if required
  if (config_.enableHeartbeat) {
    ZmqMessage response;
    response.id = generateMessageId();
    response.content = "heartbeat_ack";
    response.clientId =
        "client_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    response.timestamp = std::chrono::system_clock::now();
    response.type = ZmqMessageType::HEARTBEAT;

    enqueueMessage(response);
  }
}

void ZmqQueueClient::handleControlMessage(const ZmqMessage &message) {
  spdlog::debug("ZmqQueueClient: Received control message: {}",
                message.content);

  try {
    json controlData = json::parse(message.content);
    std::string command = controlData.value("command", "");

    if (command == "disconnect") {
      spdlog::info("ZmqQueueClient: Server requested disconnect");
      disconnect();
    } else if (command == "flush_queue") {
      spdlog::info("ZmqQueueClient: Server requested queue flush");
      flushOutgoingQueue();
    } else if (command == "config_update") {
      spdlog::info("ZmqQueueClient: Server sent configuration update");
      // Handle configuration updates if needed
    } else {
      spdlog::warn("ZmqQueueClient: Unknown control command: {}", command);
    }

  } catch (const json::parse_error &e) {
    spdlog::error("ZmqQueueClient: Failed to parse control message: {}",
                  e.what());
  }
}

void ZmqQueueClient::handleBroadcastMessage(const ZmqMessage &message) {
  spdlog::debug("ZmqQueueClient: Received broadcast message: {}", message.id);

  // Broadcast messages are typically informational and don't require responses
  // Just log and pass to user handler
  updateStatistics(false, true, false, true);
}

void ZmqQueueClient::handleSynchronousResponse(const ZmqMessage &message) {
  // Check if this is a response to a synchronous request
  std::string originalMessageId =
      message.metadata.value("originalMessageId", "");
  if (originalMessageId.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lock(responsesMutex_);
  auto waiterIt = responseWaiters_.find(originalMessageId);
  if (waiterIt != responseWaiters_.end()) {
    pendingResponses_[originalMessageId] = message.content;
    waiterIt->second->notify_one();
  }
}

std::string ZmqQueueClient::generateMessageId() const {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<uint64_t> dis;

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                       now.time_since_epoch())
                       .count();

  std::stringstream ss;
  ss << "zmq_" << timestamp << "_" << std::hex << dis(gen);
  return ss.str();
}

ZmqMessage ZmqQueueClient::createHeartbeatMessage() const {
  ZmqMessage heartbeat;
  heartbeat.id = generateMessageId();
  heartbeat.content = "heartbeat";
  heartbeat.clientId =
      "client_" + std::to_string(reinterpret_cast<uintptr_t>(this));
  heartbeat.timestamp = std::chrono::system_clock::now();
  heartbeat.type = ZmqMessageType::HEARTBEAT;
  heartbeat.metadata = json{{"heartbeat", true}};

  return heartbeat;
}

void ZmqQueueClient::updateStatistics(bool sent, bool received, bool queued,
                                      bool processed) {
  std::lock_guard<std::mutex> lock(statisticsMutex_);

  if (sent)
    statistics_.totalMessagesSent++;
  if (received)
    statistics_.totalMessagesReceived++;
  if (queued)
    statistics_.totalMessagesQueued++;
  if (processed)
    statistics_.totalMessagesProcessed++;

  if (sent || received) {
    statistics_.lastMessageTime = std::chrono::steady_clock::now();
  }
}

void ZmqQueueClient::handleError(const std::string &error) {
  spdlog::error("ZmqQueueClient: {}", error);

  if (errorHandler_) {
    try {
      errorHandler_(error);
    } catch (const std::exception &e) {
      spdlog::error("ZmqQueueClient: Error in error handler: {}", e.what());
    }
  }
}

void ZmqQueueClient::updateConnectionStatus(bool connected) {
  if (connectionHandler_) {
    try {
      connectionHandler_(connected);
    } catch (const std::exception &e) {
      spdlog::error("ZmqQueueClient: Error in connection handler: {}",
                    e.what());
    }
  }
}

bool ZmqQueueClient::simulateSend(const std::string &data) {
  // TODO: Replace with actual ZeroMQ send
  // int rc = zmq_send(socket_, data.c_str(), data.length(), ZMQ_DONTWAIT);
  // return rc != -1;

  // For now, simulate successful send
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  return true;
}

bool ZmqQueueClient::simulateReceive(std::string &data) {
  // TODO: Replace with actual ZeroMQ receive
  // char buffer[4096];
  // int rc = zmq_recv(socket_, buffer, sizeof(buffer) - 1, ZMQ_DONTWAIT);
  // if (rc > 0) {
  //     buffer[rc] = '\0';
  //     data = std::string(buffer);
  //     return true;
  // }
  // return false;

  // For now, simulate occasional message reception
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<int> dis(1, 100);

  if (dis(gen) <= 5) { // 5% chance of receiving a message
    ZmqMessage simulatedMessage;
    simulatedMessage.id = generateMessageId();
    simulatedMessage.content = "simulated_response";
    simulatedMessage.clientId = "server";
    simulatedMessage.timestamp = std::chrono::system_clock::now();
    simulatedMessage.type = ZmqMessageType::DATA;

    data = simulatedMessage.toJson().dump();
    return true;
  }

  return false;
}

// ZmqQueueClientFactory implementation
std::unique_ptr<ZmqQueueClient>
ZmqQueueClientFactory::createClient(const ZmqClientConfig &config) {
  return std::make_unique<ZmqQueueClient>(config);
}

std::unique_ptr<ZmqQueueClient>
ZmqQueueClientFactory::createReqClient(const std::string &serverAddress) {
  ZmqClientConfig config = getDefaultConfig(ZmqSocketType::REQ);
  config.serverAddress = serverAddress;
  return createClient(config);
}

std::unique_ptr<ZmqQueueClient>
ZmqQueueClientFactory::createSubClient(const std::string &serverAddress,
                                       const std::string &topic) {
  ZmqClientConfig config = getDefaultConfig(ZmqSocketType::SUB);
  config.serverAddress = serverAddress;
  auto client = createClient(config);

  // TODO: Set subscription topic
  // if (!topic.empty()) {
  //     client->setSocketOption(ZMQ_SUBSCRIBE, topic.c_str());
  // }

  return client;
}

std::unique_ptr<ZmqQueueClient>
ZmqQueueClientFactory::createPullClient(const std::string &serverAddress) {
  ZmqClientConfig config = getDefaultConfig(ZmqSocketType::PULL);
  config.serverAddress = serverAddress;
  return createClient(config);
}

bool ZmqQueueClientFactory::isZmqAvailable() {
  // TODO: Check if ZeroMQ library is available
  return true; // For now, assume it's available
}

std::string ZmqQueueClientFactory::getZmqVersion() {
  // TODO: Return actual ZeroMQ version
  return "4.3.4"; // Simulated version
}

ZmqClientConfig
ZmqQueueClientFactory::getDefaultConfig(ZmqSocketType socketType) {
  ZmqClientConfig config;
  config.socketType = socketType;

  switch (socketType) {
  case ZmqSocketType::REQ:
    config.receiveTimeout = 5000;
    config.sendTimeout = 5000;
    config.enableHeartbeat = false;
    break;
  case ZmqSocketType::SUB:
    config.receiveTimeout = -1; // Blocking
    config.sendTimeout = 1000;
    config.enableHeartbeat = false;
    break;
  case ZmqSocketType::PULL:
    config.receiveTimeout = -1; // Blocking
    config.sendTimeout = 1000;
    config.enableHeartbeat = true;
    break;
  case ZmqSocketType::DEALER:
    config.receiveTimeout = 1000;
    config.sendTimeout = 1000;
    config.enableHeartbeat = true;
    break;
  default:
    // Use default values
    break;
  }

  return config;
}

// ZmqQueueManager implementation
ZmqQueueManager &ZmqQueueManager::getInstance() {
  static ZmqQueueManager instance;
  return instance;
}

void ZmqQueueManager::registerClient(const std::string &clientId,
                                     std::shared_ptr<ZmqQueueClient> client) {
  std::lock_guard<std::mutex> lock(clientsMutex_);
  clients_[clientId] = client;
  spdlog::info("ZmqQueueManager: Registered client: {}", clientId);
}

void ZmqQueueManager::unregisterClient(const std::string &clientId) {
  std::lock_guard<std::mutex> lock(clientsMutex_);
  auto it = clients_.find(clientId);
  if (it != clients_.end()) {
    clients_.erase(it);
    spdlog::info("ZmqQueueManager: Unregistered client: {}", clientId);
  }
}

std::shared_ptr<ZmqQueueClient>
ZmqQueueManager::getClient(const std::string &clientId) {
  std::lock_guard<std::mutex> lock(clientsMutex_);
  auto it = clients_.find(clientId);
  return (it != clients_.end()) ? it->second : nullptr;
}

bool ZmqQueueManager::broadcastMessage(const ZmqMessage &message) {
  std::lock_guard<std::mutex> lock(clientsMutex_);

  bool allSent = true;
  for (const auto &[clientId, client] : clients_) {
    if (client && client->isConnected()) {
      if (!client->sendMessage(message)) {
        spdlog::warn(
            "ZmqQueueManager: Failed to broadcast message to client: {}",
            clientId);
        allSent = false;
      }
    }
  }

  return allSent;
}

size_t ZmqQueueManager::getTotalQueueSize() const {
  std::lock_guard<std::mutex> lock(clientsMutex_);

  size_t totalSize = 0;
  for (const auto &[clientId, client] : clients_) {
    if (client) {
      totalSize += client->getQueueSize();
    }
  }

  return totalSize;
}

void ZmqQueueManager::clearAllQueues() {
  std::lock_guard<std::mutex> lock(clientsMutex_);

  for (const auto &[clientId, client] : clients_) {
    if (client) {
      client->clearQueue();
    }
  }

  spdlog::info("ZmqQueueManager: Cleared all client queues");
}

ZmqClientStatistics ZmqQueueManager::getAggregatedStatistics() const {
  std::lock_guard<std::mutex> lock(clientsMutex_);

  ZmqClientStatistics aggregated;
  size_t connectedClients = 0;

  for (const auto &[clientId, client] : clients_) {
    if (client) {
      auto stats = client->getStatistics();
      aggregated.totalMessagesSent += stats.totalMessagesSent;
      aggregated.totalMessagesReceived += stats.totalMessagesReceived;
      aggregated.totalMessagesQueued += stats.totalMessagesQueued;
      aggregated.totalMessagesProcessed += stats.totalMessagesProcessed;
      aggregated.connectionAttempts += stats.connectionAttempts;
      aggregated.reconnectionAttempts += stats.reconnectionAttempts;

      if (stats.isConnected) {
        connectedClients++;
      }

      if (stats.lastMessageTime > aggregated.lastMessageTime) {
        aggregated.lastMessageTime = stats.lastMessageTime;
      }
    }
  }

  aggregated.isConnected = (connectedClients > 0);

  return aggregated;
}

std::vector<std::pair<std::string, ZmqClientStatistics>>
ZmqQueueManager::getAllClientStatistics() const {
  std::lock_guard<std::mutex> lock(clientsMutex_);

  std::vector<std::pair<std::string, ZmqClientStatistics>> allStats;

  for (const auto &[clientId, client] : clients_) {
    if (client) {
      allStats.emplace_back(clientId, client->getStatistics());
    }
  }

  return allStats;
}

} // namespace core
} // namespace hydrogen

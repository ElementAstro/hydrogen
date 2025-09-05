#include "hydrogen/core/unified_connection_manager.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>

namespace hydrogen {
namespace core {

// WebSocketConnection implementation
WebSocketConnection::WebSocketConnection(
    std::shared_ptr<WebSocketErrorHandler> errorHandler)
    : errorHandler_(errorHandler) {
  if (!errorHandler_) {
    errorHandler_ = std::make_shared<StandardWebSocketErrorHandler>();
  }
  statistics_.connectionTime = std::chrono::system_clock::now();
  statistics_.lastActivityTime = std::chrono::system_clock::now();
}

WebSocketConnection::~WebSocketConnection() { disconnect(); }

bool WebSocketConnection::connect(const ConnectionConfig &config) {
  if (state_.load() == ConnectionState::CONNECTED) {
    return true;
  }

  state_.store(ConnectionState::CONNECTING);
  config_ = config;

  try {
    bool success = establishWebSocketConnection();
    if (success) {
      state_.store(ConnectionState::CONNECTED);
      {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.state = ConnectionState::CONNECTED;
        statistics_.connectionTime = std::chrono::system_clock::now();
        statistics_.reconnectionAttempts = 0;
      }
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::info("WebSocketConnection: Connected to {}:{}", config_.host,
                   config_.port);
#endif
    } else {
      state_.store(ConnectionState::CONNECTION_ERROR);
      updateStatistics(false, false, 0, true);
    }
    return success;
  } catch (const std::exception &e) {
    state_.store(ConnectionState::CONNECTION_ERROR);
    updateStatistics(false, false, 0, true);
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("WebSocketConnection: Connection failed: {}", e.what());
#endif
    return false;
  }
}

void WebSocketConnection::disconnect() {
  if (state_.load() == ConnectionState::DISCONNECTED) {
    return;
  }

  state_.store(ConnectionState::DISCONNECTED);
  cleanupWebSocketConnection();

  {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.state = ConnectionState::DISCONNECTED;
    auto now = std::chrono::system_clock::now();
    statistics_.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - statistics_.connectionTime);
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("WebSocketConnection: Disconnected from {}:{}", config_.host,
               config_.port);
#endif
}

bool WebSocketConnection::isConnected() const {
  return state_.load() == ConnectionState::CONNECTED &&
         websocketHandle_ != nullptr;
}

ConnectionState WebSocketConnection::getState() const { return state_.load(); }

bool WebSocketConnection::sendMessage(const std::string &data) {
  if (!isConnected()) {
    return false;
  }

  try {
    // TODO: Implement actual WebSocket send
    // For now, simulate successful send
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    updateStatistics(true, false, data.size());
    return true;
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("WebSocketConnection: Send failed: {}", e.what());
#endif
    updateStatistics(false, false, 0, true);
    return false;
  }
}

bool WebSocketConnection::receiveMessage(std::string &data) {
  if (!isConnected()) {
    return false;
  }

  try {
    // TODO: Implement actual WebSocket receive
    // For now, simulate occasional message reception
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dis(1, 1000);

    if (dis(gen) <= 10) { // 1% chance of receiving a message
      json simulatedMessage;
      simulatedMessage["type"] = "heartbeat";
      simulatedMessage["timestamp"] =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
      data = simulatedMessage.dump();
      updateStatistics(false, true, data.size());
      return true;
    }

    return false;
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("WebSocketConnection: Receive failed: {}", e.what());
#endif
    updateStatistics(false, false, 0, true);
    return false;
  }
}

void WebSocketConnection::updateConfig(const ConnectionConfig &config) {
  bool needsReconnect =
      (config.host != config_.host || config.port != config_.port);
  config_ = config;

  if (needsReconnect && isConnected()) {
    disconnect();
    connect(config);
  }
}

ConnectionStatistics WebSocketConnection::getStatistics() const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  ConnectionStatistics stats = statistics_;

  if (stats.state == ConnectionState::CONNECTED) {
    auto now = std::chrono::system_clock::now();
    stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - stats.connectionTime);
  }

  return stats;
}

void WebSocketConnection::resetStatistics() {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  auto currentState = statistics_.state;
  auto connectionTime = statistics_.connectionTime;
  statistics_ = ConnectionStatistics{};
  statistics_.state = currentState;
  statistics_.connectionTime = connectionTime;
  statistics_.lastActivityTime = std::chrono::system_clock::now();
}

bool WebSocketConnection::establishWebSocketConnection() {
  // TODO: Implement actual WebSocket connection establishment
  // For now, simulate successful connection
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  websocketHandle_ = reinterpret_cast<void *>(0x12345678);
  return true;
}

void WebSocketConnection::cleanupWebSocketConnection() {
  // TODO: Implement actual WebSocket cleanup
  websocketHandle_ = nullptr;
}

void WebSocketConnection::updateStatistics(bool sent, bool received,
                                           size_t bytes, bool error) {
  std::lock_guard<std::mutex> lock(statisticsMutex_);

  if (sent)
    statistics_.messagesSent++;
  if (received)
    statistics_.messagesReceived++;
  if (error)
    statistics_.errors++;

  statistics_.bytesTransferred += bytes;

  if (sent || received) {
    statistics_.lastActivityTime = std::chrono::system_clock::now();
  }
}

// HttpConnection implementation
HttpConnection::HttpConnection() {
  statistics_.connectionTime = std::chrono::system_clock::now();
  statistics_.lastActivityTime = std::chrono::system_clock::now();
}

HttpConnection::~HttpConnection() { disconnect(); }

bool HttpConnection::connect(const ConnectionConfig &config) {
  if (state_.load() == ConnectionState::CONNECTED) {
    return true;
  }

  state_.store(ConnectionState::CONNECTING);
  config_ = config;

  try {
    bool success = establishHttpConnection();
    if (success) {
      state_.store(ConnectionState::CONNECTED);
      {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.state = ConnectionState::CONNECTED;
        statistics_.connectionTime = std::chrono::system_clock::now();
        statistics_.reconnectionAttempts = 0;
      }
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::info("HttpConnection: Connected to {}:{}", config_.host,
                   config_.port);
#endif
    } else {
      state_.store(ConnectionState::CONNECTION_ERROR);
      updateStatistics(false, false, 0, true);
    }
    return success;
  } catch (const std::exception &e) {
    state_.store(ConnectionState::CONNECTION_ERROR);
    updateStatistics(false, false, 0, true);
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("HttpConnection: Connection failed: {}", e.what());
#endif
    return false;
  }
}

void HttpConnection::disconnect() {
  if (state_.load() == ConnectionState::DISCONNECTED) {
    return;
  }

  state_.store(ConnectionState::DISCONNECTED);
  cleanupHttpConnection();

  {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.state = ConnectionState::DISCONNECTED;
    auto now = std::chrono::system_clock::now();
    statistics_.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - statistics_.connectionTime);
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("HttpConnection: Disconnected from {}:{}", config_.host,
               config_.port);
#endif
}

bool HttpConnection::isConnected() const {
  return state_.load() == ConnectionState::CONNECTED && httpHandle_ != nullptr;
}

ConnectionState HttpConnection::getState() const { return state_.load(); }

bool HttpConnection::sendMessage(const std::string &data) {
  if (!isConnected()) {
    return false;
  }

  try {
    // TODO: Implement actual HTTP send
    // For now, simulate successful send
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    updateStatistics(true, false, data.size());
    return true;
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("HttpConnection: Send failed: {}", e.what());
#endif
    updateStatistics(false, false, 0, true);
    return false;
  }
}

bool HttpConnection::receiveMessage(std::string &data) {
  if (!isConnected()) {
    return false;
  }

  try {
    // TODO: Implement actual HTTP receive (polling or long-polling)
    // For now, simulate no immediate messages available
    return false;
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("HttpConnection: Receive failed: {}", e.what());
#endif
    updateStatistics(false, false, 0, true);
    return false;
  }
}

void HttpConnection::updateConfig(const ConnectionConfig &config) {
  bool needsReconnect =
      (config.host != config_.host || config.port != config_.port);
  config_ = config;

  if (needsReconnect && isConnected()) {
    disconnect();
    connect(config);
  }
}

ConnectionStatistics HttpConnection::getStatistics() const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  ConnectionStatistics stats = statistics_;

  if (stats.state == ConnectionState::CONNECTED) {
    auto now = std::chrono::system_clock::now();
    stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - stats.connectionTime);
  }

  return stats;
}

void HttpConnection::resetStatistics() {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  auto currentState = statistics_.state;
  auto connectionTime = statistics_.connectionTime;
  statistics_ = ConnectionStatistics{};
  statistics_.state = currentState;
  statistics_.connectionTime = connectionTime;
  statistics_.lastActivityTime = std::chrono::system_clock::now();
}

bool HttpConnection::establishHttpConnection() {
  // TODO: Implement actual HTTP connection establishment
  // For now, simulate successful connection
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  httpHandle_ = reinterpret_cast<void *>(0x87654321);
  return true;
}

void HttpConnection::cleanupHttpConnection() {
  // TODO: Implement actual HTTP cleanup
  httpHandle_ = nullptr;
}

void HttpConnection::updateStatistics(bool sent, bool received, size_t bytes,
                                      bool error) {
  std::lock_guard<std::mutex> lock(statisticsMutex_);

  if (sent)
    statistics_.messagesSent++;
  if (received)
    statistics_.messagesReceived++;
  if (error)
    statistics_.errors++;

  statistics_.bytesTransferred += bytes;

  if (sent || received) {
    statistics_.lastActivityTime = std::chrono::system_clock::now();
  }
}

// GrpcConnection implementation
GrpcConnection::GrpcConnection() {
  statistics_.connectionTime = std::chrono::system_clock::now();
  statistics_.lastActivityTime = std::chrono::system_clock::now();
}

GrpcConnection::~GrpcConnection() { disconnect(); }

bool GrpcConnection::connect(const ConnectionConfig &config) {
  if (state_.load() == ConnectionState::CONNECTED) {
    return true;
  }

  state_.store(ConnectionState::CONNECTING);
  config_ = config;

  try {
    bool success = establishGrpcConnection();
    if (success) {
      state_.store(ConnectionState::CONNECTED);
      {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.state = ConnectionState::CONNECTED;
        statistics_.connectionTime = std::chrono::system_clock::now();
        statistics_.reconnectionAttempts = 0;
      }
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::info("GrpcConnection: Connected to {}:{}", config_.host,
                   config_.port);
#endif
    } else {
      state_.store(ConnectionState::CONNECTION_ERROR);
      updateStatistics(false, false, 0, true);
    }
    return success;
  } catch (const std::exception &e) {
    state_.store(ConnectionState::CONNECTION_ERROR);
    updateStatistics(false, false, 0, true);
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("GrpcConnection: Connection failed: {}", e.what());
#endif
    return false;
  }
}

void GrpcConnection::disconnect() {
  if (state_.load() == ConnectionState::DISCONNECTED) {
    return;
  }

  state_.store(ConnectionState::DISCONNECTED);
  cleanupGrpcConnection();

  {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.state = ConnectionState::DISCONNECTED;
    auto now = std::chrono::system_clock::now();
    statistics_.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - statistics_.connectionTime);
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("GrpcConnection: Disconnected from {}:{}", config_.host,
               config_.port);
#endif
}

bool GrpcConnection::isConnected() const {
  return state_.load() == ConnectionState::CONNECTED && grpcChannel_ != nullptr;
}

ConnectionState GrpcConnection::getState() const { return state_.load(); }

bool GrpcConnection::sendMessage(const std::string &data) {
  if (!isConnected()) {
    return false;
  }

  try {
    // TODO: Implement actual gRPC send
    // For now, simulate successful send
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    updateStatistics(true, false, data.size());
    return true;
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("GrpcConnection: Send failed: {}", e.what());
#endif
    updateStatistics(false, false, 0, true);
    return false;
  }
}

bool GrpcConnection::receiveMessage(std::string &data) {
  if (!isConnected()) {
    return false;
  }

  try {
    // TODO: Implement actual gRPC receive (streaming)
    // For now, simulate occasional message reception
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dis(1, 500);

    if (dis(gen) <= 5) { // 1% chance of receiving a message
      json simulatedMessage;
      simulatedMessage["type"] = "grpc_response";
      simulatedMessage["timestamp"] =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
      data = simulatedMessage.dump();
      updateStatistics(false, true, data.size());
      return true;
    }

    return false;
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("GrpcConnection: Receive failed: {}", e.what());
#endif
    updateStatistics(false, false, 0, true);
    return false;
  }
}

void GrpcConnection::updateConfig(const ConnectionConfig &config) {
  bool needsReconnect =
      (config.host != config_.host || config.port != config_.port);
  config_ = config;

  if (needsReconnect && isConnected()) {
    disconnect();
    connect(config);
  }
}

ConnectionStatistics GrpcConnection::getStatistics() const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  ConnectionStatistics stats = statistics_;

  if (stats.state == ConnectionState::CONNECTED) {
    auto now = std::chrono::system_clock::now();
    stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - stats.connectionTime);
  }

  return stats;
}

void GrpcConnection::resetStatistics() {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  auto currentState = statistics_.state;
  auto connectionTime = statistics_.connectionTime;
  statistics_ = ConnectionStatistics{};
  statistics_.state = currentState;
  statistics_.connectionTime = connectionTime;
  statistics_.lastActivityTime = std::chrono::system_clock::now();
}

bool GrpcConnection::establishGrpcConnection() {
  // TODO: Implement actual gRPC connection establishment
  // For now, simulate successful connection
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  grpcChannel_ = reinterpret_cast<void *>(0xABCDEF12);
  return true;
}

void GrpcConnection::cleanupGrpcConnection() {
  // TODO: Implement actual gRPC cleanup
  grpcChannel_ = nullptr;
}

void GrpcConnection::updateStatistics(bool sent, bool received, size_t bytes,
                                      bool error) {
  std::lock_guard<std::mutex> lock(statisticsMutex_);

  if (sent)
    statistics_.messagesSent++;
  if (received)
    statistics_.messagesReceived++;
  if (error)
    statistics_.errors++;

  statistics_.bytesTransferred += bytes;

  if (sent || received) {
    statistics_.lastActivityTime = std::chrono::system_clock::now();
  }
}

// UnifiedConnectionManager implementation
UnifiedConnectionManager::UnifiedConnectionManager()
    : errorMapper_(std::make_shared<ProtocolErrorMapper>()) {
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("UnifiedConnectionManager: Initialized");
#endif
}

UnifiedConnectionManager::~UnifiedConnectionManager() {
  disconnectAll();
  stopMessageProcessing();
}

std::string
UnifiedConnectionManager::createConnection(const ConnectionConfig &config) {
  std::string connectionId = generateConnectionId();

  auto connectionInfo = std::make_unique<ConnectionInfo>();
  connectionInfo->id = connectionId;
  connectionInfo->config = config;
  connectionInfo->connection = createProtocolConnection(config.protocol);
  connectionInfo->lastReconnectAttempt = std::chrono::system_clock::now();

  // Create protocol converter
  auto &converterRegistry = ConverterRegistry::getInstance();
  connectionInfo->converter.reset(
      converterRegistry.getConverter(config.protocol));

  {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_[connectionId] = std::move(connectionInfo);
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info(
      "UnifiedConnectionManager: Created connection {} for protocol {}",
      connectionId, static_cast<int>(config.protocol));
#endif

  return connectionId;
}

bool UnifiedConnectionManager::connectConnection(
    const std::string &connectionId) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  if (it == connections_.end()) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("UnifiedConnectionManager: Connection {} not found",
                  connectionId);
#endif
    return false;
  }

  bool success = it->second->connection->connect(it->second->config);
  if (success) {
    handleConnectionStateChange(connectionId, ConnectionState::CONNECTED);
  } else {
    handleConnectionStateChange(connectionId,
                                ConnectionState::CONNECTION_ERROR);
  }

  return success;
}

void UnifiedConnectionManager::disconnectConnection(
    const std::string &connectionId) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  if (it != connections_.end()) {
    it->second->connection->disconnect();
    handleConnectionStateChange(connectionId, ConnectionState::DISCONNECTED);
  }
}

void UnifiedConnectionManager::disconnectAll() {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  for (auto &[connectionId, connectionInfo] : connections_) {
    connectionInfo->connection->disconnect();
    handleConnectionStateChange(connectionId, ConnectionState::DISCONNECTED);
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("UnifiedConnectionManager: Disconnected all connections");
#endif
}

bool UnifiedConnectionManager::isConnected(
    const std::string &connectionId) const {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  return (it != connections_.end()) ? it->second->connection->isConnected()
                                    : false;
}

ConnectionState UnifiedConnectionManager::getConnectionState(
    const std::string &connectionId) const {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  return (it != connections_.end()) ? it->second->connection->getState()
                                    : ConnectionState::DISCONNECTED;
}

std::vector<std::string>
UnifiedConnectionManager::getActiveConnections() const {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  std::vector<std::string> activeConnections;
  for (const auto &[connectionId, connectionInfo] : connections_) {
    if (connectionInfo->connection->isConnected()) {
      activeConnections.push_back(connectionId);
    }
  }

  return activeConnections;
}

bool UnifiedConnectionManager::sendMessage(const std::string &connectionId,
                                           std::shared_ptr<Message> message) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  if (it == connections_.end()) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("UnifiedConnectionManager: Connection {} not found",
                  connectionId);
#endif
    return false;
  }

  if (!it->second->connection->isConnected()) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::warn("UnifiedConnectionManager: Connection {} is not connected",
                 connectionId);
#endif
    return false;
  }

  try {
    // Transform message to protocol format
    auto &transformer = getGlobalMessageTransformer();
    auto result = transformer.transform(*message, it->second->config.protocol);

    if (!result.success) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::error("UnifiedConnectionManager: Failed to transform message: {}",
                    result.errorMessage);
#endif
      return false;
    }

    std::string messageData = result.transformedData.dump();
    return it->second->connection->sendMessage(messageData);

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("UnifiedConnectionManager: Failed to send message on {}: {}",
                  connectionId, e.what());
#endif
    return false;
  }
}

bool UnifiedConnectionManager::sendRawMessage(const std::string &connectionId,
                                              const std::string &data) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  if (it == connections_.end()) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("UnifiedConnectionManager: Connection {} not found",
                  connectionId);
#endif
    return false;
  }

  if (!it->second->connection->isConnected()) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::warn("UnifiedConnectionManager: Connection {} is not connected",
                 connectionId);
#endif
    return false;
  }

  return it->second->connection->sendMessage(data);
}

bool UnifiedConnectionManager::broadcastMessage(
    std::shared_ptr<Message> message,
    const std::vector<std::string> &connectionIds) {
  std::vector<std::string> targetConnections;

  if (connectionIds.empty()) {
    // Broadcast to all active connections
    targetConnections = getActiveConnections();
  } else {
    targetConnections = connectionIds;
  }

  bool allSuccessful = true;
  for (const auto &connectionId : targetConnections) {
    if (!sendMessage(connectionId, message)) {
      allSuccessful = false;
    }
  }

  return allSuccessful;
}

void UnifiedConnectionManager::updateConnectionConfig(
    const std::string &connectionId, const ConnectionConfig &config) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  if (it != connections_.end()) {
    it->second->config = config;
    it->second->connection->updateConfig(config);
  }
}

ConnectionConfig UnifiedConnectionManager::getConnectionConfig(
    const std::string &connectionId) const {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  return (it != connections_.end()) ? it->second->config : ConnectionConfig{};
}

void UnifiedConnectionManager::enableAutoReconnect(
    const std::string &connectionId, bool enable) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  if (it != connections_.end()) {
    it->second->autoReconnect.store(enable);
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::debug(
        "UnifiedConnectionManager: Auto-reconnect {} for connection {}",
        enable ? "enabled" : "disabled", connectionId);
#endif
  }
}

void UnifiedConnectionManager::setReconnectSettings(
    const std::string &connectionId, std::chrono::milliseconds interval,
    int maxAttempts) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  if (it != connections_.end()) {
    it->second->config.reconnectInterval = interval;
    it->second->config.maxReconnectAttempts = maxAttempts;
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::debug("UnifiedConnectionManager: Updated reconnect settings for "
                  "{}: interval={}ms, maxAttempts={}",
                  connectionId, interval.count(), maxAttempts);
#endif
  }
}

ConnectionStatistics UnifiedConnectionManager::getConnectionStatistics(
    const std::string &connectionId) const {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  return (it != connections_.end()) ? it->second->connection->getStatistics()
                                    : ConnectionStatistics{};
}

std::unordered_map<std::string, ConnectionStatistics>
UnifiedConnectionManager::getAllStatistics() const {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  std::unordered_map<std::string, ConnectionStatistics> allStats;
  for (const auto &[connectionId, connectionInfo] : connections_) {
    allStats[connectionId] = connectionInfo->connection->getStatistics();
  }

  return allStats;
}

void UnifiedConnectionManager::resetStatistics(
    const std::string &connectionId) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  if (connectionId.empty()) {
    // Reset all statistics
    for (auto &[id, connectionInfo] : connections_) {
      connectionInfo->connection->resetStatistics();
    }
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::debug(
        "UnifiedConnectionManager: Reset statistics for all connections");
#endif
  } else {
    auto it = connections_.find(connectionId);
    if (it != connections_.end()) {
      it->second->connection->resetStatistics();
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::debug(
          "UnifiedConnectionManager: Reset statistics for connection {}",
          connectionId);
#endif
    }
  }
}

std::vector<MessageFormat>
UnifiedConnectionManager::getSupportedProtocols() const {
  return {MessageFormat::HTTP_JSON, MessageFormat::PROTOBUF,
          MessageFormat::MQTT, MessageFormat::ZEROMQ};
}

bool UnifiedConnectionManager::isProtocolSupported(
    MessageFormat protocol) const {
  auto supported = getSupportedProtocols();
  return std::find(supported.begin(), supported.end(), protocol) !=
         supported.end();
}

void UnifiedConnectionManager::startMessageProcessing() {
  if (messageProcessingActive_.load()) {
    return;
  }

  messageProcessingActive_.store(true);
  messageProcessingThread_ =
      std::thread(&UnifiedConnectionManager::messageProcessingLoop, this);
  reconnectionThread_ =
      std::thread(&UnifiedConnectionManager::reconnectionLoop, this);

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("UnifiedConnectionManager: Message processing started");
#endif
}

void UnifiedConnectionManager::stopMessageProcessing() {
  if (!messageProcessingActive_.load()) {
    return;
  }

  messageProcessingActive_.store(false);
  shutdown_.store(true);

  if (messageProcessingThread_.joinable()) {
    messageProcessingThread_.join();
  }
  if (reconnectionThread_.joinable()) {
    reconnectionThread_.join();
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("UnifiedConnectionManager: Message processing stopped");
#endif
}

bool UnifiedConnectionManager::isMessageProcessingActive() const {
  return messageProcessingActive_.load();
}

std::string UnifiedConnectionManager::generateConnectionId() const {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<uint32_t> dis;

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch())
                       .count();

  std::stringstream ss;
  ss << "conn_" << timestamp << "_" << std::hex << dis(gen);
  return ss.str();
}

std::unique_ptr<ProtocolConnection>
UnifiedConnectionManager::createProtocolConnection(MessageFormat protocol) {
  switch (protocol) {
  case MessageFormat::HTTP_JSON:
    return createWebSocketConnection();
  case MessageFormat::PROTOBUF:
    return createGrpcConnection();
  default:
    return createWebSocketConnection(); // Default fallback
  }
}

void UnifiedConnectionManager::messageProcessingLoop() {
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug("UnifiedConnectionManager: Message processing loop started");
#endif

  while (messageProcessingActive_.load() && !shutdown_.load()) {
    try {
      std::vector<std::string> activeConnections = getActiveConnections();

      for (const auto &connectionId : activeConnections) {
        std::string messageData;

        {
          std::lock_guard<std::mutex> lock(connectionsMutex_);
          auto it = connections_.find(connectionId);
          if (it != connections_.end() &&
              it->second->connection->receiveMessage(messageData)) {
            try {
              json message = json::parse(messageData);
              if (messageCallback_) {
                messageCallback_(connectionId, message);
              }
            } catch (const json::parse_error &e) {
#ifdef HYDROGEN_HAS_SPDLOG
              spdlog::error("UnifiedConnectionManager: Failed to parse message "
                            "from {}: {}",
                            connectionId, e.what());
#endif
            }
          }
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(10));

    } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::error(
          "UnifiedConnectionManager: Error in message processing loop: {}",
          e.what());
#endif
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug("UnifiedConnectionManager: Message processing loop stopped");
#endif
}

void UnifiedConnectionManager::reconnectionLoop() {
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug("UnifiedConnectionManager: Reconnection loop started");
#endif

  while (!shutdown_.load()) {
    try {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));

      std::vector<std::string> connectionsToCheck;
      {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (const auto &[connectionId, connectionInfo] : connections_) {
          if (!connectionInfo->connection->isConnected() &&
              connectionInfo->autoReconnect.load()) {
            connectionsToCheck.push_back(connectionId);
          }
        }
      }

      for (const auto &connectionId : connectionsToCheck) {
        attemptReconnection(connectionId);
      }

    } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::error("UnifiedConnectionManager: Error in reconnection loop: {}",
                    e.what());
#endif
      std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug("UnifiedConnectionManager: Reconnection loop stopped");
#endif
}

void UnifiedConnectionManager::handleConnectionStateChange(
    const std::string &connectionId, ConnectionState newState) {
  if (connectionCallback_) {
    try {
      connectionCallback_(connectionId, newState);
    } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::error(
          "UnifiedConnectionManager: Error in connection callback: {}",
          e.what());
#endif
    }
  }
}

void UnifiedConnectionManager::attemptReconnection(
    const std::string &connectionId) {
  std::lock_guard<std::mutex> lock(connectionsMutex_);

  auto it = connections_.find(connectionId);
  if (it == connections_.end()) {
    return;
  }

  auto &connectionInfo = it->second;

  // Check if enough time has passed since last attempt
  auto now = std::chrono::system_clock::now();
  auto timeSinceLastAttempt =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now - connectionInfo->lastReconnectAttempt);

  if (timeSinceLastAttempt < connectionInfo->config.reconnectInterval) {
    return;
  }

  // Check if max attempts exceeded
  int attempts = connectionInfo->reconnectAttempts.load();
  if (connectionInfo->config.maxReconnectAttempts > 0 &&
      attempts >= connectionInfo->config.maxReconnectAttempts) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::warn(
        "UnifiedConnectionManager: Max reconnect attempts exceeded for {}",
        connectionId);
#endif
    return;
  }

  connectionInfo->lastReconnectAttempt = now;
  connectionInfo->reconnectAttempts.fetch_add(1);

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info(
      "UnifiedConnectionManager: Attempting reconnection for {} (attempt {})",
      connectionId, attempts + 1);
#endif

  bool success = connectionInfo->connection->connect(connectionInfo->config);
  if (success) {
    connectionInfo->reconnectAttempts.store(0);
    handleConnectionStateChange(connectionId, ConnectionState::CONNECTED);
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("UnifiedConnectionManager: Reconnection successful for {}",
                 connectionId);
#endif
  } else {
    handleConnectionStateChange(connectionId,
                                ConnectionState::CONNECTION_ERROR);
  }
}

std::unique_ptr<WebSocketConnection>
UnifiedConnectionManager::createWebSocketConnection() {
  return std::make_unique<WebSocketConnection>();
}

std::unique_ptr<HttpConnection>
UnifiedConnectionManager::createHttpConnection() {
  return std::make_unique<HttpConnection>();
}

std::unique_ptr<GrpcConnection>
UnifiedConnectionManager::createGrpcConnection() {
  return std::make_unique<GrpcConnection>();
}

// ConnectionManagerFactory implementation
std::unique_ptr<UnifiedConnectionManager>
ConnectionManagerFactory::createManager() {
  return std::make_unique<UnifiedConnectionManager>();
}

std::unique_ptr<UnifiedConnectionManager>
ConnectionManagerFactory::createManagerWithDefaults() {
  auto manager = createManager();
  manager->startMessageProcessing();
  return manager;
}

ConnectionConfig
ConnectionManagerFactory::createWebSocketConfig(const std::string &host,
                                                uint16_t port) {
  ConnectionConfig config;
  config.protocol = MessageFormat::HTTP_JSON;
  config.host = host;
  config.port = port;
  config.endpoint = "/ws";
  config.useTls = false;
  return config;
}

ConnectionConfig
ConnectionManagerFactory::createHttpConfig(const std::string &host,
                                           uint16_t port) {
  ConnectionConfig config;
  config.protocol = MessageFormat::HTTP_JSON;
  config.host = host;
  config.port = port;
  config.endpoint = "/api";
  config.useTls = false;
  return config;
}

ConnectionConfig
ConnectionManagerFactory::createGrpcConfig(const std::string &host,
                                           uint16_t port) {
  ConnectionConfig config;
  config.protocol = MessageFormat::PROTOBUF;
  config.host = host;
  config.port = port;
  config.endpoint = "";
  config.useTls = false;
  return config;
}

ConnectionConfig
ConnectionManagerFactory::createMqttConfig(const std::string &host,
                                           uint16_t port) {
  ConnectionConfig config;
  config.protocol = MessageFormat::MQTT;
  config.host = host;
  config.port = port;
  config.endpoint = "";
  config.useTls = false;
  return config;
}

ConnectionConfig ConnectionManagerFactory::getSecureConfig(
    MessageFormat protocol, const std::string &host, uint16_t port) {
  ConnectionConfig config;
  config.protocol = protocol;
  config.host = host;
  config.port = port;
  config.useTls = true;

  switch (protocol) {
  case MessageFormat::HTTP_JSON:
    config.endpoint = "/ws";
    break;
  case MessageFormat::PROTOBUF:
    config.endpoint = "";
    break;
  default:
    config.endpoint = "/ws";
    break;
  }

  return config;
}

ConnectionConfig ConnectionManagerFactory::getHighPerformanceConfig(
    MessageFormat protocol, const std::string &host, uint16_t port) {
  ConnectionConfig config;
  config.protocol = protocol;
  config.host = host;
  config.port = port;
  config.connectTimeout = std::chrono::milliseconds{2000};
  config.messageTimeout = std::chrono::milliseconds{1000};
  config.heartbeatInterval = std::chrono::milliseconds{10000};
  config.reconnectInterval = std::chrono::milliseconds{1000};

  return config;
}

ConnectionConfig ConnectionManagerFactory::getReliableConfig(
    MessageFormat protocol, const std::string &host, uint16_t port) {
  ConnectionConfig config;
  config.protocol = protocol;
  config.host = host;
  config.port = port;
  config.enableAutoReconnect = true;
  config.reconnectInterval = std::chrono::milliseconds{5000};
  config.maxReconnectAttempts = 0; // Unlimited
  config.heartbeatInterval = std::chrono::milliseconds{30000};

  return config;
}

} // namespace core
} // namespace hydrogen

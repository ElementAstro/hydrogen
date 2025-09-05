#pragma once

#include "message.h"
#include "message_transformer.h"
#include "protocol_error_mapper.h"
#include "websocket_error_handler.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Connection configuration for the unified device client
 */
struct ClientConnectionConfig {
  std::string host = "localhost";
  uint16_t port = 8080;
  std::string endpoint = "/ws";
  bool useTls = false;
  std::chrono::milliseconds connectTimeout{5000};
  std::chrono::milliseconds messageTimeout{5000};
  std::chrono::milliseconds heartbeatInterval{30000};
  bool enableAutoReconnect = true;
  std::chrono::milliseconds reconnectInterval{5000};
  int maxReconnectAttempts = 0; // 0 = unlimited
};

/**
 * @brief Client statistics and status information
 */
struct ClientStatistics {
  bool isConnected = false;
  std::chrono::system_clock::time_point connectionTime;
  std::chrono::system_clock::time_point lastMessageTime;
  size_t messagesSent = 0;
  size_t messagesReceived = 0;
  size_t reconnectionAttempts = 0;
  size_t errors = 0;
  std::string lastError;
};

/**
 * @brief Unified Device Client
 *
 * This class consolidates the functionality of both DeviceClient and
 * DeviceClientRefactored into a single, robust implementation that supports
 * multiple protocols and provides comprehensive error handling, reconnection,
 * and message processing capabilities.
 */
class UnifiedDeviceClient {
public:
  // Callback types
  using ConnectionCallback = std::function<void(bool connected)>;
  using MessageCallback = std::function<void(const json &message)>;
  using PropertyChangeCallback =
      std::function<void(const std::string &deviceId,
                         const std::string &property, const json &value)>;
  using EventCallback =
      std::function<void(const std::string &deviceId,
                         const std::string &eventType, const json &data)>;
  using ErrorCallback = std::function<void(const std::string &error)>;

  explicit UnifiedDeviceClient(
      const ClientConnectionConfig &config = ClientConnectionConfig{});
  ~UnifiedDeviceClient();

  // Connection Management
  bool connect();
  bool connect(const std::string &host, uint16_t port);
  void disconnect();
  bool isConnected() const;

  // Configuration
  void updateConfig(const ClientConnectionConfig &config);
  const ClientConnectionConfig &getConfig() const { return config_; }

  // Auto-reconnection
  void setAutoReconnect(
      bool enable,
      std::chrono::milliseconds interval = std::chrono::milliseconds{5000},
      int maxAttempts = 0);

  // Device Discovery and Management
  json discoverDevices(const std::vector<std::string> &deviceTypes = {});
  json getDevices() const;
  json getDeviceInfo(const std::string &deviceId) const;
  json getDeviceProperties(const std::string &deviceId,
                           const std::vector<std::string> &properties = {});
  json setDeviceProperties(const std::string &deviceId, const json &properties);

  // Command Execution
  json
  executeCommand(const std::string &deviceId, const std::string &command,
                 const json &parameters = json::object(),
                 Message::QoSLevel qosLevel = Message::QoSLevel::AT_LEAST_ONCE);

  void executeCommandAsync(
      const std::string &deviceId, const std::string &command,
      const json &parameters = json::object(),
      Message::QoSLevel qosLevel = Message::QoSLevel::AT_LEAST_ONCE,
      std::function<void(const json &)> callback = nullptr);

  json executeBatchCommands(
      const std::string &deviceId,
      const std::vector<std::pair<std::string, json>> &commands,
      bool sequential = true,
      Message::QoSLevel qosLevel = Message::QoSLevel::AT_LEAST_ONCE);

  // Message Operations
  json sendMessage(
      std::shared_ptr<Message> message,
      std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});
  void sendMessageAsync(std::shared_ptr<Message> message,
                        std::function<void(const json &)> callback = nullptr);

  // Subscription Management
  bool subscribeToProperty(const std::string &deviceId,
                           const std::string &property);
  bool unsubscribeFromProperty(const std::string &deviceId,
                               const std::string &property);
  bool subscribeToEvents(const std::string &deviceId,
                         const std::string &eventType = "");
  bool unsubscribeFromEvents(const std::string &deviceId,
                             const std::string &eventType = "");

  // Callback Registration
  void setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = callback;
  }
  void setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
  }
  void setPropertyChangeCallback(PropertyChangeCallback callback) {
    propertyChangeCallback_ = callback;
  }
  void setEventCallback(EventCallback callback) { eventCallback_ = callback; }
  void setErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }

  // Message Processing Control
  void startMessageProcessing();
  void stopMessageProcessing();
  bool isMessageProcessingActive() const;

  // Statistics and Monitoring
  ClientStatistics getStatistics() const;
  void resetStatistics();

  // Protocol Support
  void setProtocol(MessageFormat protocol) { protocol_ = protocol; }
  MessageFormat getProtocol() const { return protocol_; }

  // Error Handling
  void setErrorHandler(std::shared_ptr<WebSocketErrorHandler> handler) {
    errorHandler_ = handler;
  }

private:
  ClientConnectionConfig config_;
  MessageFormat protocol_ = MessageFormat::HTTP_JSON;

  // Connection state
  std::atomic<bool> connected_{false};
  std::atomic<bool> connecting_{false};
  std::atomic<bool> shutdown_{false};
  std::atomic<bool> messageProcessingActive_{false};

  // Threading
  std::thread messageProcessingThread_;
  std::thread heartbeatThread_;
  std::thread reconnectionThread_;

  // WebSocket connection (placeholder - would use actual WebSocket library)
  void *websocketConnection_{nullptr};

  // Message handling
  std::mutex pendingResponsesMutex_;
  std::unordered_map<std::string, std::condition_variable *> responseWaiters_;
  std::unordered_map<std::string, json> pendingResponses_;
  std::unordered_map<std::string, std::function<void(const json &)>>
      asyncCallbacks_;

  // Device cache
  mutable std::mutex deviceCacheMutex_;
  json deviceCache_;
  std::chrono::system_clock::time_point lastDeviceUpdate_;

  // Subscriptions
  std::mutex subscriptionsMutex_;
  std::unordered_map<std::string, std::vector<std::string>>
      propertySubscriptions_;
  std::unordered_map<std::string, std::vector<std::string>> eventSubscriptions_;

  // Statistics
  mutable std::mutex statisticsMutex_;
  ClientStatistics statistics_;

  // Reconnection state
  std::atomic<int> reconnectionAttempts_{0};
  std::chrono::system_clock::time_point lastReconnectionAttempt_;

  // Callbacks
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  PropertyChangeCallback propertyChangeCallback_;
  EventCallback eventCallback_;
  ErrorCallback errorCallback_;

  // Error handling
  std::shared_ptr<WebSocketErrorHandler> errorHandler_;
  std::shared_ptr<ProtocolErrorMapper> errorMapper_;

  // Internal methods
  bool establishConnection();
  void cleanupConnection();
  bool attemptReconnection();

  // Message processing
  void messageProcessingLoop();
  void heartbeatLoop();
  void reconnectionLoop();

  // Message handling
  void handleIncomingMessage(const json &message);
  void handleResponse(const json &response);
  void handleEvent(const json &event);
  void handleError(const json &error);
  void handlePropertyChange(const json &propertyChange);

  // Utility methods
  std::string generateMessageId() const;
  void updateStatistics(bool sent, bool received, bool error);
  void notifyConnectionChange(bool connected);
  void notifyError(const std::string &error);

  // WebSocket operations (placeholders)
  bool websocketConnect();
  void websocketDisconnect();
  bool websocketSend(const std::string &data);
  bool websocketReceive(std::string &data);
  bool websocketIsConnected() const;
};

/**
 * @brief Factory for creating unified device clients
 */
class UnifiedDeviceClientFactory {
public:
  static std::unique_ptr<UnifiedDeviceClient>
  createClient(const ClientConnectionConfig &config = ClientConnectionConfig{});
  static std::unique_ptr<UnifiedDeviceClient>
  createAndConnect(const std::string &host, uint16_t port);

  // Protocol-specific clients
  static std::unique_ptr<UnifiedDeviceClient>
  createWebSocketClient(const std::string &host, uint16_t port);
  static std::unique_ptr<UnifiedDeviceClient>
  createHttpClient(const std::string &host, uint16_t port);

  // Configuration presets
  static ClientConnectionConfig getDefaultConfig();
  static ClientConnectionConfig getSecureConfig(const std::string &host,
                                                uint16_t port);
  static ClientConnectionConfig
  getHighPerformanceConfig(const std::string &host, uint16_t port);
};

} // namespace core
} // namespace hydrogen

#pragma once

#include "message.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <thread>
#include <unordered_map>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief ZeroMQ message types matching server implementation
 */
enum class ZmqMessageType {
  DATA = 0,
  CONTROL = 1,
  HEARTBEAT = 2,
  BROADCAST = 3
};

/**
 * @brief ZeroMQ socket types
 */
enum class ZmqSocketType {
  REQ = 0,    // Request
  REP = 1,    // Reply
  DEALER = 2, // Dealer
  ROUTER = 3, // Router
  PUB = 4,    // Publisher
  SUB = 5,    // Subscriber
  PUSH = 6,   // Push
  PULL = 7    // Pull
};

/**
 * @brief ZeroMQ message structure matching server
 */
struct ZmqMessage {
  std::string id;
  std::string content;
  std::string clientId;
  std::chrono::system_clock::time_point timestamp;
  ZmqMessageType type;
  json metadata;

  // Serialization
  json toJson() const;
  static ZmqMessage fromJson(const json &j);
};

/**
 * @brief ZeroMQ client configuration
 */
struct ZmqClientConfig {
  std::string serverAddress = "tcp://localhost:5555";
  ZmqSocketType socketType = ZmqSocketType::REQ;
  int highWaterMark = 1000;
  int lingerTime = 1000;
  int receiveTimeout = 5000;
  int sendTimeout = 5000;
  int reconnectInterval = 1000;
  int maxReconnectAttempts = 10;
  bool enableHeartbeat = true;
  int heartbeatInterval = 30000;
};

/**
 * @brief ZeroMQ client statistics
 */
struct ZmqClientStatistics {
  size_t totalMessagesSent = 0;
  size_t totalMessagesReceived = 0;
  size_t totalMessagesQueued = 0;
  size_t totalMessagesProcessed = 0;
  size_t connectionAttempts = 0;
  size_t reconnectionAttempts = 0;
  std::chrono::steady_clock::time_point lastMessageTime;
  std::chrono::steady_clock::time_point connectionTime;
  bool isConnected = false;
};

/**
 * @brief ZeroMQ queue-based client implementation
 *
 * This client matches the server's queue processing capabilities with
 * proper message queuing, threading, and error handling.
 */
class ZmqQueueClient {
public:
  using MessageHandler = std::function<void(const ZmqMessage &)>;
  using ErrorHandler = std::function<void(const std::string &)>;
  using ConnectionHandler = std::function<void(bool connected)>;

  explicit ZmqQueueClient(const ZmqClientConfig &config);
  ~ZmqQueueClient();

  // Connection management
  bool connect();
  void disconnect();
  bool isConnected() const;
  bool reconnect();

  // Message operations
  bool sendMessage(const std::string &content,
                   ZmqMessageType type = ZmqMessageType::DATA);
  bool sendMessage(const ZmqMessage &message);
  bool sendMessageSync(const std::string &content, std::string &response,
                       int timeoutMs = 5000);

  // Queue operations
  void enqueueMessage(const ZmqMessage &message);
  size_t getQueueSize() const;
  size_t getOutgoingQueueSize() const;
  size_t getIncomingQueueSize() const;
  bool isQueueFull() const;
  void clearQueue();
  void flushOutgoingQueue();

  // Event handlers
  void setMessageHandler(MessageHandler handler) { messageHandler_ = handler; }
  void setErrorHandler(ErrorHandler handler) { errorHandler_ = handler; }
  void setConnectionHandler(ConnectionHandler handler) {
    connectionHandler_ = handler;
  }

  // Configuration and statistics
  const ZmqClientConfig &getConfig() const { return config_; }
  ZmqClientStatistics getStatistics() const;
  void resetStatistics();

  // Socket options
  bool setSocketOption(int option, int value);
  bool getSocketOption(int option, int &value) const;

private:
  ZmqClientConfig config_;
  std::atomic<bool> connected_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> shutdown_{false};

  // ZeroMQ context and socket (simulated for now)
  void *context_{nullptr};
  void *socket_{nullptr};

  // Threading
  std::thread messageProcessorThread_;
  std::thread receiverThread_;
  std::thread heartbeatThread_;

  // Message queues
  std::queue<ZmqMessage> outgoingQueue_;
  std::queue<ZmqMessage> incomingQueue_;
  mutable std::mutex outgoingQueueMutex_;
  mutable std::mutex incomingQueueMutex_;
  std::condition_variable outgoingQueueCV_;
  std::condition_variable incomingQueueCV_;

  // Synchronous message handling
  std::unordered_map<std::string, std::string> pendingResponses_;
  std::unordered_map<std::string, std::condition_variable *> responseWaiters_;
  mutable std::mutex responsesMutex_;

  // Event handlers
  MessageHandler messageHandler_;
  ErrorHandler errorHandler_;
  ConnectionHandler connectionHandler_;

  // Statistics
  mutable std::mutex statisticsMutex_;
  ZmqClientStatistics statistics_;

  // Reconnection state
  std::atomic<int> reconnectAttempts_{0};
  std::chrono::steady_clock::time_point lastReconnectAttempt_;
  std::chrono::steady_clock::time_point lastHeartbeatTime_;

  // Internal methods
  bool initializeSocket();
  void cleanupSocket();

  // Threading methods
  void messageProcessorLoop();
  void receiverLoop();
  void heartbeatLoop();

  // Message processing
  bool sendQueuedMessage(const ZmqMessage &message);
  void processIncomingMessage(const ZmqMessage &message);
  void handleSynchronousResponse(const ZmqMessage &message);
  void handleHeartbeatMessage(const ZmqMessage &message);
  void handleControlMessage(const ZmqMessage &message);
  void handleBroadcastMessage(const ZmqMessage &message);

  // Utility methods
  std::string generateMessageId() const;
  ZmqMessage createHeartbeatMessage() const;
  void updateStatistics(bool sent, bool received, bool queued, bool processed);
  void handleError(const std::string &error);
  void updateConnectionStatus(bool connected);

  // Socket simulation methods (replace with actual ZeroMQ calls)
  bool simulateSend(const std::string &data);
  bool simulateReceive(std::string &data);
};

/**
 * @brief Factory for creating ZeroMQ queue clients
 */
class ZmqQueueClientFactory {
public:
  static std::unique_ptr<ZmqQueueClient>
  createClient(const ZmqClientConfig &config);
  static std::unique_ptr<ZmqQueueClient>
  createReqClient(const std::string &serverAddress);
  static std::unique_ptr<ZmqQueueClient>
  createSubClient(const std::string &serverAddress,
                  const std::string &topic = "");
  static std::unique_ptr<ZmqQueueClient>
  createPullClient(const std::string &serverAddress);

  // Utility methods
  static bool isZmqAvailable();
  static std::string getZmqVersion();
  static ZmqClientConfig getDefaultConfig(ZmqSocketType socketType);
};

/**
 * @brief ZeroMQ message queue manager
 *
 * Provides centralized queue management for multiple ZeroMQ clients
 */
class ZmqQueueManager {
public:
  static ZmqQueueManager &getInstance();

  // Client management
  void registerClient(const std::string &clientId,
                      std::shared_ptr<ZmqQueueClient> client);
  void unregisterClient(const std::string &clientId);
  std::shared_ptr<ZmqQueueClient> getClient(const std::string &clientId);

  // Global queue operations
  bool broadcastMessage(const ZmqMessage &message);
  size_t getTotalQueueSize() const;
  void clearAllQueues();

  // Statistics
  ZmqClientStatistics getAggregatedStatistics() const;
  std::vector<std::pair<std::string, ZmqClientStatistics>>
  getAllClientStatistics() const;

private:
  ZmqQueueManager() = default;
  mutable std::mutex clientsMutex_;
  std::unordered_map<std::string, std::shared_ptr<ZmqQueueClient>> clients_;
};

} // namespace core
} // namespace hydrogen

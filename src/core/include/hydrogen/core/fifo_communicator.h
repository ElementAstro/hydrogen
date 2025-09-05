#pragma once

#include "fifo_config_manager.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief FIFO connection state enumeration
 */
enum class FifoConnectionState {
  DISCONNECTED, // Not connected
  CONNECTING,   // Connection in progress
  CONNECTED,    // Successfully connected
  RECONNECTING, // Attempting to reconnect
  ERROR,        // Connection error
  CLOSING       // Connection being closed
};

/**
 * @brief FIFO communication statistics
 */
struct FifoStatistics {
  std::atomic<uint64_t> messagesSent{0};
  std::atomic<uint64_t> messagesReceived{0};
  std::atomic<uint64_t> bytesTransferred{0};
  std::atomic<uint64_t> connectionAttempts{0};
  std::atomic<uint64_t> reconnectionAttempts{0};
  std::atomic<uint64_t> errors{0};
  std::chrono::system_clock::time_point startTime;
  std::chrono::system_clock::time_point lastActivity;

  FifoStatistics()
      : startTime(std::chrono::system_clock::now()),
        lastActivity(std::chrono::system_clock::now()) {}

  double getMessagesPerSecond() const;
  double getBytesPerSecond() const;
  std::chrono::milliseconds getUptime() const;
  json toJson() const;
};

/**
 * @brief Abstract base class for FIFO communication
 */
class FifoCommunicator {
public:
  using MessageHandler = std::function<void(const std::string &message)>;
  using ErrorHandler = std::function<void(const std::string &error)>;
  using ConnectionHandler = std::function<void(bool connected)>;

  FifoCommunicator(const FifoConfig &config);
  virtual ~FifoCommunicator();

  // Communication lifecycle
  virtual bool start() = 0;
  virtual void stop() = 0;
  virtual bool isActive() const = 0;
  virtual bool isConnected() const = 0;

  // Message communication
  virtual bool sendMessage(const std::string &message) = 0;
  virtual bool sendMessage(const json &message) = 0;
  virtual std::string readMessage() = 0; // Blocking read
  virtual bool hasMessage() const = 0;   // Non-blocking check

  // Connection management
  virtual bool connect() = 0;
  virtual void disconnect() = 0;
  virtual bool reconnect() = 0;
  virtual FifoConnectionState getConnectionState() const = 0;

  // Event handlers
  void setMessageHandler(MessageHandler handler) { messageHandler_ = handler; }
  void setErrorHandler(ErrorHandler handler) { errorHandler_ = handler; }
  void setConnectionHandler(ConnectionHandler handler) {
    connectionHandler_ = handler;
  }

  // Configuration
  const FifoConfig &getConfig() const { return config_; }
  void updateConfig(const FifoConfig &config) { config_ = config; }

  // Statistics and monitoring
  virtual FifoStatistics getStatistics() const = 0;
  virtual bool isHealthy() const = 0;
  virtual std::string getHealthStatus() const = 0;

  // Advanced features
  virtual bool enableBidirectional() = 0;
  virtual bool enableMultiplexing() = 0;
  virtual std::vector<std::string> getConnectedClients() const = 0;

protected:
  FifoConfig config_;
  MessageHandler messageHandler_;
  ErrorHandler errorHandler_;
  ConnectionHandler connectionHandler_;
  std::atomic<bool> active_{false};
  std::atomic<bool> running_{false};
  mutable std::mutex configMutex_;

  // Helper methods
  virtual void handleError(const std::string &error);
  virtual void handleConnection(bool connected);
  virtual void processMessage(const std::string &message);
  virtual std::string formatMessage(const std::string &message) const;
  virtual std::string parseMessage(const std::string &rawMessage) const;
};

/**
 * @brief Cross-platform FIFO communicator implementation
 */
class FifoCommunicatorImpl : public FifoCommunicator {
public:
  explicit FifoCommunicatorImpl(const FifoConfig &config);
  ~FifoCommunicatorImpl() override;

  // FifoCommunicator implementation
  bool start() override;
  void stop() override;
  bool isActive() const override;
  bool isConnected() const override;

  bool sendMessage(const std::string &message) override;
  bool sendMessage(const json &message) override;
  std::string readMessage() override;
  bool hasMessage() const override;

  bool connect() override;
  void disconnect() override;
  bool reconnect() override;
  FifoConnectionState getConnectionState() const override;

  FifoStatistics getStatistics() const override;
  bool isHealthy() const override;
  std::string getHealthStatus() const override;

  bool enableBidirectional() override;
  bool enableMultiplexing() override;
  std::vector<std::string> getConnectedClients() const override;

private:
  // Platform-specific handles
#ifdef _WIN32
  HANDLE readHandle_ = INVALID_HANDLE_VALUE;
  HANDLE writeHandle_ = INVALID_HANDLE_VALUE;
  OVERLAPPED readOverlapped_ = {};
  OVERLAPPED writeOverlapped_ = {};
#else
  int readFd_ = -1;
  int writeFd_ = -1;
#endif

  // Connection state
  std::atomic<FifoConnectionState> connectionState_{
      FifoConnectionState::DISCONNECTED};
  std::atomic<int> reconnectAttempts_{0};
  std::chrono::system_clock::time_point lastReconnectAttempt_;

  // Threading
  std::unique_ptr<std::thread> readerThread_;
  std::unique_ptr<std::thread> writerThread_;
  std::unique_ptr<std::thread> reconnectThread_;

  // Message queues
  std::queue<std::string> incomingMessages_;
  std::queue<std::string> outgoingMessages_;
  mutable std::mutex incomingMutex_;
  mutable std::mutex outgoingMutex_;
  std::condition_variable incomingCondition_;
  std::condition_variable outgoingCondition_;

  // Statistics
  mutable FifoStatistics statistics_;
  mutable std::mutex statisticsMutex_;

  // Circuit breaker
  std::atomic<bool> circuitBreakerOpen_{false};
  std::atomic<int> consecutiveErrors_{0};
  std::chrono::system_clock::time_point circuitBreakerOpenTime_;

  // Platform-specific methods
  bool createPipe();
  bool openPipe();
  void closePipe();
  bool isPipeValid() const;

#ifdef _WIN32
  bool createWindowsNamedPipe();
  bool openWindowsNamedPipe();
  void closeWindowsNamedPipe();
  bool readFromWindowsPipe(std::string &message);
  bool writeToWindowsPipe(const std::string &message);
#else
  bool createUnixFifo();
  bool openUnixFifo();
  void closeUnixFifo();
  bool readFromUnixFifo(std::string &message);
  bool writeToUnixFifo(const std::string &message);
#endif

  // Threading methods
  void readerThreadFunction();
  void writerThreadFunction();
  void reconnectThreadFunction();

  // Message processing
  void processIncomingMessage(const std::string &message);
  void queueOutgoingMessage(const std::string &message);
  std::string dequeueIncomingMessage();
  std::string dequeueOutgoingMessage();

  // Connection management
  bool attemptConnection();
  bool shouldReconnect() const;
  void resetCircuitBreaker();
  void openCircuitBreaker();
  bool isCircuitBreakerOpen() const;

  // Message framing
  std::string frameMessage(const std::string &message) const;
  std::vector<std::string> parseFramedMessages(const std::string &data) const;

  // Error handling
  void handleConnectionError(const std::string &error);
  void handleReadError(const std::string &error);
  void handleWriteError(const std::string &error);

  // Statistics updates
  void updateStatistics(bool sent, size_t bytes);
  void incrementErrorCount();
  void updateLastActivity();

  // Health checking
  bool performHealthCheck() const;
  std::string generateHealthReport() const;

  // Cleanup
  void cleanup();
  void stopThreads();
  void clearQueues();
};

/**
 * @brief Factory class for creating FIFO communicators
 */
class FifoCommunicatorFactory {
public:
  static std::unique_ptr<FifoCommunicator> create(const FifoConfig &config);
  static std::unique_ptr<FifoCommunicator> createDefault();
  static std::unique_ptr<FifoCommunicator>
  createWithPreset(FifoConfigManager::ConfigPreset preset);

  // Platform-specific factories
  static std::unique_ptr<FifoCommunicator>
  createForWindows(const FifoConfig &config);
  static std::unique_ptr<FifoCommunicator>
  createForUnix(const FifoConfig &config);

  // Specialized factories
  static std::unique_ptr<FifoCommunicator>
  createBidirectional(const FifoConfig &config);
  static std::unique_ptr<FifoCommunicator>
  createHighPerformance(const FifoConfig &config);
  static std::unique_ptr<FifoCommunicator>
  createReliable(const FifoConfig &config);
};

/**
 * @brief FIFO communicator utilities
 */
class FifoUtils {
public:
  // Platform detection
  static bool isWindowsPlatform();
  static bool isUnixPlatform();
  static std::string getPlatformName();

  // Path utilities
  static std::string normalizePipePath(const std::string &path);
  static bool isValidPipePath(const std::string &path);
  static std::string
  generateUniquePipeName(const std::string &baseName = "hydrogen_fifo");

  // Permission utilities
  static bool setPipePermissions(const std::string &path, mode_t permissions);
  static bool checkPipePermissions(const std::string &path);

  // Pipe management
  static bool pipeExists(const std::string &path);
  static bool removePipe(const std::string &path);
  static std::vector<std::string> listPipes(const std::string &directory = "");

  // Message utilities
  static std::string escapeMessage(const std::string &message);
  static std::string unescapeMessage(const std::string &message);
  static bool isValidJsonMessage(const std::string &message);

  // Performance utilities
  static size_t getOptimalBufferSize();
  static std::chrono::milliseconds getOptimalTimeout();
  static int getOptimalConcurrency();
};

} // namespace core
} // namespace hydrogen

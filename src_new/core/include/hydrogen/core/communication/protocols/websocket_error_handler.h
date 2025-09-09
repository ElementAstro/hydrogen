#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#ifdef HYDROGEN_HAS_WEBSOCKETS
#include <boost/beast/websocket.hpp>
#include <boost/system/error_code.hpp>
#endif
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif

namespace hydrogen {
namespace core {

#ifdef HYDROGEN_HAS_WEBSOCKETS
namespace beast = boost::beast;
namespace websocket = beast::websocket;
#endif

// Forward declarations for when WebSocket support is disabled
#ifndef HYDROGEN_HAS_WEBSOCKETS
class WebSocketErrorHandler {
public:
  virtual ~WebSocketErrorHandler() = default;
};

class StandardWebSocketErrorHandler : public WebSocketErrorHandler {
public:
  virtual ~StandardWebSocketErrorHandler() = default;
};
#endif

#ifdef HYDROGEN_HAS_WEBSOCKETS
/**
 * @brief WebSocket error categories for standardized handling
 */
enum class WebSocketErrorCategory {
  CONNECTION,     // Connection establishment/loss errors
  PROTOCOL,       // WebSocket protocol errors
  TIMEOUT,        // Timeout-related errors
  AUTHENTICATION, // Authentication/authorization errors
  MESSAGE,        // Message parsing/handling errors
  RESOURCE,       // Resource exhaustion errors
  NETWORK,        // Network-level errors
  UNKNOWN         // Unclassified errors
};

/**
 * @brief WebSocket error severity levels
 */
enum class WebSocketErrorSeverity {
  LOW,     // Minor issues, continue operation
  MEDIUM,  // Moderate issues, may affect functionality
  HIGH,    // Serious issues, requires attention
  CRITICAL // Critical issues, immediate action required
};

/**
 * @brief WebSocket error recovery actions
 */
enum class WebSocketRecoveryAction {
  NONE,      // No action required
  RETRY,     // Retry the operation
  RECONNECT, // Reconnect the WebSocket
  RESET,     // Reset the connection state
  ESCALATE,  // Escalate to higher-level handler
  TERMINATE  // Terminate the connection
};

/**
 * @brief Standardized WebSocket error information
 */
struct WebSocketError {
  std::string errorId;
  std::string errorCode;
  std::string message;
  std::string details;
  WebSocketErrorCategory category;
  WebSocketErrorSeverity severity;
  WebSocketRecoveryAction recommendedAction;
  std::chrono::system_clock::time_point timestamp;
  std::string component;
  std::string operation;
  boost::system::error_code systemErrorCode;
  std::unordered_map<std::string, std::string> context;

  // Utility methods
  std::string toString() const;
  bool isRecoverable() const;
  bool requiresReconnection() const;
};

/**
 * @brief WebSocket error handler interface
 */
class WebSocketErrorHandler {
public:
  virtual ~WebSocketErrorHandler() = default;

  // Error handling methods
  virtual void handleError(const WebSocketError &error) = 0;
  virtual WebSocketRecoveryAction
  determineRecoveryAction(const WebSocketError &error) = 0;
  virtual bool shouldRetry(const WebSocketError &error, int attemptCount) = 0;
  virtual std::chrono::milliseconds getRetryDelay(const WebSocketError &error,
                                                  int attemptCount) = 0;
};

/**
 * @brief Standard WebSocket error handler implementation
 */
class StandardWebSocketErrorHandler : public WebSocketErrorHandler {
public:
  using ErrorCallback = std::function<void(const WebSocketError &)>;
  using RecoveryCallback =
      std::function<bool(const WebSocketError &, WebSocketRecoveryAction)>;

  StandardWebSocketErrorHandler();
  ~StandardWebSocketErrorHandler() override = default;

  // Configuration
  void setMaxRetryAttempts(int maxAttempts) { maxRetryAttempts_ = maxAttempts; }
  void setBaseRetryDelay(std::chrono::milliseconds delay) {
    baseRetryDelay_ = delay;
  }
  void setMaxRetryDelay(std::chrono::milliseconds delay) {
    maxRetryDelay_ = delay;
  }
  void setUseExponentialBackoff(bool enable) {
    useExponentialBackoff_ = enable;
  }

  // Callbacks
  void setErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }
  void setRecoveryCallback(RecoveryCallback callback) {
    recoveryCallback_ = callback;
  }

  // WebSocketErrorHandler interface
  void handleError(const WebSocketError &error) override;
  WebSocketRecoveryAction
  determineRecoveryAction(const WebSocketError &error) override;
  bool shouldRetry(const WebSocketError &error, int attemptCount) override;
  std::chrono::milliseconds getRetryDelay(const WebSocketError &error,
                                          int attemptCount) override;

  // Statistics
  struct ErrorStatistics {
    size_t totalErrors = 0;
    size_t connectionErrors = 0;
    size_t protocolErrors = 0;
    size_t timeoutErrors = 0;
    size_t messageErrors = 0;
    size_t retriesAttempted = 0;
    size_t successfulRecoveries = 0;
    std::chrono::system_clock::time_point lastErrorTime;
  };

  ErrorStatistics getStatistics() const;
  void resetStatistics();

private:
  int maxRetryAttempts_ = 3;
  std::chrono::milliseconds baseRetryDelay_{1000};
  std::chrono::milliseconds maxRetryDelay_{30000};
  bool useExponentialBackoff_ = true;

  ErrorCallback errorCallback_;
  RecoveryCallback recoveryCallback_;

  mutable std::mutex statisticsMutex_;
  ErrorStatistics statistics_;

  // Internal methods
  void updateStatistics(const WebSocketError &error);
  WebSocketRecoveryAction
  getDefaultRecoveryAction(WebSocketErrorCategory category,
                           WebSocketErrorSeverity severity);
};

/**
 * @brief WebSocket error factory for creating standardized errors
 */
class WebSocketErrorFactory {
public:
  // Create errors from Boost.Beast error codes
  static WebSocketError
  createFromBoostError(const boost::system::error_code &ec,
                       const std::string &component = "",
                       const std::string &operation = "");

  // Create errors from exceptions
  static WebSocketError createFromException(const std::exception &ex,
                                            const std::string &component = "",
                                            const std::string &operation = "");

  // Create specific error types
  static WebSocketError createConnectionError(const std::string &message,
                                              const std::string &details = "");
  static WebSocketError createProtocolError(const std::string &message,
                                            const std::string &details = "");
  static WebSocketError createTimeoutError(const std::string &operation,
                                           std::chrono::milliseconds timeout);
  static WebSocketError createMessageError(const std::string &message,
                                           const std::string &details = "");
  static WebSocketError
  createAuthenticationError(const std::string &message,
                            const std::string &details = "");

  // Utility methods
  static WebSocketErrorCategory
  categorizeBoostError(const boost::system::error_code &ec);
  static WebSocketErrorSeverity
  determineSeverity(WebSocketErrorCategory category,
                    const boost::system::error_code &ec);
  static std::string generateErrorId();
};

/**
 * @brief WebSocket connection wrapper with standardized error handling
 */
template <typename WebSocketType> class WebSocketWrapper {
public:
  using ErrorHandler = std::shared_ptr<WebSocketErrorHandler>;
  using MessageCallback = std::function<void(const std::string &)>;
  using ConnectionCallback = std::function<void(bool connected)>;

  WebSocketWrapper(std::unique_ptr<WebSocketType> ws,
                   ErrorHandler errorHandler);
  ~WebSocketWrapper();

  // Connection management
  bool connect(const std::string &host, const std::string &port,
               const std::string &target = "/ws");
  void disconnect();
  bool isConnected() const;

  // Message operations
  bool sendMessage(const std::string &message);
  void startReceiving();
  void stopReceiving();

  // Callbacks
  void setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
  }
  void setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = callback;
  }

  // Configuration
  void setReadTimeout(std::chrono::milliseconds timeout) {
    readTimeout_ = timeout;
  }
  void setWriteTimeout(std::chrono::milliseconds timeout) {
    writeTimeout_ = timeout;
  }
  void setAutoReconnect(bool enable) { autoReconnect_ = enable; }
  void setMaxReconnectAttempts(int attempts) {
    maxReconnectAttempts_ = attempts;
  }

  // Statistics
  struct ConnectionStatistics {
    size_t messagesSent = 0;
    size_t messagesReceived = 0;
    size_t connectionAttempts = 0;
    size_t reconnectionAttempts = 0;
    size_t errors = 0;
    std::chrono::system_clock::time_point connectionTime;
    std::chrono::system_clock::time_point lastMessageTime;
    bool isConnected = false;
  };

  ConnectionStatistics getStatistics() const;
  void resetStatistics();

private:
  std::unique_ptr<WebSocketType> ws_;
  ErrorHandler errorHandler_;

  std::atomic<bool> connected_{false};
  std::atomic<bool> receiving_{false};
  std::atomic<bool> shutdown_{false};

  std::thread receivingThread_;

  MessageCallback messageCallback_;
  ConnectionCallback connectionCallback_;

  std::chrono::milliseconds readTimeout_{30000};
  std::chrono::milliseconds writeTimeout_{30000};
  bool autoReconnect_ = false;
  int maxReconnectAttempts_ = 3;
  std::atomic<int> reconnectAttempts_{0};

  mutable std::mutex statisticsMutex_;
  ConnectionStatistics statistics_;

  std::string lastHost_;
  std::string lastPort_;
  std::string lastTarget_;

  // Internal methods
  void receivingLoop();
  bool attemptReconnection();
  void handleConnectionError(const boost::system::error_code &ec,
                             const std::string &operation);
  void updateConnectionStatus(bool connected);
  void updateStatistics(bool sent, bool received, bool error);
};

/**
 * @brief Global WebSocket error handler registry
 */
class WebSocketErrorRegistry {
public:
  static WebSocketErrorRegistry &getInstance();

  // Handler management
  void registerHandler(const std::string &component,
                       std::shared_ptr<WebSocketErrorHandler> handler);
  void unregisterHandler(const std::string &component);
  std::shared_ptr<WebSocketErrorHandler>
  getHandler(const std::string &component);

  // Global error handling
  void handleGlobalError(const WebSocketError &error);
  void setGlobalErrorHandler(std::shared_ptr<WebSocketErrorHandler> handler);

  // Statistics
  StandardWebSocketErrorHandler::ErrorStatistics getGlobalStatistics() const;

private:
  WebSocketErrorRegistry() = default;

  mutable std::mutex handlersMutex_;
  std::unordered_map<std::string, std::shared_ptr<WebSocketErrorHandler>>
      handlers_;
  std::shared_ptr<WebSocketErrorHandler> globalHandler_;
};

#endif // HYDROGEN_HAS_WEBSOCKETS

} // namespace core
} // namespace hydrogen

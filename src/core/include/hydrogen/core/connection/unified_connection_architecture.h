#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace hydrogen {
namespace core {
namespace connection {

/**
 * @brief Unified connection state enumeration
 */
enum class ConnectionState {
    DISCONNECTED = 0,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    DISCONNECTING,
    ERROR,
    SHUTDOWN
};

/**
 * @brief Connection protocol types
 */
enum class ProtocolType {
    STDIO = 0,
    FIFO,
    WEBSOCKET,
    HTTP,
    GRPC,
    MQTT,
    ZMQ,
    TCP,
    UDP
};

/**
 * @brief Connection configuration
 */
struct ConnectionConfig {
    std::string host = "localhost";
    uint16_t port = 8000;
    ProtocolType protocol = ProtocolType::WEBSOCKET;
    
    // Timeout settings
    std::chrono::milliseconds connectTimeout{5000};
    std::chrono::milliseconds readTimeout{30000};
    std::chrono::milliseconds writeTimeout{5000};
    
    // Retry settings
    bool enableAutoReconnect = true;
    uint32_t maxRetries = 5;
    std::chrono::seconds retryInterval{5};
    std::chrono::seconds backoffMultiplier{2};
    std::chrono::seconds maxBackoff{60};
    
    // Health monitoring
    bool enableHeartbeat = true;
    std::chrono::seconds heartbeatInterval{30};
    std::chrono::seconds healthCheckInterval{10};
    
    // Resource management
    size_t maxMessageQueueSize = 1000;
    size_t maxMessageSize = 1024 * 1024; // 1MB
    bool enableCompression = false;
    
    // Security
    bool enableTLS = false;
    std::string certificatePath;
    std::string privateKeyPath;
};

/**
 * @brief Connection statistics
 */
struct ConnectionStatistics {
    std::atomic<uint64_t> messagesReceived{0};
    std::atomic<uint64_t> messagesSent{0};
    std::atomic<uint64_t> bytesReceived{0};
    std::atomic<uint64_t> bytesSent{0};
    std::atomic<uint32_t> reconnectionAttempts{0};
    std::atomic<uint32_t> errorCount{0};
    
    std::chrono::system_clock::time_point connectionTime;
    std::chrono::system_clock::time_point lastActivity;
    std::chrono::milliseconds averageLatency{0};
    
    ConnectionState currentState = ConnectionState::DISCONNECTED;
    std::string lastError;
};

/**
 * @brief Connection event callbacks
 */
using ConnectionStateCallback = std::function<void(ConnectionState, const std::string&)>;
using MessageReceivedCallback = std::function<void(const std::string&)>;
using ErrorCallback = std::function<void(const std::string&, int)>;

/**
 * @brief Abstract base class for protocol-specific connections
 */
class IProtocolConnection {
public:
    virtual ~IProtocolConnection() = default;
    
    virtual bool connect(const ConnectionConfig& config) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    virtual bool sendMessage(const std::string& message) = 0;
    virtual std::string receiveMessage() = 0;
    virtual bool hasMessage() const = 0;
    
    virtual ConnectionState getState() const = 0;
    virtual ConnectionStatistics getStatistics() const = 0;
    
    virtual void setStateCallback(ConnectionStateCallback callback) = 0;
    virtual void setMessageCallback(MessageReceivedCallback callback) = 0;
    virtual void setErrorCallback(ErrorCallback callback) = 0;
};

/**
 * @brief Connection health monitor
 */
class ConnectionHealthMonitor {
public:
    explicit ConnectionHealthMonitor(std::shared_ptr<IProtocolConnection> connection);
    ~ConnectionHealthMonitor();
    
    void start();
    void stop();
    
    bool isHealthy() const;
    std::chrono::milliseconds getLatency() const;
    double getUptime() const;
    
    void setHealthCallback(std::function<void(bool)> callback);

private:
    void monitoringLoop();
    void performHealthCheck();
    void updateLatency(std::chrono::milliseconds latency);
    
    std::shared_ptr<IProtocolConnection> connection_;
    std::atomic<bool> running_{false};
    std::atomic<bool> healthy_{false};
    std::thread monitoringThread_;
    
    std::mutex latencyMutex_;
    std::chrono::milliseconds currentLatency_{0};
    std::chrono::system_clock::time_point startTime_;
    
    std::function<void(bool)> healthCallback_;
    std::chrono::seconds checkInterval_{10};
};

/**
 * @brief Connection pool for managing multiple connections
 */
class ConnectionPool {
public:
    explicit ConnectionPool(size_t maxConnections = 10);
    ~ConnectionPool();
    
    std::shared_ptr<IProtocolConnection> acquireConnection(const ConnectionConfig& config);
    void releaseConnection(std::shared_ptr<IProtocolConnection> connection);
    
    void setMaxConnections(size_t maxConnections);
    size_t getActiveConnections() const;
    size_t getAvailableConnections() const;
    
    void cleanup();

private:
    struct PooledConnection {
        std::shared_ptr<IProtocolConnection> connection;
        std::chrono::system_clock::time_point lastUsed;
        bool inUse = false;
    };
    
    mutable std::mutex poolMutex_;
    std::vector<PooledConnection> connections_;
    size_t maxConnections_;
    
    std::shared_ptr<IProtocolConnection> createConnection(const ConnectionConfig& config);
    void cleanupIdleConnections();
};

/**
 * @brief Unified connection manager
 */
class UnifiedConnectionManager {
public:
    UnifiedConnectionManager();
    ~UnifiedConnectionManager();
    
    // Connection management
    bool connect(const ConnectionConfig& config);
    void disconnect();
    bool isConnected() const;
    
    // Message handling
    bool sendMessage(const std::string& message);
    std::string receiveMessage();
    bool hasMessage() const;
    
    // State and statistics
    ConnectionState getState() const;
    ConnectionStatistics getStatistics() const;
    
    // Configuration
    void updateConfig(const ConnectionConfig& config);
    ConnectionConfig getConfig() const;
    
    // Callbacks
    void setStateCallback(ConnectionStateCallback callback);
    void setMessageCallback(MessageReceivedCallback callback);
    void setErrorCallback(ErrorCallback callback);
    
    // Health monitoring
    void enableHealthMonitoring(bool enable);
    bool isHealthy() const;
    std::chrono::milliseconds getLatency() const;
    
    // Connection pooling
    void enableConnectionPooling(bool enable, size_t maxConnections = 10);

private:
    void initializeConnection();
    void startBackgroundThreads();
    void stopBackgroundThreads();
    
    void reconnectionLoop();
    void messageProcessingLoop();
    
    void handleStateChange(ConnectionState newState, const std::string& error = "");
    void handleError(const std::string& error, int errorCode = 0);
    
    std::shared_ptr<IProtocolConnection> connection_;
    std::unique_ptr<ConnectionHealthMonitor> healthMonitor_;
    std::unique_ptr<ConnectionPool> connectionPool_;
    
    ConnectionConfig config_;
    std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
    
    // Callbacks
    ConnectionStateCallback stateCallback_;
    MessageReceivedCallback messageCallback_;
    ErrorCallback errorCallback_;
    
    // Background threads
    std::atomic<bool> running_{false};
    std::thread reconnectionThread_;
    std::thread messageThread_;
    
    // Thread safety
    mutable std::mutex configMutex_;
    mutable std::mutex callbackMutex_;
    
    // Features
    std::atomic<bool> healthMonitoringEnabled_{true};
    std::atomic<bool> connectionPoolingEnabled_{false};
};

/**
 * @brief Factory for creating protocol-specific connections
 */
class ConnectionFactory {
public:
    static std::shared_ptr<IProtocolConnection> createConnection(ProtocolType protocol);
    static std::vector<ProtocolType> getSupportedProtocols();
    static std::string getProtocolName(ProtocolType protocol);
    static ProtocolType getProtocolFromName(const std::string& name);
};

} // namespace connection
} // namespace core
} // namespace hydrogen

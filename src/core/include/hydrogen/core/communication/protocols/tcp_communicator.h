#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#ifdef HYDROGEN_ENABLE_SPDLOG
#include <spdlog/spdlog.h>
#else
#include <hydrogen/core/logging/mock_spdlog.h>
#endif

#include "hydrogen/core/device/device_communicator.h"
#include "hydrogen/core/performance/connection_pool.h"
#include "hydrogen/core/performance/message_batcher.h"
#include "hydrogen/core/performance/memory_pool.h"
#include "hydrogen/core/performance/serialization_optimizer.h"

namespace hydrogen {
namespace core {
namespace communication {
namespace protocols {

using json = nlohmann::json;
using namespace performance;

/**
 * @brief TCP connection state
 */
enum class TcpConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    TCP_ERROR
};

/**
 * @brief TCP connection configuration
 */
struct TcpConnectionConfig {
    std::string serverAddress = "localhost";
    uint16_t serverPort = 8001;
    bool isServer = false;
    std::chrono::milliseconds connectTimeout{5000};
    std::chrono::milliseconds readTimeout{30000};
    std::chrono::milliseconds writeTimeout{5000};
    size_t bufferSize = 8192;
    bool enableKeepAlive = true;
    std::chrono::seconds keepAliveInterval{30};
    int keepAliveProbes = 3;
    std::chrono::seconds keepAliveTimeout{10};
    bool enableNagle = false;
    size_t maxConnections = 100;
    bool reuseAddress = true;
    std::string bindInterface = "0.0.0.0";
    bool enableSSL = false;
    std::string sslCertPath;
    std::string sslKeyPath;
    std::string sslCaPath;
    bool enableCompression = false;
    bool enableMessageBatching = true;
    size_t maxBatchSize = 50;
    std::chrono::milliseconds batchTimeout{100};
    
    json toJson() const;
    static TcpConnectionConfig fromJson(const json& j);
};

/**
 * @brief TCP connection metrics
 */
struct TcpConnectionMetrics {
    std::atomic<size_t> connectionsEstablished{0};
    std::atomic<size_t> connectionsDropped{0};
    std::atomic<size_t> messagesSent{0};
    std::atomic<size_t> messagesReceived{0};
    std::atomic<size_t> bytesSent{0};
    std::atomic<size_t> bytesReceived{0};
    std::atomic<double> averageLatency{0.0};
    std::atomic<size_t> errorCount{0};
    std::atomic<size_t> timeoutCount{0};
    std::chrono::system_clock::time_point lastActivity;

    // Copy constructor and assignment operator
    TcpConnectionMetrics() = default;
    TcpConnectionMetrics(const TcpConnectionMetrics& other) {
        connectionsEstablished.store(other.connectionsEstablished.load());
        connectionsDropped.store(other.connectionsDropped.load());
        messagesSent.store(other.messagesSent.load());
        messagesReceived.store(other.messagesReceived.load());
        bytesSent.store(other.bytesSent.load());
        bytesReceived.store(other.bytesReceived.load());
        averageLatency.store(other.averageLatency.load());
        errorCount.store(other.errorCount.load());
        timeoutCount.store(other.timeoutCount.load());
        lastActivity = other.lastActivity;
    }

    TcpConnectionMetrics& operator=(const TcpConnectionMetrics& other) {
        if (this != &other) {
            connectionsEstablished.store(other.connectionsEstablished.load());
            connectionsDropped.store(other.connectionsDropped.load());
            messagesSent.store(other.messagesSent.load());
            messagesReceived.store(other.messagesReceived.load());
            bytesSent.store(other.bytesSent.load());
            bytesReceived.store(other.bytesReceived.load());
            averageLatency.store(other.averageLatency.load());
            errorCount.store(other.errorCount.load());
            timeoutCount.store(other.timeoutCount.load());
            lastActivity = other.lastActivity;
        }
        return *this;
    }

    json toJson() const;
};

/**
 * @brief TCP client session
 */
class TcpClientSession {
public:
    explicit TcpClientSession(const std::string& clientId);
    ~TcpClientSession();
    
    // Connection management
    bool connect(const TcpConnectionConfig& config);
    void disconnect();
    bool isConnected() const;
    
    // Message operations
    std::future<bool> sendMessage(const std::string& message);
    bool sendMessageSync(const std::string& message);
    
    // Configuration
    void setMessageCallback(std::function<void(const std::string&)> callback);
    void setConnectionStatusCallback(std::function<void(bool)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    
    // Status and metrics
    std::string getClientId() const { return clientId_; }
    TcpConnectionState getState() const { return state_.load(); }
    TcpConnectionMetrics getMetrics() const { return metrics_; }
    
private:
    std::string clientId_;
    std::atomic<TcpConnectionState> state_{TcpConnectionState::DISCONNECTED};
    TcpConnectionConfig config_;
    
    // Platform-specific socket handle
#ifdef _WIN32
    uintptr_t socket_ = 0; // SOCKET on Windows
#else
    int socket_ = -1;      // file descriptor on Unix
#endif
    
    // Threading
    std::unique_ptr<std::thread> receiveThread_;
    std::unique_ptr<std::thread> sendThread_;
    std::atomic<bool> running_{false};
    
    // Message queues
    std::queue<std::string> sendQueue_;
    std::mutex sendQueueMutex_;
    std::condition_variable sendCondition_;
    
    // Callbacks
    std::function<void(const std::string&)> messageCallback_;
    std::function<void(bool)> connectionStatusCallback_;
    std::function<void(const std::string&)> errorCallback_;
    std::mutex callbacksMutex_;
    
    // Metrics
    TcpConnectionMetrics metrics_;
    
    // Internal methods
    bool performConnect();
    void performDisconnect();
    void receiveThreadFunction();
    void sendThreadFunction();
    void handleError(const std::string& error);
    void updateMetrics();
};

/**
 * @brief TCP server for handling multiple client connections
 */
class TcpServer {
public:
    explicit TcpServer(const TcpConnectionConfig& config);
    ~TcpServer();
    
    // Server lifecycle
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // Client management
    std::vector<std::string> getConnectedClients() const;
    bool sendToClient(const std::string& clientId, const std::string& message);
    bool sendToAllClients(const std::string& message);
    void disconnectClient(const std::string& clientId);
    
    // Configuration
    void setClientConnectedCallback(std::function<void(const std::string&)> callback);
    void setClientDisconnectedCallback(std::function<void(const std::string&)> callback);
    void setMessageReceivedCallback(std::function<void(const std::string&, const std::string&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    
    // Status and metrics
    size_t getClientCount() const;
    TcpConnectionMetrics getAggregatedMetrics() const;
    json getDetailedMetrics() const;
    
private:
    TcpConnectionConfig config_;
    std::atomic<bool> running_{false};
    
    // Server socket
#ifdef _WIN32
    uintptr_t serverSocket_ = 0;
#else
    int serverSocket_ = -1;
#endif
    
    // Client sessions
    std::unordered_map<std::string, std::shared_ptr<TcpClientSession>> clients_;
    mutable std::mutex clientsMutex_;
    
    // Threading
    std::unique_ptr<std::thread> acceptThread_;
    
    // Callbacks
    std::function<void(const std::string&)> clientConnectedCallback_;
    std::function<void(const std::string&)> clientDisconnectedCallback_;
    std::function<void(const std::string&, const std::string&)> messageReceivedCallback_;
    std::function<void(const std::string&)> errorCallback_;
    std::mutex callbacksMutex_;
    
    // Internal methods
    bool setupServerSocket();
    void acceptThreadFunction();
    void handleNewConnection(int clientSocket);
    std::string generateClientId();
    void cleanupClient(const std::string& clientId);
};

/**
 * @brief High-performance TCP communicator with performance optimization integration
 */
class TcpCommunicator : public IDeviceCommunicator {
public:
    explicit TcpCommunicator(const TcpConnectionConfig& config = TcpConnectionConfig{});
    ~TcpCommunicator() override;
    
    // IDeviceCommunicator implementation
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    std::future<CommunicationResponse> sendMessage(const CommunicationMessage& message) override;
    CommunicationResponse sendMessageSync(const CommunicationMessage& message) override;
    void setMessageCallback(std::function<void(const CommunicationMessage&)> callback) override;
    void setConnectionStatusCallback(std::function<void(bool)> callback) override;
    CommunicationStats getStatistics() const override;
    void resetStatistics() override;
    std::vector<CommunicationProtocol> getSupportedProtocols() const override;
    void setQoSParameters(const json& qosParams) override;
    void setCompressionEnabled(bool enabled) override;
    void setEncryptionEnabled(bool enabled, const std::string& encryptionKey = "") override;
    
    // TCP-specific methods
    void setTcpConfiguration(const TcpConnectionConfig& config);
    TcpConnectionConfig getTcpConfiguration() const;
    TcpConnectionMetrics getTcpMetrics() const;
    json getDetailedTcpMetrics() const;
    
    // Server mode methods
    bool startServer();
    void stopServer();
    bool isServerMode() const { return config_.isServer; }
    std::vector<std::string> getConnectedClients() const;
    bool sendToClient(const std::string& clientId, const CommunicationMessage& message);
    bool sendToAllClients(const CommunicationMessage& message);
    
    // Performance optimization integration
    void enableConnectionPooling(bool enabled);
    void enableMessageBatching(bool enabled);
    void enableMemoryPooling(bool enabled);
    void enableSerializationOptimization(bool enabled);
    
private:
    TcpConnectionConfig config_;
    mutable std::mutex configMutex_;
    
    // Connection management
    std::shared_ptr<TcpClientSession> clientSession_;
    std::shared_ptr<TcpServer> server_;
    std::atomic<bool> connected_{false};
    
    // Performance optimization components
    std::shared_ptr<ConnectionPool> connectionPool_;
    std::shared_ptr<MessageBatcher> messageBatcher_;
    std::shared_ptr<MemoryPool<std::string>> stringPool_;
    std::shared_ptr<SerializationOptimizer> serializationOptimizer_;
    
    // Callbacks
    std::function<void(const CommunicationMessage&)> messageCallback_;
    std::function<void(bool)> connectionStatusCallback_;
    std::mutex callbacksMutex_;
    
    // Statistics
    mutable CommunicationStats stats_;
    mutable std::mutex statsMutex_;
    
    // QoS and optimization settings
    json qosParameters_;
    std::atomic<bool> compressionEnabled_{false};
    std::atomic<bool> encryptionEnabled_{false};
    std::string encryptionKey_;
    
    // Performance optimization flags
    std::atomic<bool> connectionPoolingEnabled_{true};
    std::atomic<bool> messageBatchingEnabled_{true};
    std::atomic<bool> memoryPoolingEnabled_{true};
    std::atomic<bool> serializationOptimizationEnabled_{true};
    
    // Internal methods
    bool initializePerformanceComponents();
    void shutdownPerformanceComponents();
    std::string serializeMessage(const CommunicationMessage& message);
    CommunicationMessage deserializeMessage(const std::string& data);
    void updateStatistics(const CommunicationResponse& response);
    std::string generateMessageId() const;
    void handleIncomingMessage(const std::string& rawMessage);
    void handleConnectionStatusChange(bool connected);
    void handleError(const std::string& error);
};

/**
 * @brief TCP communicator factory
 */
class TcpCommunicatorFactory {
public:
    static std::shared_ptr<TcpCommunicator> createClient(const TcpConnectionConfig& config);
    static std::shared_ptr<TcpCommunicator> createServer(const TcpConnectionConfig& config);
    static std::shared_ptr<TcpCommunicator> createWithPerformanceOptimization(
        const TcpConnectionConfig& config, 
        bool enableConnectionPooling = true,
        bool enableMessageBatching = true,
        bool enableMemoryPooling = true,
        bool enableSerializationOptimization = true
    );
    
    // Configuration helpers
    static TcpConnectionConfig createDefaultClientConfig(const std::string& host, uint16_t port);
    static TcpConnectionConfig createDefaultServerConfig(uint16_t port, const std::string& bindInterface = "0.0.0.0");
    static TcpConnectionConfig createHighPerformanceConfig();
    static TcpConnectionConfig createSecureConfig(const std::string& certPath, const std::string& keyPath);
};

/**
 * @brief TCP connection manager for managing multiple TCP connections
 */
class TcpConnectionManager {
public:
    static TcpConnectionManager& getInstance();
    
    // Connection management
    void registerConnection(const std::string& name, std::shared_ptr<TcpCommunicator> communicator);
    void unregisterConnection(const std::string& name);
    std::shared_ptr<TcpCommunicator> getConnection(const std::string& name);
    
    // Global operations
    void startAllConnections();
    void stopAllConnections();
    json getAllConnectionMetrics() const;
    
    // Configuration
    void setGlobalConfig(const json& config);
    json getGlobalConfig() const;
    
private:
    TcpConnectionManager() = default;
    ~TcpConnectionManager() = default;
    
    std::unordered_map<std::string, std::shared_ptr<TcpCommunicator>> connections_;
    mutable std::mutex connectionsMutex_;
    json globalConfig_;
    mutable std::mutex configMutex_;
};

} // namespace protocols
} // namespace communication
} // namespace core
} // namespace hydrogen

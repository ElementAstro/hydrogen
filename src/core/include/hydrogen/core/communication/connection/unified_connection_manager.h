#pragma once

#include "message.h"
#include "message_transformer.h"
#include "protocol_error_mapper.h"
#include "websocket_error_handler.h"
#include "protocol_converters.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Connection state enumeration
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    CONNECTION_ERROR,
    SHUTDOWN
};

/**
 * @brief Connection configuration for different protocols
 */
struct ConnectionConfig {
    MessageFormat protocol = MessageFormat::HTTP_JSON;
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
    
    // Protocol-specific settings
    json protocolSettings;
    
    // Authentication
    std::string username;
    std::string password;
    std::string token;
    json authSettings;
};

/**
 * @brief Connection statistics
 */
struct ConnectionStatistics {
    ConnectionState state = ConnectionState::DISCONNECTED;
    std::chrono::system_clock::time_point connectionTime;
    std::chrono::system_clock::time_point lastActivityTime;
    std::chrono::milliseconds uptime{0};
    size_t messagesSent = 0;
    size_t messagesReceived = 0;
    size_t bytesTransferred = 0;
    size_t reconnectionAttempts = 0;
    size_t errors = 0;
    std::string lastError;
    double averageLatency = 0.0;
    std::vector<double> recentLatencies;
};

/**
 * @brief Protocol-specific connection interface
 */
class ProtocolConnection {
public:
    virtual ~ProtocolConnection() = default;
    
    // Connection management
    virtual bool connect(const ConnectionConfig& config) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual ConnectionState getState() const = 0;
    
    // Message operations
    virtual bool sendMessage(const std::string& data) = 0;
    virtual bool receiveMessage(std::string& data) = 0;
    
    // Configuration
    virtual void updateConfig(const ConnectionConfig& config) = 0;
    virtual MessageFormat getProtocol() const = 0;
    
    // Statistics
    virtual ConnectionStatistics getStatistics() const = 0;
    virtual void resetStatistics() = 0;
};

/**
 * @brief WebSocket connection implementation
 */
class WebSocketConnection : public ProtocolConnection {
public:
    explicit WebSocketConnection(std::shared_ptr<WebSocketErrorHandler> errorHandler = nullptr);
    ~WebSocketConnection() override;
    
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    ConnectionState getState() const override;
    
    bool sendMessage(const std::string& data) override;
    bool receiveMessage(std::string& data) override;
    
    void updateConfig(const ConnectionConfig& config) override;
    MessageFormat getProtocol() const override { return MessageFormat::HTTP_JSON; }
    
    ConnectionStatistics getStatistics() const override;
    void resetStatistics() override;

private:
    ConnectionConfig config_;
    std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
    void* websocketHandle_{nullptr}; // Placeholder for actual WebSocket handle
    std::shared_ptr<WebSocketErrorHandler> errorHandler_;
    mutable std::mutex statisticsMutex_;
    ConnectionStatistics statistics_;
    
    bool establishWebSocketConnection();
    void cleanupWebSocketConnection();
    void updateStatistics(bool sent, bool received, size_t bytes, bool error = false);
};

/**
 * @brief HTTP connection implementation
 */
class HttpConnection : public ProtocolConnection {
public:
    HttpConnection();
    ~HttpConnection() override;
    
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    ConnectionState getState() const override;
    
    bool sendMessage(const std::string& data) override;
    bool receiveMessage(std::string& data) override;
    
    void updateConfig(const ConnectionConfig& config) override;
    MessageFormat getProtocol() const override { return MessageFormat::HTTP_JSON; }
    
    ConnectionStatistics getStatistics() const override;
    void resetStatistics() override;

private:
    ConnectionConfig config_;
    std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
    void* httpHandle_{nullptr}; // Placeholder for actual HTTP handle
    mutable std::mutex statisticsMutex_;
    ConnectionStatistics statistics_;
    
    bool establishHttpConnection();
    void cleanupHttpConnection();
    void updateStatistics(bool sent, bool received, size_t bytes, bool error = false);
};

/**
 * @brief gRPC connection implementation
 */
class GrpcConnection : public ProtocolConnection {
public:
    GrpcConnection();
    ~GrpcConnection() override;
    
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    ConnectionState getState() const override;
    
    bool sendMessage(const std::string& data) override;
    bool receiveMessage(std::string& data) override;
    
    void updateConfig(const ConnectionConfig& config) override;
    MessageFormat getProtocol() const override { return MessageFormat::PROTOBUF; }
    
    ConnectionStatistics getStatistics() const override;
    void resetStatistics() override;

private:
    ConnectionConfig config_;
    std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
    void* grpcChannel_{nullptr}; // Placeholder for actual gRPC channel
    mutable std::mutex statisticsMutex_;
    ConnectionStatistics statistics_;
    
    bool establishGrpcConnection();
    void cleanupGrpcConnection();
    void updateStatistics(bool sent, bool received, size_t bytes, bool error = false);
};

/**
 * @brief Unified Connection Manager
 * 
 * This class manages connections across multiple protocols and provides
 * a unified interface for connection management, message routing, and
 * protocol-specific handling.
 */
class UnifiedConnectionManager {
public:
    using ConnectionCallback = std::function<void(const std::string& connectionId, ConnectionState state)>;
    using MessageCallback = std::function<void(const std::string& connectionId, const json& message)>;
    using ErrorCallback = std::function<void(const std::string& connectionId, const std::string& error)>;

    UnifiedConnectionManager();
    ~UnifiedConnectionManager();

    // Connection Management
    std::string createConnection(const ConnectionConfig& config);
    bool connectConnection(const std::string& connectionId);
    void disconnectConnection(const std::string& connectionId);
    void disconnectAll();
    
    bool isConnected(const std::string& connectionId) const;
    ConnectionState getConnectionState(const std::string& connectionId) const;
    std::vector<std::string> getActiveConnections() const;
    
    // Message Operations
    bool sendMessage(const std::string& connectionId, std::shared_ptr<Message> message);
    bool sendRawMessage(const std::string& connectionId, const std::string& data);
    bool broadcastMessage(std::shared_ptr<Message> message, const std::vector<std::string>& connectionIds = {});
    
    // Configuration
    void updateConnectionConfig(const std::string& connectionId, const ConnectionConfig& config);
    ConnectionConfig getConnectionConfig(const std::string& connectionId) const;
    
    // Auto-reconnection
    void enableAutoReconnect(const std::string& connectionId, bool enable = true);
    void setReconnectSettings(const std::string& connectionId, std::chrono::milliseconds interval, int maxAttempts);
    
    // Callbacks
    void setConnectionCallback(ConnectionCallback callback) { connectionCallback_ = callback; }
    void setMessageCallback(MessageCallback callback) { messageCallback_ = callback; }
    void setErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }
    
    // Statistics and Monitoring
    ConnectionStatistics getConnectionStatistics(const std::string& connectionId) const;
    std::unordered_map<std::string, ConnectionStatistics> getAllStatistics() const;
    void resetStatistics(const std::string& connectionId = "");
    
    // Protocol Support
    std::vector<MessageFormat> getSupportedProtocols() const;
    bool isProtocolSupported(MessageFormat protocol) const;
    
    // Message Processing
    void startMessageProcessing();
    void stopMessageProcessing();
    bool isMessageProcessingActive() const;

private:
    struct ConnectionInfo {
        std::string id;
        std::unique_ptr<ProtocolConnection> connection;
        ConnectionConfig config;
        std::atomic<bool> autoReconnect{true};
        std::atomic<int> reconnectAttempts{0};
        std::chrono::system_clock::time_point lastReconnectAttempt;
        std::unique_ptr<ProtocolConverter> converter;
    };
    
    mutable std::mutex connectionsMutex_;
    std::unordered_map<std::string, std::unique_ptr<ConnectionInfo>> connections_;
    
    std::atomic<bool> messageProcessingActive_{false};
    std::atomic<bool> shutdown_{false};
    std::thread messageProcessingThread_;
    std::thread reconnectionThread_;
    
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    ErrorCallback errorCallback_;
    
    std::shared_ptr<ProtocolErrorMapper> errorMapper_;
    
    // Internal methods
    std::string generateConnectionId() const;
    std::unique_ptr<ProtocolConnection> createProtocolConnection(MessageFormat protocol);
    void messageProcessingLoop();
    void reconnectionLoop();
    void handleConnectionStateChange(const std::string& connectionId, ConnectionState newState);
    void attemptReconnection(const std::string& connectionId);
    
    // Protocol-specific factories
    std::unique_ptr<WebSocketConnection> createWebSocketConnection();
    std::unique_ptr<HttpConnection> createHttpConnection();
    std::unique_ptr<GrpcConnection> createGrpcConnection();
};

/**
 * @brief Factory for creating connection managers
 */
class ConnectionManagerFactory {
public:
    static std::unique_ptr<UnifiedConnectionManager> createManager();
    static std::unique_ptr<UnifiedConnectionManager> createManagerWithDefaults();
    
    // Protocol-specific connection configs
    static ConnectionConfig createWebSocketConfig(const std::string& host, uint16_t port);
    static ConnectionConfig createHttpConfig(const std::string& host, uint16_t port);
    static ConnectionConfig createGrpcConfig(const std::string& host, uint16_t port);
    static ConnectionConfig createMqttConfig(const std::string& host, uint16_t port);
    
    // Configuration presets
    static ConnectionConfig getSecureConfig(MessageFormat protocol, const std::string& host, uint16_t port);
    static ConnectionConfig getHighPerformanceConfig(MessageFormat protocol, const std::string& host, uint16_t port);
    static ConnectionConfig getReliableConfig(MessageFormat protocol, const std::string& host, uint16_t port);
};

} // namespace core
} // namespace hydrogen

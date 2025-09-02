#pragma once

#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <chrono>

namespace hydrogen {
namespace server {
namespace core {

/**
 * @brief Communication protocol enumeration
 */
enum class CommunicationProtocol {
    HTTP,
    WEBSOCKET,
    MQTT,
    GRPC,
    ZMQ_REQ_REP,
    ZMQ_PUB_SUB,
    ZMQ_PUSH_PULL
};

/**
 * @brief Server status enumeration
 */
enum class ServerStatus {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING,
    ERROR
};

/**
 * @brief Server configuration structure
 */
struct ServerConfig {
    std::string name;
    std::string host = "localhost";
    uint16_t port = 8080;
    bool enableSsl = false;
    std::string sslCertPath;
    std::string sslKeyPath;
    int maxConnections = 1000;
    std::chrono::seconds connectionTimeout{30};
    bool enableLogging = true;
    std::string logLevel = "info";
};

/**
 * @brief Connection information structure
 */
struct ConnectionInfo {
    std::string clientId;
    std::string remoteAddress;
    uint16_t remotePort;
    CommunicationProtocol protocol;
    std::chrono::system_clock::time_point connectedAt;
    std::chrono::system_clock::time_point lastActivity;
};

/**
 * @brief Message structure for inter-protocol communication
 */
struct Message {
    std::string senderId;
    std::string recipientId;
    std::string topic;
    std::string payload;
    CommunicationProtocol sourceProtocol;
    CommunicationProtocol targetProtocol;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> headers;
};

/**
 * @brief Base interface for all server implementations
 * 
 * This interface defines the common contract that all protocol-specific
 * servers must implement. It provides lifecycle management, configuration,
 * and basic monitoring capabilities.
 */
class IServerInterface {
public:
    virtual ~IServerInterface() = default;

    // Lifecycle management
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool restart() = 0;
    virtual ServerStatus getStatus() const = 0;

    // Configuration
    virtual void setConfig(const ServerConfig& config) = 0;
    virtual ServerConfig getConfig() const = 0;
    virtual bool isConfigValid() const = 0;

    // Connection management
    virtual std::vector<ConnectionInfo> getActiveConnections() const = 0;
    virtual size_t getConnectionCount() const = 0;
    virtual bool disconnectClient(const std::string& clientId) = 0;

    // Protocol identification
    virtual CommunicationProtocol getProtocol() const = 0;
    virtual std::string getProtocolName() const = 0;

    // Health monitoring
    virtual bool isHealthy() const = 0;
    virtual std::string getHealthStatus() const = 0;

    // Event callbacks
    using ConnectionCallback = std::function<void(const ConnectionInfo&, bool connected)>;
    using MessageCallback = std::function<void(const Message&)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    virtual void setConnectionCallback(ConnectionCallback callback) = 0;
    virtual void setMessageCallback(MessageCallback callback) = 0;
    virtual void setErrorCallback(ErrorCallback callback) = 0;
};

/**
 * @brief Factory interface for creating server instances
 */
class IServerFactory {
public:
    virtual ~IServerFactory() = default;
    
    virtual std::unique_ptr<IServerInterface> createServer(
        CommunicationProtocol protocol,
        const ServerConfig& config
    ) = 0;
    
    virtual std::vector<CommunicationProtocol> getSupportedProtocols() const = 0;
    virtual bool isProtocolSupported(CommunicationProtocol protocol) const = 0;
};

/**
 * @brief Multi-protocol server interface
 * 
 * This interface manages multiple protocol servers and provides
 * unified access to all communication channels.
 */
class IMultiProtocolServer {
public:
    virtual ~IMultiProtocolServer() = default;

    // Server management
    virtual bool addProtocol(CommunicationProtocol protocol, const ServerConfig& config) = 0;
    virtual bool removeProtocol(CommunicationProtocol protocol) = 0;
    virtual std::vector<CommunicationProtocol> getActiveProtocols() const = 0;

    // Lifecycle
    virtual bool startAll() = 0;
    virtual bool stopAll() = 0;
    virtual bool startProtocol(CommunicationProtocol protocol) = 0;
    virtual bool stopProtocol(CommunicationProtocol protocol) = 0;

    // Message routing
    virtual bool sendMessage(const Message& message) = 0;
    virtual bool broadcastMessage(const Message& message, 
                                const std::vector<CommunicationProtocol>& protocols = {}) = 0;

    // Status monitoring
    virtual std::vector<ServerStatus> getProtocolStatuses() const = 0;
    virtual bool isProtocolHealthy(CommunicationProtocol protocol) const = 0;
    virtual std::string getOverallHealthStatus() const = 0;

    // Event handling
    virtual void setGlobalConnectionCallback(
        std::function<void(CommunicationProtocol, const ConnectionInfo&, bool)> callback) = 0;
    virtual void setGlobalMessageCallback(
        std::function<void(const Message&)> callback) = 0;
    virtual void setGlobalErrorCallback(
        std::function<void(CommunicationProtocol, const std::string&)> callback) = 0;
};

} // namespace core
} // namespace server
} // namespace hydrogen

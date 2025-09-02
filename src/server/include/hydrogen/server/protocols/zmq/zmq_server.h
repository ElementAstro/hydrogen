#pragma once

#include "hydrogen/core/communication_protocol.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>

namespace hydrogen {
namespace server {
namespace protocols {
namespace zmq {

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
    XPUB = 6,   // Extended Publisher
    XSUB = 7,   // Extended Subscriber
    PUSH = 8,   // Push
    PULL = 9,   // Pull
    PAIR = 10   // Pair
};

/**
 * @brief ZeroMQ socket options
 */
enum class ZmqSocketOption {
    SEND_TIMEOUT = 0,
    RECEIVE_TIMEOUT = 1,
    SEND_BUFFER_SIZE = 2,
    RECEIVE_BUFFER_SIZE = 3,
    HIGH_WATER_MARK = 4,
    LINGER = 5,
    RECONNECT_INTERVAL = 6,
    MAX_RECONNECT_INTERVAL = 7
};

/**
 * @brief ZeroMQ message types
 */
enum class ZmqMessageType {
    DATA = 0,
    CONTROL = 1,
    HEARTBEAT = 2,
    BROADCAST = 3
};

/**
 * @brief ZeroMQ message structure
 */
struct ZmqMessage {
    std::string id;
    std::string content;
    std::string clientId;
    ZmqMessageType type = ZmqMessageType::DATA;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief ZeroMQ client information
 */
struct ZmqClientInfo {
    std::string clientId;
    std::string remoteAddress;
    int remotePort = 0;
    std::chrono::system_clock::time_point connectedAt;
    std::chrono::system_clock::time_point lastActivity;
    bool isConnected = false;
    std::unordered_map<std::string, std::string> properties;
};

/**
 * @brief ZeroMQ server configuration
 */
struct ZmqServerConfig {
    std::string bindAddress = "tcp://*:5555";
    ZmqSocketType socketType = ZmqSocketType::REP;
    int ioThreads = 1;
    int maxSockets = 1024;
    int sendTimeout = 1000;    // milliseconds
    int receiveTimeout = 1000; // milliseconds
    bool enableLogging = true;
    std::string logLevel = "INFO";
};

/**
 * @brief ZeroMQ server statistics
 */
struct ZmqServerStatistics {
    size_t connectedClients = 0;
    size_t totalMessagesSent = 0;
    size_t totalMessagesReceived = 0;
    size_t totalErrors = 0;
    double messagesPerSecond = 0.0;
    int64_t uptime = 0;
    size_t bytesReceived = 0;
    size_t bytesSent = 0;
};

/**
 * @brief ZeroMQ message handler function type
 */
using ZmqMessageHandler = std::function<void(const ZmqMessage& message)>;

/**
 * @brief ZeroMQ connection handler function type
 */
using ZmqConnectionHandler = std::function<void(const std::string& clientId, bool connected)>;

/**
 * @brief Interface for ZeroMQ server implementation
 */
class IZmqServer {
public:
    virtual ~IZmqServer() = default;

    // Server lifecycle
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool restart() = 0;
    virtual bool isRunning() const = 0;
    virtual bool isInitialized() const = 0;

    // Configuration
    virtual ZmqServerConfig getConfig() const = 0;
    virtual bool updateConfig(const ZmqServerConfig& config) = 0;

    // Message handling
    virtual bool sendMessage(const std::string& message, const std::string& clientId = "") = 0;
    virtual bool broadcastMessage(const std::string& message) = 0;
    virtual std::vector<ZmqMessage> getReceivedMessages() const = 0;
    virtual void clearReceivedMessages() = 0;

    // Client management
    virtual std::vector<std::string> getConnectedClients() const = 0;
    virtual size_t getClientCount() const = 0;
    virtual bool disconnectClient(const std::string& clientId) = 0;

    // Statistics and monitoring
    virtual ZmqServerStatistics getStatistics() const = 0;
    virtual void resetStatistics() = 0;

    // Health checking
    virtual bool isHealthy() const = 0;
    virtual std::string getHealthStatus() const = 0;

    // Socket configuration
    virtual bool setSocketOption(ZmqSocketOption option, int value) = 0;
    virtual int getSocketOption(ZmqSocketOption option) const = 0;

    // Message handlers
    virtual void setMessageHandler(ZmqMessageHandler handler) = 0;
    virtual void removeMessageHandler() = 0;

    // Connection handlers
    virtual void setConnectionHandler(ZmqConnectionHandler handler) = 0;
    virtual void removeConnectionHandler() = 0;
};

/**
 * @brief Factory for creating ZeroMQ server instances
 */
class ZmqServerFactory {
public:
    /**
     * @brief Create a ZeroMQ server with custom configuration
     */
    static std::unique_ptr<IZmqServer> createServer(const ZmqServerConfig& config);

    /**
     * @brief Create a ZeroMQ server with default configuration
     */
    static std::unique_ptr<IZmqServer> createServer(const std::string& bindAddress, ZmqSocketType socketType);
};

} // namespace zmq
} // namespace protocols
} // namespace server
} // namespace hydrogen

#pragma once

#include "device_communicator.h"
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>

namespace astrocomm {
namespace core {

/**
 * @brief Abstract base class for MQTT communication
 */
class MqttCommunicator {
public:
    using MessageHandler = std::function<void(const std::string& topic, const std::string& message)>;
    using ConnectionHandler = std::function<void(bool connected)>;

    MqttCommunicator(const MqttConfig& config);
    virtual ~MqttCommunicator();

    // Connection management
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Publishing
    virtual bool publish(const std::string& topic, const std::string& message, int qos = 1) = 0;
    virtual bool publish(const std::string& topic, const json& message, int qos = 1) = 0;

    // Subscription
    virtual bool subscribe(const std::string& topic, int qos = 1) = 0;
    virtual bool unsubscribe(const std::string& topic) = 0;

    // Event handlers
    void setMessageHandler(MessageHandler handler) { messageHandler_ = handler; }
    void setConnectionHandler(ConnectionHandler handler) { connectionHandler_ = handler; }

    // Configuration
    const MqttConfig& getConfig() const { return config_; }
    void updateConfig(const MqttConfig& config) { config_ = config; }

protected:
    MqttConfig config_;
    MessageHandler messageHandler_;
    ConnectionHandler connectionHandler_;
    std::atomic<bool> connected_{false};
};

/**
 * @brief Abstract base class for gRPC communication
 */
class GrpcCommunicator {
public:
    using StreamHandler = std::function<void(const std::string& data)>;
    using ErrorHandler = std::function<void(const std::string& error)>;

    GrpcCommunicator(const GrpcConfig& config);
    virtual ~GrpcCommunicator();

    // Connection management
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Unary RPC
    virtual CommunicationResponse sendUnaryRequest(const CommunicationMessage& message) = 0;

    // Streaming RPC
    virtual bool startClientStreaming(const std::string& method) = 0;
    virtual bool startServerStreaming(const std::string& method, const CommunicationMessage& request) = 0;
    virtual bool startBidirectionalStreaming(const std::string& method) = 0;

    virtual bool sendStreamMessage(const CommunicationMessage& message) = 0;
    virtual void finishStream() = 0;

    // Event handlers
    void setStreamHandler(StreamHandler handler) { streamHandler_ = handler; }
    void setErrorHandler(ErrorHandler handler) { errorHandler_ = handler; }

    // Configuration
    const GrpcConfig& getConfig() const { return config_; }
    void updateConfig(const GrpcConfig& config) { config_ = config; }

protected:
    GrpcConfig config_;
    StreamHandler streamHandler_;
    ErrorHandler errorHandler_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> streaming_{false};
};

/**
 * @brief Abstract base class for ZeroMQ communication
 */
class ZmqCommunicator {
public:
    enum class SocketType {
        REQ = 0,    // Request
        REP = 1,    // Reply
        PUB = 2,    // Publisher
        SUB = 3,    // Subscriber
        PUSH = 4,   // Push
        PULL = 5,   // Pull
        PAIR = 6    // Pair
    };

    using MessageHandler = std::function<void(const std::vector<std::string>& multipart)>;
    using ErrorHandler = std::function<void(const std::string& error)>;

    ZmqCommunicator(const ZmqConfig& config, SocketType socketType);
    virtual ~ZmqCommunicator();

    // Connection management
    virtual bool bind(const std::string& address) = 0;
    virtual bool connect(const std::string& address) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Message sending
    virtual bool send(const std::string& message, bool nonBlocking = false) = 0;
    virtual bool send(const std::vector<std::string>& multipart, bool nonBlocking = false) = 0;
    virtual bool send(const json& message, bool nonBlocking = false) = 0;

    // Message receiving
    virtual bool receive(std::string& message, bool nonBlocking = false) = 0;
    virtual bool receive(std::vector<std::string>& multipart, bool nonBlocking = false) = 0;

    // Subscription (for SUB sockets)
    virtual bool subscribe(const std::string& filter) = 0;
    virtual bool unsubscribe(const std::string& filter) = 0;

    // Event handlers
    void setMessageHandler(MessageHandler handler) { messageHandler_ = handler; }
    void setErrorHandler(ErrorHandler handler) { errorHandler_ = handler; }

    // Configuration
    const ZmqConfig& getConfig() const { return config_; }
    void updateConfig(const ZmqConfig& config) { config_ = config; }
    SocketType getSocketType() const { return socketType_; }

protected:
    ZmqConfig config_;
    SocketType socketType_;
    MessageHandler messageHandler_;
    ErrorHandler errorHandler_;
    std::atomic<bool> connected_{false};
};

/**
 * @brief Abstract base class for TCP communication
 */
class TcpCommunicator {
public:
    using MessageHandler = std::function<void(const std::string& message, const std::string& clientId)>;
    using ConnectionHandler = std::function<void(bool connected, const std::string& clientId)>;
    using ErrorHandler = std::function<void(const std::string& error)>;

    TcpCommunicator(const TcpConfig& config);
    virtual ~TcpCommunicator();

    // Connection management
    virtual bool start() = 0;  // Start server or connect as client
    virtual void stop() = 0;
    virtual bool isConnected() const = 0;

    // Communication
    virtual bool sendMessage(const std::string& message, const std::string& clientId = "") = 0;
    virtual bool sendMessage(const json& message, const std::string& clientId = "") = 0;
    virtual bool broadcastMessage(const std::string& message) = 0;  // Server mode only

    // Client management (server mode)
    virtual std::vector<std::string> getConnectedClients() const = 0;
    virtual bool disconnectClient(const std::string& clientId) = 0;

    // Event handlers
    void setMessageHandler(MessageHandler handler) { messageHandler_ = handler; }
    void setConnectionHandler(ConnectionHandler handler) { connectionHandler_ = handler; }
    void setErrorHandler(ErrorHandler handler) { errorHandler_ = handler; }

    // Configuration
    const TcpConfig& getConfig() const { return config_; }
    void updateConfig(const TcpConfig& config) { config_ = config; }

    // Statistics
    virtual size_t getConnectedClientCount() const = 0;
    virtual uint64_t getBytesSent() const = 0;
    virtual uint64_t getBytesReceived() const = 0;

protected:
    TcpConfig config_;
    MessageHandler messageHandler_;
    ConnectionHandler connectionHandler_;
    ErrorHandler errorHandler_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
};

/**
 * @brief Abstract base class for stdio communication
 */
class StdioCommunicator {
public:
    using MessageHandler = std::function<void(const std::string& message)>;
    using ErrorHandler = std::function<void(const std::string& error)>;

    StdioCommunicator(const StdioConfig& config);
    virtual ~StdioCommunicator();

    // Communication lifecycle
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isActive() const = 0;

    // Communication
    virtual bool sendMessage(const std::string& message) = 0;
    virtual bool sendMessage(const json& message) = 0;
    virtual std::string readLine() = 0;  // Blocking read
    virtual bool hasInput() const = 0;   // Non-blocking check

    // Event handlers
    void setMessageHandler(MessageHandler handler) { messageHandler_ = handler; }
    void setErrorHandler(ErrorHandler handler) { errorHandler_ = handler; }

    // Configuration
    const StdioConfig& getConfig() const { return config_; }
    void updateConfig(const StdioConfig& config) { config_ = config; }

    // Statistics
    virtual uint64_t getLinesSent() const = 0;
    virtual uint64_t getLinesReceived() const = 0;

protected:
    StdioConfig config_;
    MessageHandler messageHandler_;
    ErrorHandler errorHandler_;
    std::atomic<bool> active_{false};
    std::atomic<bool> running_{false};
};

/**
 * @brief Factory class for creating protocol communicators
 */
class ProtocolCommunicatorFactory {
public:
    static std::unique_ptr<MqttCommunicator> createMqttCommunicator(const MqttConfig& config);
    static std::unique_ptr<GrpcCommunicator> createGrpcCommunicator(const GrpcConfig& config);
    static std::unique_ptr<ZmqCommunicator> createZmqCommunicator(
        const ZmqConfig& config,
        ZmqCommunicator::SocketType socketType
    );
    static std::unique_ptr<TcpCommunicator> createTcpCommunicator(const TcpConfig& config);
    static std::unique_ptr<StdioCommunicator> createStdioCommunicator(const StdioConfig& config);
};

/**
 * @brief Enhanced device communicator with multi-protocol support
 */
class MultiProtocolDeviceCommunicator {
public:
    MultiProtocolDeviceCommunicator(const std::string& deviceId);
    ~MultiProtocolDeviceCommunicator();

    // Protocol management
    bool addProtocol(CommunicationProtocol protocol, const json& config);
    bool removeProtocol(CommunicationProtocol protocol);
    bool hasProtocol(CommunicationProtocol protocol) const;
    std::vector<CommunicationProtocol> getActiveProtocols() const;

    // Communication
    bool sendMessage(const CommunicationMessage& message, CommunicationProtocol protocol);
    bool broadcastMessage(const CommunicationMessage& message);

    // Event handling
    using MessageHandler = std::function<void(const CommunicationMessage& message, CommunicationProtocol protocol)>;
    using ConnectionHandler = std::function<void(CommunicationProtocol protocol, bool connected)>;
    
    void setMessageHandler(MessageHandler handler) { messageHandler_ = handler; }
    void setConnectionHandler(ConnectionHandler handler) { connectionHandler_ = handler; }

    // Status
    bool isConnected(CommunicationProtocol protocol) const;
    json getStatus() const;

private:
    std::string deviceId_;
    std::unordered_map<CommunicationProtocol, std::unique_ptr<MqttCommunicator>> mqttCommunicators_;
    std::unordered_map<CommunicationProtocol, std::unique_ptr<GrpcCommunicator>> grpcCommunicators_;
    std::unordered_map<CommunicationProtocol, std::unique_ptr<ZmqCommunicator>> zmqCommunicators_;
    std::unordered_map<CommunicationProtocol, std::unique_ptr<TcpCommunicator>> tcpCommunicators_;
    std::unordered_map<CommunicationProtocol, std::unique_ptr<StdioCommunicator>> stdioCommunicators_;

    MessageHandler messageHandler_;
    ConnectionHandler connectionHandler_;

    mutable std::mutex protocolsMutex_;
};

} // namespace core
} // namespace astrocomm

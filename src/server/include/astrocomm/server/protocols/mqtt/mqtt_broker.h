#pragma once

#include "astrocomm/core/communication_protocol.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace astrocomm {
namespace server {
namespace protocols {
namespace mqtt {

/**
 * @brief MQTT Quality of Service levels
 */
enum class MqttQoS {
    AT_MOST_ONCE = 0,
    AT_LEAST_ONCE = 1,
    EXACTLY_ONCE = 2
};

/**
 * @brief MQTT message structure
 */
struct MqttMessage {
    std::string id;
    std::string topic;
    std::string payload;
    MqttQoS qos = MqttQoS::AT_MOST_ONCE;
    bool retain = false;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> properties;
};

/**
 * @brief MQTT client information
 */
struct MqttClientInfo {
    std::string clientId;
    std::string username;
    std::string remoteAddress;
    int remotePort = 0;
    std::chrono::system_clock::time_point connectedAt;
    std::chrono::system_clock::time_point lastActivity;
    bool isConnected = false;
    std::unordered_map<std::string, std::string> properties;
};

/**
 * @brief MQTT topic subscription
 */
struct MqttSubscription {
    std::string clientId;
    std::string topic;
    MqttQoS qos = MqttQoS::AT_MOST_ONCE;
    std::chrono::system_clock::time_point subscribedAt;
    std::unordered_map<std::string, std::string> options;
};

/**
 * @brief MQTT broker configuration
 */
struct MqttBrokerConfig {
    std::string host = "localhost";
    int port = 1883;
    int maxClients = 1000;
    int keepAliveTimeout = 60;
    bool requireAuthentication = false;
    bool enableTLS = false;
    std::string certFile;
    std::string keyFile;
    std::string caFile;
    bool enableLogging = true;
    std::string logLevel = "INFO";
};

/**
 * @brief MQTT broker statistics
 */
struct MqttBrokerStatistics {
    size_t connectedClients = 0;
    size_t totalConnections = 0;
    size_t totalMessages = 0;
    size_t totalSubscriptions = 0;
    size_t totalBroadcast = 0;
    double messagesPerSecond = 0.0;
    int64_t uptime = 0;
    size_t bytesReceived = 0;
    size_t bytesSent = 0;
};

/**
 * @brief Interface for MQTT broker implementation
 */
class IMqttBroker {
public:
    virtual ~IMqttBroker() = default;

    // Server lifecycle
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool restart() = 0;
    virtual bool isRunning() const = 0;
    virtual bool isInitialized() const = 0;

    // Configuration
    virtual MqttBrokerConfig getConfig() const = 0;
    virtual bool updateConfig(const MqttBrokerConfig& config) = 0;

    // Client management
    virtual bool acceptClient(const std::string& clientId, const MqttClientInfo& clientInfo) = 0;
    virtual bool disconnectClient(const std::string& clientId) = 0;
    virtual std::vector<std::string> getConnectedClients() const = 0;
    virtual std::optional<MqttClientInfo> getClientInfo(const std::string& clientId) const = 0;
    virtual size_t getClientCount() const = 0;

    // Topic and subscription management
    virtual bool subscribe(const std::string& clientId, const std::string& topic, MqttQoS qos) = 0;
    virtual bool unsubscribe(const std::string& clientId, const std::string& topic) = 0;
    virtual std::vector<MqttSubscription> getSubscriptions(const std::string& clientId) const = 0;
    virtual std::vector<std::string> getTopicSubscribers(const std::string& topic) const = 0;

    // Message handling
    virtual bool publishMessage(const MqttMessage& message) = 0;
    virtual std::vector<MqttMessage> getRetainedMessages(const std::string& topicFilter = "") const = 0;
    virtual bool clearRetainedMessage(const std::string& topic) = 0;

    // Statistics and monitoring
    virtual MqttBrokerStatistics getStatistics() const = 0;
    virtual void resetStatistics() = 0;

    // Health checking
    virtual bool isHealthy() const = 0;
    virtual std::string getHealthStatus() const = 0;

    // Security and authentication
    virtual bool enableAuthentication(bool enabled) = 0;
    virtual bool setCredentials(const std::string& username, const std::string& password) = 0;
    virtual bool removeCredentials(const std::string& username) = 0;
    virtual bool validateCredentials(const std::string& username, const std::string& password) const = 0;
};

/**
 * @brief Factory for creating MQTT broker instances
 */
class MqttBrokerFactory {
public:
    /**
     * @brief Create an MQTT broker with custom configuration
     */
    static std::unique_ptr<IMqttBroker> createBroker(const MqttBrokerConfig& config);

    /**
     * @brief Create an MQTT broker with default configuration
     */
    static std::unique_ptr<IMqttBroker> createBroker(const std::string& host, int port);
};

} // namespace mqtt
} // namespace protocols
} // namespace server
} // namespace astrocomm

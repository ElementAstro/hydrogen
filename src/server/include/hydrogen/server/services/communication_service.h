#pragma once

#include "../core/service_registry.h"
#include "../core/server_interface.h"
#include "../core/protocol_handler.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <memory>
#include <queue>

namespace hydrogen {
namespace server {
namespace services {

// Forward declarations

/**
 * @brief Message priority levels
 */
enum class MessagePriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * @brief Message delivery status
 */
enum class DeliveryStatus {
    UNKNOWN,
    PENDING,
    SENT,
    DELIVERED,
    FAILED,
    EXPIRED,
    CANCELLED
};

/**
 * @brief Message request structure
 */
struct MessageRequest {
    std::string senderId;
    std::string recipientId;
    std::string content;
    std::string messageType;
    MessagePriority priority = MessagePriority::NORMAL;
    std::unordered_map<std::string, std::string> metadata;
    bool requestDeliveryReceipt = false;
};

/**
 * @brief Broadcast request structure
 */
struct BroadcastRequest {
    std::string senderId;
    std::vector<std::string> recipientIds;
    std::string content;
    std::string messageType;
    MessagePriority priority = MessagePriority::NORMAL;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Topic subscription structure
 */
struct TopicSubscription {
    std::string clientId;
    std::string topic;
    core::CommunicationProtocol protocol;
    std::chrono::system_clock::time_point subscribedAt;
    std::unordered_map<std::string, std::string> options;
};

/**
 * @brief Message status enumeration
 */
enum class MessageStatus {
    PENDING = 0,
    SENT = 1,
    DELIVERED = 2,
    READ = 3,
    FAILED = 4
};

/**
 * @brief Communication service message structure
 */
struct Message {
    std::string id;
    std::string senderId;
    std::string recipientId;
    std::string content;
    std::string messageType;
    MessagePriority priority = MessagePriority::NORMAL;
    MessageStatus status = MessageStatus::PENDING;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point sentAt;
    std::chrono::system_clock::time_point deliveredAt;
    std::chrono::system_clock::time_point readAt;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Message route structure
 */
struct MessageRoute {
    std::string routeId;
    std::string pattern;
    std::vector<std::string> targets;
    core::CommunicationProtocol protocol;
    std::unordered_map<std::string, std::string> options;
};



/**
 * @brief Message routing rule
 */
struct RoutingRule {
    std::string ruleId;
    std::string name;
    std::string description;
    core::MessageFilter filter;
    std::vector<core::CommunicationProtocol> targetProtocols;
    core::RoutingStrategy strategy;
    bool enabled;
    int priority;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point modifiedAt;
};

/**
 * @brief Message delivery receipt
 */
struct DeliveryReceipt {
    std::string messageId;
    std::string recipientId;
    core::CommunicationProtocol protocol;
    DeliveryStatus status;
    std::string errorMessage;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds deliveryTime;
};

/**
 * @brief Message statistics
 */
struct MessageStatistics {
    size_t totalSent;
    size_t totalReceived;
    size_t totalBroadcast;
    size_t totalDelivered;
    size_t totalFailed;
    std::unordered_map<core::CommunicationProtocol, size_t> sentByProtocol;
    std::unordered_map<core::CommunicationProtocol, size_t> receivedByProtocol;
    std::unordered_map<std::string, size_t> messagesByTopic;
    std::chrono::milliseconds averageDeliveryTime;
    std::chrono::system_clock::time_point lastReset;
};

/**
 * @brief Subscription information
 */
struct SubscriptionInfo {
    std::string subscriptionId;
    std::string clientId;
    std::string topic;
    core::CommunicationProtocol protocol;
    std::unordered_map<std::string, std::string> filters;
    bool isActive;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastActivity;
    size_t messagesReceived;
};

/**
 * @brief Communication service interface
 * 
 * Provides comprehensive message routing, protocol bridging, subscription management,
 * and communication statistics for multi-protocol server environments.
 */
class ICommunicationService : public virtual core::IService {
public:
    virtual ~ICommunicationService() = default;

    // Message routing
    virtual bool sendMessage(const core::Message& message) = 0;
    virtual bool sendMessageToProtocol(const core::Message& message, 
                                     core::CommunicationProtocol protocol) = 0;
    virtual bool broadcastMessage(const core::Message& message, 
                                const std::vector<core::CommunicationProtocol>& protocols = {}) = 0;
    virtual bool sendMessageToClient(const core::Message& message, 
                                   const std::string& clientId,
                                   core::CommunicationProtocol protocol) = 0;

    // Message queuing
    virtual std::string queueMessage(const core::Message& message, 
                                   MessagePriority priority = MessagePriority::NORMAL) = 0;
    virtual bool cancelQueuedMessage(const std::string& messageId) = 0;
    virtual std::vector<core::Message> getPendingMessages(const std::string& clientId = "") const = 0;
    virtual size_t getQueueSize(core::CommunicationProtocol protocol = static_cast<core::CommunicationProtocol>(-1)) const = 0;

    // Routing rules
    virtual bool addRoutingRule(const RoutingRule& rule) = 0;
    virtual bool updateRoutingRule(const RoutingRule& rule) = 0;
    virtual bool removeRoutingRule(const std::string& ruleId) = 0;
    virtual RoutingRule getRoutingRule(const std::string& ruleId) const = 0;
    virtual std::vector<RoutingRule> getAllRoutingRules() const = 0;
    virtual bool enableRoutingRule(const std::string& ruleId, bool enabled) = 0;

    // Protocol bridging
    virtual bool enableProtocolBridge(core::CommunicationProtocol sourceProtocol,
                                    core::CommunicationProtocol targetProtocol) = 0;
    virtual bool disableProtocolBridge(core::CommunicationProtocol sourceProtocol,
                                     core::CommunicationProtocol targetProtocol) = 0;
    virtual std::vector<std::pair<core::CommunicationProtocol, core::CommunicationProtocol>> 
            getActiveBridges() const = 0;

    // Subscription management
    virtual std::string subscribe(const std::string& clientId, const std::string& topic,
                                core::CommunicationProtocol protocol,
                                const std::unordered_map<std::string, std::string>& filters = {}) = 0;
    virtual bool unsubscribe(const std::string& subscriptionId) = 0;
    virtual bool unsubscribeClient(const std::string& clientId) = 0;
    virtual std::vector<SubscriptionInfo> getClientSubscriptions(const std::string& clientId) const = 0;
    virtual std::vector<SubscriptionInfo> getTopicSubscriptions(const std::string& topic) const = 0;
    virtual std::vector<SubscriptionInfo> getAllSubscriptions() const = 0;

    // Message filtering and transformation
    virtual bool addMessageFilter(const std::string& filterId,
                                const std::function<bool(const core::Message&)>& filter) = 0;
    virtual bool removeMessageFilter(const std::string& filterId) = 0;
    virtual bool addMessageTransformer(const std::string& transformerId,
                                     const std::function<core::Message(const core::Message&)>& transformer) = 0;
    virtual bool removeMessageTransformer(const std::string& transformerId) = 0;

    // Delivery tracking
    virtual std::vector<DeliveryReceipt> getDeliveryReceipts(const std::string& messageId = "") const = 0;
    virtual DeliveryStatus getMessageDeliveryStatus(const std::string& messageId) const = 0;
    virtual bool requestDeliveryReceipt(const std::string& messageId, bool enabled) = 0;

    // Statistics and monitoring
    virtual MessageStatistics getMessageStatistics() const = 0;
    virtual MessageStatistics getProtocolStatistics(core::CommunicationProtocol protocol) const = 0;
    virtual void resetStatistics() = 0;
    virtual std::unordered_map<std::string, size_t> getTopicStatistics() const = 0;
    virtual std::unordered_map<std::string, size_t> getClientStatistics() const = 0;

    // Performance monitoring
    virtual std::chrono::milliseconds getAverageLatency(core::CommunicationProtocol protocol) const = 0;
    virtual size_t getThroughput(core::CommunicationProtocol protocol) const = 0;
    virtual double getErrorRate(core::CommunicationProtocol protocol) const = 0;

    // Connection management
    virtual std::vector<core::ConnectionInfo> getActiveConnections() const = 0;
    virtual std::vector<core::ConnectionInfo> getProtocolConnections(core::CommunicationProtocol protocol) const = 0;
    virtual bool disconnectClient(const std::string& clientId, core::CommunicationProtocol protocol) = 0;
    virtual size_t getConnectionCount(core::CommunicationProtocol protocol = static_cast<core::CommunicationProtocol>(-1)) const = 0;

    // Message persistence
    virtual bool enableMessagePersistence(bool enabled) = 0;
    virtual bool isMessagePersistenceEnabled() const = 0;
    virtual std::vector<core::Message> getPersistedMessages(const std::string& topic = "",
                                                           size_t limit = 100) const = 0;
    virtual bool clearPersistedMessages(const std::string& topic = "") = 0;

    // Rate limiting
    virtual bool setRateLimit(const std::string& clientId, size_t messagesPerSecond) = 0;
    virtual bool removeRateLimit(const std::string& clientId) = 0;
    virtual size_t getRateLimit(const std::string& clientId) const = 0;
    virtual bool isRateLimited(const std::string& clientId) const = 0;

    // Event callbacks
    using MessageEventCallback = std::function<void(const core::Message&, const std::string& event)>;
    using RoutingEventCallback = std::function<void(const core::Message&, const std::vector<std::string>& targets)>;
    using DeliveryEventCallback = std::function<void(const DeliveryReceipt& receipt)>;
    using SubscriptionEventCallback = std::function<void(const SubscriptionInfo& subscription, const std::string& event)>;

    virtual void setMessageEventCallback(MessageEventCallback callback) = 0;
    virtual void setRoutingEventCallback(RoutingEventCallback callback) = 0;
    virtual void setDeliveryEventCallback(DeliveryEventCallback callback) = 0;
    virtual void setSubscriptionEventCallback(SubscriptionEventCallback callback) = 0;

    // Configuration
    virtual void setMaxQueueSize(size_t maxSize) = 0;
    virtual void setMessageTimeout(std::chrono::seconds timeout) = 0;
    virtual void setRetryAttempts(int maxRetries) = 0;
    virtual void setRetryDelay(std::chrono::milliseconds delay) = 0;

    // Protocol management
    virtual bool registerProtocol(core::CommunicationProtocol protocol,
                                std::shared_ptr<core::IServerInterface> server) = 0;
    virtual bool unregisterProtocol(core::CommunicationProtocol protocol) = 0;
    virtual std::vector<core::CommunicationProtocol> getRegisteredProtocols() const = 0;
    virtual bool isProtocolRegistered(core::CommunicationProtocol protocol) const = 0;
};

/**
 * @brief Communication service factory
 */
class CommunicationServiceFactory : public core::IServiceFactory {
public:
    std::unique_ptr<core::IService> createService(
        const std::string& serviceName,
        const std::unordered_map<std::string, std::string>& config = {}
    ) override;
    
    std::vector<std::string> getSupportedServices() const override;
    bool isServiceSupported(const std::string& serviceName) const override;
};

} // namespace services
} // namespace server
} // namespace hydrogen

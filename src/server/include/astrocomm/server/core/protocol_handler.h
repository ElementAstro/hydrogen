#pragma once

#include "server_interface.h"
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace astrocomm {
namespace server {
namespace core {

/**
 * @brief Message routing strategy enumeration
 */
enum class RoutingStrategy {
    BROADCAST,      // Send to all connected clients
    UNICAST,        // Send to specific client
    MULTICAST,      // Send to group of clients
    ROUND_ROBIN,    // Load balance across clients
    STICKY_SESSION  // Route based on session affinity
};

/**
 * @brief Message filter criteria
 */
struct MessageFilter {
    std::string topic;
    std::string clientId;
    CommunicationProtocol sourceProtocol;
    CommunicationProtocol targetProtocol;
    std::unordered_map<std::string, std::string> headerFilters;
};

/**
 * @brief Protocol-specific message handler interface
 * 
 * This interface defines how different protocols handle incoming
 * and outgoing messages, providing protocol-specific processing
 * while maintaining a common interface.
 */
class IProtocolHandler {
public:
    virtual ~IProtocolHandler() = default;

    // Protocol identification
    virtual CommunicationProtocol getProtocol() const = 0;
    virtual std::string getProtocolName() const = 0;
    virtual std::vector<std::string> getSupportedMessageTypes() const = 0;

    // Message processing
    virtual bool canHandle(const Message& message) const = 0;
    virtual bool processIncomingMessage(const Message& message) = 0;
    virtual bool processOutgoingMessage(Message& message) = 0;

    // Protocol-specific validation
    virtual bool validateMessage(const Message& message) const = 0;
    virtual std::string getValidationError(const Message& message) const = 0;

    // Message transformation
    virtual Message transformMessage(const Message& source, 
                                   CommunicationProtocol targetProtocol) const = 0;

    // Connection management
    virtual bool handleClientConnect(const ConnectionInfo& connection) = 0;
    virtual bool handleClientDisconnect(const std::string& clientId) = 0;

    // Configuration
    virtual void setProtocolConfig(const std::unordered_map<std::string, std::string>& config) = 0;
    virtual std::unordered_map<std::string, std::string> getProtocolConfig() const = 0;
};

/**
 * @brief Message router interface
 * 
 * Handles routing messages between different protocols and clients
 * based on configurable routing strategies and filters.
 */
class IMessageRouter {
public:
    virtual ~IMessageRouter() = default;

    // Routing configuration
    virtual void addRoute(const MessageFilter& filter, 
                         const std::vector<CommunicationProtocol>& targetProtocols,
                         RoutingStrategy strategy = RoutingStrategy::BROADCAST) = 0;
    virtual void removeRoute(const MessageFilter& filter) = 0;
    virtual void clearRoutes() = 0;

    // Message routing
    virtual bool routeMessage(const Message& message) = 0;
    virtual std::vector<CommunicationProtocol> findTargetProtocols(const Message& message) const = 0;
    virtual std::vector<std::string> findTargetClients(const Message& message, 
                                                       CommunicationProtocol protocol) const = 0;

    // Statistics
    virtual size_t getRoutedMessageCount() const = 0;
    virtual size_t getFailedRoutingCount() const = 0;
    virtual void resetStatistics() = 0;

    // Event callbacks
    using RoutingCallback = std::function<void(const Message&, const std::vector<std::string>&)>;
    using RoutingErrorCallback = std::function<void(const Message&, const std::string&)>;

    virtual void setRoutingCallback(RoutingCallback callback) = 0;
    virtual void setRoutingErrorCallback(RoutingErrorCallback callback) = 0;
};

/**
 * @brief Protocol handler registry
 * 
 * Manages registration and lookup of protocol handlers,
 * providing a centralized way to access protocol-specific functionality.
 */
class ProtocolHandlerRegistry {
public:
    static ProtocolHandlerRegistry& getInstance();

    // Handler registration
    void registerHandler(std::unique_ptr<IProtocolHandler> handler);
    void unregisterHandler(CommunicationProtocol protocol);
    
    // Handler lookup
    IProtocolHandler* getHandler(CommunicationProtocol protocol) const;
    std::vector<CommunicationProtocol> getRegisteredProtocols() const;
    bool isProtocolRegistered(CommunicationProtocol protocol) const;

    // Message processing
    bool processMessage(const Message& message);
    Message transformMessage(const Message& source, CommunicationProtocol targetProtocol) const;

    // Validation
    bool validateMessage(const Message& message, CommunicationProtocol protocol) const;
    std::string getValidationError(const Message& message, CommunicationProtocol protocol) const;

private:
    ProtocolHandlerRegistry() = default;
    ~ProtocolHandlerRegistry() = default;
    ProtocolHandlerRegistry(const ProtocolHandlerRegistry&) = delete;
    ProtocolHandlerRegistry& operator=(const ProtocolHandlerRegistry&) = delete;

    mutable std::mutex handlersMutex_;
    std::unordered_map<CommunicationProtocol, std::unique_ptr<IProtocolHandler>> handlers_;
};

/**
 * @brief Base implementation for protocol handlers
 * 
 * Provides common functionality that most protocol handlers will need,
 * reducing code duplication and ensuring consistent behavior.
 */
class BaseProtocolHandler : public IProtocolHandler {
public:
    explicit BaseProtocolHandler(CommunicationProtocol protocol);
    virtual ~BaseProtocolHandler() = default;

    // IProtocolHandler implementation
    CommunicationProtocol getProtocol() const override;
    std::string getProtocolName() const override;
    
    bool validateMessage(const Message& message) const override;
    std::string getValidationError(const Message& message) const override;

    void setProtocolConfig(const std::unordered_map<std::string, std::string>& config) override;
    std::unordered_map<std::string, std::string> getProtocolConfig() const override;

protected:
    // Helper methods for derived classes
    bool isValidClientId(const std::string& clientId) const;
    bool isValidTopic(const std::string& topic) const;
    bool isValidPayload(const std::string& payload) const;
    
    void logMessage(const std::string& level, const std::string& message) const;
    void updateStatistics(const std::string& operation, bool success);

private:
    CommunicationProtocol protocol_;
    std::unordered_map<std::string, std::string> config_;
    mutable std::mutex configMutex_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    std::unordered_map<std::string, size_t> operationCounts_;
    std::unordered_map<std::string, size_t> errorCounts_;
};

} // namespace core
} // namespace server
} // namespace astrocomm

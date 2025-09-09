#pragma once

#include "../../core/protocol_handler.h"
#include "../../core/server_interface.h"
#include <hydrogen/core/messaging/message.h>
#include <hydrogen/core/messaging/message_transformer.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

namespace hydrogen {
namespace server {
namespace protocols {
namespace stdio {

using namespace hydrogen::core;
using namespace hydrogen::server::core;

/**
 * @brief Stdio protocol handler configuration
 */
struct StdioProtocolConfig {
    bool enableLineBuffering = true;
    bool enableBinaryMode = false;
    std::string lineTerminator = "\n";
    bool enableEcho = false;
    bool enableFlush = true;
    std::string encoding = "utf-8";
    size_t maxMessageSize = 1024 * 1024; // 1MB
    size_t bufferSize = 4096;
    bool enableCompression = false;
    bool enableAuthentication = false;
    std::string authToken;
    int connectionTimeout = 30; // seconds
    bool enableHeartbeat = true;
    int heartbeatInterval = 10; // seconds
    bool enableMessageValidation = true;
    bool enableMessageLogging = true;
};

/**
 * @brief Stdio connection information
 */
struct StdioConnectionInfo {
    std::string clientId;
    std::string processId;
    std::chrono::system_clock::time_point connectedAt;
    std::chrono::system_clock::time_point lastActivity;
    bool isActive = false;
    bool isAuthenticated = false;
    std::unordered_map<std::string, std::string> metadata;
    uint64_t messagesSent = 0;
    uint64_t messagesReceived = 0;
    uint64_t bytesTransferred = 0;
};

/**
 * @brief Stdio protocol handler for server-side stdio communication
 * 
 * This handler manages stdio-based communication between clients and the server,
 * providing message processing, validation, transformation, and connection management
 * specifically for standard input/output streams.
 */
class StdioProtocolHandler : public IProtocolHandler {
public:
    using MessageCallback = std::function<void(const Message& message, const std::string& clientId)>;
    using ConnectionCallback = std::function<void(const std::string& clientId, bool connected)>;
    using ErrorCallback = std::function<void(const std::string& error, const std::string& clientId)>;

    explicit StdioProtocolHandler(const StdioProtocolConfig& config = StdioProtocolConfig{});
    ~StdioProtocolHandler() override;

    // IProtocolHandler implementation
    CommunicationProtocol getProtocol() const override;
    std::string getProtocolName() const override;
    std::vector<std::string> getSupportedMessageTypes() const override;

    bool canHandle(const Message& message) const override;
    bool processIncomingMessage(const Message& message) override;
    bool processOutgoingMessage(Message& message) override;

    bool validateMessage(const Message& message) const override;
    std::string getValidationError(const Message& message) const override;

    Message transformMessage(const Message& source, 
                           CommunicationProtocol targetProtocol) const override;

    bool handleClientConnect(const ConnectionInfo& connection) override;
    bool handleClientDisconnect(const std::string& clientId) override;

    void setProtocolConfig(const std::unordered_map<std::string, std::string>& config) override;
    std::unordered_map<std::string, std::string> getProtocolConfig() const override;

    // Stdio-specific methods
    void setMessageCallback(MessageCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    void setErrorCallback(ErrorCallback callback);

    // Connection management
    bool isClientConnected(const std::string& clientId) const;
    std::vector<std::string> getConnectedClients() const;
    StdioConnectionInfo getConnectionInfo(const std::string& clientId) const;

    // Message handling
    bool sendMessage(const std::string& clientId, const Message& message);
    bool broadcastMessage(const Message& message);

    // Statistics
    uint64_t getTotalMessagesProcessed() const;
    uint64_t getTotalBytesTransferred() const;
    std::unordered_map<std::string, uint64_t> getClientStatistics(const std::string& clientId) const;

    // Configuration
    void updateConfig(const StdioProtocolConfig& config);
    StdioProtocolConfig getConfig() const;

private:
    StdioProtocolConfig config_;
    std::unique_ptr<MessageTransformer> messageTransformer_;
    
    // Connection tracking
    mutable std::mutex connectionsMutex_;
    std::unordered_map<std::string, std::unique_ptr<StdioConnectionInfo>> connections_;
    
    // Statistics
    std::atomic<uint64_t> totalMessagesProcessed_{0};
    std::atomic<uint64_t> totalBytesTransferred_{0};
    
    // Callbacks
    MessageCallback messageCallback_;
    ConnectionCallback connectionCallback_;
    ErrorCallback errorCallback_;
    
    // Helper methods
    std::string generateClientId() const;
    bool validateMessageSize(const Message& message) const;
    bool validateMessageFormat(const json& messageJson) const;
    bool authenticateClient(const std::string& clientId, const std::string& token) const;
    void updateConnectionActivity(const std::string& clientId);
    void cleanupInactiveConnections();
    
    // Message processing helpers
    json serializeMessage(const Message& message) const;
    std::unique_ptr<Message> deserializeMessage(const json& messageJson) const;
    bool applyMessageFilters(const Message& message) const;
    
    // Logging helpers
    void logMessage(const std::string& direction, const Message& message, const std::string& clientId) const;
    void logError(const std::string& error, const std::string& clientId = "") const;
};

} // namespace stdio
} // namespace protocols
} // namespace server
} // namespace hydrogen

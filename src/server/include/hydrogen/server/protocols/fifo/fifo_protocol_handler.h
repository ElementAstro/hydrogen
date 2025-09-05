#pragma once

#include "hydrogen/server/core/protocol_handler.h"
#include "hydrogen/core/fifo_config_manager.h"
#include "hydrogen/core/fifo_communicator.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <unordered_map>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <chrono>

namespace hydrogen {
namespace server {
namespace protocols {
namespace fifo {

using json = nlohmann::json;
using namespace hydrogen::core;

/**
 * @brief FIFO client connection information
 */
struct FifoClientInfo {
    std::string clientId;
    std::string pipePath;
    std::unique_ptr<FifoCommunicator> communicator;
    std::chrono::system_clock::time_point connectedAt;
    std::chrono::system_clock::time_point lastActivity;
    std::atomic<bool> active{true};
    std::atomic<uint64_t> messagesSent{0};
    std::atomic<uint64_t> messagesReceived{0};
    
    FifoClientInfo(const std::string& id, const std::string& path) 
        : clientId(id), pipePath(path), 
          connectedAt(std::chrono::system_clock::now()),
          lastActivity(std::chrono::system_clock::now()) {}
};

/**
 * @brief FIFO protocol handler configuration
 */
struct FifoProtocolConfig {
    // Basic configuration
    std::string basePipePath = "/tmp/hydrogen_fifo";
    std::string windowsBasePipePath = "\\\\.\\pipe\\hydrogen_fifo";
    int maxConcurrentClients = 10;
    bool enableClientAuthentication = false;
    bool enableMessageValidation = true;
    bool enableMessageLogging = false;
    
    // Message handling
    size_t maxMessageSize = 1024 * 1024; // 1MB
    size_t maxQueueSize = 1000;
    std::chrono::milliseconds messageTimeout{5000};
    std::chrono::milliseconds clientTimeout{30000};
    
    // Connection management
    bool enableAutoCleanup = true;
    std::chrono::milliseconds cleanupInterval{60000};
    bool enableKeepAlive = true;
    std::chrono::milliseconds keepAliveInterval{30000};
    
    // Security
    std::vector<std::string> allowedCommands;
    bool enableCommandFiltering = false;
    std::string authToken = "";
    
    // Performance
    bool enableNonBlocking = true;
    bool enableBidirectional = true;
    int workerThreadCount = 2;
    
    // Serialization support
    json toJson() const;
    void fromJson(const json& j);
};

/**
 * @brief FIFO protocol handler statistics
 */
struct FifoProtocolStats {
    std::atomic<uint64_t> totalClientsConnected{0};
    std::atomic<uint64_t> currentActiveClients{0};
    std::atomic<uint64_t> totalMessagesProcessed{0};
    std::atomic<uint64_t> totalBytesTransferred{0};
    std::atomic<uint64_t> totalErrors{0};
    std::chrono::system_clock::time_point startTime;
    
    FifoProtocolStats() : startTime(std::chrono::system_clock::now()) {}
    
    double getMessagesPerSecond() const;
    double getBytesPerSecond() const;
    json toJson() const;
};

/**
 * @brief FIFO protocol handler implementation
 */
class FifoProtocolHandler : public IProtocolHandler {
public:
    explicit FifoProtocolHandler(const FifoProtocolConfig& config = FifoProtocolConfig{});
    ~FifoProtocolHandler() override;
    
    // IProtocolHandler implementation
    bool initialize() override;
    void shutdown() override;
    bool handleMessage(const Message& message, const std::string& clientId) override;
    bool sendMessage(const Message& message, const std::string& clientId) override;
    bool broadcastMessage(const Message& message) override;
    std::vector<std::string> getConnectedClients() const override;
    bool isClientConnected(const std::string& clientId) const override;
    bool disconnectClient(const std::string& clientId) override;
    CommunicationProtocol getProtocol() const override { return CommunicationProtocol::FIFO; }
    
    // FIFO-specific methods
    bool acceptClient(const std::string& clientId, const std::string& command = "");
    bool createClientPipe(const std::string& clientId);
    std::string generateClientPipePath(const std::string& clientId) const;
    
    // Configuration management
    void setConfig(const FifoProtocolConfig& config) { config_ = config; }
    const FifoProtocolConfig& getConfig() const { return config_; }
    
    // Statistics and monitoring
    FifoProtocolStats getStatistics() const;
    bool isHealthy() const;
    std::string getHealthStatus() const;
    
    // Message validation and processing
    bool validateMessage(const Message& message) const;
    std::string getValidationError(const Message& message) const;
    bool validateMessageSize(const Message& message) const;
    
    // Client management
    void cleanupInactiveClients();
    void sendKeepAliveMessages();
    std::vector<std::string> getInactiveClients() const;
    
    // Advanced features
    bool enableMultiplexing();
    bool enableCompression();
    bool enableEncryption();
    
protected:
    // Message processing
    virtual void processIncomingMessage(const std::string& clientId, const std::string& message);
    virtual void processOutgoingMessage(const std::string& clientId, const Message& message);
    
    // Client lifecycle
    virtual void onClientConnected(const std::string& clientId);
    virtual void onClientDisconnected(const std::string& clientId);
    virtual void onClientError(const std::string& clientId, const std::string& error);
    
    // Message serialization
    virtual json serializeMessage(const Message& message) const;
    virtual std::unique_ptr<Message> deserializeMessage(const json& messageJson) const;
    
    // Validation helpers
    virtual bool validateMessageFormat(const json& messageJson) const;
    virtual bool validateClientCommand(const std::string& command) const;
    virtual bool authenticateClient(const std::string& clientId, const std::string& token) const;
    
    // Logging and debugging
    virtual void logMessage(const std::string& direction, const Message& message, const std::string& clientId) const;
    virtual void logError(const std::string& error) const;
    virtual void logDebug(const std::string& message) const;
    
private:
    FifoProtocolConfig config_;
    mutable std::mutex clientsMutex_;
    std::unordered_map<std::string, std::unique_ptr<FifoClientInfo>> clients_;
    
    // Threading
    std::unique_ptr<std::thread> cleanupThread_;
    std::unique_ptr<std::thread> keepAliveThread_;
    std::vector<std::unique_ptr<std::thread>> workerThreads_;
    std::atomic<bool> running_{false};
    
    // Message queues
    std::queue<std::pair<std::string, Message>> incomingMessages_;
    std::queue<std::pair<std::string, Message>> outgoingMessages_;
    mutable std::mutex incomingMutex_;
    mutable std::mutex outgoingMutex_;
    std::condition_variable incomingCondition_;
    std::condition_variable outgoingCondition_;
    
    // Statistics
    mutable FifoProtocolStats statistics_;
    mutable std::mutex statisticsMutex_;
    
    // Threading methods
    void cleanupThreadFunction();
    void keepAliveThreadFunction();
    void workerThreadFunction();
    
    // Message queue management
    void queueIncomingMessage(const std::string& clientId, const Message& message);
    void queueOutgoingMessage(const std::string& clientId, const Message& message);
    std::pair<std::string, Message> dequeueIncomingMessage();
    std::pair<std::string, Message> dequeueOutgoingMessage();
    
    // Client management helpers
    void removeClient(const std::string& clientId);
    void updateClientActivity(const std::string& clientId);
    bool isClientActive(const std::string& clientId) const;
    
    // Statistics updates
    void updateStatistics(bool sent, size_t bytes);
    void incrementErrorCount();
    
    // Cleanup and shutdown
    void stopThreads();
    void clearQueues();
    void disconnectAllClients();
    
    // Platform-specific helpers
    std::string getPlatformSpecificPipePath(const std::string& basePath) const;
    bool createPipeDirectory(const std::string& path) const;
    bool setPipePermissions(const std::string& path) const;
};

/**
 * @brief FIFO protocol handler factory
 */
class FifoProtocolHandlerFactory {
public:
    static std::unique_ptr<FifoProtocolHandler> create(const FifoProtocolConfig& config = FifoProtocolConfig{});
    static std::unique_ptr<FifoProtocolHandler> createDefault();
    static std::unique_ptr<FifoProtocolHandler> createHighPerformance();
    static std::unique_ptr<FifoProtocolHandler> createSecure();
    static std::unique_ptr<FifoProtocolHandler> createDebug();
    
    // Platform-specific factories
    static std::unique_ptr<FifoProtocolHandler> createForWindows(const FifoProtocolConfig& config = FifoProtocolConfig{});
    static std::unique_ptr<FifoProtocolHandler> createForUnix(const FifoProtocolConfig& config = FifoProtocolConfig{});
};

} // namespace fifo
} // namespace protocols
} // namespace server
} // namespace hydrogen

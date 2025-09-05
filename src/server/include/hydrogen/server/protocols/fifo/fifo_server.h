#pragma once

#include "fifo_protocol_handler.h"
#include "hydrogen/server/core/server_interface.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>

namespace hydrogen {
namespace server {
namespace protocols {
namespace fifo {

using json = nlohmann::json;

/**
 * @brief FIFO server status enumeration
 */
enum class ServerStatus {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING,
    ERROR
};

/**
 * @brief FIFO server configuration
 */
struct FifoServerConfig {
    // Server identification
    std::string serverName = "HydrogenFifoServer";
    std::string serverVersion = "1.0.0";
    std::string serverId = "fifo_server_001";
    
    // Connection settings
    int maxConcurrentClients = 50;
    std::chrono::milliseconds clientTimeout{30000};
    std::chrono::milliseconds serverTimeout{60000};
    
    // Protocol configuration
    FifoProtocolConfig protocolConfig;
    
    // Security settings
    bool enableCommandFiltering = false;
    std::vector<std::string> allowedCommands = {"ping", "echo", "status", "help"};
    bool enableClientAuthentication = false;
    std::string authToken = "";
    
    // Performance settings
    int workerThreadCount = 4;
    bool enableAutoCleanup = true;
    std::chrono::milliseconds cleanupInterval{60000};
    bool enableHealthChecking = true;
    std::chrono::milliseconds healthCheckInterval{10000};
    
    // Logging and monitoring
    bool enableServerLogging = true;
    bool enablePerformanceMetrics = false;
    bool enableDebugMode = false;
    std::string logLevel = "INFO";
    
    // Serialization support
    json toJson() const;
    void fromJson(const json& j);
    bool validate() const;
};

/**
 * @brief FIFO server statistics
 */
struct FifoServerStatistics {
    std::atomic<uint64_t> totalClientsConnected{0};
    std::atomic<uint64_t> currentActiveClients{0};
    std::atomic<uint64_t> totalMessagesProcessed{0};
    std::atomic<uint64_t> totalBytesTransferred{0};
    std::atomic<uint64_t> totalErrors{0};
    std::atomic<uint64_t> totalCommandsExecuted{0};
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point lastActivity;
    
    FifoServerStatistics() : startTime(std::chrono::system_clock::now()),
                            lastActivity(std::chrono::system_clock::now()) {}
    
    double getMessagesPerSecond() const;
    double getBytesPerSecond() const;
    std::chrono::milliseconds getUptime() const;
    json toJson() const;
};

/**
 * @brief FIFO server implementation
 */
class FifoServer {
public:
    // Callback types
    using ClientConnectedCallback = std::function<void(const std::string& clientId)>;
    using ClientDisconnectedCallback = std::function<void(const std::string& clientId)>;
    using MessageReceivedCallback = std::function<void(const std::string& clientId, const Message& message)>;
    using ErrorCallback = std::function<void(const std::string& error, const std::string& clientId)>;
    
    explicit FifoServer(const FifoServerConfig& config = FifoServerConfig{});
    ~FifoServer();
    
    // Server lifecycle
    bool start();
    void stop();
    bool restart();
    ServerStatus getStatus() const;
    bool isRunning() const;
    
    // Client management
    bool acceptClient(const std::string& clientId, const std::string& command = "");
    bool disconnectClient(const std::string& clientId);
    bool isClientConnected(const std::string& clientId) const;
    std::vector<std::string> getConnectedClients() const;
    
    // Message handling
    bool sendMessageToClient(const std::string& clientId, const Message& message);
    bool broadcastMessage(const Message& message);
    bool sendResponse(const std::string& clientId, const std::string& response, const std::string& originalMessageId = "");
    
    // Event callbacks
    void setClientConnectedCallback(ClientConnectedCallback callback) { clientConnectedCallback_ = callback; }
    void setClientDisconnectedCallback(ClientDisconnectedCallback callback) { clientDisconnectedCallback_ = callback; }
    void setMessageReceivedCallback(MessageReceivedCallback callback) { messageReceivedCallback_ = callback; }
    void setErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }
    
    // Configuration management
    void setServerConfig(const FifoServerConfig& config) { config_ = config; }
    const FifoServerConfig& getServerConfig() const { return config_; }
    bool updateConfig(const json& configUpdates);
    
    // Statistics and monitoring
    FifoServerStatistics getStatistics() const;
    bool isHealthy() const;
    std::string getHealthStatus() const;
    json getServerInfo() const;
    
    // Advanced features
    bool enableMultiplexing();
    bool enableCompression();
    bool enableEncryption();
    
    // Command processing
    bool processCommand(const std::string& clientId, const std::string& command, const std::string& args = "");
    std::vector<std::string> getSupportedCommands() const;
    bool registerCommand(const std::string& command, std::function<std::string(const std::string&, const std::string&)> handler);
    
protected:
    // Server lifecycle management
    virtual bool initializeServer();
    virtual void shutdownServer();
    virtual bool startProtocolHandler();
    virtual void stopProtocolHandler();
    
    // Client event handlers
    virtual void onClientConnected(const std::string& clientId);
    virtual void onClientDisconnected(const std::string& clientId);
    virtual void onMessageReceived(const std::string& clientId, const Message& message);
    virtual void onError(const std::string& error, const std::string& clientId = "");
    
    // Command handlers
    virtual std::string handlePingCommand(const std::string& clientId, const std::string& args);
    virtual std::string handleEchoCommand(const std::string& clientId, const std::string& args);
    virtual std::string handleStatusCommand(const std::string& clientId, const std::string& args);
    virtual std::string handleHelpCommand(const std::string& clientId, const std::string& args);
    
    // Health and monitoring
    virtual bool performHealthCheck() const;
    virtual void updateStatistics(bool sent, size_t bytes);
    virtual void incrementErrorCount();
    
private:
    FifoServerConfig config_;
    std::atomic<ServerStatus> status_{ServerStatus::STOPPED};
    std::unique_ptr<FifoProtocolHandler> protocolHandler_;
    
    // Threading
    std::unique_ptr<std::thread> healthCheckThread_;
    std::unique_ptr<std::thread> cleanupThread_;
    std::atomic<bool> running_{false};
    
    // Callbacks
    ClientConnectedCallback clientConnectedCallback_;
    ClientDisconnectedCallback clientDisconnectedCallback_;
    MessageReceivedCallback messageReceivedCallback_;
    ErrorCallback errorCallback_;
    
    // Command handlers
    std::unordered_map<std::string, std::function<std::string(const std::string&, const std::string&)>> commandHandlers_;
    
    // Statistics
    mutable FifoServerStatistics statistics_;
    mutable std::mutex statisticsMutex_;
    
    // Threading methods
    void healthCheckThreadFunction();
    void cleanupThreadFunction();
    
    // Initialization helpers
    void initializeCommandHandlers();
    void setupProtocolHandlerCallbacks();
    
    // Cleanup
    void stopThreads();
    void cleanup();
    
    // Utility methods
    std::string generateMessageId() const;
    std::string getCurrentTimestamp() const;
    void updateLastActivity();
};

/**
 * @brief FIFO server factory for creating configured server instances
 */
class FifoServerFactory {
public:
    // Basic factory methods
    static std::unique_ptr<FifoServer> createDefault();
    static std::unique_ptr<FifoServer> createWithConfig(const FifoServerConfig& config);
    
    // Preset configurations
    static std::unique_ptr<FifoServer> createHighPerformance();
    static std::unique_ptr<FifoServer> createSecure();
    static std::unique_ptr<FifoServer> createDebug();
    static std::unique_ptr<FifoServer> createEmbedded();
    
    // Configuration presets
    static FifoServerConfig createDefaultConfig();
    static FifoServerConfig createHighPerformanceConfig();
    static FifoServerConfig createSecureConfig();
    static FifoServerConfig createDebugConfig();
    static FifoServerConfig createEmbeddedConfig();
    
    // Platform-specific factories
    static std::unique_ptr<FifoServer> createForWindows(const FifoServerConfig& config = FifoServerConfig{});
    static std::unique_ptr<FifoServer> createForUnix(const FifoServerConfig& config = FifoServerConfig{});
    
    // Specialized servers
    static std::unique_ptr<FifoServer> createBroadcastServer(const FifoServerConfig& config = FifoServerConfig{});
    static std::unique_ptr<FifoServer> createInteractiveServer(const FifoServerConfig& config = FifoServerConfig{});
};

/**
 * @brief FIFO server utilities
 */
class FifoServerUtils {
public:
    // Configuration utilities
    static bool validateServerConfig(const FifoServerConfig& config);
    static FifoServerConfig optimizeConfig(const FifoServerConfig& config);
    static json getConfigSchema();
    
    // Server management utilities
    static bool isServerRunning(const std::string& serverName);
    static std::vector<std::string> listRunningServers();
    static bool stopServer(const std::string& serverName);
    
    // Performance utilities
    static json benchmarkServer(FifoServer& server, int numClients = 10, int messagesPerClient = 100);
    static json profileServer(FifoServer& server, std::chrono::milliseconds duration = std::chrono::milliseconds(60000));
    
    // Diagnostic utilities
    static json diagnoseServer(const FifoServer& server);
    static std::string generateServerReport(const FifoServer& server);
    static bool testServerConnectivity(const FifoServer& server);
};

} // namespace fifo
} // namespace protocols
} // namespace server
} // namespace hydrogen

#pragma once

#include "stdio_protocol_handler.h"
#include "../../core/server_interface.h"
#include <hydrogen/core/stdio_config_manager.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

namespace hydrogen {
namespace server {
namespace protocols {
namespace stdio {

using namespace hydrogen::core;
using namespace hydrogen::server::core;

/**
 * @brief Stdio server implementation
 * 
 * This class provides a complete stdio-based server that can accept
 * connections from stdio-based clients and handle message processing
 * through the stdio protocol handler.
 */
class StdioServer : public core::IServerInterface {
public:
    /**
     * @brief Server configuration structure
     */
    struct ServerConfig {
        StdioProtocolConfig protocolConfig;
        std::string serverName = "StdioServer";
        int maxConcurrentClients = 100;
        bool enableClientIsolation = true;
        std::string workingDirectory = ".";
        std::vector<std::string> allowedCommands;
        bool enableCommandFiltering = false;
        std::chrono::milliseconds clientTimeout{300000}; // 5 minutes
        bool enableAutoCleanup = true;
        std::chrono::milliseconds cleanupInterval{60000}; // 1 minute
    };

    explicit StdioServer(const ServerConfig& config = ServerConfig{});
    ~StdioServer() override;

    // IServer implementation
    bool start() override;
    void stop() override;
    ServerStatus getStatus() const override;
    std::string getServerInfo() const override;

    // Stdio-specific methods
    void setServerConfig(const ServerConfig& config);
    ServerConfig getServerConfig() const;

    // Client management
    bool acceptClient(const std::string& clientId, const std::string& command = "");
    bool disconnectClient(const std::string& clientId);
    std::vector<std::string> getConnectedClients() const;
    bool isClientConnected(const std::string& clientId) const;

    // Message handling
    bool sendMessageToClient(const std::string& clientId, const Message& message);
    bool broadcastMessage(const Message& message);

    // Event callbacks
    using ClientConnectedCallback = std::function<void(const std::string& clientId)>;
    using ClientDisconnectedCallback = std::function<void(const std::string& clientId)>;
    using MessageReceivedCallback = std::function<void(const std::string& clientId, const Message& message)>;
    using ErrorCallback = std::function<void(const std::string& error, const std::string& clientId)>;

    void setClientConnectedCallback(ClientConnectedCallback callback);
    void setClientDisconnectedCallback(ClientDisconnectedCallback callback);
    void setMessageReceivedCallback(MessageReceivedCallback callback);
    void setErrorCallback(ErrorCallback callback);

    // Statistics and monitoring
    struct ServerStatistics {
        uint64_t totalClientsConnected = 0;
        uint64_t currentActiveClients = 0;
        uint64_t totalMessagesProcessed = 0;
        uint64_t totalBytesTransferred = 0;
        std::chrono::system_clock::time_point serverStartTime;
        std::chrono::milliseconds uptime{0};
    };

    ServerStatistics getStatistics() const;
    void resetStatistics();

    // Health monitoring
    bool isHealthy() const;
    std::string getHealthStatus() const;

private:
    ServerConfig config_;
    std::unique_ptr<StdioProtocolHandler> protocolHandler_;
    
    // Server state
    std::atomic<ServerStatus> status_{ServerStatus::STOPPED};
    std::chrono::system_clock::time_point startTime_;
    
    // Client management
    mutable std::mutex clientsMutex_;
    std::unordered_map<std::string, std::unique_ptr<StdioConnectionInfo>> clients_;
    
    // Background threads
    std::thread acceptorThread_;
    std::thread cleanupThread_;
    std::atomic<bool> running_{false};
    std::condition_variable cleanupCondition_;
    std::mutex cleanupMutex_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    ServerStatistics statistics_;
    
    // Callbacks
    ClientConnectedCallback clientConnectedCallback_;
    ClientDisconnectedCallback clientDisconnectedCallback_;
    MessageReceivedCallback messageReceivedCallback_;
    ErrorCallback errorCallback_;
    
    // Helper methods
    void acceptorLoop();
    void cleanupLoop();
    void handleClientConnection(const std::string& clientId);
    void handleClientDisconnection(const std::string& clientId);
    void handleMessageReceived(const Message& message, const std::string& clientId);
    void handleError(const std::string& error, const std::string& clientId = "");
    
    void updateStatistics();
    void cleanupInactiveClients();
    bool validateCommand(const std::string& command) const;
    std::string generateClientId() const;
    
    // Logging helpers
    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;
    void logDebug(const std::string& message) const;
};

/**
 * @brief Stdio server factory
 */
class StdioServerFactory {
public:
    static std::unique_ptr<StdioServer> createDefault();
    static std::unique_ptr<StdioServer> createWithConfig(const StdioServer::ServerConfig& config);
    static std::unique_ptr<StdioServer> createFromConfigFile(const std::string& configFile);
    
    // Preset configurations
    static StdioServer::ServerConfig createDefaultConfig();
    static StdioServer::ServerConfig createHighPerformanceConfig();
    static StdioServer::ServerConfig createSecureConfig();
    static StdioServer::ServerConfig createDebugConfig();
};

/**
 * @brief Multi-client stdio server
 * 
 * This class extends the basic stdio server to handle multiple concurrent
 * stdio-based clients, each potentially running in separate processes.
 */
class MultiClientStdioServer : public StdioServer {
public:
    struct MultiClientConfig : public ServerConfig {
        bool enableProcessIsolation = true;
        std::string clientExecutable;
        std::vector<std::string> clientArguments;
        std::unordered_map<std::string, std::string> clientEnvironment;
        int maxProcesses = 10;
        std::chrono::milliseconds processStartupTimeout{10000};
        bool enableProcessMonitoring = true;
    };

    explicit MultiClientStdioServer(const MultiClientConfig& config);
    ~MultiClientStdioServer() override;

    // Process management
    bool startClientProcess(const std::string& clientId);
    bool stopClientProcess(const std::string& clientId);
    std::vector<std::string> getRunningProcesses() const;
    
    // Process monitoring
    bool isProcessRunning(const std::string& clientId) const;
    int getProcessId(const std::string& clientId) const;
    std::string getProcessStatus(const std::string& clientId) const;

private:
    MultiClientConfig multiClientConfig_;
    
    struct ProcessInfo {
        std::string clientId;
        int processId = -1;
        std::chrono::system_clock::time_point startTime;
        bool isRunning = false;
        std::string status;
    };
    
    mutable std::mutex processesMutex_;
    std::unordered_map<std::string, std::unique_ptr<ProcessInfo>> processes_;
    
    std::thread processMonitorThread_;
    
    void processMonitorLoop();
    void handleProcessExit(const std::string& clientId, int exitCode);
    bool launchProcess(const std::string& clientId);
    void terminateProcess(const std::string& clientId);
};

} // namespace stdio
} // namespace protocols
} // namespace server
} // namespace hydrogen

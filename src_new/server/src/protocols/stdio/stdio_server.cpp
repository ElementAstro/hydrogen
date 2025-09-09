#include "hydrogen/server/protocols/stdio/stdio_server.h"
#include "hydrogen/server/core/server_interface.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <random>

namespace hydrogen {
namespace server {
namespace protocols {
namespace stdio {

namespace core = hydrogen::server::core;

// Helper function to convert core::StdioConfig to StdioProtocolConfig
StdioProtocolConfig convertToProtocolConfig(const hydrogen::core::StdioConfig& coreConfig) {
    StdioProtocolConfig protocolConfig;
    protocolConfig.enableLineBuffering = coreConfig.enableLineBuffering;
    protocolConfig.enableBinaryMode = coreConfig.enableBinaryMode;
    protocolConfig.lineTerminator = coreConfig.lineTerminator;
    protocolConfig.enableEcho = coreConfig.enableEcho;
    protocolConfig.enableFlush = coreConfig.enableFlush;
    protocolConfig.encoding = coreConfig.encoding;
    protocolConfig.bufferSize = coreConfig.bufferSize;
    protocolConfig.connectionTimeout = static_cast<int>(coreConfig.readTimeout.count() / 1000); // Convert ms to seconds
    // Set other fields to defaults since core config doesn't have all fields
    return protocolConfig;
}

StdioServer::StdioServer(const ServerConfig& config) : config_(config) {
    protocolHandler_ = std::make_unique<StdioProtocolHandler>(config_.protocolConfig);
    
    // Set up protocol handler callbacks
    protocolHandler_->setMessageCallback([this](const Message& message, const std::string& clientId) {
        handleMessageReceived(message, clientId);
    });
    
    protocolHandler_->setConnectionCallback([this](const std::string& clientId, bool connected) {
        if (connected) {
            handleClientConnection(clientId);
        } else {
            handleClientDisconnection(clientId);
        }
    });
    
    protocolHandler_->setErrorCallback([this](const std::string& error, const std::string& clientId) {
        handleError(error, clientId);
    });
    
    logInfo("StdioServer initialized with name: " + config_.serverName);
}

StdioServer::~StdioServer() {
    stop();
    logInfo("StdioServer destroyed");
}

bool StdioServer::start() {
    if (status_.load() == core::ServerStatus::RUNNING) {
        logInfo("Server is already running");
        return true;
    }

    try {
        status_.store(core::ServerStatus::STARTING);
        running_.store(true);
        startTime_ = std::chrono::system_clock::now();

        // Start background threads
        acceptorThread_ = std::thread(&StdioServer::acceptorLoop, this);

        if (config_.enableAutoCleanup) {
            cleanupThread_ = std::thread(&StdioServer::cleanupLoop, this);
        }

        status_.store(core::ServerStatus::RUNNING);
        statistics_.serverStartTime = startTime_;
        
        logInfo("StdioServer started successfully");
        return true;
        
    } catch (const std::exception& e) {
        status_.store(static_cast<core::ServerStatus>(4)); // ERROR = 4
        handleError("Failed to start server: " + std::string(e.what()));
        return false;
    }
}

bool StdioServer::stop() {
    if (status_.load() == core::ServerStatus::STOPPED) {
        return true;
    }

    status_.store(core::ServerStatus::STOPPING);
    running_.store(false);
    
    // Wake up cleanup thread
    cleanupCondition_.notify_all();
    
    // Join threads
    if (acceptorThread_.joinable()) {
        acceptorThread_.join();
    }
    
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
    
    // Disconnect all clients
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const auto& [clientId, client] : clients_) {
            protocolHandler_->handleClientDisconnect(clientId);
        }
        clients_.clear();
    }
    
    status_.store(core::ServerStatus::STOPPED);
    logInfo("StdioServer stopped");
    return true;
}

core::ServerStatus StdioServer::getStatus() const {
    return status_.load();
}

bool StdioServer::restart() {
    if (!stop()) {
        return false;
    }
    return start();
}

void StdioServer::setConfig(const core::ServerConfig& config) {
    // Convert core::ServerConfig to local ServerConfig
    ServerConfig localConfig;
    localConfig.serverName = config.name;
    localConfig.maxConcurrentClients = config.maxConnections;
    // Set other fields to defaults since core::ServerConfig doesn't have all fields
    config_ = localConfig;
    if (protocolHandler_) {
        protocolHandler_->updateConfig(config_.protocolConfig);
    }
    logInfo("Server configuration updated");
}

core::ServerConfig StdioServer::getConfig() const {
    // Convert local ServerConfig to core::ServerConfig
    core::ServerConfig coreConfig;
    coreConfig.name = config_.serverName;
    coreConfig.maxConnections = config_.maxConcurrentClients;
    coreConfig.port = 0; // STDIO doesn't use ports
    return coreConfig;
}



void StdioServer::setServerConfig(const ServerConfig& config) {
    config_ = config;
    protocolHandler_->updateConfig(config_.protocolConfig);
    logInfo("Server configuration updated");
}

StdioServer::ServerConfig StdioServer::getServerConfig() const {
    return config_;
}

// IServerInterface implementation
core::CommunicationProtocol StdioServer::getProtocol() const {
    return core::CommunicationProtocol::STDIO;
}

std::string StdioServer::getProtocolName() const {
    return "STDIO";
}

void StdioServer::setConnectionCallback(ConnectionCallback callback) {
    // Convert the IServerInterface callback to our internal callback format
    setClientConnectedCallback([callback](const std::string& clientId) {
        ConnectionInfo info;
        info.clientId = clientId;
        info.protocol = core::CommunicationProtocol::STDIO;
        info.connectedAt = std::chrono::system_clock::now();
        info.remoteAddress = "localhost";
        info.remotePort = 0;
        callback(info, true);
    });

    setClientDisconnectedCallback([callback](const std::string& clientId) {
        ConnectionInfo info;
        info.clientId = clientId;
        info.protocol = core::CommunicationProtocol::STDIO;
        info.connectedAt = std::chrono::system_clock::now();
        info.remoteAddress = "localhost";
        info.remotePort = 0;
        callback(info, false);
    });
}

void StdioServer::setMessageCallback(MessageCallback callback) {
    setMessageReceivedCallback([callback](const std::string& clientId, const Message& message) {
        // Convert server message to core message format
        callback(message);
    });
}

void StdioServer::setErrorCallback(IServerInterface::ErrorCallback callback) {
    // Store the IServerInterface callback and use it in our internal error callback
    interfaceErrorCallback_ = callback;

    // Set our internal error callback to call the interface callback
    errorCallback_ = [this, callback](const std::string& error, const std::string& clientId) {
        if (callback) {
            callback(error);
        }
    };
}

bool StdioServer::acceptClient(const std::string& clientId, const std::string& command) {
    if (!validateCommand(command)) {
        handleError("Invalid command: " + command, clientId);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    if (clients_.size() >= static_cast<size_t>(config_.maxConcurrentClients)) {
        handleError("Maximum client limit reached", clientId);
        return false;
    }
    
    if (clients_.find(clientId) != clients_.end()) {
        logInfo("Client already connected: " + clientId);
        return true;
    }
    
    // Create connection info
    ConnectionInfo connectionInfo;
    connectionInfo.clientId = clientId;
    connectionInfo.protocol = CommunicationProtocol::STDIO;
    connectionInfo.connectedAt = std::chrono::system_clock::now();
    connectionInfo.remoteAddress = "localhost";
    connectionInfo.remotePort = 0;
    
    bool success = protocolHandler_->handleClientConnect(connectionInfo);
    if (success) {
        auto stdioConnection = std::make_unique<StdioConnectionInfo>();
        stdioConnection->clientId = clientId;
        stdioConnection->connectedAt = connectionInfo.connectedAt;
        stdioConnection->isActive = true;
        
        clients_[clientId] = std::move(stdioConnection);
        
        {
            std::lock_guard<std::mutex> statsLock(statsMutex_);
            statistics_.totalClientsConnected++;
            statistics_.currentActiveClients = clients_.size();
        }
        
        logInfo("Client connected: " + clientId);
    }
    
    return success;
}

bool StdioServer::disconnectClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto it = clients_.find(clientId);
    if (it == clients_.end()) {
        return false;
    }
    
    bool success = protocolHandler_->handleClientDisconnect(clientId);
    clients_.erase(it);
    
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.currentActiveClients = clients_.size();
    }
    
    logInfo("Client disconnected: " + clientId);
    return success;
}

std::vector<std::string> StdioServer::getConnectedClients() const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::vector<std::string> clientIds;
    
    for (const auto& [clientId, client] : clients_) {
        if (client->isActive) {
            clientIds.push_back(clientId);
        }
    }
    
    return clientIds;
}

bool StdioServer::isClientConnected(const std::string& clientId) const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clients_.find(clientId);
    return it != clients_.end() && it->second->isActive;
}

bool StdioServer::sendMessageToClient(const std::string& clientId, const Message& message) {
    if (!isClientConnected(clientId)) {
        handleError("Client not connected: " + clientId);
        return false;
    }
    
    bool success = protocolHandler_->sendMessage(clientId, message);
    if (success) {
        updateStatistics();
    }
    
    return success;
}

bool StdioServer::broadcastMessage(const Message& message) {
    bool allSuccess = protocolHandler_->broadcastMessage(message);
    if (allSuccess) {
        updateStatistics();
    }
    return allSuccess;
}

void StdioServer::setClientConnectedCallback(ClientConnectedCallback callback) {
    clientConnectedCallback_ = callback;
}

void StdioServer::setClientDisconnectedCallback(ClientDisconnectedCallback callback) {
    clientDisconnectedCallback_ = callback;
}

void StdioServer::setMessageReceivedCallback(MessageReceivedCallback callback) {
    messageReceivedCallback_ = callback;
}

void StdioServer::setStdioErrorCallback(ErrorCallback callback) {
    errorCallback_ = callback;
}

StdioServer::ServerStatistics StdioServer::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    ServerStatistics stats = statistics_;
    
    if (status_.load() == core::ServerStatus::RUNNING) {
        auto now = std::chrono::system_clock::now();
        stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_);
    }
    
    stats.totalMessagesProcessed = protocolHandler_->getTotalMessagesProcessed();
    stats.totalBytesTransferred = protocolHandler_->getTotalBytesTransferred();
    
    return stats;
}

void StdioServer::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    statistics_ = ServerStatistics{};
    statistics_.serverStartTime = startTime_;
}

std::string StdioServer::getServerInfo() const {
    std::ostringstream info;
    info << "Server: " << config_.serverName << "\n";
    info << "Protocol: STDIO\n";
    info << "Status: " << (status_.load() == core::ServerStatus::RUNNING ? "RUNNING" : "STOPPED") << "\n";
    info << "Max Clients: " << config_.maxConcurrentClients << "\n";
    info << "Active Clients: " << getConnectionCount() << "\n";

    if (status_.load() == core::ServerStatus::RUNNING) {
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime_);
        info << "Uptime: " << uptime.count() << " seconds\n";
    }

    return info.str();
}

bool StdioServer::isHealthy() const {
    return status_.load() == core::ServerStatus::RUNNING &&
           protocolHandler_ != nullptr;
}

std::string StdioServer::getHealthStatus() const {
    if (isHealthy()) {
        return "HEALTHY";
    } else {
        return "UNHEALTHY - Status: " + std::to_string(static_cast<int>(status_.load()));
    }
}

// Private helper methods
void StdioServer::acceptorLoop() {
    logInfo("Acceptor loop started");

    while (running_.load()) {
        try {
            // In a real implementation, this would listen for incoming connections
            // For stdio, we might accept connections through other means
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        } catch (const std::exception& e) {
            handleError("Error in acceptor loop: " + std::string(e.what()));
        }
    }

    logInfo("Acceptor loop terminated");
}

void StdioServer::cleanupLoop() {
    logInfo("Cleanup loop started");

    while (running_.load()) {
        try {
            std::unique_lock<std::mutex> lock(cleanupMutex_);
            cleanupCondition_.wait_for(lock, config_.cleanupInterval);

            if (!running_.load()) {
                break;
            }

            cleanupInactiveClients();

        } catch (const std::exception& e) {
            handleError("Error in cleanup loop: " + std::string(e.what()));
        }
    }

    logInfo("Cleanup loop terminated");
}

void StdioServer::handleClientConnection(const std::string& clientId) {
    logInfo("Handling client connection: " + clientId);

    if (clientConnectedCallback_) {
        clientConnectedCallback_(clientId);
    }
}

void StdioServer::handleClientDisconnection(const std::string& clientId) {
    logInfo("Handling client disconnection: " + clientId);

    if (clientDisconnectedCallback_) {
        clientDisconnectedCallback_(clientId);
    }
}

void StdioServer::handleMessageReceived(const Message& message, const std::string& clientId) {
    logDebug("Message received from client: " + clientId);

    updateStatistics();

    if (messageReceivedCallback_) {
        messageReceivedCallback_(clientId, message);
    }
}

void StdioServer::handleError(const std::string& error, const std::string& clientId) {
    logError(error + (clientId.empty() ? "" : " (Client: " + clientId + ")"));

    if (errorCallback_) {
        errorCallback_(error, clientId);
    }
}

void StdioServer::updateStatistics() {
    // Statistics are updated in real-time by the protocol handler
    // This method can be used for additional custom statistics
}

void StdioServer::cleanupInactiveClients() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto now = std::chrono::system_clock::now();

    auto it = clients_.begin();
    while (it != clients_.end()) {
        auto& client = it->second;
        auto timeSinceActivity = now - client->lastActivity;

        if (timeSinceActivity > config_.clientTimeout) {
            logInfo("Cleaning up inactive client: " + it->first);
            protocolHandler_->handleClientDisconnect(it->first);
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.currentActiveClients = clients_.size();
    }
}

bool StdioServer::validateCommand(const std::string& command) const {
    if (!config_.enableCommandFiltering) {
        return true;
    }

    if (config_.allowedCommands.empty()) {
        return true;
    }

    return std::find(config_.allowedCommands.begin(), config_.allowedCommands.end(), command)
           != config_.allowedCommands.end();
}

std::string StdioServer::generateClientId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);

    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return "stdio_client_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

void StdioServer::logInfo(const std::string& message) const {
    spdlog::info("[{}] {}", config_.serverName, message);
}

void StdioServer::logError(const std::string& message) const {
    spdlog::error("[{}] {}", config_.serverName, message);
}

void StdioServer::logDebug(const std::string& message) const {
    spdlog::debug("[{}] {}", config_.serverName, message);
}

// StdioServerFactory implementation
std::unique_ptr<StdioServer> StdioServerFactory::createDefault() {
    return std::make_unique<StdioServer>(createDefaultConfig());
}

std::unique_ptr<StdioServer> StdioServerFactory::createWithConfig(const StdioServer::ServerConfig& config) {
    return std::make_unique<StdioServer>(config);
}

std::unique_ptr<StdioServer> StdioServerFactory::createFromConfigFile(const std::string& configFile) {
    // Load configuration from file
    auto& configManager = getGlobalStdioConfigManager();
    auto coreConfig = configManager.loadConfigFromFile(configFile);

    StdioServer::ServerConfig serverConfig;
    serverConfig.protocolConfig = convertToProtocolConfig(coreConfig);

    return std::make_unique<StdioServer>(serverConfig);
}

StdioServer::ServerConfig StdioServerFactory::createDefaultConfig() {
    StdioServer::ServerConfig config;
    config.protocolConfig = convertToProtocolConfig(getGlobalStdioConfigManager().createConfig());
    return config;
}

StdioServer::ServerConfig StdioServerFactory::createHighPerformanceConfig() {
    StdioServer::ServerConfig config;
    config.protocolConfig = convertToProtocolConfig(getGlobalStdioConfigManager().createConfig(StdioConfigManager::ConfigPreset::HIGH_PERFORMANCE));
    config.maxConcurrentClients = 1000;
    config.enableAutoCleanup = true;
    config.cleanupInterval = std::chrono::milliseconds(30000);
    return config;
}

StdioServer::ServerConfig StdioServerFactory::createSecureConfig() {
    StdioServer::ServerConfig config;
    config.protocolConfig = convertToProtocolConfig(getGlobalStdioConfigManager().createConfig(StdioConfigManager::ConfigPreset::SECURE));
    config.enableCommandFiltering = true;
    config.enableClientIsolation = true;
    return config;
}

StdioServer::ServerConfig StdioServerFactory::createDebugConfig() {
    StdioServer::ServerConfig config;
    config.protocolConfig = convertToProtocolConfig(getGlobalStdioConfigManager().createConfig(StdioConfigManager::ConfigPreset::DEBUG));
    config.enableAutoCleanup = false; // Keep clients for debugging
    return config;
}

} // namespace stdio
} // namespace protocols
} // namespace server
} // namespace hydrogen

#include "hydrogen/server/protocols/fifo/fifo_server.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>
#include <thread>

namespace hydrogen {
namespace server {
namespace protocols {
namespace fifo {

// FifoServerConfig implementation
json FifoServerConfig::toJson() const {
    json j;
    j["serverName"] = serverName;
    j["serverVersion"] = serverVersion;
    j["serverId"] = serverId;
    j["maxConcurrentClients"] = maxConcurrentClients;
    j["clientTimeout"] = clientTimeout.count();
    j["serverTimeout"] = serverTimeout.count();
    j["protocolConfig"] = protocolConfig.toJson();
    j["enableCommandFiltering"] = enableCommandFiltering;
    j["allowedCommands"] = allowedCommands;
    j["enableClientAuthentication"] = enableClientAuthentication;
    j["authToken"] = authToken;
    j["workerThreadCount"] = workerThreadCount;
    j["enableAutoCleanup"] = enableAutoCleanup;
    j["cleanupInterval"] = cleanupInterval.count();
    j["enableHealthChecking"] = enableHealthChecking;
    j["healthCheckInterval"] = healthCheckInterval.count();
    j["enableServerLogging"] = enableServerLogging;
    j["enablePerformanceMetrics"] = enablePerformanceMetrics;
    j["enableDebugMode"] = enableDebugMode;
    j["logLevel"] = logLevel;
    return j;
}

void FifoServerConfig::fromJson(const json& j) {
    if (j.contains("serverName")) serverName = j["serverName"];
    if (j.contains("serverVersion")) serverVersion = j["serverVersion"];
    if (j.contains("serverId")) serverId = j["serverId"];
    if (j.contains("maxConcurrentClients")) maxConcurrentClients = j["maxConcurrentClients"];
    if (j.contains("clientTimeout")) clientTimeout = std::chrono::milliseconds(j["clientTimeout"]);
    if (j.contains("serverTimeout")) serverTimeout = std::chrono::milliseconds(j["serverTimeout"]);
    if (j.contains("protocolConfig")) protocolConfig.fromJson(j["protocolConfig"]);
    if (j.contains("enableCommandFiltering")) enableCommandFiltering = j["enableCommandFiltering"];
    if (j.contains("allowedCommands")) allowedCommands = j["allowedCommands"];
    if (j.contains("enableClientAuthentication")) enableClientAuthentication = j["enableClientAuthentication"];
    if (j.contains("authToken")) authToken = j["authToken"];
    if (j.contains("workerThreadCount")) workerThreadCount = j["workerThreadCount"];
    if (j.contains("enableAutoCleanup")) enableAutoCleanup = j["enableAutoCleanup"];
    if (j.contains("cleanupInterval")) cleanupInterval = std::chrono::milliseconds(j["cleanupInterval"]);
    if (j.contains("enableHealthChecking")) enableHealthChecking = j["enableHealthChecking"];
    if (j.contains("healthCheckInterval")) healthCheckInterval = std::chrono::milliseconds(j["healthCheckInterval"]);
    if (j.contains("enableServerLogging")) enableServerLogging = j["enableServerLogging"];
    if (j.contains("enablePerformanceMetrics")) enablePerformanceMetrics = j["enablePerformanceMetrics"];
    if (j.contains("enableDebugMode")) enableDebugMode = j["enableDebugMode"];
    if (j.contains("logLevel")) logLevel = j["logLevel"];
}

bool FifoServerConfig::validate() const {
    if (serverName.empty() || serverId.empty()) return false;
    if (maxConcurrentClients <= 0) return false;
    if (workerThreadCount <= 0) return false;
    if (clientTimeout.count() <= 0 || serverTimeout.count() <= 0) return false;
    return true;
}

// FifoServerStatistics implementation
double FifoServerStatistics::getMessagesPerSecond() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
    if (duration.count() == 0) return 0.0;
    return static_cast<double>(totalMessagesProcessed.load()) / duration.count();
}

double FifoServerStatistics::getBytesPerSecond() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
    if (duration.count() == 0) return 0.0;
    return static_cast<double>(totalBytesTransferred.load()) / duration.count();
}

std::chrono::milliseconds FifoServerStatistics::getUptime() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
}

json FifoServerStatistics::toJson() const {
    json j;
    j["totalClientsConnected"] = totalClientsConnected.load();
    j["currentActiveClients"] = currentActiveClients.load();
    j["totalMessagesProcessed"] = totalMessagesProcessed.load();
    j["totalBytesTransferred"] = totalBytesTransferred.load();
    j["totalErrors"] = totalErrors.load();
    j["totalCommandsExecuted"] = totalCommandsExecuted.load();
    j["messagesPerSecond"] = getMessagesPerSecond();
    j["bytesPerSecond"] = getBytesPerSecond();
    j["uptimeMs"] = getUptime().count();
    
    auto now = std::chrono::system_clock::now();
    auto timeSinceActivity = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastActivity);
    j["timeSinceLastActivityMs"] = timeSinceActivity.count();
    
    return j;
}

// FifoServer implementation
FifoServer::FifoServer(const FifoServerConfig& config) : config_(config) {
    if (!config_.validate()) {
        throw std::invalid_argument("Invalid FIFO server configuration");
    }
    
    spdlog::info("FIFO server created: {}", config_.serverName);
    initializeCommandHandlers();
}

FifoServer::~FifoServer() {
    stop();
}

bool FifoServer::start() {
    if (status_.load() == ServerStatus::RUNNING) {
        spdlog::warn("FIFO server is already running");
        return true;
    }
    
    status_.store(ServerStatus::STARTING);
    spdlog::info("Starting FIFO server: {}", config_.serverName);
    
    if (!initializeServer()) {
        status_.store(ServerStatus::ERROR);
        spdlog::error("Failed to initialize FIFO server");
        return false;
    }
    
    if (!startProtocolHandler()) {
        status_.store(ServerStatus::ERROR);
        spdlog::error("Failed to start protocol handler");
        return false;
    }
    
    running_.store(true);
    
    // Start management threads
    if (config_.enableHealthChecking) {
        healthCheckThread_ = std::make_unique<std::thread>(&FifoServer::healthCheckThreadFunction, this);
    }
    
    if (config_.enableAutoCleanup) {
        cleanupThread_ = std::make_unique<std::thread>(&FifoServer::cleanupThreadFunction, this);
    }
    
    status_.store(ServerStatus::RUNNING);
    statistics_.startTime = std::chrono::system_clock::now();
    updateLastActivity();
    
    spdlog::info("FIFO server started successfully: {}", config_.serverName);
    return true;
}

void FifoServer::stop() {
    if (status_.load() == ServerStatus::STOPPED) {
        return;
    }
    
    status_.store(ServerStatus::STOPPING);
    spdlog::info("Stopping FIFO server: {}", config_.serverName);
    
    running_.store(false);
    
    // Stop threads
    stopThreads();
    
    // Stop protocol handler
    stopProtocolHandler();
    
    // Shutdown server
    shutdownServer();
    
    status_.store(ServerStatus::STOPPED);
    spdlog::info("FIFO server stopped: {}", config_.serverName);
}

bool FifoServer::restart() {
    spdlog::info("Restarting FIFO server: {}", config_.serverName);
    stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Brief pause
    return start();
}

ServerStatus FifoServer::getStatus() const {
    return status_.load();
}

bool FifoServer::isRunning() const {
    return status_.load() == ServerStatus::RUNNING;
}

bool FifoServer::acceptClient(const std::string& clientId, const std::string& command) {
    if (!isRunning()) {
        spdlog::warn("Cannot accept client - server not running");
        return false;
    }
    
    if (!protocolHandler_) {
        spdlog::error("Protocol handler not available");
        return false;
    }
    
    // Check command filtering
    if (config_.enableCommandFiltering) {
        if (std::find(config_.allowedCommands.begin(), config_.allowedCommands.end(), command) 
            == config_.allowedCommands.end()) {
            spdlog::warn("Command not allowed for client {}: {}", clientId, command);
            return false;
        }
    }
    
    if (protocolHandler_->acceptClient(clientId, command)) {
        statistics_.totalClientsConnected.fetch_add(1);
        statistics_.currentActiveClients.fetch_add(1);
        updateLastActivity();
        
        onClientConnected(clientId);
        return true;
    }
    
    return false;
}

bool FifoServer::disconnectClient(const std::string& clientId) {
    if (!protocolHandler_) {
        return false;
    }
    
    if (protocolHandler_->disconnectClient(clientId)) {
        statistics_.currentActiveClients.fetch_sub(1);
        updateLastActivity();
        
        onClientDisconnected(clientId);
        return true;
    }
    
    return false;
}

bool FifoServer::isClientConnected(const std::string& clientId) const {
    if (!protocolHandler_) {
        return false;
    }
    
    return protocolHandler_->isClientConnected(clientId);
}

std::vector<std::string> FifoServer::getConnectedClients() const {
    if (!protocolHandler_) {
        return {};
    }
    
    return protocolHandler_->getConnectedClients();
}

bool FifoServer::sendMessageToClient(const std::string& clientId, const Message& message) {
    if (!isRunning() || !protocolHandler_) {
        return false;
    }
    
    if (protocolHandler_->sendMessage(clientId, message)) {
        updateStatistics(true, message.payload.size());
        updateLastActivity();
        return true;
    }
    
    return false;
}

bool FifoServer::broadcastMessage(const Message& message) {
    if (!isRunning() || !protocolHandler_) {
        return false;
    }
    
    auto clients = getConnectedClients();
    if (clients.empty()) {
        spdlog::debug("No clients connected for broadcast");
        return true;
    }
    
    bool success = true;
    for (const auto& clientId : clients) {
        if (!sendMessageToClient(clientId, message)) {
            success = false;
            spdlog::warn("Failed to broadcast message to client: {}", clientId);
        }
    }
    
    return success;
}

bool FifoServer::sendResponse(const std::string& clientId, const std::string& response, const std::string& originalMessageId) {
    Message responseMessage;
    responseMessage.senderId = config_.serverId;
    responseMessage.recipientId = clientId;
    responseMessage.topic = "response";
    responseMessage.payload = response;
    responseMessage.sourceProtocol = CommunicationProtocol::FIFO;
    responseMessage.targetProtocol = CommunicationProtocol::FIFO;
    responseMessage.timestamp = std::chrono::system_clock::now();
    
    return sendMessageToClient(clientId, responseMessage);
}

bool FifoServer::updateConfig(const json& configUpdates) {
    try {
        FifoServerConfig newConfig = config_;
        newConfig.fromJson(configUpdates);
        
        if (!newConfig.validate()) {
            spdlog::error("Invalid configuration update");
            return false;
        }
        
        config_ = newConfig;
        
        // Update protocol handler config if needed
        if (protocolHandler_) {
            protocolHandler_->setConfig(config_.protocolConfig);
        }
        
        spdlog::info("Server configuration updated");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to update configuration: {}", e.what());
        return false;
    }
}

FifoServerStatistics FifoServer::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    return statistics_;
}

bool FifoServer::isHealthy() const {
    if (!isRunning()) {
        return false;
    }
    
    if (!protocolHandler_ || !protocolHandler_->isHealthy()) {
        return false;
    }
    
    return performHealthCheck();
}

std::string FifoServer::getHealthStatus() const {
    if (!isRunning()) {
        return "UNHEALTHY - Status: " + std::to_string(static_cast<int>(status_.load()));
    }
    
    if (!protocolHandler_) {
        return "UNHEALTHY - No protocol handler";
    }
    
    if (!protocolHandler_->isHealthy()) {
        return "UNHEALTHY - Protocol handler: " + protocolHandler_->getHealthStatus();
    }
    
    if (!performHealthCheck()) {
        return "UNHEALTHY - Health check failed";
    }
    
    return "HEALTHY";
}

json FifoServer::getServerInfo() const {
    json info;
    info["serverName"] = config_.serverName;
    info["serverVersion"] = config_.serverVersion;
    info["serverId"] = config_.serverId;
    info["status"] = static_cast<int>(status_.load());
    info["isRunning"] = isRunning();
    info["isHealthy"] = isHealthy();
    info["healthStatus"] = getHealthStatus();
    info["connectedClients"] = getConnectedClients();
    info["statistics"] = getStatistics().toJson();
    info["config"] = config_.toJson();
    
    return info;
}

// Protected methods implementation
bool FifoServer::initializeServer() {
    spdlog::debug("Initializing FIFO server components");

    // Create protocol handler
    protocolHandler_ = FifoProtocolHandlerFactory::create(config_.protocolConfig);
    if (!protocolHandler_) {
        spdlog::error("Failed to create protocol handler");
        return false;
    }

    // Setup callbacks
    setupProtocolHandlerCallbacks();

    return true;
}

void FifoServer::shutdownServer() {
    spdlog::debug("Shutting down FIFO server components");

    if (protocolHandler_) {
        protocolHandler_->shutdown();
        protocolHandler_.reset();
    }

    cleanup();
}

bool FifoServer::startProtocolHandler() {
    if (!protocolHandler_) {
        return false;
    }

    return protocolHandler_->initialize();
}

void FifoServer::stopProtocolHandler() {
    if (protocolHandler_) {
        protocolHandler_->shutdown();
    }
}

void FifoServer::onClientConnected(const std::string& clientId) {
    spdlog::info("Client connected to FIFO server: {}", clientId);

    if (clientConnectedCallback_) {
        clientConnectedCallback_(clientId);
    }
}

void FifoServer::onClientDisconnected(const std::string& clientId) {
    spdlog::info("Client disconnected from FIFO server: {}", clientId);

    if (clientDisconnectedCallback_) {
        clientDisconnectedCallback_(clientId);
    }
}

void FifoServer::onMessageReceived(const std::string& clientId, const Message& message) {
    spdlog::debug("Message received from client {}: {}", clientId, message.topic);

    updateStatistics(false, message.payload.size());
    updateLastActivity();

    if (messageReceivedCallback_) {
        messageReceivedCallback_(clientId, message);
    }
}

void FifoServer::onError(const std::string& error, const std::string& clientId) {
    spdlog::error("FIFO server error{}: {}",
                  clientId.empty() ? "" : " for client " + clientId, error);

    incrementErrorCount();

    if (errorCallback_) {
        errorCallback_(error, clientId);
    }
}

// Command handlers
std::string FifoServer::handlePingCommand(const std::string& clientId, const std::string& args) {
    statistics_.totalCommandsExecuted.fetch_add(1);
    return "pong";
}

std::string FifoServer::handleEchoCommand(const std::string& clientId, const std::string& args) {
    statistics_.totalCommandsExecuted.fetch_add(1);
    return args.empty() ? "echo" : args;
}

std::string FifoServer::handleStatusCommand(const std::string& clientId, const std::string& args) {
    statistics_.totalCommandsExecuted.fetch_add(1);

    json status;
    status["server"] = config_.serverName;
    status["status"] = static_cast<int>(status_.load());
    status["clients"] = getConnectedClients().size();
    status["uptime"] = getStatistics().getUptime().count();
    status["healthy"] = isHealthy();

    return status.dump();
}

std::string FifoServer::handleHelpCommand(const std::string& clientId, const std::string& args) {
    statistics_.totalCommandsExecuted.fetch_add(1);

    std::ostringstream help;
    help << "Available commands:\n";
    help << "  ping - Test server connectivity\n";
    help << "  echo [message] - Echo back the message\n";
    help << "  status - Get server status information\n";
    help << "  help - Show this help message\n";

    return help.str();
}

bool FifoServer::performHealthCheck() const {
    // Basic health checks
    if (!running_.load()) {
        return false;
    }

    // Check if we've had recent activity (if enabled)
    if (config_.enablePerformanceMetrics) {
        auto now = std::chrono::system_clock::now();
        auto timeSinceActivity = std::chrono::duration_cast<std::chrono::minutes>(now - statistics_.lastActivity);

        // If no activity for more than 30 minutes, consider unhealthy
        if (timeSinceActivity.count() > 30) {
            return false;
        }
    }

    return true;
}

void FifoServer::updateStatistics(bool sent, size_t bytes) {
    std::lock_guard<std::mutex> lock(statisticsMutex_);

    statistics_.totalMessagesProcessed.fetch_add(1);
    statistics_.totalBytesTransferred.fetch_add(bytes);
    updateLastActivity();
}

void FifoServer::incrementErrorCount() {
    statistics_.totalErrors.fetch_add(1);
}

// Private methods implementation
void FifoServer::healthCheckThreadFunction() {
    spdlog::debug("Health check thread started");

    while (running_.load()) {
        if (!performHealthCheck()) {
            spdlog::warn("Health check failed for server: {}", config_.serverName);
            onError("Health check failed");
        }

        std::this_thread::sleep_for(config_.healthCheckInterval);
    }

    spdlog::debug("Health check thread stopped");
}

void FifoServer::cleanupThreadFunction() {
    spdlog::debug("Cleanup thread started");

    while (running_.load()) {
        if (protocolHandler_) {
            protocolHandler_->cleanupInactiveClients();
        }

        std::this_thread::sleep_for(config_.cleanupInterval);
    }

    spdlog::debug("Cleanup thread stopped");
}

void FifoServer::initializeCommandHandlers() {
    commandHandlers_["ping"] = [this](const std::string& clientId, const std::string& args) {
        return handlePingCommand(clientId, args);
    };

    commandHandlers_["echo"] = [this](const std::string& clientId, const std::string& args) {
        return handleEchoCommand(clientId, args);
    };

    commandHandlers_["status"] = [this](const std::string& clientId, const std::string& args) {
        return handleStatusCommand(clientId, args);
    };

    commandHandlers_["help"] = [this](const std::string& clientId, const std::string& args) {
        return handleHelpCommand(clientId, args);
    };
}

void FifoServer::setupProtocolHandlerCallbacks() {
    if (!protocolHandler_) {
        return;
    }

    // Note: The actual callback setup would depend on the protocol handler interface
    // This is a conceptual implementation
    spdlog::debug("Protocol handler callbacks configured");
}

void FifoServer::stopThreads() {
    if (healthCheckThread_ && healthCheckThread_->joinable()) {
        healthCheckThread_->join();
        healthCheckThread_.reset();
    }

    if (cleanupThread_ && cleanupThread_->joinable()) {
        cleanupThread_->join();
        cleanupThread_.reset();
    }
}

void FifoServer::cleanup() {
    // Clear any remaining resources
    commandHandlers_.clear();
}

std::string FifoServer::generateMessageId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    return "msg_" + std::to_string(dis(gen));
}

std::string FifoServer::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

void FifoServer::updateLastActivity() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.lastActivity = std::chrono::system_clock::now();
}

// Factory implementation
std::unique_ptr<FifoServer> FifoServerFactory::createDefault() {
    return createWithConfig(createDefaultConfig());
}

std::unique_ptr<FifoServer> FifoServerFactory::createWithConfig(const FifoServerConfig& config) {
    return std::make_unique<FifoServer>(config);
}

std::unique_ptr<FifoServer> FifoServerFactory::createHighPerformance() {
    return createWithConfig(createHighPerformanceConfig());
}

std::unique_ptr<FifoServer> FifoServerFactory::createSecure() {
    return createWithConfig(createSecureConfig());
}

std::unique_ptr<FifoServer> FifoServerFactory::createDebug() {
    return createWithConfig(createDebugConfig());
}

std::unique_ptr<FifoServer> FifoServerFactory::createEmbedded() {
    return createWithConfig(createEmbeddedConfig());
}

FifoServerConfig FifoServerFactory::createDefaultConfig() {
    FifoServerConfig config;
    config.serverName = "HydrogenFifoServer";
    config.maxConcurrentClients = 50;
    config.workerThreadCount = 4;
    config.enableAutoCleanup = true;
    config.enableHealthChecking = true;
    config.enableServerLogging = true;
    return config;
}

FifoServerConfig FifoServerFactory::createHighPerformanceConfig() {
    FifoServerConfig config = createDefaultConfig();
    config.serverName = "HydrogenFifoServer-HighPerf";
    config.maxConcurrentClients = 200;
    config.workerThreadCount = 8;
    config.enablePerformanceMetrics = true;
    config.protocolConfig = FifoProtocolHandlerFactory::createHighPerformance()->getConfig();
    return config;
}

FifoServerConfig FifoServerFactory::createSecureConfig() {
    FifoServerConfig config = createDefaultConfig();
    config.serverName = "HydrogenFifoServer-Secure";
    config.enableCommandFiltering = true;
    config.enableClientAuthentication = true;
    config.maxConcurrentClients = 20; // Limit for security
    config.allowedCommands = {"ping", "status"}; // Restricted commands
    config.protocolConfig = FifoProtocolHandlerFactory::createSecure()->getConfig();
    return config;
}

FifoServerConfig FifoServerFactory::createDebugConfig() {
    FifoServerConfig config = createDefaultConfig();
    config.serverName = "HydrogenFifoServer-Debug";
    config.enableDebugMode = true;
    config.enableServerLogging = true;
    config.enablePerformanceMetrics = true;
    config.logLevel = "DEBUG";
    config.enableAutoCleanup = false; // Keep connections for debugging
    config.protocolConfig = FifoProtocolHandlerFactory::createDebug()->getConfig();
    return config;
}

FifoServerConfig FifoServerFactory::createEmbeddedConfig() {
    FifoServerConfig config = createDefaultConfig();
    config.serverName = "HydrogenFifoServer-Embedded";
    config.maxConcurrentClients = 10;
    config.workerThreadCount = 2;
    config.enablePerformanceMetrics = false;
    config.enableAutoCleanup = true;
    config.cleanupInterval = std::chrono::milliseconds(30000); // More frequent cleanup
    return config;
}

std::unique_ptr<FifoServer> FifoServerFactory::createForWindows(const FifoServerConfig& config) {
    FifoServerConfig windowsConfig = config;
    windowsConfig.protocolConfig.windowsBasePipePath = "\\\\.\\pipe\\hydrogen_fifo";
    return createWithConfig(windowsConfig);
}

std::unique_ptr<FifoServer> FifoServerFactory::createForUnix(const FifoServerConfig& config) {
    FifoServerConfig unixConfig = config;
    unixConfig.protocolConfig.basePipePath = "/tmp/hydrogen_fifo";
    return createWithConfig(unixConfig);
}

std::unique_ptr<FifoServer> FifoServerFactory::createBroadcastServer(const FifoServerConfig& config) {
    FifoServerConfig broadcastConfig = config;
    broadcastConfig.serverName = "HydrogenFifoServer-Broadcast";
    broadcastConfig.maxConcurrentClients = 100;
    broadcastConfig.enableCommandFiltering = false; // Allow all commands for broadcast
    return createWithConfig(broadcastConfig);
}

std::unique_ptr<FifoServer> FifoServerFactory::createInteractiveServer(const FifoServerConfig& config) {
    FifoServerConfig interactiveConfig = config;
    interactiveConfig.serverName = "HydrogenFifoServer-Interactive";
    interactiveConfig.enableCommandFiltering = true;
    interactiveConfig.allowedCommands = {"ping", "echo", "status", "help", "interactive"};
    interactiveConfig.clientTimeout = std::chrono::milliseconds(300000); // 5 minutes for interactive sessions
    return createWithConfig(interactiveConfig);
}

} // namespace fifo
} // namespace protocols
} // namespace server
} // namespace hydrogen

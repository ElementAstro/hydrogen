#include "hydrogen/server/protocols/fifo/fifo_protocol_handler.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#endif

namespace hydrogen {
namespace server {
namespace protocols {
namespace fifo {

// FifoProtocolConfig implementation
json FifoProtocolConfig::toJson() const {
    json j;
    j["basePipePath"] = basePipePath;
    j["windowsBasePipePath"] = windowsBasePipePath;
    j["maxConcurrentClients"] = maxConcurrentClients;
    j["enableClientAuthentication"] = enableClientAuthentication;
    j["enableMessageValidation"] = enableMessageValidation;
    j["enableMessageLogging"] = enableMessageLogging;
    j["maxMessageSize"] = maxMessageSize;
    j["maxQueueSize"] = maxQueueSize;
    j["messageTimeout"] = messageTimeout.count();
    j["clientTimeout"] = clientTimeout.count();
    j["enableAutoCleanup"] = enableAutoCleanup;
    j["cleanupInterval"] = cleanupInterval.count();
    j["enableKeepAlive"] = enableKeepAlive;
    j["keepAliveInterval"] = keepAliveInterval.count();
    j["allowedCommands"] = allowedCommands;
    j["enableCommandFiltering"] = enableCommandFiltering;
    j["authToken"] = authToken;
    j["enableNonBlocking"] = enableNonBlocking;
    j["enableBidirectional"] = enableBidirectional;
    j["workerThreadCount"] = workerThreadCount;
    return j;
}

void FifoProtocolConfig::fromJson(const json& j) {
    if (j.contains("basePipePath")) basePipePath = j["basePipePath"];
    if (j.contains("windowsBasePipePath")) windowsBasePipePath = j["windowsBasePipePath"];
    if (j.contains("maxConcurrentClients")) maxConcurrentClients = j["maxConcurrentClients"];
    if (j.contains("enableClientAuthentication")) enableClientAuthentication = j["enableClientAuthentication"];
    if (j.contains("enableMessageValidation")) enableMessageValidation = j["enableMessageValidation"];
    if (j.contains("enableMessageLogging")) enableMessageLogging = j["enableMessageLogging"];
    if (j.contains("maxMessageSize")) maxMessageSize = j["maxMessageSize"];
    if (j.contains("maxQueueSize")) maxQueueSize = j["maxQueueSize"];
    if (j.contains("messageTimeout")) messageTimeout = std::chrono::milliseconds(j["messageTimeout"]);
    if (j.contains("clientTimeout")) clientTimeout = std::chrono::milliseconds(j["clientTimeout"]);
    if (j.contains("enableAutoCleanup")) enableAutoCleanup = j["enableAutoCleanup"];
    if (j.contains("cleanupInterval")) cleanupInterval = std::chrono::milliseconds(j["cleanupInterval"]);
    if (j.contains("enableKeepAlive")) enableKeepAlive = j["enableKeepAlive"];
    if (j.contains("keepAliveInterval")) keepAliveInterval = std::chrono::milliseconds(j["keepAliveInterval"]);
    if (j.contains("allowedCommands")) allowedCommands = j["allowedCommands"];
    if (j.contains("enableCommandFiltering")) enableCommandFiltering = j["enableCommandFiltering"];
    if (j.contains("authToken")) authToken = j["authToken"];
    if (j.contains("enableNonBlocking")) enableNonBlocking = j["enableNonBlocking"];
    if (j.contains("enableBidirectional")) enableBidirectional = j["enableBidirectional"];
    if (j.contains("workerThreadCount")) workerThreadCount = j["workerThreadCount"];
}

// FifoProtocolStats implementation
double FifoProtocolStats::getMessagesPerSecond() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
    if (duration.count() == 0) return 0.0;
    return static_cast<double>(totalMessagesProcessed.load()) / duration.count();
}

double FifoProtocolStats::getBytesPerSecond() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
    if (duration.count() == 0) return 0.0;
    return static_cast<double>(totalBytesTransferred.load()) / duration.count();
}

json FifoProtocolStats::toJson() const {
    json j;
    j["totalClientsConnected"] = totalClientsConnected.load();
    j["currentActiveClients"] = currentActiveClients.load();
    j["totalMessagesProcessed"] = totalMessagesProcessed.load();
    j["totalBytesTransferred"] = totalBytesTransferred.load();
    j["totalErrors"] = totalErrors.load();
    j["messagesPerSecond"] = getMessagesPerSecond();
    j["bytesPerSecond"] = getBytesPerSecond();
    
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
    j["uptimeMs"] = uptime.count();
    
    return j;
}

// FifoProtocolHandler implementation
FifoProtocolHandler::FifoProtocolHandler(const FifoProtocolConfig& config) : config_(config) {
    spdlog::info("FIFO protocol handler created with base path: {}", 
                 getPlatformSpecificPipePath(config_.basePipePath));
}

FifoProtocolHandler::~FifoProtocolHandler() {
    shutdown();
}

bool FifoProtocolHandler::initialize() {
    if (running_.load()) {
        return true;
    }
    
    spdlog::info("Initializing FIFO protocol handler");
    
    // Create base pipe directory
    std::string basePath = getPlatformSpecificPipePath(config_.basePipePath);
    if (!createPipeDirectory(basePath)) {
        spdlog::error("Failed to create pipe directory: {}", basePath);
        return false;
    }
    
    running_.store(true);
    
    // Start worker threads
    for (int i = 0; i < config_.workerThreadCount; ++i) {
        workerThreads_.emplace_back(std::make_unique<std::thread>(&FifoProtocolHandler::workerThreadFunction, this));
    }
    
    // Start management threads
    if (config_.enableAutoCleanup) {
        cleanupThread_ = std::make_unique<std::thread>(&FifoProtocolHandler::cleanupThreadFunction, this);
    }
    
    if (config_.enableKeepAlive) {
        keepAliveThread_ = std::make_unique<std::thread>(&FifoProtocolHandler::keepAliveThreadFunction, this);
    }
    
    statistics_.startTime = std::chrono::system_clock::now();
    
    spdlog::info("FIFO protocol handler initialized successfully");
    return true;
}

void FifoProtocolHandler::shutdown() {
    if (!running_.load()) {
        return;
    }
    
    spdlog::info("Shutting down FIFO protocol handler");
    
    running_.store(false);
    
    // Disconnect all clients
    disconnectAllClients();
    
    // Stop threads
    stopThreads();
    
    // Clear queues
    clearQueues();
    
    spdlog::info("FIFO protocol handler shutdown complete");
}

bool FifoProtocolHandler::handleMessage(const Message& message, const std::string& clientId) {
    if (!running_.load()) {
        return false;
    }
    
    if (!isClientConnected(clientId)) {
        spdlog::warn("Received message from unknown client: {}", clientId);
        return false;
    }
    
    if (config_.enableMessageValidation && !validateMessage(message)) {
        std::string error = getValidationError(message);
        spdlog::error("Message validation failed for client {}: {}", clientId, error);
        return false;
    }
    
    // Queue message for processing
    queueIncomingMessage(clientId, message);
    
    // Update statistics
    updateStatistics(false, message.payload.size());
    updateClientActivity(clientId);
    
    if (config_.enableMessageLogging) {
        logMessage("RECEIVED", message, clientId);
    }
    
    return true;
}

bool FifoProtocolHandler::sendMessage(const Message& message, const std::string& clientId) {
    if (!running_.load()) {
        return false;
    }
    
    if (!isClientConnected(clientId)) {
        spdlog::warn("Attempted to send message to unknown client: {}", clientId);
        return false;
    }
    
    if (config_.enableMessageValidation && !validateMessage(message)) {
        std::string error = getValidationError(message);
        spdlog::error("Outgoing message validation failed for client {}: {}", clientId, error);
        return false;
    }
    
    // Queue message for sending
    queueOutgoingMessage(clientId, message);
    
    // Update statistics
    updateStatistics(true, message.payload.size());
    updateClientActivity(clientId);
    
    if (config_.enableMessageLogging) {
        logMessage("SENT", message, clientId);
    }
    
    return true;
}

bool FifoProtocolHandler::broadcastMessage(const Message& message) {
    if (!running_.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    if (clients_.empty()) {
        spdlog::debug("No clients connected for broadcast");
        return true;
    }
    
    bool success = true;
    for (const auto& [clientId, clientInfo] : clients_) {
        if (clientInfo->active.load()) {
            if (!sendMessage(message, clientId)) {
                success = false;
                spdlog::warn("Failed to broadcast message to client: {}", clientId);
            }
        }
    }
    
    return success;
}

std::vector<std::string> FifoProtocolHandler::getConnectedClients() const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    std::vector<std::string> connectedClients;
    for (const auto& [clientId, clientInfo] : clients_) {
        if (clientInfo->active.load()) {
            connectedClients.push_back(clientId);
        }
    }
    
    return connectedClients;
}

bool FifoProtocolHandler::isClientConnected(const std::string& clientId) const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto it = clients_.find(clientId);
    return it != clients_.end() && it->second->active.load();
}

bool FifoProtocolHandler::disconnectClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto it = clients_.find(clientId);
    if (it == clients_.end()) {
        return false;
    }
    
    it->second->active.store(false);
    
    // Stop the client's communicator
    if (it->second->communicator) {
        it->second->communicator->stop();
    }
    
    onClientDisconnected(clientId);
    
    // Remove from clients map
    clients_.erase(it);
    
    statistics_.currentActiveClients.fetch_sub(1);
    
    spdlog::info("Client disconnected: {}", clientId);
    return true;
}

bool FifoProtocolHandler::acceptClient(const std::string& clientId, const std::string& command) {
    if (!running_.load()) {
        return false;
    }
    
    // Check if client already exists
    if (isClientConnected(clientId)) {
        spdlog::warn("Client already connected: {}", clientId);
        return false;
    }
    
    // Check client limit
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (static_cast<int>(clients_.size()) >= config_.maxConcurrentClients) {
            spdlog::warn("Maximum client limit reached, rejecting client: {}", clientId);
            return false;
        }
    }
    
    // Validate command if filtering is enabled
    if (config_.enableCommandFiltering && !validateClientCommand(command)) {
        spdlog::warn("Invalid command from client {}: {}", clientId, command);
        return false;
    }
    
    // Create client pipe
    if (!createClientPipe(clientId)) {
        spdlog::error("Failed to create pipe for client: {}", clientId);
        return false;
    }
    
    // Create client info
    std::string pipePath = generateClientPipePath(clientId);
    auto clientInfo = std::make_unique<FifoClientInfo>(clientId, pipePath);
    
    // Create FIFO communicator for this client
    auto& configManager = getGlobalFifoConfigManager();
    FifoConfig fifoConfig = configManager.createConfig();
    
#ifdef _WIN32
    fifoConfig.windowsPipePath = pipePath;
    fifoConfig.pipeType = FifoPipeType::WINDOWS_NAMED_PIPE;
#else
    fifoConfig.unixPipePath = pipePath;
    fifoConfig.pipeType = FifoPipeType::UNIX_FIFO;
#endif
    
    fifoConfig.enableNonBlocking = config_.enableNonBlocking;
    fifoConfig.enableBidirectional = config_.enableBidirectional;
    
    clientInfo->communicator = FifoCommunicatorFactory::create(fifoConfig);
    
    // Set up message handler
    clientInfo->communicator->setMessageHandler([this, clientId](const std::string& message) {
        // Convert string message to Message object
        try {
            json messageJson = json::parse(message);
            auto msg = deserializeMessage(messageJson);
            if (msg) {
                handleMessage(*msg, clientId);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse message from client {}: {}", clientId, e.what());
        }
    });
    
    // Set up error handler
    clientInfo->communicator->setErrorHandler([this, clientId](const std::string& error) {
        onClientError(clientId, error);
    });
    
    // Start the communicator
    if (!clientInfo->communicator->start()) {
        spdlog::error("Failed to start communicator for client: {}", clientId);
        return false;
    }
    
    // Add to clients map
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_[clientId] = std::move(clientInfo);
    }
    
    statistics_.totalClientsConnected.fetch_add(1);
    statistics_.currentActiveClients.fetch_add(1);
    
    onClientConnected(clientId);
    
    spdlog::info("Client accepted: {} with pipe: {}", clientId, pipePath);
    return true;
}

bool FifoProtocolHandler::createClientPipe(const std::string& clientId) {
    std::string pipePath = generateClientPipePath(clientId);

#ifdef _WIN32
    // Windows named pipe creation is handled by the communicator
    return true;
#else
    // Create Unix FIFO
    if (mkfifo(pipePath.c_str(), 0666) == -1) {
        if (errno != EEXIST) {
            spdlog::error("Failed to create FIFO {}: {}", pipePath, strerror(errno));
            return false;
        }
    }

    return setPipePermissions(pipePath);
#endif
}

std::string FifoProtocolHandler::generateClientPipePath(const std::string& clientId) const {
    std::string basePath = getPlatformSpecificPipePath(config_.basePipePath);

#ifdef _WIN32
    return basePath + "_" + clientId;
#else
    return basePath + "/" + clientId;
#endif
}

FifoProtocolStats FifoProtocolHandler::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    return statistics_;
}

bool FifoProtocolHandler::isHealthy() const {
    if (!running_.load()) {
        return false;
    }

    // Check if we have any active clients or if we're accepting new ones
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return true; // Basic health check - can be extended
}

std::string FifoProtocolHandler::getHealthStatus() const {
    if (!running_.load()) {
        return "STOPPED";
    }

    std::lock_guard<std::mutex> lock(clientsMutex_);
    return "HEALTHY - Active clients: " + std::to_string(statistics_.currentActiveClients.load());
}

bool FifoProtocolHandler::validateMessage(const Message& message) const {
    try {
        // Convert server Message struct to JSON for validation
        json messageJson;
        messageJson["senderId"] = message.senderId;
        messageJson["recipientId"] = message.recipientId;
        messageJson["topic"] = message.topic;
        messageJson["payload"] = message.payload;
        messageJson["sourceProtocol"] = static_cast<int>(message.sourceProtocol);
        messageJson["targetProtocol"] = static_cast<int>(message.targetProtocol);

        return validateMessageFormat(messageJson);
    } catch (const std::exception&) {
        return false;
    }
}

std::string FifoProtocolHandler::getValidationError(const Message& message) const {
    try {
        // Convert server Message struct to JSON for validation
        json messageJson;
        messageJson["senderId"] = message.senderId;
        messageJson["recipientId"] = message.recipientId;
        messageJson["topic"] = message.topic;
        messageJson["payload"] = message.payload;
        messageJson["sourceProtocol"] = static_cast<int>(message.sourceProtocol);
        messageJson["targetProtocol"] = static_cast<int>(message.targetProtocol);

        if (!validateMessageFormat(messageJson)) {
            return "Invalid message format";
        }
    } catch (const std::exception& e) {
        return "Message serialization failed: " + std::string(e.what());
    }

    return "";
}

bool FifoProtocolHandler::validateMessageSize(const Message& message) const {
    try {
        // Calculate approximate message size from server Message struct
        size_t messageSize = message.senderId.size() +
                           message.recipientId.size() +
                           message.topic.size() +
                           message.payload.size() +
                           100; // Overhead for JSON structure

        return messageSize <= config_.maxMessageSize;
    } catch (const std::exception&) {
        return false;
    }
}

// Protected methods implementation
void FifoProtocolHandler::processIncomingMessage(const std::string& clientId, const std::string& message) {
    spdlog::debug("Processing incoming message from client: {}", clientId);

    try {
        json messageJson = json::parse(message);
        auto msg = deserializeMessage(messageJson);

        if (msg) {
            // Process the message (this would typically involve business logic)
            spdlog::debug("Successfully processed message from client: {}", clientId);
        } else {
            spdlog::error("Failed to deserialize message from client: {}", clientId);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error processing message from client {}: {}", clientId, e.what());
        onClientError(clientId, "Message processing error: " + std::string(e.what()));
    }
}

void FifoProtocolHandler::processOutgoingMessage(const std::string& clientId, const Message& message) {
    spdlog::debug("Processing outgoing message to client: {}", clientId);

    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clients_.find(clientId);

    if (it != clients_.end() && it->second->active.load() && it->second->communicator) {
        json messageJson = serializeMessage(message);
        std::string messageStr = messageJson.dump();

        if (it->second->communicator->sendMessage(messageStr)) {
            it->second->messagesSent.fetch_add(1);
            spdlog::debug("Successfully sent message to client: {}", clientId);
        } else {
            spdlog::error("Failed to send message to client: {}", clientId);
            onClientError(clientId, "Failed to send message");
        }
    }
}

void FifoProtocolHandler::onClientConnected(const std::string& clientId) {
    spdlog::info("FIFO client connected: {}", clientId);
}

void FifoProtocolHandler::onClientDisconnected(const std::string& clientId) {
    spdlog::info("FIFO client disconnected: {}", clientId);
}

void FifoProtocolHandler::onClientError(const std::string& clientId, const std::string& error) {
    spdlog::error("FIFO client error for {}: {}", clientId, error);
    incrementErrorCount();
}

json FifoProtocolHandler::serializeMessage(const Message& message) const {
    json messageJson;
    messageJson["senderId"] = message.senderId;
    messageJson["recipientId"] = message.recipientId;
    messageJson["topic"] = message.topic;
    messageJson["payload"] = message.payload;
    messageJson["sourceProtocol"] = static_cast<int>(message.sourceProtocol);
    messageJson["targetProtocol"] = static_cast<int>(message.targetProtocol);
    messageJson["timestamp"] = message.timestamp;
    return messageJson;
}

std::unique_ptr<Message> FifoProtocolHandler::deserializeMessage(const json& messageJson) const {
    try {
        auto message = std::make_unique<Message>();

        if (messageJson.contains("senderId")) {
            message->senderId = messageJson["senderId"].get<std::string>();
        }

        if (messageJson.contains("recipientId")) {
            message->recipientId = messageJson["recipientId"].get<std::string>();
        }

        if (messageJson.contains("topic")) {
            message->topic = messageJson["topic"].get<std::string>();
        }

        if (messageJson.contains("payload")) {
            message->payload = messageJson["payload"].get<std::string>();
        }

        if (messageJson.contains("sourceProtocol")) {
            message->sourceProtocol = static_cast<CommunicationProtocol>(messageJson["sourceProtocol"].get<int>());
        }

        if (messageJson.contains("targetProtocol")) {
            message->targetProtocol = static_cast<CommunicationProtocol>(messageJson["targetProtocol"].get<int>());
        }

        if (messageJson.contains("timestamp")) {
            message->timestamp = messageJson["timestamp"].get<std::string>();
        }

        return message;
    } catch (const std::exception& e) {
        logError("Message deserialization failed: " + std::string(e.what()));
        return nullptr;
    }
}

bool FifoProtocolHandler::validateMessageFormat(const json& messageJson) const {
    // Basic format validation
    if (!messageJson.is_object()) {
        return false;
    }

    // Check required fields for server Message format
    if (!messageJson.contains("senderId") || !messageJson.contains("recipientId")) {
        return false;
    }

    // Validate sender and recipient IDs are strings
    if (!messageJson["senderId"].is_string() || !messageJson["recipientId"].is_string()) {
        return false;
    }

    // Validate topic if present
    if (messageJson.contains("topic") && !messageJson["topic"].is_string()) {
        return false;
    }

    // Validate payload if present
    if (messageJson.contains("payload") && !messageJson["payload"].is_string()) {
        return false;
    }

    return true;
}

bool FifoProtocolHandler::validateClientCommand(const std::string& command) const {
    if (!config_.enableCommandFiltering) {
        return true;
    }

    if (config_.allowedCommands.empty()) {
        return true; // No restrictions
    }

    return std::find(config_.allowedCommands.begin(), config_.allowedCommands.end(), command)
           != config_.allowedCommands.end();
}

bool FifoProtocolHandler::authenticateClient(const std::string& clientId, const std::string& token) const {
    if (!config_.enableClientAuthentication) {
        return true;
    }

    if (config_.authToken.empty()) {
        return true; // No token required
    }

    return token == config_.authToken;
}

void FifoProtocolHandler::logMessage(const std::string& direction, const Message& message, const std::string& clientId) const {
    spdlog::debug("FIFO {} - Client: {}, Sender: {}, Recipient: {}, Topic: {}",
                  direction, clientId, message.senderId, message.recipientId, message.topic);
}

void FifoProtocolHandler::logError(const std::string& error) const {
    spdlog::error("FIFO Protocol Handler Error: {}", error);
}

void FifoProtocolHandler::logDebug(const std::string& message) const {
    spdlog::debug("FIFO Protocol Handler: {}", message);
}

// Private helper methods
std::string FifoProtocolHandler::getPlatformSpecificPipePath(const std::string& basePath) const {
#ifdef _WIN32
    return config_.windowsBasePipePath;
#else
    return basePath;
#endif
}

bool FifoProtocolHandler::createPipeDirectory(const std::string& path) const {
#ifdef _WIN32
    // Windows named pipes don't need directory creation
    return true;
#else
    try {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create pipe directory: {}", e.what());
        return false;
    }
#endif
}

bool FifoProtocolHandler::setPipePermissions(const std::string& path) const {
#ifdef _WIN32
    // Windows permissions handled differently
    return true;
#else
    if (chmod(path.c_str(), 0666) == -1) {
        spdlog::error("Failed to set pipe permissions for {}: {}", path, strerror(errno));
        return false;
    }
    return true;
#endif
}

// Factory implementation
std::unique_ptr<FifoProtocolHandler> FifoProtocolHandlerFactory::create(const FifoProtocolConfig& config) {
    return std::make_unique<FifoProtocolHandler>(config);
}

std::unique_ptr<FifoProtocolHandler> FifoProtocolHandlerFactory::createDefault() {
    return create(FifoProtocolConfig{});
}

std::unique_ptr<FifoProtocolHandler> FifoProtocolHandlerFactory::createHighPerformance() {
    FifoProtocolConfig config;
    config.maxConcurrentClients = 100;
    config.workerThreadCount = 4;
    config.enableNonBlocking = true;
    config.maxQueueSize = 10000;
    config.enableMessageValidation = false; // Reduce overhead
    return create(config);
}

std::unique_ptr<FifoProtocolHandler> FifoProtocolHandlerFactory::createSecure() {
    FifoProtocolConfig config;
    config.enableClientAuthentication = true;
    config.enableCommandFiltering = true;
    config.enableMessageValidation = true;
    config.maxConcurrentClients = 10; // Limit for security
    return create(config);
}

std::unique_ptr<FifoProtocolHandler> FifoProtocolHandlerFactory::createDebug() {
    FifoProtocolConfig config;
    config.enableMessageLogging = true;
    config.enableAutoCleanup = false; // Keep connections for debugging
    config.clientTimeout = std::chrono::milliseconds(300000); // 5 minutes
    return create(config);
}

std::unique_ptr<FifoProtocolHandler> FifoProtocolHandlerFactory::createForWindows(const FifoProtocolConfig& config) {
    FifoProtocolConfig windowsConfig = config;
    windowsConfig.windowsBasePipePath = "\\\\.\\pipe\\hydrogen_fifo";
    return create(windowsConfig);
}

std::unique_ptr<FifoProtocolHandler> FifoProtocolHandlerFactory::createForUnix(const FifoProtocolConfig& config) {
    FifoProtocolConfig unixConfig = config;
    unixConfig.basePipePath = "/tmp/hydrogen_fifo";
    return create(unixConfig);
}

} // namespace fifo
} // namespace protocols
} // namespace server
} // namespace hydrogen

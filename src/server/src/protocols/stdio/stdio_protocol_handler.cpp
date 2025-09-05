#include "hydrogen/server/protocols/stdio/stdio_protocol_handler.h"
#include <hydrogen/core/utils.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <algorithm>
#include <sstream>

namespace hydrogen {
namespace server {
namespace protocols {
namespace stdio {

StdioProtocolHandler::StdioProtocolHandler(const StdioProtocolConfig& config)
    : config_(config) {
    messageTransformer_ = std::make_unique<MessageTransformer>();
    spdlog::info("StdioProtocolHandler initialized with buffer size: {}", config_.bufferSize);
}

StdioProtocolHandler::~StdioProtocolHandler() {
    // Cleanup all connections
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_.clear();
    spdlog::info("StdioProtocolHandler destroyed");
}

CommunicationProtocol StdioProtocolHandler::getProtocol() const {
    return CommunicationProtocol::STDIO;
}

std::string StdioProtocolHandler::getProtocolName() const {
    return "STDIO";
}

std::vector<std::string> StdioProtocolHandler::getSupportedMessageTypes() const {
    return {
        "COMMAND",
        "RESPONSE", 
        "EVENT",
        "ERROR",
        "DISCOVERY_REQUEST",
        "DISCOVERY_RESPONSE",
        "REGISTRATION",
        "AUTHENTICATION"
    };
}

bool StdioProtocolHandler::canHandle(const Message& message) const {
    // Stdio can handle all message types
    return true;
}

bool StdioProtocolHandler::processIncomingMessage(const Message& message) {
    try {
        // Validate message
        if (!validateMessage(message)) {
            std::string error = getValidationError(message);
            logError("Message validation failed: " + error, message.getDeviceId());
            return false;
        }

        // Update statistics
        totalMessagesProcessed_.fetch_add(1);
        updateConnectionActivity(message.getDeviceId());

        // Log message
        logMessage("INCOMING", message, message.getDeviceId());

        // Call message callback if set
        if (messageCallback_) {
            messageCallback_(message, message.getDeviceId());
        }

        return true;
    } catch (const std::exception& e) {
        logError("Failed to process incoming message: " + std::string(e.what()), message.getDeviceId());
        return false;
    }
}

bool StdioProtocolHandler::processOutgoingMessage(Message& message) {
    try {
        // Validate message
        if (!validateMessage(message)) {
            std::string error = getValidationError(message);
            logError("Outgoing message validation failed: " + error, message.getDeviceId());
            return false;
        }

        // Update statistics
        totalMessagesProcessed_.fetch_add(1);
        updateConnectionActivity(message.getDeviceId());

        // Log message
        logMessage("OUTGOING", message, message.getDeviceId());

        return true;
    } catch (const std::exception& e) {
        logError("Failed to process outgoing message: " + std::string(e.what()), message.getDeviceId());
        return false;
    }
}

bool StdioProtocolHandler::validateMessage(const Message& message) const {
    // Check message size
    if (!validateMessageSize(message)) {
        return false;
    }

    // Check if client is connected (if authentication is enabled)
    if (config_.enableAuthentication) {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        auto it = connections_.find(message.getDeviceId());
        if (it == connections_.end() || !it->second->isAuthenticated) {
            return false;
        }
    }

    // Validate message format
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

std::string StdioProtocolHandler::getValidationError(const Message& message) const {
    if (!validateMessageSize(message)) {
        return "Message size exceeds maximum allowed size";
    }

    if (config_.enableAuthentication) {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        auto it = connections_.find(message.getDeviceId());
        if (it == connections_.end()) {
            return "Client not connected";
        }
        if (!it->second->isAuthenticated) {
            return "Client not authenticated";
        }
    }

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

Message StdioProtocolHandler::transformMessage(const Message& source, 
                                             CommunicationProtocol targetProtocol) const {
    // For now, return a copy of the source message
    // In the future, this could transform between different protocol formats
    Message transformed = source;
    return transformed;
}

bool StdioProtocolHandler::handleClientConnect(const ConnectionInfo& connection) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    auto stdioConnection = std::make_unique<StdioConnectionInfo>();
    stdioConnection->clientId = connection.clientId;
    stdioConnection->connectedAt = std::chrono::system_clock::now();
    stdioConnection->lastActivity = stdioConnection->connectedAt;
    stdioConnection->isActive = true;
    stdioConnection->isAuthenticated = !config_.enableAuthentication; // Auto-authenticate if auth is disabled
    stdioConnection->metadata = connection.metadata;

    connections_[connection.clientId] = std::move(stdioConnection);

    spdlog::info("Stdio client connected: {}", connection.clientId);

    if (connectionCallback_) {
        connectionCallback_(connection.clientId, true);
    }

    return true;
}

bool StdioProtocolHandler::handleClientDisconnect(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    auto it = connections_.find(clientId);
    if (it != connections_.end()) {
        connections_.erase(it);
        spdlog::info("Stdio client disconnected: {}", clientId);

        if (connectionCallback_) {
            connectionCallback_(clientId, false);
        }

        return true;
    }

    return false;
}

void StdioProtocolHandler::setProtocolConfig(const std::unordered_map<std::string, std::string>& config) {
    // Update configuration from string map
    for (const auto& [key, value] : config) {
        if (key == "enableLineBuffering") {
            config_.enableLineBuffering = (value == "true");
        } else if (key == "enableBinaryMode") {
            config_.enableBinaryMode = (value == "true");
        } else if (key == "lineTerminator") {
            config_.lineTerminator = value;
        } else if (key == "enableEcho") {
            config_.enableEcho = (value == "true");
        } else if (key == "enableFlush") {
            config_.enableFlush = (value == "true");
        } else if (key == "encoding") {
            config_.encoding = value;
        } else if (key == "maxMessageSize") {
            config_.maxMessageSize = std::stoull(value);
        } else if (key == "bufferSize") {
            config_.bufferSize = std::stoull(value);
        } else if (key == "enableCompression") {
            config_.enableCompression = (value == "true");
        } else if (key == "enableAuthentication") {
            config_.enableAuthentication = (value == "true");
        } else if (key == "authToken") {
            config_.authToken = value;
        } else if (key == "connectionTimeout") {
            config_.connectionTimeout = std::stoi(value);
        } else if (key == "enableHeartbeat") {
            config_.enableHeartbeat = (value == "true");
        } else if (key == "heartbeatInterval") {
            config_.heartbeatInterval = std::stoi(value);
        }
    }

    spdlog::info("StdioProtocolHandler configuration updated");
}

std::unordered_map<std::string, std::string> StdioProtocolHandler::getProtocolConfig() const {
    std::unordered_map<std::string, std::string> config;
    config["enableLineBuffering"] = config_.enableLineBuffering ? "true" : "false";
    config["enableBinaryMode"] = config_.enableBinaryMode ? "true" : "false";
    config["lineTerminator"] = config_.lineTerminator;
    config["enableEcho"] = config_.enableEcho ? "true" : "false";
    config["enableFlush"] = config_.enableFlush ? "true" : "false";
    config["encoding"] = config_.encoding;
    config["maxMessageSize"] = std::to_string(config_.maxMessageSize);
    config["bufferSize"] = std::to_string(config_.bufferSize);
    config["enableCompression"] = config_.enableCompression ? "true" : "false";
    config["enableAuthentication"] = config_.enableAuthentication ? "true" : "false";
    config["authToken"] = config_.authToken;
    config["connectionTimeout"] = std::to_string(config_.connectionTimeout);
    config["enableHeartbeat"] = config_.enableHeartbeat ? "true" : "false";
    config["heartbeatInterval"] = std::to_string(config_.heartbeatInterval);
    return config;
}

// Stdio-specific methods implementation
void StdioProtocolHandler::setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
}

void StdioProtocolHandler::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = callback;
}

void StdioProtocolHandler::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = callback;
}

bool StdioProtocolHandler::isClientConnected(const std::string& clientId) const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto it = connections_.find(clientId);
    return it != connections_.end() && it->second->isActive;
}

std::vector<std::string> StdioProtocolHandler::getConnectedClients() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    std::vector<std::string> clients;
    for (const auto& [clientId, connection] : connections_) {
        if (connection->isActive) {
            clients.push_back(clientId);
        }
    }
    return clients;
}

StdioConnectionInfo StdioProtocolHandler::getConnectionInfo(const std::string& clientId) const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto it = connections_.find(clientId);
    if (it != connections_.end()) {
        return *it->second;
    }
    return StdioConnectionInfo{};
}

bool StdioProtocolHandler::sendMessage(const std::string& clientId, const Message& message) {
    if (!isClientConnected(clientId)) {
        logError("Cannot send message to disconnected client", clientId);
        return false;
    }

    try {
        // Transform message to stdio format
        auto result = messageTransformer_->transform(message, MessageFormat::STDIO);
        if (!result.success) {
            logError("Message transformation failed: " + result.errorMessage, clientId);
            return false;
        }

        // Update statistics
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        auto it = connections_.find(clientId);
        if (it != connections_.end()) {
            it->second->messagesSent++;
            it->second->bytesTransferred += result.transformedData.dump().size();
        }

        totalBytesTransferred_.fetch_add(result.transformedData.dump().size());
        logMessage("SENT", message, clientId);

        return true;
    } catch (const std::exception& e) {
        logError("Failed to send message: " + std::string(e.what()), clientId);
        return false;
    }
}

bool StdioProtocolHandler::broadcastMessage(const Message& message) {
    auto clients = getConnectedClients();
    bool allSuccess = true;

    for (const auto& clientId : clients) {
        if (!sendMessage(clientId, message)) {
            allSuccess = false;
        }
    }

    return allSuccess;
}

uint64_t StdioProtocolHandler::getTotalMessagesProcessed() const {
    return totalMessagesProcessed_.load();
}

uint64_t StdioProtocolHandler::getTotalBytesTransferred() const {
    return totalBytesTransferred_.load();
}

std::unordered_map<std::string, uint64_t> StdioProtocolHandler::getClientStatistics(const std::string& clientId) const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    std::unordered_map<std::string, uint64_t> stats;

    auto it = connections_.find(clientId);
    if (it != connections_.end()) {
        stats["messagesSent"] = it->second->messagesSent;
        stats["messagesReceived"] = it->second->messagesReceived;
        stats["bytesTransferred"] = it->second->bytesTransferred;
    }

    return stats;
}

void StdioProtocolHandler::updateConfig(const StdioProtocolConfig& config) {
    config_ = config;
    spdlog::info("StdioProtocolHandler configuration updated");
}

StdioProtocolConfig StdioProtocolHandler::getConfig() const {
    return config_;
}

// Private helper methods implementation
std::string StdioProtocolHandler::generateClientId() const {
    // Generate a unique client ID
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "stdio_client_" + std::to_string(timestamp);
}

bool StdioProtocolHandler::validateMessageSize(const Message& message) const {
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

bool StdioProtocolHandler::validateMessageFormat(const json& messageJson) const {
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

bool StdioProtocolHandler::authenticateClient(const std::string& clientId, const std::string& token) const {
    if (!config_.enableAuthentication) {
        return true;
    }

    return token == config_.authToken;
}

void StdioProtocolHandler::updateConnectionActivity(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto it = connections_.find(clientId);
    if (it != connections_.end()) {
        it->second->lastActivity = std::chrono::system_clock::now();
    }
}

void StdioProtocolHandler::cleanupInactiveConnections() {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::seconds(config_.connectionTimeout);

    auto it = connections_.begin();
    while (it != connections_.end()) {
        if (now - it->second->lastActivity > timeout) {
            spdlog::info("Cleaning up inactive stdio connection: {}", it->first);
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
}

json StdioProtocolHandler::serializeMessage(const Message& message) const {
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

std::unique_ptr<Message> StdioProtocolHandler::deserializeMessage(const json& messageJson) const {
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

bool StdioProtocolHandler::applyMessageFilters(const Message& message) const {
    // For now, accept all messages
    // In the future, this could implement filtering logic
    return true;
}

void StdioProtocolHandler::logMessage(const std::string& direction, const Message& message, const std::string& clientId) const {
    spdlog::debug("STDIO {} - Client: {}, Sender: {}, Recipient: {}, Topic: {}",
                  direction, clientId, message.senderId, message.recipientId, message.topic);
}

void StdioProtocolHandler::logError(const std::string& error, const std::string& clientId) const {
    if (clientId.empty()) {
        spdlog::error("StdioProtocolHandler: {}", error);
    } else {
        spdlog::error("StdioProtocolHandler [{}]: {}", clientId, error);
    }

    if (errorCallback_) {
        errorCallback_(error, clientId);
    }
}

} // namespace stdio
} // namespace protocols
} // namespace server
} // namespace hydrogen

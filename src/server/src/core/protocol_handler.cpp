#include "hydrogen/server/core/protocol_handler.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <algorithm>
#include <regex>

namespace hydrogen {
namespace server {
namespace core {

// ProtocolHandlerRegistry Implementation
ProtocolHandlerRegistry& ProtocolHandlerRegistry::getInstance() {
    static ProtocolHandlerRegistry instance;
    return instance;
}

void ProtocolHandlerRegistry::registerHandler(std::unique_ptr<IProtocolHandler> handler) {
    if (!handler) {
        spdlog::error("Cannot register null protocol handler");
        return;
    }
    
    std::lock_guard<std::mutex> lock(handlersMutex_);
    auto protocol = handler->getProtocol();
    
    if (handlers_.find(protocol) != handlers_.end()) {
        spdlog::warn("Replacing existing handler for protocol: {}", static_cast<int>(protocol));
    }
    
    handlers_[protocol] = std::move(handler);
    spdlog::info("Registered protocol handler: {}", handlers_[protocol]->getProtocolName());
}

void ProtocolHandlerRegistry::unregisterHandler(CommunicationProtocol protocol) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    auto it = handlers_.find(protocol);
    if (it != handlers_.end()) {
        spdlog::info("Unregistered protocol handler: {}", it->second->getProtocolName());
        handlers_.erase(it);
    }
}

IProtocolHandler* ProtocolHandlerRegistry::getHandler(CommunicationProtocol protocol) const {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    auto it = handlers_.find(protocol);
    return (it != handlers_.end()) ? it->second.get() : nullptr;
}

std::vector<CommunicationProtocol> ProtocolHandlerRegistry::getRegisteredProtocols() const {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    std::vector<CommunicationProtocol> protocols;
    protocols.reserve(handlers_.size());
    
    for (const auto& pair : handlers_) {
        protocols.push_back(pair.first);
    }
    
    return protocols;
}

bool ProtocolHandlerRegistry::isProtocolRegistered(CommunicationProtocol protocol) const {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    return handlers_.find(protocol) != handlers_.end();
}

bool ProtocolHandlerRegistry::processMessage(const Message& message) {
    auto handler = getHandler(message.sourceProtocol);
    if (!handler) {
        spdlog::error("No handler registered for protocol: {}", static_cast<int>(message.sourceProtocol));
        return false;
    }
    
    if (!handler->canHandle(message)) {
        spdlog::warn("Handler cannot process message from protocol: {}", static_cast<int>(message.sourceProtocol));
        return false;
    }
    
    return handler->processIncomingMessage(message);
}

Message ProtocolHandlerRegistry::transformMessage(const Message& source, CommunicationProtocol targetProtocol) const {
    auto handler = getHandler(source.sourceProtocol);
    if (!handler) {
        spdlog::error("No handler for source protocol: {}", static_cast<int>(source.sourceProtocol));
        return source; // Return original message if no handler
    }
    
    return handler->transformMessage(source, targetProtocol);
}

bool ProtocolHandlerRegistry::validateMessage(const Message& message, CommunicationProtocol protocol) const {
    auto handler = getHandler(protocol);
    if (!handler) {
        return false;
    }
    
    return handler->validateMessage(message);
}

std::string ProtocolHandlerRegistry::getValidationError(const Message& message, CommunicationProtocol protocol) const {
    auto handler = getHandler(protocol);
    if (!handler) {
        return "No handler registered for protocol";
    }
    
    return handler->getValidationError(message);
}

// BaseProtocolHandler Implementation
BaseProtocolHandler::BaseProtocolHandler(CommunicationProtocol protocol) 
    : protocol_(protocol) {
}

CommunicationProtocol BaseProtocolHandler::getProtocol() const {
    return protocol_;
}

std::string BaseProtocolHandler::getProtocolName() const {
    switch (protocol_) {
        case CommunicationProtocol::HTTP: return "HTTP";
        case CommunicationProtocol::WEBSOCKET: return "WebSocket";
        case CommunicationProtocol::MQTT: return "MQTT";
        case CommunicationProtocol::GRPC: return "gRPC";
        case CommunicationProtocol::ZMQ_REQ_REP: return "ZMQ-REQ-REP";
        case CommunicationProtocol::ZMQ_PUB_SUB: return "ZMQ-PUB-SUB";
        case CommunicationProtocol::ZMQ_PUSH_PULL: return "ZMQ-PUSH-PULL";
        case CommunicationProtocol::STDIO: return "STDIO";
        case CommunicationProtocol::FIFO: return "FIFO";
        default: return "Unknown";
    }
}

bool BaseProtocolHandler::validateMessage(const Message& message) const {
    // Basic validation
    if (message.senderId.empty()) {
        return false;
    }
    
    if (message.payload.empty()) {
        return false;
    }
    
    // Protocol-specific validation can be overridden
    return isValidClientId(message.senderId) && 
           isValidTopic(message.topic) && 
           isValidPayload(message.payload);
}

std::string BaseProtocolHandler::getValidationError(const Message& message) const {
    if (message.senderId.empty()) {
        return "Sender ID is required";
    }
    
    if (message.payload.empty()) {
        return "Message payload is required";
    }
    
    if (!isValidClientId(message.senderId)) {
        return "Invalid sender ID format";
    }
    
    if (!isValidTopic(message.topic)) {
        return "Invalid topic format";
    }
    
    if (!isValidPayload(message.payload)) {
        return "Invalid payload format";
    }
    
    return "";
}

void BaseProtocolHandler::setProtocolConfig(const std::unordered_map<std::string, std::string>& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

std::unordered_map<std::string, std::string> BaseProtocolHandler::getProtocolConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

bool BaseProtocolHandler::isValidClientId(const std::string& clientId) const {
    if (clientId.empty() || clientId.length() > 255) {
        return false;
    }
    
    // Allow alphanumeric characters, hyphens, and underscores
    std::regex clientIdPattern("^[a-zA-Z0-9_-]+$");
    return std::regex_match(clientId, clientIdPattern);
}

bool BaseProtocolHandler::isValidTopic(const std::string& topic) const {
    if (topic.empty() || topic.length() > 1024) {
        return false;
    }
    
    // Basic topic validation - can be overridden for protocol-specific rules
    return topic.find('\0') == std::string::npos;
}

bool BaseProtocolHandler::isValidPayload(const std::string& payload) const {
    // Basic payload validation - check for null bytes and reasonable size
    if (payload.length() > 1024 * 1024) { // 1MB limit
        return false;
    }
    
    return payload.find('\0') == std::string::npos;
}

void BaseProtocolHandler::logMessage(const std::string& level, const std::string& message) const {
    if (level == "trace") {
        spdlog::trace("[{}] {}", getProtocolName(), message);
    } else if (level == "debug") {
        spdlog::debug("[{}] {}", getProtocolName(), message);
    } else if (level == "info") {
        spdlog::info("[{}] {}", getProtocolName(), message);
    } else if (level == "warn") {
        spdlog::warn("[{}] {}", getProtocolName(), message);
    } else if (level == "error") {
        spdlog::error("[{}] {}", getProtocolName(), message);
    } else {
        spdlog::info("[{}] {}", getProtocolName(), message);
    }
}

void BaseProtocolHandler::updateStatistics(const std::string& operation, bool success) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    operationCounts_[operation]++;
    if (!success) {
        errorCounts_[operation]++;
    }
}

} // namespace core
} // namespace server
} // namespace hydrogen

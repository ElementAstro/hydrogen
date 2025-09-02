#include "hydrogen/core/message_transformer.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace hydrogen {
namespace core {

// MessageTransformer implementation
MessageTransformer::MessageTransformer() {
    initializeDefaultTransformers();
}

void MessageTransformer::registerTransformer(MessageFormat format, std::unique_ptr<ProtocolTransformer> transformer) {
    transformers_[format] = std::move(transformer);
    spdlog::debug("Registered transformer for format: {}", static_cast<int>(format));
}

void MessageTransformer::registerValidator(MessageFormat format, std::unique_ptr<MessageValidator> validator) {
    validators_[format] = std::move(validator);
    spdlog::debug("Registered validator for format: {}", static_cast<int>(format));
}

TransformationResult MessageTransformer::transform(const Message& message, MessageFormat targetFormat) const {
    TransformationResult result;
    
    auto it = transformers_.find(targetFormat);
    if (it == transformers_.end()) {
        result.errorMessage = "No transformer registered for target format: " + std::to_string(static_cast<int>(targetFormat));
        spdlog::error(result.errorMessage);
        return result;
    }
    
    try {
        result = it->second->toProtocol(message);
        if (result.success) {
            spdlog::trace("Successfully transformed message {} to format {}", 
                         message.getMessageId(), static_cast<int>(targetFormat));
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "Transformation failed: " + std::string(e.what());
        spdlog::error("Failed to transform message {}: {}", message.getMessageId(), e.what());
    }
    
    return result;
}

std::unique_ptr<Message> MessageTransformer::transformToInternal(const json& protocolMessage, MessageFormat sourceFormat) const {
    auto it = transformers_.find(sourceFormat);
    if (it == transformers_.end()) {
        spdlog::error("No transformer registered for source format: {}", static_cast<int>(sourceFormat));
        return nullptr;
    }
    
    try {
        auto result = it->second->fromProtocol(protocolMessage);
        if (result) {
            spdlog::trace("Successfully transformed protocol message to internal format");
        }
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Failed to transform protocol message to internal format: {}", e.what());
        return nullptr;
    }
}

bool MessageTransformer::validateMessage(const json& message, MessageFormat format) const {
    auto it = validators_.find(format);
    if (it == validators_.end()) {
        spdlog::warn("No validator registered for format: {}", static_cast<int>(format));
        return true; // Assume valid if no validator
    }
    
    return it->second->validate(message);
}

std::string MessageTransformer::getValidationError(const json& message, MessageFormat format) const {
    auto it = validators_.find(format);
    if (it == validators_.end()) {
        return "No validator registered for format";
    }
    
    return it->second->getValidationError(message);
}

bool MessageTransformer::isFormatSupported(MessageFormat format) const {
    return transformers_.find(format) != transformers_.end();
}

std::vector<MessageFormat> MessageTransformer::getSupportedFormats() const {
    std::vector<MessageFormat> formats;
    for (const auto& pair : transformers_) {
        formats.push_back(pair.first);
    }
    return formats;
}

json MessageTransformer::normalizeMessage(const json& message, MessageFormat format) const {
    json normalized = message;
    
    // Ensure consistent timestamp format
    if (normalized.contains("timestamp")) {
        // Convert to ISO 8601 format if not already
        if (normalized["timestamp"].is_number()) {
            auto timestamp = std::chrono::system_clock::from_time_t(normalized["timestamp"]);
            auto time_t = std::chrono::system_clock::to_time_t(timestamp);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
            normalized["timestamp"] = ss.str();
        }
    }
    
    // Ensure message ID exists
    if (!normalized.contains("messageId") && !normalized.contains("id")) {
        normalized["messageId"] = "msg_" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    // Normalize priority values
    if (normalized.contains("priority")) {
        if (normalized["priority"].is_string()) {
            std::string priority = normalized["priority"];
            if (priority == "low" || priority == "LOW") normalized["priority"] = 0;
            else if (priority == "normal" || priority == "NORMAL") normalized["priority"] = 1;
            else if (priority == "high" || priority == "HIGH") normalized["priority"] = 2;
            else if (priority == "critical" || priority == "CRITICAL") normalized["priority"] = 3;
        }
    }
    
    return normalized;
}

MessageFormat MessageTransformer::detectFormat(const json& message) const {
    // Detection logic based on message structure
    if (message.contains("sender_id") && message.contains("recipient_id") && message.contains("type")) {
        return MessageFormat::PROTOBUF;
    }
    if (message.contains("topic") && message.contains("payload") && message.contains("qos")) {
        return MessageFormat::MQTT;
    }
    if (message.contains("clientId") && message.contains("content") && message.contains("type")) {
        return MessageFormat::ZEROMQ;
    }
    if (message.contains("senderId") && message.contains("recipientId") && message.contains("messageType")) {
        return MessageFormat::COMMUNICATION_SERVICE;
    }
    if (message.contains("messageType") && message.contains("messageId")) {
        return MessageFormat::INTERNAL;
    }
    
    return MessageFormat::HTTP_JSON; // Default fallback
}

void MessageTransformer::initializeDefaultTransformers() {
    // Register default transformers
    registerTransformer(MessageFormat::PROTOBUF, std::make_unique<ProtobufTransformer>());
    registerTransformer(MessageFormat::MQTT, std::make_unique<MqttTransformer>());
    registerTransformer(MessageFormat::ZEROMQ, std::make_unique<ZeroMqTransformer>());
    registerTransformer(MessageFormat::HTTP_JSON, std::make_unique<HttpJsonTransformer>());
    registerTransformer(MessageFormat::COMMUNICATION_SERVICE, std::make_unique<CommunicationServiceTransformer>());
    
    spdlog::info("Initialized default message transformers");
}

// ProtobufTransformer implementation
TransformationResult ProtobufTransformer::toProtocol(const Message& internalMessage) const {
    TransformationResult result;

    try {
        json protobufJson;
        protobufJson["id"] = internalMessage.getMessageId();
        protobufJson["sender_id"] = internalMessage.getDeviceId();
        protobufJson["recipient_id"] = ""; // Will be set by caller
        protobufJson["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Map message types
        switch (internalMessage.getMessageType()) {
            case MessageType::COMMAND:
                protobufJson["type"] = 1; // COMMAND
                break;
            case MessageType::RESPONSE:
                protobufJson["type"] = 2; // STATUS
                break;
            case MessageType::EVENT:
                protobufJson["type"] = 3; // ALERT
                break;
            case MessageType::ERR:
                protobufJson["type"] = 3; // ALERT
                break;
            default:
                protobufJson["type"] = 0; // TEXT
        }

        // Map priority
        switch (internalMessage.getPriority()) {
            case Message::Priority::LOW:
                protobufJson["priority"] = 0;
                break;
            case Message::Priority::NORMAL:
                protobufJson["priority"] = 1;
                break;
            case Message::Priority::HIGH:
                protobufJson["priority"] = 2;
                break;
            case Message::Priority::CRITICAL:
                protobufJson["priority"] = 3;
                break;
        }

        protobufJson["status"] = 0; // PENDING
        protobufJson["content"] = internalMessage.toJson().dump();

        result.success = true;
        result.transformedData = protobufJson;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "ProtobufTransformer::toProtocol failed: " + std::string(e.what());
    }

    return result;
}

std::unique_ptr<Message> ProtobufTransformer::fromProtocol(const json& protocolMessage) const {
    try {
        // Parse the content field which contains the original internal message
        json internalJson = json::parse(protocolMessage["content"].get<std::string>());

        auto message = createMessageFromJson(internalJson);

        return message;

    } catch (const std::exception& e) {
        spdlog::error("ProtobufTransformer::fromProtocol failed: {}", e.what());
        return nullptr;
    }
}

std::unordered_map<std::string, std::string> ProtobufTransformer::getProtocolMetadata() const {
    return {
        {"protocol", "protobuf"},
        {"version", "3.0"},
        {"encoding", "binary"},
        {"content_type", "application/x-protobuf"}
    };
}

// MqttTransformer implementation
TransformationResult MqttTransformer::toProtocol(const Message& internalMessage) const {
    TransformationResult result;

    try {
        json mqttJson;
        mqttJson["id"] = internalMessage.getMessageId();
        mqttJson["topic"] = "hydrogen/device/" + internalMessage.getDeviceId();
        mqttJson["payload"] = internalMessage.toJson().dump();

        // Map QoS levels
        switch (internalMessage.getQoSLevel()) {
            case Message::QoSLevel::AT_MOST_ONCE:
                mqttJson["qos"] = 0;
                break;
            case Message::QoSLevel::AT_LEAST_ONCE:
                mqttJson["qos"] = 1;
                break;
            case Message::QoSLevel::EXACTLY_ONCE:
                mqttJson["qos"] = 2;
                break;
        }

        mqttJson["retain"] = (internalMessage.getPriority() == Message::Priority::CRITICAL);
        mqttJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        result.success = true;
        result.transformedData = mqttJson;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "MqttTransformer::toProtocol failed: " + std::string(e.what());
    }

    return result;
}

std::unique_ptr<Message> MqttTransformer::fromProtocol(const json& protocolMessage) const {
    try {
        json internalJson = json::parse(protocolMessage["payload"].get<std::string>());
        auto message = createMessageFromJson(internalJson);
        return message;

    } catch (const std::exception& e) {
        spdlog::error("MqttTransformer::fromProtocol failed: {}", e.what());
        return nullptr;
    }
}

std::unordered_map<std::string, std::string> MqttTransformer::getProtocolMetadata() const {
    return {
        {"protocol", "mqtt"},
        {"version", "3.1.1"},
        {"encoding", "utf-8"},
        {"content_type", "application/json"}
    };
}

// ZeroMqTransformer implementation
TransformationResult ZeroMqTransformer::toProtocol(const Message& internalMessage) const {
    TransformationResult result;

    try {
        json zmqJson;
        zmqJson["id"] = internalMessage.getMessageId();
        zmqJson["content"] = internalMessage.toJson().dump();
        zmqJson["clientId"] = internalMessage.getDeviceId();

        // Map message types to ZMQ types
        switch (internalMessage.getMessageType()) {
            case MessageType::COMMAND:
            case MessageType::RESPONSE:
                zmqJson["type"] = 0; // DATA
                break;
            case MessageType::EVENT:
                zmqJson["type"] = 3; // BROADCAST
                break;
            case MessageType::ERR:
                zmqJson["type"] = 1; // CONTROL
                break;
            default:
                zmqJson["type"] = 0; // DATA
        }

        zmqJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Add metadata
        json metadata;
        metadata["priority"] = static_cast<int>(internalMessage.getPriority());
        metadata["qos"] = static_cast<int>(internalMessage.getQoSLevel());
        zmqJson["metadata"] = metadata;

        result.success = true;
        result.transformedData = zmqJson;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "ZeroMqTransformer::toProtocol failed: " + std::string(e.what());
    }

    return result;
}

std::unique_ptr<Message> ZeroMqTransformer::fromProtocol(const json& protocolMessage) const {
    try {
        json internalJson = json::parse(protocolMessage["content"].get<std::string>());
        auto message = createMessageFromJson(internalJson);
        return message;

    } catch (const std::exception& e) {
        spdlog::error("ZeroMqTransformer::fromProtocol failed: {}", e.what());
        return nullptr;
    }
}

std::unordered_map<std::string, std::string> ZeroMqTransformer::getProtocolMetadata() const {
    return {
        {"protocol", "zeromq"},
        {"version", "4.3"},
        {"encoding", "utf-8"},
        {"content_type", "application/json"}
    };
}

// HttpJsonTransformer implementation
TransformationResult HttpJsonTransformer::toProtocol(const Message& internalMessage) const {
    TransformationResult result;

    try {
        // HTTP/WebSocket uses the internal JSON format directly with minor modifications
        json httpJson = internalMessage.toJson();

        // Ensure consistent field names for HTTP/WebSocket
        if (!httpJson.contains("id") && httpJson.contains("messageId")) {
            httpJson["id"] = httpJson["messageId"];
        }

        // Add HTTP-specific headers
        result.metadata["Content-Type"] = "application/json";
        result.metadata["X-Message-Protocol"] = "hydrogen-websocket";
        result.metadata["X-Message-Version"] = "1.0";

        result.success = true;
        result.transformedData = httpJson;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "HttpJsonTransformer::toProtocol failed: " + std::string(e.what());
    }

    return result;
}

std::unique_ptr<Message> HttpJsonTransformer::fromProtocol(const json& protocolMessage) const {
    try {
        auto message = createMessageFromJson(protocolMessage);
        return message;

    } catch (const std::exception& e) {
        spdlog::error("HttpJsonTransformer::fromProtocol failed: {}", e.what());
        return nullptr;
    }
}

std::unordered_map<std::string, std::string> HttpJsonTransformer::getProtocolMetadata() const {
    return {
        {"protocol", "http-websocket"},
        {"version", "1.1"},
        {"encoding", "utf-8"},
        {"content_type", "application/json"}
    };
}

// CommunicationServiceTransformer implementation
TransformationResult CommunicationServiceTransformer::toProtocol(const Message& internalMessage) const {
    TransformationResult result;

    try {
        json commJson;
        commJson["id"] = internalMessage.getMessageId();
        commJson["senderId"] = internalMessage.getDeviceId();
        commJson["recipientId"] = ""; // Will be set by caller
        commJson["content"] = internalMessage.toJson().dump();
        commJson["messageType"] = messageTypeToString(internalMessage.getMessageType());

        // Map priority
        switch (internalMessage.getPriority()) {
            case Message::Priority::LOW:
                commJson["priority"] = "LOW";
                break;
            case Message::Priority::NORMAL:
                commJson["priority"] = "NORMAL";
                break;
            case Message::Priority::HIGH:
                commJson["priority"] = "HIGH";
                break;
            case Message::Priority::CRITICAL:
                commJson["priority"] = "URGENT";
                break;
        }

        commJson["status"] = "PENDING";

        // Add timestamps
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        commJson["timestamp"] = timestamp;
        commJson["sentAt"] = timestamp;
        commJson["deliveredAt"] = 0;
        commJson["readAt"] = 0;

        // Add metadata
        json metadata;
        metadata["qos"] = static_cast<int>(internalMessage.getQoSLevel());
        metadata["expireAfter"] = internalMessage.getExpireAfter();
        commJson["metadata"] = metadata;

        result.success = true;
        result.transformedData = commJson;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "CommunicationServiceTransformer::toProtocol failed: " + std::string(e.what());
    }

    return result;
}

std::unique_ptr<Message> CommunicationServiceTransformer::fromProtocol(const json& protocolMessage) const {
    try {
        json internalJson = json::parse(protocolMessage["content"].get<std::string>());
        auto message = createMessageFromJson(internalJson);
        return message;

    } catch (const std::exception& e) {
        spdlog::error("CommunicationServiceTransformer::fromProtocol failed: {}", e.what());
        return nullptr;
    }
}

std::unordered_map<std::string, std::string> CommunicationServiceTransformer::getProtocolMetadata() const {
    return {
        {"protocol", "communication-service"},
        {"version", "1.0"},
        {"encoding", "utf-8"},
        {"content_type", "application/json"}
    };
}

// Global instance
MessageTransformer& getGlobalMessageTransformer() {
    static MessageTransformer instance;
    return instance;
}

} // namespace core
} // namespace hydrogen

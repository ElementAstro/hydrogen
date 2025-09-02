#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include "message.h"

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Protocol-specific message format enumeration
 */
enum class MessageFormat {
    INTERNAL,           // Internal hydrogen::core::Message format
    PROTOBUF,          // Protocol Buffer format
    MQTT,              // MQTT message format
    ZEROMQ,            // ZeroMQ message format
    HTTP_JSON,         // HTTP/WebSocket JSON format
    COMMUNICATION_SERVICE  // Server communication service format
};

/**
 * @brief Message transformation result
 */
struct TransformationResult {
    bool success = false;
    std::string errorMessage;
    json transformedData;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Protocol-specific message validator
 */
class MessageValidator {
public:
    virtual ~MessageValidator() = default;
    virtual bool validate(const json& message) const = 0;
    virtual std::string getValidationError(const json& message) const = 0;
};

/**
 * @brief Protocol-specific message transformer
 */
class ProtocolTransformer {
public:
    virtual ~ProtocolTransformer() = default;
    
    // Transform from internal format to protocol format
    virtual TransformationResult toProtocol(const Message& internalMessage) const = 0;
    
    // Transform from protocol format to internal format
    virtual std::unique_ptr<Message> fromProtocol(const json& protocolMessage) const = 0;
    
    // Get protocol-specific metadata
    virtual std::unordered_map<std::string, std::string> getProtocolMetadata() const = 0;
};

/**
 * @brief Unified message transformation layer
 * 
 * This class provides centralized message transformation between different
 * protocol formats while maintaining consistency and validation.
 */
class MessageTransformer {
public:
    MessageTransformer();
    ~MessageTransformer() = default;

    // Register protocol-specific transformers
    void registerTransformer(MessageFormat format, std::unique_ptr<ProtocolTransformer> transformer);
    void registerValidator(MessageFormat format, std::unique_ptr<MessageValidator> validator);

    // Transform between formats
    TransformationResult transform(const Message& message, MessageFormat targetFormat) const;
    std::unique_ptr<Message> transformToInternal(const json& protocolMessage, MessageFormat sourceFormat) const;

    // Validation
    bool validateMessage(const json& message, MessageFormat format) const;
    std::string getValidationError(const json& message, MessageFormat format) const;

    // Utility methods
    bool isFormatSupported(MessageFormat format) const;
    std::vector<MessageFormat> getSupportedFormats() const;
    
    // Message normalization (ensures consistent field names and types)
    json normalizeMessage(const json& message, MessageFormat format) const;

private:
    std::unordered_map<MessageFormat, std::unique_ptr<ProtocolTransformer>> transformers_;
    std::unordered_map<MessageFormat, std::unique_ptr<MessageValidator>> validators_;
    
    // Helper methods
    MessageFormat detectFormat(const json& message) const;
    void initializeDefaultTransformers();
};

/**
 * @brief Protocol Buffer transformer implementation
 */
class ProtobufTransformer : public ProtocolTransformer {
public:
    TransformationResult toProtocol(const Message& internalMessage) const override;
    std::unique_ptr<Message> fromProtocol(const json& protocolMessage) const override;
    std::unordered_map<std::string, std::string> getProtocolMetadata() const override;
};

/**
 * @brief MQTT transformer implementation
 */
class MqttTransformer : public ProtocolTransformer {
public:
    TransformationResult toProtocol(const Message& internalMessage) const override;
    std::unique_ptr<Message> fromProtocol(const json& protocolMessage) const override;
    std::unordered_map<std::string, std::string> getProtocolMetadata() const override;
};

/**
 * @brief ZeroMQ transformer implementation
 */
class ZeroMqTransformer : public ProtocolTransformer {
public:
    TransformationResult toProtocol(const Message& internalMessage) const override;
    std::unique_ptr<Message> fromProtocol(const json& protocolMessage) const override;
    std::unordered_map<std::string, std::string> getProtocolMetadata() const override;
};

/**
 * @brief HTTP/WebSocket JSON transformer implementation
 */
class HttpJsonTransformer : public ProtocolTransformer {
public:
    TransformationResult toProtocol(const Message& internalMessage) const override;
    std::unique_ptr<Message> fromProtocol(const json& protocolMessage) const override;
    std::unordered_map<std::string, std::string> getProtocolMetadata() const override;
};

/**
 * @brief Communication Service transformer implementation
 */
class CommunicationServiceTransformer : public ProtocolTransformer {
public:
    TransformationResult toProtocol(const Message& internalMessage) const override;
    std::unique_ptr<Message> fromProtocol(const json& protocolMessage) const override;
    std::unordered_map<std::string, std::string> getProtocolMetadata() const override;
};

/**
 * @brief Global message transformer instance
 */
MessageTransformer& getGlobalMessageTransformer();

} // namespace core
} // namespace hydrogen

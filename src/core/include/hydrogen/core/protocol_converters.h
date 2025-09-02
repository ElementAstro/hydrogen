#pragma once

#include "message_transformer.h"
#include "message.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Protocol-specific message converter interface
 * 
 * This interface defines the contract for converting messages between
 * different protocol formats while maintaining semantic consistency.
 */
class ProtocolConverter {
public:
    virtual ~ProtocolConverter() = default;
    
    // Convert internal message to protocol-specific format
    virtual json convertToProtocol(const Message& message) const = 0;
    
    // Convert protocol-specific format to internal message
    virtual std::unique_ptr<Message> convertFromProtocol(const json& protocolData) const = 0;
    
    // Validate protocol-specific message format
    virtual bool validateProtocolMessage(const json& protocolData) const = 0;
    
    // Get protocol-specific error message
    virtual std::string getProtocolError(const json& protocolData) const = 0;
    
    // Get protocol metadata
    virtual std::unordered_map<std::string, std::string> getProtocolInfo() const = 0;
};

/**
 * @brief HTTP/WebSocket JSON converter
 * 
 * Handles conversion between internal messages and HTTP/WebSocket JSON format.
 * This format is used for REST API and WebSocket communication.
 */
class HttpWebSocketConverter : public ProtocolConverter {
public:
    json convertToProtocol(const Message& message) const override;
    std::unique_ptr<Message> convertFromProtocol(const json& protocolData) const override;
    bool validateProtocolMessage(const json& protocolData) const override;
    std::string getProtocolError(const json& protocolData) const override;
    std::unordered_map<std::string, std::string> getProtocolInfo() const override;

private:
    json convertCommandMessage(const CommandMessage& cmd) const;
    json convertResponseMessage(const ResponseMessage& resp) const;
    json convertEventMessage(const EventMessage& evt) const;
    json convertErrorMessage(const ErrorMessage& err) const;
};

/**
 * @brief MQTT message converter
 * 
 * Handles conversion between internal messages and MQTT topic/payload format.
 * Includes QoS mapping and topic routing.
 */
class MqttConverter : public ProtocolConverter {
public:
    json convertToProtocol(const Message& message) const override;
    std::unique_ptr<Message> convertFromProtocol(const json& protocolData) const override;
    bool validateProtocolMessage(const json& protocolData) const override;
    std::string getProtocolError(const json& protocolData) const override;
    std::unordered_map<std::string, std::string> getProtocolInfo() const override;

private:
    std::string generateTopic(const Message& message) const;
    int mapQoSLevel(Message::QoSLevel qos) const;
    Message::QoSLevel mapMqttQoS(int mqttQos) const;
    bool shouldRetain(const Message& message) const;
};

/**
 * @brief ZeroMQ message converter
 * 
 * Handles conversion between internal messages and ZeroMQ message format.
 * Supports different ZeroMQ socket patterns (REQ/REP, PUB/SUB, PUSH/PULL).
 */
class ZeroMqConverter : public ProtocolConverter {
public:
    enum class SocketPattern {
        REQ_REP,
        PUB_SUB,
        PUSH_PULL
    };
    
    explicit ZeroMqConverter(SocketPattern pattern = SocketPattern::REQ_REP);
    
    json convertToProtocol(const Message& message) const override;
    std::unique_ptr<Message> convertFromProtocol(const json& protocolData) const override;
    bool validateProtocolMessage(const json& protocolData) const override;
    std::string getProtocolError(const json& protocolData) const override;
    std::unordered_map<std::string, std::string> getProtocolInfo() const override;

private:
    SocketPattern socketPattern_;
    std::string getMessageTypeString(MessageType type) const;
    MessageType parseMessageType(const std::string& typeStr) const;
};

/**
 * @brief gRPC Protocol Buffer converter
 * 
 * Handles conversion between internal messages and gRPC Protocol Buffer format.
 * Uses the updated proto definitions for consistent message structure.
 */
class GrpcProtobufConverter : public ProtocolConverter {
public:
    json convertToProtocol(const Message& message) const override;
    std::unique_ptr<Message> convertFromProtocol(const json& protocolData) const override;
    bool validateProtocolMessage(const json& protocolData) const override;
    std::string getProtocolError(const json& protocolData) const override;
    std::unordered_map<std::string, std::string> getProtocolInfo() const override;

private:
    json convertToProtobufJson(const Message& message) const;
    json createCommandContent(const CommandMessage& cmd) const;
    json createResponseContent(const ResponseMessage& resp) const;
    json createEventContent(const EventMessage& evt) const;
    json createErrorContent(const ErrorMessage& err) const;
};

/**
 * @brief Communication Service converter
 * 
 * Handles conversion between internal messages and the server's
 * communication service format used for inter-service messaging.
 */
class CommunicationServiceConverter : public ProtocolConverter {
public:
    json convertToProtocol(const Message& message) const override;
    std::unique_ptr<Message> convertFromProtocol(const json& protocolData) const override;
    bool validateProtocolMessage(const json& protocolData) const override;
    std::string getProtocolError(const json& protocolData) const override;
    std::unordered_map<std::string, std::string> getProtocolInfo() const override;

private:
    std::string mapPriorityToString(Message::Priority priority) const;
    Message::Priority mapStringToPriority(const std::string& priorityStr) const;
    std::string mapStatusToString(MessageType type) const;
};

/**
 * @brief Protocol converter factory
 * 
 * Factory class for creating protocol-specific converters.
 * Provides centralized access to all converter implementations.
 */
class ProtocolConverterFactory {
public:
    static std::unique_ptr<ProtocolConverter> createConverter(MessageFormat format);
    static std::unique_ptr<ProtocolConverter> createHttpWebSocketConverter();
    static std::unique_ptr<ProtocolConverter> createMqttConverter();
    static std::unique_ptr<ProtocolConverter> createZeroMqConverter(ZeroMqConverter::SocketPattern pattern = ZeroMqConverter::SocketPattern::REQ_REP);
    static std::unique_ptr<ProtocolConverter> createGrpcProtobufConverter();
    static std::unique_ptr<ProtocolConverter> createCommunicationServiceConverter();
    
    // Get list of supported formats
    static std::vector<MessageFormat> getSupportedFormats();
    
    // Check if format is supported
    static bool isFormatSupported(MessageFormat format);
};

/**
 * @brief Converter registry for managing protocol converters
 * 
 * Centralized registry for managing and accessing protocol converters.
 * Provides caching and lifecycle management for converter instances.
 */
class ConverterRegistry {
public:
    static ConverterRegistry& getInstance();
    
    // Register a converter for a specific format
    void registerConverter(MessageFormat format, std::unique_ptr<ProtocolConverter> converter);
    
    // Get converter for a specific format
    ProtocolConverter* getConverter(MessageFormat format);
    
    // Check if converter is registered
    bool hasConverter(MessageFormat format) const;
    
    // Get all registered formats
    std::vector<MessageFormat> getRegisteredFormats() const;
    
    // Initialize default converters
    void initializeDefaultConverters();

private:
    ConverterRegistry() = default;
    std::unordered_map<MessageFormat, std::unique_ptr<ProtocolConverter>> converters_;
    mutable std::mutex mutex_;
};

} // namespace core
} // namespace hydrogen

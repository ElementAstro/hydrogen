#include "hydrogen/core/communication/infrastructure/protocol_converters.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace hydrogen {
namespace core {

// HttpWebSocketConverter implementation
json HttpWebSocketConverter::convertToProtocol(const Message &message) const {
  json result = message.toJson();

  // Ensure consistent field names for HTTP/WebSocket
  if (!result.contains("id") && result.contains("messageId")) {
    result["id"] = result["messageId"];
  }

  // Add HTTP-specific metadata
  result["protocol"] = "http-websocket";
  result["version"] = "1.0";

  return result;
}

std::unique_ptr<Message>
HttpWebSocketConverter::convertFromProtocol(const json &protocolData) const {
  try {
    return createMessageFromJson(protocolData);
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("HttpWebSocketConverter::convertFromProtocol failed: {}",
                  e.what());
#endif
    return nullptr;
  }
}

bool HttpWebSocketConverter::validateProtocolMessage(
    const json &protocolData) const {
  return protocolData.contains("messageType") &&
         protocolData.contains("messageId") &&
         protocolData.contains("timestamp");
}

std::string
HttpWebSocketConverter::getProtocolError(const json &protocolData) const {
  if (!protocolData.contains("messageType"))
    return "Missing messageType field";
  if (!protocolData.contains("messageId"))
    return "Missing messageId field";
  if (!protocolData.contains("timestamp"))
    return "Missing timestamp field";
  return "";
}

std::unordered_map<std::string, std::string>
HttpWebSocketConverter::getProtocolInfo() const {
  return {{"protocol", "http-websocket"},
          {"version", "1.0"},
          {"encoding", "utf-8"},
          {"content_type", "application/json"}};
}

// MqttConverter implementation
json MqttConverter::convertToProtocol(const Message &message) const {
  json result;
  result["id"] = message.getMessageId();
  result["topic"] = generateTopic(message);
  result["payload"] = message.toJson().dump();
  result["qos"] = mapQoSLevel(message.getQoSLevel());
  result["retain"] = shouldRetain(message);
  result["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

  return result;
}

std::unique_ptr<Message>
MqttConverter::convertFromProtocol(const json &protocolData) const {
  try {
    json internalJson = json::parse(protocolData["payload"].get<std::string>());
    return createMessageFromJson(internalJson);
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("MqttConverter::convertFromProtocol failed: {}", e.what());
#endif
    return nullptr;
  }
}

bool MqttConverter::validateProtocolMessage(const json &protocolData) const {
  return protocolData.contains("topic") && protocolData.contains("payload") &&
         protocolData.contains("qos");
}

std::string MqttConverter::getProtocolError(const json &protocolData) const {
  if (!protocolData.contains("topic"))
    return "Missing topic field";
  if (!protocolData.contains("payload"))
    return "Missing payload field";
  if (!protocolData.contains("qos"))
    return "Missing qos field";
  return "";
}

std::unordered_map<std::string, std::string>
MqttConverter::getProtocolInfo() const {
  return {{"protocol", "mqtt"},
          {"version", "3.1.1"},
          {"encoding", "utf-8"},
          {"content_type", "application/json"}};
}

std::string MqttConverter::generateTopic(const Message &message) const {
  std::string baseTopic = "hydrogen/device/" + message.getDeviceId();

  switch (message.getMessageType()) {
  case MessageType::COMMAND:
    return baseTopic + "/command";
  case MessageType::RESPONSE:
    return baseTopic + "/response";
  case MessageType::EVENT:
    return baseTopic + "/event";
  case MessageType::ERR:
    return baseTopic + "/error";
  default:
    return baseTopic + "/general";
  }
}

int MqttConverter::mapQoSLevel(Message::QoSLevel qos) const {
  switch (qos) {
  case Message::QoSLevel::AT_MOST_ONCE:
    return 0;
  case Message::QoSLevel::AT_LEAST_ONCE:
    return 1;
  case Message::QoSLevel::EXACTLY_ONCE:
    return 2;
  default:
    return 0;
  }
}

Message::QoSLevel MqttConverter::mapMqttQoS(int mqttQos) const {
  switch (mqttQos) {
  case 0:
    return Message::QoSLevel::AT_MOST_ONCE;
  case 1:
    return Message::QoSLevel::AT_LEAST_ONCE;
  case 2:
    return Message::QoSLevel::EXACTLY_ONCE;
  default:
    return Message::QoSLevel::AT_MOST_ONCE;
  }
}

bool MqttConverter::shouldRetain(const Message &message) const {
  return message.getPriority() == Message::Priority::CRITICAL ||
         message.getMessageType() == MessageType::EVENT;
}

// ZeroMqConverter implementation
ZeroMqConverter::ZeroMqConverter(SocketPattern pattern)
    : socketPattern_(pattern) {}

json ZeroMqConverter::convertToProtocol(const Message &message) const {
  json result;
  result["id"] = message.getMessageId();
  result["content"] = message.toJson().dump();
  result["clientId"] = message.getDeviceId();
  result["type"] = getMessageTypeString(message.getMessageType());
  result["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

  // Add socket pattern specific metadata
  json metadata;
  metadata["pattern"] = static_cast<int>(socketPattern_);
  metadata["priority"] = static_cast<int>(message.getPriority());
  metadata["qos"] = static_cast<int>(message.getQoSLevel());
  result["metadata"] = metadata;

  return result;
}

std::unique_ptr<Message>
ZeroMqConverter::convertFromProtocol(const json &protocolData) const {
  try {
    json internalJson = json::parse(protocolData["content"].get<std::string>());
    return createMessageFromJson(internalJson);
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("ZeroMqConverter::convertFromProtocol failed: {}", e.what());
#endif
    return nullptr;
  }
}

bool ZeroMqConverter::validateProtocolMessage(const json &protocolData) const {
  return protocolData.contains("id") && protocolData.contains("content") &&
         protocolData.contains("clientId") && protocolData.contains("type");
}

std::string ZeroMqConverter::getProtocolError(const json &protocolData) const {
  if (!protocolData.contains("id"))
    return "Missing id field";
  if (!protocolData.contains("content"))
    return "Missing content field";
  if (!protocolData.contains("clientId"))
    return "Missing clientId field";
  if (!protocolData.contains("type"))
    return "Missing type field";
  return "";
}

std::unordered_map<std::string, std::string>
ZeroMqConverter::getProtocolInfo() const {
  return {{"protocol", "zeromq"},
          {"version", "4.3"},
          {"encoding", "utf-8"},
          {"content_type", "application/json"},
          {"pattern", std::to_string(static_cast<int>(socketPattern_))}};
}

std::string ZeroMqConverter::getMessageTypeString(MessageType type) const {
  switch (type) {
  case MessageType::COMMAND:
    return "COMMAND";
  case MessageType::RESPONSE:
    return "RESPONSE";
  case MessageType::EVENT:
    return "EVENT";
  case MessageType::ERR:
    return "ERROR";
  case MessageType::DISCOVERY_REQUEST:
    return "DISCOVERY_REQUEST";
  case MessageType::DISCOVERY_RESPONSE:
    return "DISCOVERY_RESPONSE";
  case MessageType::REGISTRATION:
    return "REGISTRATION";
  case MessageType::AUTHENTICATION:
    return "AUTHENTICATION";
  default:
    return "UNKNOWN";
  }
}

MessageType
ZeroMqConverter::parseMessageType(const std::string &typeStr) const {
  if (typeStr == "COMMAND")
    return MessageType::COMMAND;
  if (typeStr == "RESPONSE")
    return MessageType::RESPONSE;
  if (typeStr == "EVENT")
    return MessageType::EVENT;
  if (typeStr == "ERROR")
    return MessageType::ERR;
  if (typeStr == "DISCOVERY_REQUEST")
    return MessageType::DISCOVERY_REQUEST;
  if (typeStr == "DISCOVERY_RESPONSE")
    return MessageType::DISCOVERY_RESPONSE;
  if (typeStr == "REGISTRATION")
    return MessageType::REGISTRATION;
  if (typeStr == "AUTHENTICATION")
    return MessageType::AUTHENTICATION;
  return MessageType::COMMAND; // Default fallback
}

// GrpcProtobufConverter implementation
json GrpcProtobufConverter::convertToProtocol(const Message &message) const {
  return convertToProtobufJson(message);
}

std::unique_ptr<Message>
GrpcProtobufConverter::convertFromProtocol(const json &protocolData) const {
  try {
    // Extract the content from the appropriate oneof field
    json internalJson;

    if (protocolData.contains("command")) {
      // Reconstruct internal message from command content
      internalJson = json::parse(
          protocolData["command"]["parameters_json"].get<std::string>());
    } else if (protocolData.contains("response")) {
      internalJson = json::parse(
          protocolData["response"]["properties_json"].get<std::string>());
    } else if (protocolData.contains("event")) {
      internalJson = json::parse(
          protocolData["event"]["properties_json"].get<std::string>());
    } else if (protocolData.contains("error")) {
      internalJson =
          json::parse(protocolData["error"]["details_json"].get<std::string>());
    } else {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::error("GrpcProtobufConverter: Unknown message content type");
#endif
      return nullptr;
    }

    return createMessageFromJson(internalJson);
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("GrpcProtobufConverter::convertFromProtocol failed: {}",
                  e.what());
#endif
    return nullptr;
  }
}

bool GrpcProtobufConverter::validateProtocolMessage(
    const json &protocolData) const {
  return protocolData.contains("message_id") &&
         protocolData.contains("message_type") &&
         protocolData.contains("timestamp");
}

std::string
GrpcProtobufConverter::getProtocolError(const json &protocolData) const {
  if (!protocolData.contains("message_id"))
    return "Missing message_id field";
  if (!protocolData.contains("message_type"))
    return "Missing message_type field";
  if (!protocolData.contains("timestamp"))
    return "Missing timestamp field";
  return "";
}

std::unordered_map<std::string, std::string>
GrpcProtobufConverter::getProtocolInfo() const {
  return {{"protocol", "grpc-protobuf"},
          {"version", "3.0"},
          {"encoding", "binary"},
          {"content_type", "application/x-protobuf"}};
}

json GrpcProtobufConverter::convertToProtobufJson(
    const Message &message) const {
  json result;
  result["message_id"] = message.getMessageId();
  result["device_id"] = message.getDeviceId();
  result["timestamp"] = message.getTimestamp();
  result["original_message_id"] = message.getOriginalMessageId();
  result["message_type"] = static_cast<int>(message.getMessageType());
  result["priority"] = static_cast<int>(message.getPriority());
  result["qos_level"] = static_cast<int>(message.getQoSLevel());
  result["expire_after_seconds"] = message.getExpireAfter();

  // Add message-specific content based on type
  switch (message.getMessageType()) {
  case MessageType::COMMAND:
    result["command"] =
        createCommandContent(static_cast<const CommandMessage &>(message));
    break;
  case MessageType::RESPONSE:
    result["response"] =
        createResponseContent(static_cast<const ResponseMessage &>(message));
    break;
  case MessageType::EVENT:
    result["event"] =
        createEventContent(static_cast<const EventMessage &>(message));
    break;
  case MessageType::ERR:
    result["error"] =
        createErrorContent(static_cast<const ErrorMessage &>(message));
    break;
  default:
    // For other types, store as generic content
    result["content"] = message.toJson().dump();
  }

  return result;
}

json GrpcProtobufConverter::createCommandContent(
    const CommandMessage &cmd) const {
  json content;
  content["command"] = cmd.getCommand();
  content["parameters_json"] = cmd.getParameters().dump();
  content["properties_json"] = cmd.getProperties().dump();
  return content;
}

json GrpcProtobufConverter::createResponseContent(
    const ResponseMessage &resp) const {
  json content;
  content["status"] = resp.getStatus();
  content["command"] = resp.getCommand();
  content["properties_json"] = resp.getProperties().dump();
  content["details_json"] = resp.getDetails().dump();
  return content;
}

json GrpcProtobufConverter::createEventContent(const EventMessage &evt) const {
  json content;
  content["event_name"] = evt.getEventName();
  content["properties_json"] = evt.getProperties().dump();
  content["details_json"] = evt.getDetails().dump();
  content["related_message_id"] = evt.getRelatedMessageId();
  return content;
}

json GrpcProtobufConverter::createErrorContent(const ErrorMessage &err) const {
  json content;
  content["error_code"] = err.getErrorCode();
  content["error_message"] = err.getErrorMessage();
  content["details_json"] = err.getDetails().dump();
  return content;
}

// CommunicationServiceConverter implementation
json CommunicationServiceConverter::convertToProtocol(
    const Message &message) const {
  json result;
  result["id"] = message.getMessageId();
  result["senderId"] = message.getDeviceId();
  result["recipientId"] = ""; // Will be set by caller
  result["content"] = message.toJson().dump();
  result["messageType"] = messageTypeToString(message.getMessageType());
  result["priority"] = mapPriorityToString(message.getPriority());
  result["status"] = mapStatusToString(message.getMessageType());

  // Add timestamps
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch())
                       .count();
  result["timestamp"] = timestamp;
  result["sentAt"] = timestamp;
  result["deliveredAt"] = 0;
  result["readAt"] = 0;

  // Add metadata
  json metadata;
  metadata["qos"] = static_cast<int>(message.getQoSLevel());
  metadata["expireAfter"] = message.getExpireAfter();
  result["metadata"] = metadata;

  return result;
}

std::unique_ptr<Message> CommunicationServiceConverter::convertFromProtocol(
    const json &protocolData) const {
  try {
    json internalJson = json::parse(protocolData["content"].get<std::string>());
    return createMessageFromJson(internalJson);
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error(
        "CommunicationServiceConverter::convertFromProtocol failed: {}",
        e.what());
#endif
    return nullptr;
  }
}

bool CommunicationServiceConverter::validateProtocolMessage(
    const json &protocolData) const {
  return protocolData.contains("id") && protocolData.contains("senderId") &&
         protocolData.contains("content") &&
         protocolData.contains("messageType");
}

std::string CommunicationServiceConverter::getProtocolError(
    const json &protocolData) const {
  if (!protocolData.contains("id"))
    return "Missing id field";
  if (!protocolData.contains("senderId"))
    return "Missing senderId field";
  if (!protocolData.contains("content"))
    return "Missing content field";
  if (!protocolData.contains("messageType"))
    return "Missing messageType field";
  return "";
}

std::unordered_map<std::string, std::string>
CommunicationServiceConverter::getProtocolInfo() const {
  return {{"protocol", "communication-service"},
          {"version", "1.0"},
          {"encoding", "utf-8"},
          {"content_type", "application/json"}};
}

std::string CommunicationServiceConverter::mapPriorityToString(
    Message::Priority priority) const {
  switch (priority) {
  case Message::Priority::LOW:
    return "LOW";
  case Message::Priority::NORMAL:
    return "NORMAL";
  case Message::Priority::HIGH:
    return "HIGH";
  case Message::Priority::CRITICAL:
    return "URGENT";
  default:
    return "NORMAL";
  }
}

Message::Priority CommunicationServiceConverter::mapStringToPriority(
    const std::string &priorityStr) const {
  if (priorityStr == "LOW")
    return Message::Priority::LOW;
  if (priorityStr == "NORMAL")
    return Message::Priority::NORMAL;
  if (priorityStr == "HIGH")
    return Message::Priority::HIGH;
  if (priorityStr == "URGENT")
    return Message::Priority::CRITICAL;
  return Message::Priority::NORMAL;
}

std::string
CommunicationServiceConverter::mapStatusToString(MessageType type) const {
  switch (type) {
  case MessageType::COMMAND:
    return "PENDING";
  case MessageType::RESPONSE:
    return "DELIVERED";
  case MessageType::EVENT:
    return "SENT";
  case MessageType::ERR:
    return "FAILED";
  default:
    return "PENDING";
  }
}

// ProtocolConverterFactory implementation
std::unique_ptr<ProtocolConverter>
ProtocolConverterFactory::createConverter(MessageFormat format) {
  switch (format) {
  case MessageFormat::HTTP_JSON:
    return createHttpWebSocketConverter();
  case MessageFormat::MQTT:
    return createMqttConverter();
  case MessageFormat::ZEROMQ:
    return createZeroMqConverter();
  case MessageFormat::PROTOBUF:
    return createGrpcProtobufConverter();
  case MessageFormat::COMMUNICATION_SERVICE:
    return createCommunicationServiceConverter();
  default:
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("ProtocolConverterFactory: Unsupported format: {}",
                  static_cast<int>(format));
#endif
    return nullptr;
  }
}

std::unique_ptr<ProtocolConverter>
ProtocolConverterFactory::createHttpWebSocketConverter() {
  return std::make_unique<HttpWebSocketConverter>();
}

std::unique_ptr<ProtocolConverter>
ProtocolConverterFactory::createMqttConverter() {
  return std::make_unique<MqttConverter>();
}

std::unique_ptr<ProtocolConverter>
ProtocolConverterFactory::createZeroMqConverter(
    ZeroMqConverter::SocketPattern pattern) {
  return std::make_unique<ZeroMqConverter>(pattern);
}

std::unique_ptr<ProtocolConverter>
ProtocolConverterFactory::createGrpcProtobufConverter() {
  return std::make_unique<GrpcProtobufConverter>();
}

std::unique_ptr<ProtocolConverter>
ProtocolConverterFactory::createCommunicationServiceConverter() {
  return std::make_unique<CommunicationServiceConverter>();
}

std::vector<MessageFormat> ProtocolConverterFactory::getSupportedFormats() {
  return {MessageFormat::HTTP_JSON,
          MessageFormat::MQTT,
          MessageFormat::ZEROMQ,
          MessageFormat::PROTOBUF,
          MessageFormat::STDIO,
          MessageFormat::FIFO,
          MessageFormat::COMMUNICATION_SERVICE};
}

bool ProtocolConverterFactory::isFormatSupported(MessageFormat format) {
  auto formats = getSupportedFormats();
  return std::find(formats.begin(), formats.end(), format) != formats.end();
}

// ConverterRegistry implementation
ConverterRegistry &ConverterRegistry::getInstance() {
  static ConverterRegistry instance;
  return instance;
}

void ConverterRegistry::registerConverter(
    MessageFormat format, std::unique_ptr<ProtocolConverter> converter) {
  std::lock_guard<std::mutex> lock(mutex_);
  converters_[format] = std::move(converter);
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug("ConverterRegistry: Registered converter for format: {}",
                static_cast<int>(format));
#endif
}

ProtocolConverter *ConverterRegistry::getConverter(MessageFormat format) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = converters_.find(format);
  if (it != converters_.end()) {
    return it->second.get();
  }

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::warn("ConverterRegistry: No converter found for format: {}",
               static_cast<int>(format));
#endif
  return nullptr;
}

bool ConverterRegistry::hasConverter(MessageFormat format) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return converters_.find(format) != converters_.end();
}

std::vector<MessageFormat> ConverterRegistry::getRegisteredFormats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<MessageFormat> formats;
  for (const auto &pair : converters_) {
    formats.push_back(pair.first);
  }
  return formats;
}

void ConverterRegistry::initializeDefaultConverters() {
  registerConverter(MessageFormat::HTTP_JSON,
                    ProtocolConverterFactory::createHttpWebSocketConverter());
  registerConverter(MessageFormat::MQTT,
                    ProtocolConverterFactory::createMqttConverter());
  registerConverter(MessageFormat::ZEROMQ,
                    ProtocolConverterFactory::createZeroMqConverter());
  registerConverter(MessageFormat::PROTOBUF,
                    ProtocolConverterFactory::createGrpcProtobufConverter());
  registerConverter(
      MessageFormat::COMMUNICATION_SERVICE,
      ProtocolConverterFactory::createCommunicationServiceConverter());

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("ConverterRegistry: Initialized default protocol converters");
#endif
}

} // namespace core
} // namespace hydrogen

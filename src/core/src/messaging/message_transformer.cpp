#include "hydrogen/core/messaging/message_transformer.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>

namespace hydrogen {
namespace core {

// MessageTransformer implementation
MessageTransformer::MessageTransformer() { initializeDefaultTransformers(); }

void MessageTransformer::registerTransformer(
    MessageFormat format, std::unique_ptr<ProtocolTransformer> transformer) {
  transformers_[format] = std::move(transformer);
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug("Registered transformer for format: {}",
                static_cast<int>(format));
#endif
}

void MessageTransformer::registerValidator(
    MessageFormat format, std::unique_ptr<MessageValidator> validator) {
  validators_[format] = std::move(validator);
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug("Registered validator for format: {}",
                static_cast<int>(format));
#endif
}

TransformationResult
MessageTransformer::transform(const Message &message,
                              MessageFormat targetFormat) const {
  TransformationResult result;

  auto it = transformers_.find(targetFormat);
  if (it == transformers_.end()) {
    result.errorMessage = "No transformer registered for target format: " +
                          std::to_string(static_cast<int>(targetFormat));
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error(result.errorMessage);
#endif
    return result;
  }

  try {
    result = it->second->toProtocol(message);
    if (result.success) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::trace("Successfully transformed message {} to format {}",
                    message.getMessageId(), static_cast<int>(targetFormat));
#endif
    }
  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = "Transformation failed: " + std::string(e.what());
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("Failed to transform message {}: {}", message.getMessageId(),
                  e.what());
#endif
  }

  return result;
}

std::unique_ptr<Message>
MessageTransformer::transformToInternal(const json &protocolMessage,
                                        MessageFormat sourceFormat) const {
  auto it = transformers_.find(sourceFormat);
  if (it == transformers_.end()) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("No transformer registered for source format: {}",
                  static_cast<int>(sourceFormat));
#endif
    return nullptr;
  }

  try {
    auto result = it->second->fromProtocol(protocolMessage);
    if (result) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::trace(
          "Successfully transformed protocol message to internal format");
#endif
    }
    return result;
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("Failed to transform protocol message to internal format: {}",
                  e.what());
#endif
    return nullptr;
  }
}

bool MessageTransformer::validateMessage(const json &message,
                                         MessageFormat format) const {
  auto it = validators_.find(format);
  if (it == validators_.end()) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::warn("No validator registered for format: {}",
                 static_cast<int>(format));
#endif
    return true; // Assume valid if no validator
  }

  return it->second->validate(message);
}

std::string MessageTransformer::getValidationError(const json &message,
                                                   MessageFormat format) const {
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
  for (const auto &pair : transformers_) {
    formats.push_back(pair.first);
  }
  return formats;
}

json MessageTransformer::normalizeMessage(const json &message,
                                          MessageFormat format) const {
  json normalized = message;

  // Ensure consistent timestamp format
  if (normalized.contains("timestamp")) {
    // Convert to ISO 8601 format if not already
    if (normalized["timestamp"].is_number()) {
      auto timestamp =
          std::chrono::system_clock::from_time_t(normalized["timestamp"]);
      auto time_t = std::chrono::system_clock::to_time_t(timestamp);
      std::stringstream ss;
      ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
      normalized["timestamp"] = ss.str();
    }
  }

  // Ensure message ID exists
  if (!normalized.contains("messageId") && !normalized.contains("id")) {
    normalized["messageId"] =
        "msg_" +
        std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
  }

  // Normalize priority values
  if (normalized.contains("priority")) {
    if (normalized["priority"].is_string()) {
      std::string priority = normalized["priority"];
      if (priority == "low" || priority == "LOW")
        normalized["priority"] = 0;
      else if (priority == "normal" || priority == "NORMAL")
        normalized["priority"] = 1;
      else if (priority == "high" || priority == "HIGH")
        normalized["priority"] = 2;
      else if (priority == "critical" || priority == "CRITICAL")
        normalized["priority"] = 3;
    }
  }

  return normalized;
}

MessageFormat MessageTransformer::detectFormat(const json &message) const {
  // Detection logic based on message structure
  if (message.contains("sender_id") && message.contains("recipient_id") &&
      message.contains("type")) {
    return MessageFormat::PROTOBUF;
  }
  if (message.contains("topic") && message.contains("payload") &&
      message.contains("qos")) {
    return MessageFormat::MQTT;
  }
  if (message.contains("clientId") && message.contains("content") &&
      message.contains("type")) {
    return MessageFormat::ZEROMQ;
  }
  if (message.contains("device") && message.contains("type") &&
      message.contains("payload")) {
    return MessageFormat::STDIO;
  }
  if (message.contains("pipe") && message.contains("type") &&
      message.contains("payload")) {
    return MessageFormat::FIFO;
  }
  if (message.contains("senderId") && message.contains("recipientId") &&
      message.contains("messageType")) {
    return MessageFormat::COMMUNICATION_SERVICE;
  }
  if (message.contains("messageType") && message.contains("messageId")) {
    return MessageFormat::INTERNAL;
  }

  return MessageFormat::HTTP_JSON; // Default fallback
}

void MessageTransformer::initializeDefaultTransformers() {
  // Register default transformers
  registerTransformer(MessageFormat::PROTOBUF,
                      std::make_unique<ProtobufTransformer>());
  registerTransformer(MessageFormat::MQTT, std::make_unique<MqttTransformer>());
  registerTransformer(MessageFormat::ZEROMQ,
                      std::make_unique<ZeroMqTransformer>());
  registerTransformer(MessageFormat::HTTP_JSON,
                      std::make_unique<HttpJsonTransformer>());
  registerTransformer(MessageFormat::STDIO,
                      std::make_unique<StdioTransformer>());
  registerTransformer(MessageFormat::FIFO, std::make_unique<FifoTransformer>());
  registerTransformer(MessageFormat::COMMUNICATION_SERVICE,
                      std::make_unique<CommunicationServiceTransformer>());

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("Initialized default message transformers");
#endif
}

// ProtobufTransformer implementation
TransformationResult
ProtobufTransformer::toProtocol(const Message &internalMessage) const {
  TransformationResult result;

  try {
    json protobufJson;
    protobufJson["id"] = internalMessage.getMessageId();
    protobufJson["sender_id"] = internalMessage.getDeviceId();
    protobufJson["recipient_id"] = ""; // Will be set by caller
    protobufJson["timestamp"] =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

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

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage =
        "ProtobufTransformer::toProtocol failed: " + std::string(e.what());
  }

  return result;
}

std::unique_ptr<Message>
ProtobufTransformer::fromProtocol(const json &protocolMessage) const {
  try {
    // Parse the content field which contains the original internal message
    json internalJson =
        json::parse(protocolMessage["content"].get<std::string>());

    auto message = createMessageFromJson(internalJson);

    return message;

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("ProtobufTransformer::fromProtocol failed: {}", e.what());
#endif
    return nullptr;
  }
}

std::unordered_map<std::string, std::string>
ProtobufTransformer::getProtocolMetadata() const {
  return {{"protocol", "protobuf"},
          {"version", "3.0"},
          {"encoding", "binary"},
          {"content_type", "application/x-protobuf"}};
}

// MqttTransformer implementation
TransformationResult
MqttTransformer::toProtocol(const Message &internalMessage) const {
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

    mqttJson["retain"] =
        (internalMessage.getPriority() == Message::Priority::CRITICAL);
    mqttJson["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    result.success = true;
    result.transformedData = mqttJson;

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage =
        "MqttTransformer::toProtocol failed: " + std::string(e.what());
  }

  return result;
}

std::unique_ptr<Message>
MqttTransformer::fromProtocol(const json &protocolMessage) const {
  try {
    json internalJson =
        json::parse(protocolMessage["payload"].get<std::string>());
    auto message = createMessageFromJson(internalJson);
    return message;

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("MqttTransformer::fromProtocol failed: {}", e.what());
#endif
    return nullptr;
  }
}

std::unordered_map<std::string, std::string>
MqttTransformer::getProtocolMetadata() const {
  return {{"protocol", "mqtt"},
          {"version", "3.1.1"},
          {"encoding", "utf-8"},
          {"content_type", "application/json"}};
}

// ZeroMqTransformer implementation
TransformationResult
ZeroMqTransformer::toProtocol(const Message &internalMessage) const {
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

    zmqJson["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    // Add metadata
    json metadata;
    metadata["priority"] = static_cast<int>(internalMessage.getPriority());
    metadata["qos"] = static_cast<int>(internalMessage.getQoSLevel());
    zmqJson["metadata"] = metadata;

    result.success = true;
    result.transformedData = zmqJson;

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage =
        "ZeroMqTransformer::toProtocol failed: " + std::string(e.what());
  }

  return result;
}

std::unique_ptr<Message>
ZeroMqTransformer::fromProtocol(const json &protocolMessage) const {
  try {
    json internalJson =
        json::parse(protocolMessage["content"].get<std::string>());
    auto message = createMessageFromJson(internalJson);
    return message;

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("ZeroMqTransformer::fromProtocol failed: {}", e.what());
#endif
    return nullptr;
  }
}

std::unordered_map<std::string, std::string>
ZeroMqTransformer::getProtocolMetadata() const {
  return {{"protocol", "zeromq"},
          {"version", "4.3"},
          {"encoding", "utf-8"},
          {"content_type", "application/json"}};
}

// HttpJsonTransformer implementation
TransformationResult
HttpJsonTransformer::toProtocol(const Message &internalMessage) const {
  TransformationResult result;

  try {
    // HTTP/WebSocket uses the internal JSON format directly with minor
    // modifications
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

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage =
        "HttpJsonTransformer::toProtocol failed: " + std::string(e.what());
  }

  return result;
}

std::unique_ptr<Message>
HttpJsonTransformer::fromProtocol(const json &protocolMessage) const {
  try {
    auto message = createMessageFromJson(protocolMessage);
    return message;

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("HttpJsonTransformer::fromProtocol failed: {}", e.what());
#endif
    return nullptr;
  }
}

std::unordered_map<std::string, std::string>
HttpJsonTransformer::getProtocolMetadata() const {
  return {{"protocol", "http-websocket"},
          {"version", "1.1"},
          {"encoding", "utf-8"},
          {"content_type", "application/json"}};
}

// CommunicationServiceTransformer implementation
TransformationResult CommunicationServiceTransformer::toProtocol(
    const Message &internalMessage) const {
  TransformationResult result;

  try {
    json commJson;
    commJson["id"] = internalMessage.getMessageId();
    commJson["senderId"] = internalMessage.getDeviceId();
    commJson["recipientId"] = ""; // Will be set by caller
    commJson["content"] = internalMessage.toJson().dump();
    commJson["messageType"] =
        messageTypeToString(internalMessage.getMessageType());

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
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
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

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage =
        "CommunicationServiceTransformer::toProtocol failed: " +
        std::string(e.what());
  }

  return result;
}

std::unique_ptr<Message> CommunicationServiceTransformer::fromProtocol(
    const json &protocolMessage) const {
  try {
    json internalJson =
        json::parse(protocolMessage["content"].get<std::string>());
    auto message = createMessageFromJson(internalJson);
    return message;

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("CommunicationServiceTransformer::fromProtocol failed: {}",
                  e.what());
#endif
    return nullptr;
  }
}

std::unordered_map<std::string, std::string>
CommunicationServiceTransformer::getProtocolMetadata() const {
  return {{"protocol", "communication-service"},
          {"version", "1.0"},
          {"encoding", "utf-8"},
          {"content_type", "application/json"}};
}

// StdioTransformer implementation
TransformationResult
StdioTransformer::toProtocol(const Message &internalMessage) const {
  TransformationResult result;

  try {
    json stdioJson;

    // Create a simplified stdio-friendly format
    stdioJson["id"] = internalMessage.getMessageId();
    stdioJson["device"] = internalMessage.getDeviceId();
    stdioJson["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    // Map message types to simple strings
    switch (internalMessage.getMessageType()) {
    case MessageType::COMMAND:
      stdioJson["type"] = "command";
      break;
    case MessageType::RESPONSE:
      stdioJson["type"] = "response";
      break;
    case MessageType::EVENT:
      stdioJson["type"] = "event";
      break;
    case MessageType::ERR:
      stdioJson["type"] = "error";
      break;
    default:
      stdioJson["type"] = "message";
    }

    // Include the full message content as payload
    json messagePayload;
    messagePayload["messageType"] =
        static_cast<int>(internalMessage.getMessageType());
    messagePayload["messageId"] = internalMessage.getMessageId();
    messagePayload["deviceId"] = internalMessage.getDeviceId();
    messagePayload["timestamp"] = internalMessage.getTimestamp();
    messagePayload["originalMessageId"] =
        internalMessage.getOriginalMessageId();

    stdioJson["payload"] = messagePayload;

    // Add stdio-specific metadata
    result.metadata["Content-Type"] = "application/json";
    result.metadata["X-Protocol"] = "stdio";
    result.metadata["X-Version"] = "1.0";
    result.metadata["X-Encoding"] = "utf-8";

    result.success = true;
    result.transformedData = stdioJson;

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage =
        "StdioTransformer::toProtocol failed: " + std::string(e.what());
  }

  return result;
}

std::unique_ptr<Message>
StdioTransformer::fromProtocol(const json &protocolMessage) const {
  try {
    auto message = std::make_unique<Message>();

    // Extract the payload which contains the original internal message
    if (protocolMessage.contains("payload") &&
        protocolMessage["payload"].is_object()) {
      json internalJson = protocolMessage["payload"];

      // Reconstruct message from payload
      if (internalJson.contains("messageId")) {
        message->setMessageId(internalJson["messageId"].get<std::string>());
      }

      if (internalJson.contains("deviceId")) {
        message->setDeviceId(internalJson["deviceId"].get<std::string>());
      }

      if (internalJson.contains("timestamp")) {
        if (internalJson["timestamp"].is_string()) {
          message->setTimestamp(internalJson["timestamp"].get<std::string>());
        } else if (internalJson["timestamp"].is_number()) {
          // Convert numeric timestamp to string
          message->setTimestamp(std::to_string(internalJson["timestamp"].get<int64_t>()));
        }
      }

      if (internalJson.contains("originalMessageId")) {
        message->setOriginalMessageId(
            internalJson["originalMessageId"].get<std::string>());
      }

      if (internalJson.contains("messageType")) {
        int typeInt = internalJson["messageType"].get<int>();
        message->setMessageType(static_cast<MessageType>(typeInt));
      }
    } else {
      // If no payload, try to construct from stdio format
      if (protocolMessage.contains("id")) {
        message->setMessageId(protocolMessage["id"].get<std::string>());
      }

      if (protocolMessage.contains("device")) {
        message->setDeviceId(protocolMessage["device"].get<std::string>());
      }

      if (protocolMessage.contains("type")) {
        std::string typeStr = protocolMessage["type"].get<std::string>();
        if (typeStr == "command") {
          message->setMessageType(MessageType::COMMAND);
        } else if (typeStr == "response") {
          message->setMessageType(MessageType::RESPONSE);
        } else if (typeStr == "event") {
          message->setMessageType(MessageType::EVENT);
        } else if (typeStr == "error") {
          message->setMessageType(MessageType::ERR);
        } else {
          message->setMessageType(MessageType::COMMAND); // Default
        }
      }
    }

    return message;

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("StdioTransformer::fromProtocol failed: {}", e.what());
#endif
    return nullptr;
  }
}

std::unordered_map<std::string, std::string>
StdioTransformer::getProtocolMetadata() const {
  return {{"protocol", "stdio"},      {"version", "1.0"},
          {"encoding", "utf-8"},      {"content_type", "application/json"},
          {"line_terminator", "\\n"}, {"supports_binary", "false"}};
}

// FifoTransformer implementation
TransformationResult
FifoTransformer::toProtocol(const Message &internalMessage) const {
  TransformationResult result;

  try {
    json fifoJson;

    // FIFO protocol format - similar to STDIO but with FIFO-specific metadata
    fifoJson["pipe"] = "fifo_pipe"; // Pipe identifier
    fifoJson["type"] = static_cast<int>(internalMessage.getMessageType());
    fifoJson["id"] = internalMessage.getMessageId();
    fifoJson["device"] = internalMessage.getDeviceId();
    fifoJson["timestamp"] = internalMessage.getTimestamp();

    // Include the full message content as payload
    json messagePayload;
    messagePayload["messageType"] =
        static_cast<int>(internalMessage.getMessageType());
    messagePayload["messageId"] = internalMessage.getMessageId();
    messagePayload["deviceId"] = internalMessage.getDeviceId();
    messagePayload["timestamp"] = internalMessage.getTimestamp();
    messagePayload["originalMessageId"] =
        internalMessage.getOriginalMessageId();

    fifoJson["payload"] = messagePayload;

    result.success = true;
    result.transformedData = fifoJson;

    // Add FIFO-specific metadata
    result.metadata["Content-Type"] = "application/json";
    result.metadata["X-Protocol"] = "fifo";
    result.metadata["X-Framing"] = "json-lines";
    result.metadata["X-Pipe-Type"] = "named-pipe";

#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::debug("Successfully transformed message to FIFO protocol");
#endif

  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage =
        "FIFO transformation failed: " + std::string(e.what());
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("FifoTransformer::toProtocol failed: {}", e.what());
#endif
  }

  return result;
}

std::unique_ptr<Message>
FifoTransformer::fromProtocol(const json &protocolMessage) const {
  try {
    auto message = std::make_unique<Message>();

    // Extract the payload which contains the original internal message
    if (protocolMessage.contains("payload") &&
        protocolMessage["payload"].is_object()) {
      json internalJson = protocolMessage["payload"];

      // Reconstruct message from payload
      if (internalJson.contains("messageId")) {
        message->setMessageId(internalJson["messageId"].get<std::string>());
      }

      if (internalJson.contains("deviceId")) {
        message->setDeviceId(internalJson["deviceId"].get<std::string>());
      }

      if (internalJson.contains("timestamp")) {
        message->setTimestamp(internalJson["timestamp"].get<std::string>());
      }

      if (internalJson.contains("originalMessageId")) {
        message->setOriginalMessageId(
            internalJson["originalMessageId"].get<std::string>());
      }

      if (internalJson.contains("messageType")) {
        int typeInt = internalJson["messageType"].get<int>();
        message->setMessageType(static_cast<MessageType>(typeInt));
      }
    } else {
      // If no payload, try to construct from FIFO format
      if (protocolMessage.contains("id")) {
        message->setMessageId(protocolMessage["id"].get<std::string>());
      }

      if (protocolMessage.contains("device")) {
        message->setDeviceId(protocolMessage["device"].get<std::string>());
      }

      if (protocolMessage.contains("type")) {
        int typeInt = protocolMessage["type"].get<int>();
        message->setMessageType(static_cast<MessageType>(typeInt));
      }

      if (protocolMessage.contains("timestamp")) {
        message->setTimestamp(protocolMessage["timestamp"].get<std::string>());
      }
    }

    return message;

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("FifoTransformer::fromProtocol failed: {}", e.what());
#endif
    return nullptr;
  }
}

std::unordered_map<std::string, std::string>
FifoTransformer::getProtocolMetadata() const {
  std::unordered_map<std::string, std::string> metadata;

  metadata["protocol"] = "fifo";
  metadata["version"] = "1.0";
  metadata["encoding"] = "utf-8";
  metadata["content_type"] = "application/json";
  metadata["framing"] = "json-lines";
  metadata["pipe_type"] = "named-pipe";
  metadata["bidirectional"] = "true";
  metadata["cross_platform"] = "true";
  metadata["supports_binary"] = "false";
  metadata["supports_compression"] = "true";
  metadata["supports_encryption"] = "true";
  metadata["max_message_size"] = "1048576"; // 1MB default

  return metadata;
}

// Global instance
MessageTransformer &getGlobalMessageTransformer() {
  static MessageTransformer instance;
  return instance;
}

} // namespace core
} // namespace hydrogen

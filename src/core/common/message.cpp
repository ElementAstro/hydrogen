#include "../include/hydrogen/core/message.h"
#include "../include/hydrogen/core/utils.h"
#include <chrono>
#include <stdexcept>

namespace hydrogen {
namespace core {

std::string messageTypeToString(MessageType type) {
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

MessageType stringToMessageType(const std::string &typeStr) {
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

  throw std::invalid_argument("Unknown message type: " + typeStr);
}

// Base Message class implementation
Message::Message() : messageType(MessageType::COMMAND) {
  messageId = generateUuid();
  timestamp = getIsoTimestamp();
}

Message::Message(MessageType type) : messageType(type) {
  messageId = generateUuid();
  timestamp = getIsoTimestamp();
}

void Message::setMessageType(MessageType type) { messageType = type; }

MessageType Message::getMessageType() const { return messageType; }

void Message::setMessageId(const std::string &id) { messageId = id; }

std::string Message::getMessageId() const { return messageId; }

void Message::setDeviceId(const std::string &id) { deviceId = id; }

std::string Message::getDeviceId() const { return deviceId; }

void Message::setTimestamp(const std::string &ts) { timestamp = ts; }

std::string Message::getTimestamp() const { return timestamp; }

void Message::setOriginalMessageId(const std::string &id) {
  originalMessageId = id;
}

std::string Message::getOriginalMessageId() const { return originalMessageId; }

void Message::setQoSLevel(QoSLevel level) { qosLevel = level; }

Message::QoSLevel Message::getQoSLevel() const { return qosLevel; }

void Message::setPriority(Priority priority) { this->priority = priority; }

Message::Priority Message::getPriority() const { return priority; }

void Message::setExpireAfter(int seconds) { expireAfterSeconds = seconds; }

int Message::getExpireAfter() const { return expireAfterSeconds; }

bool Message::isExpired() const {
  if (expireAfterSeconds <= 0) {
    return false; // Never expires
  }

  auto now = std::chrono::system_clock::now();
  auto messageTime = string_utils::parseIsoTimestamp(timestamp);
  auto elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(now - messageTime);

  return elapsed.count() >= expireAfterSeconds;
}

json Message::toJson() const {
  json j = {{"messageType", messageTypeToString(messageType)},
            {"timestamp", timestamp},
            {"messageId", messageId}};

  if (!deviceId.empty()) {
    j["deviceId"] = deviceId;
  }

  if (!originalMessageId.empty()) {
    j["originalMessageId"] = originalMessageId;
  }

  // Add QoS related properties
  if (qosLevel != QoSLevel::AT_MOST_ONCE) {
    j["qos"] = static_cast<int>(qosLevel);
  }

  if (priority != Priority::NORMAL) {
    j["priority"] = static_cast<int>(priority);
  }

  if (expireAfterSeconds > 0) {
    j["expireAfter"] = expireAfterSeconds;
  }

  return j;
}

void Message::fromJson(const json &j) {
  messageType = stringToMessageType(j.at("messageType"));
  messageId = j.at("messageId");
  timestamp = j.at("timestamp");

  if (j.contains("deviceId")) {
    deviceId = j["deviceId"];
  }

  if (j.contains("originalMessageId")) {
    originalMessageId = j["originalMessageId"];
  }

  if (j.contains("qos")) {
    qosLevel = static_cast<QoSLevel>(j["qos"]);
  }

  if (j.contains("priority")) {
    priority = static_cast<Priority>(j["priority"]);
  }

  if (j.contains("expireAfter")) {
    expireAfterSeconds = j["expireAfter"];
  }
}

std::string Message::toString() const { return toJson().dump(); }

// CommandMessage implementation
CommandMessage::CommandMessage() : Message(MessageType::COMMAND) {}

CommandMessage::CommandMessage(const std::string &cmd)
    : Message(MessageType::COMMAND), command(cmd) {}

void CommandMessage::setCommand(const std::string &cmd) { command = cmd; }

std::string CommandMessage::getCommand() const { return command; }

void CommandMessage::setParameters(const json &params) { parameters = params; }

json CommandMessage::getParameters() const { return parameters; }

void CommandMessage::setProperties(const json &props) { properties = props; }

json CommandMessage::getProperties() const { return properties; }

json CommandMessage::toJson() const {
  json j = Message::toJson();
  j["command"] = command;
  if (!parameters.is_null()) {
    j["parameters"] = parameters;
  }
  if (!properties.is_null()) {
    j["properties"] = properties;
  }
  return j;
}

void CommandMessage::fromJson(const json &j) {
  Message::fromJson(j);
  command = j.at("command");
  if (j.contains("parameters")) {
    parameters = j["parameters"];
  }
  if (j.contains("properties")) {
    properties = j["properties"];
  }
}

// ResponseMessage implementation
ResponseMessage::ResponseMessage() : Message(MessageType::RESPONSE) {}

void ResponseMessage::setStatus(const std::string &status) {
  this->status = status;
}

std::string ResponseMessage::getStatus() const { return status; }

void ResponseMessage::setCommand(const std::string &cmd) { command = cmd; }

std::string ResponseMessage::getCommand() const { return command; }

void ResponseMessage::setProperties(const json &props) { properties = props; }

json ResponseMessage::getProperties() const { return properties; }

void ResponseMessage::setDetails(const json &details) {
  this->details = details;
}

json ResponseMessage::getDetails() const { return details; }

json ResponseMessage::toJson() const {
  json j = Message::toJson();
  j["status"] = status;
  if (!command.empty()) {
    j["command"] = command;
  }
  if (!properties.is_null()) {
    j["properties"] = properties;
  }
  if (!details.is_null()) {
    j["details"] = details;
  }
  return j;
}

void ResponseMessage::fromJson(const json &j) {
  Message::fromJson(j);
  status = j.at("status");
  if (j.contains("command")) {
    command = j["command"];
  }
  if (j.contains("properties")) {
    properties = j["properties"];
  }
  if (j.contains("details")) {
    details = j["details"];
  }
}

// EventMessage implementation
EventMessage::EventMessage() : Message(MessageType::EVENT) {}

EventMessage::EventMessage(const std::string &eventName)
    : Message(MessageType::EVENT), eventName(eventName) {}

void EventMessage::setEventName(const std::string &name) { eventName = name; }

std::string EventMessage::getEventName() const { return eventName; }

void EventMessage::setProperties(const json &props) { properties = props; }

json EventMessage::getProperties() const { return properties; }

void EventMessage::setDetails(const json &d) { details = d; }

json EventMessage::getDetails() const { return details; }

void EventMessage::setRelatedMessageId(const std::string &id) {
  relatedMessageId = id;
}

std::string EventMessage::getRelatedMessageId() const {
  return relatedMessageId;
}

json EventMessage::toJson() const {
  json j = Message::toJson();
  j["event"] = eventName;
  if (!properties.is_null()) {
    j["properties"] = properties;
  }
  if (!details.is_null()) {
    j["details"] = details;
  }
  if (!relatedMessageId.empty()) {
    j["relatedMessageId"] = relatedMessageId;
  }
  return j;
}

void EventMessage::fromJson(const json &j) {
  Message::fromJson(j);
  eventName = j.at("event");
  if (j.contains("properties")) {
    properties = j["properties"];
  }
  if (j.contains("details")) {
    details = j["details"];
  }
  if (j.contains("relatedMessageId")) {
    relatedMessageId = j["relatedMessageId"];
  }
}

// ErrorMessage implementation
ErrorMessage::ErrorMessage() : Message(MessageType::ERR) {}

ErrorMessage::ErrorMessage(const std::string &errorCode,
                           const std::string &errorMsg)
    : Message(MessageType::ERR), errorCode(errorCode), errorMessage(errorMsg) {}

void ErrorMessage::setErrorCode(const std::string &code) { errorCode = code; }

std::string ErrorMessage::getErrorCode() const { return errorCode; }

void ErrorMessage::setErrorMessage(const std::string &msg) {
  errorMessage = msg;
}

std::string ErrorMessage::getErrorMessage() const { return errorMessage; }

void ErrorMessage::setDetails(const json &d) { details = d; }

json ErrorMessage::getDetails() const { return details; }

json ErrorMessage::toJson() const {
  json j = Message::toJson();
  j["errorCode"] = errorCode;
  j["errorMessage"] = errorMessage;
  if (!details.is_null()) {
    j["details"] = details;
  }
  return j;
}

void ErrorMessage::fromJson(const json &j) {
  Message::fromJson(j);
  errorCode = j.at("errorCode");
  errorMessage = j.at("errorMessage");
  if (j.contains("details")) {
    details = j["details"];
  }
}

// DiscoveryRequestMessage implementation
DiscoveryRequestMessage::DiscoveryRequestMessage()
    : Message(MessageType::DISCOVERY_REQUEST) {}

void DiscoveryRequestMessage::setDeviceTypes(
    const std::vector<std::string> &types) {
  deviceTypes = types;
}

std::vector<std::string> DiscoveryRequestMessage::getDeviceTypes() const {
  return deviceTypes;
}

void DiscoveryRequestMessage::setFilter(const json &f) { filter = f; }

json DiscoveryRequestMessage::getFilter() const { return filter; }

json DiscoveryRequestMessage::toJson() const {
  json j = Message::toJson();
  j["deviceTypes"] = deviceTypes;
  if (!filter.is_null()) {
    j["filter"] = filter;
  }
  return j;
}

void DiscoveryRequestMessage::fromJson(const json &j) {
  Message::fromJson(j);
  deviceTypes = j.at("deviceTypes");
  if (j.contains("filter")) {
    filter = j["filter"];
  }
}

// DiscoveryResponseMessage implementation
DiscoveryResponseMessage::DiscoveryResponseMessage()
    : Message(MessageType::DISCOVERY_RESPONSE) {}

void DiscoveryResponseMessage::setDevices(const json &devs) { devices = devs; }

json DiscoveryResponseMessage::getDevices() const { return devices; }

json DiscoveryResponseMessage::toJson() const {
  json j = Message::toJson();
  j["devices"] = devices;
  return j;
}

void DiscoveryResponseMessage::fromJson(const json &j) {
  Message::fromJson(j);
  devices = j.at("devices");
}

// RegistrationMessage implementation
RegistrationMessage::RegistrationMessage()
    : Message(MessageType::REGISTRATION) {}

void RegistrationMessage::setDeviceInfo(const json &info) { deviceInfo = info; }

json RegistrationMessage::getDeviceInfo() const { return deviceInfo; }

json RegistrationMessage::toJson() const {
  json j = Message::toJson();
  j["deviceInfo"] = deviceInfo;
  return j;
}

void RegistrationMessage::fromJson(const json &j) {
  Message::fromJson(j);
  deviceInfo = j.at("deviceInfo");
}

// AuthenticationMessage implementation
AuthenticationMessage::AuthenticationMessage()
    : Message(MessageType::AUTHENTICATION) {}

void AuthenticationMessage::setMethod(const std::string &m) { method = m; }

std::string AuthenticationMessage::getMethod() const { return method; }

void AuthenticationMessage::setCredentials(const std::string &creds) {
  credentials = creds;
}

std::string AuthenticationMessage::getCredentials() const {
  return credentials;
}

json AuthenticationMessage::toJson() const {
  json j = Message::toJson();
  j["method"] = method;
  j["credentials"] = credentials;
  return j;
}

void AuthenticationMessage::fromJson(const json &j) {
  Message::fromJson(j);
  method = j.at("method");
  credentials = j.at("credentials");
}

// Factory function implementation
std::unique_ptr<Message> createMessageFromJson(const json &j) {
  MessageType type = stringToMessageType(j.at("messageType"));

  std::unique_ptr<Message> message;

  switch (type) {
  case MessageType::COMMAND:
    message = std::make_unique<CommandMessage>();
    break;
  case MessageType::RESPONSE:
    message = std::make_unique<ResponseMessage>();
    break;
  case MessageType::EVENT:
    message = std::make_unique<EventMessage>();
    break;
  case MessageType::ERR:
    message = std::make_unique<ErrorMessage>();
    break;
  case MessageType::DISCOVERY_REQUEST:
    message = std::make_unique<DiscoveryRequestMessage>();
    break;
  case MessageType::DISCOVERY_RESPONSE:
    message = std::make_unique<DiscoveryResponseMessage>();
    break;
  case MessageType::REGISTRATION:
    message = std::make_unique<RegistrationMessage>();
    break;
  case MessageType::AUTHENTICATION:
    message = std::make_unique<AuthenticationMessage>();
    break;
  default:
    throw std::invalid_argument("Unknown message type");
  }

  message->fromJson(j);
  return message;
}

} // namespace core
} // namespace hydrogen

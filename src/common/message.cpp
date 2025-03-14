#include "message.h"
#include "utils.h"
#include <stdexcept>

namespace astrocomm {

std::string messageTypeToString(MessageType type) {
  switch (type) {
  case MessageType::COMMAND:
    return "COMMAND";
  case MessageType::RESPONSE:
    return "RESPONSE";
  case MessageType::EVENT:
    return "EVENT";
  case MessageType::ERROR:
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
    return MessageType::ERROR;
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

// Message 基类实现
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

  return j;
}

void Message::fromJson(const json &j) {
  messageType = stringToMessageType(j["messageType"]);
  timestamp = j["timestamp"];
  messageId = j["messageId"];

  if (j.contains("deviceId")) {
    deviceId = j["deviceId"];
  }

  if (j.contains("originalMessageId")) {
    originalMessageId = j["originalMessageId"];
  }
}

std::string Message::toString() const { return toJson().dump(2); }

// CommandMessage 实现
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

  j["payload"] = {{"command", command}};

  if (!parameters.is_null()) {
    j["payload"]["parameters"] = parameters;
  }

  if (!properties.is_null()) {
    j["payload"]["properties"] = properties;
  }

  return j;
}

void CommandMessage::fromJson(const json &j) {
  Message::fromJson(j);

  command = j["payload"]["command"];

  if (j["payload"].contains("parameters")) {
    parameters = j["payload"]["parameters"];
  }

  if (j["payload"].contains("properties")) {
    properties = j["payload"]["properties"];
  }
}

// ResponseMessage 实现
ResponseMessage::ResponseMessage() : Message(MessageType::RESPONSE) {}

void ResponseMessage::setStatus(const std::string &s) { status = s; }

std::string ResponseMessage::getStatus() const { return status; }

void ResponseMessage::setCommand(const std::string &cmd) { command = cmd; }

std::string ResponseMessage::getCommand() const { return command; }

void ResponseMessage::setProperties(const json &props) { properties = props; }

json ResponseMessage::getProperties() const { return properties; }

void ResponseMessage::setDetails(const json &d) { details = d; }

json ResponseMessage::getDetails() const { return details; }

json ResponseMessage::toJson() const {
  json j = Message::toJson();

  j["payload"] = {{"status", status}};

  if (!command.empty()) {
    j["payload"]["command"] = command;
  }

  if (!properties.is_null()) {
    j["payload"]["properties"] = properties;
  }

  if (!details.is_null()) {
    j["payload"]["details"] = details;
  }

  return j;
}

void ResponseMessage::fromJson(const json &j) {
  Message::fromJson(j);

  status = j["payload"]["status"];

  if (j["payload"].contains("command")) {
    command = j["payload"]["command"];
  }

  if (j["payload"].contains("properties")) {
    properties = j["payload"]["properties"];
  }

  if (j["payload"].contains("details")) {
    details = j["payload"]["details"];
  }
}

// EventMessage 实现
EventMessage::EventMessage() : Message(MessageType::EVENT) {}

EventMessage::EventMessage(const std::string &eventName)
    : Message(MessageType::EVENT), event(eventName) {}

void EventMessage::setEvent(const std::string &eventName) { event = eventName; }

std::string EventMessage::getEvent() const { return event; }

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

  j["payload"] = {{"event", event}};

  if (!properties.is_null()) {
    j["payload"]["properties"] = properties;
  }

  if (!details.is_null()) {
    j["payload"]["details"] = details;
  }

  if (!relatedMessageId.empty()) {
    j["payload"]["relatedMessageId"] = relatedMessageId;
  }

  return j;
}

void EventMessage::fromJson(const json &j) {
  Message::fromJson(j);

  event = j["payload"]["event"];

  if (j["payload"].contains("properties")) {
    properties = j["payload"]["properties"];
  }

  if (j["payload"].contains("details")) {
    details = j["payload"]["details"];
  }

  if (j["payload"].contains("relatedMessageId")) {
    relatedMessageId = j["payload"]["relatedMessageId"];
  }
}

// ErrorMessage 实现
ErrorMessage::ErrorMessage() : Message(MessageType::ERROR) {}

ErrorMessage::ErrorMessage(const std::string &code, const std::string &msg)
    : Message(MessageType::ERROR), errorCode(code), errorMessage(msg) {}

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

  j["payload"] = {{"error", errorCode}, {"message", errorMessage}};

  if (!details.is_null()) {
    j["payload"]["details"] = details;
  }

  return j;
}

void ErrorMessage::fromJson(const json &j) {
  Message::fromJson(j);

  errorCode = j["payload"]["error"];
  errorMessage = j["payload"]["message"];

  if (j["payload"].contains("details")) {
    details = j["payload"]["details"];
  }
}

// DiscoveryRequestMessage 实现
DiscoveryRequestMessage::DiscoveryRequestMessage()
    : Message(MessageType::DISCOVERY_REQUEST) {}

void DiscoveryRequestMessage::setDeviceTypes(
    const std::vector<std::string> &types) {
  deviceTypes = types;
}

std::vector<std::string> DiscoveryRequestMessage::getDeviceTypes() const {
  return deviceTypes;
}

json DiscoveryRequestMessage::toJson() const {
  json j = Message::toJson();

  if (!deviceTypes.empty()) {
    j["payload"] = {{"deviceTypes", deviceTypes}};
  } else {
    j["payload"] = json::object();
  }

  return j;
}

void DiscoveryRequestMessage::fromJson(const json &j) {
  Message::fromJson(j);

  if (j.contains("payload") && j["payload"].contains("deviceTypes")) {
    deviceTypes = j["payload"]["deviceTypes"].get<std::vector<std::string>>();
  }
}

// DiscoveryResponseMessage 实现
DiscoveryResponseMessage::DiscoveryResponseMessage()
    : Message(MessageType::DISCOVERY_RESPONSE) {}

void DiscoveryResponseMessage::setDevices(const json &devs) { devices = devs; }

json DiscoveryResponseMessage::getDevices() const { return devices; }

json DiscoveryResponseMessage::toJson() const {
  json j = Message::toJson();

  j["payload"] = {{"devices", devices}};

  return j;
}

void DiscoveryResponseMessage::fromJson(const json &j) {
  Message::fromJson(j);

  devices = j["payload"]["devices"];
}

// RegistrationMessage 实现
RegistrationMessage::RegistrationMessage()
    : Message(MessageType::REGISTRATION) {}

void RegistrationMessage::setDeviceInfo(const json &info) { deviceInfo = info; }

json RegistrationMessage::getDeviceInfo() const { return deviceInfo; }

json RegistrationMessage::toJson() const {
  json j = Message::toJson();

  j["payload"] = deviceInfo;

  return j;
}

void RegistrationMessage::fromJson(const json &j) {
  Message::fromJson(j);

  deviceInfo = j["payload"];
}

// AuthenticationMessage 实现
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

  j["payload"] = {{"method", method}, {"credentials", credentials}};

  return j;
}

void AuthenticationMessage::fromJson(const json &j) {
  Message::fromJson(j);

  method = j["payload"]["method"];
  credentials = j["payload"]["credentials"];
}

// Factory function
std::unique_ptr<Message> createMessageFromJson(const json &j) {
  std::string typeStr = j["messageType"];
  MessageType type = stringToMessageType(typeStr);

  std::unique_ptr<Message> msg;

  switch (type) {
  case MessageType::COMMAND:
    msg = std::make_unique<CommandMessage>();
    break;
  case MessageType::RESPONSE:
    msg = std::make_unique<ResponseMessage>();
    break;
  case MessageType::EVENT:
    msg = std::make_unique<EventMessage>();
    break;
  case MessageType::ERROR:
    msg = std::make_unique<ErrorMessage>();
    break;
  case MessageType::DISCOVERY_REQUEST:
    msg = std::make_unique<DiscoveryRequestMessage>();
    break;
  case MessageType::DISCOVERY_RESPONSE:
    msg = std::make_unique<DiscoveryResponseMessage>();
    break;
  case MessageType::REGISTRATION:
    msg = std::make_unique<RegistrationMessage>();
    break;
  case MessageType::AUTHENTICATION:
    msg = std::make_unique<AuthenticationMessage>();
    break;
  default:
    throw std::invalid_argument("Unknown message type: " + typeStr);
  }

  msg->fromJson(j);
  return msg;
}

} // namespace astrocomm
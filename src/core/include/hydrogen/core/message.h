#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

// Message type enumeration
enum class MessageType {
  COMMAND,
  RESPONSE,
  EVENT,
  ERR,  // Renamed from ERROR to avoid Windows macro conflict
  DISCOVERY_REQUEST,
  DISCOVERY_RESPONSE,
  REGISTRATION,
  AUTHENTICATION
};

// Convert MessageType to string
std::string messageTypeToString(MessageType type);
// Convert string to MessageType
MessageType stringToMessageType(const std::string &typeStr);

// Base message class
class Message {
public:
  Message();
  Message(MessageType type);
  virtual ~Message() = default;

  // Set and get basic properties
  void setMessageType(MessageType type);
  MessageType getMessageType() const;

  void setMessageId(const std::string &id);
  std::string getMessageId() const;

  void setDeviceId(const std::string &id);
  std::string getDeviceId() const;

  void setTimestamp(const std::string &ts);
  std::string getTimestamp() const;

  void setOriginalMessageId(const std::string &id);
  std::string getOriginalMessageId() const;

  // QoS level support
  enum class QoSLevel {
    AT_MOST_ONCE,   // At most once delivery, may be lost (default)
    AT_LEAST_ONCE,  // At least once delivery, may be duplicated
    EXACTLY_ONCE    // Exactly once delivery, no loss no duplication
  };

  void setQoSLevel(QoSLevel level);
  QoSLevel getQoSLevel() const;

  // Message priority
  enum class Priority {
    LOW,
    NORMAL,
    HIGH,
    CRITICAL
  };
  
  void setPriority(Priority priority);
  Priority getPriority() const;
  
  // Message expiration time
  void setExpireAfter(int seconds);
  int getExpireAfter() const;
  bool isExpired() const;

  // Serialization and deserialization
  virtual json toJson() const;
  virtual void fromJson(const json &j);

  // Helper methods
  std::string toString() const;

protected:
  MessageType messageType;
  std::string messageId;
  std::string deviceId;
  std::string timestamp;
  std::string originalMessageId;
  QoSLevel qosLevel{QoSLevel::AT_MOST_ONCE};
  Priority priority{Priority::NORMAL};
  int expireAfterSeconds{0}; // 0 means never expire
};

// Command message
class CommandMessage : public Message {
public:
  CommandMessage();
  CommandMessage(const std::string &cmd);

  void setCommand(const std::string &cmd);
  std::string getCommand() const;

  void setParameters(const json &params);
  json getParameters() const;

  void setProperties(const json &props);
  json getProperties() const;

  json toJson() const override;
  void fromJson(const json &j) override;

private:
  std::string command;
  json parameters;
  json properties;
};

// Response message
class ResponseMessage : public Message {
public:
  ResponseMessage();

  void setStatus(const std::string &status);
  std::string getStatus() const;

  void setCommand(const std::string &cmd);
  std::string getCommand() const;

  void setProperties(const json &props);
  json getProperties() const;

  void setDetails(const json &details);
  json getDetails() const;

  virtual json toJson() const override;
  virtual void fromJson(const json &j) override;

private:
  std::string status;
  std::string command;
  json properties;
  json details;
};

// Event message
class EventMessage : public Message {
public:
  EventMessage();
  EventMessage(const std::string &eventName);

  void setEventName(const std::string &name);
  std::string getEventName() const;

  void setProperties(const json &props);
  json getProperties() const;

  void setDetails(const json &details);
  json getDetails() const;

  void setRelatedMessageId(const std::string &id);
  std::string getRelatedMessageId() const;

  virtual json toJson() const override;
  virtual void fromJson(const json &j) override;

private:
  std::string eventName;
  json properties;
  json details;
  std::string relatedMessageId;
};

// Error message
class ErrorMessage : public Message {
public:
  ErrorMessage();
  ErrorMessage(const std::string &errorCode, const std::string &errorMsg);

  void setErrorCode(const std::string &code);
  std::string getErrorCode() const;

  void setErrorMessage(const std::string &msg);
  std::string getErrorMessage() const;

  void setDetails(const json &details);
  json getDetails() const;

  json toJson() const override;
  void fromJson(const json &j) override;

private:
  std::string errorCode;
  std::string errorMessage;
  json details;
};

// Device discovery request message
class DiscoveryRequestMessage : public Message {
public:
  DiscoveryRequestMessage();

  void setDeviceTypes(const std::vector<std::string> &types);
  std::vector<std::string> getDeviceTypes() const;

  void setFilter(const json &filter);
  json getFilter() const;

  json toJson() const override;
  void fromJson(const json &j) override;

private:
  std::vector<std::string> deviceTypes;
  json filter;
};

// Device discovery response message
class DiscoveryResponseMessage : public Message {
public:
  DiscoveryResponseMessage();

  void setDevices(const json &devs);
  json getDevices() const;

  json toJson() const override;
  void fromJson(const json &j) override;

private:
  json devices;
};

// Device registration message
class RegistrationMessage : public Message {
public:
  RegistrationMessage();

  void setDeviceInfo(const json &info);
  json getDeviceInfo() const;

  json toJson() const override;
  void fromJson(const json &j) override;

private:
  json deviceInfo;
};

// Authentication message
class AuthenticationMessage : public Message {
public:
  AuthenticationMessage();

  void setMethod(const std::string &m);
  std::string getMethod() const;

  void setCredentials(const std::string &creds);
  std::string getCredentials() const;

  json toJson() const override;
  void fromJson(const json &j) override;

private:
  std::string method;
  std::string credentials;
};

// Factory function to create appropriate message type from JSON
std::unique_ptr<Message> createMessageFromJson(const json &j);

} // namespace core
} // namespace hydrogen

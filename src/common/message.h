#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace astrocomm {

using json = nlohmann::json;

// 消息类型枚举
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

// 将MessageType转换为字符串
std::string messageTypeToString(MessageType type);
// 从字符串转换为MessageType
MessageType stringToMessageType(const std::string &typeStr);

// 基础消息类
class Message {
public:
  Message();
  Message(MessageType type);
  virtual ~Message() = default;

  // 设置和获取基本属性
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

  // QoS级别支持
  enum class QoSLevel {
    AT_MOST_ONCE,   // 最多一次传递，可能丢失（默认）
    AT_LEAST_ONCE,  // 至少一次传递，可能重复
    EXACTLY_ONCE    // 精确一次传递，不丢失不重复
  };

  void setQoSLevel(QoSLevel level);
  QoSLevel getQoSLevel() const;
  
  // 消息优先级
  enum class Priority {
    LOW,
    NORMAL,
    HIGH,
    CRITICAL
  };
  
  void setPriority(Priority priority);
  Priority getPriority() const;
  
  // 消息过期时间
  void setExpireAfter(int seconds);
  int getExpireAfter() const;
  bool isExpired() const;

  // 序列化和反序列化
  virtual json toJson() const;
  virtual void fromJson(const json &j);

  // 辅助方法
  std::string toString() const;

protected:
  MessageType messageType;
  std::string messageId;
  std::string deviceId;
  std::string timestamp;
  std::string originalMessageId;
  QoSLevel qosLevel{QoSLevel::AT_MOST_ONCE};
  Priority priority{Priority::NORMAL};
  int expireAfterSeconds{0}; // 0表示永不过期
};

// 命令消息
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

  virtual json toJson() const override;
  virtual void fromJson(const json &j) override;

private:
  std::string command;
  json parameters;
  json properties;
};

// 响应消息
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

// 事件消息
class EventMessage : public Message {
public:
  EventMessage();
  EventMessage(const std::string &eventName);

  void setEvent(const std::string &eventName);
  std::string getEvent() const;

  void setProperties(const json &props);
  json getProperties() const;

  void setDetails(const json &details);
  json getDetails() const;

  void setRelatedMessageId(const std::string &id);
  std::string getRelatedMessageId() const;

  virtual json toJson() const override;
  virtual void fromJson(const json &j) override;

private:
  std::string event;
  json properties;
  json details;
  std::string relatedMessageId;
};

// 错误消息
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

  virtual json toJson() const override;
  virtual void fromJson(const json &j) override;

private:
  std::string errorCode;
  std::string errorMessage;
  json details;
};

// 设备发现请求消息
class DiscoveryRequestMessage : public Message {
public:
  DiscoveryRequestMessage();

  void setDeviceTypes(const std::vector<std::string> &types);
  std::vector<std::string> getDeviceTypes() const;

  virtual json toJson() const override;
  virtual void fromJson(const json &j) override;

private:
  std::vector<std::string> deviceTypes;
};

// 设备发现响应消息
class DiscoveryResponseMessage : public Message {
public:
  DiscoveryResponseMessage();

  void setDevices(const json &devs);
  json getDevices() const;

  virtual json toJson() const override;
  virtual void fromJson(const json &j) override;

private:
  json devices;
};

// 设备注册消息
class RegistrationMessage : public Message {
public:
  RegistrationMessage();

  void setDeviceInfo(const json &info);
  json getDeviceInfo() const;

  virtual json toJson() const override;
  virtual void fromJson(const json &j) override;

private:
  json deviceInfo;
};

// 认证消息
class AuthenticationMessage : public Message {
public:
  AuthenticationMessage();

  void setMethod(const std::string &m);
  std::string getMethod() const;

  void setCredentials(const std::string &creds);
  std::string getCredentials() const;

  virtual json toJson() const override;
  virtual void fromJson(const json &j) override;

private:
  std::string method;
  std::string credentials;
};

// 工厂函数，根据JSON创建适当类型的消息
std::unique_ptr<Message> createMessageFromJson(const json &j);

} // namespace astrocomm
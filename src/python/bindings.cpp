#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "common/logger.h"
#include "common/message.h"
#include "common/utils.h"

namespace py = pybind11;
using namespace astrocomm;

// 定义模块名为 pyastrocomm
PYBIND11_MODULE(pyastrocomm, m) {
  m.doc() = "Python bindings for Astronomy Device Communication Protocol";

  // 记录日志级别枚举
  py::enum_<LogLevel>(m, "LogLevel")
      .value("DEBUG", LogLevel::DEBUG)
      .value("INFO", LogLevel::INFO)
      .value("WARNING", LogLevel::WARNING)
      .value("ERROR", LogLevel::ERROR)
      .value("CRITICAL", LogLevel::CRITICAL)
      .export_values();

  // 日志功能
  m.def("init_logger", &initLogger, py::arg("log_file_path") = "",
        py::arg("level") = LogLevel::INFO, "Initialize the logger");
  m.def("log_debug", &logDebug, py::arg("message"), py::arg("component") = "",
        "Log a debug message");
  m.def("log_info", &logInfo, py::arg("message"), py::arg("component") = "",
        "Log an info message");
  m.def("log_warning", &logWarning, py::arg("message"),
        py::arg("component") = "", "Log a warning message");
  m.def("log_error", &logError, py::arg("message"), py::arg("component") = "",
        "Log an error message");
  m.def("log_critical", &logCritical, py::arg("message"),
        py::arg("component") = "", "Log a critical message");

  // 工具函数
  m.def("generate_uuid", &generateUuid, "Generate a UUID string");
  m.def("get_iso_timestamp", &getIsoTimestamp,
        "Get current ISO 8601 timestamp");
  m.def("parse_boolean", &parseBoolean, py::arg("value"),
        "Parse string to boolean");

  // 消息类型枚举
  py::enum_<MessageType>(m, "MessageType")
      .value("COMMAND", MessageType::COMMAND)
      .value("RESPONSE", MessageType::RESPONSE)
      .value("EVENT", MessageType::EVENT)
      .value("ERROR", MessageType::ERROR)
      .value("DISCOVERY_REQUEST", MessageType::DISCOVERY_REQUEST)
      .value("DISCOVERY_RESPONSE", MessageType::DISCOVERY_RESPONSE)
      .value("REGISTRATION", MessageType::REGISTRATION)
      .value("AUTHENTICATION", MessageType::AUTHENTICATION)
      .export_values();

  // 字符串转换函数
  m.def("message_type_to_string", &messageTypeToString, py::arg("type"),
        "Convert MessageType enum to string");
  m.def("string_to_message_type", &stringToMessageType, py::arg("type_str"),
        "Convert string to MessageType enum");

  // 绑定基础消息类
  py::class_<Message, std::shared_ptr<Message>>(m, "Message")
      .def(py::init<>())
      .def(py::init<MessageType>())
      .def("get_message_type", &Message::getMessageType)
      .def("set_message_type", &Message::setMessageType)
      .def("get_message_id", &Message::getMessageId)
      .def("set_message_id", &Message::setMessageId)
      .def("get_device_id", &Message::getDeviceId)
      .def("set_device_id", &Message::setDeviceId)
      .def("get_timestamp", &Message::getTimestamp)
      .def("set_timestamp", &Message::setTimestamp)
      .def("get_original_message_id", &Message::getOriginalMessageId)
      .def("set_original_message_id", &Message::setOriginalMessageId)
      .def("to_json", &Message::toJson)
      .def("from_json", &Message::fromJson)
      .def("to_string", &Message::toString);

  // 绑定命令消息类
  py::class_<CommandMessage, Message, std::shared_ptr<CommandMessage>>(
      m, "CommandMessage")
      .def(py::init<>())
      .def(py::init<const std::string &>())
      .def("get_command", &CommandMessage::getCommand)
      .def("set_command", &CommandMessage::setCommand)
      .def("get_parameters", &CommandMessage::getParameters)
      .def("set_parameters", &CommandMessage::setParameters)
      .def("get_properties", &CommandMessage::getProperties)
      .def("set_properties", &CommandMessage::setProperties);

  // 绑定响应消息类
  py::class_<ResponseMessage, Message, std::shared_ptr<ResponseMessage>>(
      m, "ResponseMessage")
      .def(py::init<>())
      .def("get_status", &ResponseMessage::getStatus)
      .def("set_status", &ResponseMessage::setStatus)
      .def("get_command", &ResponseMessage::getCommand)
      .def("set_command", &ResponseMessage::setCommand)
      .def("get_properties", &ResponseMessage::getProperties)
      .def("set_properties", &ResponseMessage::setProperties)
      .def("get_details", &ResponseMessage::getDetails)
      .def("set_details", &ResponseMessage::setDetails);

  // 绑定事件消息类
  py::class_<EventMessage, Message, std::shared_ptr<EventMessage>>(
      m, "EventMessage")
      .def(py::init<>())
      .def(py::init<const std::string &>())
      .def("get_event", &EventMessage::getEvent)
      .def("set_event", &EventMessage::setEvent)
      .def("get_properties", &EventMessage::getProperties)
      .def("set_properties", &EventMessage::setProperties)
      .def("get_details", &EventMessage::getDetails)
      .def("set_details", &EventMessage::setDetails)
      .def("get_related_message_id", &EventMessage::getRelatedMessageId)
      .def("set_related_message_id", &EventMessage::setRelatedMessageId);

  // 绑定错误消息类
  py::class_<ErrorMessage, Message, std::shared_ptr<ErrorMessage>>(
      m, "ErrorMessage")
      .def(py::init<>())
      .def(py::init<const std::string &, const std::string &>())
      .def("get_error_code", &ErrorMessage::getErrorCode)
      .def("set_error_code", &ErrorMessage::setErrorCode)
      .def("get_error_message", &ErrorMessage::getErrorMessage)
      .def("set_error_message", &ErrorMessage::setErrorMessage)
      .def("get_details", &ErrorMessage::getDetails)
      .def("set_details", &ErrorMessage::setDetails);

  // 绑定发现请求消息类
  py::class_<DiscoveryRequestMessage, Message,
             std::shared_ptr<DiscoveryRequestMessage>>(
      m, "DiscoveryRequestMessage")
      .def(py::init<>())
      .def("get_device_types", &DiscoveryRequestMessage::getDeviceTypes)
      .def("set_device_types", &DiscoveryRequestMessage::setDeviceTypes);

  // 绑定发现响应消息类
  py::class_<DiscoveryResponseMessage, Message,
             std::shared_ptr<DiscoveryResponseMessage>>(
      m, "DiscoveryResponseMessage")
      .def(py::init<>())
      .def("get_devices", &DiscoveryResponseMessage::getDevices)
      .def("set_devices", &DiscoveryResponseMessage::setDevices);

  // 绑定设备注册消息类
  py::class_<RegistrationMessage, Message,
             std::shared_ptr<RegistrationMessage>>(m, "RegistrationMessage")
      .def(py::init<>())
      .def("get_device_info", &RegistrationMessage::getDeviceInfo)
      .def("set_device_info", &RegistrationMessage::setDeviceInfo);

  // 绑定认证消息类
  py::class_<AuthenticationMessage, Message,
             std::shared_ptr<AuthenticationMessage>>(m, "AuthenticationMessage")
      .def(py::init<>())
      .def("get_method", &AuthenticationMessage::getMethod)
      .def("set_method", &AuthenticationMessage::setMethod)
      .def("get_credentials", &AuthenticationMessage::getCredentials)
      .def("set_credentials", &AuthenticationMessage::setCredentials);

  // 消息工厂函数
  m.def("create_message_from_json", &createMessageFromJson, py::arg("json"),
        "Create appropriate message instance from JSON");

  // 字符串工具子模块
  py::module string_utils =
      m.def_submodule("string_utils", "String utility functions");
  string_utils.def("trim", &string_utils::trim,
                   "Remove whitespace from both ends of a string");
  string_utils.def("to_lower", &string_utils::toLower,
                   "Convert a string to lowercase");
  string_utils.def("to_upper", &string_utils::toUpper,
                   "Convert a string to uppercase");
  string_utils.def("split", &string_utils::split,
                   "Split a string by delimiter");
}

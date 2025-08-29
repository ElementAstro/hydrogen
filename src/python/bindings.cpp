#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <astrocomm/core.h>
#include <astrocomm/device.h>
#include <astrocomm/client.h>
#include <astrocomm/server.h>

#include <spdlog/spdlog.h>

namespace py = pybind11;
using namespace astrocomm::core;
using namespace astrocomm::device;
using namespace astrocomm::client;
using namespace astrocomm::server;

// 定义模块名为 pyastrocomm
PYBIND11_MODULE(pyastrocomm, m) {
  m.doc() = "Python bindings for Astronomy Device Communication Protocol";

  // 设置 spdlog 格式和级别
  m.def(
      "set_log_level",
      [](const std::string &level) {
        if (level == "trace") {
          spdlog::set_level(spdlog::level::trace);
        } else if (level == "debug") {
          spdlog::set_level(spdlog::level::debug);
        } else if (level == "info") {
          spdlog::set_level(spdlog::level::info);
        } else if (level == "warn") {
          spdlog::set_level(spdlog::level::warn);
        } else if (level == "error") {
          spdlog::set_level(spdlog::level::err);
        } else if (level == "critical") {
          spdlog::set_level(spdlog::level::critical);
        } else {
          throw std::invalid_argument("Invalid log level");
        }
      },
      "Set the log level for spdlog");

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
      .value("ERROR", MessageType::ERR)
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

  // 绑定设备基类
  py::class_<DeviceBase, std::shared_ptr<DeviceBase>>(m, "DeviceBase")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &, const std::string &>(),
           py::arg("device_id"), py::arg("device_type"),
           py::arg("manufacturer"), py::arg("model"))
      .def("connect", &DeviceBase::connect, py::arg("host"), py::arg("port"),
           "Connect to server")
      .def("disconnect", &DeviceBase::disconnect, "Disconnect from server")
      .def("register_device", &DeviceBase::registerDevice,
           "Register device with server")
      .def("start", &DeviceBase::start, "Start the device")
      .def("stop", &DeviceBase::stop, "Stop the device")
      .def("run", &DeviceBase::run, "Run the message loop")
      .def("get_device_id", &DeviceBase::getDeviceId, "Get the device ID")
      .def("get_device_type", &DeviceBase::getDeviceType, "Get the device type")
      .def("get_device_info", &DeviceBase::getDeviceInfo,
           "Get device information as JSON")
      .def("set_property", &DeviceBase::setProperty, py::arg("property"),
           py::arg("value"), "Set a device property")
      .def("get_property", &DeviceBase::getProperty, py::arg("property"),
           "Get a device property")
      .def(
          "register_command_handler",
          [](DeviceBase &device, const std::string &command,
             py::function callback) {
            device.registerCommandHandler(
                command, [callback](const CommandMessage &cmd,
                                    ResponseMessage &response) {
                  py::gil_scoped_acquire acquire;
                  try {
                    callback(cmd, response);
                  } catch (const py::error_already_set &e) {
                    SPDLOG_ERROR("Python error in command handler: {}",
                                 e.what());
                    response.setStatus("ERROR");
                    response.setDetails(
                        {{"error", "PYTHON_EXCEPTION"}, {"message", e.what()}});
                  }
                });
          },
          py::arg("command"), py::arg("callback"),
          "Register a Python function as command handler");

  // 添加观测事件和异步处理的回调机制
  m.def(
      "set_async_exception_handler",
      [](py::function handler) {
        py::gil_scoped_acquire acquire;
        // 全局异常处理器设置
        static py::function exception_handler;
        exception_handler = handler;

        // 设置C++中的全局异常回调
        // 这里可以添加异常捕获和处理的逻辑
      },
      py::arg("handler"), "Set global handler for asynchronous exceptions");
}

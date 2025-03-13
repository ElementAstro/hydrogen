#include "common/logger.h"
#include "device/device_base.h"
#include "device/telescope.h"
#include <pybind11/functional.h>
#include <pybind11/json.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace astrocomm;
using json = nlohmann::json;

// 为nlohmann::json类型添加转换支持
namespace pybind11 {
namespace detail {

// Python dict -> json转换
template <> struct type_caster<json> {
public:
  PYBIND11_TYPE_CASTER(json, _("json"));

  // Python对象转换为C++ json
  bool load(handle src, bool) {
    try {
      value = src.cast<py::dict>();
      return true;
    } catch (const py::cast_error &) {
      return false;
    }
  }

  // C++ json转换为Python对象
  static handle cast(const json &src, return_value_policy /* policy */,
                     handle /* parent */) {
    if (src.is_null())
      return py::none().release();
    else if (src.is_boolean())
      return py::bool_(src.get<bool>()).release();
    else if (src.is_number_integer())
      return py::int_(src.get<int64_t>()).release();
    else if (src.is_number_unsigned())
      return py::int_(src.get<uint64_t>()).release();
    else if (src.is_number_float())
      return py::float_(src.get<double>()).release();
    else if (src.is_string())
      return py::str(src.get<std::string>()).release();
    else if (src.is_array()) {
      py::list obj;
      for (const auto &el : src)
        obj.append(cast(el, return_value_policy::move, handle()));
      return obj.release();
    } else if (src.is_object()) {
      py::dict obj;
      for (auto it = src.begin(); it != src.end(); ++it)
        obj[py::str(it.key())] =
            cast(it.value(), return_value_policy::move, handle());
      return obj.release();
    }
    return py::none().release();
  }
};

} // namespace detail
} // namespace pybind11

// Python可继承的设备基类
class PyDeviceBase : public DeviceBase {
public:
  using DeviceBase::DeviceBase;

  // 重写虚方法以允许Python重载
  bool start() override {
    PYBIND11_OVERRIDE(bool,       // 返回类型
                      DeviceBase, // 父类
                      start,      // 方法名
                                  // 无参数
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,       // 返回类型
                      DeviceBase, // 父类
                      stop,       // 方法名
                                  // 无参数
    );
  }

  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,          // 返回类型
                      DeviceBase,    // 父类
                      getDeviceInfo, // 方法名
                                     // 无参数
    );
  }

  void setProperty(const std::string &property, const json &value) override {
    PYBIND11_OVERRIDE(void,        // 返回类型
                      DeviceBase,  // 父类
                      setProperty, // 方法名
                      property,    // 参数
                      value);
  }

  void handleMessage(const std::string &message) override {
    PYBIND11_OVERRIDE(void,          // 返回类型
                      DeviceBase,    // 父类
                      handleMessage, // 方法名
                      message        // 参数
    );
  }
};

// Python可继承的望远镜类
class PyTelescope : public Telescope {
public:
  using Telescope::Telescope;

  bool start() override {
    PYBIND11_OVERRIDE(bool,      // 返回类型
                      Telescope, // 父类
                      start,     // 方法名
                                 // 无参数
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,      // 返回类型
                      Telescope, // 父类
                      stop,      // 方法名
                                 // 无参数
    );
  }
};

PYBIND11_MODULE(pyastrodevice, m) {
  m.doc() = "Python bindings for Astronomy Device Communication Protocol";

  // 初始化日志系统
  m.def("init_logger", &initLogger, py::arg("log_file_path") = "",
        py::arg("level") = LogLevel::INFO, "Initialize the logging system");

  // 日志级别枚举
  py::enum_<LogLevel>(m, "LogLevel")
      .value("DEBUG", LogLevel::DEBUG)
      .value("INFO", LogLevel::INFO)
      .value("WARNING", LogLevel::WARNING)
      .value("ERROR", LogLevel::ERROR)
      .value("CRITICAL", LogLevel::CRITICAL)
      .export_values();

  // 日志函数
  m.def("log_debug", &logDebug, py::arg("message"), py::arg("component") = "");
  m.def("log_info", &logInfo, py::arg("message"), py::arg("component") = "");
  m.def("log_warning", &logWarning, py::arg("message"),
        py::arg("component") = "");
  m.def("log_error", &logError, py::arg("message"), py::arg("component") = "");
  m.def("log_critical", &logCritical, py::arg("message"),
        py::arg("component") = "");

  // 设备基类
  py::class_<DeviceBase, PyDeviceBase>(m, "DeviceBase")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &, const std::string &>(),
           py::arg("device_id"), py::arg("device_type"),
           py::arg("manufacturer"), py::arg("model"))
      .def("connect", &DeviceBase::connect, py::arg("host"), py::arg("port"))
      .def("disconnect", &DeviceBase::disconnect)
      .def("register_device", &DeviceBase::registerDevice)
      .def("start", &DeviceBase::start)
      .def("stop", &DeviceBase::stop)
      .def("run", &DeviceBase::run)
      .def("get_device_id", &DeviceBase::getDeviceId)
      .def("get_device_type", &DeviceBase::getDeviceType)
      .def("get_device_info", &DeviceBase::getDeviceInfo)
      .def("set_property", &DeviceBase::setProperty, py::arg("property"),
           py::arg("value"))
      .def("get_property", &DeviceBase::getProperty, py::arg("property"))
      .def(
          "register_command_handler",
          [](DeviceBase &self, const std::string &command,
             std::function<void(const json &, json &)> handler) {
            self.registerCommandHandler(
                command, [handler](const CommandMessage &cmd,
                                   ResponseMessage &response) {
                  json cmdJson = {{"command", cmd.getCommand()},
                                  {"parameters", cmd.getParameters()},
                                  {"properties", cmd.getProperties()},
                                  {"messageId", cmd.getMessageId()}};

                  json respJson;
                  handler(cmdJson, respJson);

                  if (respJson.contains("status"))
                    response.setStatus(respJson["status"]);
                  if (respJson.contains("properties"))
                    response.setProperties(respJson["properties"]);
                  if (respJson.contains("details"))
                    response.setDetails(respJson["details"]);
                });
          },
          py::arg("command"), py::arg("handler"))
      .def(
          "send_event",
          [](DeviceBase &self, const std::string &eventName,
             const json &details) {
            EventMessage event(eventName);
            event.setDetails(details);
            self.sendEvent(event);
          },
          py::arg("event_name"), py::arg("details") = json::object())
      .def(
          "send_property_changed_event",
          [](DeviceBase &self, const std::string &property, const json &value,
             const json &previousValue) {
            self.sendPropertyChangedEvent(property, value, previousValue);
          },
          py::arg("property"), py::arg("value"),
          py::arg("previous_value") = json());

  // 望远镜类
  py::class_<Telescope, DeviceBase, PyTelescope>(m, "Telescope")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "Celestron",
           py::arg("model") = "NexStar 8SE")
      .def("goto_position", &Telescope::gotoPosition, py::arg("ra"),
           py::arg("dec"))
      .def("set_tracking", &Telescope::setTracking, py::arg("enabled"))
      .def("set_slew_rate", &Telescope::setSlewRate, py::arg("rate"))
      .def("abort", &Telescope::abort)
      .def("park", &Telescope::park)
      .def("unpark", &Telescope::unpark)
      .def("sync", &Telescope::sync, py::arg("ra"), py::arg("dec"));

  // 工具函数
  m.def("generate_uuid", &generateUuid, "Generate a random UUID");
  m.def("get_iso_timestamp", &getIsoTimestamp,
        "Get current ISO 8601 timestamp");
}
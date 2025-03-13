#include "device/focuser.h"
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace astrocomm;

void register_focuser_bindings(py::module &m) {
  py::class_<Focuser, DeviceBase, std::shared_ptr<Focuser>>(m, "Focuser")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "ZWO",
           py::arg("model") = "EAF")
      .def("move_absolute", &Focuser::moveAbsolute, py::arg("position"))
      .def("move_relative", &Focuser::moveRelative, py::arg("steps"))
      .def("abort", &Focuser::abort)
      .def("set_max_position", &Focuser::setMaxPosition, py::arg("max_pos"))
      .def("set_speed", &Focuser::setSpeed, py::arg("speed_value"))
      .def("set_backlash", &Focuser::setBacklash, py::arg("backlash_value"))
      .def("set_temperature_compensation", &Focuser::setTemperatureCompensation,
           py::arg("enabled"), py::arg("coefficient") = 0.0);

  // Python扩展类 - 允许在Python中创建更专业化的设备
  py::class_<PyFocuser, Focuser, std::shared_ptr<PyFocuser>>(m, "PyFocuser")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "PythonFocuser",
           py::arg("model") = "v1.0");
}

// 创建一个用于Python继承的派生类
class PyFocuser : public Focuser {
public:
  // 使用与基类相同的构造函数
  using Focuser::Focuser;

  // 为Python覆盖提供虚函数
  bool start() override {
    PYBIND11_OVERRIDE(bool,    // 返回类型
                      Focuser, // 父类
                      start,   // 调用的函数
                               /* 参数列表为空 */
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,    // 返回类型
                      Focuser, // 父类
                      stop,    // 调用的函数
                               /* 参数列表为空 */
    );
  }

  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,          // 返回类型
                      Focuser,       // 父类
                      getDeviceInfo, // 调用的函数
                                     /* 参数列表为空 */
    );
  }
};
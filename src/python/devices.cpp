#include "device/device_base.h"
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace astrocomm;

// 前向声明其他设备绑定函数
void register_focuser_bindings(py::module &m);
void register_filter_wheel_bindings(py::module &m);
void register_guider_bindings(py::module &m);
void register_solver_bindings(py::module &m);

// 基础设备类包装
void register_device_base_bindings(py::module &m) {
  py::class_<DeviceBase, std::shared_ptr<DeviceBase>>(m, "DeviceBase")
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
             std::function<void(const CommandMessage &, ResponseMessage &)>
                 handler) { self.registerCommandHandler(command, handler); },
          py::arg("command"), py::arg("handler"));
}

// 定义主模块
PYBIND11_MODULE(pydevices, m) {
  m.doc() = "Python bindings for Astronomy Device classes";

  // 注册设备基类
  register_device_base_bindings(m);

  // 注册各设备类
  register_focuser_bindings(m);
  register_filter_wheel_bindings(m);
  register_guider_bindings(m);
  register_solver_bindings(m);
}
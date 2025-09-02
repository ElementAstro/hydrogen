#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "device/switch.h"

namespace py = pybind11;
using namespace hydrogen;

// 定义可被Python继承的类，以便Python代码可以重写虚方�?
class PySwitch : public Switch {
public:
  // 使用与基类相同的构造函�?
  using Switch::Switch;

  // 为Python重写虚方法提供的转发方法
  bool start() override {
    PYBIND11_OVERRIDE(bool,   // 返回类型
                      Switch, // 父类
                      start   // 函数�?
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,   // 返回类型
                      Switch, // 父类
                      stop    // 函数�?
    );
  }
};

void init_switch(py::module_ &m) {
  // 绑定Switch::SwitchType枚举
  py::enum_<Switch::SwitchType>(m, "SwitchType")
      .value("TOGGLE", Switch::SwitchType::TOGGLE)
      .value("MOMENTARY", Switch::SwitchType::MOMENTARY)
      .value("BUTTON", Switch::SwitchType::BUTTON)
      .export_values();

  // 绑定Switch::SwitchState枚举
  py::enum_<Switch::SwitchState>(m, "SwitchState")
      .value("OFF", Switch::SwitchState::OFF)
      .value("ON", Switch::SwitchState::ON)
      .export_values();

  // 绑定Switch类，注意添加PySwitch作为继承�?
  py::class_<Switch, DeviceBase, PySwitch, std::shared_ptr<Switch>>(m, "Switch")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "Generic",
           py::arg("model") = "Multi-Switch")
      .def("start", &Switch::start, "Start the switch device")
      .def("stop", &Switch::stop, "Stop the switch device")
      .def("add_switch", &Switch::addSwitch, "Add a switch to the device",
           py::arg("name"), py::arg("type") = Switch::SwitchType::TOGGLE,
           py::arg("default_state") = Switch::SwitchState::OFF)
      .def("set_state", &Switch::setState, "Set a switch state",
           py::arg("name"), py::arg("state"))
      .def("get_state", &Switch::getState, "Get a switch state",
           py::arg("name"))
      .def("get_switch_names", &Switch::getSwitchNames, "Get all switch names")
      .def("create_switch_group", &Switch::createSwitchGroup,
           "Create a switch group", py::arg("group_name"), py::arg("switches"))
      .def("set_group_state", &Switch::setGroupState,
           "Set a switch group state", py::arg("group_name"), py::arg("state"));
}
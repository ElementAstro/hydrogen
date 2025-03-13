#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "device/rotator.h"

namespace py = pybind11;
using namespace astrocomm;

void init_rotator(py::module_ &m) {
  // 绑定Rotator类
  py::class_<Rotator, DeviceBase, std::shared_ptr<Rotator>>(m, "Rotator")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "Generic",
           py::arg("model") = "Field Rotator")
      .def("start", &Rotator::start, "Start the rotator device")
      .def("stop", &Rotator::stop, "Stop the rotator device")
      .def("move_to", &Rotator::moveTo,
           "Move to a specific position in degrees", py::arg("position"))
      .def("move_by", &Rotator::moveBy, "Move by a relative offset in degrees",
           py::arg("offset"))
      .def("halt", &Rotator::halt, "Halt the current movement")
      .def("set_reverse", &Rotator::setReverse, "Set reverse direction",
           py::arg("reversed"))
      .def("sync", &Rotator::sync, "Synchronize the rotator position",
           py::arg("position"))
      .def("set_step_size", &Rotator::setStepSize,
           "Set the step size in degrees", py::arg("step_size"))
      .def("get_position", &Rotator::getPosition,
           "Get the current position in degrees")
      .def("get_target_position", &Rotator::getTargetPosition,
           "Get the target position in degrees")
      .def("is_moving", &Rotator::isMoving, "Check if the rotator is moving")
      .def("is_reversed", &Rotator::isReversed,
           "Check if the rotator direction is reversed");
}
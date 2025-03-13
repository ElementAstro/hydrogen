#include "device/guider.h"
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace astrocomm;

void register_guider_bindings(py::module &m) {
  // GuiderState 枚举
  py::enum_<GuiderState>(m, "GuiderState")
      .value("IDLE", GuiderState::IDLE)
      .value("CALIBRATING", GuiderState::CALIBRATING)
      .value("GUIDING", GuiderState::GUIDING)
      .value("PAUSED", GuiderState::PAUSED)
      .value("ERROR", GuiderState::ERROR)
      .export_values();

  // CalibrationState 枚举
  py::enum_<CalibrationState>(m, "CalibrationState")
      .value("IDLE", CalibrationState::IDLE)
      .value("NORTH_MOVING", CalibrationState::NORTH_MOVING)
      .value("NORTH_ANALYZING", CalibrationState::NORTH_ANALYZING)
      .value("SOUTH_MOVING", CalibrationState::SOUTH_MOVING)
      .value("SOUTH_ANALYZING", CalibrationState::SOUTH_ANALYZING)
      .value("EAST_MOVING", CalibrationState::EAST_MOVING)
      .value("EAST_ANALYZING", CalibrationState::EAST_ANALYZING)
      .value("WEST_MOVING", CalibrationState::WEST_MOVING)
      .value("WEST_ANALYZING", CalibrationState::WEST_ANALYZING)
      .value("COMPLETED", CalibrationState::COMPLETED)
      .value("FAILED", CalibrationState::FAILED)
      .export_values();

  py::class_<Guider, DeviceBase, std::shared_ptr<Guider>>(m, "Guider")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "QHY",
           py::arg("model") = "QHY5-II")
      .def("start_guiding", &Guider::startGuiding)
      .def("stop_guiding", &Guider::stopGuiding)
      .def("pause_guiding", &Guider::pauseGuiding)
      .def("resume_guiding", &Guider::resumeGuiding)
      .def("start_calibration", &Guider::startCalibration)
      .def("cancel_calibration", &Guider::cancelCalibration)
      .def("dither", &Guider::dither, py::arg("amount"),
           py::arg("settle") = true)
      .def("set_calibrated_pixel_scale", &Guider::setCalibratedPixelScale,
           py::arg("scale"))
      .def("set_aggressiveness", &Guider::setAggressiveness, py::arg("ra"),
           py::arg("dec"))
      .def("set_guide_rate", &Guider::setGuideRate, py::arg("ra"),
           py::arg("dec"));

  // Python扩展类
  py::class_<PyGuider, Guider, std::shared_ptr<PyGuider>>(m, "PyGuider")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "PythonGuider",
           py::arg("model") = "v1.0");
}

// 创建一个用于Python继承的派生类
class PyGuider : public Guider {
public:
  // 使用与基类相同的构造函数
  using Guider::Guider;

  // 为Python覆盖提供虚函数
  bool start() override {
    PYBIND11_OVERRIDE(bool,   // 返回类型
                      Guider, // 父类
                      start,  // 调用的函数
                              /* 参数列表为空 */
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,   // 返回类型
                      Guider, // 父类
                      stop,   // 调用的函数
                              /* 参数列表为空 */
    );
  }

  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,          // 返回类型
                      Guider,        // 父类
                      getDeviceInfo, // 调用的函数
                                     /* 参数列表为空 */
    );
  }
};
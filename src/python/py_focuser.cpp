#include "device/focuser.h"
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <spdlog/spdlog.h>

namespace py = pybind11;
using namespace hydrogen;

// 创建一个用于Python继承的派生类
class PyFocuser : public Focuser {
public:
  // 使用与基类相同的构造函�?
  using Focuser::Focuser;

  // 为Python覆盖提供虚函�?
  bool start() override {
    PYBIND11_OVERRIDE(bool,    // 返回类型
                      Focuser, // 父类
                      start,   // 调用的函�?
                               /* 参数列表为空 */
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,    // 返回类型
                      Focuser, // 父类
                      stop,    // 调用的函�?
                               /* 参数列表为空 */
    );
  }

  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,          // 返回类型
                      Focuser,       // 父类
                      getDeviceInfo, // 调用的函�?
                                     /* 参数列表为空 */
    );
  }

  // 添加新方法的覆盖
  bool moveAbsolute(int position, bool synchronous = false) override {
    PYBIND11_OVERRIDE(bool,                 // 返回类型
                      Focuser,              // 父类
                      moveAbsolute,         // 调用的函�?
                      position, synchronous // 参数
    );
  }

  bool moveRelative(int steps, bool synchronous = false) override {
    PYBIND11_OVERRIDE(bool,              // 返回类型
                      Focuser,           // 父类
                      moveRelative,      // 调用的函�?
                      steps, synchronous // 参数
    );
  }

  bool abort() override {
    PYBIND11_OVERRIDE(bool,    // 返回类型
                      Focuser, // 父类
                      abort,   // 调用的函�?
                               /* 参数列表为空 */
    );
  }

  bool setMaxPosition(int maxPos) override {
    PYBIND11_OVERRIDE(bool,           // 返回类型
                      Focuser,        // 父类
                      setMaxPosition, // 调用的函�?
                      maxPos          // 参数
    );
  }

  bool setSpeed(int speedValue) override {
    PYBIND11_OVERRIDE(bool,      // 返回类型
                      Focuser,   // 父类
                      setSpeed,  // 调用的函�?
                      speedValue // 参数
    );
  }

  bool setBacklash(int backlashValue) override {
    PYBIND11_OVERRIDE(bool,         // 返回类型
                      Focuser,      // 父类
                      setBacklash,  // 调用的函�?
                      backlashValue // 参数
    );
  }

  bool setStepMode(StepMode mode) override {
    PYBIND11_OVERRIDE(bool,        // 返回类型
                      Focuser,     // 父类
                      setStepMode, // 调用的函�?
                      mode         // 参数
    );
  }

  bool setTemperatureCompensation(bool enabled,
                                  double coefficient = 0.0) override {
    PYBIND11_OVERRIDE(bool,                       // 返回类型
                      Focuser,                    // 父类
                      setTemperatureCompensation, // 调用的函�?
                      enabled, coefficient        // 参数
    );
  }

  bool saveFocusPoint(const std::string &name,
                      const std::string &description = "") override {
    PYBIND11_OVERRIDE(bool,             // 返回类型
                      Focuser,          // 父类
                      saveFocusPoint,   // 调用的函�?
                      name, description // 参数
    );
  }

  bool moveToSavedPoint(const std::string &name,
                        bool synchronous = false) override {
    PYBIND11_OVERRIDE(bool,             // 返回类型
                      Focuser,          // 父类
                      moveToSavedPoint, // 调用的函�?
                      name, synchronous // 参数
    );
  }

  json getSavedFocusPoints() const override {
    PYBIND11_OVERRIDE(json,                // 返回类型
                      Focuser,             // 父类
                      getSavedFocusPoints, // 调用的函�?
                                           /* 参数列表为空 */
    );
  }

  bool startAutoFocus(int startPos, int endPos, int steps,
                      bool useExistingCurve = false) override {
    PYBIND11_OVERRIDE(bool,                                     // 返回类型
                      Focuser,                                  // 父类
                      startAutoFocus,                           // 调用的函�?
                      startPos, endPos, steps, useExistingCurve // 参数
    );
  }

  json getFocusCurveData() const override {
    PYBIND11_OVERRIDE(json,              // 返回类型
                      Focuser,           // 父类
                      getFocusCurveData, // 调用的函�?
                                         /* 参数列表为空 */
    );
  }

  bool saveConfiguration(const std::string &filePath) const override {
    PYBIND11_OVERRIDE(bool,              // 返回类型
                      Focuser,           // 父类
                      saveConfiguration, // 调用的函�?
                      filePath           // 参数
    );
  }

  bool loadConfiguration(const std::string &filePath) override {
    PYBIND11_OVERRIDE(bool,              // 返回类型
                      Focuser,           // 父类
                      loadConfiguration, // 调用的函�?
                      filePath           // 参数
    );
  }

  // 保护方法的特殊封装，允许Python子类重写它们
  // 注意：这些方法通常是保护的，但为了允许Python覆盖，我们需要公开它们
  void updateLoop() override {
    PYBIND11_OVERRIDE(void,       // 返回类型
                      Focuser,    // 父类
                      updateLoop, // 调用的函�?
                                  /* 参数列表为空 */
    );
  }

  double calculateFocusMetric(int position) override {
    PYBIND11_OVERRIDE(double,               // 返回类型
                      Focuser,              // 父类
                      calculateFocusMetric, // 调用的函�?
                      position              // 参数
    );
  }

  void performAutoFocus() override {
    PYBIND11_OVERRIDE(void,             // 返回类型
                      Focuser,          // 父类
                      performAutoFocus, // 调用的函�?
                                        /* 参数列表为空 */
    );
  }

  int applyTemperatureCompensation(int currentPosition) override {
    PYBIND11_OVERRIDE(int,                          // 返回类型
                      Focuser,                      // 父类
                      applyTemperatureCompensation, // 调用的函�?
                      currentPosition               // 参数
    );
  }
};

void register_focuser_bindings(py::module &m) {
  // 首先注册StepMode枚举
  py::enum_<StepMode>(m, "StepMode")
      .value("FULL_STEP", StepMode::FULL_STEP)
      .value("HALF_STEP", StepMode::HALF_STEP)
      .value("QUARTER_STEP", StepMode::QUARTER_STEP)
      .value("EIGHTH_STEP", StepMode::EIGHTH_STEP)
      .value("SIXTEENTH_STEP", StepMode::SIXTEENTH_STEP)
      .value("THIRTYSECOND_STEP", StepMode::THIRTYSECOND_STEP)
      .export_values();

  // 注册FocusPoint结构
  py::class_<FocusPoint>(m, "FocusPoint")
      .def(py::init<>())
      .def_readwrite("position", &FocusPoint::position)
      .def_readwrite("metric", &FocusPoint::metric)
      .def_readwrite("temperature", &FocusPoint::temperature)
      .def_readwrite("timestamp", &FocusPoint::timestamp);

  // Focuser基类
  py::class_<Focuser, DeviceBase, std::shared_ptr<Focuser>>(m, "Focuser")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "ZWO",
           py::arg("model") = "EAF")
      .def("move_absolute", &Focuser::moveAbsolute, py::arg("position"),
           py::arg("synchronous") = false, "Move to absolute position")
      .def("move_relative", &Focuser::moveRelative, py::arg("steps"),
           py::arg("synchronous") = false, "Move relative steps")
      .def("abort", &Focuser::abort, "Abort current movement")
      .def("set_max_position", &Focuser::setMaxPosition, py::arg("max_pos"),
           "Set maximum position")
      .def("set_speed", &Focuser::setSpeed, py::arg("speed_value"),
           "Set movement speed (1-10)")
      .def("set_backlash", &Focuser::setBacklash, py::arg("backlash_value"),
           "Set backlash compensation steps")
      .def("set_step_mode", &Focuser::setStepMode, py::arg("mode"),
           "Set stepping mode (FULL_STEP, HALF_STEP, etc)")
      .def("set_temperature_compensation", &Focuser::setTemperatureCompensation,
           py::arg("enabled"), py::arg("coefficient") = 0.0,
           "Set temperature compensation")
      .def("save_focus_point", &Focuser::saveFocusPoint, py::arg("name"),
           py::arg("description") = "",
           "Save current position as a named focus point")
      .def("move_to_saved_point", &Focuser::moveToSavedPoint, py::arg("name"),
           py::arg("synchronous") = false, "Move to a saved focus point")
      .def("get_saved_focus_points", &Focuser::getSavedFocusPoints,
           "Get all saved focus points as JSON")
      .def("start_auto_focus", &Focuser::startAutoFocus, py::arg("start_pos"),
           py::arg("end_pos"), py::arg("steps"),
           py::arg("use_existing_curve") = false, "Start auto-focus process")
      .def("get_focus_curve_data", &Focuser::getFocusCurveData,
           "Get focus curve data as JSON")
      .def("save_configuration", &Focuser::saveConfiguration,
           py::arg("file_path"), "Save focuser configuration to file")
      .def("load_configuration", &Focuser::loadConfiguration,
           py::arg("file_path"), "Load focuser configuration from file")
      .def(
          "set_focus_metric_callback",
          [](Focuser &self, py::function callback) {
            self.setFocusMetricCallback([callback](int position) -> double {
              py::gil_scoped_acquire acquire;
              try {
                return callback(position).cast<double>();
              } catch (const py::error_already_set &e) {
                SPDLOG_ERROR("Python error in focus metric callback: {}",
                             e.what());
                return 0.0; // 默认返回�?
              }
            });
          },
          py::arg("callback"), "Set focus metric evaluation callback");

  // Python扩展�?- 允许在Python中创建更专业化的设备
  py::class_<PyFocuser, Focuser, std::shared_ptr<PyFocuser>>(m, "PyFocuser")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "PythonFocuser",
           py::arg("model") = "v1.0");
}
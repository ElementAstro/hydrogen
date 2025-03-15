#include "device/guider.h"
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace astrocomm;

// 首先定义用于Python继承的派生类
class PyGuiderInterface : public GuiderInterface {
public:
  // 使用pybind11虚函数封装机制
  bool connect(const std::string &host, int port) override {
    PYBIND11_OVERRIDE_PURE(bool,            // 返回类型
                           GuiderInterface, // 父类
                           connect,         // 调用的函数
                           host, port       // 参数
    );
  }

  void disconnect() override {
    PYBIND11_OVERRIDE_PURE(void,            // 返回类型
                           GuiderInterface, // 父类
                           disconnect,      // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  bool isConnected() const override {
    PYBIND11_OVERRIDE_PURE(bool,            // 返回类型
                           GuiderInterface, // 父类
                           isConnected,     // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  bool startGuiding() override {
    PYBIND11_OVERRIDE_PURE(bool,            // 返回类型
                           GuiderInterface, // 父类
                           startGuiding,    // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  bool stopGuiding() override {
    PYBIND11_OVERRIDE_PURE(bool,            // 返回类型
                           GuiderInterface, // 父类
                           stopGuiding,     // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  bool pauseGuiding() override {
    PYBIND11_OVERRIDE_PURE(bool,            // 返回类型
                           GuiderInterface, // 父类
                           pauseGuiding,    // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  bool resumeGuiding() override {
    PYBIND11_OVERRIDE_PURE(bool,            // 返回类型
                           GuiderInterface, // 父类
                           resumeGuiding,   // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  bool startCalibration() override {
    PYBIND11_OVERRIDE_PURE(bool,             // 返回类型
                           GuiderInterface,  // 父类
                           startCalibration, // 调用的函数
                                             /* 参数列表为空 */
    );
  }

  bool cancelCalibration() override {
    PYBIND11_OVERRIDE_PURE(bool,              // 返回类型
                           GuiderInterface,   // 父类
                           cancelCalibration, // 调用的函数
                                              /* 参数列表为空 */
    );
  }

  bool dither(double amount, double settleTime, double settlePixels) override {
    PYBIND11_OVERRIDE_PURE(bool,                            // 返回类型
                           GuiderInterface,                 // 父类
                           dither,                          // 调用的函数
                           amount, settleTime, settlePixels // 参数
    );
  }

  GuiderState getGuiderState() const override {
    PYBIND11_OVERRIDE_PURE(GuiderState,     // 返回类型
                           GuiderInterface, // 父类
                           getGuiderState,  // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  CalibrationState getCalibrationState() const override {
    PYBIND11_OVERRIDE_PURE(CalibrationState,    // 返回类型
                           GuiderInterface,     // 父类
                           getCalibrationState, // 调用的函数
                                                /* 参数列表为空 */
    );
  }

  GuiderStats getStats() const override {
    PYBIND11_OVERRIDE_PURE(GuiderStats,     // 返回类型
                           GuiderInterface, // 父类
                           getStats,        // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  StarInfo getGuideStar() const override {
    PYBIND11_OVERRIDE_PURE(StarInfo,        // 返回类型
                           GuiderInterface, // 父类
                           getGuideStar,    // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  CalibrationData getCalibrationData() const override {
    PYBIND11_OVERRIDE_PURE(CalibrationData,    // 返回类型
                           GuiderInterface,    // 父类
                           getCalibrationData, // 调用的函数
                                               /* 参数列表为空 */
    );
  }

  void setPixelScale(double scaleArcsecPerPixel) override {
    PYBIND11_OVERRIDE_PURE(void,               // 返回类型
                           GuiderInterface,    // 父类
                           setPixelScale,      // 调用的函数
                           scaleArcsecPerPixel // 参数
    );
  }

  void setGuideRate(double raRateMultiplier,
                    double decRateMultiplier) override {
    PYBIND11_OVERRIDE_PURE(void,            // 返回类型
                           GuiderInterface, // 父类
                           setGuideRate,    // 调用的函数
                           raRateMultiplier, decRateMultiplier // 参数
    );
  }

  GuidingCorrection getCurrentCorrection() const override {
    PYBIND11_OVERRIDE_PURE(GuidingCorrection,    // 返回类型
                           GuiderInterface,      // 父类
                           getCurrentCorrection, // 调用的函数
                                                 /* 参数列表为空 */
    );
  }

  GuiderInterfaceType getInterfaceType() const override {
    PYBIND11_OVERRIDE_PURE(GuiderInterfaceType, // 返回类型
                           GuiderInterface,     // 父类
                           getInterfaceType,    // 调用的函数
                                                /* 参数列表为空 */
    );
  }

  std::string getInterfaceName() const override {
    PYBIND11_OVERRIDE_PURE(std::string,      // 返回类型
                           GuiderInterface,  // 父类
                           getInterfaceName, // 调用的函数
                                             /* 参数列表为空 */
    );
  }

  void update() override {
    PYBIND11_OVERRIDE_PURE(void,            // 返回类型
                           GuiderInterface, // 父类
                           update,          // 调用的函数
                                            /* 参数列表为空 */
    );
  }
};

// 定义PyGuiderDevice
class PyGuiderDevice : public GuiderDevice {
public:
  // 使用与基类相同的构造函数
  using GuiderDevice::GuiderDevice;

  // 为Python覆盖提供虚函数
  bool start() override {
    PYBIND11_OVERRIDE(bool,         // 返回类型
                      GuiderDevice, // 父类
                      start,        // 调用的函数
                                    /* 参数列表为空 */
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,         // 返回类型
                      GuiderDevice, // 父类
                      stop,         // 调用的函数
                                    /* 参数列表为空 */
    );
  }

  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,          // 返回类型
                      GuiderDevice,  // 父类
                      getDeviceInfo, // 调用的函数
                                     /* 参数列表为空 */
    );
  }
};

void register_guider_bindings(py::module &m) {
  // GuiderState 枚举
  py::enum_<GuiderState>(m, "GuiderState")
      .value("DISCONNECTED", GuiderState::DISCONNECTED)
      .value("CONNECTED", GuiderState::CONNECTED)
      .value("CALIBRATING", GuiderState::CALIBRATING)
      .value("GUIDING", GuiderState::GUIDING)
      .value("PAUSED", GuiderState::PAUSED)
      .value("SETTLING", GuiderState::SETTLING)
      .value("ERROR", GuiderState::ERROR)
      .export_values();

  // CalibrationState 枚举
  py::enum_<CalibrationState>(m, "CalibrationState")
      .value("IDLE", CalibrationState::IDLE)
      .value("NORTH_MOVING", CalibrationState::NORTH_MOVING)
      .value("NORTH_COMPLETE", CalibrationState::NORTH_COMPLETE)
      .value("SOUTH_MOVING", CalibrationState::SOUTH_MOVING)
      .value("SOUTH_COMPLETE", CalibrationState::SOUTH_COMPLETE)
      .value("EAST_MOVING", CalibrationState::EAST_MOVING)
      .value("EAST_COMPLETE", CalibrationState::EAST_COMPLETE)
      .value("WEST_MOVING", CalibrationState::WEST_MOVING)
      .value("WEST_COMPLETE", CalibrationState::WEST_COMPLETE)
      .value("COMPLETED", CalibrationState::COMPLETED)
      .value("FAILED", CalibrationState::FAILED)
      .export_values();

  // GuiderInterfaceType 枚举
  py::enum_<GuiderInterfaceType>(m, "GuiderInterfaceType")
      .value("PHD2", GuiderInterfaceType::PHD2)
      .value("LINGUIDER", GuiderInterfaceType::LINGUIDER)
      .value("METAGUIDE", GuiderInterfaceType::METAGUIDE)
      .value("DIREKTGUIDER", GuiderInterfaceType::DIREKTGUIDER)
      .value("ASTROPHOTOGRAPHY_TOOL",
             GuiderInterfaceType::ASTROPHOTOGRAPHY_TOOL)
      .value("KSTARS_EKOS", GuiderInterfaceType::KSTARS_EKOS)
      .value("MAXIM_DL", GuiderInterfaceType::MAXIM_DL)
      .value("ASTROART", GuiderInterfaceType::ASTROART)
      .value("ASTAP", GuiderInterfaceType::ASTAP)
      .value("VOYAGER", GuiderInterfaceType::VOYAGER)
      .value("NINA", GuiderInterfaceType::NINA)
      .value("CUSTOM", GuiderInterfaceType::CUSTOM)
      .export_values();

  // 注册数据结构
  py::class_<GuidingCorrection>(m, "GuidingCorrection")
      .def(py::init<>())
      .def_readwrite("ra_correction", &GuidingCorrection::raCorrection)
      .def_readwrite("dec_correction", &GuidingCorrection::decCorrection)
      .def_readwrite("ra_raw", &GuidingCorrection::raRaw)
      .def_readwrite("dec_raw", &GuidingCorrection::decRaw);

  py::class_<CalibrationData>(m, "CalibrationData")
      .def(py::init<>())
      .def_readwrite("ra_angle", &CalibrationData::raAngle)
      .def_readwrite("dec_angle", &CalibrationData::decAngle)
      .def_readwrite("ra_rate", &CalibrationData::raRate)
      .def_readwrite("dec_rate", &CalibrationData::decRate)
      .def_readwrite("flipped", &CalibrationData::flipped)
      .def_readwrite("calibrated", &CalibrationData::calibrated);

  py::class_<StarInfo>(m, "StarInfo")
      .def(py::init<>())
      .def(py::init<double, double>())
      .def_readwrite("x", &StarInfo::x)
      .def_readwrite("y", &StarInfo::y)
      .def_readwrite("flux", &StarInfo::flux)
      .def_readwrite("snr", &StarInfo::snr)
      .def_readwrite("locked", &StarInfo::locked);

  py::class_<GuiderStats>(m, "GuiderStats")
      .def(py::init<>())
      .def_readwrite("rms", &GuiderStats::rms)
      .def_readwrite("rms_ra", &GuiderStats::rmsRa)
      .def_readwrite("rms_dec", &GuiderStats::rmsDec)
      .def_readwrite("peak", &GuiderStats::peak)
      .def_readwrite("total_frames", &GuiderStats::totalFrames)
      .def_readwrite("snr", &GuiderStats::snr)
      .def_readwrite("elapsed_time", &GuiderStats::elapsedTime);

  // 注册GuiderInterface接口（现在PyGuiderInterface已经定义）
  py::class_<GuiderInterface, PyGuiderInterface,
             std::shared_ptr<GuiderInterface>>(m, "GuiderInterface")
      .def(py::init<>())
      .def("connect", &GuiderInterface::connect)
      .def("disconnect", &GuiderInterface::disconnect)
      .def("is_connected", &GuiderInterface::isConnected)
      .def("start_guiding", &GuiderInterface::startGuiding)
      .def("stop_guiding", &GuiderInterface::stopGuiding)
      .def("pause_guiding", &GuiderInterface::pauseGuiding)
      .def("resume_guiding", &GuiderInterface::resumeGuiding)
      .def("start_calibration", &GuiderInterface::startCalibration)
      .def("cancel_calibration", &GuiderInterface::cancelCalibration)
      .def("dither", &GuiderInterface::dither, py::arg("amount"),
           py::arg("settle_time") = 5.0, py::arg("settle_pixels") = 1.5)
      .def("get_guider_state", &GuiderInterface::getGuiderState)
      .def("get_calibration_state", &GuiderInterface::getCalibrationState)
      .def("get_stats", &GuiderInterface::getStats)
      .def("get_guide_star", &GuiderInterface::getGuideStar)
      .def("get_calibration_data", &GuiderInterface::getCalibrationData)
      .def("set_pixel_scale", &GuiderInterface::setPixelScale)
      .def("set_guide_rate", &GuiderInterface::setGuideRate)
      .def("get_current_correction", &GuiderInterface::getCurrentCorrection)
      .def("get_interface_type", &GuiderInterface::getInterfaceType)
      .def("get_interface_name", &GuiderInterface::getInterfaceName)
      .def("update", &GuiderInterface::update);

  // 注册GuiderDevice类
  py::class_<GuiderDevice, DeviceBase, std::shared_ptr<GuiderDevice>>(
      m, "GuiderDevice")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "Generic",
           py::arg("model") = "Guider")
      .def("connect_to_guider", &GuiderDevice::connectToGuider, py::arg("type"),
           py::arg("host"), py::arg("port"), "Connect to a guiding software")
      .def("disconnect_from_guider", &GuiderDevice::disconnectFromGuider,
           "Disconnect from guiding software")
      .def("get_interface_type", &GuiderDevice::getInterfaceType,
           "Get the current interface type")
      .def("get_interface", &GuiderDevice::getInterface,
           "Get the current guider interface instance")
      .def_static("interface_type_to_string",
                  &GuiderDevice::interfaceTypeToString,
                  "Convert interface type to string")
      .def_static("string_to_interface_type",
                  &GuiderDevice::stringToInterfaceType,
                  "Convert string to interface type")
      .def_static("guider_state_to_string", &GuiderDevice::guiderStateToString,
                  "Convert guider state to string")
      .def_static("calibration_state_to_string",
                  &GuiderDevice::calibrationStateToString,
                  "Convert calibration state to string");

  // 工厂函数
  m.def("create_guider_interface", &createGuiderInterface, py::arg("type"),
        "Create a guider interface of specified type");

  // Python扩展类 - 现在PyGuiderDevice已经定义
  py::class_<PyGuiderDevice, GuiderDevice, std::shared_ptr<PyGuiderDevice>>(
      m, "PyGuiderDevice")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "PythonGuider",
           py::arg("model") = "v1.0");
}

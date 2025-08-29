#include "device/guider.h"
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace astrocomm;

// First define a derived class for Python inheritance
class PyGuiderInterface : public GuiderInterface {
public:
  // Using pybind11 trampoline mechanism for virtual functions
  bool connect(const std::string &host, int port) override {
    PYBIND11_OVERRIDE_PURE(bool,            // Return type
                           GuiderInterface, // Parent class
                           connect,         // Function name
                           host, port       // Parameters
    );
  }

  void disconnect() override {
    PYBIND11_OVERRIDE_PURE(void,            // Return type
                           GuiderInterface, // Parent class
                           disconnect,      // Function name
                                            /* No parameters */
    );
  }

  bool isConnected() const override {
    PYBIND11_OVERRIDE_PURE(bool,            // Return type
                           GuiderInterface, // Parent class
                           isConnected,     // Function name
                                            /* No parameters */
    );
  }

  bool startGuiding() override {
    PYBIND11_OVERRIDE_PURE(bool,            // Return type
                           GuiderInterface, // Parent class
                           startGuiding,    // Function name
                                            /* No parameters */
    );
  }

  bool stopGuiding() override {
    PYBIND11_OVERRIDE_PURE(bool,            // Return type
                           GuiderInterface, // Parent class
                           stopGuiding,     // Function name
                                            /* No parameters */
    );
  }

  bool pauseGuiding() override {
    PYBIND11_OVERRIDE_PURE(bool,            // Return type
                           GuiderInterface, // Parent class
                           pauseGuiding,    // Function name
                                            /* No parameters */
    );
  }

  bool resumeGuiding() override {
    PYBIND11_OVERRIDE_PURE(bool,            // Return type
                           GuiderInterface, // Parent class
                           resumeGuiding,   // Function name
                                            /* No parameters */
    );
  }

  bool startCalibration() override {
    PYBIND11_OVERRIDE_PURE(bool,             // Return type
                           GuiderInterface,  // Parent class
                           startCalibration, // Function name
                                             /* No parameters */
    );
  }

  bool cancelCalibration() override {
    PYBIND11_OVERRIDE_PURE(bool,              // Return type
                           GuiderInterface,   // Parent class
                           cancelCalibration, // Function name
                                              /* No parameters */
    );
  }

  bool dither(double amount, double settleTime, double settlePixels) override {
    PYBIND11_OVERRIDE_PURE(bool,                            // Return type
                           GuiderInterface,                 // Parent class
                           dither,                          // Function name
                           amount, settleTime, settlePixels // Parameters
    );
  }

  GuiderState getGuiderState() const override {
    PYBIND11_OVERRIDE_PURE(GuiderState,     // Return type
                           GuiderInterface, // Parent class
                           getGuiderState,  // Function name
                                            /* No parameters */
    );
  }

  CalibrationState getCalibrationState() const override {
    PYBIND11_OVERRIDE_PURE(CalibrationState,    // Return type
                           GuiderInterface,     // Parent class
                           getCalibrationState, // Function name
                                                /* No parameters */
    );
  }

  GuiderStats getStats() const override {
    PYBIND11_OVERRIDE_PURE(GuiderStats,     // Return type
                           GuiderInterface, // Parent class
                           getStats,        // Function name
                                            /* No parameters */
    );
  }

  StarInfo getGuideStar() const override {
    PYBIND11_OVERRIDE_PURE(StarInfo,        // Return type
                           GuiderInterface, // Parent class
                           getGuideStar,    // Function name
                                            /* No parameters */
    );
  }

  CalibrationData getCalibrationData() const override {
    PYBIND11_OVERRIDE_PURE(CalibrationData,    // Return type
                           GuiderInterface,    // Parent class
                           getCalibrationData, // Function name
                                               /* No parameters */
    );
  }

  void setPixelScale(double scaleArcsecPerPixel) override {
    PYBIND11_OVERRIDE_PURE(void,               // Return type
                           GuiderInterface,    // Parent class
                           setPixelScale,      // Function name
                           scaleArcsecPerPixel // Parameters
    );
  }

  void setGuideRate(double raRateMultiplier,
                    double decRateMultiplier) override {
    PYBIND11_OVERRIDE_PURE(void,                               // Return type
                           GuiderInterface,                    // Parent class
                           setGuideRate,                       // Function name
                           raRateMultiplier, decRateMultiplier // Parameters
    );
  }

  GuidingCorrection getCurrentCorrection() const override {
    PYBIND11_OVERRIDE_PURE(GuidingCorrection,    // Return type
                           GuiderInterface,      // Parent class
                           getCurrentCorrection, // Function name
                                                 /* No parameters */
    );
  }

  GuiderInterfaceType getInterfaceType() const override {
    PYBIND11_OVERRIDE_PURE(GuiderInterfaceType, // Return type
                           GuiderInterface,     // Parent class
                           getInterfaceType,    // Function name
                                                /* No parameters */
    );
  }

  std::string getInterfaceName() const override {
    PYBIND11_OVERRIDE_PURE(std::string,      // Return type
                           GuiderInterface,  // Parent class
                           getInterfaceName, // Function name
                                             /* No parameters */
    );
  }

  void update() override {
    PYBIND11_OVERRIDE_PURE(void,            // Return type
                           GuiderInterface, // Parent class
                           update,          // Function name
                                            /* No parameters */
    );
  }
};

// Define PyGuiderDevice
class PyGuiderDevice : public GuiderDevice {
public:
  // Use the same constructor as the base class
  using GuiderDevice::GuiderDevice;

  // Provide trampoline for virtual functions
  bool start() override {
    PYBIND11_OVERRIDE(bool,         // Return type
                      GuiderDevice, // Parent class
                      start,        // Function name
                                    /* No parameters */
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,         // Return type
                      GuiderDevice, // Parent class
                      stop,         // Function name
                                    /* No parameters */
    );
  }

  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,          // Return type
                      GuiderDevice,  // Parent class
                      getDeviceInfo, // Function name
                                     /* No parameters */
    );
  }

  // Add new overrides for optimized virtual methods
  bool connectToGuider(GuiderInterfaceType type, const std::string &host,
                       int port) override {
    PYBIND11_OVERRIDE(bool,            // Return type
                      GuiderDevice,    // Parent class
                      connectToGuider, // Function name
                      type, host, port // Parameters
    );
  }

  void disconnectFromGuider() override {
    PYBIND11_OVERRIDE(void,                 // Return type
                      GuiderDevice,         // Parent class
                      disconnectFromGuider, // Function name
                                            // No parameters
    );
  }

  std::shared_ptr<GuiderInterface> getInterface() const override {
    PYBIND11_OVERRIDE(std::shared_ptr<GuiderInterface>, // Return type
                      GuiderDevice,                     // Parent class
                      getInterface,                     // Function name
                                                        // No parameters
    );
  }

  // Add overrides for protected methods that Python may need to customize
  void handleStateChanged(GuiderState newState) override {
    PYBIND11_OVERRIDE(void,               // Return type
                      GuiderDevice,       // Parent class
                      handleStateChanged, // Function name
                      newState            // Parameters
    );
  }

  void handleCorrectionReceived(const GuidingCorrection &correction) override {
    PYBIND11_OVERRIDE(void,                     // Return type
                      GuiderDevice,             // Parent class
                      handleCorrectionReceived, // Function name
                      correction                // Parameters
    );
  }

  void handleCalibrationChanged(CalibrationState newState,
                                const CalibrationData &data) override {
    PYBIND11_OVERRIDE(void,                     // Return type
                      GuiderDevice,             // Parent class
                      handleCalibrationChanged, // Function name
                      newState, data            // Parameters
    );
  }

  void handleStatsUpdated(const GuiderStats &newStats) override {
    PYBIND11_OVERRIDE(void,               // Return type
                      GuiderDevice,       // Parent class
                      handleStatsUpdated, // Function name
                      newStats            // Parameters
    );
  }
};

void register_guider_bindings(py::module &m) {
  // GuiderState enum
  py::enum_<GuiderState>(m, "GuiderState")
      .value("DISCONNECTED", GuiderState::DISCONNECTED)
      .value("CONNECTED", GuiderState::CONNECTED)
      .value("CALIBRATING", GuiderState::CALIBRATING)
      .value("GUIDING", GuiderState::GUIDING)
      .value("PAUSED", GuiderState::PAUSED)
      .value("SETTLING", GuiderState::SETTLING)
      .value("ERROR", GuiderState::ERROR)
      .export_values();

  // CalibrationState enum
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

  // GuiderInterfaceType enum
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

  // Register data structures
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

  // Register GuiderInterface interface (now with PyGuiderInterface defined)
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

  // Register GuiderDevice class with extended functionality
  py::class_<GuiderDevice, PyGuiderDevice, DeviceBase,
             std::shared_ptr<GuiderDevice>>(m, "GuiderDevice")
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

  // Factory function
  m.def("create_guider_interface", &createGuiderInterface, py::arg("type"),
        "Create a guider interface of specified type");

  // Python extension class - now with PyGuiderDevice defined with all overrides
  py::class_<PyGuiderDevice, GuiderDevice, std::shared_ptr<PyGuiderDevice>>(
      m, "PyGuiderDevice")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "PythonGuider",
           py::arg("model") = "v1.0")
      // Expose protected methods so Python can override them
      .def("handle_state_changed", &PyGuiderDevice::handleStateChanged,
           "Handle guider state changes")
      .def("handle_correction_received",
           &PyGuiderDevice::handleCorrectionReceived,
           "Handle guiding corrections")
      .def("handle_calibration_changed",
           &PyGuiderDevice::handleCalibrationChanged,
           "Handle calibration state changes")
      .def("handle_stats_updated", &PyGuiderDevice::handleStatsUpdated,
           "Handle guider statistics updates");
}

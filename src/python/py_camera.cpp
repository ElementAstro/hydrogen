#include "device/camera.h"
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <spdlog/spdlog.h>

namespace py = pybind11;
using namespace astrocomm;

// Create a camera-derived class for Python inheritance
class PyCamera : public Camera {
public:
  // Use the same constructor as the base class
  using Camera::Camera;

  // Provide virtual function overrides for Python
  bool start() override {
    PYBIND11_OVERRIDE(bool,   // Return type
                      Camera, // Parent class
                      start,  // Function to call
                              /* No parameters */
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,   // Return type
                      Camera, // Parent class
                      stop,   // Function to call
                              /* No parameters */
    );
  }

  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,          // Return type
                      Camera,        // Parent class
                      getDeviceInfo, // Function to call
                                     /* No parameters */
    );
  }

  // Exposure control methods
  bool startExposure(double duration, ImageType type = ImageType::LIGHT,
                     bool autoSave = false) override {
    PYBIND11_OVERRIDE(bool,                    // Return type
                      Camera,                  // Parent class
                      startExposure,           // Function to call
                      duration, type, autoSave // Parameters
    );
  }

  bool abortExposure() override {
    PYBIND11_OVERRIDE(bool,          // Return type
                      Camera,        // Parent class
                      abortExposure, // Function to call
                                     /* No parameters */
    );
  }

  std::vector<uint8_t> getImageData() const override {
    PYBIND11_OVERRIDE(std::vector<uint8_t>, // Return type
                      Camera,               // Parent class
                      getImageData,         // Function to call
                                            /* No parameters */
    );
  }

  bool saveImage(const std::string &filename = "",
                 const std::string &format = "FITS") override {
    PYBIND11_OVERRIDE(bool,            // Return type
                      Camera,          // Parent class
                      saveImage,       // Function to call
                      filename, format // Parameters
    );
  }

  // Camera parameter controls
  bool setGain(int gain) override {
    PYBIND11_OVERRIDE(bool,    // Return type
                      Camera,  // Parent class
                      setGain, // Function to call
                      gain     // Parameter
    );
  }

  bool setOffset(int offset) override {
    PYBIND11_OVERRIDE(bool,      // Return type
                      Camera,    // Parent class
                      setOffset, // Function to call
                      offset     // Parameter
    );
  }

  bool setROI(int x, int y, int width, int height) override {
    PYBIND11_OVERRIDE(bool,               // Return type
                      Camera,             // Parent class
                      setROI,             // Function to call
                      x, y, width, height // Parameters
    );
  }

  bool setBinning(int binX, int binY) override {
    PYBIND11_OVERRIDE(bool,       // Return type
                      Camera,     // Parent class
                      setBinning, // Function to call
                      binX, binY  // Parameters
    );
  }

  // Cooling controls
  bool setCoolerTemperature(double temperature) override {
    PYBIND11_OVERRIDE(bool,                 // Return type
                      Camera,               // Parent class
                      setCoolerTemperature, // Function to call
                      temperature           // Parameter
    );
  }

  bool setCoolerEnabled(bool enabled) override {
    PYBIND11_OVERRIDE(bool,             // Return type
                      Camera,           // Parent class
                      setCoolerEnabled, // Function to call
                      enabled           // Parameter
    );
  }

  double getCurrentTemperature() const override {
    PYBIND11_OVERRIDE(double,                // Return type
                      Camera,                // Parent class
                      getCurrentTemperature, // Function to call
                                             /* No parameters */
    );
  }

  int getCoolerPower() const override {
    PYBIND11_OVERRIDE(int,            // Return type
                      Camera,         // Parent class
                      getCoolerPower, // Function to call
                                      /* No parameters */
    );
  }

  // Filter wheel controls
  bool setFilterPosition(int position) override {
    PYBIND11_OVERRIDE(bool,              // Return type
                      Camera,            // Parent class
                      setFilterPosition, // Function to call
                      position           // Parameter
    );
  }

  int getFilterPosition() const override {
    PYBIND11_OVERRIDE(int,               // Return type
                      Camera,            // Parent class
                      getFilterPosition, // Function to call
                                         /* No parameters */
    );
  }

  bool setFilterName(int position, const std::string &name) override {
    PYBIND11_OVERRIDE(bool,          // Return type
                      Camera,        // Parent class
                      setFilterName, // Function to call
                      position, name // Parameters
    );
  }

  std::string getFilterName(int position) const override {
    PYBIND11_OVERRIDE(std::string,   // Return type
                      Camera,        // Parent class
                      getFilterName, // Function to call
                      position       // Parameter
    );
  }

  // Status queries
  CameraState getState() const override {
    PYBIND11_OVERRIDE(CameraState, // Return type
                      Camera,      // Parent class
                      getState,    // Function to call
                                   /* No parameters */
    );
  }

  double getExposureProgress() const override {
    PYBIND11_OVERRIDE(double,              // Return type
                      Camera,              // Parent class
                      getExposureProgress, // Function to call
                                           /* No parameters */
    );
  }

  bool isExposing() const override {
    PYBIND11_OVERRIDE(bool,       // Return type
                      Camera,     // Parent class
                      isExposing, // Function to call
                                  /* No parameters */
    );
  }

  // Advanced features
  bool setAutoExposure(int targetBrightness, int tolerance = 5) override {
    PYBIND11_OVERRIDE(bool,                       // Return type
                      Camera,                     // Parent class
                      setAutoExposure,            // Function to call
                      targetBrightness, tolerance // Parameters
    );
  }

  bool setExposureDelay(double delaySeconds) override {
    PYBIND11_OVERRIDE(bool,             // Return type
                      Camera,           // Parent class
                      setExposureDelay, // Function to call
                      delaySeconds      // Parameter
    );
  }

  bool isBaseImplementation() const override {
    PYBIND11_OVERRIDE(bool,                 // Return type
                      Camera,               // Parent class
                      isBaseImplementation, // Function to call
                                            /* No parameters */
    );
  }

  // Special wrappers for protected methods, allowing Python subclasses to override them
  void updateLoop() override {
    PYBIND11_OVERRIDE(void,       // Return type
                      Camera,     // Parent class
                      updateLoop, // Function to call
                                  /* No parameters */
    );
  }

  void generateImageData() override {
    PYBIND11_OVERRIDE(void,              // Return type
                      Camera,            // Parent class
                      generateImageData, // Function to call
                                         /* No parameters */
    );
  }

  void applyImageEffects(std::vector<uint8_t> &imageData) override {
    PYBIND11_OVERRIDE(void,              // Return type
                      Camera,            // Parent class
                      applyImageEffects, // Function to call
                      imageData          // Parameter
    );
  }

  ImageType stringToImageType(const std::string &typeStr) override {
    PYBIND11_OVERRIDE(ImageType,         // Return type
                      Camera,            // Parent class
                      stringToImageType, // Function to call
                      typeStr            // Parameter
    );
  }

  std::string imageTypeToString(ImageType type) override {
    PYBIND11_OVERRIDE(std::string,       // Return type
                      Camera,            // Parent class
                      imageTypeToString, // Function to call
                      type               // Parameter
    );
  }

  CameraState stringToCameraState(const std::string &stateStr) override {
    PYBIND11_OVERRIDE(CameraState,         // Return type
                      Camera,              // Parent class
                      stringToCameraState, // Function to call
                      stateStr             // Parameter
    );
  }

  std::string cameraStateToString(CameraState state) override {
    PYBIND11_OVERRIDE(std::string,         // Return type
                      Camera,              // Parent class
                      cameraStateToString, // Function to call
                      state                // Parameter
    );
  }
};

void register_camera_bindings(py::module &m) {
  // Register CameraParameters structure
  py::class_<CameraParameters>(m, "CameraParameters")
      .def(py::init<>())
      .def_readwrite("width", &CameraParameters::width, "Image width in pixels")
      .def_readwrite("height", &CameraParameters::height,
                     "Image height in pixels")
      .def_readwrite("bit_depth", &CameraParameters::bitDepth,
                     "Pixel bit depth")
      .def_readwrite("has_color_sensor", &CameraParameters::hasColorSensor,
                     "Whether the sensor is color")
      .def_readwrite("has_cooler", &CameraParameters::hasCooler,
                     "Whether the camera has cooling capability")
      .def_readwrite("has_filter_wheel", &CameraParameters::hasFilterWheel,
                     "Whether the camera has a filter wheel")
      .def_readwrite("max_binning_x", &CameraParameters::maxBinningX,
                     "Maximum X-axis binning")
      .def_readwrite("max_binning_y", &CameraParameters::maxBinningY,
                     "Maximum Y-axis binning")
      .def_readwrite("pixel_size_x", &CameraParameters::pixelSizeX,
                     "Pixel size in X direction (microns)")
      .def_readwrite("pixel_size_y", &CameraParameters::pixelSizeY,
                     "Pixel size in Y direction (microns)")
      .def_readwrite("max_gain", &CameraParameters::maxGain,
                     "Maximum gain value")
      .def_readwrite("max_offset", &CameraParameters::maxOffset,
                     "Maximum offset value")
      .def_readwrite("min_exposure_time", &CameraParameters::minExposureTime,
                     "Minimum exposure time in seconds")
      .def_readwrite("max_exposure_time", &CameraParameters::maxExposureTime,
                     "Maximum exposure time in seconds")
      .def_readwrite("min_cooler_temp", &CameraParameters::minCoolerTemp,
                     "Minimum cooler temperature in Celsius")
      .def_readwrite("num_filters", &CameraParameters::numFilters,
                     "Number of filters in the filter wheel");

  // Register CameraState enum
  py::enum_<CameraState>(m, "CameraState")
      .value("IDLE", CameraState::IDLE)
      .value("EXPOSING", CameraState::EXPOSING)
      .value("READING_OUT", CameraState::READING_OUT)
      .value("DOWNLOADING", CameraState::DOWNLOADING)
      .value("PROCESSING", CameraState::PROCESSING)
      .value("ERROR", CameraState::ERROR)
      .value("WAITING_TRIGGER", CameraState::WAITING_TRIGGER)
      .value("COOLING", CameraState::COOLING)
      .value("WARMING_UP", CameraState::WARMING_UP)
      .export_values();

  // Register ImageType enum
  py::enum_<ImageType>(m, "ImageType")
      .value("LIGHT", ImageType::LIGHT)
      .value("DARK", ImageType::DARK)
      .value("BIAS", ImageType::BIAS)
      .value("FLAT", ImageType::FLAT)
      .value("TEST", ImageType::TEST)
      .export_values();

  // Register Camera base class
  py::class_<Camera, DeviceBase, std::shared_ptr<Camera>>(m, "Camera")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &, const CameraParameters &>(),
           py::arg("device_id"), py::arg("manufacturer") = "ZWO",
           py::arg("model") = "ASI294MM Pro",
           py::arg("params") = CameraParameters())
      // Exposure control methods
      .def("start_exposure", &Camera::startExposure, py::arg("duration"),
           py::arg("type") = ImageType::LIGHT, py::arg("auto_save") = false,
           "Start an exposure with specified duration")
      .def("abort_exposure", &Camera::abortExposure,
           "Abort the current exposure")
      .def("get_image_data", &Camera::getImageData,
           "Get the image data as bytes")
      .def("save_image", &Camera::saveImage, py::arg("filename") = "",
           py::arg("format") = "FITS", "Save the image to a file")

      // Camera parameter controls
      .def("set_gain", &Camera::setGain, py::arg("gain"), "Set the camera gain")
      .def("set_offset", &Camera::setOffset, py::arg("offset"),
           "Set the camera offset")
      .def("set_roi", &Camera::setROI, py::arg("x"), py::arg("y"),
           py::arg("width"), py::arg("height"), "Set the region of interest")
      .def("set_binning", &Camera::setBinning, py::arg("bin_x"),
           py::arg("bin_y"), "Set the pixel binning")

      // Cooling controls
      .def("set_cooler_temperature", &Camera::setCoolerTemperature,
           py::arg("temperature"),
           "Set the target cooler temperature in Celsius")
      .def("set_cooler_enabled", &Camera::setCoolerEnabled, py::arg("enabled"),
           "Enable or disable the cooler")
      .def("get_current_temperature", &Camera::getCurrentTemperature,
           "Get the current sensor temperature in Celsius")
      .def("get_cooler_power", &Camera::getCoolerPower,
           "Get the current cooler power as percentage (0-100)")

      // Filter wheel controls
      .def("set_filter_position", &Camera::setFilterPosition,
           py::arg("position"), "Set the filter wheel position")
      .def("get_filter_position", &Camera::getFilterPosition,
           "Get the current filter wheel position")
      .def("set_filter_name", &Camera::setFilterName, py::arg("position"),
           py::arg("name"), "Set a name for a filter position")
      .def("get_filter_name", &Camera::getFilterName, py::arg("position"),
           "Get the name of a filter position")

      // Status queries
      .def("get_state", &Camera::getState, "Get the current camera state")
      .def("get_exposure_progress", &Camera::getExposureProgress,
           "Get the current exposure progress (0.0-1.0)")
      .def("get_camera_parameters", &Camera::getCameraParameters,
           py::return_value_policy::reference_internal,
           "Get the camera parameters")
      .def("is_exposing", &Camera::isExposing,
           "Check if the camera is currently exposing")

      // Advanced features
      .def("set_auto_exposure", &Camera::setAutoExposure,
           py::arg("target_brightness"), py::arg("tolerance") = 5,
           "Set auto exposure parameters")
      .def("set_exposure_delay", &Camera::setExposureDelay,
           py::arg("delay_seconds"), "Set a delay before starting exposure")
      .def(
          "set_exposure_callback",
          [](Camera &self, py::function callback) {
            self.setExposureCallback([callback](bool success,
                                                const std::string &message) {
              py::gil_scoped_acquire acquire; // Acquire Python GIL
              try {
                callback(success, message);
              } catch (const py::error_already_set &e) {
                SPDLOG_ERROR("Python error in exposure callback: {}", e.what());
              }
            });
          },
          py::arg("callback"),
          "Set a callback function for exposure completion");

  // Python extension class - allows more specialized camera development in Python
  py::class_<PyCamera, Camera, std::shared_ptr<PyCamera>>(m, "PyCamera")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &, const CameraParameters &>(),
           py::arg("device_id"), py::arg("manufacturer") = "PythonCamera",
           py::arg("model") = "v1.0", py::arg("params") = CameraParameters());
}
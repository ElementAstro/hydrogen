#include "device/camera.h"
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <spdlog/spdlog.h>

namespace py = pybind11;
using namespace astrocomm;

// 创建一个用于Python继承的相机派生类
class PyCamera : public Camera {
public:
  // 使用与基类相同的构造函数
  using Camera::Camera;

  // 为Python覆盖提供虚函数
  bool start() override {
    PYBIND11_OVERRIDE(bool,   // 返回类型
                      Camera, // 父类
                      start,  // 调用的函数
                              /* 参数列表为空 */
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,   // 返回类型
                      Camera, // 父类
                      stop,   // 调用的函数
                              /* 参数列表为空 */
    );
  }

  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,          // 返回类型
                      Camera,        // 父类
                      getDeviceInfo, // 调用的函数
                                     /* 参数列表为空 */
    );
  }

  // 曝光控制方法
  bool startExposure(double duration, ImageType type = ImageType::LIGHT,
                     bool autoSave = false) override {
    PYBIND11_OVERRIDE(bool,                    // 返回类型
                      Camera,                  // 父类
                      startExposure,           // 调用的函数
                      duration, type, autoSave // 参数
    );
  }

  bool abortExposure() override {
    PYBIND11_OVERRIDE(bool,          // 返回类型
                      Camera,        // 父类
                      abortExposure, // 调用的函数
                                     /* 参数列表为空 */
    );
  }

  std::vector<uint8_t> getImageData() const override {
    PYBIND11_OVERRIDE(std::vector<uint8_t>, // 返回类型
                      Camera,               // 父类
                      getImageData,         // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  bool saveImage(const std::string &filename = "",
                 const std::string &format = "FITS") override {
    PYBIND11_OVERRIDE(bool,            // 返回类型
                      Camera,          // 父类
                      saveImage,       // 调用的函数
                      filename, format // 参数
    );
  }

  // 相机参数控制
  bool setGain(int gain) override {
    PYBIND11_OVERRIDE(bool,    // 返回类型
                      Camera,  // 父类
                      setGain, // 调用的函数
                      gain     // 参数
    );
  }

  bool setOffset(int offset) override {
    PYBIND11_OVERRIDE(bool,      // 返回类型
                      Camera,    // 父类
                      setOffset, // 调用的函数
                      offset     // 参数
    );
  }

  bool setROI(int x, int y, int width, int height) override {
    PYBIND11_OVERRIDE(bool,               // 返回类型
                      Camera,             // 父类
                      setROI,             // 调用的函数
                      x, y, width, height // 参数
    );
  }

  bool setBinning(int binX, int binY) override {
    PYBIND11_OVERRIDE(bool,       // 返回类型
                      Camera,     // 父类
                      setBinning, // 调用的函数
                      binX, binY  // 参数
    );
  }

  // 冷却控制
  bool setCoolerTemperature(double temperature) override {
    PYBIND11_OVERRIDE(bool,                 // 返回类型
                      Camera,               // 父类
                      setCoolerTemperature, // 调用的函数
                      temperature           // 参数
    );
  }

  bool setCoolerEnabled(bool enabled) override {
    PYBIND11_OVERRIDE(bool,             // 返回类型
                      Camera,           // 父类
                      setCoolerEnabled, // 调用的函数
                      enabled           // 参数
    );
  }

  double getCurrentTemperature() const override {
    PYBIND11_OVERRIDE(double,                // 返回类型
                      Camera,                // 父类
                      getCurrentTemperature, // 调用的函数
                                             /* 参数列表为空 */
    );
  }

  int getCoolerPower() const override {
    PYBIND11_OVERRIDE(int,            // 返回类型
                      Camera,         // 父类
                      getCoolerPower, // 调用的函数
                                      /* 参数列表为空 */
    );
  }

  // 滤镜轮控制
  bool setFilterPosition(int position) override {
    PYBIND11_OVERRIDE(bool,              // 返回类型
                      Camera,            // 父类
                      setFilterPosition, // 调用的函数
                      position           // 参数
    );
  }

  int getFilterPosition() const override {
    PYBIND11_OVERRIDE(int,               // 返回类型
                      Camera,            // 父类
                      getFilterPosition, // 调用的函数
                                         /* 参数列表为空 */
    );
  }

  bool setFilterName(int position, const std::string &name) override {
    PYBIND11_OVERRIDE(bool,          // 返回类型
                      Camera,        // 父类
                      setFilterName, // 调用的函数
                      position, name // 参数
    );
  }

  std::string getFilterName(int position) const override {
    PYBIND11_OVERRIDE(std::string,   // 返回类型
                      Camera,        // 父类
                      getFilterName, // 调用的函数
                      position       // 参数
    );
  }

  // 状态查询
  CameraState getState() const override {
    PYBIND11_OVERRIDE(CameraState, // 返回类型
                      Camera,      // 父类
                      getState,    // 调用的函数
                                   /* 参数列表为空 */
    );
  }

  double getExposureProgress() const override {
    PYBIND11_OVERRIDE(double,              // 返回类型
                      Camera,              // 父类
                      getExposureProgress, // 调用的函数
                                           /* 参数列表为空 */
    );
  }

  /*
  const CameraParameters &getCameraParameters() const override {
    PYBIND11_OVERRIDE_NAME(const CameraParameters &, // 返回类型
                            Camera,                   // 父类
                            getCameraParameters,      // 调用的函数名
                            get_camera_parameters,    // Python方法名
        );
    }
    */

  bool isExposing() const override {
    PYBIND11_OVERRIDE(bool,       // 返回类型
                      Camera,     // 父类
                      isExposing, // 调用的函数
                                  /* 参数列表为空 */
    );
  }

  // 高级功能
  bool setAutoExposure(int targetBrightness, int tolerance = 5) override {
    PYBIND11_OVERRIDE(bool,                       // 返回类型
                      Camera,                     // 父类
                      setAutoExposure,            // 调用的函数
                      targetBrightness, tolerance // 参数
    );
  }

  bool setExposureDelay(double delaySeconds) override {
    PYBIND11_OVERRIDE(bool,             // 返回类型
                      Camera,           // 父类
                      setExposureDelay, // 调用的函数
                      delaySeconds      // 参数
    );
  }

  bool isBaseImplementation() const override {
    PYBIND11_OVERRIDE(bool,                 // 返回类型
                      Camera,               // 父类
                      isBaseImplementation, // 调用的函数
                                            /* 参数列表为空 */
    );
  }

  // 保护方法的特殊封装，允许Python子类重写它们
  void updateLoop() override {
    PYBIND11_OVERRIDE(void,       // 返回类型
                      Camera,     // 父类
                      updateLoop, // 调用的函数
                                  /* 参数列表为空 */
    );
  }

  void generateImageData() override {
    PYBIND11_OVERRIDE(void,              // 返回类型
                      Camera,            // 父类
                      generateImageData, // 调用的函数
                                         /* 参数列表为空 */
    );
  }

  void applyImageEffects(std::vector<uint8_t> &imageData) override {
    PYBIND11_OVERRIDE(void,              // 返回类型
                      Camera,            // 父类
                      applyImageEffects, // 调用的函数
                      imageData          // 参数
    );
  }

  ImageType stringToImageType(const std::string &typeStr) override {
    PYBIND11_OVERRIDE(ImageType,         // 返回类型
                      Camera,            // 父类
                      stringToImageType, // 调用的函数
                      typeStr            // 参数
    );
  }

  std::string imageTypeToString(ImageType type) override {
    PYBIND11_OVERRIDE(std::string,       // 返回类型
                      Camera,            // 父类
                      imageTypeToString, // 调用的函数
                      type               // 参数
    );
  }

  CameraState stringToCameraState(const std::string &stateStr) override {
    PYBIND11_OVERRIDE(CameraState,         // 返回类型
                      Camera,              // 父类
                      stringToCameraState, // 调用的函数
                      stateStr             // 参数
    );
  }

  std::string cameraStateToString(CameraState state) override {
    PYBIND11_OVERRIDE(std::string,         // 返回类型
                      Camera,              // 父类
                      cameraStateToString, // 调用的函数
                      state                // 参数
    );
  }
};

void register_camera_bindings(py::module &m) {
  // 注册CameraParameters结构体
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

  // 注册CameraState枚举
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

  // 注册ImageType枚举
  py::enum_<ImageType>(m, "ImageType")
      .value("LIGHT", ImageType::LIGHT)
      .value("DARK", ImageType::DARK)
      .value("BIAS", ImageType::BIAS)
      .value("FLAT", ImageType::FLAT)
      .value("TEST", ImageType::TEST)
      .export_values();

  // 注册Camera基类
  py::class_<Camera, DeviceBase, std::shared_ptr<Camera>>(m, "Camera")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &, const CameraParameters &>(),
           py::arg("device_id"), py::arg("manufacturer") = "ZWO",
           py::arg("model") = "ASI294MM Pro",
           py::arg("params") = CameraParameters())
      // 曝光控制
      .def("start_exposure", &Camera::startExposure, py::arg("duration"),
           py::arg("type") = ImageType::LIGHT, py::arg("auto_save") = false,
           "Start an exposure with specified duration")
      .def("abort_exposure", &Camera::abortExposure,
           "Abort the current exposure")
      .def("get_image_data", &Camera::getImageData,
           "Get the image data as bytes")
      .def("save_image", &Camera::saveImage, py::arg("filename") = "",
           py::arg("format") = "FITS", "Save the image to a file")

      // 相机参数控制
      .def("set_gain", &Camera::setGain, py::arg("gain"), "Set the camera gain")
      .def("set_offset", &Camera::setOffset, py::arg("offset"),
           "Set the camera offset")
      .def("set_roi", &Camera::setROI, py::arg("x"), py::arg("y"),
           py::arg("width"), py::arg("height"), "Set the region of interest")
      .def("set_binning", &Camera::setBinning, py::arg("bin_x"),
           py::arg("bin_y"), "Set the pixel binning")

      // 冷却控制
      .def("set_cooler_temperature", &Camera::setCoolerTemperature,
           py::arg("temperature"),
           "Set the target cooler temperature in Celsius")
      .def("set_cooler_enabled", &Camera::setCoolerEnabled, py::arg("enabled"),
           "Enable or disable the cooler")
      .def("get_current_temperature", &Camera::getCurrentTemperature,
           "Get the current sensor temperature in Celsius")
      .def("get_cooler_power", &Camera::getCoolerPower,
           "Get the current cooler power as percentage (0-100)")

      // 滤镜轮控制
      .def("set_filter_position", &Camera::setFilterPosition,
           py::arg("position"), "Set the filter wheel position")
      .def("get_filter_position", &Camera::getFilterPosition,
           "Get the current filter wheel position")
      .def("set_filter_name", &Camera::setFilterName, py::arg("position"),
           py::arg("name"), "Set a name for a filter position")
      .def("get_filter_name", &Camera::getFilterName, py::arg("position"),
           "Get the name of a filter position")

      // 状态查询
      .def("get_state", &Camera::getState, "Get the current camera state")
      .def("get_exposure_progress", &Camera::getExposureProgress,
           "Get the current exposure progress (0.0-1.0)")
      .def("get_camera_parameters", &Camera::getCameraParameters,
           py::return_value_policy::reference_internal,
           "Get the camera parameters")
      .def("is_exposing", &Camera::isExposing,
           "Check if the camera is currently exposing")

      // 高级功能
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
              py::gil_scoped_acquire acquire;
              try {
                callback(success, message);
              } catch (const py::error_already_set &e) {
                SPDLOG_ERROR("Python error in exposure callback: {}", e.what());
              }
            });
          },
          py::arg("callback"),
          "Set a callback function for exposure completion");

  // Python扩展类 - 允许在Python中创建更专业化的相机
  py::class_<PyCamera, Camera, std::shared_ptr<PyCamera>>(m, "PyCamera")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &, const CameraParameters &>(),
           py::arg("device_id"), py::arg("manufacturer") = "PythonCamera",
           py::arg("model") = "v1.0", py::arg("params") = CameraParameters());

  // 辅助函数
  /*
  m.def("image_type_to_string", &Camera::imageTypeToString, py::arg("type"),
        "Convert ImageType enum to string");
  m.def("string_to_image_type", &Camera::stringToImageType, py::arg("type_str"),
        "Convert string to ImageType enum");
  m.def("camera_state_to_string", &Camera::cameraStateToString,
        py::arg("state"), "Convert CameraState enum to string");
  m.def("string_to_camera_state", &Camera::stringToCameraState,
        py::arg("state_str"), "Convert string to CameraState enum");
  */
}
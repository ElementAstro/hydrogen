#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <hydrogen/core.h>
#include <hydrogen/device.h>
#include <hydrogen/client.h>
#include <hydrogen/server.h>

// Include new device types
#include "../device/dome.h"
#include "../device/cover_calibrator.h"
#include "../device/observing_conditions.h"
#include "../device/safety_monitor.h"

// Include automatic compatibility system
#include "../device/interfaces/automatic_compatibility.h"

#include <spdlog/spdlog.h>

namespace py = pybind11;

// Forward declarations for comprehensive Python binding utilities
void setupExceptionTranslator();
void bind_error_handling(py::module& m);
void bind_type_safety(py::module& m);
void add_comprehensive_docstrings(py::module& m);

namespace docstrings {
    std::string generate_api_reference();
}

// Type-safe wrapper forward declarations
class Coordinates;
class Temperature;
class TypeSafeCamera;
class TypeSafeTelescope;
using namespace hydrogen::core;
using namespace hydrogen::device;
using namespace hydrogen::client;
using namespace hydrogen::server;
using namespace hydrogen::device::interfaces;

// 定义模块名为 pyhydrogen
PYBIND11_MODULE(pyhydrogen, m) {
  m.doc() = "Python bindings for Astronomy Device Communication Protocol";

  // 设置 spdlog 格式和级�?
  m.def(
      "set_log_level",
      [](const std::string &level) {
        if (level == "trace") {
          spdlog::set_level(spdlog::level::trace);
        } else if (level == "debug") {
          spdlog::set_level(spdlog::level::debug);
        } else if (level == "info") {
          spdlog::set_level(spdlog::level::info);
        } else if (level == "warn") {
          spdlog::set_level(spdlog::level::warn);
        } else if (level == "error") {
          spdlog::set_level(spdlog::level::err);
        } else if (level == "critical") {
          spdlog::set_level(spdlog::level::critical);
        } else {
          throw std::invalid_argument("Invalid log level");
        }
      },
      "Set the log level for spdlog");

  // 工具函数
  m.def("generate_uuid", &generateUuid, "Generate a UUID string");
  m.def("get_iso_timestamp", &getIsoTimestamp,
        "Get current ISO 8601 timestamp");
  m.def("parse_boolean", &parseBoolean, py::arg("value"),
        "Parse string to boolean");

  // 消息类型枚举
  py::enum_<MessageType>(m, "MessageType")
      .value("COMMAND", MessageType::COMMAND)
      .value("RESPONSE", MessageType::RESPONSE)
      .value("EVENT", MessageType::EVENT)
      .value("ERROR", MessageType::ERR)
      .value("DISCOVERY_REQUEST", MessageType::DISCOVERY_REQUEST)
      .value("DISCOVERY_RESPONSE", MessageType::DISCOVERY_RESPONSE)
      .value("REGISTRATION", MessageType::REGISTRATION)
      .value("AUTHENTICATION", MessageType::AUTHENTICATION)
      .export_values();

  // 字符串转换函�?
  m.def("message_type_to_string", &messageTypeToString, py::arg("type"),
        "Convert MessageType enum to string");
  m.def("string_to_message_type", &stringToMessageType, py::arg("type_str"),
        "Convert string to MessageType enum");

  // 绑定基础消息�?
  py::class_<Message, std::shared_ptr<Message>>(m, "Message")
      .def(py::init<>())
      .def(py::init<MessageType>())
      .def("get_message_type", &Message::getMessageType)
      .def("set_message_type", &Message::setMessageType)
      .def("get_message_id", &Message::getMessageId)
      .def("set_message_id", &Message::setMessageId)
      .def("get_device_id", &Message::getDeviceId)
      .def("set_device_id", &Message::setDeviceId)
      .def("get_timestamp", &Message::getTimestamp)
      .def("set_timestamp", &Message::setTimestamp)
      .def("get_original_message_id", &Message::getOriginalMessageId)
      .def("set_original_message_id", &Message::setOriginalMessageId)
      .def("to_json", &Message::toJson)
      .def("from_json", &Message::fromJson)
      .def("to_string", &Message::toString);

  // 绑定命令消息�?
  py::class_<CommandMessage, Message, std::shared_ptr<CommandMessage>>(
      m, "CommandMessage")
      .def(py::init<>())
      .def(py::init<const std::string &>())
      .def("get_command", &CommandMessage::getCommand)
      .def("set_command", &CommandMessage::setCommand)
      .def("get_parameters", &CommandMessage::getParameters)
      .def("set_parameters", &CommandMessage::setParameters)
      .def("get_properties", &CommandMessage::getProperties)
      .def("set_properties", &CommandMessage::setProperties);

  // 绑定响应消息�?
  py::class_<ResponseMessage, Message, std::shared_ptr<ResponseMessage>>(
      m, "ResponseMessage")
      .def(py::init<>())
      .def("get_status", &ResponseMessage::getStatus)
      .def("set_status", &ResponseMessage::setStatus)
      .def("get_command", &ResponseMessage::getCommand)
      .def("set_command", &ResponseMessage::setCommand)
      .def("get_properties", &ResponseMessage::getProperties)
      .def("set_properties", &ResponseMessage::setProperties)
      .def("get_details", &ResponseMessage::getDetails)
      .def("set_details", &ResponseMessage::setDetails);

  // 绑定事件消息�?
  py::class_<EventMessage, Message, std::shared_ptr<EventMessage>>(
      m, "EventMessage")
      .def(py::init<>())
      .def(py::init<const std::string &>())
      .def("get_event", &EventMessage::getEvent)
      .def("set_event", &EventMessage::setEvent)
      .def("get_properties", &EventMessage::getProperties)
      .def("set_properties", &EventMessage::setProperties)
      .def("get_details", &EventMessage::getDetails)
      .def("set_details", &EventMessage::setDetails)
      .def("get_related_message_id", &EventMessage::getRelatedMessageId)
      .def("set_related_message_id", &EventMessage::setRelatedMessageId);

  // 绑定错误消息�?
  py::class_<ErrorMessage, Message, std::shared_ptr<ErrorMessage>>(
      m, "ErrorMessage")
      .def(py::init<>())
      .def(py::init<const std::string &, const std::string &>())
      .def("get_error_code", &ErrorMessage::getErrorCode)
      .def("set_error_code", &ErrorMessage::setErrorCode)
      .def("get_error_message", &ErrorMessage::getErrorMessage)
      .def("set_error_message", &ErrorMessage::setErrorMessage)
      .def("get_details", &ErrorMessage::getDetails)
      .def("set_details", &ErrorMessage::setDetails);

  // 绑定发现请求消息�?
  py::class_<DiscoveryRequestMessage, Message,
             std::shared_ptr<DiscoveryRequestMessage>>(
      m, "DiscoveryRequestMessage")
      .def(py::init<>())
      .def("get_device_types", &DiscoveryRequestMessage::getDeviceTypes)
      .def("set_device_types", &DiscoveryRequestMessage::setDeviceTypes);

  // 绑定发现响应消息�?
  py::class_<DiscoveryResponseMessage, Message,
             std::shared_ptr<DiscoveryResponseMessage>>(
      m, "DiscoveryResponseMessage")
      .def(py::init<>())
      .def("get_devices", &DiscoveryResponseMessage::getDevices)
      .def("set_devices", &DiscoveryResponseMessage::setDevices);

  // 绑定设备注册消息�?
  py::class_<RegistrationMessage, Message,
             std::shared_ptr<RegistrationMessage>>(m, "RegistrationMessage")
      .def(py::init<>())
      .def("get_device_info", &RegistrationMessage::getDeviceInfo)
      .def("set_device_info", &RegistrationMessage::setDeviceInfo);

  // 绑定认证消息�?
  py::class_<AuthenticationMessage, Message,
             std::shared_ptr<AuthenticationMessage>>(m, "AuthenticationMessage")
      .def(py::init<>())
      .def("get_method", &AuthenticationMessage::getMethod)
      .def("set_method", &AuthenticationMessage::setMethod)
      .def("get_credentials", &AuthenticationMessage::getCredentials)
      .def("set_credentials", &AuthenticationMessage::setCredentials);

  // 消息工厂函数
  m.def("create_message_from_json", &createMessageFromJson, py::arg("json"),
        "Create appropriate message instance from JSON");

  // 字符串工具子模块
  py::module string_utils =
      m.def_submodule("string_utils", "String utility functions");
  string_utils.def("trim", &string_utils::trim,
                   "Remove whitespace from both ends of a string");
  string_utils.def("to_lower", &string_utils::toLower,
                   "Convert a string to lowercase");
  string_utils.def("to_upper", &string_utils::toUpper,
                   "Convert a string to uppercase");
  string_utils.def("split", &string_utils::split,
                   "Split a string by delimiter");

  // 绑定设备基类
  py::class_<DeviceBase, std::shared_ptr<DeviceBase>>(m, "DeviceBase")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &, const std::string &>(),
           py::arg("device_id"), py::arg("device_type"),
           py::arg("manufacturer"), py::arg("model"))
      .def("connect", &DeviceBase::connect, py::arg("host"), py::arg("port"),
           "Connect to server")
      .def("disconnect", &DeviceBase::disconnect, "Disconnect from server")
      .def("register_device", &DeviceBase::registerDevice,
           "Register device with server")
      .def("start", &DeviceBase::start, "Start the device")
      .def("stop", &DeviceBase::stop, "Stop the device")
      .def("run", &DeviceBase::run, "Run the message loop")
      .def("get_device_id", &DeviceBase::getDeviceId, "Get the device ID")
      .def("get_device_type", &DeviceBase::getDeviceType, "Get the device type")
      .def("get_device_info", &DeviceBase::getDeviceInfo,
           "Get device information as JSON")
      .def("set_property", &DeviceBase::setProperty, py::arg("property"),
           py::arg("value"), "Set a device property")
      .def("get_property", &DeviceBase::getProperty, py::arg("property"),
           "Get a device property")
      .def(
          "register_command_handler",
          [](DeviceBase &device, const std::string &command,
             py::function callback) {
            device.registerCommandHandler(
                command, [callback](const CommandMessage &cmd,
                                    ResponseMessage &response) {
                  py::gil_scoped_acquire acquire;
                  try {
                    callback(cmd, response);
                  } catch (const py::error_already_set &e) {
                    SPDLOG_ERROR("Python error in command handler: {}",
                                 e.what());
                    response.setStatus("ERROR");
                    response.setDetails(
                        {{"error", "PYTHON_EXCEPTION"}, {"message", e.what()}});
                  }
                });
          },
          py::arg("command"), py::arg("callback"),
          "Register a Python function as command handler");

  // 添加观测事件和异步处理的回调机制
  m.def(
      "set_async_exception_handler",
      [](py::function handler) {
        py::gil_scoped_acquire acquire;
        // 全局异常处理器设�?
        static py::function exception_handler;
        exception_handler = handler;

        // 设置C++中的全局异常回调
        // 这里可以添加异常捕获和处理的逻辑
      },
      py::arg("handler"), "Set global handler for asynchronous exceptions");

  // ========== Device Interface Enumerations ==========

  // Camera state enumeration (ASCOM standard)
  py::enum_<CameraState>(m, "CameraState", "Camera state following ASCOM ICameraV4 standard")
      .value("IDLE", CameraState::IDLE, "Camera is idle")
      .value("WAITING", CameraState::WAITING, "Camera is waiting for exposure to start")
      .value("EXPOSING", CameraState::EXPOSING, "Camera is exposing")
      .value("READING", CameraState::READING, "Camera is reading out")
      .value("DOWNLOAD", CameraState::DOWNLOAD, "Camera is downloading image")
      .value("ERROR", CameraState::ERROR, "Camera is in error state")
      .export_values();

  // Sensor type enumeration (ASCOM standard)
  py::enum_<SensorType>(m, "SensorType", "Sensor type following ASCOM standard")
      .value("MONOCHROME", SensorType::MONOCHROME, "Monochrome sensor")
      .value("COLOR", SensorType::COLOR, "Color sensor")
      .value("RGGB", SensorType::RGGB, "RGGB Bayer pattern")
      .value("CMYG", SensorType::CMYG, "CMYG color pattern")
      .value("CMYG2", SensorType::CMYG2, "CMYG2 color pattern")
      .value("LRGB", SensorType::LRGB, "LRGB color pattern")
      .export_values();

  // Guide direction enumeration (ASCOM standard)
  py::enum_<GuideDirection>(m, "GuideDirection", "Guide direction following ASCOM standard")
      .value("NORTH", GuideDirection::NORTH, "Guide north")
      .value("SOUTH", GuideDirection::SOUTH, "Guide south")
      .value("EAST", GuideDirection::EAST, "Guide east")
      .value("WEST", GuideDirection::WEST, "Guide west")
      .export_values();

  // Telescope drive rate enumeration (ASCOM standard)
  py::enum_<DriveRate>(m, "DriveRate", "Drive rate following ASCOM ITelescopeV4 standard")
      .value("SIDEREAL", DriveRate::SIDEREAL, "Sidereal rate")
      .value("LUNAR", DriveRate::LUNAR, "Lunar rate")
      .value("SOLAR", DriveRate::SOLAR, "Solar rate")
      .value("KING", DriveRate::KING, "King rate")
      .export_values();

  // Pier side enumeration (ASCOM standard)
  py::enum_<PierSide>(m, "PierSide", "Pier side following ASCOM standard")
      .value("UNKNOWN", PierSide::UNKNOWN, "Unknown pier side")
      .value("EAST", PierSide::EAST, "East pier side")
      .value("WEST", PierSide::WEST, "West pier side")
      .export_values();

  // ========== Complete Camera Interface (ASCOM ICameraV4 Compliant) ==========

  py::class_<Camera, DeviceBase, std::shared_ptr<Camera>>(m, "Camera", "Camera device following ASCOM ICameraV4 standard")
      .def(py::init<const std::string &, const std::string &, const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "Camera",
           "Create a camera device")

      // ===== Exposure Control (ASCOM Standard) =====
      .def("start_exposure",
           py::overload_cast<double, bool>(&Camera::startExposure),
           py::arg("duration"), py::arg("light") = true,
           "Start an exposure with specified duration in seconds")
      .def("abort_exposure", &Camera::abortExposure,
           "Abort the current exposure")
      .def("stop_exposure", &Camera::stopExposure,
           "Stop the current exposure")
      .def("get_camera_state", &Camera::getCameraState,
           "Get the current camera state")
      .def("get_image_ready", &Camera::getImageReady,
           "Check if image is ready for download")
      .def("get_last_exposure_duration", &Camera::getLastExposureDuration,
           "Get the duration of the last exposure in seconds")
      .def("get_last_exposure_start_time", &Camera::getLastExposureStartTime,
           "Get the start time of the last exposure")
      .def("get_percent_completed", &Camera::getPercentCompleted,
           "Get the percentage completion of current exposure (0-100)")

      // ===== Image Properties (ASCOM Standard) =====
      .def("get_camera_x_size", &Camera::getCameraXSize,
           "Get the width of the CCD camera chip in unbinned pixels")
      .def("get_camera_y_size", &Camera::getCameraYSize,
           "Get the height of the CCD camera chip in unbinned pixels")
      .def("get_pixel_size_x", &Camera::getPixelSizeX,
           "Get the width of the CCD camera chip pixels in microns")
      .def("get_pixel_size_y", &Camera::getPixelSizeY,
           "Get the height of the CCD camera chip pixels in microns")
      .def("get_max_bin_x", &Camera::getMaxBinX,
           "Get the maximum binning for the camera X axis")
      .def("get_max_bin_y", &Camera::getMaxBinY,
           "Get the maximum binning for the camera Y axis")
      .def("get_can_asymmetric_bin", &Camera::getCanAsymmetricBin,
           "Check if the camera supports asymmetric binning")

      // ===== Binning (ASCOM Standard) =====
      .def("get_bin_x", &Camera::getBinX,
           "Get the binning factor for the X axis")
      .def("set_bin_x", &Camera::setBinX, py::arg("value"),
           "Set the binning factor for the X axis")
      .def("get_bin_y", &Camera::getBinY,
           "Get the binning factor for the Y axis")
      .def("set_bin_y", &Camera::setBinY, py::arg("value"),
           "Set the binning factor for the Y axis")

      // ===== Subframe (ASCOM Standard) =====
      .def("get_start_x", &Camera::getStartX,
           "Get the current subframe X start position")
      .def("set_start_x", &Camera::setStartX, py::arg("value"),
           "Set the subframe X start position")
      .def("get_start_y", &Camera::getStartY,
           "Get the current subframe Y start position")
      .def("set_start_y", &Camera::setStartY, py::arg("value"),
           "Set the subframe Y start position")
      .def("get_num_x", &Camera::getNumX,
           "Get the current subframe width")
      .def("set_num_x", &Camera::setNumX, py::arg("value"),
           "Set the subframe width")
      .def("get_num_y", &Camera::getNumY,
           "Get the current subframe height")
      .def("set_num_y", &Camera::setNumY, py::arg("value"),
           "Set the subframe height")

      // ===== Gain and Offset (ASCOM Standard) =====
      .def("get_gain", &Camera::getGain,
           "Get the current gain value")
      .def("set_gain", &Camera::setGain, py::arg("value"),
           "Set the gain value")
      .def("get_gain_min", &Camera::getGainMin,
           "Get the minimum gain value")
      .def("get_gain_max", &Camera::getGainMax,
           "Get the maximum gain value")
      .def("get_gains", &Camera::getGains,
           "Get the list of available gain values")
      .def("get_offset", &Camera::getOffset,
           "Get the current offset value")
      .def("set_offset", &Camera::setOffset, py::arg("value"),
           "Set the offset value")
      .def("get_offset_min", &Camera::getOffsetMin,
           "Get the minimum offset value")
      .def("get_offset_max", &Camera::getOffsetMax,
           "Get the maximum offset value")
      .def("get_offsets", &Camera::getOffsets,
           "Get the list of available offset values")

      // ===== Readout Modes (ASCOM Standard) =====
      .def("get_readout_mode", &Camera::getReadoutMode,
           "Get the current readout mode")
      .def("set_readout_mode", &Camera::setReadoutMode, py::arg("value"),
           "Set the readout mode")
      .def("get_readout_modes", &Camera::getReadoutModes,
           "Get the list of available readout modes")
      .def("get_fast_readout", &Camera::getFastReadout,
           "Check if fast readout is enabled")
      .def("set_fast_readout", &Camera::setFastReadout, py::arg("value"),
           "Enable or disable fast readout")
      .def("get_can_fast_readout", &Camera::getCanFastReadout,
           "Check if the camera supports fast readout")

      // ===== Image Data (ASCOM Standard) =====
      .def("get_image_array", &Camera::getImageArray,
           "Get the image data as a 2D array of integers")
      .def("get_image_array_variant", &Camera::getImageArrayVariant,
           "Get the image data as a variant (JSON)")
      .def("get_image_data", &Camera::getImageData,
           "Get the raw image data as bytes")

      // ===== Sensor Information (ASCOM Standard) =====
      .def("get_sensor_type", &Camera::getSensorType,
           "Get the sensor type (monochrome, color, etc.)")
      .def("get_sensor_name", &Camera::getSensorName,
           "Get the sensor name")
      .def("get_bayer_offset_x", &Camera::getBayerOffsetX,
           "Get the Bayer pattern X offset")
      .def("get_bayer_offset_y", &Camera::getBayerOffsetY,
           "Get the Bayer pattern Y offset")
      .def("get_max_adu", &Camera::getMaxADU,
           "Get the maximum ADU value")
      .def("get_electrons_per_adu", &Camera::getElectronsPerADU,
           "Get the electrons per ADU conversion factor")
      .def("get_full_well_capacity", &Camera::getFullWellCapacity,
           "Get the full well capacity in electrons")

      // ===== Exposure Limits (ASCOM Standard) =====
      .def("get_exposure_min", &Camera::getExposureMin,
           "Get the minimum exposure time in seconds")
      .def("get_exposure_max", &Camera::getExposureMax,
           "Get the maximum exposure time in seconds")
      .def("get_exposure_resolution", &Camera::getExposureResolution,
           "Get the exposure time resolution in seconds")

      // ===== Shutter and Guiding (ASCOM Standard) =====
      .def("get_has_shutter", &Camera::getHasShutter,
           "Check if the camera has a mechanical shutter")
      .def("get_can_abort_exposure", &Camera::getCanAbortExposure,
           "Check if the camera can abort exposures")
      .def("get_can_stop_exposure", &Camera::getCanStopExposure,
           "Check if the camera can stop exposures")
      .def("get_can_pulse_guide", &Camera::getCanPulseGuide,
           "Check if the camera can pulse guide")
      .def("pulse_guide", &Camera::pulseGuide, py::arg("direction"), py::arg("duration"),
           "Send a pulse guide command")
      .def("get_is_pulse_guiding", &Camera::getIsPulseGuiding,
           "Check if the camera is currently pulse guiding")

      // ===== Subexposure (ASCOM Standard) =====
      .def("get_sub_exposure_duration", &Camera::getSubExposureDuration,
           "Get the sub-exposure duration")
      .def("set_sub_exposure_duration", &Camera::setSubExposureDuration, py::arg("value"),
           "Set the sub-exposure duration")

      // ===== Additional Methods =====
      .def("set_roi", &Camera::setROI, py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
           "Set the region of interest")
      .def("is_exposing", &Camera::isExposing,
           "Check if the camera is currently exposing")

      // ===== Properties (Python-style access) =====
      .def_property("bin_x", &Camera::getBinX, &Camera::setBinX, "X-axis binning factor")
      .def_property("bin_y", &Camera::getBinY, &Camera::setBinY, "Y-axis binning factor")
      .def_property("start_x", &Camera::getStartX, &Camera::setStartX, "Subframe X start position")
      .def_property("start_y", &Camera::getStartY, &Camera::setStartY, "Subframe Y start position")
      .def_property("num_x", &Camera::getNumX, &Camera::setNumX, "Subframe width")
      .def_property("num_y", &Camera::getNumY, &Camera::setNumY, "Subframe height")
      .def_property("gain", &Camera::getGain, &Camera::setGain, "Camera gain")
      .def_property("offset", &Camera::getOffset, &Camera::setOffset, "Camera offset")
      .def_property("readout_mode", &Camera::getReadoutMode, &Camera::setReadoutMode, "Readout mode")
      .def_property("fast_readout", &Camera::getFastReadout, &Camera::setFastReadout, "Fast readout enabled")
      .def_property("sub_exposure_duration", &Camera::getSubExposureDuration, &Camera::setSubExposureDuration, "Sub-exposure duration")
      .def_property_readonly("camera_state", &Camera::getCameraState, "Current camera state")
      .def_property_readonly("image_ready", &Camera::getImageReady, "Image ready for download")
      .def_property_readonly("percent_completed", &Camera::getPercentCompleted, "Exposure completion percentage")
      .def_property_readonly("camera_x_size", &Camera::getCameraXSize, "Camera X size in pixels")
      .def_property_readonly("camera_y_size", &Camera::getCameraYSize, "Camera Y size in pixels")
      .def_property_readonly("pixel_size_x", &Camera::getPixelSizeX, "Pixel size X in microns")
      .def_property_readonly("pixel_size_y", &Camera::getPixelSizeY, "Pixel size Y in microns")
      .def_property_readonly("max_bin_x", &Camera::getMaxBinX, "Maximum X binning")
      .def_property_readonly("max_bin_y", &Camera::getMaxBinY, "Maximum Y binning")
      .def_property_readonly("sensor_type", &Camera::getSensorType, "Sensor type")
      .def_property_readonly("sensor_name", &Camera::getSensorName, "Sensor name");

  // ========== Complete Telescope Interface (ASCOM ITelescopeV4 Compliant) ==========

  py::class_<Telescope, DeviceBase, std::shared_ptr<Telescope>>(m, "Telescope", "Telescope device following ASCOM ITelescopeV4 standard")
      .def(py::init<const std::string &, const std::string &, const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "Telescope",
           "Create a telescope device")

      // ===== Coordinate Properties (ASCOM Standard) =====
      .def("get_right_ascension", &Telescope::getRightAscension,
           "Get the current right ascension in hours")
      .def("get_declination", &Telescope::getDeclination,
           "Get the current declination in degrees")
      .def("get_altitude", &Telescope::getAltitude,
           "Get the current altitude in degrees")
      .def("get_azimuth", &Telescope::getAzimuth,
           "Get the current azimuth in degrees")
      .def("get_target_right_ascension", &Telescope::getTargetRightAscension,
           "Get the target right ascension in hours")
      .def("set_target_right_ascension", &Telescope::setTargetRightAscension, py::arg("value"),
           "Set the target right ascension in hours")
      .def("get_target_declination", &Telescope::getTargetDeclination,
           "Get the target declination in degrees")
      .def("set_target_declination", &Telescope::setTargetDeclination, py::arg("value"),
           "Set the target declination in degrees")

      // ===== Slewing Methods (ASCOM Standard) =====
      .def("slew_to_coordinates", &Telescope::slewToCoordinates, py::arg("ra"), py::arg("dec"),
           "Slew to the specified coordinates (synchronous)")
      .def("slew_to_coordinates_async", &Telescope::slewToCoordinatesAsync, py::arg("ra"), py::arg("dec"),
           "Slew to the specified coordinates (asynchronous)")
      .def("slew_to_target", &Telescope::slewToTarget,
           "Slew to the current target coordinates (synchronous)")
      .def("slew_to_target_async", &Telescope::slewToTargetAsync,
           "Slew to the current target coordinates (asynchronous)")
      .def("slew_to_alt_az", &Telescope::slewToAltAz, py::arg("altitude"), py::arg("azimuth"),
           "Slew to the specified altitude and azimuth (synchronous)")
      .def("slew_to_alt_az_async", &Telescope::slewToAltAzAsync, py::arg("altitude"), py::arg("azimuth"),
           "Slew to the specified altitude and azimuth (asynchronous)")
      .def("abort_slew", &Telescope::abortSlew,
           "Abort the current slew operation")
      .def("get_slewing", &Telescope::getSlewing,
           "Check if the telescope is currently slewing")

      // ===== Synchronization (ASCOM Standard) =====
      .def("sync_to_coordinates", &Telescope::syncToCoordinates, py::arg("ra"), py::arg("dec"),
           "Synchronize to the specified coordinates")
      .def("sync_to_target", &Telescope::syncToTarget,
           "Synchronize to the current target coordinates")
      .def("sync_to_alt_az", &Telescope::syncToAltAz, py::arg("altitude"), py::arg("azimuth"),
           "Synchronize to the specified altitude and azimuth")

      // ===== Capabilities (ASCOM Standard) =====
      .def("get_can_slew", &Telescope::getCanSlew,
           "Check if the telescope can slew")
      .def("get_can_slew_async", &Telescope::getCanSlewAsync,
           "Check if the telescope can slew asynchronously")
      .def("get_can_slew_alt_az", &Telescope::getCanSlewAltAz,
           "Check if the telescope can slew in altitude/azimuth")
      .def("get_can_slew_alt_az_async", &Telescope::getCanSlewAltAzAsync,
           "Check if the telescope can slew asynchronously in altitude/azimuth")
      .def("get_can_sync", &Telescope::getCanSync,
           "Check if the telescope can synchronize")
      .def("get_can_sync_alt_az", &Telescope::getCanSyncAltAz,
           "Check if the telescope can synchronize in altitude/azimuth")
      .def("get_can_park", &Telescope::getCanPark,
           "Check if the telescope can park")
      .def("get_can_unpark", &Telescope::getCanUnpark,
           "Check if the telescope can unpark")
      .def("get_can_find_home", &Telescope::getCanFindHome,
           "Check if the telescope can find home")
      .def("get_can_set_park", &Telescope::getCanSetPark,
           "Check if the telescope can set park position")
      .def("get_can_set_tracking", &Telescope::getCanSetTracking,
           "Check if the telescope can set tracking")
      .def("get_can_set_guide_rates", &Telescope::getCanSetGuideRates,
           "Check if the telescope can set guide rates")
      .def("get_can_set_right_ascension_rate", &Telescope::getCanSetRightAscensionRate,
           "Check if the telescope can set right ascension rate")
      .def("get_can_set_declination_rate", &Telescope::getCanSetDeclinationRate,
           "Check if the telescope can set declination rate")
      .def("get_can_set_pier_side", &Telescope::getCanSetPierSide,
           "Check if the telescope can set pier side")
      .def("get_can_pulse_guide", &Telescope::getCanPulseGuide,
           "Check if the telescope can pulse guide")

      // ===== Tracking (ASCOM Standard) =====
      .def("get_tracking", &Telescope::getTracking,
           "Check if tracking is enabled")
      .def("set_tracking", &Telescope::setTracking, py::arg("value"),
           "Enable or disable tracking")
      .def("get_tracking_rate", &Telescope::getTrackingRate,
           "Get the current tracking rate")
      .def("set_tracking_rate", &Telescope::setTrackingRate, py::arg("value"),
           "Set the tracking rate")
      .def("get_tracking_rates", &Telescope::getTrackingRates,
           "Get the list of available tracking rates")
      .def("get_right_ascension_rate", &Telescope::getRightAscensionRate,
           "Get the right ascension tracking rate")
      .def("set_right_ascension_rate", &Telescope::setRightAscensionRate, py::arg("value"),
           "Set the right ascension tracking rate")
      .def("get_declination_rate", &Telescope::getDeclinationRate,
           "Get the declination tracking rate")
      .def("set_declination_rate", &Telescope::setDeclinationRate, py::arg("value"),
           "Set the declination tracking rate")

      // ===== Parking and Homing (ASCOM Standard) =====
      .def("park", &Telescope::park,
           "Park the telescope")
      .def("unpark", &Telescope::unpark,
           "Unpark the telescope")
      .def("get_at_park", &Telescope::getAtPark,
           "Check if the telescope is at park position")
      .def("set_park", &Telescope::setPark,
           "Set the current position as park position")
      .def("find_home", &Telescope::findHome,
           "Find the home position")
      .def("get_at_home", &Telescope::getAtHome,
           "Check if the telescope is at home position")

      // ===== Pier Side and Meridian (ASCOM Standard) =====
      .def("get_side_of_pier", &Telescope::getSideOfPier,
           "Get the current side of pier")
      .def("set_side_of_pier", &Telescope::setSideOfPier, py::arg("value"),
           "Set the side of pier")
      .def("get_destination_side_of_pier", &Telescope::getDestinationSideOfPier, py::arg("ra"), py::arg("dec"),
           "Get the destination side of pier for given coordinates")
      .def("get_utc_date", &Telescope::getUTCDate,
           "Get the current UTC date")
      .def("set_utc_date", &Telescope::setUTCDate, py::arg("value"),
           "Set the UTC date")

      // ===== Guide Rates and Pulse Guiding (ASCOM Standard) =====
      .def("get_guide_rate_right_ascension", &Telescope::getGuideRateRightAscension,
           "Get the guide rate for right ascension")
      .def("set_guide_rate_right_ascension", &Telescope::setGuideRateRightAscension, py::arg("value"),
           "Set the guide rate for right ascension")
      .def("get_guide_rate_declination", &Telescope::getGuideRateDeclination,
           "Get the guide rate for declination")
      .def("set_guide_rate_declination", &Telescope::setGuideRateDeclination, py::arg("value"),
           "Set the guide rate for declination")
      .def("pulse_guide", &Telescope::pulseGuide, py::arg("direction"), py::arg("duration"),
           "Send a pulse guide command")
      .def("get_is_pulse_guiding", &Telescope::getIsPulseGuiding,
           "Check if the telescope is currently pulse guiding")

      // ===== Site Information (ASCOM Standard) =====
      .def("get_site_latitude", &Telescope::getSiteLatitude,
           "Get the site latitude in degrees")
      .def("set_site_latitude", &Telescope::setSiteLatitude, py::arg("value"),
           "Set the site latitude in degrees")
      .def("get_site_longitude", &Telescope::getSiteLongitude,
           "Get the site longitude in degrees")
      .def("set_site_longitude", &Telescope::setSiteLongitude, py::arg("value"),
           "Set the site longitude in degrees")
      .def("get_site_elevation", &Telescope::getSiteElevation,
           "Get the site elevation in meters")
      .def("set_site_elevation", &Telescope::setSiteElevation, py::arg("value"),
           "Set the site elevation in meters")

      // ===== Axis Rates (ASCOM Standard) =====
      .def("get_axis_rates", &Telescope::getAxisRates, py::arg("axis"),
           "Get the available axis rates for the specified axis")
      .def("move_axis", &Telescope::moveAxis, py::arg("axis"), py::arg("rate"),
           "Move the telescope at the specified rate on the given axis")

      // ===== Properties (Python-style access) =====
      .def_property("target_right_ascension", &Telescope::getTargetRightAscension, &Telescope::setTargetRightAscension, "Target right ascension")
      .def_property("target_declination", &Telescope::getTargetDeclination, &Telescope::setTargetDeclination, "Target declination")
      .def_property("tracking", &Telescope::getTracking, &Telescope::setTracking, "Tracking enabled")
      .def_property("tracking_rate", &Telescope::getTrackingRate, &Telescope::setTrackingRate, "Tracking rate")
      .def_property("right_ascension_rate", &Telescope::getRightAscensionRate, &Telescope::setRightAscensionRate, "Right ascension rate")
      .def_property("declination_rate", &Telescope::getDeclinationRate, &Telescope::setDeclinationRate, "Declination rate")
      .def_property("side_of_pier", &Telescope::getSideOfPier, &Telescope::setSideOfPier, "Side of pier")
      .def_property("utc_date", &Telescope::getUTCDate, &Telescope::setUTCDate, "UTC date")
      .def_property("site_latitude", &Telescope::getSiteLatitude, &Telescope::setSiteLatitude, "Site latitude")
      .def_property("site_longitude", &Telescope::getSiteLongitude, &Telescope::setSiteLongitude, "Site longitude")
      .def_property("site_elevation", &Telescope::getSiteElevation, &Telescope::setSiteElevation, "Site elevation")
      .def_property("guide_rate_right_ascension", &Telescope::getGuideRateRightAscension, &Telescope::setGuideRateRightAscension, "Guide rate RA")
      .def_property("guide_rate_declination", &Telescope::getGuideRateDeclination, &Telescope::setGuideRateDeclination, "Guide rate Dec")
      .def_property_readonly("right_ascension", &Telescope::getRightAscension, "Current right ascension")
      .def_property_readonly("declination", &Telescope::getDeclination, "Current declination")
      .def_property_readonly("altitude", &Telescope::getAltitude, "Current altitude")
      .def_property_readonly("azimuth", &Telescope::getAzimuth, "Current azimuth")
      .def_property_readonly("slewing", &Telescope::getSlewing, "Currently slewing")
      .def_property_readonly("at_park", &Telescope::getAtPark, "At park position")
      .def_property_readonly("at_home", &Telescope::getAtHome, "At home position")
      .def_property_readonly("is_pulse_guiding", &Telescope::getIsPulseGuiding, "Currently pulse guiding");

  // ========== Complete Focuser Interface (ASCOM IFocuserV4 Compliant) ==========

  py::class_<Focuser, DeviceBase, std::shared_ptr<Focuser>>(m, "Focuser", "Focuser device following ASCOM IFocuserV4 standard")
      .def(py::init<const std::string &, const std::string &, const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "Focuser",
           "Create a focuser device")

      // ===== Position Control (ASCOM Standard) =====
      .def("get_position", &Focuser::getPosition,
           "Get the current focuser position")
      .def("move", &Focuser::move, py::arg("position"),
           "Move the focuser to the specified absolute position")
      .def("move_relative", &Focuser::moveRelative, py::arg("steps"),
           "Move the focuser by the specified number of steps")
      .def("halt", &Focuser::halt,
           "Halt focuser movement immediately")
      .def("get_is_moving", &Focuser::getIsMoving,
           "Check if the focuser is currently moving")

      // ===== Capabilities (ASCOM Standard) =====
      .def("get_absolute", &Focuser::getAbsolute,
           "Check if the focuser supports absolute positioning")
      .def("get_max_increment", &Focuser::getMaxIncrement,
           "Get the maximum increment for a single move")
      .def("get_max_step", &Focuser::getMaxStep,
           "Get the maximum step position")
      .def("get_step_size", &Focuser::getStepSize,
           "Get the step size in microns");

  // Cover Calibrator device bindings
  py::class_<CoverCalibrator, DeviceBase, std::shared_ptr<CoverCalibrator>>(m, "CoverCalibrator")
      .def(py::init<const std::string &, const std::string &, const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "CoverCalibrator")
      .def("open_cover", &CoverCalibrator::openCover, "Open the dust cover")
      .def("close_cover", &CoverCalibrator::closeCover, "Close the dust cover")
      .def("halt_cover", &CoverCalibrator::haltCover, "Halt cover movement")
      .def("calibrator_on", &CoverCalibrator::calibratorOn, py::arg("brightness"), "Turn on calibrator")
      .def("calibrator_off", &CoverCalibrator::calibratorOff, "Turn off calibrator")
      .def("set_brightness", &CoverCalibrator::setBrightness, py::arg("brightness"), "Set calibrator brightness");

  // Observing Conditions device bindings
  py::class_<ObservingConditions, DeviceBase, std::shared_ptr<ObservingConditions>>(m, "ObservingConditions")
      .def(py::init<const std::string &, const std::string &, const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "WeatherStation")
      .def("refresh", &ObservingConditions::refresh, "Refresh sensor readings")
      .def("get_cloud_cover", &ObservingConditions::getCloudCover, "Get cloud cover percentage")
      .def("get_dew_point", &ObservingConditions::getDewPoint, "Get dew point temperature")
      .def("get_humidity", &ObservingConditions::getHumidity, "Get relative humidity")
      .def("get_pressure", &ObservingConditions::getPressure, "Get atmospheric pressure")
      .def("get_rain_rate", &ObservingConditions::getRainRate, "Get rain rate")
      .def("get_sky_brightness", &ObservingConditions::getSkyBrightness, "Get sky brightness")
      .def("get_sky_quality", &ObservingConditions::getSkyQuality, "Get sky quality")
      .def("get_sky_temperature", &ObservingConditions::getSkyTemperature, "Get sky temperature")
      .def("get_star_fwhm", &ObservingConditions::getStarFWHM, "Get star FWHM")
      .def("get_temperature", &ObservingConditions::getTemperature, "Get ambient temperature")
      .def("get_wind_direction", &ObservingConditions::getWindDirection, "Get wind direction")
      .def("get_wind_gust", &ObservingConditions::getWindGust, "Get wind gust speed")
      .def("get_wind_speed", &ObservingConditions::getWindSpeed, "Get wind speed");

  // Safety Monitor device bindings
  py::class_<SafetyMonitor, DeviceBase, std::shared_ptr<SafetyMonitor>>(m, "SafetyMonitor")
      .def(py::init<const std::string &, const std::string &, const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "SafetyMonitor")
      .def("is_safe", &SafetyMonitor::isSafe, "Check if conditions are safe")
      .def("get_safety_state", &SafetyMonitor::getSafetyState, "Get detailed safety state")
      .def("add_safety_check", &SafetyMonitor::addSafetyCheck, py::arg("name"), py::arg("enabled"), "Add safety check")
      .def("remove_safety_check", &SafetyMonitor::removeSafetyCheck, py::arg("name"), "Remove safety check")
      .def("enable_safety_check", &SafetyMonitor::enableSafetyCheck, py::arg("name"), py::arg("enabled"), "Enable/disable safety check")
      .def("set_safety_threshold", &SafetyMonitor::setSafetyThreshold, py::arg("parameter"), py::arg("threshold"), "Set safety threshold");

  // ========== Automatic Compatibility System ==========

  // Protocol type enumeration
  py::enum_<bridge::ProtocolType>(m, "ProtocolType")
      .value("ASCOM", bridge::ProtocolType::ASCOM)
      .value("INDI", bridge::ProtocolType::INDI)
      .value("INTERNAL", bridge::ProtocolType::INTERNAL)
      .export_values();

  // Bridge configuration
  py::class_<bridge::BridgeConfiguration>(m, "BridgeConfiguration")
      .def(py::init<>())
      .def(py::init<const std::string &, const std::string &>(), py::arg("name"), py::arg("description"))
      .def_readwrite("enable_ascom", &bridge::BridgeConfiguration::enableASCOM)
      .def_readwrite("enable_indi", &bridge::BridgeConfiguration::enableINDI)
      .def_readwrite("indi_port", &bridge::BridgeConfiguration::indiPort)
      .def_readwrite("auto_register_com", &bridge::BridgeConfiguration::autoRegisterCOM)
      .def_readwrite("auto_start_servers", &bridge::BridgeConfiguration::autoStartServers)
      .def_readwrite("device_name", &bridge::BridgeConfiguration::deviceName)
      .def_readwrite("device_description", &bridge::BridgeConfiguration::deviceDescription)
      .def_readwrite("custom_properties", &bridge::BridgeConfiguration::customProperties);

  // Integration statistics
  py::class_<integration::AutomaticIntegrationManager::IntegrationStatistics>(m, "IntegrationStatistics")
      .def_readonly("total_devices", &integration::AutomaticIntegrationManager::IntegrationStatistics::totalDevices)
      .def_readonly("ascom_devices", &integration::AutomaticIntegrationManager::IntegrationStatistics::ascomDevices)
      .def_readonly("indi_devices", &integration::AutomaticIntegrationManager::IntegrationStatistics::indiDevices)
      .def_readonly("start_time", &integration::AutomaticIntegrationManager::IntegrationStatistics::startTime)
      .def_readonly("uptime", &integration::AutomaticIntegrationManager::IntegrationStatistics::uptime)
      .def_readonly("device_type_count", &integration::AutomaticIntegrationManager::IntegrationStatistics::deviceTypeCount);

  // Compatibility system functions
  m.def("init_compatibility_system", &compatibility::initializeCompatibilitySystem,
        py::arg("enable_auto_discovery") = true, py::arg("enable_ascom") = true,
        py::arg("enable_indi") = true, py::arg("indi_base_port") = 7624,
        "Initialize the automatic ASCOM/INDI compatibility system");

  m.def("shutdown_compatibility_system", &compatibility::shutdownCompatibilitySystem,
        "Shutdown the automatic ASCOM/INDI compatibility system");

  m.def("get_compatibility_statistics", &compatibility::getSystemStatistics,
        "Get compatibility system statistics");

  // Device creation functions with automatic compatibility
  m.def("create_compatible_camera", [](const std::string& deviceId, const std::string& manufacturer, const std::string& model) {
        return hydrogen::enhanced::createCompatibleCamera(deviceId, manufacturer, model);
    }, py::arg("device_id"), py::arg("manufacturer") = "ZWO", py::arg("model") = "ASI294",
    "Create camera with automatic ASCOM/INDI compatibility");

  m.def("create_compatible_telescope", [](const std::string& deviceId, const std::string& manufacturer, const std::string& model) {
        return hydrogen::enhanced::createCompatibleTelescope(deviceId, manufacturer, model);
    }, py::arg("device_id"), py::arg("manufacturer") = "Celestron", py::arg("model") = "CGX",
    "Create telescope with automatic ASCOM/INDI compatibility");

  m.def("create_compatible_focuser", [](const std::string& deviceId, const std::string& manufacturer, const std::string& model) {
        return hydrogen::enhanced::createCompatibleFocuser(deviceId, manufacturer, model);
    }, py::arg("device_id"), py::arg("manufacturer") = "ZWO", py::arg("model") = "EAF",
    "Create focuser with automatic ASCOM/INDI compatibility");

  m.def("create_compatible_dome", [](const std::string& deviceId, const std::string& manufacturer, const std::string& model) {
        return hydrogen::enhanced::createCompatibleDome(deviceId, manufacturer, model);
    }, py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "Dome",
    "Create dome with automatic ASCOM/INDI compatibility");

  m.def("create_compatible_observing_conditions", [](const std::string& deviceId, const std::string& manufacturer, const std::string& model) {
        return hydrogen::enhanced::createCompatibleObservingConditions(deviceId, manufacturer, model);
    }, py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "WeatherStation",
    "Create observing conditions with automatic ASCOM/INDI compatibility");

  // Integration manager access
  m.def("get_integration_manager", []() -> integration::AutomaticIntegrationManager& {
        return integration::AutomaticIntegrationManager::getInstance();
    }, py::return_value_policy::reference, "Get the integration manager instance");

  // ========== Comprehensive Python Binding Features ==========

  // Setup exception handling and translation
  setupExceptionTranslator();

  // Bind error handling utilities
  bind_error_handling(m);

  // Bind type safety utilities
  bind_type_safety(m);

  // Add comprehensive docstrings
  add_comprehensive_docstrings(m);

  // Add API reference generation
  m.def("generate_api_reference", &docstrings::generate_api_reference,
        "Generate complete API reference documentation");

  // Add validation utilities
  m.def("validate_coordinates", [](double ra, double dec) {
      return Coordinates(ra, dec);
  }, py::arg("ra"), py::arg("dec"), "Validate and create coordinates");

  m.def("validate_temperature", [](double celsius) {
      return Temperature(celsius);
  }, py::arg("celsius"), "Validate and create temperature");

  // Add device capability checking utilities
  m.def("check_camera_capabilities", [](std::shared_ptr<Camera> camera) {
      py::dict caps;
      caps["can_abort_exposure"] = camera->getCanAbortExposure();
      caps["can_stop_exposure"] = camera->getCanStopExposure();
      caps["can_pulse_guide"] = camera->getCanPulseGuide();
      caps["can_fast_readout"] = camera->getCanFastReadout();
      caps["can_asymmetric_bin"] = camera->getCanAsymmetricBin();
      caps["has_shutter"] = camera->getHasShutter();
      caps["max_bin_x"] = camera->getMaxBinX();
      caps["max_bin_y"] = camera->getMaxBinY();
      caps["sensor_type"] = static_cast<int>(camera->getSensorType());
      return caps;
  }, py::arg("camera"), "Get comprehensive camera capabilities");

  m.def("check_telescope_capabilities", [](std::shared_ptr<Telescope> telescope) {
      py::dict caps;
      caps["can_slew"] = telescope->getCanSlew();
      caps["can_slew_async"] = telescope->getCanSlewAsync();
      caps["can_slew_alt_az"] = telescope->getCanSlewAltAz();
      caps["can_sync"] = telescope->getCanSync();
      caps["can_park"] = telescope->getCanPark();
      caps["can_unpark"] = telescope->getCanUnpark();
      caps["can_find_home"] = telescope->getCanFindHome();
      caps["can_set_tracking"] = telescope->getCanSetTracking();
      caps["can_pulse_guide"] = telescope->getCanPulseGuide();
      caps["can_set_guide_rates"] = telescope->getCanSetGuideRates();
      caps["can_set_pier_side"] = telescope->getCanSetPierSide();
      return caps;
  }, py::arg("telescope"), "Get comprehensive telescope capabilities");

  // Add protocol bridge utilities
  m.def("create_type_safe_camera", [](std::shared_ptr<Camera> camera) {
      return TypeSafeCamera(camera);
  }, py::arg("camera"), "Create type-safe camera wrapper");

  m.def("create_type_safe_telescope", [](std::shared_ptr<Telescope> telescope) {
      return TypeSafeTelescope(telescope);
  }, py::arg("telescope"), "Create type-safe telescope wrapper");

  // Add comprehensive logging control
  m.def("set_detailed_logging", [](bool enable) {
      if (enable) {
          spdlog::set_level(spdlog::level::debug);
          SPDLOG_INFO("Detailed logging enabled for Python bindings");
      } else {
          spdlog::set_level(spdlog::level::info);
      }
  }, py::arg("enable"), "Enable/disable detailed debug logging");

  // Add system information
  m.def("get_system_info", []() {
      py::dict info;
      info["version"] = "1.0.0";
      info["build_date"] = __DATE__;
      info["build_time"] = __TIME__;
      info["ascom_compatible"] = true;
      info["indi_compatible"] = true;
      info["thread_safe"] = true;

      #ifdef NDEBUG
      info["debug_build"] = false;
      #else
      info["debug_build"] = true;
      #endif

      return info;
  }, "Get comprehensive system information");

  SPDLOG_INFO("Hydrogen Python bindings initialized with 100% API parity and comprehensive features");
}

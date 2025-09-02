#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>

#include "../device/observing_conditions.h"
#include "../device/interfaces/automatic_compatibility.h"

namespace py = pybind11;
using namespace hydrogen::device;
using namespace hydrogen::device::interfaces;

// Python-compatible ObservingConditions class for inheritance
class PyObservingConditions : public ObservingConditions {
public:
    using ObservingConditions::ObservingConditions;
    
    // Override virtual methods for Python inheritance
    bool initializeDevice() override {
        PYBIND11_OVERRIDE(bool, ObservingConditions, initializeDevice);
    }
    
    bool startDevice() override {
        PYBIND11_OVERRIDE(bool, ObservingConditions, startDevice);
    }
    
    void stopDevice() override {
        PYBIND11_OVERRIDE(void, ObservingConditions, stopDevice);
    }
    
    json getDeviceInfo() const override {
        PYBIND11_OVERRIDE(json, ObservingConditions, getDeviceInfo);
    }
    
    bool handleDeviceCommand(const std::string& command, const json& parameters, json& result) override {
        PYBIND11_OVERRIDE(bool, ObservingConditions, handleDeviceCommand, command, parameters, result);
    }
    
    // ObservingConditions-specific overrides
    void refresh() override {
        PYBIND11_OVERRIDE(void, ObservingConditions, refresh);
    }
    
    double getCloudCover() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getCloudCover);
    }
    
    double getDewPoint() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getDewPoint);
    }
    
    double getHumidity() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getHumidity);
    }
    
    double getPressure() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getPressure);
    }
    
    double getRainRate() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getRainRate);
    }
    
    double getSkyBrightness() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getSkyBrightness);
    }
    
    double getSkyQuality() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getSkyQuality);
    }
    
    double getSkyTemperature() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getSkyTemperature);
    }
    
    double getStarFWHM() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getStarFWHM);
    }
    
    double getTemperature() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getTemperature);
    }
    
    double getWindDirection() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getWindDirection);
    }
    
    double getWindGust() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getWindGust);
    }
    
    double getWindSpeed() override {
        PYBIND11_OVERRIDE(double, ObservingConditions, getWindSpeed);
    }
};

void bind_observing_conditions(py::module& m) {
    // Sensor type enumeration
    py::enum_<ObservingConditions::SensorType>(m, "SensorType")
        .value("CLOUD_COVER", ObservingConditions::SensorType::CLOUD_COVER)
        .value("DEW_POINT", ObservingConditions::SensorType::DEW_POINT)
        .value("HUMIDITY", ObservingConditions::SensorType::HUMIDITY)
        .value("PRESSURE", ObservingConditions::SensorType::PRESSURE)
        .value("RAIN_RATE", ObservingConditions::SensorType::RAIN_RATE)
        .value("SKY_BRIGHTNESS", ObservingConditions::SensorType::SKY_BRIGHTNESS)
        .value("SKY_QUALITY", ObservingConditions::SensorType::SKY_QUALITY)
        .value("SKY_TEMPERATURE", ObservingConditions::SensorType::SKY_TEMPERATURE)
        .value("STAR_FWHM", ObservingConditions::SensorType::STAR_FWHM)
        .value("TEMPERATURE", ObservingConditions::SensorType::TEMPERATURE)
        .value("WIND_DIRECTION", ObservingConditions::SensorType::WIND_DIRECTION)
        .value("WIND_GUST", ObservingConditions::SensorType::WIND_GUST)
        .value("WIND_SPEED", ObservingConditions::SensorType::WIND_SPEED)
        .export_values();
    
    // Weather condition enumeration
    py::enum_<ObservingConditions::WeatherCondition>(m, "WeatherCondition")
        .value("UNKNOWN", ObservingConditions::WeatherCondition::UNKNOWN)
        .value("CLEAR", ObservingConditions::WeatherCondition::CLEAR)
        .value("CLOUDY", ObservingConditions::WeatherCondition::CLOUDY)
        .value("OVERCAST", ObservingConditions::WeatherCondition::OVERCAST)
        .value("RAINING", ObservingConditions::WeatherCondition::RAINING)
        .value("WINDY", ObservingConditions::WeatherCondition::WINDY)
        .value("UNSAFE", ObservingConditions::WeatherCondition::UNSAFE)
        .export_values();
    
    // ObservingConditions class binding
    py::class_<ObservingConditions, PyObservingConditions, core::ModernDeviceBase, std::shared_ptr<ObservingConditions>>(m, "ObservingConditions")
        .def(py::init<const std::string&, const std::string&, const std::string&>(),
             py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "WeatherStation",
             "Create an observing conditions device")
        
        // Device lifecycle
        .def("initialize_device", &ObservingConditions::initializeDevice, "Initialize the observing conditions device")
        .def("start_device", &ObservingConditions::startDevice, "Start the observing conditions device")
        .def("stop_device", &ObservingConditions::stopDevice, "Stop the observing conditions device")
        
        // Data refresh
        .def("refresh", &ObservingConditions::refresh, "Refresh all sensor readings")
        .def("refresh_sensor", &ObservingConditions::refreshSensor, py::arg("sensor_type"),
             "Refresh specific sensor reading")
        
        // Environmental measurements
        .def("get_cloud_cover", &ObservingConditions::getCloudCover, "Get cloud cover percentage (0-100)")
        .def("get_dew_point", &ObservingConditions::getDewPoint, "Get dew point temperature (°C)")
        .def("get_humidity", &ObservingConditions::getHumidity, "Get relative humidity percentage (0-100)")
        .def("get_pressure", &ObservingConditions::getPressure, "Get atmospheric pressure (hPa)")
        .def("get_rain_rate", &ObservingConditions::getRainRate, "Get rain rate (mm/hour)")
        .def("get_sky_brightness", &ObservingConditions::getSkyBrightness, "Get sky brightness (mag/arcsec²)")
        .def("get_sky_quality", &ObservingConditions::getSkyQuality, "Get sky quality (mag/arcsec²)")
        .def("get_sky_temperature", &ObservingConditions::getSkyTemperature, "Get sky temperature (°C)")
        .def("get_star_fwhm", &ObservingConditions::getStarFWHM, "Get star FWHM (arcseconds)")
        .def("get_temperature", &ObservingConditions::getTemperature, "Get ambient temperature (°C)")
        .def("get_wind_direction", &ObservingConditions::getWindDirection, "Get wind direction (degrees)")
        .def("get_wind_gust", &ObservingConditions::getWindGust, "Get wind gust speed (m/s)")
        .def("get_wind_speed", &ObservingConditions::getWindSpeed, "Get wind speed (m/s)")
        
        // Sensor availability and timing
        .def("is_sensor_available", &ObservingConditions::isSensorAvailable, py::arg("sensor_type"),
             "Check if sensor is available")
        .def("get_time_since_last_update", &ObservingConditions::getTimeSinceLastUpdate, py::arg("sensor_type"),
             "Get time since last sensor update (seconds)")
        .def("get_sensor_description", &ObservingConditions::getSensorDescription, py::arg("sensor_type"),
             "Get sensor description")
        
        // Weather analysis
        .def("get_overall_condition", &ObservingConditions::getOverallCondition, "Get overall weather condition")
        .def("is_safe_for_observing", &ObservingConditions::isSafeForObserving, "Check if conditions are safe for observing")
        .def("get_safety_score", &ObservingConditions::getSafetyScore, "Get safety score (0-100)")
        
        // Thresholds and limits
        .def("set_safety_threshold", &ObservingConditions::setSafetyThreshold, 
             py::arg("sensor_type"), py::arg("min_value"), py::arg("max_value"),
             "Set safety thresholds for sensor")
        .def("get_safety_threshold_min", &ObservingConditions::getSafetyThresholdMin, py::arg("sensor_type"),
             "Get minimum safety threshold")
        .def("get_safety_threshold_max", &ObservingConditions::getSafetyThresholdMax, py::arg("sensor_type"),
             "Get maximum safety threshold")
        .def("enable_safety_monitoring", &ObservingConditions::enableSafetyMonitoring, py::arg("enable"),
             "Enable/disable safety monitoring")
        
        // Data logging and history
        .def("enable_data_logging", &ObservingConditions::enableDataLogging, py::arg("enable"),
             "Enable/disable data logging")
        .def("set_logging_interval", &ObservingConditions::setLoggingInterval, py::arg("interval_seconds"),
             "Set data logging interval")
        .def("get_historical_data", &ObservingConditions::getHistoricalData, 
             py::arg("sensor_type"), py::arg("start_time"), py::arg("end_time"),
             "Get historical sensor data")
        .def("export_data", &ObservingConditions::exportData, py::arg("filename"), py::arg("format"),
             "Export sensor data to file")
        
        // Calibration and configuration
        .def("calibrate_sensor", &ObservingConditions::calibrateSensor, py::arg("sensor_type"),
             "Calibrate specific sensor")
        .def("set_sensor_offset", &ObservingConditions::setSensorOffset, 
             py::arg("sensor_type"), py::arg("offset"),
             "Set sensor calibration offset")
        .def("get_sensor_offset", &ObservingConditions::getSensorOffset, py::arg("sensor_type"),
             "Get sensor calibration offset")
        .def("set_update_interval", &ObservingConditions::setUpdateInterval, py::arg("interval_seconds"),
             "Set sensor update interval")
        .def("get_update_interval", &ObservingConditions::getUpdateInterval, "Get sensor update interval")
        
        // Location and site information
        .def("set_site_location", &ObservingConditions::setSiteLocation, 
             py::arg("latitude"), py::arg("longitude"), py::arg("elevation"),
             "Set observing site location")
        .def("get_site_latitude", &ObservingConditions::getSiteLatitude, "Get site latitude")
        .def("get_site_longitude", &ObservingConditions::getSiteLongitude, "Get site longitude")
        .def("get_site_elevation", &ObservingConditions::getSiteElevation, "Get site elevation")
        
        // Event callbacks
        .def("set_weather_change_callback", [](ObservingConditions& oc, py::function callback) {
            oc.setWeatherChangeCallback([callback](ObservingConditions::WeatherCondition condition) {
                py::gil_scoped_acquire acquire;
                try {
                    callback(condition);
                } catch (const py::error_already_set& e) {
                    SPDLOG_ERROR("Python error in weather change callback: {}", e.what());
                }
            });
        }, py::arg("callback"), "Set weather condition change callback")
        
        .def("set_safety_alert_callback", [](ObservingConditions& oc, py::function callback) {
            oc.setSafetyAlertCallback([callback](ObservingConditions::SensorType sensor, double value, bool is_safe) {
                py::gil_scoped_acquire acquire;
                try {
                    callback(sensor, value, is_safe);
                } catch (const py::error_already_set& e) {
                    SPDLOG_ERROR("Python error in safety alert callback: {}", e.what());
                }
            });
        }, py::arg("callback"), "Set safety alert callback");
    
    // Convenience functions for creating compatible observing conditions
    m.def("create_compatible_observing_conditions", [](const std::string& deviceId, const std::string& manufacturer, const std::string& model) {
        return hydrogen::enhanced::createCompatibleObservingConditions(deviceId, manufacturer, model);
    }, py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "WeatherStation",
    "Create observing conditions with automatic ASCOM/INDI compatibility");
    
    // Enable compatibility for existing observing conditions
    m.def("enable_observing_conditions_compatibility", [](std::shared_ptr<ObservingConditions> oc, const std::string& deviceId) {
        return compatibility::enableAutomaticCompatibility(oc, deviceId, true, true);
    }, py::arg("observing_conditions"), py::arg("device_id"),
    "Enable automatic ASCOM/INDI compatibility for existing observing conditions");
}

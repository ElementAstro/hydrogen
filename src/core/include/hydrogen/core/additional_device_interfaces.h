#pragma once

#include "device_interface.h"
#include <vector>
#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Filter wheel device interface
 */
class IFilterWheel : public IDevice {
public:
    virtual ~IFilterWheel() = default;
    
    /**
     * @brief Get number of filter positions
     * @return Number of filter positions
     */
    virtual int getFilterCount() const = 0;
    
    /**
     * @brief Get current filter position (1-based)
     * @return Current filter position
     */
    virtual int getCurrentPosition() const = 0;
    
    /**
     * @brief Set filter position
     * @param position Target position (1-based)
     * @return True if successful
     */
    virtual bool setPosition(int position) = 0;
    
    /**
     * @brief Get filter names
     * @return Vector of filter names
     */
    virtual std::vector<std::string> getFilterNames() const = 0;
    
    /**
     * @brief Set filter name for position
     * @param position Filter position (1-based)
     * @param name Filter name
     * @return True if successful
     */
    virtual bool setFilterName(int position, const std::string& name) = 0;
    
    /**
     * @brief Get filter by name
     * @param name Filter name
     * @return Filter position (1-based), 0 if not found
     */
    virtual int getFilterByName(const std::string& name) const = 0;
    
    /**
     * @brief Check if filter wheel is moving
     * @return True if moving
     */
    virtual bool isMoving() const = 0;
    
    /**
     * @brief Get estimated move time to position
     * @param position Target position
     * @return Estimated time in milliseconds
     */
    virtual std::chrono::milliseconds getEstimatedMoveTime(int position) const = 0;
    
    /**
     * @brief Calibrate filter wheel
     * @return True if calibration successful
     */
    virtual bool calibrate() = 0;
    
    /**
     * @brief Get filter wheel temperature (if available)
     * @return Temperature in Celsius, NaN if not available
     */
    virtual double getTemperature() const = 0;
};

/**
 * @brief Weather station device interface
 */
class IWeatherStation : public IDevice {
public:
    virtual ~IWeatherStation() = default;
    
    /**
     * @brief Weather data structure
     */
    struct WeatherData {
        double temperature = NAN;        // Celsius
        double humidity = NAN;           // Percentage
        double pressure = NAN;           // hPa
        double windSpeed = NAN;          // m/s
        double windDirection = NAN;      // Degrees
        double dewPoint = NAN;           // Celsius
        double cloudCover = NAN;         // Percentage
        double skyTemperature = NAN;     // Celsius
        double rainRate = NAN;           // mm/hour
        bool isSafe = false;             // Safe for observation
        std::chrono::system_clock::time_point timestamp;
        
        json toJson() const;
        static WeatherData fromJson(const json& j);
    };
    
    /**
     * @brief Get current weather data
     * @return Current weather readings
     */
    virtual WeatherData getCurrentWeather() const = 0;
    
    /**
     * @brief Get weather history
     * @param hours Number of hours of history to retrieve
     * @return Vector of weather data points
     */
    virtual std::vector<WeatherData> getWeatherHistory(int hours = 24) const = 0;
    
    /**
     * @brief Check if conditions are safe for observation
     * @return True if safe
     */
    virtual bool isSafeForObservation() const = 0;
    
    /**
     * @brief Get safety limits
     * @return JSON object with safety thresholds
     */
    virtual json getSafetyLimits() const = 0;
    
    /**
     * @brief Set safety limits
     * @param limits JSON object with safety thresholds
     * @return True if successful
     */
    virtual bool setSafetyLimits(const json& limits) = 0;
    
    /**
     * @brief Register callback for weather updates
     * @param callback Function to call when weather data updates
     */
    virtual void setWeatherUpdateCallback(std::function<void(const WeatherData&)> callback) = 0;
    
    /**
     * @brief Register callback for safety status changes
     * @param callback Function to call when safety status changes
     */
    virtual void setSafetyStatusCallback(std::function<void(bool)> callback) = 0;
    
    /**
     * @brief Get supported sensors
     * @return Vector of supported sensor names
     */
    virtual std::vector<std::string> getSupportedSensors() const = 0;
    
    /**
     * @brief Check if specific sensor is available
     * @param sensorName Name of the sensor
     * @return True if sensor is available
     */
    virtual bool isSensorAvailable(const std::string& sensorName) const = 0;
};

/**
 * @brief Dome controller device interface
 */
class IDomeController : public IDevice {
public:
    virtual ~IDomeController() = default;
    
    /**
     * @brief Dome shutter state
     */
    enum class ShutterState {
        CLOSED,
        OPENING,
        OPEN,
        CLOSING,
        ERROR,
        UNKNOWN
    };
    
    /**
     * @brief Get current dome azimuth
     * @return Azimuth in degrees (0-360)
     */
    virtual double getAzimuth() const = 0;
    
    /**
     * @brief Set dome azimuth
     * @param azimuth Target azimuth in degrees
     * @return True if successful
     */
    virtual bool setAzimuth(double azimuth) = 0;
    
    /**
     * @brief Check if dome is moving
     * @return True if moving
     */
    virtual bool isMoving() const = 0;
    
    /**
     * @brief Stop dome movement
     * @return True if successful
     */
    virtual bool stop() = 0;
    
    /**
     * @brief Get shutter state
     * @return Current shutter state
     */
    virtual ShutterState getShutterState() const = 0;
    
    /**
     * @brief Open shutter
     * @return True if successful
     */
    virtual bool openShutter() = 0;
    
    /**
     * @brief Close shutter
     * @return True if successful
     */
    virtual bool closeShutter() = 0;
    
    /**
     * @brief Check if dome can slave to telescope
     * @return True if slaving is supported
     */
    virtual bool canSlave() const = 0;
    
    /**
     * @brief Enable/disable dome slaving to telescope
     * @param enabled Whether to enable slaving
     * @return True if successful
     */
    virtual bool setSlaving(bool enabled) = 0;
    
    /**
     * @brief Check if dome is slaved to telescope
     * @return True if slaved
     */
    virtual bool isSlaved() const = 0;
    
    /**
     * @brief Sync dome to telescope position
     * @param telescopeAzimuth Telescope azimuth in degrees
     * @return True if successful
     */
    virtual bool syncToTelescope(double telescopeAzimuth) = 0;
    
    /**
     * @brief Find home position
     * @return True if successful
     */
    virtual bool findHome() = 0;
    
    /**
     * @brief Check if dome is at home position
     * @return True if at home
     */
    virtual bool isAtHome() const = 0;
    
    /**
     * @brief Park dome
     * @return True if successful
     */
    virtual bool park() = 0;
    
    /**
     * @brief Check if dome is parked
     * @return True if parked
     */
    virtual bool isParked() const = 0;
};

/**
 * @brief Flat panel device interface for calibration
 */
class IFlatPanel : public IDevice {
public:
    virtual ~IFlatPanel() = default;
    
    /**
     * @brief Light source types
     */
    enum class LightSource {
        LED_WHITE,
        LED_RED,
        LED_GREEN,
        LED_BLUE,
        HALOGEN,
        ELECTROLUMINESCENT,
        CUSTOM
    };
    
    /**
     * @brief Get maximum brightness level
     * @return Maximum brightness (0-100)
     */
    virtual int getMaxBrightness() const = 0;
    
    /**
     * @brief Get current brightness level
     * @return Current brightness (0-100)
     */
    virtual int getBrightness() const = 0;
    
    /**
     * @brief Set brightness level
     * @param brightness Target brightness (0-100)
     * @return True if successful
     */
    virtual bool setBrightness(int brightness) = 0;
    
    /**
     * @brief Turn light on
     * @return True if successful
     */
    virtual bool turnOn() = 0;
    
    /**
     * @brief Turn light off
     * @return True if successful
     */
    virtual bool turnOff() = 0;
    
    /**
     * @brief Check if light is on
     * @return True if light is on
     */
    virtual bool isLightOn() const = 0;
    
    /**
     * @brief Get supported light sources
     * @return Vector of supported light sources
     */
    virtual std::vector<LightSource> getSupportedLightSources() const = 0;
    
    /**
     * @brief Get current light source
     * @return Current light source
     */
    virtual LightSource getCurrentLightSource() const = 0;
    
    /**
     * @brief Set light source
     * @param source Target light source
     * @return True if successful
     */
    virtual bool setLightSource(LightSource source) = 0;
    
    /**
     * @brief Get panel temperature (if available)
     * @return Temperature in Celsius, NaN if not available
     */
    virtual double getTemperature() const = 0;
    
    /**
     * @brief Check if panel supports covers/dust protection
     * @return True if covers are supported
     */
    virtual bool supportsCover() const = 0;
    
    /**
     * @brief Open cover (if supported)
     * @return True if successful
     */
    virtual bool openCover() = 0;
    
    /**
     * @brief Close cover (if supported)
     * @return True if successful
     */
    virtual bool closeCover() = 0;
    
    /**
     * @brief Check if cover is open
     * @return True if cover is open
     */
    virtual bool isCoverOpen() const = 0;
    
    /**
     * @brief Calibrate panel brightness
     * @return True if calibration successful
     */
    virtual bool calibrateBrightness() = 0;
    
    /**
     * @brief Get calibration data
     * @return JSON object with calibration information
     */
    virtual json getCalibrationData() const = 0;
};

/**
 * @brief Helper functions for additional device interfaces
 */
std::string shutterStateToString(IDomeController::ShutterState state);
IDomeController::ShutterState stringToShutterState(const std::string& state);
std::string lightSourceToString(IFlatPanel::LightSource source);
IFlatPanel::LightSource stringToLightSource(const std::string& source);

} // namespace core
} // namespace hydrogen

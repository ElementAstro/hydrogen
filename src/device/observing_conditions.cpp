#include "observing_conditions.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace astrocomm {
namespace device {

ObservingConditions::ObservingConditions(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "OBSERVING_CONDITIONS", manufacturer, model)
    , averagePeriod_(10.0)  // 10 minutes default
    , updateInterval_(30)   // 30 seconds default
    , averagingWindow_(20)  // 20 readings default
    , dataLoggingEnabled_(false)
    , sensorUpdateRunning_(false)
    , dataLoggingRunning_(false)
    , safetyMonitorRunning_(false)
    , safeToObserve_(true) {
    
    // Initialize sensor data
    sensors_["CloudCover"] = SensorData();
    sensors_["DewPoint"] = SensorData();
    sensors_["Humidity"] = SensorData();
    sensors_["Pressure"] = SensorData();
    sensors_["RainRate"] = SensorData();
    sensors_["SkyBrightness"] = SensorData();
    sensors_["SkyQuality"] = SensorData();
    sensors_["SkyTemperature"] = SensorData();
    sensors_["StarFWHM"] = SensorData();
    sensors_["Temperature"] = SensorData();
    sensors_["WindDirection"] = SensorData();
    sensors_["WindGust"] = SensorData();
    sensors_["WindSpeed"] = SensorData();
    
    initializeSensorDescriptions();
    initializeManufacturerSpecific();
}

ObservingConditions::~ObservingConditions() {
    stopDevice();
}

bool ObservingConditions::initializeDevice() {
    SPDLOG_INFO("Initializing observing conditions device {}", getDeviceId());
    
    // Initialize properties
    setProperty("averagePeriod", averagePeriod_.load());
    setProperty("updateInterval", updateInterval_);
    setProperty("averagingWindow", averagingWindow_);
    setProperty("dataLoggingEnabled", dataLoggingEnabled_);
    setProperty("safeToObserve", safeToObserve_.load());
    
    // Initialize sensor capabilities
    for (const auto& [name, data] : sensors_) {
        sensorCapabilities_[name] = data.enabled;
        setProperty("sensor_" + name + "_enabled", data.enabled);
        setProperty("sensor_" + name + "_description", data.description);
    }
    
    // Set default safety limits
    safetyLimits_ = json{
        {"maxWindSpeed", 50.0},      // km/h
        {"maxHumidity", 85.0},       // %
        {"maxRainRate", 0.1},        // mm/h
        {"minTemperature", -20.0},   // °C
        {"maxTemperature", 50.0},    // °C
        {"maxCloudCover", 80.0}      // %
    };
    
    // Set default alert thresholds
    alertThresholds_ = json{
        {"windSpeedWarning", 30.0},
        {"humidityWarning", 75.0},
        {"rainRateWarning", 0.05},
        {"cloudCoverWarning", 60.0}
    };
    
    return true;
}

bool ObservingConditions::startDevice() {
    SPDLOG_INFO("Starting observing conditions device {}", getDeviceId());
    
    // Start sensor update thread
    sensorUpdateRunning_ = true;
    sensorThread_ = std::thread(&ObservingConditions::sensorUpdateThread, this);
    
    // Start safety monitoring thread
    safetyMonitorRunning_ = true;
    safetyThread_ = std::thread(&ObservingConditions::safetyMonitorThread, this);
    
    // Start data logging thread if enabled
    if (dataLoggingEnabled_) {
        dataLoggingRunning_ = true;
        loggingThread_ = std::thread(&ObservingConditions::dataLoggingThread, this);
    }
    
    return true;
}

void ObservingConditions::stopDevice() {
    SPDLOG_INFO("Stopping observing conditions device {}", getDeviceId());
    
    // Stop all threads
    sensorUpdateRunning_ = false;
    dataLoggingRunning_ = false;
    safetyMonitorRunning_ = false;
    
    sensorCV_.notify_all();
    loggingCV_.notify_all();
    safetyCV_.notify_all();
    
    if (sensorThread_.joinable()) {
        sensorThread_.join();
    }
    if (loggingThread_.joinable()) {
        loggingThread_.join();
    }
    if (safetyThread_.joinable()) {
        safetyThread_.join();
    }
}

// IObservingConditions interface implementation
double ObservingConditions::getCloudCover() const {
    return sensors_.at("CloudCover").averageValue.load();
}

double ObservingConditions::getDewPoint() const {
    return sensors_.at("DewPoint").averageValue.load();
}

double ObservingConditions::getHumidity() const {
    return sensors_.at("Humidity").averageValue.load();
}

double ObservingConditions::getPressure() const {
    return sensors_.at("Pressure").averageValue.load();
}

double ObservingConditions::getRainRate() const {
    return sensors_.at("RainRate").averageValue.load();
}

double ObservingConditions::getSkyBrightness() const {
    return sensors_.at("SkyBrightness").averageValue.load();
}

double ObservingConditions::getSkyQuality() const {
    return sensors_.at("SkyQuality").averageValue.load();
}

double ObservingConditions::getSkyTemperature() const {
    return sensors_.at("SkyTemperature").averageValue.load();
}

double ObservingConditions::getStarFWHM() const {
    return sensors_.at("StarFWHM").averageValue.load();
}

double ObservingConditions::getTemperature() const {
    return sensors_.at("Temperature").averageValue.load();
}

double ObservingConditions::getWindDirection() const {
    return sensors_.at("WindDirection").averageValue.load();
}

double ObservingConditions::getWindGust() const {
    return sensors_.at("WindGust").averageValue.load();
}

double ObservingConditions::getWindSpeed() const {
    return sensors_.at("WindSpeed").averageValue.load();
}

double ObservingConditions::getAveragePeriod() const {
    return averagePeriod_.load();
}

void ObservingConditions::setAveragePeriod(double value) {
    if (value > 0.0) {
        averagePeriod_ = value;
        setProperty("averagePeriod", value);
        SPDLOG_DEBUG("Observing conditions {} average period set to {:.1f} minutes", getDeviceId(), value);
    }
}

void ObservingConditions::refresh() {
    SPDLOG_DEBUG("Observing conditions {} refreshing sensor readings", getDeviceId());
    updateSensorReadings();
    updateAverages();
}

std::string ObservingConditions::sensorDescription(const std::string& propertyName) const {
    auto it = sensors_.find(propertyName);
    if (it != sensors_.end()) {
        return it->second.description;
    }
    return "Unknown sensor";
}

double ObservingConditions::timeSinceLastUpdate(const std::string& propertyName) const {
    auto it = sensors_.find(propertyName);
    if (it != sensors_.end()) {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.lastUpdate);
        return duration.count();
    }
    return -1.0;  // Invalid property
}

// Additional methods
void ObservingConditions::setSensorEnabled(const std::string& sensorName, bool enabled) {
    auto it = sensors_.find(sensorName);
    if (it != sensors_.end()) {
        it->second.enabled = enabled;
        sensorCapabilities_[sensorName] = enabled;
        setProperty("sensor_" + sensorName + "_enabled", enabled);
        SPDLOG_DEBUG("Observing conditions {} sensor {} {}", getDeviceId(), sensorName, enabled ? "enabled" : "disabled");
    }
}

bool ObservingConditions::isSensorEnabled(const std::string& sensorName) const {
    auto it = sensors_.find(sensorName);
    return (it != sensors_.end()) ? it->second.enabled : false;
}

void ObservingConditions::setSensorCalibration(const std::string& sensorName, double offset, double scale) {
    auto it = sensors_.find(sensorName);
    if (it != sensors_.end()) {
        it->second.calibrationOffset = offset;
        it->second.calibrationScale = scale;
        setProperty("sensor_" + sensorName + "_offset", offset);
        setProperty("sensor_" + sensorName + "_scale", scale);
        SPDLOG_DEBUG("Observing conditions {} sensor {} calibration: offset={:.3f}, scale={:.3f}", 
                    getDeviceId(), sensorName, offset, scale);
    }
}

void ObservingConditions::setUpdateInterval(int intervalSeconds) {
    if (intervalSeconds > 0) {
        updateInterval_ = intervalSeconds;
        setProperty("updateInterval", intervalSeconds);
        sensorCV_.notify_one();
    }
}

void ObservingConditions::setAveragingWindow(int windowSize) {
    if (windowSize > 0) {
        averagingWindow_ = windowSize;
        setProperty("averagingWindow", windowSize);
    }
}

void ObservingConditions::setSafetyLimits(const json& limits) {
    safetyLimits_ = limits;
    setProperty("safetyLimits", limits);
    SPDLOG_INFO("Observing conditions {} safety limits updated", getDeviceId());
}

json ObservingConditions::getSafetyLimits() const {
    return safetyLimits_;
}

bool ObservingConditions::isSafeToObserve() const {
    return safeToObserve_.load();
}

std::vector<std::string> ObservingConditions::getActiveAlerts() const {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    return activeAlerts_;
}

void ObservingConditions::setAlertThresholds(const json& thresholds) {
    alertThresholds_ = thresholds;
    setProperty("alertThresholds", thresholds);
}

void ObservingConditions::enableDataLogging(bool enabled) {
    dataLoggingEnabled_ = enabled;
    setProperty("dataLoggingEnabled", enabled);
    
    if (enabled && !dataLoggingRunning_.load()) {
        dataLoggingRunning_ = true;
        if (loggingThread_.joinable()) {
            loggingThread_.join();
        }
        loggingThread_ = std::thread(&ObservingConditions::dataLoggingThread, this);
    } else if (!enabled && dataLoggingRunning_.load()) {
        dataLoggingRunning_ = false;
        loggingCV_.notify_one();
    }
}

json ObservingConditions::getHistoricalData(const std::string& property, int hours) const {
    auto it = sensors_.find(property);
    if (it == sensors_.end()) {
        return json::array();
    }

    json data = json::array();
    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(hours);

    for (const auto& [timestamp, value] : it->second.history) {
        if (timestamp >= cutoff) {
            data.push_back({
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count()},
                {"value", value}
            });
        }
    }

    return data;
}

void ObservingConditions::clearHistoricalData() {
    for (auto& [name, sensor] : sensors_) {
        sensor.history.clear();
    }
    SPDLOG_INFO("Observing conditions {} historical data cleared", getDeviceId());
}

// Sensor reading methods (simulation)
double ObservingConditions::readCloudCover() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 100.0);
    return dis(gen);
}

double ObservingConditions::readDewPoint() {
    double temp = readTemperature();
    double humidity = readHumidity();
    // Simplified dew point calculation
    return temp - ((100.0 - humidity) / 5.0);
}

double ObservingConditions::readHumidity() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(30.0, 90.0);
    return dis(gen);
}

double ObservingConditions::readPressure() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(980.0, 1030.0);
    return dis(gen);
}

double ObservingConditions::readRainRate() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 2.0);
    return dis(gen);
}

double ObservingConditions::readSkyBrightness() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(15.0, 22.0);
    return dis(gen);
}

double ObservingConditions::readSkyQuality() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(15.0, 22.0);
    return dis(gen);
}

double ObservingConditions::readSkyTemperature() {
    double ambient = readTemperature();
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(-20.0, -5.0);
    return ambient + dis(gen);
}

double ObservingConditions::readStarFWHM() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(1.5, 4.0);
    return dis(gen);
}

double ObservingConditions::readTemperature() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(-10.0, 35.0);
    return dis(gen);
}

double ObservingConditions::readWindDirection() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 360.0);
    return dis(gen);
}

double ObservingConditions::readWindGust() {
    double baseSpeed = readWindSpeed();
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(1.0, 2.5);
    return baseSpeed * dis(gen);
}

double ObservingConditions::readWindSpeed() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 25.0);
    return dis(gen);
}

// Control threads
void ObservingConditions::sensorUpdateThread() {
    while (sensorUpdateRunning_.load()) {
        updateSensorReadings();
        updateAverages();

        std::unique_lock<std::mutex> lock(sensorMutex_);
        sensorCV_.wait_for(lock, std::chrono::seconds(updateInterval_),
                          [this] { return !sensorUpdateRunning_.load(); });
    }
}

void ObservingConditions::dataLoggingThread() {
    while (dataLoggingRunning_.load()) {
        // Log current sensor readings
        for (const auto& [name, sensor] : sensors_) {
            if (sensor.enabled) {
                SPDLOG_DEBUG("Sensor {}: {:.2f}", name, sensor.currentValue.load());
            }
        }

        std::unique_lock<std::mutex> lock(loggingMutex_);
        loggingCV_.wait_for(lock, std::chrono::minutes(1),
                           [this] { return !dataLoggingRunning_.load(); });
    }
}

void ObservingConditions::safetyMonitorThread() {
    while (safetyMonitorRunning_.load()) {
        checkSafetyConditions();
        processAlerts();

        std::unique_lock<std::mutex> lock(safetyMutex_);
        safetyCV_.wait_for(lock, std::chrono::seconds(10),
                          [this] { return !safetyMonitorRunning_.load(); });
    }
}

// Utility methods
void ObservingConditions::updateSensorReadings() {
    auto now = std::chrono::system_clock::now();

    for (auto& [name, sensor] : sensors_) {
        if (!sensor.enabled) continue;

        double rawValue = 0.0;

        // Read sensor based on name
        if (name == "CloudCover") rawValue = readCloudCover();
        else if (name == "DewPoint") rawValue = readDewPoint();
        else if (name == "Humidity") rawValue = readHumidity();
        else if (name == "Pressure") rawValue = readPressure();
        else if (name == "RainRate") rawValue = readRainRate();
        else if (name == "SkyBrightness") rawValue = readSkyBrightness();
        else if (name == "SkyQuality") rawValue = readSkyQuality();
        else if (name == "SkyTemperature") rawValue = readSkyTemperature();
        else if (name == "StarFWHM") rawValue = readStarFWHM();
        else if (name == "Temperature") rawValue = readTemperature();
        else if (name == "WindDirection") rawValue = readWindDirection();
        else if (name == "WindGust") rawValue = readWindGust();
        else if (name == "WindSpeed") rawValue = readWindSpeed();

        // Apply calibration
        double calibratedValue = (rawValue + sensor.calibrationOffset) * sensor.calibrationScale;

        if (isValidReading(calibratedValue)) {
            sensor.currentValue = calibratedValue;
            sensor.lastUpdate = now;
            addToHistory(name, calibratedValue);
        }
    }
}

void ObservingConditions::updateAverages() {
    for (auto& [name, sensor] : sensors_) {
        if (!sensor.enabled || sensor.history.empty()) continue;

        double average = calculateAverage(name);
        sensor.averageValue = average;
        setProperty(name, average);
    }
}

void ObservingConditions::checkSafetyConditions() {
    bool safe = true;
    std::vector<std::string> newAlerts;

    // Check wind speed
    double windSpeed = getWindSpeed();
    if (safetyLimits_.contains("maxWindSpeed") && windSpeed > safetyLimits_["maxWindSpeed"]) {
        safe = false;
        newAlerts.push_back("Wind speed too high: " + std::to_string(windSpeed) + " km/h");
    }

    // Check humidity
    double humidity = getHumidity();
    if (safetyLimits_.contains("maxHumidity") && humidity > safetyLimits_["maxHumidity"]) {
        safe = false;
        newAlerts.push_back("Humidity too high: " + std::to_string(humidity) + "%");
    }

    // Check rain rate
    double rainRate = getRainRate();
    if (safetyLimits_.contains("maxRainRate") && rainRate > safetyLimits_["maxRainRate"]) {
        safe = false;
        newAlerts.push_back("Rain detected: " + std::to_string(rainRate) + " mm/h");
    }

    // Check cloud cover
    double cloudCover = getCloudCover();
    if (safetyLimits_.contains("maxCloudCover") && cloudCover > safetyLimits_["maxCloudCover"]) {
        safe = false;
        newAlerts.push_back("Cloud cover too high: " + std::to_string(cloudCover) + "%");
    }

    // Check temperature limits
    double temperature = getTemperature();
    if (safetyLimits_.contains("minTemperature") && temperature < safetyLimits_["minTemperature"]) {
        safe = false;
        newAlerts.push_back("Temperature too low: " + std::to_string(temperature) + "°C");
    }
    if (safetyLimits_.contains("maxTemperature") && temperature > safetyLimits_["maxTemperature"]) {
        safe = false;
        newAlerts.push_back("Temperature too high: " + std::to_string(temperature) + "°C");
    }

    safeToObserve_ = safe;
    setProperty("safeToObserve", safe);

    {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        activeAlerts_ = newAlerts;
    }
}

void ObservingConditions::processAlerts() {
    // Process warning thresholds
    std::vector<std::string> warnings;

    if (alertThresholds_.contains("windSpeedWarning") &&
        getWindSpeed() > alertThresholds_["windSpeedWarning"]) {
        warnings.push_back("Wind speed warning");
    }

    if (alertThresholds_.contains("humidityWarning") &&
        getHumidity() > alertThresholds_["humidityWarning"]) {
        warnings.push_back("Humidity warning");
    }

    if (alertThresholds_.contains("rainRateWarning") &&
        getRainRate() > alertThresholds_["rainRateWarning"]) {
        warnings.push_back("Rain rate warning");
    }

    if (alertThresholds_.contains("cloudCoverWarning") &&
        getCloudCover() > alertThresholds_["cloudCoverWarning"]) {
        warnings.push_back("Cloud cover warning");
    }

    if (!warnings.empty()) {
        for (const auto& warning : warnings) {
            SPDLOG_WARN("Observing conditions {}: {}", getDeviceId(), warning);
        }
    }
}

double ObservingConditions::calculateAverage(const std::string& property) const {
    auto it = sensors_.find(property);
    if (it == sensors_.end() || it->second.history.empty()) {
        return 0.0;
    }

    const auto& history = it->second.history;
    int count = std::min(averagingWindow_, static_cast<int>(history.size()));

    if (count == 0) return 0.0;

    double sum = 0.0;
    for (int i = history.size() - count; i < static_cast<int>(history.size()); ++i) {
        sum += history[i].second;
    }

    return sum / count;
}

void ObservingConditions::addToHistory(const std::string& property, double value) {
    auto it = sensors_.find(property);
    if (it == sensors_.end()) return;

    auto now = std::chrono::system_clock::now();
    it->second.history.emplace_back(now, value);

    // Keep only recent history (24 hours)
    auto cutoff = now - std::chrono::hours(24);
    auto& history = it->second.history;
    history.erase(
        std::remove_if(history.begin(), history.end(),
                      [cutoff](const auto& entry) { return entry.first < cutoff; }),
        history.end()
    );
}

bool ObservingConditions::isValidReading(double value) const {
    return std::isfinite(value) && !std::isnan(value);
}

// IStateful interface implementation
bool ObservingConditions::setProperty(const std::string& property, const json& value) {
    if (property == "averagePeriod") {
        setAveragePeriod(value.get<double>());
        return true;
    } else if (property == "updateInterval") {
        setUpdateInterval(value.get<int>());
        return true;
    } else if (property == "averagingWindow") {
        setAveragingWindow(value.get<int>());
        return true;
    } else if (property == "dataLoggingEnabled") {
        enableDataLogging(value.get<bool>());
        return true;
    } else if (property == "safetyLimits") {
        setSafetyLimits(value);
        return true;
    } else if (property == "alertThresholds") {
        setAlertThresholds(value);
        return true;
    }

    return ModernDeviceBase::setProperty(property, value);
}

json ObservingConditions::getProperty(const std::string& property) const {
    if (property == "CloudCover") return getCloudCover();
    else if (property == "DewPoint") return getDewPoint();
    else if (property == "Humidity") return getHumidity();
    else if (property == "Pressure") return getPressure();
    else if (property == "RainRate") return getRainRate();
    else if (property == "SkyBrightness") return getSkyBrightness();
    else if (property == "SkyQuality") return getSkyQuality();
    else if (property == "SkyTemperature") return getSkyTemperature();
    else if (property == "StarFWHM") return getStarFWHM();
    else if (property == "Temperature") return getTemperature();
    else if (property == "WindDirection") return getWindDirection();
    else if (property == "WindGust") return getWindGust();
    else if (property == "WindSpeed") return getWindSpeed();
    else if (property == "averagePeriod") return averagePeriod_.load();
    else if (property == "safeToObserve") return safeToObserve_.load();

    return ModernDeviceBase::getProperty(property);
}

json ObservingConditions::getAllProperties() const {
    json properties = ModernDeviceBase::getAllProperties();

    properties["CloudCover"] = getCloudCover();
    properties["DewPoint"] = getDewPoint();
    properties["Humidity"] = getHumidity();
    properties["Pressure"] = getPressure();
    properties["RainRate"] = getRainRate();
    properties["SkyBrightness"] = getSkyBrightness();
    properties["SkyQuality"] = getSkyQuality();
    properties["SkyTemperature"] = getSkyTemperature();
    properties["StarFWHM"] = getStarFWHM();
    properties["Temperature"] = getTemperature();
    properties["WindDirection"] = getWindDirection();
    properties["WindGust"] = getWindGust();
    properties["WindSpeed"] = getWindSpeed();
    properties["averagePeriod"] = averagePeriod_.load();
    properties["safeToObserve"] = safeToObserve_.load();
    properties["activeAlerts"] = getActiveAlerts();

    return properties;
}

std::vector<std::string> ObservingConditions::getCapabilities() const {
    std::vector<std::string> capabilities;

    for (const auto& [name, enabled] : sensorCapabilities_) {
        if (enabled) {
            capabilities.push_back(name + "_SENSOR");
        }
    }

    capabilities.push_back("SAFETY_MONITORING");
    capabilities.push_back("DATA_LOGGING");
    capabilities.push_back("HISTORICAL_DATA");
    capabilities.push_back("ALERT_SYSTEM");

    return capabilities;
}

// ModernDeviceBase overrides
bool ObservingConditions::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "REFRESH") {
        refresh();
        result["success"] = true;
        return true;
    } else if (command == "SET_SENSOR_ENABLED") {
        std::string sensorName = parameters.at("sensor").get<std::string>();
        bool enabled = parameters.at("enabled").get<bool>();
        setSensorEnabled(sensorName, enabled);
        result["success"] = true;
        return true;
    } else if (command == "SET_SENSOR_CALIBRATION") {
        std::string sensorName = parameters.at("sensor").get<std::string>();
        double offset = parameters.value("offset", 0.0);
        double scale = parameters.value("scale", 1.0);
        setSensorCalibration(sensorName, offset, scale);
        result["success"] = true;
        return true;
    } else if (command == "GET_HISTORICAL_DATA") {
        std::string property = parameters.at("property").get<std::string>();
        int hours = parameters.value("hours", 24);
        result["data"] = getHistoricalData(property, hours);
        result["success"] = true;
        return true;
    } else if (command == "CLEAR_HISTORICAL_DATA") {
        clearHistoricalData();
        result["success"] = true;
        return true;
    }

    return false;
}

void ObservingConditions::updateDevice() {
    // Update sensor readings periodically
    static auto lastUpdate = std::chrono::system_clock::now();
    auto now = std::chrono::system_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count() >= updateInterval_) {
        updateSensorReadings();
        updateAverages();
        lastUpdate = now;
    }
}

// Manufacturer-specific initialization
void ObservingConditions::initializeManufacturerSpecific() {
    std::string manufacturer = getProperty("manufacturer").get<std::string>();

    if (manufacturer == "Generic") {
        initializeGeneric();
    } else if (manufacturer == "Boltwood") {
        initializeBoltwood();
    } else if (manufacturer == "Davis") {
        initializeDavis();
    } else if (manufacturer == "AAG") {
        initializeAAG();
    } else if (manufacturer == "Lunatico") {
        initializeLunatico();
    } else if (manufacturer == "PrimaLuceLab") {
        initializePrimaLuceLab();
    } else if (manufacturer == "Vaisala") {
        initializeVaisala();
    } else {
        initializeGeneric();
    }
}

void ObservingConditions::initializeGeneric() {
    updateInterval_ = 30;
    averagingWindow_ = 20;
    // All sensors enabled by default
}

void ObservingConditions::initializeBoltwood() {
    updateInterval_ = 60;
    averagingWindow_ = 10;
    // Boltwood Cloud Sensor specific settings
    sensors_["SkyBrightness"].enabled = false;  // Not available
    sensors_["StarFWHM"].enabled = false;       // Not available
}

void ObservingConditions::initializeDavis() {
    updateInterval_ = 15;
    averagingWindow_ = 30;
    // Davis weather station specific settings
    sensors_["SkyBrightness"].enabled = false;
    sensors_["SkyQuality"].enabled = false;
    sensors_["StarFWHM"].enabled = false;
}

void ObservingConditions::initializeAAG() {
    updateInterval_ = 30;
    averagingWindow_ = 20;
    // AAG CloudWatcher specific settings
    sensors_["StarFWHM"].enabled = false;
}

void ObservingConditions::initializeLunatico() {
    updateInterval_ = 20;
    averagingWindow_ = 25;
    // Lunatico AAG specific settings
}

void ObservingConditions::initializePrimaLuceLab() {
    updateInterval_ = 10;
    averagingWindow_ = 40;
    // PrimaLuceLab EAGLE specific settings
}

void ObservingConditions::initializeVaisala() {
    updateInterval_ = 5;
    averagingWindow_ = 60;
    // Vaisala professional weather station settings
}

void ObservingConditions::initializeSensorDescriptions() {
    sensors_["CloudCover"].description = "Cloud cover percentage (0-100%)";
    sensors_["DewPoint"].description = "Dew point temperature (°C)";
    sensors_["Humidity"].description = "Relative humidity (0-100%)";
    sensors_["Pressure"].description = "Atmospheric pressure (hPa)";
    sensors_["RainRate"].description = "Rain rate (mm/h)";
    sensors_["SkyBrightness"].description = "Sky brightness (mag/arcsec²)";
    sensors_["SkyQuality"].description = "Sky quality (mag/arcsec²)";
    sensors_["SkyTemperature"].description = "Sky temperature (°C)";
    sensors_["StarFWHM"].description = "Star FWHM (arcseconds)";
    sensors_["Temperature"].description = "Ambient temperature (°C)";
    sensors_["WindDirection"].description = "Wind direction (degrees)";
    sensors_["WindGust"].description = "Wind gust speed (km/h)";
    sensors_["WindSpeed"].description = "Wind speed (km/h)";
}

// Factory function
std::unique_ptr<ObservingConditions> createModernObservingConditions(const std::string& deviceId,
                                                                     const std::string& manufacturer,
                                                                     const std::string& model) {
    return std::make_unique<ObservingConditions>(deviceId, manufacturer, model);
}

} // namespace device
} // namespace astrocomm

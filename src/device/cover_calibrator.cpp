#include "cover_calibrator.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace astrocomm {
namespace device {

CoverCalibrator::CoverCalibrator(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "COVER_CALIBRATOR", manufacturer, model)
    , coverState_(interfaces::CoverState::UNKNOWN)
    , calibratorState_(interfaces::CalibratorState::UNKNOWN)
    , coverMoving_(false)
    , calibratorChanging_(false)
    , currentBrightness_(0)
    , targetBrightness_(0)
    , calibratorTemperature_(20.0)
    , calibratorPower_(0)
    , coverPresent_(true)
    , calibratorPresent_(true)
    , maxBrightness_(255)
    , coverTimeout_(30)
    , calibratorTimeout_(10)
    , warmupTime_(5)
    , cooldownTime_(3)
    , coverType_("Flip-Flat")
    , calibratorType_("LED")
    , brightnessSteps_({0, 25, 50, 75, 100, 125, 150, 175, 200, 225, 255})
    , coverOperationRunning_(false)
    , calibratorOperationRunning_(false)
    , temperatureMonitorRunning_(false)
    , emergencyStop_(false)
    , overheatingProtection_(false)
    , maxTemperature_(60.0)
    , minTemperature_(-20.0)
    , hasCover_(true)
    , hasCalibrator_(true)
    , hasTemperatureSensor_(true)
    , hasPowerSensor_(true)
    , supportsBrightnessControl_(true)
    , supportsWarmup_(true) {
    
    initializeManufacturerSpecific();
}

CoverCalibrator::~CoverCalibrator() {
    stopDevice();
}

bool CoverCalibrator::initializeDevice() {
    SPDLOG_INFO("Initializing cover calibrator device {}", getDeviceId());
    
    // Initialize cover state
    if (coverPresent_) {
        coverState_ = interfaces::CoverState::CLOSED;
    } else {
        coverState_ = interfaces::CoverState::NOT_PRESENT;
    }
    
    // Initialize calibrator state
    if (calibratorPresent_) {
        calibratorState_ = interfaces::CalibratorState::OFF;
    } else {
        calibratorState_ = interfaces::CalibratorState::NOT_PRESENT;
    }
    
    // Initialize properties
    setProperty("coverState", static_cast<int>(coverState_.load()));
    setProperty("calibratorState", static_cast<int>(calibratorState_.load()));
    setProperty("coverMoving", coverMoving_.load());
    setProperty("calibratorChanging", calibratorChanging_.load());
    setProperty("currentBrightness", currentBrightness_.load());
    setProperty("maxBrightness", maxBrightness_);
    setProperty("calibratorTemperature", calibratorTemperature_.load());
    setProperty("calibratorPower", calibratorPower_.load());
    setProperty("coverPresent", coverPresent_);
    setProperty("calibratorPresent", calibratorPresent_);
    setProperty("coverType", coverType_);
    setProperty("calibratorType", calibratorType_);
    setProperty("warmupTime", warmupTime_);
    setProperty("cooldownTime", cooldownTime_);
    
    // Set capabilities
    setProperty("hasCover", hasCover_);
    setProperty("hasCalibrator", hasCalibrator_);
    setProperty("hasTemperatureSensor", hasTemperatureSensor_);
    setProperty("hasPowerSensor", hasPowerSensor_);
    setProperty("supportsBrightnessControl", supportsBrightnessControl_);
    setProperty("supportsWarmup", supportsWarmup_);
    
    return true;
}

bool CoverCalibrator::startDevice() {
    SPDLOG_INFO("Starting cover calibrator device {}", getDeviceId());
    
    // Start temperature monitoring thread if sensor is present
    if (hasTemperatureSensor_) {
        temperatureMonitorRunning_ = true;
        temperatureThread_ = std::thread(&CoverCalibrator::temperatureMonitorThread, this);
    }
    
    return true;
}

void CoverCalibrator::stopDevice() {
    SPDLOG_INFO("Stopping cover calibrator device {}", getDeviceId());
    
    // Turn off calibrator
    if (calibratorState_.load() != interfaces::CalibratorState::OFF && 
        calibratorState_.load() != interfaces::CalibratorState::NOT_PRESENT) {
        calibratorOff();
    }
    
    // Stop all operations
    coverOperationRunning_ = false;
    calibratorOperationRunning_ = false;
    temperatureMonitorRunning_ = false;
    
    coverCV_.notify_all();
    calibratorCV_.notify_all();
    temperatureCV_.notify_all();
    
    if (coverThread_.joinable()) {
        coverThread_.join();
    }
    if (calibratorThread_.joinable()) {
        calibratorThread_.join();
    }
    if (temperatureThread_.joinable()) {
        temperatureThread_.join();
    }
}

// ICoverCalibrator interface implementation
interfaces::CoverState CoverCalibrator::getCoverState() const {
    return coverState_.load();
}

void CoverCalibrator::openCover() {
    if (!coverPresent_) {
        throw std::runtime_error("Cover is not present");
    }
    
    if (coverState_.load() == interfaces::CoverState::OPEN) {
        return;  // Already open
    }
    
    if (emergencyStop_.load()) {
        throw std::runtime_error("Device is in emergency stop state");
    }
    
    coverState_ = interfaces::CoverState::MOVING;
    coverMoving_ = true;
    setProperty("coverState", static_cast<int>(coverState_.load()));
    setProperty("coverMoving", true);
    
    coverOperationStart_ = std::chrono::system_clock::now();
    
    // Start cover operation thread
    coverOperationRunning_ = true;
    if (coverThread_.joinable()) {
        coverThread_.join();
    }
    coverThread_ = std::thread(&CoverCalibrator::coverControlThread, this);
    
    SPDLOG_INFO("Cover calibrator {} opening cover", getDeviceId());
}

void CoverCalibrator::closeCover() {
    if (!coverPresent_) {
        throw std::runtime_error("Cover is not present");
    }
    
    if (coverState_.load() == interfaces::CoverState::CLOSED) {
        return;  // Already closed
    }
    
    if (emergencyStop_.load()) {
        throw std::runtime_error("Device is in emergency stop state");
    }
    
    coverState_ = interfaces::CoverState::MOVING;
    coverMoving_ = true;
    setProperty("coverState", static_cast<int>(coverState_.load()));
    setProperty("coverMoving", true);
    
    coverOperationStart_ = std::chrono::system_clock::now();
    
    // Start cover operation thread
    coverOperationRunning_ = true;
    if (coverThread_.joinable()) {
        coverThread_.join();
    }
    coverThread_ = std::thread(&CoverCalibrator::coverControlThread, this);
    
    SPDLOG_INFO("Cover calibrator {} closing cover", getDeviceId());
}

void CoverCalibrator::haltCover() {
    if (!coverPresent_) {
        return;
    }
    
    coverOperationRunning_ = false;
    coverMoving_ = false;
    setProperty("coverMoving", false);
    
    executeCoverHalt();
    
    SPDLOG_INFO("Cover calibrator {} cover halted", getDeviceId());
}

bool CoverCalibrator::getCoverMoving() const {
    return coverMoving_.load();
}

interfaces::CalibratorState CoverCalibrator::getCalibratorState() const {
    return calibratorState_.load();
}

int CoverCalibrator::getBrightness() const {
    return currentBrightness_.load();
}

void CoverCalibrator::setBrightness(int value) {
    if (!calibratorPresent_) {
        throw std::runtime_error("Calibrator is not present");
    }
    
    if (!supportsBrightnessControl_) {
        throw std::runtime_error("Brightness control is not supported");
    }
    
    int clampedValue = clampBrightness(value);
    targetBrightness_ = clampedValue;
    
    if (calibratorState_.load() == interfaces::CalibratorState::READY) {
        calibratorOn(clampedValue);
    }
    
    SPDLOG_DEBUG("Cover calibrator {} brightness set to {}", getDeviceId(), clampedValue);
}

int CoverCalibrator::getMaxBrightness() const {
    return maxBrightness_;
}

void CoverCalibrator::calibratorOn(int brightness) {
    if (!calibratorPresent_) {
        throw std::runtime_error("Calibrator is not present");
    }
    
    if (emergencyStop_.load()) {
        throw std::runtime_error("Device is in emergency stop state");
    }
    
    if (overheatingProtection_.load()) {
        throw std::runtime_error("Calibrator is overheating");
    }
    
    int clampedBrightness = clampBrightness(brightness);
    targetBrightness_ = clampedBrightness;
    
    calibratorState_ = interfaces::CalibratorState::NOT_READY;
    calibratorChanging_ = true;
    setProperty("calibratorState", static_cast<int>(calibratorState_.load()));
    setProperty("calibratorChanging", true);
    
    calibratorOperationStart_ = std::chrono::system_clock::now();
    
    // Start calibrator operation thread
    calibratorOperationRunning_ = true;
    if (calibratorThread_.joinable()) {
        calibratorThread_.join();
    }
    calibratorThread_ = std::thread(&CoverCalibrator::calibratorControlThread, this);
    
    SPDLOG_INFO("Cover calibrator {} turning on calibrator at brightness {}", getDeviceId(), clampedBrightness);
}

void CoverCalibrator::calibratorOff() {
    if (!calibratorPresent_) {
        return;
    }
    
    targetBrightness_ = 0;
    
    calibratorState_ = interfaces::CalibratorState::NOT_READY;
    calibratorChanging_ = true;
    setProperty("calibratorState", static_cast<int>(calibratorState_.load()));
    setProperty("calibratorChanging", true);
    
    calibratorOperationStart_ = std::chrono::system_clock::now();
    
    // Start calibrator operation thread
    calibratorOperationRunning_ = true;
    if (calibratorThread_.joinable()) {
        calibratorThread_.join();
    }
    calibratorThread_ = std::thread(&CoverCalibrator::calibratorControlThread, this);
    
    SPDLOG_INFO("Cover calibrator {} turning off calibrator", getDeviceId());
}

bool CoverCalibrator::getCalibratorChanging() const {
    return calibratorChanging_.load();
}

// IStateful interface implementation
bool CoverCalibrator::setProperty(const std::string& property, const json& value) {
    if (property == "maxBrightness") {
        setMaxBrightness(value.get<int>());
        return true;
    } else if (property == "coverTimeout") {
        setCoverTimeout(value.get<int>());
        return true;
    } else if (property == "calibratorTimeout") {
        setCalibratorTimeout(value.get<int>());
        return true;
    } else if (property == "warmupTime") {
        setWarmupTime(value.get<int>());
        return true;
    } else if (property == "cooldownTime") {
        setCooldownTime(value.get<int>());
        return true;
    } else if (property == "coverPresent") {
        setCoverPresent(value.get<bool>());
        return true;
    } else if (property == "calibratorPresent") {
        setCalibratorPresent(value.get<bool>());
        return true;
    } else if (property == "coverType") {
        setCoverType(value.get<std::string>());
        return true;
    } else if (property == "calibratorType") {
        setCalibratorType(value.get<std::string>());
        return true;
    } else if (property == "emergencyStop") {
        emergencyStop_ = value.get<bool>();
        if (emergencyStop_.load()) {
            haltCover();
            calibratorOff();
        }
        return true;
    }

    return ModernDeviceBase::setProperty(property, value);
}

json CoverCalibrator::getProperty(const std::string& property) const {
    if (property == "coverState") {
        return static_cast<int>(coverState_.load());
    } else if (property == "calibratorState") {
        return static_cast<int>(calibratorState_.load());
    } else if (property == "coverMoving") {
        return coverMoving_.load();
    } else if (property == "calibratorChanging") {
        return calibratorChanging_.load();
    } else if (property == "currentBrightness") {
        return currentBrightness_.load();
    } else if (property == "maxBrightness") {
        return maxBrightness_;
    } else if (property == "calibratorTemperature") {
        return calibratorTemperature_.load();
    } else if (property == "calibratorPower") {
        return calibratorPower_.load();
    } else if (property == "coverPresent") {
        return coverPresent_;
    } else if (property == "calibratorPresent") {
        return calibratorPresent_;
    } else if (property == "coverType") {
        return coverType_;
    } else if (property == "calibratorType") {
        return calibratorType_;
    } else if (property == "warmupTime") {
        return warmupTime_;
    } else if (property == "cooldownTime") {
        return cooldownTime_;
    } else if (property == "emergencyStop") {
        return emergencyStop_.load();
    }

    return ModernDeviceBase::getProperty(property);
}

json CoverCalibrator::getAllProperties() const {
    json properties = ModernDeviceBase::getAllProperties();

    properties["coverState"] = static_cast<int>(coverState_.load());
    properties["calibratorState"] = static_cast<int>(calibratorState_.load());
    properties["coverMoving"] = coverMoving_.load();
    properties["calibratorChanging"] = calibratorChanging_.load();
    properties["currentBrightness"] = currentBrightness_.load();
    properties["maxBrightness"] = maxBrightness_;
    properties["calibratorTemperature"] = calibratorTemperature_.load();
    properties["calibratorPower"] = calibratorPower_.load();
    properties["coverPresent"] = coverPresent_;
    properties["calibratorPresent"] = calibratorPresent_;
    properties["coverType"] = coverType_;
    properties["calibratorType"] = calibratorType_;
    properties["warmupTime"] = warmupTime_;
    properties["cooldownTime"] = cooldownTime_;
    properties["emergencyStop"] = emergencyStop_.load();

    return properties;
}

std::vector<std::string> CoverCalibrator::getCapabilities() const {
    std::vector<std::string> capabilities;

    if (hasCover_) {
        capabilities.push_back("COVER_CONTROL");
    }

    if (hasCalibrator_) {
        capabilities.push_back("CALIBRATOR_CONTROL");
    }

    if (supportsBrightnessControl_) {
        capabilities.push_back("BRIGHTNESS_CONTROL");
    }

    if (hasTemperatureSensor_) {
        capabilities.push_back("TEMPERATURE_MONITORING");
    }

    if (hasPowerSensor_) {
        capabilities.push_back("POWER_MONITORING");
    }

    if (supportsWarmup_) {
        capabilities.push_back("WARMUP_CONTROL");
    }

    return capabilities;
}

// Additional methods
void CoverCalibrator::setCoverTimeout(int timeoutSeconds) {
    if (timeoutSeconds > 0) {
        coverTimeout_ = timeoutSeconds;
        setProperty("coverTimeout", timeoutSeconds);
    }
}

void CoverCalibrator::setCalibratorTimeout(int timeoutSeconds) {
    if (timeoutSeconds > 0) {
        calibratorTimeout_ = timeoutSeconds;
        setProperty("calibratorTimeout", timeoutSeconds);
    }
}

void CoverCalibrator::setMaxBrightness(int maxBrightness) {
    if (maxBrightness > 0) {
        maxBrightness_ = maxBrightness;
        setProperty("maxBrightness", maxBrightness);
    }
}

void CoverCalibrator::setBrightnessSteps(const std::vector<int>& steps) {
    brightnessSteps_ = steps;
    std::sort(brightnessSteps_.begin(), brightnessSteps_.end());
}

void CoverCalibrator::setWarmupTime(int warmupSeconds) {
    if (warmupSeconds >= 0) {
        warmupTime_ = warmupSeconds;
        setProperty("warmupTime", warmupSeconds);
    }
}

void CoverCalibrator::setCooldownTime(int cooldownSeconds) {
    if (cooldownSeconds >= 0) {
        cooldownTime_ = cooldownSeconds;
        setProperty("cooldownTime", cooldownSeconds);
    }
}

void CoverCalibrator::setCoverPresent(bool present) {
    coverPresent_ = present;
    hasCover_ = present;

    if (!present) {
        coverState_ = interfaces::CoverState::NOT_PRESENT;
    } else if (coverState_.load() == interfaces::CoverState::NOT_PRESENT) {
        coverState_ = interfaces::CoverState::UNKNOWN;
    }

    setProperty("coverPresent", present);
    setProperty("hasCover", present);
    setProperty("coverState", static_cast<int>(coverState_.load()));
}

void CoverCalibrator::setCalibratorPresent(bool present) {
    calibratorPresent_ = present;
    hasCalibrator_ = present;

    if (!present) {
        calibratorState_ = interfaces::CalibratorState::NOT_PRESENT;
    } else if (calibratorState_.load() == interfaces::CalibratorState::NOT_PRESENT) {
        calibratorState_ = interfaces::CalibratorState::UNKNOWN;
    }

    setProperty("calibratorPresent", present);
    setProperty("hasCalibrator", present);
    setProperty("calibratorState", static_cast<int>(calibratorState_.load()));
}

void CoverCalibrator::setCoverType(const std::string& type) {
    coverType_ = type;
    setProperty("coverType", type);
}

void CoverCalibrator::setCalibratorType(const std::string& type) {
    calibratorType_ = type;
    setProperty("calibratorType", type);
}

void CoverCalibrator::setLEDConfiguration(const json& config) {
    ledConfiguration_ = config;
    setProperty("ledConfiguration", config);
}

// Status methods
bool CoverCalibrator::isCoverPresent() const {
    return coverPresent_;
}

bool CoverCalibrator::isCalibratorPresent() const {
    return calibratorPresent_;
}

double CoverCalibrator::getCalibratorTemperature() const {
    return calibratorTemperature_.load();
}

int CoverCalibrator::getCalibratorPower() const {
    return calibratorPower_.load();
}

std::chrono::milliseconds CoverCalibrator::getCoverOperationTime() const {
    if (coverOperationRunning_.load()) {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - coverOperationStart_);
    }
    return std::chrono::milliseconds::zero();
}

std::chrono::milliseconds CoverCalibrator::getCalibratorOperationTime() const {
    if (calibratorOperationRunning_.load()) {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - calibratorOperationStart_);
    }
    return std::chrono::milliseconds::zero();
}

// Hardware abstraction methods (simulation)
bool CoverCalibrator::executeCoverOpen() {
    SPDLOG_DEBUG("Cover calibrator {} executing cover open", getDeviceId());
    return true;
}

bool CoverCalibrator::executeCoverClose() {
    SPDLOG_DEBUG("Cover calibrator {} executing cover close", getDeviceId());
    return true;
}

bool CoverCalibrator::executeCoverHalt() {
    SPDLOG_DEBUG("Cover calibrator {} executing cover halt", getDeviceId());
    return true;
}

bool CoverCalibrator::executeCalibratorOn(int brightness) {
    SPDLOG_DEBUG("Cover calibrator {} executing calibrator on at brightness {}", getDeviceId(), brightness);
    return true;
}

bool CoverCalibrator::executeCalibratorOff() {
    SPDLOG_DEBUG("Cover calibrator {} executing calibrator off", getDeviceId());
    return true;
}

interfaces::CoverState CoverCalibrator::readCoverState() {
    return coverState_.load();
}

interfaces::CalibratorState CoverCalibrator::readCalibratorState() {
    return calibratorState_.load();
}

int CoverCalibrator::readCurrentBrightness() {
    return currentBrightness_.load();
}

double CoverCalibrator::readCalibratorTemperature() {
    return calibratorTemperature_.load();
}

int CoverCalibrator::readCalibratorPower() {
    return calibratorPower_.load();
}

// Control threads
void CoverCalibrator::coverControlThread() {
    auto targetState = (coverState_.load() == interfaces::CoverState::MOVING) ?
                      interfaces::CoverState::OPEN : interfaces::CoverState::CLOSED;

    if (targetState == interfaces::CoverState::OPEN) {
        executeCoverOpen();
    } else {
        executeCoverClose();
    }

    // Simulate cover movement time
    std::this_thread::sleep_for(std::chrono::seconds(coverTimeout_));

    if (coverOperationRunning_.load()) {
        coverState_ = targetState;
        coverMoving_ = false;
        setProperty("coverState", static_cast<int>(coverState_.load()));
        setProperty("coverMoving", false);

        SPDLOG_INFO("Cover calibrator {} cover operation completed", getDeviceId());
    }

    coverOperationRunning_ = false;
}

void CoverCalibrator::calibratorControlThread() {
    int targetBrightness = targetBrightness_.load();

    if (targetBrightness > 0) {
        // Turning on calibrator
        executeCalibratorOn(targetBrightness);

        // Warmup time
        if (supportsWarmup_ && warmupTime_ > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(warmupTime_));
        }

        if (calibratorOperationRunning_.load()) {
            currentBrightness_ = targetBrightness;
            calibratorState_ = interfaces::CalibratorState::READY;
            calibratorChanging_ = false;

            setProperty("currentBrightness", currentBrightness_.load());
            setProperty("calibratorState", static_cast<int>(calibratorState_.load()));
            setProperty("calibratorChanging", false);

            SPDLOG_INFO("Cover calibrator {} calibrator on at brightness {}", getDeviceId(), targetBrightness);
        }
    } else {
        // Turning off calibrator
        executeCalibratorOff();

        // Cooldown time
        if (cooldownTime_ > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(cooldownTime_));
        }

        if (calibratorOperationRunning_.load()) {
            currentBrightness_ = 0;
            calibratorState_ = interfaces::CalibratorState::OFF;
            calibratorChanging_ = false;

            setProperty("currentBrightness", 0);
            setProperty("calibratorState", static_cast<int>(calibratorState_.load()));
            setProperty("calibratorChanging", false);

            SPDLOG_INFO("Cover calibrator {} calibrator turned off", getDeviceId());
        }
    }

    calibratorOperationRunning_ = false;
}

void CoverCalibrator::temperatureMonitorThread() {
    while (temperatureMonitorRunning_.load()) {
        // Simulate temperature reading
        double baseTemp = 20.0;
        if (currentBrightness_.load() > 0) {
            // Temperature rises with brightness
            double tempIncrease = (currentBrightness_.load() / static_cast<double>(maxBrightness_)) * 30.0;
            baseTemp += tempIncrease;
        }

        calibratorTemperature_ = baseTemp;
        setProperty("calibratorTemperature", baseTemp);

        // Check for overheating
        if (baseTemp > maxTemperature_) {
            overheatingProtection_ = true;
            SPDLOG_WARN("Cover calibrator {} overheating detected: {:.1f}°C", getDeviceId(), baseTemp);
            calibratorOff();
        } else {
            overheatingProtection_ = false;
        }

        // Simulate power consumption
        int power = (currentBrightness_.load() * 100) / maxBrightness_;  // 0-100W
        calibratorPower_ = power;
        setProperty("calibratorPower", power);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// Utility methods
bool CoverCalibrator::isValidBrightness(int brightness) const {
    return brightness >= 0 && brightness <= maxBrightness_;
}

int CoverCalibrator::clampBrightness(int brightness) const {
    return std::clamp(brightness, 0, maxBrightness_);
}

void CoverCalibrator::updateCoverState() {
    if (coverPresent_) {
        coverState_ = readCoverState();
        setProperty("coverState", static_cast<int>(coverState_.load()));
    }
}

void CoverCalibrator::updateCalibratorState() {
    if (calibratorPresent_) {
        calibratorState_ = readCalibratorState();
        setProperty("calibratorState", static_cast<int>(calibratorState_.load()));
    }
}

void CoverCalibrator::checkSafetyLimits() {
    if (emergencyStop_.load()) {
        haltCover();
        calibratorOff();
        return;
    }

    if (overheatingProtection_.load() && calibratorState_.load() != interfaces::CalibratorState::OFF) {
        calibratorOff();
    }
}

// ModernDeviceBase overrides
bool CoverCalibrator::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "OPEN_COVER") {
        openCover();
        result["success"] = true;
        return true;
    } else if (command == "CLOSE_COVER") {
        closeCover();
        result["success"] = true;
        return true;
    } else if (command == "HALT_COVER") {
        haltCover();
        result["success"] = true;
        return true;
    } else if (command == "CALIBRATOR_ON") {
        int brightness = parameters.value("brightness", maxBrightness_);
        calibratorOn(brightness);
        result["success"] = true;
        return true;
    } else if (command == "CALIBRATOR_OFF") {
        calibratorOff();
        result["success"] = true;
        return true;
    } else if (command == "SET_BRIGHTNESS") {
        int brightness = parameters.at("brightness").get<int>();
        setBrightness(brightness);
        result["success"] = true;
        return true;
    }

    return false;
}

void CoverCalibrator::updateDevice() {
    checkSafetyLimits();
    updateCoverState();
    updateCalibratorState();
}

// Manufacturer-specific initialization
void CoverCalibrator::initializeManufacturerSpecific() {
    std::string manufacturer = getProperty("manufacturer").get<std::string>();

    if (manufacturer == "Generic") {
        initializeGeneric();
    } else if (manufacturer == "Alnitak") {
        initializeAlnitak();
    } else if (manufacturer == "Optec") {
        initializeOptec();
    } else if (manufacturer == "FLI") {
        initializeFLI();
    } else if (manufacturer == "Lacerta") {
        initializeLacerta();
    } else if (manufacturer == "Pegasus Astro") {
        initializePegasusAstro();
    } else {
        initializeGeneric();
    }
}

void CoverCalibrator::initializeGeneric() {
    maxBrightness_ = 255;
    coverTimeout_ = 30;
    calibratorTimeout_ = 10;
    warmupTime_ = 5;
    cooldownTime_ = 3;
}

void CoverCalibrator::initializeAlnitak() {
    maxBrightness_ = 255;
    coverTimeout_ = 15;
    calibratorTimeout_ = 5;
    warmupTime_ = 3;
    cooldownTime_ = 2;
    coverType_ = "Flip-Flat";
    calibratorType_ = "LED";
}

void CoverCalibrator::initializeOptec() {
    maxBrightness_ = 100;
    coverTimeout_ = 20;
    calibratorTimeout_ = 8;
    warmupTime_ = 4;
    cooldownTime_ = 3;
    coverType_ = "Dust Cover";
    calibratorType_ = "EL Panel";
}

void CoverCalibrator::initializeFLI() {
    maxBrightness_ = 255;
    coverTimeout_ = 25;
    calibratorTimeout_ = 12;
    warmupTime_ = 6;
    cooldownTime_ = 4;
    coverType_ = "Motorized Cover";
    calibratorType_ = "LED Array";
}

void CoverCalibrator::initializeLacerta() {
    maxBrightness_ = 255;
    coverTimeout_ = 18;
    calibratorTimeout_ = 7;
    warmupTime_ = 4;
    cooldownTime_ = 2;
    coverType_ = "Flat Panel";
    calibratorType_ = "LED";
}

void CoverCalibrator::initializePegasusAstro() {
    maxBrightness_ = 255;
    coverTimeout_ = 12;
    calibratorTimeout_ = 4;
    warmupTime_ = 2;
    cooldownTime_ = 1;
    coverType_ = "Flip-Flat";
    calibratorType_ = "High-Power LED";
}

// Factory function
std::unique_ptr<CoverCalibrator> createModernCoverCalibrator(const std::string& deviceId,
                                                             const std::string& manufacturer,
                                                             const std::string& model) {
    return std::make_unique<CoverCalibrator>(deviceId, manufacturer, model);
}

} // namespace device
} // namespace astrocomm

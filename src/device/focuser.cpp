#include "focuser.h"
#include "behaviors/movable_behavior.h"
#include "behaviors/temperature_control_behavior.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace hydrogen {
namespace device {

// Forward declarations for behavior classes
class FocuserMovableBehavior;
class FocuserTemperatureBehavior;

// Focuser implementation
Focuser::Focuser(const ::std::string& deviceId, const ::std::string& manufacturer, const ::std::string& model)
    : ::hydrogen::device::core::ModernDeviceBase(deviceId, "FOCUSER", manufacturer, model)
    , movableBehavior_(nullptr)
    , temperatureBehavior_(nullptr)
    , maxPosition_(10000)
    , stepSize_(1)
    , backlash_(0)
    , temperatureCompensation_(false)
    , tempCompCoefficient_(0.0)
    , currentTemperature_(20.0)
    , ambientTemperature_(20.0)
    , hardwareMaxPosition_(30000)
    , hardwareMinPosition_(0)
    , hardwareStepSize_(1.0)
    , hasTemperatureSensor_(true)
    , hasTemperatureControl_(false)
    , serialPort_("")
    , baudRate_(9600)
    , maxSpeed_(1000)
    , acceleration_(500)
    , temperatureOffset_(0.0)
    , temperatureScale_(1.0)
    , cancelAutoFocus_(false) {

    initializeHardware();

    ::spdlog::info("Focuser {} created with manufacturer: {}, model: {}", deviceId, manufacturer, model);
}

Focuser::~Focuser() {
    stop();
}

bool Focuser::initializeDevice() {
    initializeFocuserBehaviors();
    
    // Initialize focuser-specific properties
    setProperty("maxPosition", maxPosition_.load());
    setProperty("stepSize", stepSize_.load());
    setProperty("backlash", backlash_.load());
    setProperty("temperatureCompensation", temperatureCompensation_.load());
    setProperty("tempCompCoefficient", tempCompCoefficient_.load());
    setProperty("currentTemperature", currentTemperature_.load());
    setProperty("ambientTemperature", ambientTemperature_.load());
    
    return true;
}

bool Focuser::startDevice() {
    return true; // Behaviors handle their own lifecycle
}

void Focuser::stopDevice() {
    // Stop any ongoing movement
    if (isMoving()) {
        stopMovement();
    }
    
    // Stop temperature control
    if (temperatureBehavior_) {
        temperatureBehavior_->stopControl();
    }
}

void Focuser::initializeHardware() {
    // Set manufacturer-specific parameters (enhanced from modern_focuser)
    ::std::string manufacturer = getDeviceInfo()["manufacturer"];
    if (manufacturer == "ZWO") {
        hardwareMaxPosition_ = 30000;
        serialPort_ = "COM3"; // Default for ZWO EAF
        baudRate_ = 115200;
        hasTemperatureSensor_ = true;
        hasTemperatureControl_ = false;
        temperatureOffset_ = 0.0;
        temperatureScale_ = 1.0;
        maxSpeed_ = 1000;
        acceleration_ = 500;
    }
    else if (manufacturer == "Celestron") {
        hardwareMaxPosition_ = 9999;
        serialPort_ = "COM4";
        baudRate_ = 9600;
        hasTemperatureSensor_ = false;
        hasTemperatureControl_ = false;
        temperatureOffset_ = 0.0;
        temperatureScale_ = 1.0;
        maxSpeed_ = 800;
        acceleration_ = 400;
    }
    else if (manufacturer == "Moonlite") {
        hardwareMaxPosition_ = 65535;
        serialPort_ = "COM5";
        baudRate_ = 9600;
        hasTemperatureSensor_ = true;
        hasTemperatureControl_ = true;
        temperatureOffset_ = -2.5; // Typical offset for Moonlite
        temperatureScale_ = 1.0;
        maxSpeed_ = 1200;
        acceleration_ = 600;
    }
    else if (manufacturer == "QHY") {
        hardwareMaxPosition_ = 50000;
        serialPort_ = "COM6";
        baudRate_ = 115200;
        hasTemperatureSensor_ = true;
        hasTemperatureControl_ = false;
        temperatureOffset_ = 0.5;
        temperatureScale_ = 1.0;
        maxSpeed_ = 1500;
        acceleration_ = 750;
    }

    // Update max position in atomic variable
    maxPosition_ = hardwareMaxPosition_;
}

void Focuser::initializeFocuserBehaviors() {
    // TODO: Implement behavior initialization
    // The FocuserMovableBehavior and FocuserTemperatureBehavior classes need to be defined

    // Add movable behavior for position control
    // movableBehavior_ = new FocuserMovableBehavior(this);
    // addBehavior(::std::unique_ptr<behaviors::DeviceBehavior>(movableBehavior_));

    // Add temperature control behavior
    // temperatureBehavior_ = new FocuserTemperatureBehavior(this);
    // addBehavior(::std::unique_ptr<behaviors::DeviceBehavior>(temperatureBehavior_));
}

// IMovable interface implementation (delegated to MovableBehavior)
bool Focuser::moveToPosition(int position) {
    if (movableBehavior_) {
        return movableBehavior_->moveToPosition(position);
    }
    return false;
}

bool Focuser::moveRelative(int steps) {
    if (movableBehavior_) {
        return movableBehavior_->moveRelative(steps);
    }
    return false;
}

bool Focuser::stopMovement() {
    if (movableBehavior_) {
        return movableBehavior_->stopMovement();
    }
    return false;
}

bool Focuser::home() {
    if (movableBehavior_) {
        return movableBehavior_->home();
    }
    return false;
}

int Focuser::getCurrentPosition() const {
    if (movableBehavior_) {
        return movableBehavior_->getCurrentPosition();
    }
    return 0;
}

bool Focuser::isMoving() const {
    if (movableBehavior_) {
        return movableBehavior_->isMoving();
    }
    return false;
}

// IFocuser specific interface
double Focuser::getTemperature() const {
    return currentTemperature_;
}

bool Focuser::supportsTemperatureCompensation() const {
    return true; // Most modern focusers support temperature compensation
}

bool Focuser::setTemperatureCompensation(bool enabled) {
    temperatureCompensation_ = enabled;
    setProperty("temperatureCompensation", enabled);

    ::spdlog::info("Focuser {} temperature compensation {}", getDeviceId(), enabled ? "enabled" : "disabled");
    return true;
}

// ITemperatureControlled interface implementation (delegated to TemperatureControlBehavior)
bool Focuser::setTargetTemperature(double temperature) {
    if (temperatureBehavior_) {
        return temperatureBehavior_->setTargetTemperature(temperature);
    }
    return false;
}

double Focuser::getCurrentTemperature() const {
    if (temperatureBehavior_) {
        return temperatureBehavior_->getCurrentTemperature();
    }
    return currentTemperature_;
}

double Focuser::getTargetTemperature() const {
    if (temperatureBehavior_) {
        return temperatureBehavior_->getTargetTemperature();
    }
    return currentTemperature_;
}

bool Focuser::stopTemperatureControl() {
    if (temperatureBehavior_) {
        return temperatureBehavior_->stopControl();
    }
    return true;
}

bool Focuser::isTemperatureStable() const {
    if (temperatureBehavior_) {
        return temperatureBehavior_->isTemperatureStable();
    }
    return true;
}

// Extended functionality (backward compatibility)
bool Focuser::moveAbsolute(int position, bool synchronous) {
    bool result = moveToPosition(position);

    if (result && synchronous) {
        // Wait for movement to complete
        while (isMoving()) {
            ::std::this_thread::sleep_for(::std::chrono::milliseconds(100));
        }
    }

    return result;
}

int Focuser::getMaxPosition() const {
    return maxPosition_;
}

bool Focuser::setMaxPosition(int maxPos) {
    if (maxPos <= 0) {
        return false;
    }

    maxPosition_ = maxPos;
    setProperty("maxPosition", maxPos);
    return true;
}

int Focuser::getBacklash() const {
    return backlash_;
}

bool Focuser::setBacklash(int backlash) {
    if (backlash < 0) {
        return false;
    }

    backlash_ = backlash;
    setProperty("backlash", backlash);
    return true;
}

double Focuser::getTempCompCoefficient() const {
    return tempCompCoefficient_;
}

bool Focuser::setTempCompCoefficient(double coefficient) {
    tempCompCoefficient_ = coefficient;
    setProperty("tempCompCoefficient", coefficient);
    return true;
}

// Hardware abstraction methods (simulation)
bool Focuser::executeMovement(int targetPosition) {
    if (!validatePosition(targetPosition)) {
        ::spdlog::error("Focuser {} invalid target position: {}", getDeviceId(), targetPosition);
        return false;
    }

    ::spdlog::debug("Focuser {} executing movement to position {}", getDeviceId(), targetPosition);

    // Enhanced movement simulation from modern_focuser
    ::std::thread([this, targetPosition]() {
        int currentPos = getCurrentPosition();
        int distance = ::std::abs(targetPosition - currentPos);
        int movementTime = calculateMovementTime(distance);

        // Simulate gradual movement
        int steps = distance / 10; // Move in 10 increments
        if (steps < 1) steps = 1;

        int stepDelay = movementTime / steps;
        int stepSize = distance / steps;

        for (int i = 0; i < steps && isMoving(); ++i) {
            ::std::this_thread::sleep_for(::std::chrono::milliseconds(stepDelay));

            int newPos = currentPos + (targetPosition > currentPos ? stepSize : -stepSize);
            if (i == steps - 1) {
                newPos = targetPosition; // Ensure we reach exact target
            }

            if (movableBehavior_) {
                movableBehavior_->updateCurrentPosition(newPos);
            }
        }

        if (movableBehavior_) {
            movableBehavior_->onMovementComplete(true);
        }

        ::spdlog::info("Focuser {} movement to position {} completed", getDeviceId(), targetPosition);
    }).detach();

    return true;
}

bool Focuser::executeStop() {
    ::spdlog::debug("Focuser {} executing stop", getDeviceId());
    return true;
}

bool Focuser::executeHome() {
    ::spdlog::debug("Focuser {} executing home", getDeviceId());
    return executeMovement(hardwareMinPosition_);
}

bool Focuser::validatePosition(int position) const {
    return position >= hardwareMinPosition_ && position <= hardwareMaxPosition_;
}

int Focuser::calculateMovementTime(int distance) const {
    // Calculate movement time based on distance and speed
    // Formula: time = distance / speed + acceleration_time

    double baseTime = static_cast<double>(distance) / maxSpeed_ * 1000; // Convert to ms
    double accelTime = static_cast<double>(maxSpeed_) / acceleration_ * 1000; // Acceleration time

    return static_cast<int>(baseTime + accelTime);
}

double Focuser::readTemperature() {
    if (!hasTemperatureSensor_) {
        return 20.0; // Default temperature if no sensor
    }

    // Enhanced temperature reading with realistic variation from modern_focuser
    static double baseTemp = 15.0;
    static ::std::random_device rd;
    static ::std::mt19937 gen(rd());
    static ::std::uniform_real_distribution<> variation(-0.2, 0.2);

    double rawTemp = baseTemp + variation(gen);
    double calibratedTemp = (rawTemp * temperatureScale_) + temperatureOffset_;

    currentTemperature_ = calibratedTemp;
    setProperty("currentTemperature", calibratedTemp);
    return calibratedTemp;
}

double Focuser::readAmbientTemperature() {
    // Simulate ambient temperature reading
    static ::std::random_device rd;
    static ::std::mt19937 gen(rd());
    static ::std::uniform_real_distribution<> dis(-1.0, 1.0);

    return ambientTemperature_ + dis(gen);
}

bool Focuser::setTemperatureControl(double power) {
    if (!hasTemperatureControl_) {
        ::spdlog::warn("Focuser {} does not support temperature control", getDeviceId());
        return false;
    }

    // Clamp power to valid range
    power = ::std::clamp(power, 0.0, 100.0);

    ::spdlog::debug("Focuser {} setting temperature control power to {:.1f}%",
                 getDeviceId(), power);

    // Enhanced temperature control simulation
    if (power > 0) {
        double coolingEffect = power * 0.01;
        double newTemp = currentTemperature_.load() - coolingEffect;
        currentTemperature_ = newTemp;
    }

    setProperty("temperatureControlPower", power);
    return true;
}

bool Focuser::handleDeviceCommand(const ::std::string& command, const json& parameters, json& result) {
    if (command == "MOVE_ABSOLUTE") {
        int position = parameters.value("position", 0);
        bool sync = parameters.value("synchronous", false);
        result["success"] = moveAbsolute(position, sync);
        return true;
    }
    else if (command == "MOVE_RELATIVE") {
        int steps = parameters.value("steps", 0);
        bool sync = parameters.value("synchronous", false);
        result["success"] = moveRelative(steps);
        return true;
    }
    else if (command == "ABORT") {
        result["success"] = stopMovement();
        return true;
    }
    else if (command == "HOME") {
        result["success"] = home();
        return true;
    }
    else if (command == "SET_MAX_POSITION") {
        int maxPos = parameters.value("maxPosition", 10000);
        result["success"] = setMaxPosition(maxPos);
        return true;
    }
    else if (command == "SET_BACKLASH") {
        int backlash = parameters.value("backlash", 0);
        result["success"] = setBacklash(backlash);
        return true;
    }
    else if (command == "SET_TEMPERATURE_COMPENSATION") {
        bool enabled = parameters.value("enabled", false);
        result["success"] = setTemperatureCompensation(enabled);
        return true;
    }

    return false;
}

void Focuser::updateDevice() {
    // Update position and movement status
    setProperty("currentPosition", getCurrentPosition());
    setProperty("isMoving", isMoving());

    // Update temperature readings
    setProperty("currentTemperature", readTemperature());
    setProperty("ambientTemperature", readAmbientTemperature());

    // Apply temperature compensation if enabled
    if (temperatureCompensation_) {
        double tempDiff = readTemperature() - 20.0; // Reference temperature
        int compensation = static_cast<int>(tempDiff * tempCompCoefficient_);

        if (::std::abs(compensation) > 5) { // Only compensate for significant changes
            moveRelative(compensation);
            ::spdlog::debug("Focuser {} applied temperature compensation: {} steps", getDeviceId(), compensation);
        }
    }
}

::std::vector<::std::string> Focuser::getCapabilities() const {
    return {
        "MOVE_ABSOLUTE",
        "MOVE_RELATIVE",
        "ABORT",
        "HOME",
        "SET_MAX_POSITION",
        "SET_BACKLASH",
        "SET_TEMPERATURE_COMPENSATION",
        "TEMPERATURE_CONTROL"
    };
}

// Factory function for creating focuser instances (merged from modern_focuser)
::std::unique_ptr<Focuser> createModernFocuser(const ::std::string& deviceId,
                                             const ::std::string& manufacturer,
                                             const ::std::string& model) {
    return ::std::make_unique<Focuser>(deviceId, manufacturer, model);
}

// FocuserMovableBehavior implementation
class FocuserMovableBehavior : public behaviors::MovableBehavior {
private:
    Focuser* focuser_;

public:
    explicit FocuserMovableBehavior(Focuser* focuser)
        : MovableBehavior("focuser_movable"), focuser_(focuser) {
    }

protected:
    bool executeMovement(int targetPosition) override {
        return focuser_->executeMovement(targetPosition);
    }

    bool executeStop() override {
        return focuser_->executeStop();
    }

    bool executeHome() override {
        return focuser_->executeHome();
    }
};

// FocuserTemperatureBehavior implementation
class FocuserTemperatureBehavior : public behaviors::TemperatureControlBehavior {
private:
    Focuser* focuser_;

public:
    explicit FocuserTemperatureBehavior(Focuser* focuser)
        : TemperatureControlBehavior("focuser_temperature"), focuser_(focuser) {
    }

protected:
    double readCurrentTemperature() override {
        return focuser_->readTemperature();
    }

    double readAmbientTemperature() override {
        return focuser_->readAmbientTemperature();
    }

    bool setControlPower(double power) override {
        return focuser_->setTemperatureControl(power);
    }
};

// Missing IDevice interface method implementations
std::string Focuser::getName() const {
    return getDeviceId();
}

std::string Focuser::getDescription() const {
    return "Generic Focuser Device";
}

std::string Focuser::getDriverInfo() const {
    return "Hydrogen Focuser Driver v1.0";
}

std::string Focuser::getDriverVersion() const {
    return "1.0.0";
}

int Focuser::getInterfaceVersion() const {
    return 1; // Interface version 1
}

std::vector<std::string> Focuser::getSupportedActions() const {
    return {"moveAbsolute", "moveRelative", "stop", "home", "setTemperatureCompensation"};
}

bool Focuser::isConnecting() const {
    return false; // Not in connecting state
}

interfaces::DeviceState Focuser::getDeviceState() const {
    if (isConnected()) {
        if (isMoving()) {
            return interfaces::DeviceState::BUSY;
        }
        return interfaces::DeviceState::IDLE;
    }
    return interfaces::DeviceState::UNKNOWN;
}

std::string Focuser::action(const std::string& actionName, const std::string& actionParameters) {
    // Basic action handling
    (void)actionName;
    (void)actionParameters;
    return "OK";
}

void Focuser::commandBlind(const std::string& command, bool raw) {
    // Basic command handling
    (void)command;
    (void)raw;
}

bool Focuser::commandBool(const std::string& command, bool raw) {
    // Basic command handling
    (void)command;
    (void)raw;
    return true;
}

std::string Focuser::commandString(const std::string& command, bool raw) {
    // Basic command handling
    (void)command;
    (void)raw;
    return "OK";
}

void Focuser::setupDialog() {
    // No setup dialog for basic implementation
}

void Focuser::run() {
    // Main focuser device loop
    SPDLOG_INFO("Focuser {} starting main loop", getDeviceId());

    while (isRunning()) {
        try {
            // Update focuser state and handle any pending operations
            if (isMoving()) {
                // Check if movement is complete
                // This is a stub implementation
            }

            // Sleep for a short time to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        } catch (const std::exception& e) {
            SPDLOG_ERROR("Focuser {} error in main loop: {}", getDeviceId(), e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    SPDLOG_INFO("Focuser {} main loop stopped", getDeviceId());
}

// Missing virtual method implementations
bool Focuser::abort() {
    return stopMovement();
}

bool Focuser::setSpeed(int speed) {
    if (speed < 1 || speed > maxSpeed_) {
        return false;
    }
    // Speed setting implementation
    SPDLOG_DEBUG("Focuser {} speed set to {}", getDeviceId(), speed);
    return true;
}

bool Focuser::setStepMode(StepMode mode) {
    // Step mode implementation
    SPDLOG_DEBUG("Focuser {} step mode set to {}", getDeviceId(), static_cast<int>(mode));
    return true;
}

bool Focuser::saveFocusPoint(const std::string& name, const std::string& description) {
    // Save focus point implementation
    SPDLOG_DEBUG("Focuser {} saved focus point '{}': {}", getDeviceId(), name, description);
    return true;
}

bool Focuser::moveToSavedPoint(const std::string& name, bool synchronous) {
    // Move to saved point implementation
    SPDLOG_DEBUG("Focuser {} moving to saved point '{}'", getDeviceId(), name);
    return moveAbsolute(5000, synchronous); // Default position
}

json Focuser::getSavedFocusPoints() const {
    // Return saved focus points as JSON
    return json::array(); // Empty array for stub implementation
}

bool Focuser::startAutoFocus(int startPosition, int endPosition, int stepSize, bool useTemperatureCompensation) {
    // Auto focus implementation
    SPDLOG_DEBUG("Focuser {} starting auto focus from {} to {} with step size {}",
                 getDeviceId(), startPosition, endPosition, stepSize);
    (void)useTemperatureCompensation;
    return true;
}

json Focuser::getFocusCurveData() const {
    // Return focus curve data as JSON
    return json::array(); // Empty array for stub implementation
}

bool Focuser::saveConfiguration(const std::string& filename) const {
    // Save configuration implementation
    SPDLOG_DEBUG("Focuser {} saving configuration to '{}'", getDeviceId(), filename);
    return true;
}

bool Focuser::loadConfiguration(const std::string& filename) {
    // Load configuration implementation
    SPDLOG_DEBUG("Focuser {} loading configuration from '{}'", getDeviceId(), filename);
    return true;
}

void Focuser::updateLoop() {
    // Update loop implementation
    updateDevice();
}

void Focuser::sendMoveCompletedEvent(const std::string& eventData) {
    // Send move completed event
    SPDLOG_DEBUG("Focuser {} move completed: {}", getDeviceId(), eventData);
}

int Focuser::applyTemperatureCompensation(int currentPosition) {
    // Temperature compensation implementation
    if (!temperatureCompensation_) {
        return currentPosition;
    }

    double tempDiff = readTemperature() - 20.0; // Reference temperature
    int compensation = static_cast<int>(tempDiff * tempCompCoefficient_);
    return currentPosition + compensation;
}

double Focuser::calculateFocusMetric(int position) {
    // Focus metric calculation implementation
    (void)position;
    return 1.0; // Stub implementation
}

void Focuser::performAutoFocus() {
    // Perform auto focus implementation
    SPDLOG_DEBUG("Focuser {} performing auto focus", getDeviceId());
}

} // namespace device
} // namespace hydrogen



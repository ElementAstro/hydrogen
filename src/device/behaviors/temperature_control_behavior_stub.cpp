#include "temperature_control_behavior.h"
#include <spdlog/spdlog.h>

namespace hydrogen {
namespace device {
namespace behaviors {

TemperatureControlBehavior::TemperatureControlBehavior(const std::string& /*deviceId*/)
    : DeviceBehavior("TemperatureControl")
    , currentTemperature_(20.0)
    , targetTemperature_(20.0)
    , ambientTemperature_(20.0)
    , controlMode_(TemperatureControlMode::MANUAL)
    , minTemperature_(-50.0)
    , maxTemperature_(50.0)
    , pidKp_(1.0)
    , pidKi_(0.1)
    , pidKd_(0.01)
    , controlPower_(0.0) {
    SPDLOG_DEBUG("TemperatureControlBehavior created for device {}", deviceId_);
}

TemperatureControlBehavior::~TemperatureControlBehavior() {
    stopControl();
    SPDLOG_DEBUG("TemperatureControlBehavior destroyed for device {}", deviceId_);
}

bool TemperatureControlBehavior::initialize(std::shared_ptr<core::StateManager> stateManager,
                                          std::shared_ptr<core::ConfigManager> configManager) {
    if (!DeviceBehavior::initialize(stateManager, configManager)) {
        return false;
    }
    
    initializeTemperatureConfigs();
    SPDLOG_DEBUG("TemperatureControlBehavior initialized for device {}", deviceId_);
    return true;
}

bool TemperatureControlBehavior::start() {
    if (!DeviceBehavior::start()) {
        return false;
    }
    
    SPDLOG_DEBUG("TemperatureControlBehavior started for device {}", deviceId_);
    return true;
}

void TemperatureControlBehavior::stop() {
    stopControl();
    DeviceBehavior::stop();
    SPDLOG_DEBUG("TemperatureControlBehavior stopped for device {}", deviceId_);
}

void TemperatureControlBehavior::update() {
    // Stub implementation - basic temperature simulation
    if (isControlling()) {
        // Simple temperature simulation
        double current = currentTemperature_.load();
        double target = targetTemperature_.load();
        double diff = target - current;
        if (std::abs(diff) > 0.1) {
            currentTemperature_.store(current + diff * 0.1); // Gradual approach
        }
    }
}

bool TemperatureControlBehavior::handleCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "setTargetTemperature") {
        if (parameters.contains("temperature")) {
            double temp = parameters["temperature"];
            setTargetTemperature(temp, [&result](bool success, double actualTemp) {
                result["success"] = success;
                result["temperature"] = actualTemp;
            });
            return true;
        }
    } else if (command == "getCurrentTemperature") {
        result["temperature"] = getCurrentTemperature();
        return true;
    } else if (command == "getTargetTemperature") {
        result["temperature"] = getTargetTemperature();
        return true;
    } else if (command == "stopControl") {
        stopControl();
        result["success"] = true;
        return true;
    }
    
    return DeviceBehavior::handleCommand(command, parameters, result);
}

json TemperatureControlBehavior::getStatus() const {
    json status = DeviceBehavior::getStatus();
    status["currentTemperature"] = currentTemperature_.load();
    status["targetTemperature"] = targetTemperature_.load();
    status["isControlling"] = isControlling();
    return status;
}

std::vector<std::string> TemperatureControlBehavior::getCapabilities() const {
    return {"temperature_control", "pid_control", "temperature_monitoring"};
}

bool TemperatureControlBehavior::setTargetTemperature(double temperature, TemperatureStabilizedCallback callback) {
    if (!isValidTemperature(temperature)) {
        if (callback) callback(false, currentTemperature_.load());
        return false;
    }

    targetTemperature_.store(temperature);

    SPDLOG_DEBUG("Target temperature set to {}", temperature);

    if (callback) callback(true, temperature);
    return true;
}

double TemperatureControlBehavior::getCurrentTemperature() const {
    return currentTemperature_;
}

double TemperatureControlBehavior::getTargetTemperature() const {
    return targetTemperature_;
}

double TemperatureControlBehavior::getAmbientTemperature() const {
    return ambientTemperature_;
}

TemperatureControlState TemperatureControlBehavior::getControlState() const {
    return TemperatureControlState::IDLE; // Stub implementation
}

TemperatureControlMode TemperatureControlBehavior::getControlMode() const {
    return controlMode_.load();
}

void TemperatureControlBehavior::setControlMode(TemperatureControlMode mode) {
    controlMode_.store(mode);
    SPDLOG_DEBUG("Control mode set to {}", static_cast<int>(mode));
}

bool TemperatureControlBehavior::isControlling() const {
    return false; // Stub implementation
}

bool TemperatureControlBehavior::isTemperatureStable() const {
    return true; // Stub implementation
}

bool TemperatureControlBehavior::stopControl() {
    controlPower_.store(0.0);
    SPDLOG_DEBUG("Temperature control stopped");
    return true;
}

void TemperatureControlBehavior::setTemperatureRange(double minTemp, double maxTemp) {
    minTemperature_ = minTemp;
    maxTemperature_ = maxTemp;
    SPDLOG_DEBUG("Temperature range set to [{}, {}] for device {}", minTemp, maxTemp, deviceId_);
}

double TemperatureControlBehavior::getMinTemperature() const {
    return minTemperature_;
}

double TemperatureControlBehavior::getMaxTemperature() const {
    return maxTemperature_;
}

void TemperatureControlBehavior::setPIDParameters(double kp, double ki, double kd) {
    pidKp_ = kp;
    pidKi_ = ki;
    pidKd_ = kd;
    SPDLOG_DEBUG("PID parameters set to Kp={}, Ki={}, Kd={} for device {}", kp, ki, kd, deviceId_);
}

void TemperatureControlBehavior::getPIDParameters(double& kp, double& ki, double& kd) const {
    kp = pidKp_;
    ki = pidKi_;
    kd = pidKd_;
}

double TemperatureControlBehavior::getControlPower() const {
    return controlPower_;
}

void TemperatureControlBehavior::initializeTemperatureConfigs() {
    // Stub implementation - would normally set up configuration definitions
    SPDLOG_DEBUG("Temperature configs initialized for device {}", deviceId_);
}

void TemperatureControlBehavior::updateCurrentTemperature(double temperature) {
    currentTemperature_ = temperature;
}

void TemperatureControlBehavior::updateAmbientTemperature(double temperature) {
    ambientTemperature_ = temperature;
}

bool TemperatureControlBehavior::checkTemperatureStability() {
    double current = currentTemperature_.load();
    double target = targetTemperature_.load();
    double diff = std::abs(target - current);
    return (diff < 0.5); // 0.5 degree tolerance
}

void TemperatureControlBehavior::onTemperatureStabilized(bool /*stabilized*/, double /*temperature*/) {
    // Stub implementation - would normally trigger callbacks
    SPDLOG_DEBUG("Temperature stabilized: {} at {}", stabilized, temperature);
}

bool TemperatureControlBehavior::isValidTemperature(double temperature) const {
    return temperature >= minTemperature_ && temperature <= maxTemperature_;
}

double TemperatureControlBehavior::calculatePIDOutput(double setpoint, double processValue) {
    // Simplified PID calculation
    double error = setpoint - processValue;
    return pidKp_ * error; // Simplified - just proportional term
}

void TemperatureControlBehavior::temperatureControlLoop() {
    // Stub implementation - would normally run control loop
    update();
}

} // namespace behaviors
} // namespace device
} // namespace hydrogen

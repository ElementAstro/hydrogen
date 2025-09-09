#include "temperature_control_behavior.h"
#include "../core/state_manager.h"
#include "../core/config_manager.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <cmath>

namespace hydrogen {
namespace device {
namespace behaviors {

TemperatureControlBehavior::TemperatureControlBehavior(const std::string& behaviorName)
    : DeviceBehavior(behaviorName)
    , currentTemperature_(20.0)
    , targetTemperature_(20.0)
    , ambientTemperature_(20.0)
    , minTemperature_(-50.0)
    , maxTemperature_(50.0)
    , controlState_(TemperatureControlState::IDLE)
    , controlMode_(TemperatureControlMode::PID)
    , controlPower_(0.0)
    , pidKp_(1.0)
    , pidKi_(0.1)
    , pidKd_(0.05)
    , pidIntegral_(0.0)
    , pidLastError_(0.0)
    , stabilityTolerance_(0.5)
    , stabilityDuration_(30)
    , controlRunning_(false)
    , controlInterval_(1000)
    , stabilizationTimeout_(300) {
    
    SPDLOG_DEBUG("TemperatureControlBehavior '{}' created", behaviorName);
}

TemperatureControlBehavior::~TemperatureControlBehavior() {
    stop();
}

bool TemperatureControlBehavior::initialize(std::shared_ptr<core::StateManager> stateManager,
                                          std::shared_ptr<core::ConfigManager> configManager) {
    if (!DeviceBehavior::initialize(stateManager, configManager)) {
        return false;
    }

    // 初始化温度控制配�?    initializeTemperatureConfigs();
    
    // 设置初始属�?    setProperty("currentTemperature", currentTemperature_.load());
    setProperty("targetTemperature", targetTemperature_.load());
    setProperty("ambientTemperature", ambientTemperature_.load());
    setProperty("controlState", static_cast<int>(controlState_.load()));
    setProperty("controlMode", static_cast<int>(controlMode_.load()));
    setProperty("controlPower", controlPower_.load());
    setProperty("minTemperature", minTemperature_.load());
    setProperty("maxTemperature", maxTemperature_.load());
    
    SPDLOG_DEBUG("TemperatureControlBehavior '{}' initialized", behaviorName_);
    return true;
}

bool TemperatureControlBehavior::start() {
    if (!DeviceBehavior::start()) {
        return false;
    }

    // 启动温度控制线程
    startTemperatureControl();
    
    return true;
}

void TemperatureControlBehavior::stop() {
    // 停止温度控制
    stopControl();
    
    // 停止控制线程
    stopTemperatureControl();
    
    DeviceBehavior::stop();
}

void TemperatureControlBehavior::update() {
    // 更新属�?    setProperty("currentTemperature", currentTemperature_.load());
    setProperty("targetTemperature", targetTemperature_.load());
    setProperty("ambientTemperature", ambientTemperature_.load());
    setProperty("controlState", static_cast<int>(controlState_.load()));
    setProperty("controlPower", controlPower_.load());
    setProperty("isStable", isTemperatureStable());
}

bool TemperatureControlBehavior::handleCommand(const std::string& command, const json& parameters, json& result) {
    // 先尝试基类处�?    if (DeviceBehavior::handleCommand(command, parameters, result)) {
        return true;
    }

    // 处理温度控制相关命令
    if (command == "SET_TARGET_TEMPERATURE") {
        if (!parameters.contains("temperature")) {
            result["error"] = "Missing temperature parameter";
            return true;
        }
        
        double temperature = parameters["temperature"];
        bool success = setTargetTemperature(temperature);
        result["success"] = success;
        if (!success) {
            result["error"] = "Failed to set target temperature";
        }
        return true;
    }
    else if (command == "GET_TEMPERATURE") {
        result["currentTemperature"] = getCurrentTemperature();
        result["targetTemperature"] = getTargetTemperature();
        result["ambientTemperature"] = getAmbientTemperature();
        return true;
    }
    else if (command == "STOP_CONTROL") {
        bool success = stopControl();
        result["success"] = success;
        return true;
    }
    else if (command == "SET_CONTROL_MODE") {
        if (!parameters.contains("mode")) {
            result["error"] = "Missing mode parameter";
            return true;
        }
        
        int mode = parameters["mode"];
        setControlMode(static_cast<TemperatureControlMode>(mode));
        result["success"] = true;
        return true;
    }
    else if (command == "SET_TEMPERATURE_RANGE") {
        if (!parameters.contains("minTemperature") || !parameters.contains("maxTemperature")) {
            result["error"] = "Missing temperature range parameters";
            return true;
        }
        
        double minTemp = parameters["minTemperature"];
        double maxTemp = parameters["maxTemperature"];
        setTemperatureRange(minTemp, maxTemp);
        result["success"] = true;
        return true;
    }
    else if (command == "SET_PID_PARAMETERS") {
        if (!parameters.contains("kp") || !parameters.contains("ki") || !parameters.contains("kd")) {
            result["error"] = "Missing PID parameters";
            return true;
        }
        
        double kp = parameters["kp"];
        double ki = parameters["ki"];
        double kd = parameters["kd"];
        setPIDParameters(kp, ki, kd);
        result["success"] = true;
        return true;
    }
    else if (command == "GET_PID_PARAMETERS") {
        double kp, ki, kd;
        getPIDParameters(kp, ki, kd);
        result["kp"] = kp;
        result["ki"] = ki;
        result["kd"] = kd;
        return true;
    }

    return false;
}

json TemperatureControlBehavior::getStatus() const {
    json status = DeviceBehavior::getStatus();
    
    status["currentTemperature"] = getCurrentTemperature();
    status["targetTemperature"] = getTargetTemperature();
    status["ambientTemperature"] = getAmbientTemperature();
    status["controlState"] = static_cast<int>(getControlState());
    status["controlMode"] = static_cast<int>(getControlMode());
    status["isControlling"] = isControlling();
    status["isStable"] = isTemperatureStable();
    status["controlPower"] = getControlPower();
    status["minTemperature"] = getMinTemperature();
    status["maxTemperature"] = getMaxTemperature();
    
    return status;
}

std::vector<std::string> TemperatureControlBehavior::getCapabilities() const {
    auto capabilities = DeviceBehavior::getCapabilities();
    
    std::vector<std::string> tempCapabilities = {
        "SET_TARGET_TEMPERATURE",
        "GET_TEMPERATURE",
        "STOP_CONTROL",
        "SET_CONTROL_MODE",
        "SET_TEMPERATURE_RANGE",
        "SET_PID_PARAMETERS",
        "GET_PID_PARAMETERS"
    };
    
    capabilities.insert(capabilities.end(), tempCapabilities.begin(), tempCapabilities.end());
    return capabilities;
}

bool TemperatureControlBehavior::setTargetTemperature(double temperature, TemperatureStabilizedCallback callback) {
    if (!isRunning()) {
        SPDLOG_WARN("TemperatureControlBehavior '{}' not running, cannot set target temperature", behaviorName_);
        return false;
    }

    if (!isValidTemperature(temperature)) {
        SPDLOG_WARN("Invalid target temperature {} for TemperatureControlBehavior '{}'", temperature, behaviorName_);
        return false;
    }

    std::lock_guard<std::mutex> lock(controlMutex_);
    
    targetTemperature_ = temperature;
    currentCallback_ = callback;
    
    // 重置PID积分�?    pidIntegral_ = 0.0;
    pidLastError_ = 0.0;
    
    // 设置控制状�?    double currentTemp = getCurrentTemperature();
    if (std::abs(temperature - currentTemp) < stabilityTolerance_) {
        controlState_ = TemperatureControlState::IDLE;
    } else if (temperature < currentTemp) {
        controlState_ = TemperatureControlState::COOLING;
    } else {
        controlState_ = TemperatureControlState::HEATING;
    }
    
    // 更新属�?    setProperty("targetTemperature", temperature);
    setProperty("controlState", static_cast<int>(controlState_.load()));
    
    SPDLOG_INFO("TemperatureControlBehavior '{}' target temperature set to {:.2f}°C", behaviorName_, temperature);
    return true;
}

double TemperatureControlBehavior::getCurrentTemperature() const {
    return currentTemperature_.load();
}

double TemperatureControlBehavior::getTargetTemperature() const {
    return targetTemperature_.load();
}

double TemperatureControlBehavior::getAmbientTemperature() const {
    return ambientTemperature_.load();
}

TemperatureControlState TemperatureControlBehavior::getControlState() const {
    return controlState_.load();
}

TemperatureControlMode TemperatureControlBehavior::getControlMode() const {
    return controlMode_.load();
}

void TemperatureControlBehavior::setControlMode(TemperatureControlMode mode) {
    controlMode_ = mode;
    setProperty("controlMode", static_cast<int>(mode));
    setConfig("controlMode", static_cast<int>(mode));
    
    SPDLOG_DEBUG("TemperatureControlBehavior '{}' control mode set to {}", behaviorName_, static_cast<int>(mode));
}

bool TemperatureControlBehavior::isControlling() const {
    TemperatureControlState state = controlState_.load();
    return state == TemperatureControlState::HEATING || 
           state == TemperatureControlState::COOLING || 
           state == TemperatureControlState::STABILIZING;
}

bool TemperatureControlBehavior::isTemperatureStable() const {
    double currentTemp = getCurrentTemperature();
    double targetTemp = getTargetTemperature();
    double tolerance = stabilityTolerance_.load();
    
    return std::abs(currentTemp - targetTemp) <= tolerance;
}

bool TemperatureControlBehavior::stopControl() {
    if (!isControlling()) {
        return true;
    }

    std::lock_guard<std::mutex> lock(controlMutex_);
    
    controlState_ = TemperatureControlState::IDLE;
    setProperty("controlState", static_cast<int>(TemperatureControlState::IDLE));
    
    // 停止控制输出
    setControlPower(0.0);
    controlPower_ = 0.0;
    
    // 通知控制完成
    if (currentCallback_) {
        currentCallback_(false, getCurrentTemperature());
        currentCallback_ = nullptr;
    }
    
    SPDLOG_INFO("TemperatureControlBehavior '{}' control stopped", behaviorName_);
    return true;
}

void TemperatureControlBehavior::setTemperatureRange(double minTemp, double maxTemp) {
    if (minTemp >= maxTemp) {
        SPDLOG_WARN("Invalid temperature range for TemperatureControlBehavior '{}': min={:.2f}, max={:.2f}", 
                   behaviorName_, minTemp, maxTemp);
        return;
    }

    minTemperature_ = minTemp;
    maxTemperature_ = maxTemp;
    
    setProperty("minTemperature", minTemp);
    setProperty("maxTemperature", maxTemp);
    setConfig("minTemperature", minTemp);
    setConfig("maxTemperature", maxTemp);
    
    SPDLOG_DEBUG("TemperatureControlBehavior '{}' temperature range set: {:.2f} - {:.2f}°C", 
                behaviorName_, minTemp, maxTemp);
}

double TemperatureControlBehavior::getMinTemperature() const {
    return minTemperature_.load();
}

double TemperatureControlBehavior::getMaxTemperature() const {
    return maxTemperature_.load();
}

void TemperatureControlBehavior::setPIDParameters(double kp, double ki, double kd) {
    pidKp_ = kp;
    pidKi_ = ki;
    pidKd_ = kd;
    
    setConfig("pidKp", kp);
    setConfig("pidKi", ki);
    setConfig("pidKd", kd);
    
    SPDLOG_DEBUG("TemperatureControlBehavior '{}' PID parameters set: Kp={:.3f}, Ki={:.3f}, Kd={:.3f}", 
                behaviorName_, kp, ki, kd);
}

void TemperatureControlBehavior::getPIDParameters(double& kp, double& ki, double& kd) const {
    kp = pidKp_.load();
    ki = pidKi_.load();
    kd = pidKd_.load();
}

double TemperatureControlBehavior::getControlPower() const {
    return controlPower_.load();
}

void TemperatureControlBehavior::initializeTemperatureConfigs() {
    if (!configManager_) {
        return;
    }

    // 定义温度控制相关配置
    std::vector<core::ConfigDefinition> tempConfigs = {
        {
            .name = getConfigName("minTemperature"),
            .type = core::ConfigType::DOUBLE,
            .defaultValue = -50.0,
            .description = "Minimum temperature",
            .required = false,
            .readOnly = false
        },
        {
            .name = getConfigName("maxTemperature"),
            .type = core::ConfigType::DOUBLE,
            .defaultValue = 50.0,
            .description = "Maximum temperature",
            .required = false,
            .readOnly = false
        },
        {
            .name = getConfigName("stabilityTolerance"),
            .type = core::ConfigType::DOUBLE,
            .defaultValue = 0.5,
            .minValue = 0.1,
            .maxValue = 5.0,
            .description = "Temperature stability tolerance",
            .required = false,
            .readOnly = false
        },
        {
            .name = getConfigName("stabilityDuration"),
            .type = core::ConfigType::INTEGER,
            .defaultValue = 30,
            .minValue = 5,
            .maxValue = 300,
            .description = "Stability duration in seconds",
            .required = false,
            .readOnly = false
        },
        {
            .name = getConfigName("pidKp"),
            .type = core::ConfigType::DOUBLE,
            .defaultValue = 1.0,
            .description = "PID proportional gain",
            .required = false,
            .readOnly = false
        },
        {
            .name = getConfigName("pidKi"),
            .type = core::ConfigType::DOUBLE,
            .defaultValue = 0.1,
            .description = "PID integral gain",
            .required = false,
            .readOnly = false
        },
        {
            .name = getConfigName("pidKd"),
            .type = core::ConfigType::DOUBLE,
            .defaultValue = 0.05,
            .description = "PID derivative gain",
            .required = false,
            .readOnly = false
        }
    };
    
    configManager_->defineConfigs(tempConfigs);
    
    // 从配置加载�?    minTemperature_ = getConfig("minTemperature", -50.0);
    maxTemperature_ = getConfig("maxTemperature", 50.0);
    stabilityTolerance_ = getConfig("stabilityTolerance", 0.5);
    stabilityDuration_ = getConfig("stabilityDuration", 30);
    pidKp_ = getConfig("pidKp", 1.0);
    pidKi_ = getConfig("pidKi", 0.1);
    pidKd_ = getConfig("pidKd", 0.05);
}

void TemperatureControlBehavior::updateCurrentTemperature(double temperature) {
    currentTemperature_ = temperature;
    setProperty("currentTemperature", temperature);
}

void TemperatureControlBehavior::updateAmbientTemperature(double temperature) {
    ambientTemperature_ = temperature;
    setProperty("ambientTemperature", temperature);
}

bool TemperatureControlBehavior::checkTemperatureStability() {
    if (!isTemperatureStable()) {
        // 温度不稳定，重置计时�?        stabilityStartTime_ = std::chrono::steady_clock::now();
        return false;
    }
    
    // 检查稳定持续时�?    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - stabilityStartTime_).count();
    
    return duration >= stabilityDuration_.load();
}

void TemperatureControlBehavior::onTemperatureStabilized(bool success, double finalTemperature) {
    std::lock_guard<std::mutex> lock(controlMutex_);
    
    controlState_ = success ? TemperatureControlState::IDLE : TemperatureControlState::ERROR;
    setProperty("controlState", static_cast<int>(controlState_.load()));
    
    if (success) {
        SPDLOG_INFO("TemperatureControlBehavior '{}' temperature stabilized at {:.2f}°C", 
                   behaviorName_, finalTemperature);
    } else {
        SPDLOG_ERROR("TemperatureControlBehavior '{}' failed to stabilize temperature", behaviorName_);
    }
    
    // 调用回调函数
    if (currentCallback_) {
        currentCallback_(success, finalTemperature);
        currentCallback_ = nullptr;
    }
}

bool TemperatureControlBehavior::isValidTemperature(double temperature) const {
    return temperature >= minTemperature_.load() && temperature <= maxTemperature_.load();
}

double TemperatureControlBehavior::calculatePIDOutput(double error, double deltaTime) {
    if (deltaTime <= 0) {
        return 0.0;
    }

    double kp = pidKp_.load();
    double ki = pidKi_.load();
    double kd = pidKd_.load();
    
    // 比例�?    double proportional = kp * error;
    
    // 积分�?    pidIntegral_ += error * deltaTime;
    double integral = ki * pidIntegral_;
    
    // 微分�?    double derivative = kd * (error - pidLastError_) / deltaTime;
    pidLastError_ = error;
    
    // PID输出
    double output = proportional + integral + derivative;
    
    // 限制输出范围
    output = std::max(-100.0, std::min(100.0, output));
    
    return output;
}

void TemperatureControlBehavior::temperatureControlLoop() {
    auto lastTime = std::chrono::steady_clock::now();
    stabilityStartTime_ = lastTime;
    
    while (controlRunning_) {
        try {
            auto currentTime = std::chrono::steady_clock::now();
            double deltaTime = std::chrono::duration<double>(currentTime - lastTime).count();
            lastTime = currentTime;
            
            // 读取当前温度
            double currentTemp = readCurrentTemperature();
            updateCurrentTemperature(currentTemp);
            
            // 读取环境温度
            double ambientTemp = readAmbientTemperature();
            updateAmbientTemperature(ambientTemp);
            
            if (isControlling()) {
                double targetTemp = getTargetTemperature();
                double error = targetTemp - currentTemp;
                
                double power = 0.0;
                
                if (controlMode_.load() == TemperatureControlMode::PID) {
                    // PID控制
                    power = calculatePIDOutput(error, deltaTime);
                } else if (controlMode_.load() == TemperatureControlMode::AUTO) {
                    // 简单自动控�?                    if (std::abs(error) > stabilityTolerance_.load()) {
                        power = error > 0 ? 50.0 : -50.0; // 50%功率
                    }
                }
                
                // 设置控制功率
                setControlPower(power);
                controlPower_ = power;
                
                // 检查温度稳定�?                if (checkTemperatureStability()) {
                    controlState_ = TemperatureControlState::STABILIZING;
                    onTemperatureStabilized(true, currentTemp);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(controlInterval_.load()));
            
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in temperature control loop for TemperatureControlBehavior '{}': {}", 
                        behaviorName_, e.what());
        }
    }
}

void TemperatureControlBehavior::startTemperatureControl() {
    if (controlRunning_) {
        return;
    }
    
    controlRunning_ = true;
    controlThread_ = std::thread(&TemperatureControlBehavior::temperatureControlLoop, this);
    
    SPDLOG_DEBUG("Temperature control started for TemperatureControlBehavior '{}'", behaviorName_);
}

void TemperatureControlBehavior::stopTemperatureControl() {
    if (!controlRunning_) {
        return;
    }
    
    controlRunning_ = false;
    
    if (controlThread_.joinable()) {
        controlThread_.join();
    }
    
    SPDLOG_DEBUG("Temperature control stopped for TemperatureControlBehavior '{}'", behaviorName_);
}

} // namespace behaviors
} // namespace device
} // namespace hydrogen

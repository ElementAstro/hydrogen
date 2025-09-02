#include "device_behavior.h"
#include "../core/state_manager.h"
#include "../core/config_manager.h"
#include <spdlog/spdlog.h>

namespace hydrogen {
namespace device {
namespace behaviors {

DeviceBehavior::DeviceBehavior(const std::string& behaviorName)
    : behaviorName_(behaviorName)
    , initialized_(false)
    , running_(false) {
    
    SPDLOG_DEBUG("DeviceBehavior '{}' created", behaviorName_);
}

bool DeviceBehavior::initialize(std::shared_ptr<core::StateManager> stateManager,
                               std::shared_ptr<core::ConfigManager> configManager) {
    if (initialized_) {
        SPDLOG_WARN("Behavior '{}' already initialized", behaviorName_);
        return true;
    }

    if (!stateManager || !configManager) {
        SPDLOG_ERROR("Invalid managers provided to behavior '{}'", behaviorName_);
        return false;
    }

    stateManager_ = stateManager;
    configManager_ = configManager;
    
    // 设置基础属�?    setProperty("initialized", true);
    setProperty("running", false);
    
    initialized_ = true;
    SPDLOG_DEBUG("Behavior '{}' initialized", behaviorName_);
    return true;
}

bool DeviceBehavior::start() {
    if (!initialized_) {
        SPDLOG_ERROR("Behavior '{}' not initialized, cannot start", behaviorName_);
        return false;
    }

    if (running_) {
        SPDLOG_WARN("Behavior '{}' already running", behaviorName_);
        return true;
    }

    running_ = true;
    setProperty("running", true);
    
    SPDLOG_DEBUG("Behavior '{}' started", behaviorName_);
    return true;
}

void DeviceBehavior::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    setProperty("running", false);
    
    SPDLOG_DEBUG("Behavior '{}' stopped", behaviorName_);
}

void DeviceBehavior::update() {
    // 基类默认实现为空，子类可以重�?}

bool DeviceBehavior::handleCommand(const std::string& command, 
                                  const json& parameters, 
                                  json& result) {
    // 处理通用命令
    if (command == "GET_STATUS") {
        result = getStatus();
        return true;
    }
    else if (command == "GET_CAPABILITIES") {
        result["capabilities"] = getCapabilities();
        return true;
    }
    else if (command == "START") {
        bool success = start();
        result["success"] = success;
        return true;
    }
    else if (command == "STOP") {
        stop();
        result["success"] = true;
        return true;
    }
    
    // 未处理的命令
    return false;
}

json DeviceBehavior::getStatus() const {
    json status;
    status["behaviorName"] = behaviorName_;
    status["initialized"] = initialized_;
    status["running"] = running_;
    return status;
}

std::vector<std::string> DeviceBehavior::getCapabilities() const {
    return {
        "GET_STATUS",
        "GET_CAPABILITIES", 
        "START",
        "STOP"
    };
}

void DeviceBehavior::setProperty(const std::string& property, const json& value) {
    if (stateManager_) {
        stateManager_->setProperty(getPropertyName(property), value);
    }
}

json DeviceBehavior::getProperty(const std::string& property) const {
    if (stateManager_) {
        return stateManager_->getProperty(getPropertyName(property));
    }
    return json();
}

void DeviceBehavior::setConfig(const std::string& name, const json& value) {
    if (configManager_) {
        configManager_->setConfig(getConfigName(name), value);
    }
}

json DeviceBehavior::getConfig(const std::string& name) const {
    if (configManager_) {
        return configManager_->getConfig(getConfigName(name));
    }
    return json();
}

std::string DeviceBehavior::getPropertyName(const std::string& property) const {
    return behaviorName_ + "." + property;
}

std::string DeviceBehavior::getConfigName(const std::string& name) const {
    return behaviorName_ + "." + name;
}

} // namespace behaviors
} // namespace device
} // namespace hydrogen

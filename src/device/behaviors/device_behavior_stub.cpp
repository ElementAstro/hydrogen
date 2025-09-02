#include "device_behavior.h"
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

// Destructor is explicitly defaulted in the header

// getBehaviorName() is implemented inline in the header

bool DeviceBehavior::initialize(std::shared_ptr<core::StateManager> stateManager,
                               std::shared_ptr<core::ConfigManager> configManager) {
    stateManager_ = stateManager;
    configManager_ = configManager;
    initialized_ = true;
    SPDLOG_DEBUG("DeviceBehavior '{}' initialized", behaviorName_);
    return true;
}

bool DeviceBehavior::start() {
    if (!initialized_) {
        SPDLOG_WARN("DeviceBehavior '{}' not initialized", behaviorName_);
        return false;
    }
    
    running_ = true;
    SPDLOG_DEBUG("DeviceBehavior '{}' started", behaviorName_);
    return true;
}

void DeviceBehavior::stop() {
    running_ = false;
    SPDLOG_DEBUG("DeviceBehavior '{}' stopped", behaviorName_);
}

void DeviceBehavior::update() {
    // Stub implementation - do nothing
}

bool DeviceBehavior::handleCommand(const std::string& command, const json& /*parameters*/, json& result) {
    // Basic command handling
    if (command == "getStatus") {
        result = getStatus();
        return true;
    } else if (command == "getCapabilities") {
        result["capabilities"] = getCapabilities();
        return true;
    }
    
    return false; // Command not handled
}

json DeviceBehavior::getStatus() const {
    json status;
    if (!initialized_) {
        status["state"] = "not_initialized";
    } else if (running_) {
        status["state"] = "running";
    } else {
        status["state"] = "stopped";
    }
    return status;
}

std::vector<std::string> DeviceBehavior::getCapabilities() const {
    return {}; // Stub implementation
}

// isInitialized() and isRunning() are implemented inline in the header

} // namespace behaviors
} // namespace device
} // namespace hydrogen

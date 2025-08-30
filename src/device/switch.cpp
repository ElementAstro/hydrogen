#include "switch.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <algorithm>

namespace astrocomm {
namespace device {

Switch::Switch(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "SWITCH", manufacturer, model)
    , switchCount_(4) // Default to 4 switches
    , groupControlEnabled_(true) {
    
    // Set default switch count based on manufacturer
    if (manufacturer == "Pegasus") {
        switchCount_ = 4; // UPB typically has 4 switches
    } else if (manufacturer == "ZWO") {
        switchCount_ = 4; // ASI Power typically has 4 switches
    } else if (manufacturer == "Lunatico") {
        switchCount_ = 8; // AAG CloudWatcher has 8 switches
    }
    
    initializeDefaultSwitches();
    
    SPDLOG_INFO("Switch {} created with manufacturer: {}, model: {}, {} switches", 
                deviceId, manufacturer, model, switchCount_.load());
}

Switch::~Switch() {
    stop();
}

bool Switch::initializeDevice() {
    // Initialize switch-specific properties
    setProperty("switchCount", switchCount_.load());
    setProperty("groupControlEnabled", groupControlEnabled_.load());
    
    // Initialize individual switch states
    std::lock_guard<std::mutex> lock(switchesMutex_);
    for (const auto& [name, info] : switches_) {
        setProperty("switch_" + name + "_state", static_cast<int>(info.currentState));
        setProperty("switch_" + name + "_type", static_cast<int>(info.type));
    }
    
    return true;
}

bool Switch::startDevice() {
    return true; // Switches don't need background threads
}

void Switch::stopDevice() {
    // Turn off all switches for safety
    std::lock_guard<std::mutex> lock(switchesMutex_);
    for (auto& [name, info] : switches_) {
        if (info.currentState == SwitchState::ON) {
            setSwitchState(name, SwitchState::OFF);
        }
    }
}

// ISwitch interface implementation
bool Switch::setSwitchState(const std::string& name, SwitchState state) {
    std::lock_guard<std::mutex> lock(switchesMutex_);
    
    auto it = switches_.find(name);
    if (it == switches_.end()) {
        SPDLOG_ERROR("Switch {} switch '{}' not found", getDeviceId(), name);
        return false;
    }
    
    SwitchInfo& switchInfo = it->second;
    
    // Check if switch supports the requested state
    if (switchInfo.type == SwitchType::MOMENTARY && state == SwitchState::ON) {
        // For momentary switches, execute a pulse instead
        return executePulse(name, 1000); // 1 second pulse
    }
    
    switchInfo.currentState = state;
    setProperty("switch_" + name + "_state", static_cast<int>(state));
    
    SPDLOG_INFO("Switch {} '{}' set to {}", getDeviceId(), name, 
                state == SwitchState::ON ? "ON" : "OFF");
    
    return executeSetState(name, state);
}

SwitchState Switch::getSwitchState(const std::string& name) const {
    std::lock_guard<std::mutex> lock(switchesMutex_);
    
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        return it->second.currentState;
    }
    
    return SwitchState::OFF;
}

std::vector<std::string> Switch::getSwitchNames() const {
    std::lock_guard<std::mutex> lock(switchesMutex_);
    
    std::vector<std::string> names;
    for (const auto& [name, info] : switches_) {
        names.push_back(name);
    }
    
    return names;
}

int Switch::getSwitchCount() const {
    return switchCount_;
}

// Extended functionality
bool Switch::addSwitch(const std::string& name, SwitchType type, SwitchState defaultState) {
    std::lock_guard<std::mutex> lock(switchesMutex_);
    
    if (switches_.find(name) != switches_.end()) {
        SPDLOG_WARN("Switch {} switch '{}' already exists", getDeviceId(), name);
        return false;
    }
    
    SwitchInfo info;
    info.name = name;
    info.type = type;
    info.currentState = defaultState;
    info.description = "User-defined switch";
    
    switches_[name] = info;
    
    setProperty("switch_" + name + "_state", static_cast<int>(defaultState));
    setProperty("switch_" + name + "_type", static_cast<int>(type));
    
    SPDLOG_INFO("Switch {} added switch '{}' of type {}", getDeviceId(), name, 
                type == SwitchType::TOGGLE ? "TOGGLE" : "MOMENTARY");
    
    return true;
}

bool Switch::removeSwitch(const std::string& name) {
    std::lock_guard<std::mutex> lock(switchesMutex_);
    
    auto it = switches_.find(name);
    if (it == switches_.end()) {
        return false;
    }
    
    // Turn off switch before removing
    if (it->second.currentState == SwitchState::ON) {
        executeSetState(name, SwitchState::OFF);
    }
    
    switches_.erase(it);
    
    SPDLOG_INFO("Switch {} removed switch '{}'", getDeviceId(), name);
    return true;
}

bool Switch::createSwitchGroup(const std::string& groupName, const std::vector<std::string>& switchNames) {
    std::lock_guard<std::mutex> lock(groupsMutex_);
    
    // Validate that all switches exist
    {
        std::lock_guard<std::mutex> switchLock(switchesMutex_);
        for (const auto& switchName : switchNames) {
            if (switches_.find(switchName) == switches_.end()) {
                SPDLOG_ERROR("Switch {} switch '{}' not found for group '{}'", 
                           getDeviceId(), switchName, groupName);
                return false;
            }
        }
    }
    
    switchGroups_[groupName] = switchNames;
    
    SPDLOG_INFO("Switch {} created group '{}' with {} switches", 
                getDeviceId(), groupName, switchNames.size());
    
    return true;
}

bool Switch::setGroupState(const std::string& groupName, SwitchState state) {
    std::lock_guard<std::mutex> lock(groupsMutex_);
    
    auto it = switchGroups_.find(groupName);
    if (it == switchGroups_.end()) {
        SPDLOG_ERROR("Switch {} group '{}' not found", getDeviceId(), groupName);
        return false;
    }
    
    bool allSuccess = true;
    for (const auto& switchName : it->second) {
        if (!setSwitchState(switchName, state)) {
            allSuccess = false;
        }
    }
    
    SPDLOG_INFO("Switch {} group '{}' set to {}", getDeviceId(), groupName,
                state == SwitchState::ON ? "ON" : "OFF");
    
    return allSuccess;
}

bool Switch::pulse(const std::string& name, int durationMs) {
    return executePulse(name, durationMs);
}

bool Switch::setAllSwitches(SwitchState state) {
    std::lock_guard<std::mutex> lock(switchesMutex_);
    
    bool allSuccess = true;
    for (const auto& [name, info] : switches_) {
        if (!setSwitchState(name, state)) {
            allSuccess = false;
        }
    }
    
    SPDLOG_INFO("Switch {} all switches set to {}", getDeviceId(),
                state == SwitchState::ON ? "ON" : "OFF");
    
    return allSuccess;
}

// Hardware abstraction methods (simulation)
bool Switch::executeSetState(const std::string& name, SwitchState state) {
    SPDLOG_DEBUG("Switch {} executing set state for '{}' to {}", 
                 getDeviceId(), name, state == SwitchState::ON ? "ON" : "OFF");
    
    // Simulate switch operation delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return true;
}

bool Switch::executePulse(const std::string& name, int durationMs) {
    SPDLOG_DEBUG("Switch {} executing pulse for '{}' duration {}ms", 
                 getDeviceId(), name, durationMs);
    
    // Simulate pulse operation
    std::thread([this, name, durationMs]() {
        // Turn on
        executeSetState(name, SwitchState::ON);
        
        // Wait for duration
        std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
        
        // Turn off
        executeSetState(name, SwitchState::OFF);
        
        // Update state
        std::lock_guard<std::mutex> lock(switchesMutex_);
        auto it = switches_.find(name);
        if (it != switches_.end()) {
            it->second.currentState = SwitchState::OFF;
            setProperty("switch_" + name + "_state", static_cast<int>(SwitchState::OFF));
        }
        
        SPDLOG_INFO("Switch {} pulse for '{}' completed", getDeviceId(), name);
    }).detach();
    
    return true;
}

void Switch::initializeDefaultSwitches() {
    std::lock_guard<std::mutex> lock(switchesMutex_);
    
    // Create default switches based on common astronomy power box configurations
    switches_["Power1"] = {"Power1", SwitchType::TOGGLE, SwitchState::OFF, "Main power output 1"};
    switches_["Power2"] = {"Power2", SwitchType::TOGGLE, SwitchState::OFF, "Main power output 2"};
    switches_["Power3"] = {"Power3", SwitchType::TOGGLE, SwitchState::OFF, "Main power output 3"};
    switches_["Power4"] = {"Power4", SwitchType::TOGGLE, SwitchState::OFF, "Main power output 4"};
    
    if (switchCount_ > 4) {
        switches_["Dew1"] = {"Dew1", SwitchType::TOGGLE, SwitchState::OFF, "Dew heater 1"};
        switches_["Dew2"] = {"Dew2", SwitchType::TOGGLE, SwitchState::OFF, "Dew heater 2"};
    }
    
    if (switchCount_ > 6) {
        switches_["USB1"] = {"USB1", SwitchType::TOGGLE, SwitchState::OFF, "USB hub 1"};
        switches_["USB2"] = {"USB2", SwitchType::TOGGLE, SwitchState::OFF, "USB hub 2"};
    }
}

bool Switch::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "SET_SWITCH_STATE") {
        std::string name = parameters.value("name", "");
        int stateInt = parameters.value("state", 0);
        SwitchState state = static_cast<SwitchState>(stateInt);
        result["success"] = setSwitchState(name, state);
        return true;
    }
    else if (command == "GET_SWITCH_STATE") {
        std::string name = parameters.value("name", "");
        SwitchState state = getSwitchState(name);
        result["state"] = static_cast<int>(state);
        result["success"] = true;
        return true;
    }
    else if (command == "PULSE") {
        std::string name = parameters.value("name", "");
        int duration = parameters.value("duration", 1000);
        result["success"] = pulse(name, duration);
        return true;
    }
    else if (command == "SET_ALL_SWITCHES") {
        int stateInt = parameters.value("state", 0);
        SwitchState state = static_cast<SwitchState>(stateInt);
        result["success"] = setAllSwitches(state);
        return true;
    }
    
    return false;
}

void Switch::updateDevice() {
    // Update switch states
    std::lock_guard<std::mutex> lock(switchesMutex_);
    
    json switchStates = json::object();
    for (const auto& [name, info] : switches_) {
        switchStates[name] = static_cast<int>(info.currentState);
    }
    setProperty("switchStates", switchStates);
}

std::vector<std::string> Switch::getCapabilities() const {
    return {
        "SET_SWITCH_STATE",
        "GET_SWITCH_STATE",
        "PULSE",
        "SET_ALL_SWITCHES"
    };
}

} // namespace device
} // namespace astrocomm

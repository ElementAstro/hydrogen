#include "switch.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <algorithm>

namespace hydrogen {
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
    auto createSwitch = [](const std::string& name, const std::string& desc) {
        SwitchInfo info;
        info.name = name;
        info.description = desc;
        info.type = SwitchType::TOGGLE;
        info.currentState = SwitchState::OFF;
        info.state = false;
        info.canWrite = true;
        info.canRead = true;
        return info;
    };

    switches_["Power1"] = createSwitch("Power1", "Main power output 1");
    switches_["Power2"] = createSwitch("Power2", "Main power output 2");
    switches_["Power3"] = createSwitch("Power3", "Main power output 3");
    switches_["Power4"] = createSwitch("Power4", "Main power output 4");

    if (switchCount_ > 4) {
        switches_["Dew1"] = createSwitch("Dew1", "Dew heater 1");
        switches_["Dew2"] = createSwitch("Dew2", "Dew heater 2");
    }

    if (switchCount_ > 6) {
        switches_["USB1"] = createSwitch("USB1", "USB hub 1");
        switches_["USB2"] = createSwitch("USB2", "USB hub 2");
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
        bool state = parameters.value("state", false);
        result["success"] = setAllSwitches(state);
        return true;
    }
    
    return false;
}

void Switch::updateDevice() {
    // Update switch states
    std::lock_guard<std::mutex> lock(switchInfoMutex_);

    json switchStates = json::object();
    for (const auto& [id, info] : switchInfo_) {
        switchStates[std::to_string(id)] = info.state;
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

// Missing IDevice interface method implementations
std::string Switch::getName() const {
    return getDeviceId();
}

std::string Switch::getDescription() const {
    return "Generic Switch Device";
}

std::string Switch::getDriverInfo() const {
    return "Hydrogen Switch Driver v1.0";
}

std::string Switch::getDriverVersion() const {
    return "1.0.0";
}

int Switch::getInterfaceVersion() const {
    return 1; // Interface version 1
}

std::vector<std::string> Switch::getSupportedActions() const {
    return {"setSwitchState", "getSwitchState", "pulse", "setAllSwitches"};
}

bool Switch::isConnecting() const {
    return false; // Not in connecting state
}

interfaces::DeviceState Switch::getDeviceState() const {
    if (isConnected()) {
        return interfaces::DeviceState::IDLE; // Switches are always idle when connected
    }
    return interfaces::DeviceState::UNKNOWN;
}

std::string Switch::action(const std::string& actionName, const std::string& actionParameters) {
    // Basic action handling
    (void)actionName;
    (void)actionParameters;
    return "OK";
}

void Switch::commandBlind(const std::string& command, bool raw) {
    // Basic command handling
    (void)command;
    (void)raw;
}

bool Switch::commandBool(const std::string& command, bool raw) {
    // Basic command handling
    (void)command;
    (void)raw;
    return true;
}

std::string Switch::commandString(const std::string& command, bool raw) {
    // Basic command handling
    (void)command;
    (void)raw;
    return "OK";
}

void Switch::setupDialog() {
    // No setup dialog for basic implementation
}

void Switch::run() {
    // Main switch device loop
    SPDLOG_INFO("Switch {} starting main loop", getDeviceId());

    while (isRunning()) {
        try {
            // Update switch state and handle any pending operations
            // Monitor switch states and handle any changes

            // Sleep for a short time to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        } catch (const std::exception& e) {
            SPDLOG_ERROR("Switch {} error in main loop: {}", getDeviceId(), e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    SPDLOG_INFO("Switch {} main loop stopped", getDeviceId());
}

// Missing virtual method implementations for ISwitch interface
int Switch::getNumSwitches() const {
    return static_cast<int>(switches_.size());
}

void Switch::setSwitchName(int index, const std::string& name) {
    std::string oldName = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(oldName);
    if (it != switches_.end()) {
        SwitchInfo info = it->second;
        switches_.erase(it);
        info.name = name;
        switches_[name] = info;
        setProperty("switch_" + std::to_string(index) + "_name", name);
    }
}

std::string Switch::getSwitchName(int index) const {
    std::string defaultName = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(defaultName);
    if (it != switches_.end()) {
        return it->second.name;
    }
    return defaultName;
}

bool Switch::setSwitchCount(int count) {
    if (count > 0) {
        switches_.clear();
        for (int i = 0; i < count; ++i) {
            std::string name = "Switch " + std::to_string(i + 1);
            SwitchInfo info;
            info.id = i;
            info.name = name;
            info.description = "Switch " + std::to_string(i + 1);
            info.type = SwitchType::TOGGLE;
            info.state = false;
            info.currentState = SwitchState::OFF;
            info.value = 0.0;
            info.minValue = 0.0;
            info.maxValue = 1.0;
            info.currentStateIndex = 0;
            info.canWrite = true;
            info.canRead = true;
            switches_[name] = info;
        }
        setProperty("switchCount", count);
        return true;
    }
    return false;
}

SwitchInfo Switch::getSwitchInfo(int index) const {
    std::string name = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        return it->second;
    }
    return SwitchInfo{};
}

bool Switch::setSwitchInfo(int index, const SwitchInfo& info) {
    std::string name = "Switch " + std::to_string(index + 1);
    switches_[name] = info;
    setProperty("switch_" + std::to_string(index) + "_info", "updated");
    return true;
}

std::vector<SwitchInfo> Switch::getAllSwitchInfo() const {
    std::vector<SwitchInfo> allInfo;
    for (const auto& pair : switches_) {
        allInfo.push_back(pair.second);
    }
    return allInfo;
}

bool Switch::setSwitchState(int index, bool state) {
    std::string name = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        it->second.state = state;
        it->second.currentState = state ? SwitchState::ON : SwitchState::OFF;
        setProperty("switch_" + std::to_string(index) + "_state", state);
        return executeSetSwitch(index, state);
    }
    return false;
}

bool Switch::getSwitchState(int index) const {
    std::string name = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        return it->second.currentState == SwitchState::ON;
    }
    return false;
}

bool Switch::setSwitchValue(int index, double value) {
    std::string name = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        it->second.value = value;
        setProperty("switch_" + std::to_string(index) + "_value", value);
        return executeSetSwitchValue(index, value);
    }
    return false;
}

double Switch::getSwitchValue(int index) const {
    std::string name = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        return it->second.value;
    }
    return 0.0;
}

bool Switch::setSwitchStateIndex(int index, int stateIndex) {
    if (index >= 0 && index < static_cast<int>(switches_.size())) {
        // Convert state index to boolean (0 = OFF, 1+ = ON)
        bool state = stateIndex > 0;
        return setSwitchState(index, state);
    }
    return false;
}

int Switch::getSwitchStateIndex(int index) const {
    if (index >= 0 && index < static_cast<int>(switches_.size())) {
        return getSwitchState(index) ? 1 : 0;
    }
    return 0;
}

bool Switch::pulseSwitch(int index, int duration) {
    if (index >= 0 && index < static_cast<int>(switches_.size())) {
        return executePulseSwitch(index, duration);
    }
    return false;
}

int Switch::getSwitchByName(const std::string& name) const {
    int index = 0;
    for (const auto& pair : switches_) {
        if (pair.second.name == name) {
            return index;
        }
        index++;
    }
    return -1;
}

bool Switch::setSwitchByName(const std::string& name, bool state) {
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        it->second.state = state;
        it->second.currentState = state ? SwitchState::ON : SwitchState::OFF;
        return executeSetSwitch(it->second.id, state);
    }
    return false;
}

bool Switch::setAllSwitches(bool state) {
    bool success = true;
    for (auto& pair : switches_) {
        pair.second.state = state;
        pair.second.currentState = state ? SwitchState::ON : SwitchState::OFF;
        if (!executeSetSwitch(pair.second.id, state)) {
            success = false;
        }
    }
    return success;
}

std::vector<bool> Switch::getAllSwitchesState() const {
    std::vector<bool> states;
    for (const auto& pair : switches_) {
        states.push_back(pair.second.currentState == SwitchState::ON);
    }
    return states;
}

bool Switch::setSwitchGroup(const std::vector<int>& indices, const std::string& groupName) {
    // Store switch group information directly as indices
    switchGroupsInt_[groupName] = indices;
    return true;
}

std::unordered_map<std::string, std::vector<int>> Switch::getSwitchGroups() const {
    return switchGroupsInt_;
}

bool Switch::setGroupState(const std::string& groupName, bool state) {
    auto it = switchGroupsInt_.find(groupName);
    if (it != switchGroupsInt_.end()) {
        bool success = true;
        for (int index : it->second) {
            if (!setSwitchState(index, state)) {
                success = false;
            }
        }
        return success;
    }
    return false;
}

bool Switch::getGroupState(const std::string& groupName) const {
    auto it = switchGroupsInt_.find(groupName);
    if (it != switchGroupsInt_.end()) {
        // Return true if all switches in group are ON
        for (int index : it->second) {
            if (!getSwitchState(index)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

bool Switch::setSwitchSequence(const std::vector<std::pair<int, int>>& sequence) {
    switchSequence_ = sequence;
    return true;
}

bool Switch::executeSwitchSequence() {
    for (const auto& step : switchSequence_) {
        int index = step.first;
        int duration = step.second;
        if (!pulseSwitch(index, duration)) {
            return false;
        }
        // Wait for the duration
        std::this_thread::sleep_for(std::chrono::milliseconds(duration));
    }
    return true;
}

bool Switch::setSwitchInterlock(int index, const std::vector<int>& interlockIndices) {
    if (index >= 0 && index < static_cast<int>(switches_.size())) {
        switchInterlocks_[index] = interlockIndices;
        return true;
    }
    return false;
}

std::vector<int> Switch::getSwitchInterlock(int index) const {
    auto it = switchInterlocks_.find(index);
    if (it != switchInterlocks_.end()) {
        return it->second;
    }
    return {};
}

bool Switch::setSwitchEnabled(int index, bool enabled) {
    std::string name = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        // Store in separate map since SwitchInfo doesn't have enabled field
        switchEnabled_[index] = enabled;
        setProperty("switch_" + std::to_string(index) + "_enabled", enabled);
        return true;
    }
    return false;
}

bool Switch::isSwitchEnabled(int index) const {
    auto it = switchEnabled_.find(index);
    if (it != switchEnabled_.end()) {
        return it->second;
    }
    return true; // Default to enabled
}

bool Switch::setSwitchProtection(int index, bool protected_) {
    std::string name = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        // Store in separate map since SwitchInfo doesn't have protected_ field
        switchProtected_[index] = protected_;
        setProperty("switch_" + std::to_string(index) + "_protected", protected_);
        return true;
    }
    return false;
}

bool Switch::isSwitchProtected(int index) const {
    auto it = switchProtected_.find(index);
    if (it != switchProtected_.end()) {
        return it->second;
    }
    return false; // Default to not protected
}

bool Switch::loadSwitchConfiguration(const std::string& filename) {
    // Load switch configuration from file
    SPDLOG_DEBUG("Switch {} loading configuration from '{}'", getDeviceId(), filename);
    (void)filename; // Suppress unused parameter warning
    return true; // Stub implementation
}

bool Switch::saveSwitchConfiguration(const std::string& filename) const {
    // Save switch configuration to file
    SPDLOG_DEBUG("Switch {} saving configuration to '{}'", getDeviceId(), filename);
    (void)filename; // Suppress unused parameter warning
    return true; // Stub implementation
}

bool Switch::resetAllSwitches() {
    return setAllSwitches(false);
}

json Switch::getSwitchStatistics() const {
    // Return switch statistics as JSON
    return json::object(); // Stub implementation
}

// Hardware abstraction methods
bool Switch::executeSetSwitch(int index, bool state) {
    // Execute hardware command to set switch state
    SPDLOG_DEBUG("Switch {} setting switch {} to {}", getDeviceId(), index, state ? "ON" : "OFF");
    (void)index; (void)state; // Suppress unused parameter warnings
    return true; // Stub implementation
}

bool Switch::executeSetSwitchValue(int index, double value) {
    // Execute hardware command to set switch value
    SPDLOG_DEBUG("Switch {} setting switch {} value to {}", getDeviceId(), index, value);
    (void)index; (void)value; // Suppress unused parameter warnings
    return true; // Stub implementation
}

bool Switch::executePulseSwitch(int index, int duration) {
    // Execute hardware command to pulse switch
    SPDLOG_DEBUG("Switch {} pulsing switch {} for {}ms", getDeviceId(), index, duration);

    // Turn switch on
    if (!executeSetSwitch(index, true)) {
        return false;
    }

    // Wait for duration
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));

    // Turn switch off
    return executeSetSwitch(index, false);
}

bool Switch::readSwitchState(int index) {
    // Read switch state from hardware
    std::string name = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        // Simulate reading from hardware
        return it->second.currentState == SwitchState::ON;
    }
    return false;
}

double Switch::readSwitchValue(int index) {
    // Read switch value from hardware
    std::string name = "Switch " + std::to_string(index + 1);
    auto it = switches_.find(name);
    if (it != switches_.end()) {
        // Simulate reading from hardware
        return it->second.value;
    }
    return 0.0;
}

} // namespace device
} // namespace hydrogen

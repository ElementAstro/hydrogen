#include "astrocomm/core/device_lifecycle.h"
#include "astrocomm/core/utils.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace astrocomm {
namespace core {

// StateTransition implementation
json StateTransition::toJson() const {
    return json{
        {"fromState", lifecycleStateToString(fromState)},
        {"toState", lifecycleStateToString(toState)},
        {"trigger", trigger},
        {"timestamp", getIsoTimestamp()},
        {"reason", reason}
    };
}

StateTransition StateTransition::fromJson(const json& j) {
    StateTransition transition;
    transition.fromState = stringToLifecycleState(j.value("fromState", "UNINITIALIZED"));
    transition.toState = stringToLifecycleState(j.value("toState", "UNINITIALIZED"));
    transition.trigger = j.value("trigger", "");
    transition.reason = j.value("reason", "");
    
    if (j.contains("timestamp")) {
        transition.timestamp = string_utils::parseIsoTimestamp(j["timestamp"]);
    }
    
    return transition;
}

// LifecycleEvent implementation
json LifecycleEvent::toJson() const {
    return json{
        {"deviceId", deviceId},
        {"previousState", lifecycleStateToString(previousState)},
        {"newState", lifecycleStateToString(newState)},
        {"trigger", trigger},
        {"reason", reason},
        {"timestamp", getIsoTimestamp()},
        {"metadata", metadata}
    };
}

LifecycleEvent LifecycleEvent::fromJson(const json& j) {
    LifecycleEvent event;
    event.deviceId = j.value("deviceId", "");
    event.previousState = stringToLifecycleState(j.value("previousState", "UNINITIALIZED"));
    event.newState = stringToLifecycleState(j.value("newState", "UNINITIALIZED"));
    event.trigger = j.value("trigger", "");
    event.reason = j.value("reason", "");
    event.metadata = j.value("metadata", json::object());
    
    if (j.contains("timestamp")) {
        event.timestamp = string_utils::parseIsoTimestamp(j["timestamp"]);
    }
    
    return event;
}

// DeviceLifecycleManager implementation
DeviceLifecycleManager::DeviceLifecycleManager() {
    initializeValidTransitions();
}

DeviceLifecycleManager::~DeviceLifecycleManager() = default;

void DeviceLifecycleManager::registerDevice(const std::string& deviceId, 
                                           DeviceLifecycleState initialState) {
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    
    if (deviceStates_.find(deviceId) != deviceStates_.end()) {
        return; // Already registered
    }
    
    DeviceLifecycleInfo info;
    info.currentState = initialState;
    info.registrationTime = std::chrono::system_clock::now();
    
    // Add initial state to history
    StateTransition initialTransition;
    initialTransition.fromState = DeviceLifecycleState::UNINITIALIZED;
    initialTransition.toState = initialState;
    initialTransition.trigger = "REGISTRATION";
    initialTransition.timestamp = info.registrationTime;
    initialTransition.reason = "Device registered";
    
    info.history.push_back(initialTransition);
    deviceStates_[deviceId] = info;
}

void DeviceLifecycleManager::unregisterDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    deviceStates_.erase(deviceId);
}

bool DeviceLifecycleManager::transitionTo(const std::string& deviceId, 
                                         DeviceLifecycleState newState,
                                         const std::string& trigger,
                                         const std::string& reason) {
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    
    auto it = deviceStates_.find(deviceId);
    if (it == deviceStates_.end()) {
        return false; // Device not registered
    }
    
    DeviceLifecycleState currentState = it->second.currentState;
    
    // Check if transition is valid
    if (strictValidation_ && !isTransitionAllowed(currentState, newState)) {
        return false;
    }
    
    // Create transition record
    StateTransition transition;
    transition.fromState = currentState;
    transition.toState = newState;
    transition.trigger = trigger;
    transition.reason = reason;
    transition.timestamp = std::chrono::system_clock::now();
    
    // Update state
    it->second.currentState = newState;
    addToHistory(deviceId, transition);
    
    // Create and notify lifecycle event
    LifecycleEvent event;
    event.deviceId = deviceId;
    event.previousState = currentState;
    event.newState = newState;
    event.trigger = trigger;
    event.reason = reason;
    event.timestamp = transition.timestamp;
    
    notifyStateChange(event);
    
    return true;
}

DeviceLifecycleState DeviceLifecycleManager::getCurrentState(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    
    auto it = deviceStates_.find(deviceId);
    if (it == deviceStates_.end()) {
        return DeviceLifecycleState::UNINITIALIZED;
    }
    
    return it->second.currentState;
}

bool DeviceLifecycleManager::isValidTransition(const std::string& /* deviceId */,
                                              DeviceLifecycleState fromState,
                                              DeviceLifecycleState toState) const {
    return isTransitionAllowed(fromState, toState);
}

std::vector<DeviceLifecycleState> DeviceLifecycleManager::getValidNextStates(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    
    auto it = deviceStates_.find(deviceId);
    if (it == deviceStates_.end()) {
        return {};
    }
    
    DeviceLifecycleState currentState = it->second.currentState;
    std::vector<DeviceLifecycleState> validStates;
    
    auto transitionIt = validTransitions_.find(currentState);
    if (transitionIt != validTransitions_.end()) {
        for (const auto& state : transitionIt->second) {
            validStates.push_back(state);
        }
    }
    
    return validStates;
}

std::vector<StateTransition> DeviceLifecycleManager::getStateHistory(const std::string& deviceId, 
                                                                    size_t maxEntries) const {
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    
    auto it = deviceStates_.find(deviceId);
    if (it == deviceStates_.end()) {
        return {};
    }
    
    const auto& history = it->second.history;
    if (maxEntries == 0 || history.size() <= maxEntries) {
        return history;
    }
    
    // Return the most recent entries
    return std::vector<StateTransition>(
        history.end() - maxEntries, history.end());
}

void DeviceLifecycleManager::setStateChangeCallback(
    std::function<void(const LifecycleEvent&)> callback) {
    stateChangeCallback_ = callback;
}

void DeviceLifecycleManager::forceErrorState(const std::string& deviceId, const std::string& errorReason) {
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    
    auto it = deviceStates_.find(deviceId);
    if (it == deviceStates_.end()) {
        return;
    }
    
    it->second.lastErrorReason = errorReason;
    
    // Force transition to error state (bypass validation)
    DeviceLifecycleState currentState = it->second.currentState;
    
    StateTransition transition;
    transition.fromState = currentState;
    transition.toState = DeviceLifecycleState::ERROR;
    transition.trigger = "FORCE_ERROR";
    transition.reason = errorReason;
    transition.timestamp = std::chrono::system_clock::now();
    
    it->second.currentState = DeviceLifecycleState::ERROR;
    addToHistory(deviceId, transition);
    
    // Notify state change
    LifecycleEvent event;
    event.deviceId = deviceId;
    event.previousState = currentState;
    event.newState = DeviceLifecycleState::ERROR;
    event.trigger = "FORCE_ERROR";
    event.reason = errorReason;
    event.timestamp = transition.timestamp;
    
    notifyStateChange(event);
}

bool DeviceLifecycleManager::attemptRecovery(const std::string& deviceId) {
    return transitionTo(deviceId, DeviceLifecycleState::RECOVERING, "RECOVERY_ATTEMPT", "Attempting automatic recovery");
}

std::vector<std::string> DeviceLifecycleManager::getDevicesInState(DeviceLifecycleState state) const {
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    
    std::vector<std::string> devices;
    for (const auto& [deviceId, info] : deviceStates_) {
        if (info.currentState == state) {
            devices.push_back(deviceId);
        }
    }
    
    return devices;
}

json DeviceLifecycleManager::getLifecycleStatistics() const {
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    
    std::unordered_map<DeviceLifecycleState, int> stateCounts;
    int totalTransitions = 0;
    
    for (const auto& [deviceId, info] : deviceStates_) {
        stateCounts[info.currentState]++;
        totalTransitions += info.history.size();
    }
    
    json stats = json::object();
    stats["totalDevices"] = deviceStates_.size();
    stats["totalTransitions"] = totalTransitions;
    
    json stateDistribution = json::object();
    for (const auto& [state, count] : stateCounts) {
        stateDistribution[lifecycleStateToString(state)] = count;
    }
    stats["stateDistribution"] = stateDistribution;
    
    return stats;
}

DeviceLifecycleManager& DeviceLifecycleManager::getInstance() {
    static DeviceLifecycleManager instance;
    return instance;
}

void DeviceLifecycleManager::setStrictValidation(bool enabled) {
    strictValidation_ = enabled;
}

void DeviceLifecycleManager::setMaxHistoryEntries(size_t maxEntries) {
    maxHistoryEntries_ = maxEntries;
}

// Helper function implementations
std::string lifecycleStateToString(DeviceLifecycleState state) {
    switch (state) {
        case DeviceLifecycleState::UNINITIALIZED: return "UNINITIALIZED";
        case DeviceLifecycleState::INITIALIZING: return "INITIALIZING";
        case DeviceLifecycleState::INITIALIZED: return "INITIALIZED";
        case DeviceLifecycleState::CONNECTING: return "CONNECTING";
        case DeviceLifecycleState::CONNECTED: return "CONNECTED";
        case DeviceLifecycleState::STARTING: return "STARTING";
        case DeviceLifecycleState::RUNNING: return "RUNNING";
        case DeviceLifecycleState::PAUSING: return "PAUSING";
        case DeviceLifecycleState::PAUSED: return "PAUSED";
        case DeviceLifecycleState::RESUMING: return "RESUMING";
        case DeviceLifecycleState::STOPPING: return "STOPPING";
        case DeviceLifecycleState::STOPPED: return "STOPPED";
        case DeviceLifecycleState::DISCONNECTING: return "DISCONNECTING";
        case DeviceLifecycleState::DISCONNECTED: return "DISCONNECTED";
        case DeviceLifecycleState::ERROR: return "ERROR";
        case DeviceLifecycleState::RECOVERING: return "RECOVERING";
        case DeviceLifecycleState::MAINTENANCE: return "MAINTENANCE";
        case DeviceLifecycleState::UPDATING: return "UPDATING";
        case DeviceLifecycleState::SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

DeviceLifecycleState stringToLifecycleState(const std::string& state) {
    if (state == "UNINITIALIZED") return DeviceLifecycleState::UNINITIALIZED;
    if (state == "INITIALIZING") return DeviceLifecycleState::INITIALIZING;
    if (state == "INITIALIZED") return DeviceLifecycleState::INITIALIZED;
    if (state == "CONNECTING") return DeviceLifecycleState::CONNECTING;
    if (state == "CONNECTED") return DeviceLifecycleState::CONNECTED;
    if (state == "STARTING") return DeviceLifecycleState::STARTING;
    if (state == "RUNNING") return DeviceLifecycleState::RUNNING;
    if (state == "PAUSING") return DeviceLifecycleState::PAUSING;
    if (state == "PAUSED") return DeviceLifecycleState::PAUSED;
    if (state == "RESUMING") return DeviceLifecycleState::RESUMING;
    if (state == "STOPPING") return DeviceLifecycleState::STOPPING;
    if (state == "STOPPED") return DeviceLifecycleState::STOPPED;
    if (state == "DISCONNECTING") return DeviceLifecycleState::DISCONNECTING;
    if (state == "DISCONNECTED") return DeviceLifecycleState::DISCONNECTED;
    if (state == "ERROR") return DeviceLifecycleState::ERROR;
    if (state == "RECOVERING") return DeviceLifecycleState::RECOVERING;
    if (state == "MAINTENANCE") return DeviceLifecycleState::MAINTENANCE;
    if (state == "UPDATING") return DeviceLifecycleState::UPDATING;
    if (state == "SHUTDOWN") return DeviceLifecycleState::SHUTDOWN;
    return DeviceLifecycleState::UNINITIALIZED;
}

bool isErrorState(DeviceLifecycleState state) {
    return state == DeviceLifecycleState::ERROR;
}

bool isTransitionalState(DeviceLifecycleState state) {
    return state == DeviceLifecycleState::INITIALIZING ||
           state == DeviceLifecycleState::CONNECTING ||
           state == DeviceLifecycleState::STARTING ||
           state == DeviceLifecycleState::PAUSING ||
           state == DeviceLifecycleState::RESUMING ||
           state == DeviceLifecycleState::STOPPING ||
           state == DeviceLifecycleState::DISCONNECTING ||
           state == DeviceLifecycleState::RECOVERING ||
           state == DeviceLifecycleState::UPDATING;
}

bool isStableState(DeviceLifecycleState state) {
    return !isTransitionalState(state) && !isErrorState(state);
}

void DeviceLifecycleManager::initializeValidTransitions() {
    // Define valid state transitions
    validTransitions_[DeviceLifecycleState::UNINITIALIZED] = {
        DeviceLifecycleState::INITIALIZING,
        DeviceLifecycleState::ERROR
    };
    
    validTransitions_[DeviceLifecycleState::INITIALIZING] = {
        DeviceLifecycleState::INITIALIZED,
        DeviceLifecycleState::ERROR
    };
    
    validTransitions_[DeviceLifecycleState::INITIALIZED] = {
        DeviceLifecycleState::CONNECTING,
        DeviceLifecycleState::ERROR,
        DeviceLifecycleState::SHUTDOWN
    };
    
    validTransitions_[DeviceLifecycleState::CONNECTING] = {
        DeviceLifecycleState::CONNECTED,
        DeviceLifecycleState::ERROR,
        DeviceLifecycleState::DISCONNECTED
    };
    
    validTransitions_[DeviceLifecycleState::CONNECTED] = {
        DeviceLifecycleState::STARTING,
        DeviceLifecycleState::DISCONNECTING,
        DeviceLifecycleState::ERROR,
        DeviceLifecycleState::MAINTENANCE
    };
    
    validTransitions_[DeviceLifecycleState::STARTING] = {
        DeviceLifecycleState::RUNNING,
        DeviceLifecycleState::ERROR,
        DeviceLifecycleState::STOPPING
    };
    
    validTransitions_[DeviceLifecycleState::RUNNING] = {
        DeviceLifecycleState::PAUSING,
        DeviceLifecycleState::STOPPING,
        DeviceLifecycleState::ERROR,
        DeviceLifecycleState::MAINTENANCE,
        DeviceLifecycleState::UPDATING
    };
    
    validTransitions_[DeviceLifecycleState::PAUSING] = {
        DeviceLifecycleState::PAUSED,
        DeviceLifecycleState::ERROR
    };
    
    validTransitions_[DeviceLifecycleState::PAUSED] = {
        DeviceLifecycleState::RESUMING,
        DeviceLifecycleState::STOPPING,
        DeviceLifecycleState::ERROR
    };
    
    validTransitions_[DeviceLifecycleState::RESUMING] = {
        DeviceLifecycleState::RUNNING,
        DeviceLifecycleState::ERROR
    };
    
    validTransitions_[DeviceLifecycleState::STOPPING] = {
        DeviceLifecycleState::STOPPED,
        DeviceLifecycleState::ERROR
    };
    
    validTransitions_[DeviceLifecycleState::STOPPED] = {
        DeviceLifecycleState::STARTING,
        DeviceLifecycleState::DISCONNECTING,
        DeviceLifecycleState::ERROR,
        DeviceLifecycleState::MAINTENANCE
    };
    
    validTransitions_[DeviceLifecycleState::DISCONNECTING] = {
        DeviceLifecycleState::DISCONNECTED,
        DeviceLifecycleState::ERROR
    };
    
    validTransitions_[DeviceLifecycleState::DISCONNECTED] = {
        DeviceLifecycleState::CONNECTING,
        DeviceLifecycleState::SHUTDOWN,
        DeviceLifecycleState::ERROR
    };
    
    validTransitions_[DeviceLifecycleState::ERROR] = {
        DeviceLifecycleState::RECOVERING,
        DeviceLifecycleState::SHUTDOWN,
        DeviceLifecycleState::MAINTENANCE
    };
    
    validTransitions_[DeviceLifecycleState::RECOVERING] = {
        DeviceLifecycleState::INITIALIZED,
        DeviceLifecycleState::CONNECTED,
        DeviceLifecycleState::ERROR,
        DeviceLifecycleState::SHUTDOWN
    };
    
    validTransitions_[DeviceLifecycleState::MAINTENANCE] = {
        DeviceLifecycleState::CONNECTED,
        DeviceLifecycleState::STOPPED,
        DeviceLifecycleState::ERROR,
        DeviceLifecycleState::SHUTDOWN
    };
    
    validTransitions_[DeviceLifecycleState::UPDATING] = {
        DeviceLifecycleState::RUNNING,
        DeviceLifecycleState::ERROR,
        DeviceLifecycleState::MAINTENANCE
    };
    
    // SHUTDOWN is terminal state - no transitions allowed
    validTransitions_[DeviceLifecycleState::SHUTDOWN] = {};
}

bool DeviceLifecycleManager::isTransitionAllowed(DeviceLifecycleState from, DeviceLifecycleState to) const {
    auto it = validTransitions_.find(from);
    if (it == validTransitions_.end()) {
        return false;
    }
    
    return it->second.find(to) != it->second.end();
}

void DeviceLifecycleManager::addToHistory(const std::string& deviceId, const StateTransition& transition) {
    auto& info = deviceStates_[deviceId];
    info.history.push_back(transition);
    trimHistory(info.history);
}

void DeviceLifecycleManager::notifyStateChange(const LifecycleEvent& event) {
    if (stateChangeCallback_) {
        stateChangeCallback_(event);
    }
}

void DeviceLifecycleManager::trimHistory(std::vector<StateTransition>& history) {
    if (history.size() > maxHistoryEntries_) {
        size_t toRemove = history.size() - maxHistoryEntries_;
        history.erase(history.begin(), history.begin() + toRemove);
    }
}

bool DeviceLifecycleManager::saveLifecycleData(const std::string& filename) const {
    try {
        std::lock_guard<std::mutex> lock(lifecycleMutex_);

        json data = json::object();
        data["devices"] = json::object();

        for (const auto& [deviceId, info] : deviceStates_) {
            json deviceData = json::object();
            deviceData["currentState"] = lifecycleStateToString(info.currentState);
            deviceData["registrationTime"] = getIsoTimestamp();
            deviceData["lastErrorReason"] = info.lastErrorReason;

            json historyArray = json::array();
            for (const auto& transition : info.history) {
                historyArray.push_back(transition.toJson());
            }
            deviceData["history"] = historyArray;

            data["devices"][deviceId] = deviceData;
        }

        data["configuration"] = json::object();
        data["configuration"]["strictValidation"] = strictValidation_.load();
        data["configuration"]["maxHistoryEntries"] = maxHistoryEntries_.load();

        std::ofstream file(filename);
        file << data.dump(2);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool DeviceLifecycleManager::loadLifecycleData(const std::string& filename) {
    try {
        std::ifstream file(filename);
        json data;
        file >> data;

        std::lock_guard<std::mutex> lock(lifecycleMutex_);
        deviceStates_.clear();

        if (data.contains("devices")) {
            for (const auto& [deviceId, deviceData] : data["devices"].items()) {
                DeviceLifecycleInfo info;
                info.currentState = stringToLifecycleState(deviceData.value("currentState", "UNINITIALIZED"));
                info.lastErrorReason = deviceData.value("lastErrorReason", "");

                if (deviceData.contains("registrationTime")) {
                    info.registrationTime = string_utils::parseIsoTimestamp(deviceData["registrationTime"]);
                }

                if (deviceData.contains("history") && deviceData["history"].is_array()) {
                    for (const auto& transitionJson : deviceData["history"]) {
                        info.history.push_back(StateTransition::fromJson(transitionJson));
                    }
                }

                deviceStates_[deviceId] = info;
            }
        }

        if (data.contains("configuration")) {
            const auto& config = data["configuration"];
            strictValidation_ = config.value("strictValidation", true);
            maxHistoryEntries_ = config.value("maxHistoryEntries", 100);
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace core
} // namespace astrocomm

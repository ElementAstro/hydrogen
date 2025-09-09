#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Comprehensive device lifecycle states
 */
enum class DeviceLifecycleState {
  UNINITIALIZED, // Device object created but not initialized
  INITIALIZING,  // Device is being initialized
  INITIALIZED,   // Device initialized but not connected
  CONNECTING,    // Device is attempting to connect
  CONNECTED,     // Device connected but not started
  STARTING,      // Device is starting up
  RUNNING,       // Device is running and operational
  PAUSING,       // Device is being paused
  PAUSED,        // Device is paused
  RESUMING,      // Device is resuming from pause
  STOPPING,      // Device is stopping
  STOPPED,       // Device stopped but still connected
  DISCONNECTING, // Device is disconnecting
  DISCONNECTED,  // Device disconnected
  ERROR,         // Device in error state
  RECOVERING,    // Device is recovering from error
  MAINTENANCE,   // Device in maintenance mode
  UPDATING,      // Device firmware/software updating
  SHUTDOWN       // Device permanently shut down
};

/**
 * @brief State transition information
 */
struct StateTransition {
  DeviceLifecycleState fromState;
  DeviceLifecycleState toState;
  std::string trigger;
  std::chrono::system_clock::time_point timestamp;
  std::string reason;

  json toJson() const;
  static StateTransition fromJson(const json &j);
};

/**
 * @brief Device lifecycle event information
 */
struct LifecycleEvent {
  std::string deviceId;
  DeviceLifecycleState previousState;
  DeviceLifecycleState newState;
  std::string trigger;
  std::string reason;
  std::chrono::system_clock::time_point timestamp;
  json metadata;

  json toJson() const;
  static LifecycleEvent fromJson(const json &j);
};

/**
 * @brief Interface for device lifecycle management
 */
class IDeviceLifecycleManager {
public:
  virtual ~IDeviceLifecycleManager() = default;

  /**
   * @brief Register a device for lifecycle management
   * @param deviceId Device identifier
   * @param initialState Initial state of the device
   */
  virtual void registerDevice(const std::string &deviceId,
                              DeviceLifecycleState initialState =
                                  DeviceLifecycleState::UNINITIALIZED) = 0;

  /**
   * @brief Unregister a device from lifecycle management
   * @param deviceId Device identifier
   */
  virtual void unregisterDevice(const std::string &deviceId) = 0;

  /**
   * @brief Attempt to transition device to new state
   * @param deviceId Device identifier
   * @param newState Target state
   * @param trigger What triggered this transition
   * @param reason Optional reason for the transition
   * @return True if transition is valid and allowed
   */
  virtual bool transitionTo(const std::string &deviceId,
                            DeviceLifecycleState newState,
                            const std::string &trigger,
                            const std::string &reason = "") = 0;

  /**
   * @brief Get current state of a device
   * @param deviceId Device identifier
   * @return Current lifecycle state
   */
  virtual DeviceLifecycleState
  getCurrentState(const std::string &deviceId) const = 0;

  /**
   * @brief Check if a state transition is valid
   * @param deviceId Device identifier
   * @param fromState Source state
   * @param toState Target state
   * @return True if transition is valid
   */
  virtual bool isValidTransition(const std::string &deviceId,
                                 DeviceLifecycleState fromState,
                                 DeviceLifecycleState toState) const = 0;

  /**
   * @brief Get valid next states from current state
   * @param deviceId Device identifier
   * @return Vector of valid next states
   */
  virtual std::vector<DeviceLifecycleState>
  getValidNextStates(const std::string &deviceId) const = 0;

  /**
   * @brief Get state transition history for a device
   * @param deviceId Device identifier
   * @param maxEntries Maximum number of entries to return (0 = all)
   * @return Vector of state transitions
   */
  virtual std::vector<StateTransition>
  getStateHistory(const std::string &deviceId, size_t maxEntries = 0) const = 0;

  /**
   * @brief Register callback for state changes
   * @param callback Function to call when state changes
   */
  virtual void setStateChangeCallback(
      std::function<void(const LifecycleEvent &)> callback) = 0;

  /**
   * @brief Force device into error state
   * @param deviceId Device identifier
   * @param errorReason Reason for error state
   */
  virtual void forceErrorState(const std::string &deviceId,
                               const std::string &errorReason) = 0;

  /**
   * @brief Attempt to recover device from error state
   * @param deviceId Device identifier
   * @return True if recovery initiated successfully
   */
  virtual bool attemptRecovery(const std::string &deviceId) = 0;

  /**
   * @brief Get all devices in a specific state
   * @param state Target state
   * @return Vector of device IDs in that state
   */
  virtual std::vector<std::string>
  getDevicesInState(DeviceLifecycleState state) const = 0;

  /**
   * @brief Get lifecycle statistics
   * @return JSON object with statistics
   */
  virtual json getLifecycleStatistics() const = 0;
};

/**
 * @brief Concrete implementation of device lifecycle manager
 */
class DeviceLifecycleManager : public IDeviceLifecycleManager {
public:
  DeviceLifecycleManager();
  virtual ~DeviceLifecycleManager();

  // IDeviceLifecycleManager implementation
  void registerDevice(const std::string &deviceId,
                      DeviceLifecycleState initialState =
                          DeviceLifecycleState::UNINITIALIZED) override;
  void unregisterDevice(const std::string &deviceId) override;
  bool transitionTo(const std::string &deviceId, DeviceLifecycleState newState,
                    const std::string &trigger,
                    const std::string &reason = "") override;
  DeviceLifecycleState
  getCurrentState(const std::string &deviceId) const override;
  bool isValidTransition(const std::string &deviceId,
                         DeviceLifecycleState fromState,
                         DeviceLifecycleState toState) const override;
  std::vector<DeviceLifecycleState>
  getValidNextStates(const std::string &deviceId) const override;
  std::vector<StateTransition>
  getStateHistory(const std::string &deviceId,
                  size_t maxEntries = 0) const override;
  void setStateChangeCallback(
      std::function<void(const LifecycleEvent &)> callback) override;
  void forceErrorState(const std::string &deviceId,
                       const std::string &errorReason) override;
  bool attemptRecovery(const std::string &deviceId) override;
  std::vector<std::string>
  getDevicesInState(DeviceLifecycleState state) const override;
  json getLifecycleStatistics() const override;

  /**
   * @brief Get singleton instance
   * @return Reference to singleton instance
   */
  static DeviceLifecycleManager &getInstance();

  /**
   * @brief Enable/disable state validation
   * @param enabled Whether to enable strict state validation
   */
  void setStrictValidation(bool enabled);

  /**
   * @brief Set maximum history entries per device
   * @param maxEntries Maximum number of history entries to keep
   */
  void setMaxHistoryEntries(size_t maxEntries);

  /**
   * @brief Save lifecycle data to file
   * @param filename File to save to
   * @return True if successful
   */
  bool saveLifecycleData(const std::string &filename) const;

  /**
   * @brief Load lifecycle data from file
   * @param filename File to load from
   * @return True if successful
   */
  bool loadLifecycleData(const std::string &filename);

private:
  struct DeviceLifecycleInfo {
    DeviceLifecycleState currentState;
    std::vector<StateTransition> history;
    std::chrono::system_clock::time_point registrationTime;
    std::string lastErrorReason;
  };

  mutable std::mutex lifecycleMutex_;
  std::unordered_map<std::string, DeviceLifecycleInfo> deviceStates_;

  // Configuration
  std::atomic<bool> strictValidation_{true};
  std::atomic<size_t> maxHistoryEntries_{100};

  // Callback for state changes
  std::function<void(const LifecycleEvent &)> stateChangeCallback_;

  // Valid state transitions map
  std::unordered_map<DeviceLifecycleState,
                     std::unordered_set<DeviceLifecycleState>>
      validTransitions_;

  // Helper methods
  void initializeValidTransitions();
  bool isTransitionAllowed(DeviceLifecycleState from,
                           DeviceLifecycleState to) const;
  void addToHistory(const std::string &deviceId,
                    const StateTransition &transition);
  void notifyStateChange(const LifecycleEvent &event);
  void trimHistory(std::vector<StateTransition> &history);
};

/**
 * @brief Helper functions for lifecycle states
 */
std::string lifecycleStateToString(DeviceLifecycleState state);
DeviceLifecycleState stringToLifecycleState(const std::string &state);
bool isErrorState(DeviceLifecycleState state);
bool isTransitionalState(DeviceLifecycleState state);
bool isStableState(DeviceLifecycleState state);

} // namespace core
} // namespace hydrogen

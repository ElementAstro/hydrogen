#pragma once

#include "device/device_base.h"

#include <memory>
#include <string>
#include <vector>

namespace astrocomm {

/**
 * @brief Represents a switch device, potentially containing multiple individual
 * switches.
 *
 * This class models a device that can have one or more switches, each with its
 * own type (toggle, momentary, button) and state (ON/OFF). It supports grouping
 * switches for simultaneous control and handling commands via the base class
 * communication mechanism. It inherits from DeviceBase to provide standard
 * device communication capabilities.
 */
class Switch : public DeviceBase {
public:
  /**
   * @brief Defines the behavior type of an individual switch.
   */
  enum class SwitchType {
    TOGGLE,    /**< A standard switch that stays in the state it's set to (ON or
                  OFF). */
    MOMENTARY, /**< A switch that automatically returns to its default state
                  after a short delay when activated. */
    BUTTON     /**< A switch, typically ON, that automatically returns to its
                  default state (usually OFF) immediately after being activated. */
  };

  /**
   * @brief Defines the possible states of an individual switch.
   */
  enum class SwitchState {
    OFF, /**< The switch is in the OFF or deactivated state. */
    ON   /**< The switch is in the ON or activated state. */
  };

  /**
   * @brief Constructs a new Switch device instance.
   * @param deviceId A unique identifier for this switch device.
   * @param manufacturer The manufacturer name (defaults to "Generic").
   * @param model The model name (defaults to "Multi-Switch").
   */
  Switch(const std::string &deviceId,
         const std::string &manufacturer = "Generic",
         const std::string &model = "Multi-Switch");

  /**
   * @brief Destructor. Cleans up resources.
   */
  ~Switch() override;

  /**
   * @brief Starts the switch device, initializing communication and setting
   * initial states.
   * @return True if the device started successfully, false otherwise.
   * @override
   */
  bool start() override;

  /**
   * @brief Stops the switch device, disconnecting and cleaning up.
   * @override
   */
  void stop() override;

  /**
   * @brief Adds a new individual switch to this device.
   * @param name The unique name for this switch within the device.
   * @param type The behavior type of the switch (default: TOGGLE).
   * @param defaultState The initial and momentary-return state of the switch
   * (default: OFF).
   */
  void addSwitch(const std::string &name, SwitchType type = SwitchType::TOGGLE,
                 SwitchState defaultState = SwitchState::OFF);

  /**
   * @brief Sets the state of a specific switch.
   * @param name The name of the switch to modify.
   * @param state The desired new state (ON or OFF).
   * @return True if the state was set successfully (switch exists), false
   * otherwise.
   */
  bool setState(const std::string &name, SwitchState state);

  /**
   * @brief Gets the current state of a specific switch.
   * @param name The name of the switch to query.
   * @return The current SwitchState (ON or OFF).
   * @throw std::runtime_error if the switch with the given name does not exist.
   */
  SwitchState getState(const std::string &name) const;

  /**
   * @brief Gets the names of all switches configured on this device.
   * @return A vector of strings containing the names of all switches.
   */
  std::vector<std::string> getSwitchNames() const;

  /**
   * @brief Creates a named group of existing switches.
   * Groups allow setting the state of multiple switches with a single command.
   * @param groupName The unique name for the new group.
   * @param switches A vector of names of the switches to include in this group.
   * @warning If any switch name in the vector does not exist, the group
   * creation might fail silently or partially.
   */
  void createSwitchGroup(const std::string &groupName,
                         const std::vector<std::string> &switches);

  /**
   * @brief Sets the state of all switches within a specified group.
   * @param groupName The name of the group to modify.
   * @param state The desired new state (ON or OFF) for all switches in the
   * group.
   * @return True if the group exists and the state was attempted to be set for
   * all switches, false if the group doesn't exist.
   * @note This function might return true even if setting the state for
   * individual switches within the group failed. Check logs for details.
   */
  bool setGroupState(const std::string &groupName, SwitchState state);

protected:
  /**
   * @brief Handles the "SET_SWITCH" command message.
   * @param cmd The incoming command message.
   * @param response The response message to populate.
   */
  void handleSetStateCommand(const CommandMessage &cmd,
                             ResponseMessage &response);
  /**
   * @brief Handles the "GET_SWITCH" command message.
   * @param cmd The incoming command message.
   * @param response The response message to populate.
   */
  void handleGetStateCommand(const CommandMessage &cmd,
                             ResponseMessage &response);
  /**
   * @brief Handles the "SET_GROUP" command message.
   * @param cmd The incoming command message.
   * @param response The response message to populate.
   */
  void handleSetGroupCommand(const CommandMessage &cmd,
                             ResponseMessage &response);
  /**
   * @brief Handles the "PULSE_SWITCH" command message.
   * Sets a switch to the opposite of its current state for a specified
   * duration, then reverts it.
   * @param cmd The incoming command message containing 'switch' and 'duration'
   * (ms).
   * @param response The response message to populate.
   */
  void handlePulseCommand(const CommandMessage &cmd, ResponseMessage &response);

  /**
   * @brief Sends an event notification when a switch's state changes.
   * @param switchName The name of the switch that changed.
   * @param newState The new state of the switch.
   * @param oldState The previous state of the switch.
   */
  void sendSwitchStateChangedEvent(const std::string &switchName,
                                   SwitchState newState, SwitchState oldState);

  /**
   * @brief Handles the automatic state restoration for MOMENTARY or BUTTON
   * switches. This is typically called internally after a delay or immediately.
   * @param switchName The name of the switch to restore.
   * @param originalState The state to restore the switch to (usually its
   * default state).
   */
  void handleMomentaryRestore(const std::string &switchName,
                              SwitchState originalState);

private:
  /**
   * @brief Forward declaration for the implementation class (PIMPL pattern).
   * This hides the internal implementation details from the header file.
   */
  class SwitchImpl;
  /**
   * @brief Unique pointer to the implementation object.
   */
  std::unique_ptr<SwitchImpl> impl_;

  /**
   * @brief Converts a SwitchState enum to its string representation.
   * @param state The enum value.
   * @return The string representation ("ON", "OFF", or "UNKNOWN").
   */
  std::string switchStateToString(SwitchState state) const;
  /**
   * @brief Converts a string representation to a SwitchState enum.
   * Case-insensitive comparison ("ON", "OFF").
   * @param stateStr The string to convert.
   * @return The corresponding SwitchState enum value.
   * @throw std::invalid_argument if the string is not a valid state.
   */
  SwitchState stringToSwitchState(const std::string &stateStr) const;
  /**
   * @brief Converts a SwitchType enum to its string representation.
   * @param type The enum value.
   * @return The string representation ("TOGGLE", "MOMENTARY", "BUTTON", or
   * "UNKNOWN").
   */
  std::string switchTypeToString(SwitchType type) const;
};

} // namespace astrocomm
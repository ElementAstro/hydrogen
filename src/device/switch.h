#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"

#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace device {

using json = nlohmann::json;

/**
 * @brief Switch type enumeration
 */
enum class SwitchType {
  TOGGLE,       // Toggle switch (on/off)
  MOMENTARY,    // Momentary switch (pulse)
  MULTI_STATE,  // Multi-state switch
  ANALOG        // Analog control
};

/**
 * @brief Switch state enumeration
 */
enum class SwitchState {
  OFF,          // Switch is off
  ON            // Switch is on
};

/**
 * @brief Switch information structure
 */
struct SwitchInfo {
  int id;                      // Switch ID
  std::string name;           // Switch name
  std::string description;    // Switch description
  SwitchType type;            // Switch type
  bool state;                 // Current state (for toggle switches) - legacy
  SwitchState currentState;   // Current state (using enum)
  double value;               // Current value (for analog switches)
  double minValue;            // Minimum value (for analog switches)
  double maxValue;            // Maximum value (for analog switches)
  std::vector<std::string> states; // Available states (for multi-state switches)
  int currentStateIndex;      // Current state index (for multi-state switches)
  bool canWrite;              // Whether switch can be controlled
  bool canRead;               // Whether switch state can be read
};

/**
 * @brief 开关设备实�? *
 * 基于新架构的开关设备实现，提供完整的开关控制功能�? * 支持多种类型的开关设备，包括继电器、电源开关、USB开关等�? */
class Switch : public core::ModernDeviceBase {
public:
  /**
   * @brief Constructor
   * @param deviceId Device identifier
   * @param manufacturer Manufacturer name
   * @param model Model name
   */
  Switch(const std::string &deviceId, 
         const std::string &manufacturer = "Pegasus",
         const std::string &model = "Ultimate Powerbox");

  /**
   * @brief Virtual destructor
   */
  virtual ~Switch();

  /**
   * @brief 获取设备类型名称
   */
  static std::string getDeviceTypeName() { return "SWITCH"; }

  /**
   * @brief 获取支持的制造商列表
   */
  static std::vector<std::string> getSupportedManufacturers() {
    return {"Pegasus", "Lunatico", "AAG", "Optec", "Generic"};
  }

  /**
   * @brief 获取支持的型号列�?   */
  static std::vector<std::string> getSupportedModels(const std::string& manufacturer) {
    if (manufacturer == "Pegasus") return {"Ultimate Powerbox", "Pocket Powerbox", "FocusCube"};
    if (manufacturer == "Lunatico") return {"Seletek", "Armadillo", "Platypus"};
    if (manufacturer == "AAG") return {"CloudWatcher", "SkyAlert"};
    if (manufacturer == "Optec") return {"Gemini", "IFW"};
    return {"Generic Switch"};
  }

  // ==== 向后兼容接口 ====

  /**
   * @brief Get number of switches (向后兼容)
   */
  virtual int getNumSwitches() const;

  /**
   * @brief Set switch name (向后兼容)
   */
  virtual void setSwitchName(int switchId, const std::string& name);

  /**
   * @brief Get switch name (向后兼容)
   */
  virtual std::string getSwitchName(int switchId) const;

  // ==== 开关控制接�?====

  /**
   * @brief Get number of switches
   */
  virtual int getSwitchCount() const;

  /**
   * @brief Set switch count
   */
  virtual bool setSwitchCount(int count);

  /**
   * @brief Get switch information
   */
  virtual SwitchInfo getSwitchInfo(int switchId) const;

  /**
   * @brief Set switch information
   */
  virtual bool setSwitchInfo(int switchId, const SwitchInfo& info);

  /**
   * @brief Get all switch information
   */
  virtual std::vector<SwitchInfo> getAllSwitchInfo() const;

  /**
   * @brief Set switch state (for toggle switches)
   */
  virtual bool setSwitchState(int switchId, bool state);

  /**
   * @brief Get switch state (for toggle switches)
   */
  virtual bool getSwitchState(int switchId) const;

  /**
   * @brief Set switch value (for analog switches)
   */
  virtual bool setSwitchValue(int switchId, double value);

  /**
   * @brief Get switch value (for analog switches)
   */
  virtual double getSwitchValue(int switchId) const;

  /**
   * @brief Set multi-state switch
   */
  virtual bool setSwitchStateIndex(int switchId, int stateIndex);

  /**
   * @brief Get multi-state switch index
   */
  virtual int getSwitchStateIndex(int switchId) const;

  /**
   * @brief Pulse switch (for momentary switches)
   */
  virtual bool pulseSwitch(int switchId, int duration = 1000);

  /**
   * @brief Get switch by name
   */
  virtual int getSwitchByName(const std::string& name) const;

  /**
   * @brief Set switch by name
   */
  virtual bool setSwitchByName(const std::string& name, bool state);

  /**
   * @brief Set switch state by name (using SwitchState enum)
   */
  virtual bool setSwitchState(const std::string& name, SwitchState state);

  /**
   * @brief Get switch state by name (using SwitchState enum)
   */
  virtual SwitchState getSwitchState(const std::string& name) const;

  /**
   * @brief Set all switches state
   */
  virtual bool setAllSwitches(bool state);

  /**
   * @brief Set all switches state (using SwitchState enum)
   */
  virtual bool setAllSwitches(SwitchState state);

  /**
   * @brief Get all switches state
   */
  virtual std::vector<bool> getAllSwitchesState() const;

  // ==== 高级功能接口 ====

  /**
   * @brief Set switch group
   */
  virtual bool setSwitchGroup(const std::vector<int>& switchIds, const std::string& groupName);

  /**
   * @brief Get switch groups
   */
  virtual std::unordered_map<std::string, std::vector<int>> getSwitchGroups() const;

  /**
   * @brief Set group state
   */
  virtual bool setGroupState(const std::string& groupName, bool state);

  /**
   * @brief Get group state
   */
  virtual bool getGroupState(const std::string& groupName) const;

  /**
   * @brief Set switch sequence
   */
  virtual bool setSwitchSequence(const std::vector<std::pair<int, int>>& sequence);

  /**
   * @brief Execute switch sequence
   */
  virtual bool executeSwitchSequence();

  /**
   * @brief Set switch interlock
   */
  virtual bool setSwitchInterlock(int switchId, const std::vector<int>& interlockSwitches);

  /**
   * @brief Get switch interlock
   */
  virtual std::vector<int> getSwitchInterlock(int switchId) const;

  /**
   * @brief Enable/disable switch
   */
  virtual bool setSwitchEnabled(int switchId, bool enabled);

  /**
   * @brief Check if switch is enabled
   */
  virtual bool isSwitchEnabled(int switchId) const;

  /**
   * @brief Set switch protection
   */
  virtual bool setSwitchProtection(int switchId, bool protected_);

  /**
   * @brief Check if switch is protected
   */
  virtual bool isSwitchProtected(int switchId) const;

  /**
   * @brief Load switch configuration
   */
  virtual bool loadSwitchConfiguration(const std::string& filename);

  /**
   * @brief Save switch configuration
   */
  virtual bool saveSwitchConfiguration(const std::string& filename) const;

  /**
   * @brief Reset all switches to default state
   */
  virtual bool resetAllSwitches();

  /**
   * @brief Get switch usage statistics
   */
  virtual json getSwitchStatistics() const;

  /**
   * @brief Get device capabilities
   */
  std::vector<std::string> getCapabilities() const override;

  // Extended functionality methods
  virtual bool addSwitch(const std::string& name, SwitchType type, SwitchState defaultState);
  virtual bool removeSwitch(const std::string& name);
  virtual bool pulse(const std::string& name, int durationMs);
  virtual std::vector<std::string> getSwitchNames() const;
  virtual bool createSwitchGroup(const std::string& groupName, const std::vector<std::string>& switchNames);
  virtual bool setGroupState(const std::string& groupName, SwitchState state);

  // Missing IDevice interface methods
  std::string getName() const override;
  std::string getDescription() const override;
  std::string getDriverInfo() const override;
  std::string getDriverVersion() const override;
  int getInterfaceVersion() const override;
  std::vector<std::string> getSupportedActions() const override;
  bool isConnecting() const override;
  interfaces::DeviceState getDeviceState() const override;
  std::string action(const std::string& actionName, const std::string& actionParameters) override;
  void commandBlind(const std::string& command, bool raw) override;
  bool commandBool(const std::string& command, bool raw) override;
  std::string commandString(const std::string& command, bool raw) override;
  void setupDialog() override;

  // Device lifecycle methods
  virtual void run();  // Main device loop

protected:
  // 重写基类方法
  bool initializeDevice() override;
  bool startDevice() override;
  void stopDevice() override;
  bool handleDeviceCommand(const std::string& command,
                          const json& parameters,
                          json& result) override;
  void updateDevice() override;

private:
  // Hardware abstraction interface
  virtual bool executeSetSwitch(int switchId, bool state);
  virtual bool executeSetSwitchValue(int switchId, double value);
  virtual bool executePulseSwitch(int switchId, int duration);
  virtual bool readSwitchState(int switchId);
  virtual double readSwitchValue(int switchId);

  // Additional hardware methods used by implementation
  virtual bool executeSetState(const std::string& name, SwitchState state);
  virtual bool executePulse(const std::string& name, int durationMs);

  // 配置管理
  void initializeDefaultSwitches();
  bool validateSwitchId(int switchId) const;
  bool checkSwitchInterlock(int switchId, bool newState);

  // 统计更新
  void updateSwitchStatistics(int switchId, bool state);

private:
  // Switch information (ID-based)
  mutable std::mutex switchInfoMutex_;
  std::unordered_map<int, SwitchInfo> switchInfo_;
  std::atomic<int> switchCount_;

  // Switch information (name-based, used by implementation)
  mutable std::mutex switchesMutex_;
  std::unordered_map<std::string, SwitchInfo> switches_;
  std::atomic<bool> groupControlEnabled_;

  // Switch groups
  mutable std::mutex groupsMutex_;
  std::unordered_map<std::string, std::vector<std::string>> switchGroups_;
  std::unordered_map<std::string, std::vector<int>> switchGroupsInt_;

  // Switch interlocks
  mutable std::mutex interlockMutex_;
  std::unordered_map<int, std::vector<int>> switchInterlocks_;

  // 开关序�?  mutable std::mutex sequenceMutex_;
  std::vector<std::pair<int, int>> switchSequence_;

  // 开关保护和启用状�?  mutable std::mutex protectionMutex_;
  std::unordered_map<int, bool> switchEnabled_;
  std::unordered_map<int, bool> switchProtected_;

  // 统计数据
  mutable std::mutex statisticsMutex_;
  std::unordered_map<int, int> switchUsageCount_;
  std::unordered_map<int, std::chrono::system_clock::time_point> lastSwitchTime_;
};

/**
 * @brief 开关设备工�? */
class SwitchFactory : public core::TypedDeviceFactory<Switch> {
public:
  SwitchFactory(const std::string& manufacturer = "Generic", 
                const std::string& model = "Switch")
      : TypedDeviceFactory<Switch>(manufacturer, model) {}
};

} // namespace device
} // namespace hydrogen

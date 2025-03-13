#pragma once
#include "device/device_base.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace astrocomm {

// 开关设备类
class Switch : public DeviceBase {
public:
  // 开关类型枚举
  enum class SwitchType {
    TOGGLE,    // 常规开关，保持在设置的状态
    MOMENTARY, // 临时开关，自动恢复到默认状态
    BUTTON     // 按钮，按下后立即恢复
  };

  // 开关状态枚举
  enum class SwitchState {
    OFF, // 关闭状态
    ON   // 开启状态
  };

  Switch(const std::string &deviceId,
         const std::string &manufacturer = "Generic",
         const std::string &model = "Multi-Switch");
  virtual ~Switch();

  // 重写启动和停止方法
  virtual bool start() override;
  virtual void stop() override;

  // 添加开关
  void addSwitch(const std::string &name, SwitchType type = SwitchType::TOGGLE,
                 SwitchState defaultState = SwitchState::OFF);

  // 设置开关状态
  bool setState(const std::string &name, SwitchState state);

  // 获取开关状态
  SwitchState getState(const std::string &name) const;

  // 获取所有开关
  std::vector<std::string> getSwitchNames() const;

  // 创建开关组（用于同时控制多个开关）
  void createSwitchGroup(const std::string &groupName,
                         const std::vector<std::string> &switches);

  // 设置开关组状态
  bool setGroupState(const std::string &groupName, SwitchState state);

protected:
  // 命令处理器
  void handleSetStateCommand(const CommandMessage &cmd,
                             ResponseMessage &response);
  void handleGetStateCommand(const CommandMessage &cmd,
                             ResponseMessage &response);
  void handleSetGroupCommand(const CommandMessage &cmd,
                             ResponseMessage &response);
  void handlePulseCommand(const CommandMessage &cmd, ResponseMessage &response);

  // 发送开关状态变更事件
  void sendSwitchStateChangedEvent(const std::string &switchName,
                                   SwitchState newState, SwitchState oldState);

  // 处理瞬时开关的恢复
  void handleMomentaryRestore(const std::string &switchName,
                              SwitchState originalState);

private:
  // 开关信息结构
  struct SwitchInfo {
    std::string name;
    SwitchType type;
    SwitchState state;
    SwitchState defaultState;
  };

  // 开关信息存储
  std::unordered_map<std::string, SwitchInfo> switches;

  // 开关组存储
  std::unordered_map<std::string, std::vector<std::string>> switchGroups;

  // 恢复线程管理（用于瞬时开关）
  std::unordered_map<std::string, std::thread> restoreThreads;

  // 内部实现方法
  std::string switchStateToString(SwitchState state) const;
  SwitchState stringToSwitchState(const std::string &stateStr) const;
  std::string switchTypeToString(SwitchType type) const;
};

} // namespace astrocomm
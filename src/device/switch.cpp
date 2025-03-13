#include "device/switch.h"
#include "common/logger.h"
#include <chrono>
#include <stdexcept>

namespace astrocomm {

Switch::Switch(const std::string &deviceId, const std::string &manufacturer,
               const std::string &model)
    : DeviceBase(deviceId, "SWITCH", manufacturer, model) {

  // 设置设备能力
  capabilities = {"MULTI_SWITCH", "GROUPING", "MOMENTARY_SWITCH"};

  // 注册命令处理器
  registerCommandHandler("SET_SWITCH", [this](const CommandMessage &cmd,
                                              ResponseMessage &response) {
    handleSetStateCommand(cmd, response);
  });

  registerCommandHandler("GET_SWITCH", [this](const CommandMessage &cmd,
                                              ResponseMessage &response) {
    handleGetStateCommand(cmd, response);
  });

  registerCommandHandler("SET_GROUP", [this](const CommandMessage &cmd,
                                             ResponseMessage &response) {
    handleSetGroupCommand(cmd, response);
  });

  registerCommandHandler("PULSE_SWITCH", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
    handlePulseCommand(cmd, response);
  });

  logInfo("Switch device initialized", deviceId);
}

Switch::~Switch() { stop(); }

bool Switch::start() {
  if (!DeviceBase::start()) {
    return false;
  }

  // 初始化所有开关为默认状态
  for (auto &switchItem : switches) {
    switchItem.second.state = switchItem.second.defaultState;
    setProperty("switch_" + switchItem.first,
                switchStateToString(switchItem.second.state));
  }

  setProperty("connected", true);
  logInfo("Switch device started", deviceId);
  return true;
}

void Switch::stop() {
  // 停止所有恢复线程
  for (auto &thread : restoreThreads) {
    if (thread.second.joinable()) {
      thread.second.join();
    }
  }
  restoreThreads.clear();

  setProperty("connected", false);
  DeviceBase::stop();
  logInfo("Switch device stopped", deviceId);
}

void Switch::addSwitch(const std::string &name, SwitchType type,
                       SwitchState defaultState) {
  if (switches.find(name) != switches.end()) {
    logWarning("Switch already exists: " + name, deviceId);
    return;
  }

  SwitchInfo info;
  info.name = name;
  info.type = type;
  info.state = defaultState;
  info.defaultState = defaultState;

  switches[name] = info;

  // 添加属性
  setProperty("switch_" + name, switchStateToString(defaultState));
  setProperty("switch_" + name + "_type", switchTypeToString(type));

  logInfo("Added switch: " + name + ", type: " + switchTypeToString(type) +
              ", default state: " + switchStateToString(defaultState),
          deviceId);
}

bool Switch::setState(const std::string &name, SwitchState state) {
  auto it = switches.find(name);
  if (it == switches.end()) {
    logWarning("Switch not found: " + name, deviceId);
    return false;
  }

  SwitchState oldState = it->second.state;
  if (oldState == state) {
    // 状态没有变化
    return true;
  }

  it->second.state = state;
  setProperty("switch_" + name, switchStateToString(state));

  // 发送状态变更事件
  sendSwitchStateChangedEvent(name, state, oldState);

  logInfo("Switch state changed: " + name + " -> " + switchStateToString(state),
          deviceId);

  // 如果是瞬时开关，则安排恢复
  if (it->second.type == SwitchType::MOMENTARY ||
      (it->second.type == SwitchType::BUTTON && state == SwitchState::ON)) {

    // 取消现有的恢复线程（如果有）
    auto threadIt = restoreThreads.find(name);
    if (threadIt != restoreThreads.end()) {
      if (threadIt->second.joinable()) {
        threadIt->second.join();
      }
      restoreThreads.erase(threadIt);
    }

    // 创建新的恢复线程
    restoreThreads[name] = std::thread(&Switch::handleMomentaryRestore, this,
                                       name, it->second.defaultState);
  }

  return true;
}

Switch::SwitchState Switch::getState(const std::string &name) const {
  auto it = switches.find(name);
  if (it == switches.end()) {
    throw std::runtime_error("Switch not found: " + name);
  }

  return it->second.state;
}

std::vector<std::string> Switch::getSwitchNames() const {
  std::vector<std::string> names;
  for (const auto &switchItem : switches) {
    names.push_back(switchItem.first);
  }
  return names;
}

void Switch::createSwitchGroup(const std::string &groupName,
                               const std::vector<std::string> &switchNames) {
  // 验证所有开关是否存在
  for (const auto &name : switchNames) {
    if (switches.find(name) == switches.end()) {
      logWarning("Cannot create group, switch not found: " + name, deviceId);
      return;
    }
  }

  switchGroups[groupName] = switchNames;

  // 添加组属性
  json groupSwitches = json::array();
  for (const auto &name : switchNames) {
    groupSwitches.push_back(name);
  }
  setProperty("group_" + groupName, groupSwitches);

  logInfo("Created switch group: " + groupName + " with " +
              std::to_string(switchNames.size()) + " switches",
          deviceId);
}

bool Switch::setGroupState(const std::string &groupName, SwitchState state) {
  auto groupIt = switchGroups.find(groupName);
  if (groupIt == switchGroups.end()) {
    logWarning("Switch group not found: " + groupName, deviceId);
    return false;
  }

  bool success = true;
  for (const auto &switchName : groupIt->second) {
    if (!setState(switchName, state)) {
      success = false;
    }
  }

  return success;
}

// 命令处理器
void Switch::handleSetStateCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("switch") || !params.contains("state")) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_PARAMETERS"},
         {"message", "Missing required parameters 'switch' and 'state'"}});
    return;
  }

  std::string switchName = params["switch"];
  std::string stateStr = params["state"];

  try {
    SwitchState state = stringToSwitchState(stateStr);

    if (setState(switchName, state)) {
      response.setStatus("SUCCESS");
      response.setDetails(
          {{"switch", switchName}, {"state", switchStateToString(state)}});
    } else {
      response.setStatus("ERROR");
      response.setDetails({{"error", "SET_STATE_FAILED"},
                           {"message", "Failed to set switch state"}});
    }
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_STATE"}, {"message", e.what()}});
  }
}

void Switch::handleGetStateCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("switch")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'switch'"}});
    return;
  }

  std::string switchName = params["switch"];

  try {
    SwitchState state = getState(switchName);

    response.setStatus("SUCCESS");
    response.setDetails(
        {{"switch", switchName}, {"state", switchStateToString(state)}});
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "GET_STATE_FAILED"}, {"message", e.what()}});
  }
}

void Switch::handleSetGroupCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("group") || !params.contains("state")) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_PARAMETERS"},
         {"message", "Missing required parameters 'group' and 'state'"}});
    return;
  }

  std::string groupName = params["group"];
  std::string stateStr = params["state"];

  try {
    SwitchState state = stringToSwitchState(stateStr);

    if (setGroupState(groupName, state)) {
      response.setStatus("SUCCESS");
      response.setDetails(
          {{"group", groupName}, {"state", switchStateToString(state)}});
    } else {
      response.setStatus("ERROR");
      response.setDetails({{"error", "SET_GROUP_FAILED"},
                           {"message", "Failed to set group state"}});
    }
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_STATE"}, {"message", e.what()}});
  }
}

void Switch::handlePulseCommand(const CommandMessage &cmd,
                                ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("switch") || !params.contains("duration")) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_PARAMETERS"},
         {"message", "Missing required parameters 'switch' and 'duration'"}});
    return;
  }

  std::string switchName = params["switch"];
  int duration = params["duration"]; // milliseconds

  auto it = switches.find(switchName);
  if (it == switches.end()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "SWITCH_NOT_FOUND"},
                         {"message", "Switch not found: " + switchName}});
    return;
  }

  // 保存当前状态
  SwitchState currentState = it->second.state;

  // 设置为相反状态
  SwitchState pulseState =
      (currentState == SwitchState::ON) ? SwitchState::OFF : SwitchState::ON;

  if (setState(switchName, pulseState)) {
    // 创建定时器来恢复状态
    std::thread([this, switchName, currentState, duration]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(duration));
      setState(switchName, currentState);
    }).detach();

    response.setStatus("SUCCESS");
    response.setDetails({{"switch", switchName},
                         {"pulse_state", switchStateToString(pulseState)},
                         {"duration", duration}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "PULSE_FAILED"}, {"message", "Failed to set pulse state"}});
  }
}

void Switch::sendSwitchStateChangedEvent(const std::string &switchName,
                                         SwitchState newState,
                                         SwitchState oldState) {
  EventMessage event("SWITCH_STATE_CHANGED");

  event.setDetails({{"switch", switchName},
                    {"state", switchStateToString(newState)},
                    {"previous_state", switchStateToString(oldState)}});

  sendEvent(event);
}

void Switch::handleMomentaryRestore(const std::string &switchName,
                                    SwitchState originalState) {
  // 等待一段时间然后恢复状态
  std::this_thread::sleep_for(
      std::chrono::milliseconds(500)); // 500ms的短暂按下

  try {
    setState(switchName, originalState);
  } catch (const std::exception &e) {
    logError("Error restoring momentary switch: " + std::string(e.what()),
             deviceId);
  }
}

std::string Switch::switchStateToString(SwitchState state) const {
  switch (state) {
  case SwitchState::OFF:
    return "OFF";
  case SwitchState::ON:
    return "ON";
  default:
    return "UNKNOWN";
  }
}

Switch::SwitchState
Switch::stringToSwitchState(const std::string &stateStr) const {
  std::string upperState = string_utils::toUpper(stateStr);

  if (upperState == "OFF")
    return SwitchState::OFF;
  if (upperState == "ON")
    return SwitchState::ON;

  throw std::invalid_argument("Invalid switch state: " + stateStr);
}

std::string Switch::switchTypeToString(SwitchType type) const {
  switch (type) {
  case SwitchType::TOGGLE:
    return "TOGGLE";
  case SwitchType::MOMENTARY:
    return "MOMENTARY";
  case SwitchType::BUTTON:
    return "BUTTON";
  default:
    return "UNKNOWN";
  }
}

} // namespace astrocomm
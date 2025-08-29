#include "device/switch.h"
// #include "common/logger.h" // Remove this line
#include <spdlog/spdlog.h> // Add spdlog header
#include <spdlog/sinks/stdout_color_sinks.h> // For console logging
#include <chrono>
#include <stdexcept>
#include <string> // Add for std::string
#include <algorithm> // Add for std::transform
#include <cctype> // Add for std::toupper
#include <map>
#include <thread>
#include <mutex>

namespace astrocomm {

// Helper function for string conversion (can be moved to a utility header)
namespace string_utils {
std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return s;
}
} // namespace string_utils

// ============================================================================
// SwitchImpl - Implementation detail class
// ============================================================================
class Switch::SwitchImpl {
public:
    SwitchImpl(Switch* parent, const std::string& deviceId, 
               const std::string& manufacturer, const std::string& model);
    ~SwitchImpl();

    bool start();
    void stop();

    void addSwitch(const std::string &name, Switch::SwitchType type, 
                  Switch::SwitchState defaultState);
    bool setState(const std::string &name, Switch::SwitchState state);
    Switch::SwitchState getState(const std::string &name) const;
    std::vector<std::string> getSwitchNames() const;
    void createSwitchGroup(const std::string &groupName, 
                           const std::vector<std::string> &switchNames);
    bool setGroupState(const std::string &groupName, Switch::SwitchState state);
    
    void handleSetStateCommand(const CommandMessage &cmd, ResponseMessage &response);
    void handleGetStateCommand(const CommandMessage &cmd, ResponseMessage &response);
    void handleSetGroupCommand(const CommandMessage &cmd, ResponseMessage &response);
    void handlePulseCommand(const CommandMessage &cmd, ResponseMessage &response);

    void sendSwitchStateChangedEvent(const std::string &switchName, 
                                    Switch::SwitchState newState, 
                                    Switch::SwitchState oldState);
    void handleMomentaryRestore(const std::string &switchName, 
                               Switch::SwitchState originalState);
    
    std::string switchStateToString(Switch::SwitchState state) const;
    Switch::SwitchState stringToSwitchState(const std::string &stateStr) const;
    std::string switchTypeToString(Switch::SwitchType type) const;

private:
    // Switch information structure
    struct SwitchInfo {
        std::string name;
        Switch::SwitchType type;
        Switch::SwitchState state;
        Switch::SwitchState defaultState;
    };

    Switch* parent_; // Pointer to parent class (no ownership)
    std::string deviceId; // Store device ID for logging
    std::shared_ptr<spdlog::logger> logger_; // Logger
    
    // Switch state management
    std::map<std::string, SwitchInfo> switches;
    std::map<std::string, std::vector<std::string>> switchGroups;
    std::map<std::string, std::thread> restoreThreads;
    
    // Thread safety mutex
    mutable std::mutex mutex_;
};

// SwitchImpl 构造函数
Switch::SwitchImpl::SwitchImpl(Switch* parent, const std::string& deviceId, 
                              const std::string& manufacturer, const std::string& model)
    : parent_(parent), deviceId(deviceId) {
    
    // 初始化日志记录器
    try {
        logger_ = spdlog::stdout_color_mt("switch_" + deviceId);
    } catch (const spdlog::spdlog_ex& ex) {
        logger_ = spdlog::get("switch_" + deviceId); // 如果已存在则获取现有记录器
    }
    logger_->set_level(spdlog::level::info);
    
    // 设置设备功能
    parent_->capabilities = {"MULTI_SWITCH", "GROUPING", "MOMENTARY_SWITCH"};
    
    // 注册命令处理器
    parent_->registerCommandHandler("SET_SWITCH", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
        handleSetStateCommand(cmd, response);
    });

    parent_->registerCommandHandler("GET_SWITCH", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
        handleGetStateCommand(cmd, response);
    });

    parent_->registerCommandHandler("SET_GROUP", [this](const CommandMessage &cmd,
                                               ResponseMessage &response) {
        handleSetGroupCommand(cmd, response);
    });

    parent_->registerCommandHandler("PULSE_SWITCH", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
        handlePulseCommand(cmd, response);
    });

    logger_->info("Switch device initialized: {}", deviceId);
}

// SwitchImpl 析构函数
Switch::SwitchImpl::~SwitchImpl() {
    stop();
    spdlog::drop("switch_" + deviceId); // 清理日志记录器
}

// 启动开关设备
bool Switch::SwitchImpl::start() {
    if (!parent_->DeviceBase::start()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    // 将所有开关初始化为默认状态
    for (auto &switchItem : switches) {
        switchItem.second.state = switchItem.second.defaultState;
        parent_->setProperty("switch_" + switchItem.first,
                    switchStateToString(switchItem.second.state));
    }

    parent_->setProperty("connected", true);
    logger_->info("Switch device started: {}", deviceId);
    return true;
}

// 停止开关设备
void Switch::SwitchImpl::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 停止所有恢复线程
        for (auto &pair : restoreThreads) {
            if (pair.second.joinable()) {
                pair.second.join();
            }
        }
        restoreThreads.clear();
    }

    parent_->setProperty("connected", false);
    parent_->DeviceBase::stop();
    logger_->info("Switch device stopped: {}", deviceId);
}

// 添加开关
void Switch::SwitchImpl::addSwitch(const std::string &name, Switch::SwitchType type,
                               Switch::SwitchState defaultState) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (switches.count(name)) {
        logger_->warn("Switch already exists: {} in device {}", name, deviceId);
        return;
    }

    SwitchInfo info;
    info.name = name;
    info.type = type;
    info.state = defaultState;
    info.defaultState = defaultState;

    switches[name] = info;

    // 添加属性
    parent_->setProperty("switch_" + name, switchStateToString(defaultState));
    parent_->setProperty("switch_" + name + "_type", switchTypeToString(type));

    logger_->info("Added switch: {}, type: {}, default state: {} to device {}",
                name, switchTypeToString(type), switchStateToString(defaultState),
                deviceId);
}

// 设置开关状态
bool Switch::SwitchImpl::setState(const std::string &name, Switch::SwitchState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = switches.find(name);
    if (it == switches.end()) {
        logger_->warn("Switch not found: {} in device {}", name, deviceId);
        return false;
    }

    Switch::SwitchState oldState = it->second.state;
    if (oldState == state) {
        // 状态未改变
        return true;
    }

    it->second.state = state;
    parent_->setProperty("switch_" + name, switchStateToString(state));

    // 发送状态变更事件
    sendSwitchStateChangedEvent(name, state, oldState);

    logger_->info("Switch state changed: {} -> {} in device {}", name,
                switchStateToString(state), deviceId);

    // 如果是瞬时开关或按钮开关打开时，安排恢复
    if (it->second.type == Switch::SwitchType::MOMENTARY ||
        (it->second.type == Switch::SwitchType::BUTTON && state == Switch::SwitchState::ON)) {

        // 取消现有恢复线程（如果有）
        auto threadIt = restoreThreads.find(name);
        if (threadIt != restoreThreads.end()) {
            if (threadIt->second.joinable()) {
                threadIt->second.join(); // 等待之前的线程完成
            }
            restoreThreads.erase(threadIt);
        }

        // 创建新的恢复线程
        restoreThreads[name] = std::thread(&Switch::SwitchImpl::handleMomentaryRestore, this,
                                       name, it->second.defaultState);
    }

    return true;
}

// 获取开关状态
Switch::SwitchState Switch::SwitchImpl::getState(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = switches.find(name);
    if (it == switches.end()) {
        throw std::runtime_error("Switch not found: " + name);
    }

    return it->second.state;
}

// 获取所有开关名称
std::vector<std::string> Switch::SwitchImpl::getSwitchNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> names;
    names.reserve(switches.size()); // 优化分配
    for (const auto &pair : switches) {
        names.push_back(pair.first);
    }
    return names;
}

// 创建开关组
void Switch::SwitchImpl::createSwitchGroup(const std::string &groupName,
                                       const std::vector<std::string> &switchNames) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 验证所有开关是否存在
    for (const auto &name : switchNames) {
        if (!switches.count(name)) {
            logger_->warn("Cannot create group '{}', switch not found: {} in device {}",
                        groupName, name, deviceId);
            return;
        }
    }

    switchGroups[groupName] = switchNames;

    // 添加组属性
    json groupSwitches = json::array();
    for (const auto &name : switchNames) {
        groupSwitches.push_back(name);
    }
    parent_->setProperty("group_" + groupName, groupSwitches);

    logger_->info("Created switch group: {} with {} switches in device {}",
                groupName, switchNames.size(), deviceId);
}

// 设置组状态
bool Switch::SwitchImpl::setGroupState(const std::string &groupName, Switch::SwitchState state) {
    // 修复：不能在持有mutex_锁的情况下递归调用setState，否则会死锁
    std::vector<std::string> groupSwitches;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto groupIt = switchGroups.find(groupName);
        if (groupIt == switchGroups.end()) {
            logger_->warn("Switch group not found: {} in device {}", groupName, deviceId);
            return false;
        }
        groupSwitches = groupIt->second;
    }

    bool allSuccess = true;
    logger_->info("Setting state for group '{}' to {} in device {}", groupName, switchStateToString(state), deviceId);

    for (const auto &switchName : groupSwitches) {
        if (!setState(switchName, state)) {
            logger_->error("Failed to set state for switch '{}' in group '{}' in device {}", 
                         switchName, groupName, deviceId);
            allSuccess = false;
        }
    }

    return allSuccess;
}

// 处理设置状态命令
void Switch::SwitchImpl::handleSetStateCommand(const CommandMessage &cmd,
                                           ResponseMessage &response) {
    json params = cmd.getParameters();

    if (!params.contains("switch") || !params.contains("state")) {
        logger_->warn("SetState command missing 'switch' or 'state' parameter for device {}", deviceId);
        response.setStatus("ERROR");
        response.setDetails(
            {{"error", "INVALID_PARAMETERS"},
            {"message", "Missing required parameters 'switch' and 'state'"}});
        return;
    }

    std::string switchName = params["switch"];
    std::string stateStr = params["state"];

    try {
        Switch::SwitchState state = stringToSwitchState(stateStr);
        logger_->debug("Received SetState command for switch '{}', state '{}' in device {}", 
                     switchName, stateStr, deviceId);

        if (setState(switchName, state)) {
            response.setStatus("SUCCESS");
            response.setDetails(
                {{"switch", switchName}, {"state", switchStateToString(state)}});
            logger_->info("Successfully set state for switch '{}' to {} in device {}", 
                        switchName, switchStateToString(state), deviceId);
        } else {
            response.setStatus("ERROR");
            response.setDetails({{"error", "SET_STATE_FAILED"},
                               {"message", "Failed to set switch state for " + switchName}});
            logger_->error("Failed to set state for switch '{}' in device {}", switchName, deviceId);
        }
    } catch (const std::invalid_argument &e) {
        logger_->error("Invalid state value '{}' received for switch '{}' in device {}: {}", 
                     stateStr, switchName, deviceId, e.what());
        response.setStatus("ERROR");
        response.setDetails({{"error", "INVALID_STATE"}, {"message", e.what()}});
    } catch (const std::exception &e) {
        logger_->error("Exception during SetState for switch '{}' in device {}: {}", 
                     switchName, deviceId, e.what());
        response.setStatus("ERROR");
        response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
    }
}

// 处理获取状态命令
void Switch::SwitchImpl::handleGetStateCommand(const CommandMessage &cmd,
                                           ResponseMessage &response) {
    json params = cmd.getParameters();

    if (!params.contains("switch")) {
        logger_->warn("GetState command missing 'switch' parameter for device {}", deviceId);
        response.setStatus("ERROR");
        response.setDetails({{"error", "INVALID_PARAMETERS"},
                           {"message", "Missing required parameter 'switch'"}});
        return;
    }

    std::string switchName = params["switch"];
    logger_->debug("Received GetState command for switch '{}' in device {}", switchName, deviceId);

    try {
        Switch::SwitchState state = getState(switchName); // 会抛出异常如果找不到

        response.setStatus("SUCCESS");
        response.setDetails(
            {{"switch", switchName}, {"state", switchStateToString(state)}});
        logger_->info("Successfully retrieved state for switch '{}': {} in device {}", 
                    switchName, switchStateToString(state), deviceId);
    } catch (const std::runtime_error &e) {
        logger_->error("GetState failed for switch '{}' in device {}: {}", 
                     switchName, deviceId, e.what());
        response.setStatus("ERROR");
        response.setDetails({{"error", "SWITCH_NOT_FOUND"}, {"message", e.what()}});
    } catch (const std::exception &e) {
        logger_->error("Exception during GetState for switch '{}' in device {}: {}", 
                     switchName, deviceId, e.what());
        response.setStatus("ERROR");
        response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
    }
}

// 处理设置组命令
void Switch::SwitchImpl::handleSetGroupCommand(const CommandMessage &cmd,
                                           ResponseMessage &response) {
    json params = cmd.getParameters();

    if (!params.contains("group") || !params.contains("state")) {
        logger_->warn("SetGroup command missing 'group' or 'state' parameter for device {}", deviceId);
        response.setStatus("ERROR");
        response.setDetails(
            {{"error", "INVALID_PARAMETERS"},
            {"message", "Missing required parameters 'group' and 'state'"}});
        return;
    }

    std::string groupName = params["group"];
    std::string stateStr = params["state"];

    try {
        Switch::SwitchState state = stringToSwitchState(stateStr);
        logger_->debug("Received SetGroup command for group '{}', state '{}' in device {}", 
                     groupName, stateStr, deviceId);

        if (setGroupState(groupName, state)) {
            response.setStatus("SUCCESS");
            response.setDetails(
                {{"group", groupName}, {"state", switchStateToString(state)}});
            logger_->info("Successfully set state for group '{}' to {} in device {}", 
                        groupName, switchStateToString(state), deviceId);
        } else {
            response.setStatus("ERROR");
            response.setDetails({{"error", "SET_GROUP_FAILED"},
                              {"message", "Failed to set state for group " + groupName + 
                                           ". Some switches may have failed."}});
            logger_->error("Failed to set state for group '{}' in device {}", groupName, deviceId);
        }
    } catch (const std::invalid_argument &e) {
        logger_->error("Invalid state value '{}' received for group '{}' in device {}: {}", 
                     stateStr, groupName, deviceId, e.what());
        response.setStatus("ERROR");
        response.setDetails({{"error", "INVALID_STATE"}, {"message", e.what()}});
    } catch (const std::exception &e) {
        logger_->error("Exception during SetGroup for group '{}' in device {}: {}", 
                     groupName, deviceId, e.what());
        response.setStatus("ERROR");
        response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
    }
}

// 处理脉冲命令
void Switch::SwitchImpl::handlePulseCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
    json params = cmd.getParameters();

    if (!params.contains("switch") || !params.contains("duration")) {
        logger_->warn("Pulse command missing 'switch' or 'duration' parameter for device {}", deviceId);
        response.setStatus("ERROR");
        response.setDetails(
            {{"error", "INVALID_PARAMETERS"},
            {"message", "Missing required parameters 'switch' and 'duration'"}});
        return;
    }

    std::string switchName = params["switch"];
    int duration = 0;
    try {
        duration = params["duration"].get<int>(); // 毫秒
        if (duration <= 0) {
            throw std::invalid_argument("Duration must be positive");
        }
    } catch (const std::exception& e) {
        logger_->error("Invalid duration value '{}' received for pulse command on switch '{}' in device {}: {}",
                     params["duration"].dump(), switchName, deviceId, e.what());
        response.setStatus("ERROR");
        response.setDetails({{"error", "INVALID_PARAMETERS"},
                           {"message", "Invalid 'duration' parameter: must be a positive integer."}});
        return;
    }

    logger_->debug("Received Pulse command for switch '{}', duration {}ms in device {}",
                 switchName, duration, deviceId);

    // 修复：不能在持有锁的情况下调用 setState
    Switch::SwitchState currentState;
    Switch::SwitchState pulseState;
    { // 临界区开始：获取当前状态
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = switches.find(switchName);
        if (it == switches.end()) {
            logger_->error("Pulse command failed: Switch not found: {} in device {}", switchName, deviceId);
            response.setStatus("ERROR");
            response.setDetails({{"error", "SWITCH_NOT_FOUND"},
                              {"message", "Switch not found: " + switchName}});
            return; // 锁在此处自动释放
        }
        currentState = it->second.state;
    } // 临界区结束：锁已释放

    // 确定脉冲状态（当前状态的相反）
    pulseState = (currentState == Switch::SwitchState::ON) ? Switch::SwitchState::OFF : Switch::SwitchState::ON;

    logger_->info("Pulsing switch '{}' to {} for {}ms in device {}",
                switchName, switchStateToString(pulseState), duration, deviceId);

    // 设置脉冲状态（现在不持有锁）
    if (setState(switchName, pulseState)) {
        // 创建分离线程以在持续时间后恢复状态
        // 捕获必要的变量，包括日志记录器
        std::thread([this, switchName, currentState, duration, logger = this->logger_, deviceId = this->deviceId]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(duration));
            try {
                logger->info("Restoring switch '{}' to state {} after pulse in device {}",
                           switchName, switchStateToString(currentState), deviceId);
                // setState 会自己处理锁
                if (!setState(switchName, currentState)) {
                     logger->error("Failed to restore switch '{}' after pulse in device {}", switchName, deviceId);
                }
            } catch (const std::exception& e) {
                logger->error("Error restoring switch '{}' after pulse in device {}: {}",
                            switchName, deviceId, e.what());
            }
        }).detach(); // 分离线程，允许它独立运行

        response.setStatus("SUCCESS");
        response.setDetails({{"switch", switchName},
                          {"pulse_state", switchStateToString(pulseState)},
                          {"duration", duration}});
        logger_->info("Successfully initiated pulse for switch '{}' in device {}", switchName, deviceId);
    } else {
        // 如果设置初始脉冲状态失败
        logger_->error("Pulse command failed: Could not set initial pulse state for switch '{}' to {} in device {}",
                     switchName, switchStateToString(pulseState), deviceId);
        response.setStatus("ERROR");
        response.setDetails(
            {{"error", "PULSE_FAILED"},
            {"message", "Failed to set initial pulse state for " + switchName}});
        // 不需要恢复状态，因为初始设置失败了
    }
}

// 发送开关状态变更事件
void Switch::SwitchImpl::sendSwitchStateChangedEvent(const std::string &switchName,
                                                 Switch::SwitchState newState,
                                                 Switch::SwitchState oldState) {
    EventMessage event("SWITCH_STATE_CHANGED");

    event.setDetails({{"switch", switchName},
                    {"state", switchStateToString(newState)},
                    {"previous_state", switchStateToString(oldState)}});

    logger_->debug("Sending SWITCH_STATE_CHANGED event for switch '{}': {} -> {} in device {}",
                 switchName, switchStateToString(oldState), switchStateToString(newState), deviceId);
    parent_->sendEvent(event);
}

// 处理瞬时恢复
void Switch::SwitchImpl::handleMomentaryRestore(const std::string &switchName,
                                            Switch::SwitchState originalState) {
    // 瞬时/按钮释放的默认延迟
    constexpr std::chrono::milliseconds momentaryDelay(500);
    logger_->debug("Starting momentary restore timer for switch '{}' to state {} in device {}", 
                 switchName, switchStateToString(originalState), deviceId);

    std::this_thread::sleep_for(momentaryDelay);

    try {
        // 恢复开关到原始状态
        logger_->info("Restoring momentary switch '{}' to state {} in device {}", 
                    switchName, switchStateToString(originalState), deviceId);
        setState(switchName, originalState);
    } catch (const std::exception &e) {
        logger_->error("Error restoring momentary switch '{}' in device {}: {}",
                    switchName, deviceId, e.what());
    }
}

// 开关状态转字符串
std::string Switch::SwitchImpl::switchStateToString(Switch::SwitchState state) const {
    switch (state) {
    case Switch::SwitchState::OFF:
        return "OFF";
    case Switch::SwitchState::ON:
        return "ON";
    default:
        logger_->warn("Attempted to convert unknown SwitchState enum value to string in device {}", deviceId);
        return "UNKNOWN";
    }
}

// 字符串转开关状态
Switch::SwitchState
Switch::SwitchImpl::stringToSwitchState(const std::string &stateStr) const {
    std::string upperState = string_utils::toUpper(stateStr);

    if (upperState == "OFF")
        return Switch::SwitchState::OFF;
    if (upperState == "ON")
        return Switch::SwitchState::ON;

    logger_->warn("Invalid switch state string received: '{}' in device {}", stateStr, deviceId);
    throw std::invalid_argument("Invalid switch state string: " + stateStr);
}

// 开关类型转字符串
std::string Switch::SwitchImpl::switchTypeToString(Switch::SwitchType type) const {
    switch (type) {
    case Switch::SwitchType::TOGGLE:
        return "TOGGLE";
    case Switch::SwitchType::MOMENTARY:
        return "MOMENTARY";
    case Switch::SwitchType::BUTTON:
        return "BUTTON";
    default:
        logger_->warn("Attempted to convert unknown SwitchType enum value to string in device {}", deviceId);
        return "UNKNOWN";
    }
}

Switch::Switch(const std::string &deviceId, const std::string &manufacturer,
               const std::string &model)
    : DeviceBase(deviceId, "SWITCH", manufacturer, model),
      impl_(std::make_unique<SwitchImpl>(this, deviceId, manufacturer, model)) {
}

Switch::~Switch() = default;

bool Switch::start() {
    return impl_->start();
}

void Switch::stop() {
    impl_->stop();
}

void Switch::addSwitch(const std::string &name, SwitchType type,
                       SwitchState defaultState) {
    impl_->addSwitch(name, type, defaultState);
}

bool Switch::setState(const std::string &name, SwitchState state) {
    return impl_->setState(name, state);
}

Switch::SwitchState Switch::getState(const std::string &name) const {
    return impl_->getState(name);
}

std::vector<std::string> Switch::getSwitchNames() const {
    return impl_->getSwitchNames();
}

void Switch::createSwitchGroup(const std::string &groupName,
                               const std::vector<std::string> &switchNames) {
    impl_->createSwitchGroup(groupName, switchNames);
}

bool Switch::setGroupState(const std::string &groupName, SwitchState state) {
    return impl_->setGroupState(groupName, state);
}

void Switch::handleSetStateCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
    impl_->handleSetStateCommand(cmd, response);
}

void Switch::handleGetStateCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
    impl_->handleGetStateCommand(cmd, response);
}

void Switch::handleSetGroupCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
    impl_->handleSetGroupCommand(cmd, response);
}

void Switch::handlePulseCommand(const CommandMessage &cmd,
                                ResponseMessage &response) {
    impl_->handlePulseCommand(cmd, response);
}

void Switch::sendSwitchStateChangedEvent(const std::string &switchName,
                                         SwitchState newState,
                                         SwitchState oldState) {
    impl_->sendSwitchStateChangedEvent(switchName, newState, oldState);
}

void Switch::handleMomentaryRestore(const std::string &switchName,
                                    SwitchState originalState) {
    impl_->handleMomentaryRestore(switchName, originalState);
}

std::string Switch::switchStateToString(SwitchState state) const {
    return impl_->switchStateToString(state);
}

Switch::SwitchState Switch::stringToSwitchState(const std::string &stateStr) const {
    return impl_->stringToSwitchState(stateStr);
}

std::string Switch::switchTypeToString(SwitchType type) const {
    return impl_->switchTypeToString(type);
}

} // namespace astrocomm
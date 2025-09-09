#include "movable_behavior.h"
#include "../core/state_manager.h"
#include "../core/config_manager.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace hydrogen {
namespace device {
namespace behaviors {

MovableBehavior::MovableBehavior(const std::string& behaviorName)
    : DeviceBehavior(behaviorName)
    , currentPosition_(0)
    , targetPosition_(0)
    , minPosition_(0)
    , maxPosition_(10000)
    , movementState_(MovementState::IDLE)
    , reversed_(false)
    , movementSpeed_(100)
    , monitorRunning_(false)
    , movementTimeout_(30000) { // 30秒超�?    
    SPDLOG_DEBUG("MovableBehavior '{}' created", behaviorName);
}

MovableBehavior::~MovableBehavior() {
    stop();
}

bool MovableBehavior::initialize(std::shared_ptr<core::StateManager> stateManager,
                                std::shared_ptr<core::ConfigManager> configManager) {
    if (!DeviceBehavior::initialize(stateManager, configManager)) {
        return false;
    }

    // 初始化移动相关配�?    initializeMovementConfigs();
    
    // 设置初始属�?    setProperty("currentPosition", currentPosition_.load());
    setProperty("targetPosition", targetPosition_.load());
    setProperty("movementState", static_cast<int>(movementState_.load()));
    setProperty("minPosition", minPosition_.load());
    setProperty("maxPosition", maxPosition_.load());
    setProperty("reversed", reversed_.load());
    setProperty("movementSpeed", movementSpeed_.load());
    
    SPDLOG_DEBUG("MovableBehavior '{}' initialized", behaviorName_);
    return true;
}

bool MovableBehavior::start() {
    if (!DeviceBehavior::start()) {
        return false;
    }

    // 启动移动监控线程
    startMovementMonitor();
    
    return true;
}

void MovableBehavior::stop() {
    // 停止任何正在进行的移�?    stopMovement();
    
    // 停止监控线程
    stopMovementMonitor();
    
    DeviceBehavior::stop();
}

void MovableBehavior::update() {
    // 更新属�?    setProperty("currentPosition", currentPosition_.load());
    setProperty("targetPosition", targetPosition_.load());
    setProperty("movementState", static_cast<int>(movementState_.load()));
}

bool MovableBehavior::handleCommand(const std::string& command, const json& parameters, json& result) {
    // 先尝试基类处�?    if (DeviceBehavior::handleCommand(command, parameters, result)) {
        return true;
    }

    // 处理移动相关命令
    if (command == "MOVE_TO_POSITION") {
        if (!parameters.contains("position")) {
            result["error"] = "Missing position parameter";
            return true;
        }
        
        int position = parameters["position"];
        bool success = moveToPosition(position);
        result["success"] = success;
        if (!success) {
            result["error"] = "Failed to start movement";
        }
        return true;
    }
    else if (command == "MOVE_RELATIVE") {
        if (!parameters.contains("steps")) {
            result["error"] = "Missing steps parameter";
            return true;
        }
        
        int steps = parameters["steps"];
        bool success = moveRelative(steps);
        result["success"] = success;
        if (!success) {
            result["error"] = "Failed to start relative movement";
        }
        return true;
    }
    else if (command == "STOP_MOVEMENT") {
        bool success = stopMovement();
        result["success"] = success;
        return true;
    }
    else if (command == "HOME") {
        bool success = home();
        result["success"] = success;
        if (!success) {
            result["error"] = "Failed to start homing";
        }
        return true;
    }
    else if (command == "CALIBRATE") {
        bool success = calibrate();
        result["success"] = success;
        if (!success) {
            result["error"] = "Failed to start calibration";
        }
        return true;
    }
    else if (command == "GET_POSITION") {
        result["currentPosition"] = getCurrentPosition();
        result["targetPosition"] = getTargetPosition();
        return true;
    }
    else if (command == "SET_POSITION_RANGE") {
        if (!parameters.contains("minPosition") || !parameters.contains("maxPosition")) {
            result["error"] = "Missing position range parameters";
            return true;
        }
        
        int minPos = parameters["minPosition"];
        int maxPos = parameters["maxPosition"];
        setPositionRange(minPos, maxPos);
        result["success"] = true;
        return true;
    }
    else if (command == "SET_MOVEMENT_SPEED") {
        if (!parameters.contains("speed")) {
            result["error"] = "Missing speed parameter";
            return true;
        }
        
        int speed = parameters["speed"];
        setMovementSpeed(speed);
        result["success"] = true;
        return true;
    }
    else if (command == "SET_REVERSED") {
        if (!parameters.contains("reversed")) {
            result["error"] = "Missing reversed parameter";
            return true;
        }
        
        bool reversed = parameters["reversed"];
        setReversed(reversed);
        result["success"] = true;
        return true;
    }

    return false;
}

json MovableBehavior::getStatus() const {
    json status = DeviceBehavior::getStatus();
    
    status["currentPosition"] = getCurrentPosition();
    status["targetPosition"] = getTargetPosition();
    status["movementState"] = static_cast<int>(getMovementState());
    status["isMoving"] = isMoving();
    status["minPosition"] = getMinPosition();
    status["maxPosition"] = getMaxPosition();
    status["reversed"] = isReversed();
    status["movementSpeed"] = getMovementSpeed();
    
    return status;
}

std::vector<std::string> MovableBehavior::getCapabilities() const {
    auto capabilities = DeviceBehavior::getCapabilities();
    
    std::vector<std::string> movableCapabilities = {
        "MOVE_TO_POSITION",
        "MOVE_RELATIVE", 
        "STOP_MOVEMENT",
        "HOME",
        "CALIBRATE",
        "GET_POSITION",
        "SET_POSITION_RANGE",
        "SET_MOVEMENT_SPEED",
        "SET_REVERSED"
    };
    
    capabilities.insert(capabilities.end(), movableCapabilities.begin(), movableCapabilities.end());
    return capabilities;
}

bool MovableBehavior::moveToPosition(int position, MovementCompleteCallback callback) {
    if (!isRunning()) {
        SPDLOG_WARN("MovableBehavior '{}' not running, cannot move", behaviorName_);
        return false;
    }

    if (!isValidPosition(position)) {
        SPDLOG_WARN("Invalid position {} for MovableBehavior '{}'", position, behaviorName_);
        return false;
    }

    if (isMoving()) {
        SPDLOG_WARN("MovableBehavior '{}' already moving, cannot start new movement", behaviorName_);
        return false;
    }

    std::lock_guard<std::mutex> lock(movementMutex_);
    
    targetPosition_ = position;
    currentCallback_ = callback;
    movementState_ = MovementState::MOVING;
    
    // 更新属�?    setProperty("targetPosition", position);
    setProperty("movementState", static_cast<int>(MovementState::MOVING));
    
    // 执行具体的移动操�?    bool success = executeMovement(position);
    if (!success) {
        movementState_ = MovementState::ERROR;
        setProperty("movementState", static_cast<int>(MovementState::ERROR));
        return false;
    }
    
    SPDLOG_INFO("MovableBehavior '{}' started movement to position {}", behaviorName_, position);
    return true;
}

bool MovableBehavior::moveRelative(int steps, MovementCompleteCallback callback) {
    int currentPos = getCurrentPosition();
    int targetPos = currentPos + steps;
    
    return moveToPosition(targetPos, callback);
}

bool MovableBehavior::stopMovement() {
    if (!isMoving()) {
        return true;
    }

    std::lock_guard<std::mutex> lock(movementMutex_);
    
    bool success = executeStop();
    
    movementState_ = success ? MovementState::IDLE : MovementState::ERROR;
    setProperty("movementState", static_cast<int>(movementState_.load()));
    
    // 通知移动完成
    if (currentCallback_) {
        currentCallback_(false, "Movement stopped");
        currentCallback_ = nullptr;
    }
    
    movementCV_.notify_all();
    
    SPDLOG_INFO("MovableBehavior '{}' movement stopped", behaviorName_);
    return success;
}

bool MovableBehavior::home(MovementCompleteCallback callback) {
    if (!isRunning()) {
        SPDLOG_WARN("MovableBehavior '{}' not running, cannot home", behaviorName_);
        return false;
    }

    if (isMoving()) {
        SPDLOG_WARN("MovableBehavior '{}' already moving, cannot start homing", behaviorName_);
        return false;
    }

    std::lock_guard<std::mutex> lock(movementMutex_);
    
    currentCallback_ = callback;
    movementState_ = MovementState::HOMING;
    setProperty("movementState", static_cast<int>(MovementState::HOMING));
    
    bool success = executeHome();
    if (!success) {
        movementState_ = MovementState::ERROR;
        setProperty("movementState", static_cast<int>(MovementState::ERROR));
        return false;
    }
    
    SPDLOG_INFO("MovableBehavior '{}' started homing", behaviorName_);
    return true;
}

bool MovableBehavior::calibrate(MovementCompleteCallback callback) {
    if (!isRunning()) {
        SPDLOG_WARN("MovableBehavior '{}' not running, cannot calibrate", behaviorName_);
        return false;
    }

    if (isMoving()) {
        SPDLOG_WARN("MovableBehavior '{}' already moving, cannot start calibration", behaviorName_);
        return false;
    }

    std::lock_guard<std::mutex> lock(movementMutex_);
    
    currentCallback_ = callback;
    movementState_ = MovementState::CALIBRATING;
    setProperty("movementState", static_cast<int>(MovementState::CALIBRATING));
    
    // 默认校准实现：先归零，然后移动到最大位置，再回到中间位�?    bool success = executeHome();
    if (!success) {
        movementState_ = MovementState::ERROR;
        setProperty("movementState", static_cast<int>(MovementState::ERROR));
        return false;
    }
    
    SPDLOG_INFO("MovableBehavior '{}' started calibration", behaviorName_);
    return true;
}

int MovableBehavior::getCurrentPosition() const {
    return currentPosition_.load();
}

int MovableBehavior::getTargetPosition() const {
    return targetPosition_.load();
}

MovementState MovableBehavior::getMovementState() const {
    return movementState_.load();
}

bool MovableBehavior::isMoving() const {
    MovementState state = movementState_.load();
    return state == MovementState::MOVING || 
           state == MovementState::HOMING || 
           state == MovementState::CALIBRATING;
}

void MovableBehavior::setPositionRange(int minPosition, int maxPosition) {
    if (minPosition >= maxPosition) {
        SPDLOG_WARN("Invalid position range for MovableBehavior '{}': min={}, max={}", 
                   behaviorName_, minPosition, maxPosition);
        return;
    }

    minPosition_ = minPosition;
    maxPosition_ = maxPosition;
    
    setProperty("minPosition", minPosition);
    setProperty("maxPosition", maxPosition);
    setConfig("minPosition", minPosition);
    setConfig("maxPosition", maxPosition);
    
    SPDLOG_DEBUG("MovableBehavior '{}' position range set: {} - {}", 
                behaviorName_, minPosition, maxPosition);
}

int MovableBehavior::getMinPosition() const {
    return minPosition_.load();
}

int MovableBehavior::getMaxPosition() const {
    return maxPosition_.load();
}

void MovableBehavior::setMovementSpeed(int speed) {
    if (speed <= 0) {
        SPDLOG_WARN("Invalid movement speed {} for MovableBehavior '{}'", speed, behaviorName_);
        return;
    }

    movementSpeed_ = speed;
    setProperty("movementSpeed", speed);
    setConfig("movementSpeed", speed);
    
    SPDLOG_DEBUG("MovableBehavior '{}' movement speed set to {}", behaviorName_, speed);
}

int MovableBehavior::getMovementSpeed() const {
    return movementSpeed_.load();
}

void MovableBehavior::setReversed(bool reversed) {
    reversed_ = reversed;
    setProperty("reversed", reversed);
    setConfig("reversed", reversed);
    
    SPDLOG_DEBUG("MovableBehavior '{}' reversed set to {}", behaviorName_, reversed);
}

bool MovableBehavior::isReversed() const {
    return reversed_.load();
}

void MovableBehavior::initializeMovementConfigs() {
    if (!configManager_) {
        return;
    }

    // 定义移动相关配置
    std::vector<core::ConfigDefinition> movementConfigs = {
        {
            .name = getConfigName("minPosition"),
            .type = core::ConfigType::INTEGER,
            .defaultValue = 0,
            .description = "Minimum position",
            .required = false,
            .readOnly = false
        },
        {
            .name = getConfigName("maxPosition"),
            .type = core::ConfigType::INTEGER,
            .defaultValue = 10000,
            .description = "Maximum position",
            .required = false,
            .readOnly = false
        },
        {
            .name = getConfigName("movementSpeed"),
            .type = core::ConfigType::INTEGER,
            .defaultValue = 100,
            .minValue = 1,
            .maxValue = 1000,
            .description = "Movement speed",
            .required = false,
            .readOnly = false
        },
        {
            .name = getConfigName("reversed"),
            .type = core::ConfigType::BOOLEAN,
            .defaultValue = false,
            .description = "Reverse movement direction",
            .required = false,
            .readOnly = false
        },
        {
            .name = getConfigName("movementTimeout"),
            .type = core::ConfigType::INTEGER,
            .defaultValue = 30000,
            .minValue = 1000,
            .maxValue = 300000,
            .description = "Movement timeout in milliseconds",
            .required = false,
            .readOnly = false
        }
    };
    
    configManager_->defineConfigs(movementConfigs);
    
    // 从配置加载�?    minPosition_ = getConfig("minPosition", 0);
    maxPosition_ = getConfig("maxPosition", 10000);
    movementSpeed_ = getConfig("movementSpeed", 100);
    reversed_ = getConfig("reversed", false);
    movementTimeout_ = getConfig("movementTimeout", 30000);
}

void MovableBehavior::updateCurrentPosition(int position) {
    currentPosition_ = position;
    setProperty("currentPosition", position);
}

void MovableBehavior::onMovementComplete(bool success, const std::string& error) {
    std::lock_guard<std::mutex> lock(movementMutex_);
    
    movementState_ = success ? MovementState::IDLE : MovementState::ERROR;
    setProperty("movementState", static_cast<int>(movementState_.load()));
    
    if (!success && !error.empty()) {
        setProperty("lastError", error);
        SPDLOG_ERROR("MovableBehavior '{}' movement failed: {}", behaviorName_, error);
    } else if (success) {
        SPDLOG_INFO("MovableBehavior '{}' movement completed successfully", behaviorName_);
    }
    
    // 调用回调函数
    if (currentCallback_) {
        currentCallback_(success, error);
        currentCallback_ = nullptr;
    }
    
    movementCV_.notify_all();
}

bool MovableBehavior::isValidPosition(int position) const {
    return position >= minPosition_.load() && position <= maxPosition_.load();
}

void MovableBehavior::movementMonitorLoop() {
    while (monitorRunning_) {
        try {
            // 检查移动超�?            if (isMoving()) {
                // 这里可以添加超时检查逻辑
                // 具体实现取决于子类的需�?            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in movement monitor for MovableBehavior '{}': {}", 
                        behaviorName_, e.what());
        }
    }
}

void MovableBehavior::startMovementMonitor() {
    if (monitorRunning_) {
        return;
    }
    
    monitorRunning_ = true;
    monitorThread_ = std::thread(&MovableBehavior::movementMonitorLoop, this);
    
    SPDLOG_DEBUG("Movement monitor started for MovableBehavior '{}'", behaviorName_);
}

void MovableBehavior::stopMovementMonitor() {
    if (!monitorRunning_) {
        return;
    }
    
    monitorRunning_ = false;
    
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    
    SPDLOG_DEBUG("Movement monitor stopped for MovableBehavior '{}'", behaviorName_);
}

} // namespace behaviors
} // namespace device
} // namespace hydrogen

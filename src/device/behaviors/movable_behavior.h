#pragma once

#include "device_behavior.h"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>

namespace hydrogen {
namespace device {
namespace behaviors {

/**
 * @brief 移动状态枚�? */
enum class MovementState {
    IDLE,           // Idle state
    MOVING,         // Moving
    HOMING,         // Homing
    CALIBRATING,    // Calibrating
    MOVEMENT_ERROR  // Error state
};

/**
 * @brief Movement direction enumeration
 */
enum class MovementDirection {
    INWARD,         // Move inward
    OUTWARD,        // Move outward
    POSITIVE,       // Positive direction
    NEGATIVE,       // Negative direction
    CLOCKWISE,      // Clockwise
    COUNTERCLOCKWISE // Counterclockwise
};

/**
 * @brief Movement completion callback function type
 */
using MovementCompleteCallback = std::function<void(bool success, const std::string& error)>;

/**
 * @brief Movable device behavior component
 *
 * Provides common movement control functionality for focusers, filter wheels, rotators and other movable devices.
 * Supports absolute position movement, relative position movement, homing, calibration and other functions.
 */
class MovableBehavior : public DeviceBehavior {
public:
    /**
     * @brief 构造函�?     * @param behaviorName 行为名称
     */
    explicit MovableBehavior(const std::string& behaviorName = "movable");
    
    /**
     * @brief 析构函数
     */
    virtual ~MovableBehavior();

    /**
     * @brief 获取行为类型名称
     */
    static std::string getTypeName() { return "MovableBehavior"; }

    // 重写基类方法
    bool initialize(std::shared_ptr<core::StateManager> stateManager,
                   std::shared_ptr<core::ConfigManager> configManager) override;
    bool start() override;
    void stop() override;
    void update() override;
    bool handleCommand(const std::string& command, const json& parameters, json& result) override;
    json getStatus() const override;
    std::vector<std::string> getCapabilities() const override;

    /**
     * @brief 移动到绝对位�?     * @param position 目标位置
     * @param callback 完成回调（可选）
     * @return 移动是否开始成�?     */
    virtual bool moveToPosition(int position, MovementCompleteCallback callback = nullptr);

    /**
     * @brief 相对移动
     * @param steps 移动步数（正数向�?正方向，负数向内/负方向）
     * @param callback 完成回调（可选）
     * @return 移动是否开始成�?     */
    virtual bool moveRelative(int steps, MovementCompleteCallback callback = nullptr);

    /**
     * @brief 停止移动
     * @return 停止是否成功
     */
    virtual bool stopMovement();

    /**
     * @brief 归零操作
     * @param callback 完成回调（可选）
     * @return 归零是否开始成�?     */
    virtual bool home(MovementCompleteCallback callback = nullptr);

    /**
     * @brief 校准操作
     * @param callback 完成回调（可选）
     * @return 校准是否开始成�?     */
    virtual bool calibrate(MovementCompleteCallback callback = nullptr);

    /**
     * @brief 获取当前位置
     * @return 当前位置
     */
    virtual int getCurrentPosition() const;

    /**
     * @brief 获取目标位置
     * @return 目标位置
     */
    virtual int getTargetPosition() const;

    /**
     * @brief 获取移动状�?     * @return 移动状�?     */
    virtual MovementState getMovementState() const;

    /**
     * @brief 是否正在移动
     * @return 移动状�?     */
    virtual bool isMoving() const;

    /**
     * @brief 设置位置范围
     * @param minPosition 最小位�?     * @param maxPosition 最大位�?     */
    virtual void setPositionRange(int minPosition, int maxPosition);

    /**
     * @brief 获取最小位�?     * @return 最小位�?     */
    virtual int getMinPosition() const;

    /**
     * @brief 获取最大位�?     * @return 最大位�?     */
    virtual int getMaxPosition() const;

    /**
     * @brief 设置移动速度
     * @param speed 移动速度（设备相关单位）
     */
    virtual void setMovementSpeed(int speed);

    /**
     * @brief 获取移动速度
     * @return 移动速度
     */
    virtual int getMovementSpeed() const;

    /**
     * @brief 设置反向标志
     * @param reversed 是否反向
     */
    virtual void setReversed(bool reversed);

    /**
     * @brief 是否反向
     * @return 反向状�?     */
    virtual bool isReversed() const;

    /**
     * @brief Update current position (called by subclasses)
     * @param position New position
     */
    virtual void updateCurrentPosition(int position);

    /**
     * @brief Movement completion handler
     * @param success Whether successful
     * @param error Error message
     */
    virtual void onMovementComplete(bool success, const std::string& error = "");

protected:
    /**
     * @brief 初始化移动行为配�?     */
    virtual void initializeMovementConfigs();

    /**
     * @brief 执行移动操作（子类实现）
     * @param targetPosition 目标位置
     * @return 移动是否成功启动
     */
    virtual bool executeMovement(int targetPosition) = 0;

    /**
     * @brief 执行停止操作（子类实现）
     * @return 停止是否成功
     */
    virtual bool executeStop() = 0;

    /**
     * @brief 执行归零操作（子类实现）
     * @return 归零是否成功启动
     */
    virtual bool executeHome() = 0;

    /**
     * @brief 验证位置是否有效
     * @param position 位置�?     * @return 是否有效
     */
    virtual bool isValidPosition(int position) const;

    /**
     * @brief 移动监控线程函数
     */
    virtual void movementMonitorLoop();

    /**
     * @brief 启动移动监控线程
     */
    void startMovementMonitor();

    /**
     * @brief 停止移动监控线程
     */
    void stopMovementMonitor();

protected:
    // 位置信息
    std::atomic<int> currentPosition_;
    std::atomic<int> targetPosition_;
    std::atomic<int> minPosition_;
    std::atomic<int> maxPosition_;
    
    // 移动状�?    std::atomic<MovementState> movementState_;
    std::atomic<bool> reversed_;
    std::atomic<int> movementSpeed_;
    
    // 移动控制
    mutable std::mutex movementMutex_;
    std::condition_variable movementCV_;
    MovementCompleteCallback currentCallback_;
    
    // 监控线程
    std::thread monitorThread_;
    std::atomic<bool> monitorRunning_;
    
    // 移动超时（毫秒）
    std::atomic<int> movementTimeout_;
};

} // namespace behaviors
} // namespace device
} // namespace hydrogen

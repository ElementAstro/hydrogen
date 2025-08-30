#pragma once

#include "device_behavior.h"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>

namespace astrocomm {
namespace device {
namespace behaviors {

/**
 * @brief 移动状态枚举
 */
enum class MovementState {
    IDLE,           // 空闲状态
    MOVING,         // 正在移动
    HOMING,         // 正在归零
    CALIBRATING,    // 正在校准
    ERROR           // 错误状态
};

/**
 * @brief 移动方向枚举
 */
enum class MovementDirection {
    INWARD,         // 向内移动
    OUTWARD,        // 向外移动
    POSITIVE,       // 正方向
    NEGATIVE,       // 负方向
    CLOCKWISE,      // 顺时针
    COUNTERCLOCKWISE // 逆时针
};

/**
 * @brief 移动完成回调函数类型
 */
using MovementCompleteCallback = std::function<void(bool success, const std::string& error)>;

/**
 * @brief 可移动设备行为组件
 * 
 * 提供通用的移动控制功能，适用于调焦器、滤镜轮、旋转器等可移动设备。
 * 支持绝对位置移动、相对位置移动、归零、校准等功能。
 */
class MovableBehavior : public DeviceBehavior {
public:
    /**
     * @brief 构造函数
     * @param behaviorName 行为名称
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
     * @brief 移动到绝对位置
     * @param position 目标位置
     * @param callback 完成回调（可选）
     * @return 移动是否开始成功
     */
    virtual bool moveToPosition(int position, MovementCompleteCallback callback = nullptr);

    /**
     * @brief 相对移动
     * @param steps 移动步数（正数向外/正方向，负数向内/负方向）
     * @param callback 完成回调（可选）
     * @return 移动是否开始成功
     */
    virtual bool moveRelative(int steps, MovementCompleteCallback callback = nullptr);

    /**
     * @brief 停止移动
     * @return 停止是否成功
     */
    virtual bool stopMovement();

    /**
     * @brief 归零操作
     * @param callback 完成回调（可选）
     * @return 归零是否开始成功
     */
    virtual bool home(MovementCompleteCallback callback = nullptr);

    /**
     * @brief 校准操作
     * @param callback 完成回调（可选）
     * @return 校准是否开始成功
     */
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
     * @brief 获取移动状态
     * @return 移动状态
     */
    virtual MovementState getMovementState() const;

    /**
     * @brief 是否正在移动
     * @return 移动状态
     */
    virtual bool isMoving() const;

    /**
     * @brief 设置位置范围
     * @param minPosition 最小位置
     * @param maxPosition 最大位置
     */
    virtual void setPositionRange(int minPosition, int maxPosition);

    /**
     * @brief 获取最小位置
     * @return 最小位置
     */
    virtual int getMinPosition() const;

    /**
     * @brief 获取最大位置
     * @return 最大位置
     */
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
     * @return 反向状态
     */
    virtual bool isReversed() const;

protected:
    /**
     * @brief 初始化移动行为配置
     */
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
     * @brief 更新当前位置（子类调用）
     * @param position 新位置
     */
    virtual void updateCurrentPosition(int position);

    /**
     * @brief 移动完成处理
     * @param success 是否成功
     * @param error 错误信息
     */
    virtual void onMovementComplete(bool success, const std::string& error = "");

    /**
     * @brief 验证位置是否有效
     * @param position 位置值
     * @return 是否有效
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
    
    // 移动状态
    std::atomic<MovementState> movementState_;
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
} // namespace astrocomm

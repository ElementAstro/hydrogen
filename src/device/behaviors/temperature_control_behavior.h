#pragma once

#include "device_behavior.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

namespace hydrogen {
namespace device {
namespace behaviors {

/**
 * @brief 温度控制状态枚�? */
enum class TemperatureControlState {
    IDLE,           // Idle state
    HEATING,        // Heating
    COOLING,        // Cooling
    STABILIZING,    // Stabilizing temperature
    CONTROL_ERROR   // Error state
};

/**
 * @brief Temperature control mode enumeration
 */
enum class TemperatureControlMode {
    MANUAL,         // 手动模式
    AUTO,           // 自动模式
    PID             // PID控制模式
};

/**
 * @brief 温度稳定回调函数类型
 */
using TemperatureStabilizedCallback = std::function<void(bool success, double finalTemperature)>;

/**
 * @brief 温度控制行为组件
 * 
 * 提供通用的温度控制功能，适用于相机制冷、加热器等温度控制设备�? * 支持目标温度设置、PID控制、温度监控等功能�? */
class TemperatureControlBehavior : public DeviceBehavior {
public:
    /**
     * @brief 构造函�?     * @param behaviorName 行为名称
     */
    explicit TemperatureControlBehavior(const std::string& behaviorName = "temperature_control");
    
    /**
     * @brief 析构函数
     */
    virtual ~TemperatureControlBehavior();

    /**
     * @brief 获取行为类型名称
     */
    static std::string getTypeName() { return "TemperatureControlBehavior"; }

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
     * @brief 设置目标温度
     * @param temperature 目标温度（摄氏度�?     * @param callback 稳定回调（可选）
     * @return 设置是否成功
     */
    virtual bool setTargetTemperature(double temperature, TemperatureStabilizedCallback callback = nullptr);

    /**
     * @brief 获取当前温度
     * @return 当前温度（摄氏度�?     */
    virtual double getCurrentTemperature() const;

    /**
     * @brief 获取目标温度
     * @return 目标温度（摄氏度�?     */
    virtual double getTargetTemperature() const;

    /**
     * @brief 获取环境温度
     * @return 环境温度（摄氏度�?     */
    virtual double getAmbientTemperature() const;

    /**
     * @brief 获取温度控制状�?     * @return 控制状�?     */
    virtual TemperatureControlState getControlState() const;

    /**
     * @brief 获取温度控制模式
     * @return 控制模式
     */
    virtual TemperatureControlMode getControlMode() const;

    /**
     * @brief 设置温度控制模式
     * @param mode 控制模式
     */
    virtual void setControlMode(TemperatureControlMode mode);

    /**
     * @brief 是否正在控制温度
     * @return 控制状�?     */
    virtual bool isControlling() const;

    /**
     * @brief 温度是否已稳�?     * @return 稳定状�?     */
    virtual bool isTemperatureStable() const;

    /**
     * @brief 停止温度控制
     * @return 停止是否成功
     */
    virtual bool stopControl();

    /**
     * @brief 设置温度范围
     * @param minTemp 最低温�?     * @param maxTemp 最高温�?     */
    virtual void setTemperatureRange(double minTemp, double maxTemp);

    /**
     * @brief 获取最低温�?     * @return 最低温�?     */
    virtual double getMinTemperature() const;

    /**
     * @brief 获取最高温�?     * @return 最高温�?     */
    virtual double getMaxTemperature() const;

    /**
     * @brief 设置PID参数
     * @param kp 比例系数
     * @param ki 积分系数
     * @param kd 微分系数
     */
    virtual void setPIDParameters(double kp, double ki, double kd);

    /**
     * @brief 获取PID参数
     * @param kp 比例系数输出
     * @param ki 积分系数输出
     * @param kd 微分系数输出
     */
    virtual void getPIDParameters(double& kp, double& ki, double& kd) const;

    /**
     * @brief 获取控制功率百分�?     * @return 功率百分�?(0-100)
     */
    virtual double getControlPower() const;

protected:
    /**
     * @brief 初始化温度控制配�?     */
    virtual void initializeTemperatureConfigs();

    /**
     * @brief 读取当前温度（子类实现）
     * @return 当前温度
     */
    virtual double readCurrentTemperature() = 0;

    /**
     * @brief 读取环境温度（子类实现）
     * @return 环境温度
     */
    virtual double readAmbientTemperature() = 0;

    /**
     * @brief 设置控制功率（子类实现）
     * @param power 功率百分�?(0-100)
     * @return 设置是否成功
     */
    virtual bool setControlPower(double power) = 0;

    /**
     * @brief 更新当前温度（子类调用）
     * @param temperature 新温�?     */
    virtual void updateCurrentTemperature(double temperature);

    /**
     * @brief 更新环境温度（子类调用）
     * @param temperature 环境温度
     */
    virtual void updateAmbientTemperature(double temperature);

    /**
     * @brief 温度稳定检�?     * @return 是否稳定
     */
    virtual bool checkTemperatureStability();

    /**
     * @brief 温度控制完成处理
     * @param success 是否成功
     * @param finalTemperature 最终温�?     */
    virtual void onTemperatureStabilized(bool success, double finalTemperature);

    /**
     * @brief 验证温度是否有效
     * @param temperature 温度�?     * @return 是否有效
     */
    virtual bool isValidTemperature(double temperature) const;

    /**
     * @brief PID控制计算
     * @param error 温度误差
     * @param deltaTime 时间间隔
     * @return 控制输出
     */
    virtual double calculatePIDOutput(double error, double deltaTime);

    /**
     * @brief 温度控制循环
     */
    virtual void temperatureControlLoop();

    /**
     * @brief 启动温度控制线程
     */
    void startTemperatureControl();

    /**
     * @brief 停止温度控制线程
     */
    void stopTemperatureControl();

protected:
    // 温度信息
    std::atomic<double> currentTemperature_;
    std::atomic<double> targetTemperature_;
    std::atomic<double> ambientTemperature_;
    std::atomic<double> minTemperature_;
    std::atomic<double> maxTemperature_;
    
    // Control state
    std::atomic<TemperatureControlState> controlState_;
    std::atomic<TemperatureControlMode> controlMode_;
    std::atomic<double> controlPower_;
    
    // PID参数
    std::atomic<double> pidKp_;
    std::atomic<double> pidKi_;
    std::atomic<double> pidKd_;
    double pidIntegral_;
    double pidLastError_;
    
    // 稳定性检�?    std::atomic<double> stabilityTolerance_;
    std::atomic<int> stabilityDuration_;
    std::chrono::steady_clock::time_point stabilityStartTime_;
    
    // 控制线程
    mutable std::mutex controlMutex_;
    std::thread controlThread_;
    std::atomic<bool> controlRunning_;
    TemperatureStabilizedCallback currentCallback_;
    
    // Control parameters
    std::atomic<int> controlInterval_; // Control loop interval (milliseconds)
    std::atomic<int> stabilizationTimeout_; // Stabilization timeout (seconds)
};

} // namespace behaviors
} // namespace device
} // namespace hydrogen

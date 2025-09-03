#pragma once

#include "../interfaces/device_interface.h"
#include "device_manager.h"
#include "../behaviors/device_behavior.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>

namespace hydrogen {
namespace device {
namespace core {

/**
 * @brief 现代化设备基�? * 
 * 基于组合模式的新设备基类，使用DeviceManager和行为组件来实现功能�? * 相比旧的DeviceBase类，这个类更加模块化和可扩展�? */
class ModernDeviceBase : public interfaces::IDevice,
                        public interfaces::IConfigurable,
                        public interfaces::IStateful {
public:
    /**
     * @brief 构造函�?     * @param deviceId 设备ID
     * @param deviceType 设备类型
     * @param manufacturer 制造商
     * @param model 型号
     */
    ModernDeviceBase(const std::string& deviceId,
                    const std::string& deviceType,
                    const std::string& manufacturer = "",
                    const std::string& model = "");

    /**
     * @brief 虚析构函�?     */
    virtual ~ModernDeviceBase();

    // 实现IDevice接口
    std::string getDeviceId() const override;
    std::string getDeviceType() const override;
    json getDeviceInfo() const override;
    bool initialize() override;
    bool connect(const std::string& host, uint16_t port) override;
    void disconnect() override;
    bool start() override;
    void stop() override;
    bool isConnected() const override;
    bool isRunning() const override;

    // 实现IConfigurable接口
    bool setConfig(const std::string& name, const json& value) override;
    json getConfig(const std::string& name) const override;
    json getAllConfigs() const override;
    bool saveConfig() override;
    bool loadConfig() override;

    // 实现IStateful接口
    bool setProperty(const std::string& property, const json& value) override;
    json getProperty(const std::string& property) const override;
    json getAllProperties() const override;
    std::vector<std::string> getCapabilities() const override;

    /**
     * @brief 添加行为组件
     * @param behavior 行为组件
     * @return 添加是否成功
     */
    bool addBehavior(std::unique_ptr<behaviors::DeviceBehavior> behavior);

    /**
     * @brief 移除行为组件
     * @param behaviorName 行为名称
     * @return 移除是否成功
     */
    bool removeBehavior(const std::string& behaviorName);

    /**
     * @brief 获取行为组件
     * @param behaviorName 行为名称
     * @return 行为组件指针，如果不存在返回nullptr
     */
    behaviors::DeviceBehavior* getBehavior(const std::string& behaviorName);

    /**
     * @brief 获取指定类型的行为组�?     * @tparam T 行为类型
     * @param behaviorName 行为名称
     * @return 行为组件指针，如果不存在或类型不匹配返回nullptr
     */
    template<typename T>
    T* getBehavior(const std::string& behaviorName) {
        auto* behavior = getBehavior(behaviorName);
        return dynamic_cast<T*>(behavior);
    }

    /**
     * @brief 检查是否具有指定行�?     * @param behaviorName 行为名称
     * @return 是否具有该行�?     */
    bool hasBehavior(const std::string& behaviorName) const;

    /**
     * @brief 获取所有行为名�?     * @return 行为名称列表
     */
    std::vector<std::string> getBehaviorNames() const;

    /**
     * @brief 处理命令
     * @param command 命令名称
     * @param parameters 命令参数
     * @param result 命令结果输出
     * @return 命令是否被处�?     */
    virtual bool handleCommand(const std::string& command, 
                              const json& parameters, 
                              json& result);

    /**
     * @brief 发送消�?     * @param message 消息内容
     * @return 发送是否成�?     */
    bool sendMessage(const std::string& message);

    /**
     * @brief 发送JSON消息
     * @param jsonMessage JSON消息
     * @return 发送是否成�?     */
    bool sendMessage(const json& jsonMessage);

    /**
     * @brief 注册设备到服务器
     * @return 注册是否成功
     */
    virtual bool registerDevice();

    /**
     * @brief Get device manager
     * @return Device manager pointer
     */
    std::shared_ptr<DeviceManager> getDeviceManager() { return deviceManager_; }

protected:
    /**
     * @brief Initialize device-specific functionality (override in subclasses)
     * @return Whether initialization was successful
     */
    virtual bool initializeDevice() { return true; }

    /**
     * @brief 启动设备特定功能（子类重写）
     * @return 启动是否成功
     */
    virtual bool startDevice() { return true; }

    /**
     * @brief 停止设备特定功能（子类重写）
     */
    virtual void stopDevice() {}

    /**
     * @brief 处理设备特定命令（子类重写）
     * @param command 命令名称
     * @param parameters 命令参数
     * @param result 命令结果输出
     * @return 命令是否被处�?     */
    virtual bool handleDeviceCommand(const std::string& command,
                                   const json& parameters,
                                   json& result) {
        (void)command; (void)parameters; (void)result;
        return false;
    }

    /**
     * @brief 更新设备状态（定期调用�?     */
    virtual void updateDevice() {}

    /**
     * @brief 启动更新线程
     */
    void startUpdateThread();

    /**
     * @brief 停止更新线程
     */
    void stopUpdateThread();

    /**
     * @brief 更新循环线程函数
     */
    void updateLoop();

private:
    /**
     * @brief 初始化所有行为组�?     * @return 初始化是否成�?     */
    bool initializeBehaviors();

    /**
     * @brief 启动所有行为组�?     * @return 启动是否成功
     */
    bool startBehaviors();

    /**
     * @brief 停止所有行为组�?     */
    void stopBehaviors();

    /**
     * @brief Update all behavior components
     */
    void updateBehaviors();

protected:
    // Device manager
    std::shared_ptr<DeviceManager> deviceManager_;

    // 行为组件管理
    mutable std::mutex behaviorsMutex_;
    std::unordered_map<std::string, std::unique_ptr<behaviors::DeviceBehavior>> behaviors_;

    // 更新线程
    std::thread updateThread_;
    std::atomic<bool> updateRunning_;
    std::atomic<int> updateInterval_; // 更新间隔（毫秒）

private:
    // 设备基本信息
    std::string deviceId_;
    std::string deviceType_;
    std::string manufacturer_;
    std::string model_;
};

/**
 * @brief 设备工厂基类
 */
class DeviceFactory {
public:
    virtual ~DeviceFactory() = default;
    
    /**
     * @brief 创建设备实例
     * @param deviceId 设备ID
     * @return 设备实例
     */
    virtual std::unique_ptr<ModernDeviceBase> createDevice(const std::string& deviceId) = 0;
    
    /**
     * @brief 获取设备类型名称
     * @return 设备类型名称
     */
    virtual std::string getDeviceType() const = 0;
    
    /**
     * @brief 获取支持的制造商列表
     * @return 制造商列表
     */
    virtual std::vector<std::string> getSupportedManufacturers() const = 0;
    
    /**
     * @brief 获取支持的型号列�?     * @param manufacturer 制造商
     * @return 型号列表
     */
    virtual std::vector<std::string> getSupportedModels(const std::string& manufacturer) const = 0;
};

/**
 * @brief 模板化设备工�? */
template<typename DeviceType>
class TypedDeviceFactory : public DeviceFactory {
public:
    explicit TypedDeviceFactory(const std::string& manufacturer = "", const std::string& model = "")
        : manufacturer_(manufacturer), model_(model) {}
    
    std::unique_ptr<ModernDeviceBase> createDevice(const std::string& deviceId) override {
        return std::make_unique<DeviceType>(deviceId, manufacturer_, model_);
    }
    
    std::string getDeviceType() const override {
        return DeviceType::getDeviceTypeName();
    }
    
    std::vector<std::string> getSupportedManufacturers() const override {
        return DeviceType::getSupportedManufacturers();
    }
    
    std::vector<std::string> getSupportedModels(const std::string& manufacturer) const override {
        return DeviceType::getSupportedModels(manufacturer);
    }

private:
    std::string manufacturer_;
    std::string model_;
};

} // namespace core
} // namespace device
} // namespace hydrogen

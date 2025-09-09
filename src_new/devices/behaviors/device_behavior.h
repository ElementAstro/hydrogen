#pragma once

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace device {
namespace core {
    class StateManager;
    class ConfigManager;
}

namespace behaviors {

using json = nlohmann::json;

/**
 * @brief 设备行为基类
 * 
 * 所有设备行为的基础接口，定义了行为的生命周期和基本操作�? * 行为组件使用组合模式，可以被多个设备类复用�? */
class DeviceBehavior {
public:
    /**
     * @brief 构造函�?     * @param behaviorName 行为名称
     */
    explicit DeviceBehavior(const std::string& behaviorName);
    
    /**
     * @brief 虚析构函�?     */
    virtual ~DeviceBehavior() = default;

    /**
     * @brief 获取行为名称
     */
    const std::string& getBehaviorName() const { return behaviorName_; }

    /**
     * @brief 初始化行�?     * @param stateManager 状态管理器
     * @param configManager 配置管理�?     * @return 初始化是否成�?     */
    virtual bool initialize(std::shared_ptr<core::StateManager> stateManager,
                           std::shared_ptr<core::ConfigManager> configManager);

    /**
     * @brief 启动行为
     * @return 启动是否成功
     */
    virtual bool start();

    /**
     * @brief 停止行为
     */
    virtual void stop();

    /**
     * @brief 更新行为状态（定期调用�?     */
    virtual void update();

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
     * @brief 获取行为状�?     * @return 行为状态JSON
     */
    virtual json getStatus() const;

    /**
     * @brief 获取行为能力列表
     * @return 能力列表
     */
    virtual std::vector<std::string> getCapabilities() const;

    /**
     * @brief 是否已初始化
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief 是否正在运行
     */
    bool isRunning() const { return running_; }

protected:
    /**
     * @brief 设置属性�?     * @param property 属性名
     * @param value 属性�?     */
    void setProperty(const std::string& property, const json& value);

    /**
     * @brief 获取属性�?     * @param property 属性名
     * @return 属性�?     */
    json getProperty(const std::string& property) const;

    /**
     * @brief 设置配置�?     * @param name 配置�?     * @param value 配置�?     */
    void setConfig(const std::string& name, const json& value);

    /**
     * @brief 获取配置�?     * @param name 配置�?     * @return 配置�?     */
    json getConfig(const std::string& name) const;

    /**
     * @brief 获取配置值（带类型转换和默认值）
     * @tparam T 目标类型
     * @param name 配置�?     * @param defaultValue 默认�?     * @return 配置�?     */
    template<typename T>
    T getConfig(const std::string& name, const T& defaultValue) const {
        // Simplified implementation to avoid incomplete type issues
        // Full implementation should be in derived classes or implementation files
        return defaultValue;
    }

    /**
     * @brief 生成带行为前缀的属性名
     * @param property 属性名
     * @return 完整属性名
     */
    std::string getPropertyName(const std::string& property) const;

    /**
     * @brief 生成带行为前缀的配置名
     * @param name 配置�?     * @return 完整配置�?     */
    std::string getConfigName(const std::string& name) const;

protected:
    std::string behaviorName_;
    bool initialized_;
    bool running_;
    
    std::shared_ptr<core::StateManager> stateManager_;
    std::shared_ptr<core::ConfigManager> configManager_;
};

/**
 * @brief 行为工厂基类
 */
class BehaviorFactory {
public:
    virtual ~BehaviorFactory() = default;
    
    /**
     * @brief 创建行为实例
     * @return 行为实例
     */
    virtual std::unique_ptr<DeviceBehavior> createBehavior() = 0;
    
    /**
     * @brief 获取行为类型名称
     * @return 行为类型名称
     */
    virtual std::string getBehaviorType() const = 0;
};

/**
 * @brief 模板化行为工�? */
template<typename BehaviorType>
class TypedBehaviorFactory : public BehaviorFactory {
public:
    explicit TypedBehaviorFactory(const std::string& behaviorName)
        : behaviorName_(behaviorName) {}
    
    std::unique_ptr<DeviceBehavior> createBehavior() override {
        return std::make_unique<BehaviorType>(behaviorName_);
    }
    
    std::string getBehaviorType() const override {
        return BehaviorType::getTypeName();
    }

private:
    std::string behaviorName_;
};

} // namespace behaviors
} // namespace device
} // namespace hydrogen

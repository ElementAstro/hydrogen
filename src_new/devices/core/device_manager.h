#pragma once

#include "communication_manager.h"
#include "state_manager.h"
#include "config_manager.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <thread>
#include <atomic>

namespace hydrogen {
namespace device {
namespace core {

/**
 * @brief 设备管理�?- 统一管理设备的核心组�? * 
 * 该类整合了通信管理器、状态管理器和配置管理器�? * 为设备提供统一的管理接口，简化设备实现�? */
class DeviceManager {
public:
    /**
     * @brief 构造函�?     * @param deviceId 设备ID
     * @param deviceType 设备类型
     * @param manufacturer 制造商
     * @param model 型号
     */
    DeviceManager(const std::string& deviceId, 
                  const std::string& deviceType,
                  const std::string& manufacturer = "",
                  const std::string& model = "");
    
    /**
     * @brief 析构函数
     */
    ~DeviceManager();

    /**
     * @brief 获取设备ID
     */
    const std::string& getDeviceId() const { return deviceId_; }

    /**
     * @brief 获取设备类型
     */
    const std::string& getDeviceType() const { return deviceType_; }

    /**
     * @brief 获取制造商
     */
    const std::string& getManufacturer() const { return manufacturer_; }

    /**
     * @brief 获取型号
     */
    const std::string& getModel() const { return model_; }

    /**
     * @brief 获取通信管理�?     */
    std::shared_ptr<CommunicationManager> getCommunicationManager() { return commManager_; }

    /**
     * @brief 获取状态管理器
     */
    std::shared_ptr<StateManager> getStateManager() { return stateManager_; }

    /**
     * @brief 获取配置管理�?     */
    std::shared_ptr<ConfigManager> getConfigManager() { return configManager_; }

    /**
     * @brief 初始化设�?     * @return 初始化是否成�?     */
    virtual bool initialize();

    /**
     * @brief 连接到服务器
     * @param host 服务器地址
     * @param port 服务器端�?     * @return 连接是否成功
     */
    virtual bool connect(const std::string& host, uint16_t port);

    /**
     * @brief 断开连接
     */
    virtual void disconnect();

    /**
     * @brief 启动设备
     * @return 启动是否成功
     */
    virtual bool start();

    /**
     * @brief 停止设备
     */
    virtual void stop();

    /**
     * @brief 注册设备到服务器
     * @return 注册是否成功
     */
    virtual bool registerDevice();

    /**
     * @brief 获取设备信息JSON
     */
    virtual json getDeviceInfo() const;

    /**
     * @brief 设置设备属�?     * @param property 属性名
     * @param value 属性�?     * @return 设置是否成功
     */
    bool setProperty(const std::string& property, const json& value);

    /**
     * @brief 获取设备属�?     * @param property 属性名
     * @return 属性�?     */
    json getProperty(const std::string& property) const;

    /**
     * @brief 设置设备配置
     * @param name 配置�?     * @param value 配置�?     * @return 设置是否成功
     */
    bool setConfig(const std::string& name, const json& value);

    /**
     * @brief 获取设备配置
     * @param name 配置�?     * @return 配置�?     */
    json getConfig(const std::string& name) const;

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
     * @brief 是否已连�?     */
    bool isConnected() const;

    /**
     * @brief 是否正在运行
     */
    bool isRunning() const { return running_; }

    /**
     * @brief 设置消息处理�?     * @param handler 消息处理函数
     */
    void setMessageHandler(MessageHandler handler);

    /**
     * @brief 设置连接状态处理器
     * @param handler 连接状态变化处理函�?     */
    void setConnectionStateHandler(ConnectionStateHandler handler);

protected:
    /**
     * @brief 初始化基础属�?     */
    virtual void initializeBaseProperties();

    /**
     * @brief 初始化基础配置
     */
    virtual void initializeBaseConfigs();

    /**
     * @brief 处理接收到的消息
     * @param message 消息内容
     */
    virtual void handleMessage(const std::string& message);

    /**
     * @brief 处理连接状态变�?     * @param state 新的连接状�?     * @param error 错误信息
     */
    virtual void handleConnectionStateChange(ConnectionState state, const std::string& error);

    /**
     * @brief 发送设备状态更新事�?     */
    virtual void sendStatusUpdate();

    /**
     * @brief 状态更新线程函�?     */
    virtual void statusUpdateLoop();

    /**
     * @brief 启动状态更新线�?     */
    void startStatusUpdateThread();

    /**
     * @brief Stop status update thread
     */
    void stopStatusUpdateThread();

protected:
    // 设备基本信息
    std::string deviceId_;
    std::string deviceType_;
    std::string manufacturer_;
    std::string model_;
    std::string firmwareVersion_;

    // 核心组件
    std::shared_ptr<CommunicationManager> commManager_;
    std::shared_ptr<StateManager> stateManager_;
    std::shared_ptr<ConfigManager> configManager_;

    // Runtime state
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;

    // Status update thread
    std::thread statusUpdateThread_;
    std::atomic<bool> statusUpdateRunning_;
    std::atomic<int> statusUpdateInterval_; // milliseconds

    // Mutex
    mutable std::mutex managerMutex_;
};

} // namespace core
} // namespace device
} // namespace hydrogen

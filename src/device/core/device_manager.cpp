#include "device_manager.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace astrocomm {
namespace device {
namespace core {

DeviceManager::DeviceManager(const std::string& deviceId, 
                           const std::string& deviceType,
                           const std::string& manufacturer,
                           const std::string& model)
    : deviceId_(deviceId)
    , deviceType_(deviceType)
    , manufacturer_(manufacturer)
    , model_(model)
    , firmwareVersion_("1.0.0")
    , running_(false)
    , initialized_(false)
    , statusUpdateRunning_(false)
    , statusUpdateInterval_(5) {
    
    // 创建核心组件
    commManager_ = std::make_shared<CommunicationManager>(deviceId_);
    stateManager_ = std::make_shared<StateManager>(deviceId_);
    configManager_ = std::make_shared<ConfigManager>(deviceId_);
    
    SPDLOG_INFO("DeviceManager created for device: {} ({})", deviceId_, deviceType_);
}

DeviceManager::~DeviceManager() {
    stop();
    disconnect();
    SPDLOG_INFO("DeviceManager destroyed for device: {}", deviceId_);
}

bool DeviceManager::initialize() {
    if (initialized_) {
        SPDLOG_WARN("Device {} already initialized", deviceId_);
        return true;
    }

    try {
        // 初始化基础属性和配置
        initializeBaseProperties();
        initializeBaseConfigs();
        
        // 加载配置文件
        configManager_->loadFromFile();
        
        // 设置消息和连接状态处理器
        commManager_->setMessageHandler(
            [this](const std::string& message) {
                handleMessage(message);
            });
        
        commManager_->setConnectionStateHandler(
            [this](ConnectionState state, const std::string& error) {
                handleConnectionStateChange(state, error);
            });
        
        initialized_ = true;
        SPDLOG_INFO("Device {} initialized successfully", deviceId_);
        return true;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to initialize device {}: {}", deviceId_, e.what());
        return false;
    }
}

bool DeviceManager::connect(const std::string& host, uint16_t port) {
    if (!initialized_) {
        SPDLOG_ERROR("Device {} not initialized, cannot connect", deviceId_);
        return false;
    }

    bool success = commManager_->connect(host, port);
    if (success) {
        // 启用自动重连
        commManager_->setAutoReconnect(true, 5, 0);
        
        // 启动消息循环
        commManager_->startMessageLoop();
        
        SPDLOG_INFO("Device {} connected to {}:{}", deviceId_, host, port);
    }
    
    return success;
}

void DeviceManager::disconnect() {
    stopStatusUpdateThread();
    
    if (commManager_) {
        commManager_->disconnect();
    }
    
    SPDLOG_INFO("Device {} disconnected", deviceId_);
}

bool DeviceManager::start() {
    if (!initialized_) {
        SPDLOG_ERROR("Device {} not initialized, cannot start", deviceId_);
        return false;
    }

    if (running_) {
        SPDLOG_WARN("Device {} already running", deviceId_);
        return true;
    }

    try {
        running_ = true;
        
        // 更新设备状态
        stateManager_->setProperty("connected", isConnected());
        stateManager_->setProperty("running", true);
        
        // 启动状态更新线程
        startStatusUpdateThread();
        
        SPDLOG_INFO("Device {} started", deviceId_);
        return true;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to start device {}: {}", deviceId_, e.what());
        running_ = false;
        return false;
    }
}

void DeviceManager::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    
    // 停止状态更新线程
    stopStatusUpdateThread();
    
    // 更新设备状态
    if (stateManager_) {
        stateManager_->setProperty("running", false);
    }
    
    SPDLOG_INFO("Device {} stopped", deviceId_);
}

bool DeviceManager::registerDevice() {
    if (!isConnected()) {
        SPDLOG_ERROR("Device {} not connected, cannot register", deviceId_);
        return false;
    }

    try {
        json registerMsg;
        registerMsg["messageType"] = "REGISTER";
        registerMsg["deviceId"] = deviceId_;
        registerMsg["deviceType"] = deviceType_;
        registerMsg["deviceInfo"] = getDeviceInfo();
        
        bool success = sendMessage(registerMsg);
        if (success) {
            SPDLOG_INFO("Device {} registration message sent", deviceId_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to register device {}: {}", deviceId_, e.what());
        return false;
    }
}

json DeviceManager::getDeviceInfo() const {
    json info;
    info["deviceId"] = deviceId_;
    info["deviceType"] = deviceType_;
    info["manufacturer"] = manufacturer_;
    info["model"] = model_;
    info["firmwareVersion"] = firmwareVersion_;
    info["connected"] = isConnected();
    info["running"] = running_.load();
    
    // 添加能力列表
    if (stateManager_) {
        info["capabilities"] = stateManager_->getCapabilities();
    }
    
    // 添加当前属性
    if (stateManager_) {
        info["properties"] = stateManager_->getAllProperties();
    }
    
    return info;
}

bool DeviceManager::setProperty(const std::string& property, const json& value) {
    if (!stateManager_) {
        return false;
    }
    
    return stateManager_->setProperty(property, value);
}

json DeviceManager::getProperty(const std::string& property) const {
    if (!stateManager_) {
        return json();
    }
    
    return stateManager_->getProperty(property);
}

bool DeviceManager::setConfig(const std::string& name, const json& value) {
    if (!configManager_) {
        return false;
    }
    
    return configManager_->setConfig(name, value);
}

json DeviceManager::getConfig(const std::string& name) const {
    if (!configManager_) {
        return json();
    }
    
    return configManager_->getConfig(name);
}

bool DeviceManager::sendMessage(const std::string& message) {
    if (!commManager_) {
        return false;
    }
    
    return commManager_->sendMessage(message);
}

bool DeviceManager::sendMessage(const json& jsonMessage) {
    if (!commManager_) {
        return false;
    }
    
    return commManager_->sendMessage(jsonMessage);
}

bool DeviceManager::isConnected() const {
    if (!commManager_) {
        return false;
    }
    
    return commManager_->isConnected();
}

void DeviceManager::setMessageHandler(MessageHandler handler) {
    if (commManager_) {
        commManager_->setMessageHandler(handler);
    }
}

void DeviceManager::setConnectionStateHandler(ConnectionStateHandler handler) {
    if (commManager_) {
        commManager_->setConnectionStateHandler(handler);
    }
}

void DeviceManager::initializeBaseProperties() {
    if (!stateManager_) {
        return;
    }

    // 设置基础属性
    stateManager_->setProperty("deviceId", deviceId_);
    stateManager_->setProperty("deviceType", deviceType_);
    stateManager_->setProperty("manufacturer", manufacturer_);
    stateManager_->setProperty("model", model_);
    stateManager_->setProperty("firmwareVersion", firmwareVersion_);
    stateManager_->setProperty("connected", false);
    stateManager_->setProperty("running", false);
    
    // 设置基础能力
    stateManager_->addCapability("BASIC_CONTROL");
    stateManager_->addCapability("STATUS_REPORTING");
    stateManager_->addCapability("CONFIGURATION");
}

void DeviceManager::initializeBaseConfigs() {
    if (!configManager_) {
        return;
    }

    // 定义基础配置项
    std::vector<ConfigDefinition> baseConfigs = {
        {
            .name = "statusUpdateInterval",
            .type = ConfigType::INTEGER,
            .defaultValue = 5,
            .minValue = 1,
            .maxValue = 60,
            .description = "Status update interval in seconds",
            .required = false,
            .readOnly = false
        },
        {
            .name = "autoReconnect",
            .type = ConfigType::BOOLEAN,
            .defaultValue = true,
            .description = "Enable automatic reconnection",
            .required = false,
            .readOnly = false
        },
        {
            .name = "logLevel",
            .type = ConfigType::STRING,
            .defaultValue = "INFO",
            .description = "Logging level (DEBUG, INFO, WARN, ERROR)",
            .required = false,
            .readOnly = false
        }
    };
    
    configManager_->defineConfigs(baseConfigs);
}

void DeviceManager::handleMessage(const std::string& message) {
    SPDLOG_DEBUG("Device {} received message: {}", deviceId_, message);
    
    try {
        json msgJson = json::parse(message);
        
        // 处理基础消息类型
        if (msgJson.contains("messageType")) {
            std::string msgType = msgJson["messageType"];
            
            if (msgType == "PING") {
                // 响应心跳
                json pongMsg;
                pongMsg["messageType"] = "PONG";
                pongMsg["deviceId"] = deviceId_;
                pongMsg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                sendMessage(pongMsg);
            }
            else if (msgType == "GET_STATUS") {
                // 发送状态更新
                sendStatusUpdate();
            }
            else if (msgType == "GET_CONFIG") {
                // 发送配置信息
                json configMsg;
                configMsg["messageType"] = "CONFIG_RESPONSE";
                configMsg["deviceId"] = deviceId_;
                configMsg["configs"] = configManager_->getAllConfigs();
                sendMessage(configMsg);
            }
        }
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error handling message for device {}: {}", deviceId_, e.what());
    }
}

void DeviceManager::handleConnectionStateChange(ConnectionState state, const std::string& error) {
    SPDLOG_INFO("Device {} connection state changed to {}", deviceId_, static_cast<int>(state));
    
    // 更新连接状态属性
    stateManager_->setProperty("connected", state == ConnectionState::CONNECTED);
    
    if (state == ConnectionState::CONNECTED) {
        // 连接成功后注册设备
        registerDevice();
    }
    else if (state == ConnectionState::ERROR && !error.empty()) {
        SPDLOG_ERROR("Connection error for device {}: {}", deviceId_, error);
        stateManager_->setProperty("lastError", error);
    }
}

void DeviceManager::sendStatusUpdate() {
    if (!isConnected()) {
        return;
    }

    try {
        json statusMsg;
        statusMsg["messageType"] = "STATUS_UPDATE";
        statusMsg["deviceId"] = deviceId_;
        statusMsg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        statusMsg["deviceInfo"] = getDeviceInfo();
        
        sendMessage(statusMsg);
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error sending status update for device {}: {}", deviceId_, e.what());
    }
}

void DeviceManager::statusUpdateLoop() {
    while (statusUpdateRunning_) {
        try {
            sendStatusUpdate();
            
            int interval = statusUpdateInterval_.load();
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in status update loop for device {}: {}", deviceId_, e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

void DeviceManager::startStatusUpdateThread() {
    if (statusUpdateRunning_) {
        return;
    }
    
    statusUpdateRunning_ = true;
    statusUpdateThread_ = std::thread(&DeviceManager::statusUpdateLoop, this);
    
    SPDLOG_DEBUG("Status update thread started for device {}", deviceId_);
}

void DeviceManager::stopStatusUpdateThread() {
    if (!statusUpdateRunning_) {
        return;
    }
    
    statusUpdateRunning_ = false;
    
    if (statusUpdateThread_.joinable()) {
        statusUpdateThread_.join();
    }
    
    SPDLOG_DEBUG("Status update thread stopped for device {}", deviceId_);
}

} // namespace core
} // namespace device
} // namespace astrocomm

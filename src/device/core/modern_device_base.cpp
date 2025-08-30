#include "modern_device_base.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <algorithm>

namespace astrocomm {
namespace device {
namespace core {

ModernDeviceBase::ModernDeviceBase(const std::string& deviceId,
                                 const std::string& deviceType,
                                 const std::string& manufacturer,
                                 const std::string& model)
    : deviceId_(deviceId)
    , deviceType_(deviceType)
    , manufacturer_(manufacturer)
    , model_(model)
    , updateRunning_(false)
    , updateInterval_(1000) {
    
    // 创建设备管理器
    deviceManager_ = std::make_shared<DeviceManager>(deviceId, deviceType, manufacturer, model);
    
    SPDLOG_INFO("ModernDeviceBase created: {} ({})", deviceId_, deviceType_);
}

ModernDeviceBase::~ModernDeviceBase() {
    stop();
    SPDLOG_INFO("ModernDeviceBase destroyed: {}", deviceId_);
}

std::string ModernDeviceBase::getDeviceId() const {
    return deviceId_;
}

std::string ModernDeviceBase::getDeviceType() const {
    return deviceType_;
}

json ModernDeviceBase::getDeviceInfo() const {
    json info = deviceManager_->getDeviceInfo();
    
    // 添加行为信息
    std::vector<std::string> behaviorNames = getBehaviorNames();
    if (!behaviorNames.empty()) {
        info["behaviors"] = behaviorNames;
    }
    
    return info;
}

bool ModernDeviceBase::initialize() {
    if (!deviceManager_->initialize()) {
        SPDLOG_ERROR("Failed to initialize device manager for {}", deviceId_);
        return false;
    }
    
    // 初始化行为组件
    if (!initializeBehaviors()) {
        SPDLOG_ERROR("Failed to initialize behaviors for {}", deviceId_);
        return false;
    }
    
    // 初始化设备特定功能
    if (!initializeDevice()) {
        SPDLOG_ERROR("Failed to initialize device-specific functionality for {}", deviceId_);
        return false;
    }
    
    SPDLOG_INFO("Device {} initialized successfully", deviceId_);
    return true;
}

bool ModernDeviceBase::connect(const std::string& host, uint16_t port) {
    return deviceManager_->connect(host, port);
}

void ModernDeviceBase::disconnect() {
    stopUpdateThread();
    stopBehaviors();
    deviceManager_->disconnect();
}

bool ModernDeviceBase::start() {
    if (!deviceManager_->start()) {
        SPDLOG_ERROR("Failed to start device manager for {}", deviceId_);
        return false;
    }
    
    // 启动行为组件
    if (!startBehaviors()) {
        SPDLOG_ERROR("Failed to start behaviors for {}", deviceId_);
        return false;
    }
    
    // 启动设备特定功能
    if (!startDevice()) {
        SPDLOG_ERROR("Failed to start device-specific functionality for {}", deviceId_);
        return false;
    }
    
    // 启动更新线程
    startUpdateThread();
    
    SPDLOG_INFO("Device {} started successfully", deviceId_);
    return true;
}

void ModernDeviceBase::stop() {
    stopUpdateThread();
    stopDevice();
    stopBehaviors();
    deviceManager_->stop();
    
    SPDLOG_INFO("Device {} stopped", deviceId_);
}

bool ModernDeviceBase::isConnected() const {
    return deviceManager_->isConnected();
}

bool ModernDeviceBase::isRunning() const {
    return deviceManager_->isRunning();
}

bool ModernDeviceBase::setConfig(const std::string& name, const json& value) {
    return deviceManager_->setConfig(name, value);
}

json ModernDeviceBase::getConfig(const std::string& name) const {
    return deviceManager_->getConfig(name);
}

json ModernDeviceBase::getAllConfigs() const {
    return deviceManager_->getConfigManager()->getAllConfigs();
}

bool ModernDeviceBase::saveConfig() {
    return deviceManager_->getConfigManager()->saveToFile();
}

bool ModernDeviceBase::loadConfig() {
    return deviceManager_->getConfigManager()->loadFromFile();
}

bool ModernDeviceBase::setProperty(const std::string& property, const json& value) {
    return deviceManager_->setProperty(property, value);
}

json ModernDeviceBase::getProperty(const std::string& property) const {
    return deviceManager_->getProperty(property);
}

json ModernDeviceBase::getAllProperties() const {
    return deviceManager_->getStateManager()->getAllProperties();
}

std::vector<std::string> ModernDeviceBase::getCapabilities() const {
    auto capabilities = deviceManager_->getStateManager()->getCapabilities();
    
    // 添加行为组件的能力
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    for (const auto& [name, behavior] : behaviors_) {
        auto behaviorCaps = behavior->getCapabilities();
        capabilities.insert(capabilities.end(), behaviorCaps.begin(), behaviorCaps.end());
    }
    
    // 去重
    std::sort(capabilities.begin(), capabilities.end());
    capabilities.erase(std::unique(capabilities.begin(), capabilities.end()), capabilities.end());
    
    return capabilities;
}

bool ModernDeviceBase::addBehavior(std::unique_ptr<behaviors::DeviceBehavior> behavior) {
    if (!behavior) {
        SPDLOG_WARN("Cannot add null behavior to device {}", deviceId_);
        return false;
    }
    
    std::string behaviorName = behavior->getBehaviorName();
    
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    
    if (behaviors_.find(behaviorName) != behaviors_.end()) {
        SPDLOG_WARN("Behavior '{}' already exists in device {}", behaviorName, deviceId_);
        return false;
    }
    
    // 如果设备管理器已初始化，立即初始化行为
    if (deviceManager_ && deviceManager_->getStateManager() && deviceManager_->getConfigManager()) {
        if (!behavior->initialize(deviceManager_->getStateManager(), deviceManager_->getConfigManager())) {
            SPDLOG_ERROR("Failed to initialize behavior '{}' for device {}", behaviorName, deviceId_);
            return false;
        }
        
        // 如果设备正在运行，启动行为
        if (isRunning()) {
            if (!behavior->start()) {
                SPDLOG_ERROR("Failed to start behavior '{}' for device {}", behaviorName, deviceId_);
                return false;
            }
        }
    }
    
    behaviors_[behaviorName] = std::move(behavior);
    
    SPDLOG_DEBUG("Behavior '{}' added to device {}", behaviorName, deviceId_);
    return true;
}

bool ModernDeviceBase::removeBehavior(const std::string& behaviorName) {
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    
    auto it = behaviors_.find(behaviorName);
    if (it == behaviors_.end()) {
        SPDLOG_WARN("Behavior '{}' not found in device {}", behaviorName, deviceId_);
        return false;
    }
    
    // 停止行为
    it->second->stop();
    
    // 移除行为
    behaviors_.erase(it);
    
    SPDLOG_DEBUG("Behavior '{}' removed from device {}", behaviorName, deviceId_);
    return true;
}

behaviors::DeviceBehavior* ModernDeviceBase::getBehavior(const std::string& behaviorName) {
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    
    auto it = behaviors_.find(behaviorName);
    if (it != behaviors_.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

bool ModernDeviceBase::hasBehavior(const std::string& behaviorName) const {
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    return behaviors_.find(behaviorName) != behaviors_.end();
}

std::vector<std::string> ModernDeviceBase::getBehaviorNames() const {
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    
    std::vector<std::string> names;
    names.reserve(behaviors_.size());
    
    for (const auto& [name, behavior] : behaviors_) {
        names.push_back(name);
    }
    
    return names;
}

bool ModernDeviceBase::handleCommand(const std::string& command, const json& parameters, json& result) {
    // 首先尝试设备特定命令处理
    if (handleDeviceCommand(command, parameters, result)) {
        return true;
    }
    
    // 然后尝试行为组件命令处理
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    for (const auto& [name, behavior] : behaviors_) {
        if (behavior->handleCommand(command, parameters, result)) {
            return true;
        }
    }
    
    // 处理通用设备命令
    if (command == "GET_DEVICE_INFO") {
        result = getDeviceInfo();
        return true;
    }
    else if (command == "GET_CAPABILITIES") {
        result["capabilities"] = getCapabilities();
        return true;
    }
    else if (command == "GET_BEHAVIORS") {
        result["behaviors"] = getBehaviorNames();
        return true;
    }
    else if (command == "GET_ALL_PROPERTIES") {
        result["properties"] = getAllProperties();
        return true;
    }
    else if (command == "GET_ALL_CONFIGS") {
        result["configs"] = getAllConfigs();
        return true;
    }
    else if (command == "SAVE_CONFIG") {
        bool success = saveConfig();
        result["success"] = success;
        return true;
    }
    else if (command == "LOAD_CONFIG") {
        bool success = loadConfig();
        result["success"] = success;
        return true;
    }
    
    return false;
}

bool ModernDeviceBase::sendMessage(const std::string& message) {
    return deviceManager_->sendMessage(message);
}

bool ModernDeviceBase::sendMessage(const json& jsonMessage) {
    return deviceManager_->sendMessage(jsonMessage);
}

bool ModernDeviceBase::registerDevice() {
    return deviceManager_->registerDevice();
}

bool ModernDeviceBase::initializeBehaviors() {
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    
    for (const auto& [name, behavior] : behaviors_) {
        if (!behavior->initialize(deviceManager_->getStateManager(), deviceManager_->getConfigManager())) {
            SPDLOG_ERROR("Failed to initialize behavior '{}' for device {}", name, deviceId_);
            return false;
        }
    }
    
    SPDLOG_DEBUG("All behaviors initialized for device {}", deviceId_);
    return true;
}

bool ModernDeviceBase::startBehaviors() {
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    
    for (const auto& [name, behavior] : behaviors_) {
        if (!behavior->start()) {
            SPDLOG_ERROR("Failed to start behavior '{}' for device {}", name, deviceId_);
            return false;
        }
    }
    
    SPDLOG_DEBUG("All behaviors started for device {}", deviceId_);
    return true;
}

void ModernDeviceBase::stopBehaviors() {
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    
    for (const auto& [name, behavior] : behaviors_) {
        behavior->stop();
    }
    
    SPDLOG_DEBUG("All behaviors stopped for device {}", deviceId_);
}

void ModernDeviceBase::updateBehaviors() {
    std::lock_guard<std::mutex> lock(behaviorsMutex_);
    
    for (const auto& [name, behavior] : behaviors_) {
        try {
            behavior->update();
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error updating behavior '{}' for device {}: {}", name, deviceId_, e.what());
        }
    }
}

void ModernDeviceBase::startUpdateThread() {
    if (updateRunning_) {
        return;
    }
    
    updateRunning_ = true;
    updateThread_ = std::thread(&ModernDeviceBase::updateLoop, this);
    
    SPDLOG_DEBUG("Update thread started for device {}", deviceId_);
}

void ModernDeviceBase::stopUpdateThread() {
    if (!updateRunning_) {
        return;
    }
    
    updateRunning_ = false;
    
    if (updateThread_.joinable()) {
        updateThread_.join();
    }
    
    SPDLOG_DEBUG("Update thread stopped for device {}", deviceId_);
}

void ModernDeviceBase::updateLoop() {
    while (updateRunning_) {
        try {
            // 更新行为组件
            updateBehaviors();
            
            // 更新设备特定功能
            updateDevice();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(updateInterval_.load()));
            
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in update loop for device {}: {}", deviceId_, e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

} // namespace core
} // namespace device
} // namespace astrocomm

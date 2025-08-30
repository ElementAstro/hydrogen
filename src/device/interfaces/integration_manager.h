#pragma once

#include "protocol_bridge.h"
#include "../core/modern_device_base.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <type_traits>

namespace astrocomm {
namespace device {
namespace interfaces {
namespace integration {

/**
 * @brief Device discovery callback function type
 */
using DeviceDiscoveryCallback = std::function<void(const std::string& deviceId, std::shared_ptr<core::IDevice> device)>;

/**
 * @brief Device removal callback function type
 */
using DeviceRemovalCallback = std::function<void(const std::string& deviceId)>;

/**
 * @brief Integration configuration for automatic device handling
 */
struct IntegrationConfiguration {
    bool autoDiscovery = true;
    bool autoRegistration = true;
    bool enableASCOM = true;
    bool enableINDI = true;
    int discoveryInterval = 5000; // milliseconds
    int indiBasePort = 7624;
    std::string deviceNamePrefix = "Hydrogen_";
    std::unordered_map<std::string, bridge::BridgeConfiguration> deviceConfigs;
    
    IntegrationConfiguration() = default;
};

/**
 * @brief Automatic integration manager for seamless ASCOM/INDI device handling
 * 
 * This class automatically discovers internal devices and creates transparent
 * protocol bridges without requiring any changes to existing device code.
 */
class AutomaticIntegrationManager {
public:
    static AutomaticIntegrationManager& getInstance() {
        static AutomaticIntegrationManager instance;
        return instance;
    }
    
    // Manager lifecycle
    void initialize(const IntegrationConfiguration& config = IntegrationConfiguration()) {
        if (initialized_.load()) return;
        
        config_ = config;
        initialized_ = true;
        
        SPDLOG_INFO("Automatic integration manager initialized");
    }
    
    void start() {
        if (running_.load()) return;
        
        running_ = true;
        
        if (config_.autoDiscovery) {
            discoveryThread_ = std::thread(&AutomaticIntegrationManager::discoveryLoop, this);
        }
        
        SPDLOG_INFO("Automatic integration manager started");
    }
    
    void stop() {
        if (!running_.load()) return;
        
        running_ = false;
        
        // Stop discovery thread
        if (discoveryThread_.joinable()) {
            discoveryThread_.join();
        }
        
        // Stop all bridges
        stopAllBridges();
        
        SPDLOG_INFO("Automatic integration manager stopped");
    }
    
    // Device registration
    template<typename DeviceType>
    void registerDevice(const std::string& deviceId, std::shared_ptr<DeviceType> device) {
        if (!initialized_.load()) {
            SPDLOG_WARN("Integration manager not initialized, initializing with defaults");
            initialize();
        }
        
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        // Check if device is already registered
        if (registeredDevices_.find(deviceId) != registeredDevices_.end()) {
            SPDLOG_WARN("Device {} already registered", deviceId);
            return;
        }
        
        // Create bridge configuration
        bridge::BridgeConfiguration bridgeConfig;
        auto configIt = config_.deviceConfigs.find(deviceId);
        if (configIt != config_.deviceConfigs.end()) {
            bridgeConfig = configIt->second;
        } else {
            // Use default configuration
            bridgeConfig.deviceName = config_.deviceNamePrefix + deviceId;
            bridgeConfig.deviceDescription = getDeviceDescription<DeviceType>();
            bridgeConfig.enableASCOM = config_.enableASCOM;
            bridgeConfig.enableINDI = config_.enableINDI;
        }
        
        // Create and start bridge
        auto bridge = bridge::ProtocolBridgeFactory::createAndStartBridge(device, bridgeConfig);
        
        // Register protocols
        if (config_.autoRegistration) {
            bridge->registerWithProtocols();
        }
        
        // Store device info
        DeviceInfo info;
        info.device = std::static_pointer_cast<core::IDevice>(device);
        info.bridge = std::static_pointer_cast<void>(bridge);
        info.deviceType = getDeviceTypeName<DeviceType>();
        info.registrationTime = std::chrono::system_clock::now();
        
        registeredDevices_[deviceId] = info;
        
        // Notify callbacks
        notifyDeviceDiscovered(deviceId, info.device);
        
        SPDLOG_INFO("Automatically registered device: {} (type: {})", deviceId, info.deviceType);
    }
    
    void unregisterDevice(const std::string& deviceId) {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = registeredDevices_.find(deviceId);
        if (it != registeredDevices_.end()) {
            // Stop bridge
            stopBridge(deviceId);
            
            // Remove from registry
            registeredDevices_.erase(it);
            
            // Notify callbacks
            notifyDeviceRemoved(deviceId);
            
            SPDLOG_INFO("Unregistered device: {}", deviceId);
        }
    }
    
    // Device discovery
    void addDeviceDiscoveryCallback(DeviceDiscoveryCallback callback) {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        discoveryCallbacks_.push_back(callback);
    }
    
    void addDeviceRemovalCallback(DeviceRemovalCallback callback) {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        removalCallbacks_.push_back(callback);
    }
    
    // Device access
    std::vector<std::string> getRegisteredDeviceIds() const {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        std::vector<std::string> deviceIds;
        for (const auto& [deviceId, info] : registeredDevices_) {
            deviceIds.push_back(deviceId);
        }
        return deviceIds;
    }
    
    std::shared_ptr<core::IDevice> getDevice(const std::string& deviceId) const {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        auto it = registeredDevices_.find(deviceId);
        return (it != registeredDevices_.end()) ? it->second.device : nullptr;
    }
    
    template<typename DeviceType>
    std::shared_ptr<DeviceType> getTypedDevice(const std::string& deviceId) const {
        auto device = getDevice(deviceId);
        return std::dynamic_pointer_cast<DeviceType>(device);
    }
    
    // Protocol access
    template<typename DeviceType, typename T>
    T getDeviceProperty(const std::string& deviceId, const std::string& propertyName, 
                       bridge::ProtocolType protocol = bridge::ProtocolType::INTERNAL) {
        auto bridge = getBridge<DeviceType>(deviceId);
        if (bridge) {
            return bridge->template getProperty<T>(propertyName, protocol);
        }
        throw std::runtime_error("Device not found or bridge not available: " + deviceId);
    }
    
    template<typename DeviceType, typename T>
    void setDeviceProperty(const std::string& deviceId, const std::string& propertyName, const T& value,
                          bridge::ProtocolType protocol = bridge::ProtocolType::INTERNAL) {
        auto bridge = getBridge<DeviceType>(deviceId);
        if (bridge) {
            bridge->template setProperty<T>(propertyName, value, protocol);
        } else {
            throw std::runtime_error("Device not found or bridge not available: " + deviceId);
        }
    }
    
    template<typename DeviceType, typename ReturnType, typename... Args>
    ReturnType invokeDeviceMethod(const std::string& deviceId, const std::string& methodName,
                                 bridge::ProtocolType protocol, Args... args) {
        auto bridge = getBridge<DeviceType>(deviceId);
        if (bridge) {
            return bridge->template invokeMethod<ReturnType>(methodName, protocol, args...);
        }
        throw std::runtime_error("Device not found or bridge not available: " + deviceId);
    }
    
    // Statistics and information
    struct IntegrationStatistics {
        size_t totalDevices = 0;
        size_t ascomDevices = 0;
        size_t indiDevices = 0;
        std::chrono::system_clock::time_point startTime;
        std::chrono::milliseconds uptime;
        std::unordered_map<std::string, size_t> deviceTypeCount;
    };
    
    IntegrationStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        IntegrationStatistics stats;
        stats.totalDevices = registeredDevices_.size();
        stats.startTime = startTime_;
        stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - startTime_);
        
        for (const auto& [deviceId, info] : registeredDevices_) {
            stats.deviceTypeCount[info.deviceType]++;
            
            // Count protocol-enabled devices
            // Note: In real implementation, would check bridge statistics
            stats.ascomDevices++;
            stats.indiDevices++;
        }
        
        return stats;
    }
    
    IntegrationConfiguration getConfiguration() const { return config_; }

private:
    struct DeviceInfo {
        std::shared_ptr<core::IDevice> device;
        std::shared_ptr<void> bridge; // Type-erased bridge pointer
        std::string deviceType;
        std::chrono::system_clock::time_point registrationTime;
    };
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    IntegrationConfiguration config_;
    std::chrono::system_clock::time_point startTime_;
    
    // Device management
    mutable std::mutex devicesMutex_;
    std::unordered_map<std::string, DeviceInfo> registeredDevices_;
    
    // Discovery
    std::thread discoveryThread_;
    
    // Callbacks
    mutable std::mutex callbacksMutex_;
    std::vector<DeviceDiscoveryCallback> discoveryCallbacks_;
    std::vector<DeviceRemovalCallback> removalCallbacks_;
    
    AutomaticIntegrationManager() {
        startTime_ = std::chrono::system_clock::now();
    }
    
    ~AutomaticIntegrationManager() {
        stop();
    }
    
    // Prevent copying
    AutomaticIntegrationManager(const AutomaticIntegrationManager&) = delete;
    AutomaticIntegrationManager& operator=(const AutomaticIntegrationManager&) = delete;
    
    void discoveryLoop() {
        SPDLOG_DEBUG("Device discovery loop started");
        
        while (running_.load()) {
            try {
                discoverDevices();
                std::this_thread::sleep_for(std::chrono::milliseconds(config_.discoveryInterval));
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Error in discovery loop: {}", e.what());
            }
        }
        
        SPDLOG_DEBUG("Device discovery loop stopped");
    }
    
    void discoverDevices() {
        // In real implementation, would scan for new devices
        // This could involve:
        // - Checking device manager for new devices
        // - Scanning USB/serial ports
        // - Network discovery
        // - Registry monitoring
        
        // For now, this is a placeholder that would be implemented
        // based on the specific device discovery mechanism
    }
    
    template<typename DeviceType>
    std::shared_ptr<bridge::TransparentProtocolBridge<DeviceType>> getBridge(const std::string& deviceId) const {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        auto it = registeredDevices_.find(deviceId);
        if (it != registeredDevices_.end()) {
            return std::static_pointer_cast<bridge::TransparentProtocolBridge<DeviceType>>(it->second.bridge);
        }
        return nullptr;
    }
    
    void stopBridge(const std::string& deviceId) {
        auto it = registeredDevices_.find(deviceId);
        if (it != registeredDevices_.end()) {
            // Type-erased bridge stop - would need proper implementation
            // based on bridge type information stored in DeviceInfo
        }
    }
    
    void stopAllBridges() {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        for (const auto& [deviceId, info] : registeredDevices_) {
            stopBridge(deviceId);
        }
    }
    
    void notifyDeviceDiscovered(const std::string& deviceId, std::shared_ptr<core::IDevice> device) {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        for (const auto& callback : discoveryCallbacks_) {
            try {
                callback(deviceId, device);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Error in device discovery callback: {}", e.what());
            }
        }
    }
    
    void notifyDeviceRemoved(const std::string& deviceId) {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        for (const auto& callback : removalCallbacks_) {
            try {
                callback(deviceId);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Error in device removal callback: {}", e.what());
            }
        }
    }
    
    template<typename DeviceType>
    std::string getDeviceTypeName() const {
        if constexpr (std::is_same_v<DeviceType, ICamera>) {
            return "Camera";
        } else if constexpr (std::is_same_v<DeviceType, ITelescope>) {
            return "Telescope";
        } else if constexpr (std::is_same_v<DeviceType, IFocuser>) {
            return "Focuser";
        } else if constexpr (std::is_same_v<DeviceType, IRotator>) {
            return "Rotator";
        } else if constexpr (std::is_same_v<DeviceType, IFilterWheel>) {
            return "FilterWheel";
        } else if constexpr (std::is_same_v<DeviceType, IDome>) {
            return "Dome";
        } else if constexpr (std::is_same_v<DeviceType, ISwitch>) {
            return "Switch";
        } else if constexpr (std::is_same_v<DeviceType, ISafetyMonitor>) {
            return "SafetyMonitor";
        } else if constexpr (std::is_same_v<DeviceType, ICoverCalibrator>) {
            return "CoverCalibrator";
        } else if constexpr (std::is_same_v<DeviceType, IObservingConditions>) {
            return "ObservingConditions";
        } else {
            return "Unknown";
        }
    }
    
    template<typename DeviceType>
    std::string getDeviceDescription() const {
        return "Hydrogen " + getDeviceTypeName<DeviceType>() + " Device";
    }
};

/**
 * @brief Convenience macros for automatic device registration
 */
#define REGISTER_DEVICE_AUTO(deviceId, device) \
    astrocomm::device::interfaces::integration::AutomaticIntegrationManager::getInstance().registerDevice(deviceId, device)

#define UNREGISTER_DEVICE_AUTO(deviceId) \
    astrocomm::device::interfaces::integration::AutomaticIntegrationManager::getInstance().unregisterDevice(deviceId)

#define GET_DEVICE_AUTO(DeviceType, deviceId) \
    astrocomm::device::interfaces::integration::AutomaticIntegrationManager::getInstance().getTypedDevice<DeviceType>(deviceId)

#define GET_DEVICE_PROPERTY_AUTO(DeviceType, deviceId, propertyName, PropertyType, protocol) \
    astrocomm::device::interfaces::integration::AutomaticIntegrationManager::getInstance().getDeviceProperty<DeviceType, PropertyType>(deviceId, propertyName, protocol)

#define SET_DEVICE_PROPERTY_AUTO(DeviceType, deviceId, propertyName, value, protocol) \
    astrocomm::device::interfaces::integration::AutomaticIntegrationManager::getInstance().setDeviceProperty<DeviceType>(deviceId, propertyName, value, protocol)

#define INVOKE_DEVICE_METHOD_AUTO(DeviceType, ReturnType, deviceId, methodName, protocol, ...) \
    astrocomm::device::interfaces::integration::AutomaticIntegrationManager::getInstance().invokeDeviceMethod<DeviceType, ReturnType>(deviceId, methodName, protocol, __VA_ARGS__)

} // namespace integration
} // namespace interfaces
} // namespace device
} // namespace astrocomm

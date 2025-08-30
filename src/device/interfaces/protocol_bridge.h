#pragma once

#include "automatic_adapter.h"
#include "ascom_bridge.h"
#include "indi_bridge.h"
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
namespace bridge {

/**
 * @brief Protocol types supported by the bridge
 */
enum class ProtocolType {
    ASCOM,
    INDI,
    INTERNAL
};

/**
 * @brief Bridge configuration for automatic protocol handling
 */
struct BridgeConfiguration {
    bool enableASCOM = true;
    bool enableINDI = true;
    int indiPort = 7624;
    bool autoRegisterCOM = true;
    bool autoStartServers = true;
    std::string deviceName;
    std::string deviceDescription;
    std::unordered_map<std::string, std::string> customProperties;
    
    BridgeConfiguration() = default;
    BridgeConfiguration(const std::string& name, const std::string& desc)
        : deviceName(name), deviceDescription(desc) {}
};

/**
 * @brief Transparent protocol bridge for seamless ASCOM/INDI integration
 * 
 * This class provides automatic, transparent integration between internal devices
 * and ASCOM/INDI protocols without requiring changes to existing code.
 */
template<typename DeviceType>
class TransparentProtocolBridge {
public:
    explicit TransparentProtocolBridge(std::shared_ptr<DeviceType> device, 
                                      const BridgeConfiguration& config = BridgeConfiguration())
        : device_(device), config_(config), running_(false) {
        
        // Create adapters
        ascomAdapter_ = std::make_shared<automatic::ASCOMAutomaticAdapter<DeviceType>>(device);
        indiAdapter_ = std::make_shared<automatic::INDIAutomaticAdapter<DeviceType>>(device);
        
        // Create bridges
        if (config_.enableASCOM) {
            ascomBridge_ = std::make_shared<ascom::ASCOMCOMBridge<DeviceType>>(ascomAdapter_);
        }
        
        if (config_.enableINDI) {
            indiBridge_ = std::make_shared<indi::INDIProtocolBridge<DeviceType>>(indiAdapter_);
        }
        
        initializeBridge();
    }
    
    virtual ~TransparentProtocolBridge() {
        stop();
    }
    
    // Bridge lifecycle
    void start() {
        if (running_.load()) return;
        
        running_ = true;
        
        // Start ASCOM bridge
        if (config_.enableASCOM && ascomBridge_) {
            startASCOMBridge();
        }
        
        // Start INDI bridge
        if (config_.enableINDI && indiBridge_) {
            startINDIBridge();
        }
        
        // Start property synchronization
        syncThread_ = std::thread(&TransparentProtocolBridge::synchronizationLoop, this);
        
        SPDLOG_INFO("Transparent protocol bridge started for device: {}", config_.deviceName);
    }
    
    void stop() {
        if (!running_.load()) return;
        
        running_ = false;
        
        // Stop bridges
        if (indiBridge_) {
            indiBridge_->stop();
        }
        
        // Stop synchronization thread
        if (syncThread_.joinable()) {
            syncThread_.join();
        }
        
        // Unregister from protocols
        unregisterFromProtocols();
        
        SPDLOG_INFO("Transparent protocol bridge stopped for device: {}", config_.deviceName);
    }
    
    // Protocol registration
    void registerWithProtocols() {
        if (config_.enableASCOM) {
            registerWithASCOM();
        }
        
        if (config_.enableINDI) {
            registerWithINDI();
        }
    }
    
    void unregisterFromProtocols() {
        if (config_.enableASCOM) {
            unregisterFromASCOM();
        }
        
        if (config_.enableINDI) {
            unregisterFromINDI();
        }
    }
    
    // Property access (transparent to protocol)
    template<typename T>
    T getProperty(const std::string& propertyName, ProtocolType protocol = ProtocolType::INTERNAL) {
        switch (protocol) {
            case ProtocolType::ASCOM:
                if (ascomAdapter_) {
                    return ascomAdapter_->getASCOMProperty<T>(propertyName);
                }
                break;
            case ProtocolType::INDI:
                if (indiAdapter_) {
                    auto property = indiAdapter_->getINDIProperty(propertyName);
                    return automatic::TypeConverter::fromJson<T>(property.toXML());
                }
                break;
            case ProtocolType::INTERNAL:
            default:
                if (device_) {
                    json value = device_->getProperty(propertyName);
                    return automatic::TypeConverter::fromJson<T>(value);
                }
                break;
        }
        
        throw std::runtime_error("Failed to get property: " + propertyName);
    }
    
    template<typename T>
    void setProperty(const std::string& propertyName, const T& value, ProtocolType protocol = ProtocolType::INTERNAL) {
        switch (protocol) {
            case ProtocolType::ASCOM:
                if (ascomAdapter_) {
                    ascomAdapter_->setASCOMProperty(propertyName, value);
                }
                break;
            case ProtocolType::INDI:
                if (indiAdapter_) {
                    // Convert value to INDI property and set
                    json jsonValue = automatic::TypeConverter::toJson(value);
                    indiAdapter_->setProperty(propertyName, jsonValue, "INDI");
                }
                break;
            case ProtocolType::INTERNAL:
            default:
                if (device_) {
                    json jsonValue = automatic::TypeConverter::toJson(value);
                    device_->setProperty(propertyName, jsonValue);
                }
                break;
        }
        
        // Synchronize across all protocols
        synchronizeProperty(propertyName);
    }
    
    // Method invocation (transparent to protocol)
    template<typename ReturnType, typename... Args>
    ReturnType invokeMethod(const std::string& methodName, ProtocolType protocol, Args... args) {
        switch (protocol) {
            case ProtocolType::ASCOM:
                if (ascomAdapter_) {
                    return ascomAdapter_->invokeASCOMMethod<ReturnType>(methodName, args...);
                }
                break;
            case ProtocolType::INDI:
                if (indiAdapter_) {
                    std::vector<json> parameters;
                    (parameters.push_back(automatic::TypeConverter::toJson(args)), ...);
                    json result = indiAdapter_->invokeMethod(methodName, parameters, "INDI");
                    
                    if constexpr (std::is_void_v<ReturnType>) {
                        return;
                    } else {
                        return automatic::TypeConverter::fromJson<ReturnType>(result);
                    }
                }
                break;
            case ProtocolType::INTERNAL:
            default:
                if (device_) {
                    json params = json::object();
                    size_t i = 0;
                    (params["param" + std::to_string(i++)] = automatic::TypeConverter::toJson(args), ...);
                    
                    json result;
                    device_->handleDeviceCommand(methodName, params, result);
                    
                    if constexpr (std::is_void_v<ReturnType>) {
                        return;
                    } else {
                        return automatic::TypeConverter::fromJson<ReturnType>(result);
                    }
                }
                break;
        }
        
        throw std::runtime_error("Failed to invoke method: " + methodName);
    }
    
    // Status and information
    bool isProtocolEnabled(ProtocolType protocol) const {
        switch (protocol) {
            case ProtocolType::ASCOM: return config_.enableASCOM && ascomBridge_ != nullptr;
            case ProtocolType::INDI: return config_.enableINDI && indiBridge_ != nullptr;
            case ProtocolType::INTERNAL: return device_ != nullptr;
            default: return false;
        }
    }
    
    std::vector<ProtocolType> getEnabledProtocols() const {
        std::vector<ProtocolType> protocols;
        if (isProtocolEnabled(ProtocolType::INTERNAL)) protocols.push_back(ProtocolType::INTERNAL);
        if (isProtocolEnabled(ProtocolType::ASCOM)) protocols.push_back(ProtocolType::ASCOM);
        if (isProtocolEnabled(ProtocolType::INDI)) protocols.push_back(ProtocolType::INDI);
        return protocols;
    }
    
    BridgeConfiguration getConfiguration() const { return config_; }
    
    // Statistics
    struct BridgeStatistics {
        size_t ascomConnections = 0;
        size_t indiConnections = 0;
        size_t propertiesSynchronized = 0;
        size_t methodsInvoked = 0;
        std::chrono::system_clock::time_point startTime;
        std::chrono::milliseconds uptime;
    };
    
    BridgeStatistics getStatistics() const {
        BridgeStatistics stats;
        stats.startTime = startTime_;
        stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - startTime_);
        stats.propertiesSynchronized = propertiesSynchronized_.load();
        stats.methodsInvoked = methodsInvoked_.load();
        return stats;
    }

private:
    std::shared_ptr<DeviceType> device_;
    BridgeConfiguration config_;
    std::atomic<bool> running_;
    std::chrono::system_clock::time_point startTime_;
    
    // Adapters and bridges
    std::shared_ptr<automatic::ASCOMAutomaticAdapter<DeviceType>> ascomAdapter_;
    std::shared_ptr<automatic::INDIAutomaticAdapter<DeviceType>> indiAdapter_;
    std::shared_ptr<ascom::ASCOMCOMBridge<DeviceType>> ascomBridge_;
    std::shared_ptr<indi::INDIProtocolBridge<DeviceType>> indiBridge_;
    
    // Threading
    std::thread syncThread_;
    
    // Statistics
    std::atomic<size_t> propertiesSynchronized_{0};
    std::atomic<size_t> methodsInvoked_{0};
    
    void initializeBridge() {
        startTime_ = std::chrono::system_clock::now();
        
        // Set device information
        if (device_) {
            if (!config_.deviceName.empty()) {
                device_->setProperty("name", config_.deviceName);
            }
            if (!config_.deviceDescription.empty()) {
                device_->setProperty("description", config_.deviceDescription);
            }
            
            // Set custom properties
            for (const auto& [key, value] : config_.customProperties) {
                device_->setProperty(key, value);
            }
        }
    }
    
    void startASCOMBridge() {
        if (config_.autoRegisterCOM) {
            registerWithASCOM();
        }
        SPDLOG_DEBUG("ASCOM bridge started for device: {}", config_.deviceName);
    }
    
    void startINDIBridge() {
        if (config_.autoStartServers && indiBridge_) {
            indiBridge_->start(config_.indiPort);
        }
        SPDLOG_DEBUG("INDI bridge started for device: {}", config_.deviceName);
    }
    
    void registerWithASCOM() {
        if (ascomAdapter_) {
            ascomAdapter_->registerWithProtocol("ASCOM");
            
            // Register with ASCOM device registry
            auto deviceType = getASCOMDeviceType();
            ascom::ASCOMDeviceRegistry::getInstance().registerDevice(
                config_.deviceName, device_, deviceType);
        }
    }
    
    void unregisterFromASCOM() {
        if (ascomAdapter_) {
            ascomAdapter_->unregisterFromProtocol("ASCOM");
            ascom::ASCOMDeviceRegistry::getInstance().unregisterDevice(config_.deviceName);
        }
    }
    
    void registerWithINDI() {
        if (indiAdapter_) {
            indiAdapter_->registerWithProtocol("INDI");
            
            // Register with INDI device registry
            indi::INDIDeviceRegistry::getInstance().registerDevice(config_.deviceName, device_);
        }
    }
    
    void unregisterFromINDI() {
        if (indiAdapter_) {
            indiAdapter_->unregisterFromProtocol("INDI");
            indi::INDIDeviceRegistry::getInstance().unregisterDevice(config_.deviceName);
        }
    }
    
    void synchronizationLoop() {
        SPDLOG_DEBUG("Property synchronization loop started");
        
        while (running_.load()) {
            try {
                synchronizeAllProperties();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Error in synchronization loop: {}", e.what());
            }
        }
        
        SPDLOG_DEBUG("Property synchronization loop stopped");
    }
    
    void synchronizeAllProperties() {
        if (!device_) return;
        
        // Get all properties from internal device
        json allProperties = device_->getAllProperties();
        
        for (auto& [propertyName, value] : allProperties.items()) {
            synchronizeProperty(propertyName);
        }
    }
    
    void synchronizeProperty(const std::string& propertyName) {
        try {
            if (!device_) return;
            
            json internalValue = device_->getProperty(propertyName);
            
            // Synchronize to ASCOM
            if (ascomAdapter_) {
                try {
                    ascomAdapter_->setProperty(propertyName, internalValue, "ASCOM");
                } catch (const std::exception& e) {
                    SPDLOG_DEBUG("Failed to sync property {} to ASCOM: {}", propertyName, e.what());
                }
            }
            
            // Synchronize to INDI
            if (indiAdapter_) {
                try {
                    indiAdapter_->setProperty(propertyName, internalValue, "INDI");
                } catch (const std::exception& e) {
                    SPDLOG_DEBUG("Failed to sync property {} to INDI: {}", propertyName, e.what());
                }
            }
            
            propertiesSynchronized_++;
            
        } catch (const std::exception& e) {
            SPDLOG_DEBUG("Failed to synchronize property {}: {}", propertyName, e.what());
        }
    }
    
    ascom::ASCOMDeviceType getASCOMDeviceType() const {
        // Determine ASCOM device type based on DeviceType
        if constexpr (std::is_same_v<DeviceType, ICamera>) {
            return ascom::ASCOMDeviceType::CAMERA;
        } else if constexpr (std::is_same_v<DeviceType, ITelescope>) {
            return ascom::ASCOMDeviceType::TELESCOPE;
        } else if constexpr (std::is_same_v<DeviceType, IFocuser>) {
            return ascom::ASCOMDeviceType::FOCUSER;
        } else if constexpr (std::is_same_v<DeviceType, IRotator>) {
            return ascom::ASCOMDeviceType::ROTATOR;
        } else if constexpr (std::is_same_v<DeviceType, IFilterWheel>) {
            return ascom::ASCOMDeviceType::FILTER_WHEEL;
        } else if constexpr (std::is_same_v<DeviceType, IDome>) {
            return ascom::ASCOMDeviceType::DOME;
        } else if constexpr (std::is_same_v<DeviceType, ISwitch>) {
            return ascom::ASCOMDeviceType::SWITCH;
        } else if constexpr (std::is_same_v<DeviceType, ISafetyMonitor>) {
            return ascom::ASCOMDeviceType::SAFETY_MONITOR;
        } else if constexpr (std::is_same_v<DeviceType, ICoverCalibrator>) {
            return ascom::ASCOMDeviceType::COVER_CALIBRATOR;
        } else if constexpr (std::is_same_v<DeviceType, IObservingConditions>) {
            return ascom::ASCOMDeviceType::OBSERVING_CONDITIONS;
        } else {
            return ascom::ASCOMDeviceType::TELESCOPE; // Default
        }
    }
};

/**
 * @brief Bridge factory for creating transparent protocol bridges
 */
class ProtocolBridgeFactory {
public:
    template<typename DeviceType>
    static std::shared_ptr<TransparentProtocolBridge<DeviceType>> 
    createBridge(std::shared_ptr<DeviceType> device, const BridgeConfiguration& config = BridgeConfiguration()) {
        return std::make_shared<TransparentProtocolBridge<DeviceType>>(device, config);
    }
    
    template<typename DeviceType>
    static std::shared_ptr<TransparentProtocolBridge<DeviceType>>
    createAndStartBridge(std::shared_ptr<DeviceType> device, const BridgeConfiguration& config = BridgeConfiguration()) {
        auto bridge = createBridge(device, config);
        bridge->start();
        return bridge;
    }
};

} // namespace bridge
} // namespace interfaces
} // namespace device
} // namespace astrocomm

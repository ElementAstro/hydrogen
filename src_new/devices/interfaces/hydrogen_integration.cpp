#include "automatic_compatibility.h"
#include "../device_registry.h"
#include "../../device_component/src/device.cpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <unordered_map>

namespace hydrogen {
namespace device {
namespace interfaces {
namespace hydrogen {

/**
 * @brief Integration class that connects the automatic compatibility system
 * with the existing Hydrogen device infrastructure
 */
class HydrogenCompatibilityIntegration {
public:
    static HydrogenCompatibilityIntegration& getInstance() {
        static HydrogenCompatibilityIntegration instance;
        return instance;
    }
    
    /**
     * @brief Initialize the integration with existing Hydrogen systems
     */
    void initialize() {
        if (initialized_) return;
        
        SPDLOG_INFO("Initializing Hydrogen ASCOM/INDI compatibility integration");
        
        // Initialize the automatic compatibility system
        compatibility::initializeCompatibilitySystem(
            true,  // enableAutoDiscovery
            true,  // enableASCOM
            true,  // enableINDI
            7624   // indiBasePort
        );
        
        // Hook into existing device registration system
        setupDeviceRegistrationHooks();
        
        // Setup automatic discovery for existing devices
        setupExistingDeviceDiscovery();
        
        initialized_ = true;
        SPDLOG_INFO("Hydrogen compatibility integration initialized successfully");
    }
    
    /**
     * @brief Shutdown the integration
     */
    void shutdown() {
        if (!initialized_) return;
        
        SPDLOG_INFO("Shutting down Hydrogen compatibility integration");
        
        // Disable compatibility for all registered devices
        for (const auto& [deviceId, bridge] : deviceBridges_) {
            compatibility::disableAutomaticCompatibility(deviceId);
        }
        deviceBridges_.clear();
        
        // Shutdown the compatibility system
        compatibility::shutdownCompatibilitySystem();
        
        initialized_ = false;
        SPDLOG_INFO("Hydrogen compatibility integration shutdown complete");
    }
    
    /**
     * @brief Enable compatibility for a specific device type
     */
    template<typename DeviceType>
    void enableDeviceCompatibility(const std::string& deviceId, std::shared_ptr<DeviceType> device) {
        if (!initialized_) {
            SPDLOG_WARN("Integration not initialized, initializing now");
            initialize();
        }
        
        try {
            auto bridge = compatibility::enableAutomaticCompatibility(device, deviceId, true, true);
            deviceBridges_[deviceId] = std::static_pointer_cast<void>(bridge);
            
            SPDLOG_INFO("Enabled ASCOM/INDI compatibility for device: {}", deviceId);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Failed to enable compatibility for device {}: {}", deviceId, e.what());
        }
    }
    
    /**
     * @brief Get statistics for the integration
     */
    integration::AutomaticIntegrationManager::IntegrationStatistics getStatistics() const {
        if (!initialized_) {
            integration::AutomaticIntegrationManager::IntegrationStatistics empty;
            return empty;
        }
        
        return compatibility::getSystemStatistics();
    }

private:
    bool initialized_ = false;
    std::unordered_map<std::string, std::shared_ptr<void>> deviceBridges_;
    
    HydrogenCompatibilityIntegration() = default;
    ~HydrogenCompatibilityIntegration() {
        shutdown();
    }
    
    // Prevent copying
    HydrogenCompatibilityIntegration(const HydrogenCompatibilityIntegration&) = delete;
    HydrogenCompatibilityIntegration& operator=(const HydrogenCompatibilityIntegration&) = delete;
    
    void setupDeviceRegistrationHooks() {
        // Hook into the existing device registration system
        // This would integrate with the DeviceRegistry and DeviceManager
        
        auto& manager = integration::AutomaticIntegrationManager::getInstance();
        
        // Add callback for when devices are discovered
        manager.addDeviceDiscoveryCallback([this](const std::string& deviceId, std::shared_ptr<core::IDevice> device) {
            SPDLOG_INFO("Auto-discovered device for compatibility: {}", deviceId);
            
            // Automatically enable compatibility based on device type
            auto deviceType = device->getProperty("deviceType").get<std::string>();
            
            if (deviceType == "CAMERA") {
                auto camera = std::dynamic_pointer_cast<Camera>(device);
                if (camera) {
                    enableDeviceCompatibility(deviceId, camera);
                }
            } else if (deviceType == "TELESCOPE") {
                auto telescope = std::dynamic_pointer_cast<Telescope>(device);
                if (telescope) {
                    enableDeviceCompatibility(deviceId, telescope);
                }
            } else if (deviceType == "FOCUSER") {
                auto focuser = std::dynamic_pointer_cast<Focuser>(device);
                if (focuser) {
                    enableDeviceCompatibility(deviceId, focuser);
                }
            } else if (deviceType == "FILTER_WHEEL") {
                auto filterWheel = std::dynamic_pointer_cast<FilterWheel>(device);
                if (filterWheel) {
                    enableDeviceCompatibility(deviceId, filterWheel);
                }
            } else if (deviceType == "ROTATOR") {
                auto rotator = std::dynamic_pointer_cast<Rotator>(device);
                if (rotator) {
                    enableDeviceCompatibility(deviceId, rotator);
                }
            } else if (deviceType == "DOME") {
                auto dome = std::dynamic_pointer_cast<Dome>(device);
                if (dome) {
                    enableDeviceCompatibility(deviceId, dome);
                }
            } else if (deviceType == "COVER_CALIBRATOR") {
                auto coverCalibrator = std::dynamic_pointer_cast<CoverCalibrator>(device);
                if (coverCalibrator) {
                    enableDeviceCompatibility(deviceId, coverCalibrator);
                }
            } else if (deviceType == "OBSERVING_CONDITIONS") {
                auto observingConditions = std::dynamic_pointer_cast<ObservingConditions>(device);
                if (observingConditions) {
                    enableDeviceCompatibility(deviceId, observingConditions);
                }
            } else if (deviceType == "SAFETY_MONITOR") {
                auto safetyMonitor = std::dynamic_pointer_cast<SafetyMonitor>(device);
                if (safetyMonitor) {
                    enableDeviceCompatibility(deviceId, safetyMonitor);
                }
            }
        });
        
        // Add callback for when devices are removed
        manager.addDeviceRemovalCallback([this](const std::string& deviceId) {
            SPDLOG_INFO("Device removed from compatibility: {}", deviceId);
            deviceBridges_.erase(deviceId);
        });
    }
    
    void setupExistingDeviceDiscovery() {
        // In a real implementation, this would scan the existing DeviceManager
        // for already registered devices and enable compatibility for them
        
        SPDLOG_DEBUG("Scanning for existing devices to enable compatibility");
        
        // This is where we would integrate with the existing device management system
        // to automatically discover and enable compatibility for devices that are
        // already running in the system
    }
};

/**
 * @brief Convenience functions for Hydrogen integration
 */

/**
 * @brief Initialize Hydrogen ASCOM/INDI compatibility
 * Call this once at application startup
 */
void initializeHydrogenCompatibility() {
    HydrogenCompatibilityIntegration::getInstance().initialize();
}

/**
 * @brief Shutdown Hydrogen ASCOM/INDI compatibility
 * Call this at application shutdown
 */
void shutdownHydrogenCompatibility() {
    HydrogenCompatibilityIntegration::getInstance().shutdown();
}

/**
 * @brief Enable compatibility for a device created through Hydrogen's device system
 */
template<typename DeviceType>
void enableHydrogenDeviceCompatibility(const std::string& deviceId, std::shared_ptr<DeviceType> device) {
    HydrogenCompatibilityIntegration::getInstance().enableDeviceCompatibility(deviceId, device);
}

/**
 * @brief Get integration statistics
 */
integration::AutomaticIntegrationManager::IntegrationStatistics getHydrogenCompatibilityStatistics() {
    return HydrogenCompatibilityIntegration::getInstance().getStatistics();
}

/**
 * @brief Modified device creation functions that automatically enable compatibility
 */
namespace enhanced {

/**
 * @brief Create camera with automatic ASCOM/INDI compatibility
 */
inline std::shared_ptr<Camera> createCompatibleCamera(const std::string& deviceId,
                                                      const std::string& manufacturer = "ZWO",
                                                      const std::string& model = "ASI") {
    auto camera = DeviceCreator::createCamera(deviceId, manufacturer, model);
    camera->initializeDevice();
    camera->startDevice();
    
    enableHydrogenDeviceCompatibility(deviceId, camera);
    
    return camera;
}

/**
 * @brief Create telescope with automatic ASCOM/INDI compatibility
 */
inline std::shared_ptr<Telescope> createCompatibleTelescope(const std::string& deviceId,
                                                            const std::string& manufacturer = "Celestron",
                                                            const std::string& model = "CGX") {
    auto telescope = DeviceCreator::createTelescope(deviceId, manufacturer, model);
    telescope->initializeDevice();
    telescope->startDevice();
    
    enableHydrogenDeviceCompatibility(deviceId, telescope);
    
    return telescope;
}

/**
 * @brief Create focuser with automatic ASCOM/INDI compatibility
 */
inline std::shared_ptr<Focuser> createCompatibleFocuser(const std::string& deviceId,
                                                        const std::string& manufacturer = "ZWO",
                                                        const std::string& model = "EAF") {
    auto focuser = DeviceCreator::createFocuser(deviceId, manufacturer, model);
    focuser->initializeDevice();
    focuser->startDevice();
    
    enableHydrogenDeviceCompatibility(deviceId, focuser);
    
    return focuser;
}

/**
 * @brief Create dome with automatic ASCOM/INDI compatibility
 */
inline std::shared_ptr<Dome> createCompatibleDome(const std::string& deviceId,
                                                  const std::string& manufacturer = "Generic",
                                                  const std::string& model = "Dome") {
    auto dome = DeviceCreator::createDome(deviceId, manufacturer, model);
    dome->initializeDevice();
    dome->startDevice();
    
    enableHydrogenDeviceCompatibility(deviceId, dome);
    
    return dome;
}

/**
 * @brief Create observing conditions with automatic ASCOM/INDI compatibility
 */
inline std::shared_ptr<ObservingConditions> createCompatibleObservingConditions(const std::string& deviceId,
                                                                                const std::string& manufacturer = "Generic",
                                                                                const std::string& model = "WeatherStation") {
    auto observingConditions = DeviceCreator::createObservingConditions(deviceId, manufacturer, model);
    observingConditions->initializeDevice();
    observingConditions->startDevice();
    
    enableHydrogenDeviceCompatibility(deviceId, observingConditions);
    
    return observingConditions;
}

} // namespace enhanced

/**
 * @brief Enhanced macros for Hydrogen integration
 */
#define CREATE_COMPATIBLE_CAMERA(deviceId, manufacturer, model) \
    hydrogen::device::interfaces::hydrogen::enhanced::createCompatibleCamera(deviceId, manufacturer, model)

#define CREATE_COMPATIBLE_TELESCOPE(deviceId, manufacturer, model) \
    hydrogen::device::interfaces::hydrogen::enhanced::createCompatibleTelescope(deviceId, manufacturer, model)

#define CREATE_COMPATIBLE_FOCUSER(deviceId, manufacturer, model) \
    hydrogen::device::interfaces::hydrogen::enhanced::createCompatibleFocuser(deviceId, manufacturer, model)

#define CREATE_COMPATIBLE_DOME(deviceId, manufacturer, model) \
    hydrogen::device::interfaces::hydrogen::enhanced::createCompatibleDome(deviceId, manufacturer, model)

#define CREATE_COMPATIBLE_OBSERVING_CONDITIONS(deviceId, manufacturer, model) \
    hydrogen::device::interfaces::hydrogen::enhanced::createCompatibleObservingConditions(deviceId, manufacturer, model)

#define INIT_HYDROGEN_COMPATIBILITY() \
    hydrogen::device::interfaces::hydrogen::initializeHydrogenCompatibility()

#define SHUTDOWN_HYDROGEN_COMPATIBILITY() \
    hydrogen::device::interfaces::hydrogen::shutdownHydrogenCompatibility()

#define GET_HYDROGEN_COMPATIBILITY_STATS() \
    hydrogen::device::interfaces::hydrogen::getHydrogenCompatibilityStatistics()

} // namespace hydrogen
} // namespace interfaces
} // namespace device
} // namespace hydrogen

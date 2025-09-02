#pragma once

/**
 * @file automatic_compatibility.h
 * @brief Master header for automatic ASCOM/INDI compatibility system
 * 
 * This header provides a single include point for the complete automatic
 * compatibility system that enables seamless ASCOM and INDI protocol
 * support for internal devices without requiring code changes.
 */

#include "automatic_adapter.h"
#include "ascom_bridge.h"
#include "indi_bridge.h"
#include "protocol_bridge.h"
#include "integration_manager.h"

namespace hydrogen {
namespace device {
namespace interfaces {

/**
 * @brief Main compatibility system namespace
 */
namespace compatibility {

/**
 * @brief Quick setup function for automatic compatibility
 * 
 * This function provides a one-line setup for enabling automatic
 * ASCOM/INDI compatibility for any device.
 * 
 * @tparam DeviceType The type of device to enable compatibility for
 * @param device Shared pointer to the device instance
 * @param deviceId Unique identifier for the device
 * @param enableASCOM Enable ASCOM protocol support
 * @param enableINDI Enable INDI protocol support
 * @return Shared pointer to the protocol bridge for advanced control
 */
template<typename DeviceType>
std::shared_ptr<bridge::TransparentProtocolBridge<DeviceType>>
enableAutomaticCompatibility(std::shared_ptr<DeviceType> device,
                             const std::string& deviceId,
                             bool enableASCOM = true,
                             bool enableINDI = true) {
    
    // Create bridge configuration
    bridge::BridgeConfiguration config;
    config.deviceName = deviceId;
    config.deviceDescription = "Hydrogen " + deviceId;
    config.enableASCOM = enableASCOM;
    config.enableINDI = enableINDI;
    config.autoRegisterCOM = true;
    config.autoStartServers = true;
    
    // Create and start bridge
    auto bridge = bridge::ProtocolBridgeFactory::createAndStartBridge(device, config);
    
    // Register with integration manager
    auto& manager = integration::AutomaticIntegrationManager::getInstance();
    if (!manager.getConfiguration().autoDiscovery) {
        integration::IntegrationConfiguration integrationConfig;
        manager.initialize(integrationConfig);
        manager.start();
    }
    
    manager.registerDevice(deviceId, device);
    
    SPDLOG_INFO("Automatic compatibility enabled for device: {} (ASCOM: {}, INDI: {})", 
               deviceId, enableASCOM ? "Yes" : "No", enableINDI ? "Yes" : "No");
    
    return bridge;
}

/**
 * @brief Disable automatic compatibility for a device
 * 
 * @param deviceId The device identifier to disable compatibility for
 */
inline void disableAutomaticCompatibility(const std::string& deviceId) {
    auto& manager = integration::AutomaticIntegrationManager::getInstance();
    manager.unregisterDevice(deviceId);
    
    SPDLOG_INFO("Automatic compatibility disabled for device: {}", deviceId);
}

/**
 * @brief Initialize the complete compatibility system
 * 
 * This function initializes the entire automatic compatibility system
 * with sensible defaults for most use cases.
 * 
 * @param enableAutoDiscovery Enable automatic device discovery
 * @param enableASCOM Enable ASCOM protocol support globally
 * @param enableINDI Enable INDI protocol support globally
 * @param indiBasePort Base port for INDI servers (incremented for each device)
 */
inline void initializeCompatibilitySystem(bool enableAutoDiscovery = true,
                                         bool enableASCOM = true,
                                         bool enableINDI = true,
                                         int indiBasePort = 7624) {
    
    integration::IntegrationConfiguration config;
    config.autoDiscovery = enableAutoDiscovery;
    config.autoRegistration = true;
    config.enableASCOM = enableASCOM;
    config.enableINDI = enableINDI;
    config.indiBasePort = indiBasePort;
    config.deviceNamePrefix = "Hydrogen_";
    
    auto& manager = integration::AutomaticIntegrationManager::getInstance();
    manager.initialize(config);
    manager.start();
    
    SPDLOG_INFO("Automatic compatibility system initialized (Auto-discovery: {}, ASCOM: {}, INDI: {})",
               enableAutoDiscovery ? "Yes" : "No",
               enableASCOM ? "Yes" : "No", 
               enableINDI ? "Yes" : "No");
}

/**
 * @brief Shutdown the compatibility system
 */
inline void shutdownCompatibilitySystem() {
    auto& manager = integration::AutomaticIntegrationManager::getInstance();
    manager.stop();
    
    SPDLOG_INFO("Automatic compatibility system shutdown");
}

/**
 * @brief Get system statistics
 */
inline integration::AutomaticIntegrationManager::IntegrationStatistics getSystemStatistics() {
    auto& manager = integration::AutomaticIntegrationManager::getInstance();
    return manager.getStatistics();
}

/**
 * @brief Convenience class for RAII-style compatibility management
 */
class CompatibilitySystemManager {
public:
    explicit CompatibilitySystemManager(bool enableAutoDiscovery = true,
                                       bool enableASCOM = true,
                                       bool enableINDI = true,
                                       int indiBasePort = 7624) {
        initializeCompatibilitySystem(enableAutoDiscovery, enableASCOM, enableINDI, indiBasePort);
    }
    
    ~CompatibilitySystemManager() {
        shutdownCompatibilitySystem();
    }
    
    // Prevent copying
    CompatibilitySystemManager(const CompatibilitySystemManager&) = delete;
    CompatibilitySystemManager& operator=(const CompatibilitySystemManager&) = delete;
    
    // Allow moving
    CompatibilitySystemManager(CompatibilitySystemManager&&) = default;
    CompatibilitySystemManager& operator=(CompatibilitySystemManager&&) = default;
    
    template<typename DeviceType>
    std::shared_ptr<bridge::TransparentProtocolBridge<DeviceType>>
    enableDevice(std::shared_ptr<DeviceType> device, const std::string& deviceId,
                bool enableASCOM = true, bool enableINDI = true) {
        return enableAutomaticCompatibility(device, deviceId, enableASCOM, enableINDI);
    }
    
    void disableDevice(const std::string& deviceId) {
        disableAutomaticCompatibility(deviceId);
    }
    
    integration::AutomaticIntegrationManager::IntegrationStatistics getStatistics() const {
        return getSystemStatistics();
    }
};

} // namespace compatibility

/**
 * @brief Ultra-simple macros for one-line compatibility enabling
 */
#define ENABLE_ASCOM_INDI_COMPATIBILITY(device, deviceId) \
    hydrogen::device::interfaces::compatibility::enableAutomaticCompatibility(device, deviceId, true, true)

#define ENABLE_ASCOM_COMPATIBILITY(device, deviceId) \
    hydrogen::device::interfaces::compatibility::enableAutomaticCompatibility(device, deviceId, true, false)

#define ENABLE_INDI_COMPATIBILITY(device, deviceId) \
    hydrogen::device::interfaces::compatibility::enableAutomaticCompatibility(device, deviceId, false, true)

#define DISABLE_COMPATIBILITY(deviceId) \
    hydrogen::device::interfaces::compatibility::disableAutomaticCompatibility(deviceId)

#define INIT_COMPATIBILITY_SYSTEM() \
    hydrogen::device::interfaces::compatibility::initializeCompatibilitySystem()

#define SHUTDOWN_COMPATIBILITY_SYSTEM() \
    hydrogen::device::interfaces::compatibility::shutdownCompatibilitySystem()

/**
 * @brief Example usage patterns
 * 
 * // Basic usage - enable both ASCOM and INDI for a camera
 * auto camera = std::make_shared<Camera>("cam1", "Manufacturer", "Model");
 * ENABLE_ASCOM_INDI_COMPATIBILITY(camera, "camera1");
 * 
 * // Advanced usage with custom configuration
 * auto telescope = std::make_shared<Telescope>("tel1", "Manufacturer", "Model");
 * bridge::BridgeConfiguration config;
 * config.deviceName = "MyTelescope";
 * config.deviceDescription = "Custom Telescope";
 * config.enableASCOM = true;
 * config.enableINDI = true;
 * auto bridge = bridge::ProtocolBridgeFactory::createAndStartBridge(telescope, config);
 * 
 * // RAII-style system management
 * {
 *     compatibility::CompatibilitySystemManager manager;
 *     
 *     auto camera = std::make_shared<Camera>("cam1", "Mfg", "Model");
 *     auto bridge = manager.enableDevice(camera, "camera1");
 *     
 *     // Use device through any protocol
 *     bridge->setProperty<bool>("Connected", true, bridge::ProtocolType::ASCOM);
 *     double temp = bridge->getProperty<double>("CCDTemperature", bridge::ProtocolType::ASCOM);
 *     
 *     // System automatically shuts down when manager goes out of scope
 * }
 * 
 * // Direct protocol access through integration manager
 * auto temp = GET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "temperature", double, bridge::ProtocolType::INTERNAL);
 * SET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "coolerOn", true, bridge::ProtocolType::ASCOM);
 * INVOKE_DEVICE_METHOD_AUTO(Camera, void, "camera1", "StartExposure", bridge::ProtocolType::ASCOM, 5.0, true);
 */

} // namespace interfaces
} // namespace device
} // namespace hydrogen

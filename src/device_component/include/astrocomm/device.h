#pragma once

/**
 * @file device.h
 * @brief Main header for AstroComm Device component
 * 
 * This header provides access to all device functionality including:
 * - Device interfaces and base classes
 * - WebSocket-enabled device communication
 * - Concrete device implementations (telescope, camera, etc.)
 * - Device registration and factory system
 */

// Core functionality (required dependency)
#include <astrocomm/core.h>

// Device components
#include "device/websocket_device.h"
#include "device/telescope.h"

/**
 * @namespace astrocomm::device
 * @brief Device functionality namespace for AstroComm library
 * 
 * This namespace contains all the device implementations for controlling
 * astronomical equipment, including telescopes, cameras, focusers, and
 * other specialized devices.
 */
namespace astrocomm {
namespace device {

/**
 * @brief Initialize the device component
 * 
 * This function should be called before using any device functionality.
 * It initializes the core component and registers built-in device types.
 */
void initialize();

/**
 * @brief Cleanup the device component
 * 
 * This function should be called when shutting down to clean up
 * any device resources and stop running devices.
 */
void cleanup();

/**
 * @brief Get the version of the device component
 * 
 * @return Version string in the format "major.minor.patch"
 */
std::string getVersion();

/**
 * @brief Register built-in device types
 * 
 * This function registers all the built-in device types with the
 * device registry, making them available for creation.
 */
void registerBuiltinDeviceTypes();

/**
 * @brief Create a telescope device
 * 
 * @param deviceId Unique device identifier
 * @param manufacturer Device manufacturer (optional)
 * @param model Device model (optional)
 * @return Unique pointer to telescope device
 */
std::unique_ptr<Telescope> createTelescope(const std::string& deviceId,
                                         const std::string& manufacturer = "Generic",
                                         const std::string& model = "Telescope");

/**
 * @brief Create a device from registry
 * 
 * @param deviceType Device type name
 * @param deviceId Unique device identifier
 * @param config Device configuration (optional)
 * @return Unique pointer to device, or nullptr if type not found
 */
std::unique_ptr<core::IDevice> createDevice(const std::string& deviceType,
                                          const std::string& deviceId,
                                          const nlohmann::json& config = nlohmann::json::object());

} // namespace device
} // namespace astrocomm

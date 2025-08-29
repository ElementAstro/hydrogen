#pragma once

/**
 * @file server.h
 * @brief Main header for AstroComm Server component
 * 
 * This header provides access to all server functionality including:
 * - Device server for HTTP/WebSocket communication
 * - Device management and registration
 * - Authentication and authorization
 * - Session management
 */

// Core functionality (required dependency)
#include <astrocomm/core.h>

// Server components
#include "server/device_server.h"
#include "server/device_manager.h"
#include "server/auth_manager.h"

/**
 * @namespace astrocomm::server
 * @brief Server functionality namespace for AstroComm library
 * 
 * This namespace contains all the server-side components for managing
 * astronomical devices, including HTTP/WebSocket communication, device
 * registration, and user authentication.
 */
namespace astrocomm {
namespace server {

/**
 * @brief Initialize the server component
 * 
 * This function should be called before using any server functionality.
 * It initializes the core component and sets up server-specific resources.
 */
void initialize();

/**
 * @brief Cleanup the server component
 * 
 * This function should be called when shutting down to clean up
 * any server resources and stop running services.
 */
void cleanup();

/**
 * @brief Get the version of the server component
 * 
 * @return Version string in the format "major.minor.patch"
 */
std::string getVersion();

/**
 * @brief Create a default configured device server
 * 
 * @param port Port number for the server (default: 8000)
 * @param persistenceDir Directory for storing configurations (default: "./data")
 * @return Unique pointer to configured DeviceServer
 */
std::unique_ptr<DeviceServer> createDeviceServer(int port = 8000, 
                                               const std::string& persistenceDir = "./data");

} // namespace server
} // namespace astrocomm

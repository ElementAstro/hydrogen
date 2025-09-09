#pragma once

/**
 * @file client.h
 * @brief Main header for Hydrogen Client component
 * 
 * This header provides access to all client functionality including:
 * - Device client for connecting to servers
 * - Device discovery and control
 * - Property and event subscriptions
 * - Asynchronous communication
 */

// Core functionality (required dependency)
#include <hydrogen/core.h>

// Client components
#include "client/device_client.h"

/**
 * @namespace hydrogen::client
 * @brief Client functionality namespace for Hydrogen library
 * 
 * This namespace contains all the client-side components for connecting
 * to astronomical device servers and controlling devices remotely.
 */
namespace hydrogen {
namespace client {

/**
 * @brief Initialize the client component
 * 
 * This function should be called before using any client functionality.
 * It initializes the core component and sets up client-specific resources.
 */
void initialize();

/**
 * @brief Cleanup the client component
 * 
 * This function should be called when shutting down to clean up
 * any client resources and close connections.
 */
void cleanup();

/**
 * @brief Get the version of the client component
 * 
 * @return Version string in the format "major.minor.patch"
 */
std::string getVersion();

/**
 * @brief Create a device client with default configuration
 * 
 * @return Unique pointer to configured DeviceClient
 */
std::unique_ptr<DeviceClient> createDeviceClient();

/**
 * @brief Create a device client and connect to server
 * 
 * @param host Server hostname or IP address
 * @param port Server port number
 * @return Unique pointer to connected DeviceClient, or nullptr if connection failed
 */
std::unique_ptr<DeviceClient> createAndConnect(const std::string& host, uint16_t port);

} // namespace client
} // namespace hydrogen

#pragma once

/**
 * @file core.h
 * @brief Main header for Hydrogen Core component
 * 
 * This header provides access to all core functionality including:
 * - Message handling and communication protocols
 * - Utility functions for common operations
 * - Message queue management
 * - Error recovery and handling
 */

// Core utilities
#include "core/utils.h"

// Message system
#include "core/message.h"
#include "core/message_queue.h"

// Error handling
#include "core/error_recovery.h"

// Device interfaces
#include "core/device_interface.h"
#include "core/device_health.h"
#include "core/device_lifecycle.h"

/**
 * @namespace hydrogen::core
 * @brief Core functionality namespace for Hydrogen library
 * 
 * This namespace contains all the fundamental components that are shared
 * across the Hydrogen library, including message handling, utilities,
 * and error recovery mechanisms.
 */
namespace hydrogen {
namespace core {

/**
 * @brief Initialize the core component
 * 
 * This function should be called before using any core functionality.
 * It sets up logging and other global state.
 */
void initialize();

/**
 * @brief Cleanup the core component
 * 
 * This function should be called when shutting down to clean up
 * any global resources.
 */
void cleanup();

/**
 * @brief Get the version of the core component
 * 
 * @return Version string in the format "major.minor.patch"
 */
std::string getVersion();

} // namespace core
} // namespace hydrogen

#pragma once

#include "client/message_processor.h"
#include "common/message.h"
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace astrocomm {

using json = nlohmann::json;

/**
 * @brief Manages device discovery, caching, and property operations
 * 
 * This class is responsible for:
 * - Device discovery and enumeration
 * - Caching device information locally
 * - Getting and setting device properties
 * - Managing device state information
 */
class DeviceManager {
public:
    /**
     * @brief Constructor
     * @param messageProcessor Pointer to the message processor for communication
     */
    explicit DeviceManager(MessageProcessor* messageProcessor);

    /**
     * @brief Destructor
     */
    ~DeviceManager();

    /**
     * @brief Discover devices on the network
     * @param deviceTypes Optional list of device types to filter by
     * @return JSON object containing discovered devices
     * @throws std::runtime_error if not connected or discovery fails
     */
    json discoverDevices(const std::vector<std::string>& deviceTypes = {});

    /**
     * @brief Get the cached list of devices
     * @return JSON object containing cached device information
     */
    json getDevices() const;

    /**
     * @brief Get specific properties from a device
     * @param deviceId The ID of the device
     * @param properties List of property names to retrieve
     * @return JSON object containing the requested properties
     * @throws std::runtime_error if not connected or device not found
     */
    json getDeviceProperties(const std::string& deviceId, 
                            const std::vector<std::string>& properties);

    /**
     * @brief Set properties on a device
     * @param deviceId The ID of the device
     * @param properties JSON object containing property name-value pairs
     * @return JSON object containing the result of the operation
     * @throws std::runtime_error if not connected or device not found
     */
    json setDeviceProperties(const std::string& deviceId, const json& properties);

    /**
     * @brief Get information about a specific device
     * @param deviceId The ID of the device
     * @return JSON object containing device information, or null if not found
     */
    json getDeviceInfo(const std::string& deviceId) const;

    /**
     * @brief Check if a device exists in the cache
     * @param deviceId The ID of the device
     * @return true if device exists, false otherwise
     */
    bool hasDevice(const std::string& deviceId) const;

    /**
     * @brief Get a list of all device IDs
     * @return Vector of device ID strings
     */
    std::vector<std::string> getDeviceIds() const;

    /**
     * @brief Get devices filtered by type
     * @param deviceType The type of devices to filter by
     * @return JSON object containing devices of the specified type
     */
    json getDevicesByType(const std::string& deviceType) const;

    /**
     * @brief Clear the device cache
     */
    void clearDeviceCache();

    /**
     * @brief Get device management statistics
     * @return JSON object with device management statistics
     */
    json getDeviceStats() const;

    /**
     * @brief Update device information in the cache
     * @param deviceId The ID of the device
     * @param deviceInfo JSON object containing updated device information
     */
    void updateDeviceInfo(const std::string& deviceId, const json& deviceInfo);

    /**
     * @brief Remove a device from the cache
     * @param deviceId The ID of the device to remove
     */
    void removeDevice(const std::string& deviceId);

private:
    // Message processor for communication
    MessageProcessor* messageProcessor;

    // Device cache
    mutable std::mutex devicesMutex;
    json devices;

    // Statistics
    mutable std::mutex statsMutex;
    mutable size_t discoveryRequests;
    mutable size_t propertyRequests;
    mutable size_t propertyUpdates;
    mutable size_t cacheHits;
    mutable size_t cacheMisses;

    /**
     * @brief Update device management statistics
     * @param discoveries Number of discovery requests
     * @param propRequests Number of property requests
     * @param propUpdates Number of property updates
     * @param hits Number of cache hits
     * @param misses Number of cache misses
     */
    void updateStats(size_t discoveries = 0, size_t propRequests = 0,
                    size_t propUpdates = 0, size_t hits = 0, size_t misses = 0) const;

    /**
     * @brief Validate device ID format
     * @param deviceId The device ID to validate
     * @return true if valid, false otherwise
     */
    bool isValidDeviceId(const std::string& deviceId) const;

    /**
     * @brief Process discovery response and update cache
     * @param response The discovery response JSON
     */
    void processDiscoveryResponse(const json& response);
};

} // namespace astrocomm

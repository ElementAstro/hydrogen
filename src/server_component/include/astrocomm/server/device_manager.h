#pragma once

#include <astrocomm/core/message.h>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <condition_variable>
#include <functional>
#include <set>
#include <map>

namespace astrocomm {
namespace server {

using json = nlohmann::json;
using namespace astrocomm::core;

/**
 * @class DeviceManager
 * @brief Manages all connected astronomical devices in the system.
 *
 * This class provides functionality to track, monitor, and persist information
 * about astronomical devices connected to the system. It supports device
 * lifecycle management, property updates, status monitoring, and configuration
 * persistence.
 */
class DeviceManager {
public:
  /**
   * @brief Default constructor.
   *
   * Initializes the device manager with default settings.
   */
  DeviceManager();

  /**
   * @brief Constructor with persistence configuration.
   *
   * @param persistenceDir Directory for storing device configuration files
   * @param autosaveInterval Interval in seconds for automatic configuration
   * saving
   */
  DeviceManager(const std::string &persistenceDir, int autosaveInterval = 300);

  /**
   * @brief Destructor.
   *
   * Stops the autosave thread and saves the current configuration.
   */
  ~DeviceManager();

  /**
   * @brief Register a new device.
   *
   * @param deviceId Unique identifier for the device
   * @param deviceInfo JSON object containing device information
   * @return True if registration was successful, false otherwise
   */
  bool registerDevice(const std::string &deviceId, const json &deviceInfo);

  /**
   * @brief Unregister a device.
   *
   * @param deviceId Unique identifier for the device to unregister
   * @return True if unregistration was successful, false otherwise
   */
  bool unregisterDevice(const std::string &deviceId);

  /**
   * @brief Update device properties.
   *
   * @param deviceId Unique identifier for the device
   * @param properties JSON object containing updated properties
   * @return True if update was successful, false otherwise
   */
  bool updateDeviceProperties(const std::string &deviceId,
                             const json &properties);

  /**
   * @brief Get device information.
   *
   * @param deviceId Unique identifier for the device
   * @return JSON object containing device information, or null if not found
   */
  json getDeviceInfo(const std::string &deviceId) const;

  /**
   * @brief Get all registered devices.
   *
   * @return JSON object containing all device information
   */
  json getAllDevices() const;

  /**
   * @brief Check if a device is registered.
   *
   * @param deviceId Unique identifier for the device
   * @return True if device is registered, false otherwise
   */
  bool isDeviceRegistered(const std::string &deviceId) const;

  /**
   * @brief Set device connection status.
   *
   * @param deviceId Unique identifier for the device
   * @param connected True if device is connected, false otherwise
   * @return True if status was updated, false if device not found
   */
  bool setDeviceConnectionStatus(const std::string &deviceId, bool connected);

  /**
   * @brief Get device connection status.
   *
   * @param deviceId Unique identifier for the device
   * @return True if device is connected, false otherwise
   */
  bool isDeviceConnected(const std::string &deviceId) const;

  /**
   * @brief Get list of connected devices.
   *
   * @return Vector of device IDs that are currently connected
   */
  std::vector<std::string> getConnectedDevices() const;

  /**
   * @brief Get list of devices by type.
   *
   * @param deviceType Type of devices to retrieve
   * @return Vector of device IDs matching the specified type
   */
  std::vector<std::string> getDevicesByType(const std::string &deviceType) const;

  /**
   * @brief Save device configuration to persistent storage.
   *
   * @return True if save was successful, false otherwise
   */
  bool saveConfiguration();

  /**
   * @brief Load device configuration from persistent storage.
   *
   * @return True if load was successful, false otherwise
   */
  bool loadConfiguration();

  /**
   * @brief Enable or disable automatic configuration saving.
   *
   * @param enabled True to enable autosave, false to disable
   */
  void setAutosaveEnabled(bool enabled);

  /**
   * @brief Set the autosave interval.
   *
   * @param intervalSeconds Interval in seconds between automatic saves
   */
  void setAutosaveInterval(int intervalSeconds);

  /**
   * @brief Get device statistics.
   *
   * @return JSON object containing device statistics
   */
  json getDeviceStatistics() const;

  /**
   * @brief Set device property change callback.
   *
   * @param callback Function to call when device properties change
   */
  void setPropertyChangeCallback(
      std::function<void(const std::string &, const json &)> callback);

  /**
   * @brief Set device connection change callback.
   *
   * @param callback Function to call when device connection status changes
   */
  void setConnectionChangeCallback(
      std::function<void(const std::string &, bool)> callback);

  /**
   * @brief Enable distributed mode for device discovery.
   *
   * @param enabled True to enable distributed mode
   * @param serverId Unique identifier for this server
   * @param discoveryPort UDP port for device discovery
   * @param multicastGroup Multicast group address
   */
  void enableDistributedMode(bool enabled, const std::string &serverId = "",
                           uint16_t discoveryPort = 8001,
                           const std::string &multicastGroup = "239.255.0.1");

  /**
   * @brief Get remote devices from other servers.
   *
   * @return JSON object containing remote device information
   */
  json getRemoteDevices() const;

  /**
   * @brief Broadcast device discovery message.
   */
  void broadcastDeviceDiscovery();

  /**
   * @brief Handle device discovery response from remote server.
   *
   * @param serverId ID of the remote server
   * @param devices JSON object containing remote device information
   */
  void handleRemoteDeviceDiscovery(const std::string &serverId, const json &devices);

private:
  // Internal methods
  void startAutosaveThread();
  void stopAutosaveThread();
  void autosaveThreadFunction();
  void startDiscoveryThread();
  void stopDiscoveryThread();
  void discoveryThreadFunction();
  void notifyPropertyChange(const std::string &deviceId, const json &properties);
  void notifyConnectionChange(const std::string &deviceId, bool connected);

  // Device storage
  mutable std::mutex devicesMutex;
  std::unordered_map<std::string, json> devices;

  // Persistence settings
  std::string persistenceDirectory;
  std::atomic<bool> autosaveEnabled;
  std::atomic<int> autosaveIntervalSeconds;
  std::atomic<bool> shutdownRequested;
  std::thread autosaveThread;
  
  // Distributed mode settings
  std::atomic<bool> distributedModeEnabled{false};
  std::string serverId;
  uint16_t discoveryPort{8001};
  std::string multicastGroup{"239.255.0.1"};
  std::thread discoveryThread;
  std::atomic<bool> discoveryRunning{false};
  mutable std::mutex remoteDevicesMutex;
  std::map<std::string, json> remoteDevices; // 服务器ID -> 设备列表

  // Callbacks
  std::function<void(const std::string &, const json &)> propertyChangeCallback;
  std::function<void(const std::string &, bool)> connectionChangeCallback;
};

} // namespace server
} // namespace astrocomm

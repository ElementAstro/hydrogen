#pragma once

#include "common/message.h"
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

using json = nlohmann::json;

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
   * Ensures safe shutdown of the device manager and persistence of current
   * state.
   */
  ~DeviceManager();

  /**
   * @brief Add a new device to the manager.
   *
   * @param deviceId Unique identifier for the device
   * @param deviceInfo JSON object containing device information
   * @throws std::runtime_error if device already exists
   */
  void addDevice(const std::string &deviceId, const json &deviceInfo);

  /**
   * @brief Update information for an existing device.
   *
   * @param deviceId Identifier of the device to update
   * @param deviceInfo JSON object containing updated device information
   * @throws std::runtime_error if device doesn't exist
   */
  void updateDevice(const std::string &deviceId, const json &deviceInfo);

  /**
   * @brief Remove a device from the manager.
   *
   * @param deviceId Identifier of the device to remove
   */
  void removeDevice(const std::string &deviceId);

  /**
   * @brief Get detailed information about a specific device.
   *
   * @param deviceId Identifier of the device
   * @return JSON object containing device information
   * @throws std::runtime_error if device doesn't exist
   */
  json getDeviceInfo(const std::string &deviceId) const;

  /**
   * @brief Get a list of devices, optionally filtered by type.
   *
   * @param deviceTypes Vector of device types to filter by (empty for all
   * devices)
   * @return JSON array of devices matching the filter
   */
  json getDevices(const std::vector<std::string> &deviceTypes = {}) const;

  /**
   * @brief Check if a device exists in the manager.
   *
   * @param deviceId Identifier of the device to check
   * @return true if device exists, false otherwise
   */
  bool deviceExists(const std::string &deviceId) const;

  /**
   * @brief Get a specific property value from a device.
   *
   * @param deviceId Identifier of the device
   * @param property Name of the property to retrieve
   * @return JSON value of the property
   * @throws std::runtime_error if device or property doesn't exist
   */
  json getDeviceProperty(const std::string &deviceId,
                         const std::string &property) const;

  /**
   * @brief Update a specific property of a device.
   *
   * @param deviceId Identifier of the device
   * @param property Name of the property to update
   * @param value New JSON value for the property
   * @throws std::runtime_error if device doesn't exist
   */
  void updateDeviceProperty(const std::string &deviceId,
                            const std::string &property, const json &value);

  /**
   * @brief Save current device configuration to file.
   *
   * @param filePath Path to the file where configuration will be saved
   * @return true if successful, false otherwise
   */
  bool saveDeviceConfiguration(const std::string &filePath) const;

  /**
   * @brief Load device configuration from file.
   *
   * @param filePath Path to the configuration file
   * @return true if successful, false otherwise
   */
  bool loadDeviceConfiguration(const std::string &filePath);

  /**
   * @brief Update the activity timestamp for a device.
   *
   * @param deviceId Identifier of the device
   */
  void updateDeviceActivity(const std::string &deviceId);

  /**
   * @brief Check status of all devices and mark inactive ones offline.
   *
   * @param timeoutSeconds Seconds of inactivity before marking a device offline
   */
  void checkDeviceStatus(int timeoutSeconds = 60);

  /**
   * @brief Increment the command counter for a device.
   *
   * @param deviceId Identifier of the device
   */
  void incrementDeviceCommandReceived(const std::string &deviceId);

  /**
   * @brief Get status information for all devices.
   *
   * @return JSON array containing status information for all devices
   */
  json getDeviceStatus() const;

  /**
   * @brief Get devices belonging to a specific group.
   *
   * @param groupName Name of the group to filter by
   * @return JSON array of devices in the specified group
   */
  json getDevicesByGroup(const std::string &groupName) const;

  /**
   * @brief Set the group for a device.
   *
   * @param deviceId Identifier of the device
   * @param groupName Name of the group to assign
   * @throws std::runtime_error if device doesn't exist
   */
  void setDeviceGroup(const std::string &deviceId,
                      const std::string &groupName);

  /**
   * @brief Configure automatic saving of device configurations.
   *
   * @param enabled Whether to enable automatic saving
   * @param intervalSeconds Interval between saves in seconds (if enabled)
   * @param directoryPath Directory path for saving configurations
   */
  void configureAutosave(bool enabled, int intervalSeconds = 300,
                         const std::string &directoryPath = "");

  /**
   * @brief Create a backup of current device configuration.
   *
   * @param backupDir Directory to store the backup
   * @return true if successful, false otherwise
   */
  bool backupConfiguration(const std::string &backupDir = "");

  /**
   * @brief Restore device configuration from a backup file.
   *
   * @param backupFilePath Path to the backup file
   * @return true if successful, false otherwise
   */
  bool restoreFromBackup(const std::string &backupFilePath);

  /**
   * @brief Get the current persistence directory.
   *
   * @return Current directory path used for persistence
   */
  std::string getPersistenceDirectory() const;

  /**
   * @brief 启用分布式模式
   * 
   * 启用后，设备管理器将在网络上发现其他管理器，并共享设备信息
   * 
   * @param enabled 是否启用分布式模式
   * @param discoveryPort 用于设备发现的UDP多播端口
   * @param multicastGroup 多播组地址
   */
  void enableDistributedMode(bool enabled, uint16_t discoveryPort = 8001,
                            const std::string &multicastGroup = "239.255.0.1");
                            
  /**
   * @brief 获取远程设备列表
   * 
   * @return 远程设备信息的JSON数组
   */
  json getRemoteDevices() const;
  
  /**
   * @brief 转发命令到远程设备
   * 
   * @param deviceId 远程设备ID
   * @param command 命令消息
   * @return 包含操作状态的JSON对象
   */
  json forwardCommandToRemoteDevice(const std::string &deviceId, const CommandMessage &command);
  
  /**
   * @brief 设置设备健康检查参数
   * 
   * @param checkIntervalSeconds 健康检查间隔(秒)
   * @param timeoutSeconds 超时时间(秒)
   * @param maxRetries 最大重试次数
   */
  void setHealthCheckParams(int checkIntervalSeconds = 30, 
                           int timeoutSeconds = 5,
                           int maxRetries = 3);
  
  /**
   * @brief 获取设备健康状态
   * 
   * @param deviceId 设备ID
   * @return 包含健康状态详情的JSON对象
   */
  json getDeviceHealthInfo(const std::string &deviceId) const;
  
  /**
   * @brief 手动触发设备健康检查
   * 
   * @param deviceId 设备ID(为空则检查所有设备)
   * @return 健康检查结果
   */
  json triggerHealthCheck(const std::string &deviceId = "");

  /**
   * @brief 获取设备拓扑图信息
   * 
   * @return 包含设备拓扑关系的JSON对象
   */
  json getDeviceTopology() const;
  
  /**
   * @brief 设置设备间的依赖关系
   * 
   * @param dependentDeviceId 依赖其他设备的设备ID
   * @param dependencyDeviceId 被依赖的设备ID
   * @param dependencyType 依赖类型(如"REQUIRED"或"OPTIONAL")
   */
  void setDeviceDependency(const std::string &dependentDeviceId,
                          const std::string &dependencyDeviceId,
                          const std::string &dependencyType = "REQUIRED");
                          
  /**
   * @brief 移除设备间的依赖关系
   * 
   * @param dependentDeviceId 依赖其他设备的设备ID
   * @param dependencyDeviceId 被依赖的设备ID
   */
  void removeDeviceDependency(const std::string &dependentDeviceId,
                             const std::string &dependencyDeviceId);
                             
  /**
   * @brief 设置设备管理器的服务器ID
   * 
   * @param serverId 唯一的服务器ID
   */
  void setServerId(const std::string &serverId);

private:
  /**
   * @brief Background thread function for autosaving configurations.
   */
  void autosaveWorker();

  /**
   * @brief Generate a timestamp-based filename for backups.
   *
   * @return Filename with timestamp
   */
  std::string generateTimestampedFilename() const;

  /**
   * @brief Ensure directory exists, create if it doesn't.
   *
   * @param dirPath Path to check/create
   * @return true if directory exists or was created, false otherwise
   */
  bool ensureDirectoryExists(const std::string &dirPath) const;

  mutable std::mutex devicesMutex;
  std::unordered_map<std::string, json> devices;

  // Persistence settings
  std::string persistenceDirectory;
  std::atomic<bool> autosaveEnabled;
  std::atomic<int> autosaveIntervalSeconds;
  std::atomic<bool> shutdownRequested;
  std::thread autosaveThread;
  
  // 分布式模式设置
  std::atomic<bool> distributedModeEnabled{false};
  std::string serverId;
  uint16_t discoveryPort{8001};
  std::string multicastGroup{"239.255.0.1"};
  std::thread discoveryThread;
  std::atomic<bool> discoveryRunning{false};
  mutable std::mutex remoteDevicesMutex;
  std::map<std::string, json> remoteDevices; // 服务器ID -> 设备列表
  
  // 设备健康检查
  std::atomic<bool> healthCheckEnabled{true};
  std::atomic<int> healthCheckIntervalSeconds{30};
  std::atomic<int> healthCheckTimeoutSeconds{5};
  std::atomic<int> healthCheckMaxRetries{3};
  std::thread healthCheckThread;
  std::atomic<bool> healthCheckRunning{false};
  mutable std::mutex healthInfoMutex;
  std::map<std::string, json> deviceHealthInfo;
  
  // 设备依赖关系
  mutable std::mutex dependencyMutex;
  std::map<std::string, std::map<std::string, std::string>> deviceDependencies; // 设备ID -> (依赖设备ID -> 依赖类型)
  
  // 辅助函数
  void startDiscoveryService();
  void stopDiscoveryService();
  void discoveryWorker();
  void broadcastDeviceInfo();
  void receiveRemoteDeviceInfo();
  
  // 健康检查相关
  void startHealthCheckService();
  void stopHealthCheckService();
  void healthCheckWorker();
  bool checkDeviceHealth(const std::string &deviceId, int retries = 1);
  void updateDeviceHealthStatus(const std::string &deviceId, bool healthy, const std::string &message = "");
  
  // 设备拓扑相关
  bool checkDependencyCycle(const std::string &startDeviceId, const std::string &newDependencyId) const;
  void validateDependencies(const std::string &deviceId);
};

} // namespace astrocomm
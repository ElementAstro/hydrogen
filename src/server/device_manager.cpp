#include "server/device_manager.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>


namespace astrocomm {

namespace fs = std::filesystem;

/**
 * @brief Default constructor.
 */
DeviceManager::DeviceManager()
    : persistenceDirectory("./data/devices"), autosaveEnabled(false),
      autosaveIntervalSeconds(300), shutdownRequested(false) {
  spdlog::info("[DeviceManager] Device manager initialized");
}

/**
 * @brief Constructor with persistence configuration.
 */
DeviceManager::DeviceManager(const std::string &persistenceDir,
                             int autosaveInterval)
    : persistenceDirectory(persistenceDir), autosaveEnabled(true),
      autosaveIntervalSeconds(autosaveInterval), shutdownRequested(false) {

  spdlog::info("[DeviceManager] Device manager initialized with persistence "
               "directory: {}",
               persistenceDir);

  // Ensure the persistence directory exists
  ensureDirectoryExists(persistenceDirectory);

  // Start the autosave thread if enabled
  if (autosaveEnabled) {
    autosaveThread = std::thread(&DeviceManager::autosaveWorker, this);
    spdlog::info("[DeviceManager] Autosave configured with {} second interval",
                 autosaveInterval);
  }
}

/**
 * @brief Destructor.
 */
DeviceManager::~DeviceManager() {
  // Signal autosave thread to stop and wait for it
  if (autosaveEnabled && autosaveThread.joinable()) {
    shutdownRequested = true;
    autosaveThread.join();
  }

  // Save configuration one final time before shutdown
  if (!persistenceDirectory.empty()) {
    std::string configPath = persistenceDirectory + "/devices.json";
    saveDeviceConfiguration(configPath);
  }

  spdlog::info("[DeviceManager] Device manager shutting down");
}

/**
 * @brief Add a new device to the manager.
 */
void DeviceManager::addDevice(const std::string &deviceId,
                              const json &deviceInfo) {
  std::lock_guard<std::mutex> lock(devicesMutex);

  if (devices.find(deviceId) != devices.end()) {
    throw std::runtime_error("Device already exists: " + deviceId);
  }

  // Create a structure containing device info and additional metadata
  json enhancedInfo = deviceInfo;

  // Add connection timestamp and initialize metadata
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();

  enhancedInfo["_metadata"] = {{"connectedAt", now},
                               {"lastSeen", now},
                               {"status", "online"},
                               {"statistics",
                                {{"commandsReceived", 0},
                                 {"commandsProcessed", 0},
                                 {"eventsGenerated", 0},
                                 {"errors", 0}}}};

  devices[deviceId] = enhancedInfo;
  spdlog::info("[DeviceManager] Device added: {}", deviceId);
}

/**
 * @brief Update information for an existing device.
 */
void DeviceManager::updateDevice(const std::string &deviceId,
                                 const json &deviceInfo) {
  std::lock_guard<std::mutex> lock(devicesMutex);

  auto it = devices.find(deviceId);
  if (it == devices.end()) {
    throw std::runtime_error("Device not found: " + deviceId);
  }

  // Preserve metadata
  json metadata = it->second.contains("_metadata") ? it->second["_metadata"]
                                                   : json::object();

  // Update device info
  it->second = deviceInfo;

  // Restore metadata
  if (!metadata.is_null()) {
    it->second["_metadata"] = metadata;
  }

  // Update last activity time
  updateDeviceActivity(deviceId);

  spdlog::info("[DeviceManager] Device updated: {}", deviceId);
}

/**
 * @brief Remove a device from the manager.
 */
void DeviceManager::removeDevice(const std::string &deviceId) {
  std::lock_guard<std::mutex> lock(devicesMutex);

  auto it = devices.find(deviceId);
  if (it != devices.end()) {
    devices.erase(it);
    spdlog::info("[DeviceManager] Device removed: {}", deviceId);
  } else {
    spdlog::warn("[DeviceManager] Attempted to remove non-existent device: {}",
                 deviceId);
  }
}

/**
 * @brief Get detailed information about a specific device.
 */
json DeviceManager::getDeviceInfo(const std::string &deviceId) const {
  std::lock_guard<std::mutex> lock(devicesMutex);

  auto it = devices.find(deviceId);
  if (it == devices.end()) {
    throw std::runtime_error("Device not found: " + deviceId);
  }

  return it->second;
}

/**
 * @brief Get a list of devices, optionally filtered by type.
 */
json DeviceManager::getDevices(
    const std::vector<std::string> &deviceTypes) const {
  std::lock_guard<std::mutex> lock(devicesMutex);

  json result = json::array();

  for (const auto &entry : devices) {
    // If no device types specified, include all devices
    if (deviceTypes.empty()) {
      result.push_back(entry.second);
      continue;
    }

    // Otherwise, filter by device type
    if (entry.second.contains("deviceType")) {
      std::string deviceType = entry.second["deviceType"];

      for (const auto &type : deviceTypes) {
        if (deviceType == type) {
          result.push_back(entry.second);
          break;
        }
      }
    }
  }

  return result;
}

/**
 * @brief Check if a device exists in the manager.
 */
bool DeviceManager::deviceExists(const std::string &deviceId) const {
  std::lock_guard<std::mutex> lock(devicesMutex);
  return devices.find(deviceId) != devices.end();
}

/**
 * @brief Get a specific property value from a device.
 */
json DeviceManager::getDeviceProperty(const std::string &deviceId,
                                      const std::string &property) const {
  std::lock_guard<std::mutex> lock(devicesMutex);

  auto deviceIt = devices.find(deviceId);
  if (deviceIt == devices.end()) {
    throw std::runtime_error("Device not found: " + deviceId);
  }

  // Check if device info has a properties object
  if (deviceIt->second.contains("properties") &&
      deviceIt->second["properties"].is_object() &&
      deviceIt->second["properties"].contains(property)) {

    return deviceIt->second["properties"][property];
  }

  throw std::runtime_error("Property not found: " + property +
                           " for device: " + deviceId);
}

/**
 * @brief Update a specific property of a device.
 */
void DeviceManager::updateDeviceProperty(const std::string &deviceId,
                                         const std::string &property,
                                         const json &value) {
  std::lock_guard<std::mutex> lock(devicesMutex);

  auto deviceIt = devices.find(deviceId);
  if (deviceIt == devices.end()) {
    throw std::runtime_error("Device not found: " + deviceId);
  }

  // Ensure properties object exists
  if (!deviceIt->second.contains("properties") ||
      !deviceIt->second["properties"].is_object()) {
    deviceIt->second["properties"] = json::object();
  }

  deviceIt->second["properties"][property] = value;

  // Update device activity
  updateDeviceActivity(deviceId);

  spdlog::info("[DeviceManager] Updated property: {} for device: {}", property,
               deviceId);
}

/**
 * @brief Save current device configuration to file.
 */
bool DeviceManager::saveDeviceConfiguration(const std::string &filePath) const {
  std::lock_guard<std::mutex> lock(devicesMutex);

  try {
    // Ensure directory exists before writing file
    fs::path path(filePath);
    ensureDirectoryExists(path.parent_path().string());

    std::ofstream file(filePath);
    if (!file.is_open()) {
      spdlog::error("[DeviceManager] Failed to open file for writing: {}",
                    filePath);
      return false;
    }

    json config;
    for (const auto &entry : devices) {
      // Create a copy without internal metadata
      json deviceCopy = entry.second;
      if (deviceCopy.contains("_metadata")) {
        deviceCopy.erase("_metadata");
      }
      config[entry.first] = deviceCopy;
    }

    file << config.dump(2);
    file.close();

    spdlog::info("[DeviceManager] Device configuration saved to {}", filePath);
    return true;
  } catch (const std::exception &e) {
    spdlog::error("[DeviceManager] Error saving device configuration: {}",
                  e.what());
    return false;
  }
}

/**
 * @brief Load device configuration from file.
 */
bool DeviceManager::loadDeviceConfiguration(const std::string &filePath) {
  try {
    std::ifstream file(filePath);
    if (!file.is_open()) {
      spdlog::warn("[DeviceManager] Failed to open configuration file: {}",
                   filePath);
      return false;
    }

    json config;
    file >> config;
    file.close();

    std::lock_guard<std::mutex> lock(devicesMutex);
    devices.clear();

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();

    for (auto it = config.begin(); it != config.end(); ++it) {
      // Add metadata
      it.value()["_metadata"] = {
          {"connectedAt", now},
          {"lastSeen", now},
          {"status", "offline"}, // Devices loaded from config start as offline
          {"statistics",
           {{"commandsReceived", 0},
            {"commandsProcessed", 0},
            {"eventsGenerated", 0},
            {"errors", 0}}}};

      devices[it.key()] = it.value();
    }

    spdlog::info(
        "[DeviceManager] Loaded device configuration from {} ({} devices)",
        filePath, devices.size());
    return true;
  } catch (const std::exception &e) {
    spdlog::error("[DeviceManager] Error loading device configuration: {}",
                  e.what());
    return false;
  }
}

/**
 * @brief Update the activity timestamp for a device.
 */
void DeviceManager::updateDeviceActivity(const std::string &deviceId) {
  auto deviceIt = devices.find(deviceId);
  if (deviceIt != devices.end()) {
    // Ensure _metadata object exists
    if (!deviceIt->second.contains("_metadata") ||
        !deviceIt->second["_metadata"].is_object()) {
      deviceIt->second["_metadata"] = json::object();
    }

    // Update last activity time
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();

    deviceIt->second["_metadata"]["lastSeen"] = now;
    deviceIt->second["_metadata"]["status"] = "online";
  }
}

/**
 * @brief Check status of all devices and mark inactive ones offline.
 */
void DeviceManager::checkDeviceStatus(int timeoutSeconds) {
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();

  std::lock_guard<std::mutex> lock(devicesMutex);

  for (auto &entry : devices) {
    if (entry.second.contains("_metadata") &&
        entry.second["_metadata"].contains("lastSeen")) {

      int64_t lastSeen = entry.second["_metadata"]["lastSeen"];
      int64_t elapsed = now - lastSeen;

      // Mark as offline if timeout exceeded
      if (elapsed > timeoutSeconds * 1000) {
        if (entry.second["_metadata"]["status"] != "offline") {
          entry.second["_metadata"]["status"] = "offline";
          spdlog::warn("[DeviceManager] Device {} marked as offline (no "
                       "activity for {}s)",
                       entry.first, elapsed / 1000);
        }
      }
    }
  }
}

/**
 * @brief Increment the command counter for a device.
 */
void DeviceManager::incrementDeviceCommandReceived(
    const std::string &deviceId) {
  std::lock_guard<std::mutex> lock(devicesMutex);

  auto deviceIt = devices.find(deviceId);
  if (deviceIt != devices.end() && deviceIt->second.contains("_metadata") &&
      deviceIt->second["_metadata"].contains("statistics")) {

    deviceIt->second["_metadata"]["statistics"]["commandsReceived"] =
        deviceIt->second["_metadata"]["statistics"]["commandsReceived"]
            .get<int>() +
        1;

    // Update device activity
    updateDeviceActivity(deviceId);
  }
}

/**
 * @brief Get status information for all devices.
 */
json DeviceManager::getDeviceStatus() const {
  std::lock_guard<std::mutex> lock(devicesMutex);

  json statusInfo = json::array();

  for (const auto &entry : devices) {
    json deviceStatus = {
        {"deviceId", entry.first},
        {"deviceType", entry.second.contains("deviceType")
                           ? entry.second["deviceType"]
                           : "unknown"},
        {"deviceName", entry.second.contains("deviceName")
                           ? entry.second["deviceName"].get<std::string>()
                           : entry.first}};

    if (entry.second.contains("_metadata")) {
      deviceStatus["status"] = entry.second["_metadata"]["status"];
      deviceStatus["lastSeen"] = entry.second["_metadata"]["lastSeen"];
      deviceStatus["statistics"] = entry.second["_metadata"]["statistics"];
    } else {
      deviceStatus["status"] = "unknown";
    }

    statusInfo.push_back(deviceStatus);
  }

  return statusInfo;
}

/**
 * @brief Get devices belonging to a specific group.
 */
json DeviceManager::getDevicesByGroup(const std::string &groupName) const {
  std::lock_guard<std::mutex> lock(devicesMutex);

  json result = json::array();

  for (const auto &entry : devices) {
    // Check if device has group information
    if (entry.second.contains("group") && entry.second["group"] == groupName) {
      result.push_back(entry.second);
    }
  }

  return result;
}

/**
 * @brief Set the group for a device.
 */
void DeviceManager::setDeviceGroup(const std::string &deviceId,
                                   const std::string &groupName) {
  std::lock_guard<std::mutex> lock(devicesMutex);

  auto deviceIt = devices.find(deviceId);
  if (deviceIt == devices.end()) {
    throw std::runtime_error("Device not found: " + deviceId);
  }

  deviceIt->second["group"] = groupName;
  spdlog::info("[DeviceManager] Device {} assigned to group: {}", deviceId,
               groupName);
}

/**
 * @brief Configure automatic saving of device configurations.
 */
void DeviceManager::configureAutosave(bool enabled, int intervalSeconds,
                                      const std::string &directoryPath) {
  // Stop existing autosave thread if running
  if (autosaveEnabled && autosaveThread.joinable()) {
    shutdownRequested = true;
    autosaveThread.join();
    shutdownRequested = false;
  }

  // Update settings
  autosaveEnabled = enabled;
  autosaveIntervalSeconds = intervalSeconds;

  if (!directoryPath.empty()) {
    persistenceDirectory = directoryPath;
    ensureDirectoryExists(persistenceDirectory);
  }

  // Start new autosave thread if enabled
  if (autosaveEnabled) {
    autosaveThread = std::thread(&DeviceManager::autosaveWorker, this);
    spdlog::info(
        "[DeviceManager] Autosave configured with {} second interval to {}",
        intervalSeconds, persistenceDirectory);
  } else {
    spdlog::info("[DeviceManager] Autosave disabled");
  }
}

/**
 * @brief Create a backup of current device configuration.
 */
bool DeviceManager::backupConfiguration(const std::string &backupDir) {
  std::string targetDir =
      backupDir.empty() ? persistenceDirectory + "/backups" : backupDir;

  if (!ensureDirectoryExists(targetDir)) {
    spdlog::error("[DeviceManager] Failed to create backup directory: {}",
                  targetDir);
    return false;
  }

  std::string backupFile = targetDir + "/" + generateTimestampedFilename();

  bool success = saveDeviceConfiguration(backupFile);
  if (success) {
    spdlog::info("[DeviceManager] Configuration backup created at {}",
                 backupFile);
  }

  return success;
}

/**
 * @brief Restore device configuration from a backup file.
 */
bool DeviceManager::restoreFromBackup(const std::string &backupFilePath) {
  if (!fs::exists(backupFilePath)) {
    spdlog::error("[DeviceManager] Backup file not found: {}", backupFilePath);
    return false;
  }

  // Create a backup of current state before restoring
  std::string emergencyBackupPath =
      persistenceDirectory + "/pre_restore_" + generateTimestampedFilename();
  saveDeviceConfiguration(emergencyBackupPath);

  bool success = loadDeviceConfiguration(backupFilePath);
  if (success) {
    spdlog::info("[DeviceManager] Configuration restored from {}",
                 backupFilePath);
  } else {
    spdlog::error("[DeviceManager] Failed to restore from backup: {}",
                  backupFilePath);
  }

  return success;
}

/**
 * @brief Get the current persistence directory.
 */
std::string DeviceManager::getPersistenceDirectory() const {
  return persistenceDirectory;
}

/**
 * @brief Background thread function for autosaving configurations.
 */
void DeviceManager::autosaveWorker() {
  spdlog::info("[DeviceManager] Autosave worker thread started");

  while (!shutdownRequested) {
    for (int i = 0; i < autosaveIntervalSeconds && !shutdownRequested; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!shutdownRequested) {
      std::string configPath = persistenceDirectory + "/devices.json";
      try {
        saveDeviceConfiguration(configPath);
      } catch (const std::exception &e) {
        spdlog::error("[DeviceManager] Error in autosave: {}", e.what());
      }
    }
  }

  spdlog::info("[DeviceManager] Autosave worker thread stopped");
}

/**
 * @brief Generate a timestamp-based filename for backups.
 */
std::string DeviceManager::generateTimestampedFilename() const {
  auto now = std::chrono::system_clock::now();
  auto now_time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << "devices_";
  ss << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S");
  ss << ".json";

  return ss.str();
}

/**
 * @brief Ensure directory exists, create if it doesn't.
 */
bool DeviceManager::ensureDirectoryExists(const std::string &dirPath) const {
  try {
    if (dirPath.empty())
      return false;

    fs::path path(dirPath);
    if (!fs::exists(path)) {
      spdlog::info("[DeviceManager] Creating directory: {}", dirPath);
      return fs::create_directories(path);
    }
    return true;
  } catch (const std::exception &e) {
    spdlog::error("[DeviceManager] Error creating directory {}: {}", dirPath,
                  e.what());
    return false;
  }
}

void DeviceManager::enableDistributedMode(bool enabled, uint16_t discoveryPort,
                                 const std::string &multicastGroup) {
  // 如果状态没有改变，则不做任何事情
  if (distributedModeEnabled.load() == enabled) {
    return;
  }
  
  distributedModeEnabled.store(enabled);
  this->discoveryPort = discoveryPort;
  this->multicastGroup = multicastGroup;
  
  if (enabled) {
    // 如果未设置服务器ID，则生成一个随机ID
    if (serverId.empty()) {
      serverId = "server_" + generateUuid().substr(0, 8);
      spdlog::info("[DeviceManager] Generated server ID: {}", serverId);
    }
    
    // 启动设备发现服务
    startDiscoveryService();
    spdlog::info("[DeviceManager] Distributed mode enabled on port {} with group {}", 
                  discoveryPort, multicastGroup);
  } else {
    // 停止设备发现服务
    stopDiscoveryService();
    spdlog::info("[DeviceManager] Distributed mode disabled");
  }
}

void DeviceManager::setServerId(const std::string &id) {
  serverId = id;
  spdlog::info("[DeviceManager] Server ID set to: {}", serverId);
}

json DeviceManager::getRemoteDevices() const {
  std::lock_guard<std::mutex> lock(remoteDevicesMutex);
  
  json result = json::array();
  
  for (const auto &[serverId, deviceList] : remoteDevices) {
    for (auto it = deviceList.begin(); it != deviceList.end(); ++it) {
      json deviceInfo = *it;
      deviceInfo["remoteServerId"] = serverId;
      result.push_back(deviceInfo);
    }
  }
  
  return result;
}

json DeviceManager::forwardCommandToRemoteDevice(const std::string &deviceId, 
                                               const CommandMessage &command) {
  // 在远程设备列表中查找设备
  std::string targetServerId;
  {
    std::lock_guard<std::mutex> lock(remoteDevicesMutex);
    
    for (const auto &[serverId, deviceList] : remoteDevices) {
      for (auto it = deviceList.begin(); it != deviceList.end(); ++it) {
        if (it->contains("deviceId") && (*it)["deviceId"] == deviceId) {
          targetServerId = serverId;
          break;
        }
      }
      if (!targetServerId.empty()) break;
    }
  }
  
  if (targetServerId.empty()) {
    spdlog::error("[DeviceManager] Remote device not found: {}", deviceId);
    return json{{"status", "ERROR"}, {"message", "Remote device not found"}};
  }
  
  try {
    // 在实际实现中，这里需要通过网络将命令转发到目标服务器
    // 这里仅作为示例，实际网络通信需要根据具体情况实现
    spdlog::info("[DeviceManager] Forwarding command to server {} for device {}: {}", 
                 targetServerId, deviceId, command.getCommand());
                 
    // 注：实际实现需要与目标服务器建立连接并发送消息
    // 此处仅返回模拟结果
    return json{
      {"status", "SUCCESS"},
      {"message", "Command forwarded (simulated)"},
      {"targetServerId", targetServerId},
      {"deviceId", deviceId},
      {"command", command.getCommand()}
    };
  } catch (const std::exception &e) {
    spdlog::error("[DeviceManager] Error forwarding command: {}", e.what());
    return json{{"status", "ERROR"}, {"message", e.what()}};
  }
}

void DeviceManager::setHealthCheckParams(int checkIntervalSeconds, 
                                int timeoutSeconds,
                                int maxRetries) {
  healthCheckIntervalSeconds.store(checkIntervalSeconds);
  healthCheckTimeoutSeconds.store(timeoutSeconds);
  healthCheckMaxRetries.store(maxRetries);
  
  spdlog::info("[DeviceManager] Health check parameters updated: interval={}s, timeout={}s, maxRetries={}", 
              checkIntervalSeconds, timeoutSeconds, maxRetries);
  
  // 如果健康检查线程正在运行，无需重启
  if (!healthCheckRunning.load()) {
    startHealthCheckService();
  }
}

json DeviceManager::getDeviceHealthInfo(const std::string &deviceId) const {
  std::lock_guard<std::mutex> lock(healthInfoMutex);
  
  if (deviceId.empty()) {
    // 返回所有设备的健康信息
    json result = json::array();
    for (const auto &[id, info] : deviceHealthInfo) {
      json deviceHealth = info;
      deviceHealth["deviceId"] = id;
      result.push_back(deviceHealth);
    }
    return result;
  } else {
    // 返回特定设备的健康信息
    auto it = deviceHealthInfo.find(deviceId);
    if (it == deviceHealthInfo.end()) {
      return json{
        {"deviceId", deviceId},
        {"status", "unknown"},
        {"message", "No health information available"},
        {"lastChecked", 0}
      };
    }
    
    json result = it->second;
    result["deviceId"] = deviceId;
    return result;
  }
}

json DeviceManager::triggerHealthCheck(const std::string &deviceId) {
  json results = json::array();
  
  if (deviceId.empty()) {
    // 对所有设备执行健康检查
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    for (const auto &[id, deviceInfo] : devices) {
      bool healthy = checkDeviceHealth(id, healthCheckMaxRetries.load());
      
      json result = {
        {"deviceId", id},
        {"status", healthy ? "healthy" : "unhealthy"},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()}
      };
      
      results.push_back(result);
    }
  } else {
    // 对特定设备执行健康检查
    bool healthy = checkDeviceHealth(deviceId, healthCheckMaxRetries.load());
    
    json result = {
      {"deviceId", deviceId},
      {"status", healthy ? "healthy" : "unhealthy"},
      {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    results.push_back(result);
  }
  
  return results;
}

void DeviceManager::startHealthCheckService() {
  if (healthCheckRunning.load()) {
    return; // 已经在运行
  }
  
  healthCheckRunning.store(true);
  healthCheckThread = std::thread(&DeviceManager::healthCheckWorker, this);
  
  spdlog::info("[DeviceManager] Health check service started with interval {}s", 
               healthCheckIntervalSeconds.load());
}

void DeviceManager::stopHealthCheckService() {
  if (!healthCheckRunning.load()) {
    return; // 已经停止
  }
  
  healthCheckRunning.store(false);
  
  if (healthCheckThread.joinable()) {
    healthCheckThread.join();
  }
  
  spdlog::info("[DeviceManager] Health check service stopped");
}

void DeviceManager::healthCheckWorker() {
  spdlog::info("[DeviceManager] Health check worker started");
  
  while (healthCheckRunning.load() && !shutdownRequested.load()) {
    // 对所有设备执行健康检查
    {
      std::lock_guard<std::mutex> lock(devicesMutex);
      
      for (const auto &[deviceId, deviceInfo] : devices) {
        try {
          // 执行健康检查
          bool healthy = checkDeviceHealth(deviceId, 1); // 在工作线程中只尝试一次
          
          // 更新状态
          if (!healthy) {
            spdlog::warn("[DeviceManager] Device {} health check failed", deviceId);
          }
        } catch (const std::exception &e) {
          spdlog::error("[DeviceManager] Exception during health check for device {}: {}", 
                      deviceId, e.what());
        }
      }
    }
    
    // 等待下一次检查
    for (int i = 0; i < healthCheckIntervalSeconds.load() && 
                    healthCheckRunning.load() && 
                    !shutdownRequested.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  
  spdlog::info("[DeviceManager] Health check worker stopped");
}

bool DeviceManager::checkDeviceHealth(const std::string &deviceId, int retries) {
  // 检查设备是否存在
  if (!deviceExists(deviceId)) {
    updateDeviceHealthStatus(deviceId, false, "Device not found");
    return false;
  }
  
  // 实际实现中应该发送ping或健康检查命令给设备
  // 这里简单地检查设备最后活动时间
  try {
    std::lock_guard<std::mutex> lock(devicesMutex);
    auto it = devices.find(deviceId);
    
    if (it == devices.end()) {
      updateDeviceHealthStatus(deviceId, false, "Device not found");
      return false;
    }
    
    if (!it->second.contains("_metadata") || 
        !it->second["_metadata"].contains("lastSeen")) {
      updateDeviceHealthStatus(deviceId, false, "Missing metadata");
      return false;
    }
    
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t lastSeen = it->second["_metadata"]["lastSeen"];
    int64_t elapsed = now - lastSeen;
    
    bool healthy = (elapsed <= healthCheckTimeoutSeconds.load() * 1000);
    
    updateDeviceHealthStatus(
      deviceId, 
      healthy, 
      healthy ? "Device active" : "Device inactive for " + std::to_string(elapsed/1000) + "s"
    );
    
    return healthy;
  } catch (const std::exception &e) {
    updateDeviceHealthStatus(deviceId, false, std::string("Error during health check: ") + e.what());
    return false;
  }
}

void DeviceManager::updateDeviceHealthStatus(const std::string &deviceId, 
                                           bool healthy, 
                                           const std::string &message) {
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
  
  std::lock_guard<std::mutex> lock(healthInfoMutex);
  
  deviceHealthInfo[deviceId] = {
    {"status", healthy ? "healthy" : "unhealthy"},
    {"message", message},
    {"lastChecked", now}
  };
  
  // 如果不健康，增加检查失败计数
  if (!healthy) {
    if (!deviceHealthInfo[deviceId].contains("failureCount")) {
      deviceHealthInfo[deviceId]["failureCount"] = 1;
    } else {
      deviceHealthInfo[deviceId]["failureCount"] = 
        deviceHealthInfo[deviceId]["failureCount"].get<int>() + 1;
    }
  } else {
    // 健康状态下重置失败计数
    deviceHealthInfo[deviceId]["failureCount"] = 0;
  }
}

json DeviceManager::getDeviceTopology() const {
  json topology = {
    {"nodes", json::array()},
    {"links", json::array()}
  };
  
  // 添加设备节点信息
  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    for (const auto &[id, deviceInfo] : devices) {
      json node = {
        {"id", id},
        {"type", deviceInfo.contains("deviceType") ? deviceInfo["deviceType"].get<std::string>() : "unknown"},
        {"name", deviceInfo.contains("deviceName") ? deviceInfo["deviceName"].get<std::string>() : id}
      };
      
      if (deviceInfo.contains("_metadata") && deviceInfo["_metadata"].contains("status")) {
        node["status"] = deviceInfo["_metadata"]["status"];
      } else {
        node["status"] = "unknown";
      }
      
      topology["nodes"].push_back(node);
    }
  }
  
  // 添加远程设备节点信息
  {
    std::lock_guard<std::mutex> lock(remoteDevicesMutex);
    
    for (const auto &[serverId, deviceList] : remoteDevices) {
      for (auto it = deviceList.begin(); it != deviceList.end(); ++it) {
        json deviceInfo = *it;
        std::string id = deviceInfo.contains("deviceId") ? 
                         deviceInfo["deviceId"].get<std::string>() : "unknown";
        
        json node = {
          {"id", id},
          {"type", deviceInfo.contains("deviceType") ? deviceInfo["deviceType"].get<std::string>() : "unknown"},
          {"name", deviceInfo.contains("deviceName") ? deviceInfo["deviceName"].get<std::string>() : id},
          {"remoteServerId", serverId},
          {"isRemote", true}
        };
        
        topology["nodes"].push_back(node);
      }
    }
  }
  
  // 添加依赖关系边
  {
    std::lock_guard<std::mutex> lock(dependencyMutex);
    
    for (const auto &[sourceId, dependencies] : deviceDependencies) {
      for (const auto &[targetId, dependencyType] : dependencies) {
        json link = {
          {"source", sourceId},
          {"target", targetId},
          {"type", dependencyType}
        };
        
        topology["links"].push_back(link);
      }
    }
  }
  
  return topology;
}

void DeviceManager::setDeviceDependency(const std::string &dependentDeviceId,
                                      const std::string &dependencyDeviceId,
                                      const std::string &dependencyType) {
  // 检查设备是否存在
  if (!deviceExists(dependentDeviceId)) {
    throw std::runtime_error("Dependent device not found: " + dependentDeviceId);
  }
  
  // 检查依赖设备是否存在（本地或远程）
  bool deviceFound = deviceExists(dependencyDeviceId);
  
  // 如果在本地没找到，检查远程设备
  if (!deviceFound) {
    std::lock_guard<std::mutex> lock(remoteDevicesMutex);
    
    for (const auto &[serverId, deviceList] : remoteDevices) {
      for (auto it = deviceList.begin(); it != deviceList.end(); ++it) {
        if (it->contains("deviceId") && (*it)["deviceId"] == dependencyDeviceId) {
          deviceFound = true;
          break;
        }
      }
      if (deviceFound) break;
    }
  }
  
  if (!deviceFound) {
    throw std::runtime_error("Dependency device not found: " + dependencyDeviceId);
  }
  
  // 检查是否形成循环依赖
  if (checkDependencyCycle(dependentDeviceId, dependencyDeviceId)) {
    throw std::runtime_error("Circular dependency detected between " + 
                            dependentDeviceId + " and " + dependencyDeviceId);
  }
  
  // 设置依赖关系
  {
    std::lock_guard<std::mutex> lock(dependencyMutex);
    deviceDependencies[dependentDeviceId][dependencyDeviceId] = dependencyType;
  }
  
  spdlog::info("[DeviceManager] Set dependency: {} -> {} ({})", 
              dependentDeviceId, dependencyDeviceId, dependencyType);
}

void DeviceManager::removeDeviceDependency(const std::string &dependentDeviceId,
                                         const std::string &dependencyDeviceId) {
  std::lock_guard<std::mutex> lock(dependencyMutex);
  
  auto it = deviceDependencies.find(dependentDeviceId);
  if (it != deviceDependencies.end()) {
    auto& dependencies = it->second;
    auto depIt = dependencies.find(dependencyDeviceId);
    
    if (depIt != dependencies.end()) {
      dependencies.erase(depIt);
      spdlog::info("[DeviceManager] Removed dependency: {} -> {}", 
                  dependentDeviceId, dependencyDeviceId);
      
      // 如果该设备没有其他依赖关系，则移除整个条目
      if (dependencies.empty()) {
        deviceDependencies.erase(it);
      }
    }
  }
}

bool DeviceManager::checkDependencyCycle(const std::string &startDeviceId, 
                                       const std::string &newDependencyId) const {
  std::lock_guard<std::mutex> lock(dependencyMutex);
  
  // 检查新的依赖是否依赖于起始设备，形成循环
  if (startDeviceId == newDependencyId) {
    return true;  // 直接自我依赖
  }
  
  // 使用DFS检查是否形成循环
  std::set<std::string> visited;
  std::function<bool(const std::string&)> hasCycle;
  
  hasCycle = [&](const std::string& currentId) -> bool {
    if (currentId == startDeviceId) {
      return true;  // 找到了循环
    }
    
    if (visited.find(currentId) != visited.end()) {
      return false;  // 已访问过，但没有形成循环
    }
    
    visited.insert(currentId);
    
    // 检查当前节点的所有依赖
    auto it = deviceDependencies.find(currentId);
    if (it != deviceDependencies.end()) {
      for (const auto& [depId, _] : it->second) {
        if (hasCycle(depId)) {
          return true;
        }
      }
    }
    
    return false;
  };
  
  // 从新依赖开始检查是否能回到起始设备
  return hasCycle(newDependencyId);
}

void DeviceManager::validateDependencies(const std::string &deviceId) {
  std::lock_guard<std::mutex> lock(dependencyMutex);
  
  // 找到该设备的所有依赖
  auto it = deviceDependencies.find(deviceId);
  if (it == deviceDependencies.end()) {
    return;  // 没有依赖关系
  }
  
  std::vector<std::string> invalidDependencies;
  
  for (const auto& [depId, depType] : it->second) {
    // 检查依赖设备是否存在
    bool exists = deviceExists(depId);
    
    // 检查远程设备
    if (!exists) {
      std::lock_guard<std::mutex> remoteLock(remoteDevicesMutex);
      
      for (const auto &[serverId, deviceList] : remoteDevices) {
        for (auto deviceIt = deviceList.begin(); deviceIt != deviceList.end(); ++deviceIt) {
          if (deviceIt->contains("deviceId") && (*deviceIt)["deviceId"] == depId) {
            exists = true;
            break;
          }
        }
        if (exists) break;
      }
    }
    
    if (!exists) {
      invalidDependencies.push_back(depId);
    }
  }
  
  // 移除无效的依赖
  for (const auto& depId : invalidDependencies) {
    it->second.erase(depId);
    spdlog::warn("[DeviceManager] Removed invalid dependency: {} -> {} (device not found)", 
                deviceId, depId);
  }
  
  // 如果所有依赖都无效，移除整个条目
  if (it->second.empty()) {
    deviceDependencies.erase(it);
  }
}

void DeviceManager::startDiscoveryService() {
  if (discoveryRunning.load()) {
    return; // 已经在运行
  }
  
  discoveryRunning.store(true);
  discoveryThread = std::thread(&DeviceManager::discoveryWorker, this);
  
  spdlog::info("[DeviceManager] Device discovery service started on port {} with group {}", 
               discoveryPort, multicastGroup);
}

void DeviceManager::stopDiscoveryService() {
  if (!discoveryRunning.load()) {
    return; // 已经停止
  }
  
  discoveryRunning.store(false);
  
  if (discoveryThread.joinable()) {
    discoveryThread.join();
  }
  
  spdlog::info("[DeviceManager] Device discovery service stopped");
}

void DeviceManager::discoveryWorker() {
  spdlog::info("[DeviceManager] Discovery worker thread started");
  
  try {
    // 创建套接字和多播组
    // 注意：这里为了示例简化，没有实现实际的网络通信代码
    // 在实际实现中，需要使用UDP套接字进行多播通信
    
    // 主循环
    while (discoveryRunning.load() && !shutdownRequested.load()) {
      try {
        // 广播本地设备信息
        broadcastDeviceInfo();
        
        // 接收远程设备信息
        receiveRemoteDeviceInfo();
        
        // 等待一段时间再下一次发现
        for (int i = 0; i < 10 && discoveryRunning.load() && !shutdownRequested.load(); ++i) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }
      } catch (const std::exception &e) {
        spdlog::error("[DeviceManager] Error in discovery cycle: {}", e.what());
        std::this_thread::sleep_for(std::chrono::seconds(5));
      }
    }
  } catch (const std::exception &e) {
    spdlog::error("[DeviceManager] Fatal error in discovery worker: {}", e.what());
  }
  
  spdlog::info("[DeviceManager] Discovery worker thread stopped");
}

void DeviceManager::broadcastDeviceInfo() {
  // 在实际实现中，这里会将本地设备信息编码为JSON
  // 然后通过UDP多播发送给网络上的其他服务器
  
  json broadcastData = {
    {"messageType", "DEVICE_INFO_BROADCAST"},
    {"serverId", serverId},
    {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch()).count()},
    {"devices", json::array()}
  };
  
  // 添加本地设备信息
  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    for (const auto &[id, deviceInfo] : devices) {
      // 创建一个不包含内部元数据的设备信息副本
      json deviceCopy = deviceInfo;
      if (deviceCopy.contains("_metadata")) {
        // 只保留状态信息
        json status = deviceCopy["_metadata"]["status"];
        deviceCopy.erase("_metadata");
        deviceCopy["status"] = status;
      }
      
      deviceCopy["deviceId"] = id;
      broadcastData["devices"].push_back(deviceCopy);
    }
  }
  
  // 模拟发送广播消息
  spdlog::debug("[DeviceManager] Broadcasting device info: {} devices", 
                broadcastData["devices"].size());
                
  // 注意：在实际实现中，这里会通过套接字发送broadcastData.dump()
}

void DeviceManager::receiveRemoteDeviceInfo() {
  // 在实际实现中，这里会接收来自网络的远程设备广播
  // 并解析为JSON数据
  
  // 为模拟目的，这里生成一些测试数据
  // 注意：这仅用于示例，实际实现应该通过网络接收数据
  if (serverId == "server_test123") {
    // 生成一些模拟的远程设备数据，仅用于测试
    std::string remoteServerId = "server_remote1";
    
    json remoteDevices = json::array();
    json device1 = {
      {"deviceId", "remote_telescope_1"},
      {"deviceType", "telescope"},
      {"deviceName", "Remote Telescope 1"},
      {"status", "online"}
    };
    
    json device2 = {
      {"deviceId", "remote_camera_1"},
      {"deviceType", "camera"},
      {"deviceName", "Remote CCD Camera"},
      {"status", "online"}
    };
    
    remoteDevices.push_back(device1);
    remoteDevices.push_back(device2);
    
    // 更新远程设备列表
    std::lock_guard<std::mutex> lock(remoteDevicesMutex);
    this->remoteDevices[remoteServerId] = remoteDevices;
    
    spdlog::debug("[DeviceManager] Updated remote devices for server {}: {} devices", 
                  remoteServerId, remoteDevices.size());
  }
  
  // 注意：在实际实现中，这里会从套接字读取数据，解析JSON，并更新remoteDevices映射
}

} // namespace astrocomm
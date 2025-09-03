#include "client/device_manager.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace hydrogen {

DeviceManager::DeviceManager(MessageProcessor* messageProcessor)
    : messageProcessor(messageProcessor), devices(json::object()),
      discoveryRequests(0), propertyRequests(0), propertyUpdates(0),
      cacheHits(0), cacheMisses(0) {
  if (!messageProcessor) {
    throw std::invalid_argument("MessageProcessor cannot be null");
  }
  spdlog::debug("DeviceManager initialized");
}

DeviceManager::~DeviceManager() {
  spdlog::debug("DeviceManager destroyed");
}

json DeviceManager::discoverDevices(const std::vector<std::string>& deviceTypes) {
  if (!messageProcessor) {
    throw std::runtime_error("MessageProcessor is null");
  }

  // Create discovery request message
  DiscoveryRequestMessage msg;
  msg.setDeviceTypes(deviceTypes);

  // Send request and wait for response
  json response = messageProcessor->sendAndWaitForResponse(msg);

  // Process and cache the response
  processDiscoveryResponse(response);

  updateStats(1, 0, 0, 0, 0);
  spdlog::info("Device discovery completed. Found {} devices", devices.size());

  return getDevices();
}

json DeviceManager::getDevices() const {
  std::lock_guard<std::mutex> lock(devicesMutex);
  updateStats(0, 0, 0, 1, 0);
  return devices;
}

json DeviceManager::getDeviceProperties(const std::string& deviceId,
                                       const std::vector<std::string>& properties) {
  if (!isValidDeviceId(deviceId)) {
    throw std::invalid_argument("Invalid device ID: " + deviceId);
  }

  if (!hasDevice(deviceId)) {
    updateStats(0, 1, 0, 0, 1);
    throw std::runtime_error("Device not found: " + deviceId);
  }

  // Create command message
  CommandMessage msg("GET_PROPERTY");
  msg.setDeviceId(deviceId);

  // Build property list
  json props = json::array();
  for (const auto& prop : properties) {
    props.push_back(prop);
  }
  msg.setProperties(props);

  // Send request and wait for response
  json response = messageProcessor->sendAndWaitForResponse(msg);

  updateStats(0, 1, 0, 0, 0);
  spdlog::debug("Retrieved {} properties from device {}", properties.size(), deviceId);

  return response;
}

json DeviceManager::setDeviceProperties(const std::string& deviceId, const json& properties) {
  if (!isValidDeviceId(deviceId)) {
    throw std::invalid_argument("Invalid device ID: " + deviceId);
  }

  if (!hasDevice(deviceId)) {
    updateStats(0, 0, 1, 0, 1);
    throw std::runtime_error("Device not found: " + deviceId);
  }

  if (!properties.is_object()) {
    throw std::invalid_argument("Properties must be a JSON object");
  }

  // Create command message
  CommandMessage msg("SET_PROPERTY");
  msg.setDeviceId(deviceId);
  msg.setProperties(properties);

  // Send request and wait for response
  json response = messageProcessor->sendAndWaitForResponse(msg);

  updateStats(0, 0, 1, 0, 0);
  spdlog::debug("Set {} properties on device {}", properties.size(), deviceId);

  return response;
}

json DeviceManager::getDeviceInfo(const std::string& deviceId) const {
  if (!isValidDeviceId(deviceId)) {
    return json::object();
  }

  std::lock_guard<std::mutex> lock(devicesMutex);
  
  if (devices.is_object() && devices.contains(deviceId)) {
    updateStats(0, 0, 0, 1, 0);
    return devices[deviceId];
  }

  updateStats(0, 0, 0, 0, 1);
  return json::object();
}

bool DeviceManager::hasDevice(const std::string& deviceId) const {
  if (!isValidDeviceId(deviceId)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(devicesMutex);
  bool found = devices.is_object() && devices.contains(deviceId);
  
  if (found) {
    updateStats(0, 0, 0, 1, 0);
  } else {
    updateStats(0, 0, 0, 0, 1);
  }
  
  return found;
}

std::vector<std::string> DeviceManager::getDeviceIds() const {
  std::lock_guard<std::mutex> lock(devicesMutex);
  std::vector<std::string> ids;

  if (devices.is_object()) {
    for (auto it = devices.begin(); it != devices.end(); ++it) {
      ids.push_back(it.key());
    }
  }

  updateStats(0, 0, 0, 1, 0);
  return ids;
}

json DeviceManager::getDevicesByType(const std::string& deviceType) const {
  std::lock_guard<std::mutex> lock(devicesMutex);
  json filteredDevices = json::object();

  if (devices.is_object()) {
    for (auto it = devices.begin(); it != devices.end(); ++it) {
      const json& deviceInfo = it.value();
      if (deviceInfo.is_object() && deviceInfo.contains("type") &&
          deviceInfo["type"] == deviceType) {
        filteredDevices[it.key()] = deviceInfo;
      }
    }
  }

  updateStats(0, 0, 0, 1, 0);
  spdlog::debug("Found {} devices of type '{}'", filteredDevices.size(), deviceType);
  
  return filteredDevices;
}

void DeviceManager::clearDeviceCache() {
  std::lock_guard<std::mutex> lock(devicesMutex);
  size_t previousSize = devices.is_object() ? devices.size() : 0;
  devices = json::object();
  spdlog::info("Cleared device cache. Removed {} devices", previousSize);
}

json DeviceManager::getDeviceStats() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  
  json stats;
  stats["discoveryRequests"] = discoveryRequests;
  stats["propertyRequests"] = propertyRequests;
  stats["propertyUpdates"] = propertyUpdates;
  stats["cacheHits"] = cacheHits;
  stats["cacheMisses"] = cacheMisses;
  
  {
    std::lock_guard<std::mutex> deviceLock(devicesMutex);
    stats["cachedDevices"] = devices.is_object() ? devices.size() : 0;
  }
  
  return stats;
}

void DeviceManager::updateDeviceInfo(const std::string& deviceId, const json& deviceInfo) {
  if (!isValidDeviceId(deviceId)) {
    spdlog::warn("Attempted to update device with invalid ID: {}", deviceId);
    return;
  }

  if (!deviceInfo.is_object()) {
    spdlog::warn("Attempted to update device {} with non-object info", deviceId);
    return;
  }

  // Validate device info has required fields
  if (!deviceInfo.contains("id") || !deviceInfo.contains("type")) {
    spdlog::warn("Attempted to update device {} with missing required fields (id, type)", deviceId);
    return;
  }

  // Validate that the ID in the device info matches the provided ID (if present)
  if (deviceInfo.contains("id") && !deviceInfo["id"].is_string()) {
    spdlog::warn("Attempted to update device {} with non-string ID field", deviceId);
    return;
  }

  if (deviceInfo.contains("id") && deviceInfo["id"].get<std::string>().empty()) {
    spdlog::warn("Attempted to update device {} with empty ID field", deviceId);
    return;
  }

  // Validate type field
  if (!deviceInfo["type"].is_string() || deviceInfo["type"].get<std::string>().empty()) {
    spdlog::warn("Attempted to update device {} with invalid type field", deviceId);
    return;
  }

  std::lock_guard<std::mutex> lock(devicesMutex);
  devices[deviceId] = deviceInfo;
  spdlog::debug("Updated device info for: {}", deviceId);
}

void DeviceManager::removeDevice(const std::string& deviceId) {
  if (!isValidDeviceId(deviceId)) {
    spdlog::warn("Attempted to remove device with invalid ID: {}", deviceId);
    return;
  }

  std::lock_guard<std::mutex> lock(devicesMutex);
  auto it = devices.find(deviceId);
  if (it != devices.end()) {
    devices.erase(it);
    spdlog::info("Removed device from cache: {}", deviceId);
  } else {
    spdlog::warn("Attempted to remove non-existent device: {}", deviceId);
  }
}

void DeviceManager::updateStats(size_t discoveries, size_t propRequests,
                               size_t propUpdates, size_t hits, size_t misses) const {
  std::lock_guard<std::mutex> lock(statsMutex);
  // Use mutable members to allow modification in const methods
  discoveryRequests += discoveries;
  propertyRequests += propRequests;
  propertyUpdates += propUpdates;
  cacheHits += hits;
  cacheMisses += misses;
}

bool DeviceManager::isValidDeviceId(const std::string& deviceId) const {
  // Basic validation: non-empty, reasonable length, no special characters
  if (deviceId.empty() || deviceId.length() > 256) {
    return false;
  }

  // Check for valid characters (alphanumeric, underscore, hyphen, dot)
  return std::all_of(deviceId.begin(), deviceId.end(), [](char c) {
    return std::isalnum(c) || c == '_' || c == '-' || c == '.';
  });
}

void DeviceManager::processDiscoveryResponse(const json& response) {
  if (!response.is_object()) {
    spdlog::warn("Discovery response is not a JSON object");
    return;
  }

  json newDevices = json::object();

  // Extract devices from response payload
  if (response.contains("payload") && response["payload"].is_object() &&
      response["payload"].contains("devices")) {
    const json& deviceList = response["payload"]["devices"];
    
    if (deviceList.is_object()) {
      newDevices = deviceList;
    } else if (deviceList.is_array()) {
      // Convert array format to object format
      for (const auto& device : deviceList) {
        if (device.is_object() && device.contains("id")) {
          std::string deviceId = device["id"];
          newDevices[deviceId] = device;
        }
      }
    }
  }

  // Update the device cache
  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    devices = newDevices;
    spdlog::debug("Updated local device cache with {} devices", devices.size());
  }
}

} // namespace hydrogen

#include "server/device_manager.h"
#include "common/logger.h"
#include <fstream>
#include <stdexcept>

namespace astrocomm {

DeviceManager::DeviceManager() {
    logInfo("Device manager initialized", "DeviceManager");
}

DeviceManager::~DeviceManager() {
    logInfo("Device manager shutting down", "DeviceManager");
}

void DeviceManager::addDevice(const std::string& deviceId, const json& deviceInfo) {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    if (devices.find(deviceId) != devices.end()) {
        throw std::runtime_error("Device already exists: " + deviceId);
    }
    
    devices[deviceId] = deviceInfo;
    logInfo("Device added: " + deviceId, "DeviceManager");
}

void DeviceManager::updateDevice(const std::string& deviceId, const json& deviceInfo) {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    if (devices.find(deviceId) == devices.end()) {
        throw std::runtime_error("Device not found: " + deviceId);
    }
    
    devices[deviceId] = deviceInfo;
    logInfo("Device updated: " + deviceId, "DeviceManager");
}

void DeviceManager::removeDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    auto it = devices.find(deviceId);
    if (it != devices.end()) {
        devices.erase(it);
        logInfo("Device removed: " + deviceId, "DeviceManager");
    } else {
        logWarning("Attempted to remove non-existent device: " + deviceId, "DeviceManager");
    }
}

json DeviceManager::getDeviceInfo(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        throw std::runtime_error("Device not found: " + deviceId);
    }
    
    return it->second;
}

json DeviceManager::getDevices(const std::vector<std::string>& deviceTypes) const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    json result = json::array();
    
    for (const auto& entry : devices) {
        // If no device types are specified, include all devices
        if (deviceTypes.empty()) {
            result.push_back(entry.second);
            continue;
        }
        
        // Otherwise, filter by device type
        if (entry.second.contains("deviceType")) {
            std::string deviceType = entry.second["deviceType"];
            
            for (const auto& type : deviceTypes) {
                if (deviceType == type) {
                    result.push_back(entry.second);
                    break;
                }
            }
        }
    }
    
    return result;
}

bool DeviceManager::deviceExists(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    return devices.find(deviceId) != devices.end();
}

json DeviceManager::getDeviceProperty(const std::string& deviceId, const std::string& property) const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    auto deviceIt = devices.find(deviceId);
    if (deviceIt == devices.end()) {
        throw std::runtime_error("Device not found: " + deviceId);
    }
    
    // Check if device info has properties object
    if (deviceIt->second.contains("properties") && 
        deviceIt->second["properties"].is_object() && 
        deviceIt->second["properties"].contains(property)) {
        
        return deviceIt->second["properties"][property];
    }
    
    throw std::runtime_error("Property not found: " + property + " for device: " + deviceId);
}

void DeviceManager::updateDeviceProperty(const std::string& deviceId, const std::string& property, const json& value) {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    auto deviceIt = devices.find(deviceId);
    if (deviceIt == devices.end()) {
        throw std::runtime_error("Device not found: " + deviceId);
    }
    
    // Ensure the properties object exists
    if (!deviceIt->second.contains("properties") || !deviceIt->second["properties"].is_object()) {
        deviceIt->second["properties"] = json::object();
    }
    
    deviceIt->second["properties"][property] = value;
    logInfo("Updated property: " + property + " for device: " + deviceId, "DeviceManager");
}

bool DeviceManager::saveDeviceConfiguration(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            logError("Failed to open file for writing: " + filePath, "DeviceManager");
            return false;
        }
        
        json config;
        for (const auto& entry : devices) {
            config[entry.first] = entry.second;
        }
        
        file << config.dump(2);
        file.close();
        
        logInfo("Device configuration saved to " + filePath, "DeviceManager");
        return true;
    }
    catch (const std::exception& e) {
        logError("Error saving device configuration: " + std::string(e.what()), "DeviceManager");
        return false;
    }
}

bool DeviceManager::loadDeviceConfiguration(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            logWarning("Failed to open configuration file: " + filePath, "DeviceManager");
            return false;
        }
        
        json config;
        file >> config;
        file.close();
        
        std::lock_guard<std::mutex> lock(devicesMutex);
        devices.clear();
        
        for (auto it = config.begin(); it != config.end(); ++it) {
            devices[it.key()] = it.value();
        }
        
        logInfo("Loaded device configuration from " + filePath + 
                " (" + std::to_string(devices.size()) + " devices)", "DeviceManager");
        return true;
    }
    catch (const std::exception& e) {
        logError("Error loading device configuration: " + std::string(e.what()), "DeviceManager");
        return false;
    }
}

} // namespace astrocomm
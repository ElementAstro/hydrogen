/**
 * @file device_registry.cpp
 * @brief Implementation of DeviceRegistry class
 */

#include "device_registry.h"
#include "camera.h"
#include "telescope.h"
#include "focuser.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>

namespace hydrogen {
namespace device {

// Singleton instance
DeviceRegistry& DeviceRegistry::getInstance() {
    static DeviceRegistry instance;
    return instance;
}

DeviceRegistry::DeviceRegistry() {
    SPDLOG_DEBUG("DeviceRegistry created");
}

DeviceRegistry::~DeviceRegistry() {
    stopAllDevices();
    disconnectAllDevices();
    SPDLOG_DEBUG("DeviceRegistry destroyed");
}

bool DeviceRegistry::registerDeviceFactory(const std::string& deviceType, 
                                          std::unique_ptr<core::DeviceFactory> factory) {
    std::lock_guard<std::mutex> lock(factoriesMutex_);
    deviceFactories_[deviceType] = std::move(factory);
    SPDLOG_DEBUG("Registered device factory for type: {}", deviceType);
    return true;
}

bool DeviceRegistry::unregisterDeviceFactory(const std::string& deviceType) {
    std::lock_guard<std::mutex> lock(factoriesMutex_);
    auto it = deviceFactories_.find(deviceType);
    if (it != deviceFactories_.end()) {
        deviceFactories_.erase(it);
        SPDLOG_DEBUG("Unregistered device factory for type: {}", deviceType);
        return true;
    }
    return false;
}

std::unique_ptr<core::ModernDeviceBase> DeviceRegistry::createDevice(const std::string& deviceType,
                                                                    const std::string& deviceId,
                                                                    const std::string& manufacturer,
                                                                    const std::string& model) {
    std::lock_guard<std::mutex> lock(factoriesMutex_);
    
    // Simple factory implementation for basic device types
    if (deviceType == "Camera") {
        return std::make_unique<Camera>(deviceId, manufacturer.empty() ? "ZWO" : manufacturer, 
                                       model.empty() ? "ASI2600MC" : model);
    } else if (deviceType == "Telescope") {
        return std::make_unique<Telescope>(deviceId, manufacturer.empty() ? "Celestron" : manufacturer, 
                                          model.empty() ? "EdgeHD" : model);
    } else if (deviceType == "Focuser") {
        return std::make_unique<Focuser>(deviceId, manufacturer.empty() ? "ZWO" : manufacturer, 
                                        model.empty() ? "EAF" : model);
    }
    
    // Try using registered factories
    auto it = deviceFactories_.find(deviceType);
    if (it != deviceFactories_.end()) {
        // Note: This would need proper factory interface implementation
        SPDLOG_DEBUG("Using factory for device type: {}", deviceType);
        // return it->second->createDevice(deviceId, manufacturer, model);
    }
    
    SPDLOG_WARN("No factory found for device type: {}", deviceType);
    return nullptr;
}

std::vector<std::string> DeviceRegistry::getSupportedDeviceTypes() const {
    std::lock_guard<std::mutex> lock(factoriesMutex_);
    
    std::vector<std::string> types = {"Camera", "Telescope", "Focuser"};
    
    // Add registered factory types
    for (const auto& [type, factory] : deviceFactories_) {
        types.push_back(type);
    }
    
    return types;
}

std::vector<std::string> DeviceRegistry::getSupportedManufacturers(const std::string& deviceType) const {
    // Simple implementation - return common manufacturers
    if (deviceType == "Camera") {
        return {"ZWO", "QHY", "SBIG", "Atik"};
    } else if (deviceType == "Telescope") {
        return {"Celestron", "Meade", "Orion", "Sky-Watcher"};
    } else if (deviceType == "Focuser") {
        return {"ZWO", "Pegasus", "Lakeside", "MoonLite"};
    }
    return {};
}

std::vector<std::string> DeviceRegistry::getSupportedModels(const std::string& deviceType, 
                                                           const std::string& manufacturer) const {
    // Simple implementation - return common models
    if (deviceType == "Camera" && manufacturer == "ZWO") {
        return {"ASI2600MC", "ASI294MC", "ASI183MC", "ASI533MC"};
    } else if (deviceType == "Telescope" && manufacturer == "Celestron") {
        return {"EdgeHD", "Schmidt-Cassegrain", "Refractor"};
    } else if (deviceType == "Focuser" && manufacturer == "ZWO") {
        return {"EAF", "EAF-S"};
    }
    return {};
}

bool DeviceRegistry::registerDeviceInstance(std::shared_ptr<core::ModernDeviceBase> device) {
    if (!device) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(instancesMutex_);
    deviceInstances_[device->getDeviceId()] = device;
    SPDLOG_DEBUG("Registered device instance: {}", device->getDeviceId());
    return true;
}

bool DeviceRegistry::unregisterDeviceInstance(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    auto it = deviceInstances_.find(deviceId);
    if (it != deviceInstances_.end()) {
        deviceInstances_.erase(it);
        SPDLOG_DEBUG("Unregistered device instance: {}", deviceId);
        return true;
    }
    return false;
}

std::shared_ptr<core::ModernDeviceBase> DeviceRegistry::getDeviceInstance(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    auto it = deviceInstances_.find(deviceId);
    if (it != deviceInstances_.end()) {
        return it->second;
    }
    return nullptr;
}

std::unordered_map<std::string, std::shared_ptr<core::ModernDeviceBase>> DeviceRegistry::getAllDeviceInstances() const {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    return deviceInstances_;
}

std::vector<std::shared_ptr<core::ModernDeviceBase>> DeviceRegistry::getDeviceInstancesByType(const std::string& deviceType) const {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    std::vector<std::shared_ptr<core::ModernDeviceBase>> result;
    
    for (const auto& [id, device] : deviceInstances_) {
        if (device && device->getDeviceType() == deviceType) {
            result.push_back(device);
        }
    }
    
    return result;
}

size_t DeviceRegistry::initializeAllDevices() {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    size_t count = 0;

    for (const auto& [id, device] : deviceInstances_) {
        if (device && device->initialize()) {
            count++;
        }
    }

    SPDLOG_INFO("Initialized {} devices", count);
    return count;
}

size_t DeviceRegistry::startAllDevices() {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    size_t count = 0;

    for (const auto& [id, device] : deviceInstances_) {
        if (device && device->start()) {
            count++;
        }
    }

    SPDLOG_INFO("Started {} devices", count);
    return count;
}

void DeviceRegistry::stopAllDevices() {
    std::lock_guard<std::mutex> lock(instancesMutex_);

    for (const auto& [id, device] : deviceInstances_) {
        if (device) {
            device->stop();
        }
    }

    SPDLOG_INFO("Stopped all devices");
}

void DeviceRegistry::disconnectAllDevices() {
    std::lock_guard<std::mutex> lock(instancesMutex_);

    for (const auto& [id, device] : deviceInstances_) {
        if (device) {
            device->disconnect();
        }
    }

    SPDLOG_INFO("Disconnected all devices");
}

json DeviceRegistry::getDeviceStatistics() const {
    std::lock_guard<std::mutex> lock(instancesMutex_);
    
    json stats;
    stats["totalDevices"] = deviceInstances_.size();
    stats["deviceTypes"] = getSupportedDeviceTypes().size();
    
    return stats;
}

bool DeviceRegistry::exportDeviceConfigurations(const std::string& filename) const {
    // Stub implementation
    SPDLOG_DEBUG("Export device configurations to: {}", filename);
    return true;
}

bool DeviceRegistry::importDeviceConfigurations(const std::string& filename) {
    // Stub implementation
    SPDLOG_DEBUG("Import device configurations from: {}", filename);
    return true;
}

void DeviceRegistry::registerDefaultFactories() {
    SPDLOG_DEBUG("Registering default device factories");
    // Default factories are handled in createDevice method for now
}

} // namespace device
} // namespace hydrogen

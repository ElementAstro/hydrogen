#include <astrocomm/server/device_manager.h>
#include <astrocomm/core/utils.h>
#include <filesystem>
#include <fstream>

namespace astrocomm {
namespace server {

DeviceManager::DeviceManager() 
    : persistenceDirectory("./data/devices"), 
      autosaveEnabled(false), 
      autosaveIntervalSeconds(300),
      shutdownRequested(false) {
}

DeviceManager::DeviceManager(const std::string& persistenceDir, int autosaveInterval)
    : persistenceDirectory(persistenceDir),
      autosaveEnabled(true),
      autosaveIntervalSeconds(autosaveInterval),
      shutdownRequested(false) {
    
    // Ensure persistence directory exists
    std::filesystem::create_directories(persistenceDirectory);
    
    // Start autosave thread if enabled
    if (autosaveEnabled.load()) {
        startAutosaveThread();
    }
}

DeviceManager::~DeviceManager() {
    stopAutosaveThread();
    saveConfiguration();
}

bool DeviceManager::registerDevice(const std::string& deviceId, const json& deviceInfo) {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    // Check if device already exists
    if (devices.find(deviceId) != devices.end()) {
        return false;
    }
    
    // Add device with default properties
    json device = deviceInfo;
    device["id"] = deviceId;
    device["connected"] = false;
    device["registrationTime"] = astrocomm::core::getIsoTimestamp();
    device["lastSeen"] = astrocomm::core::getIsoTimestamp();
    
    devices[deviceId] = device;
    
    // Notify callback if set
    if (propertyChangeCallback) {
        propertyChangeCallback(deviceId, device);
    }
    
    return true;
}

bool DeviceManager::unregisterDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        return false;
    }
    
    devices.erase(it);
    
    return true;
}

bool DeviceManager::updateDeviceProperties(const std::string& deviceId, const json& properties) {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        return false;
    }
    
    // Update properties
    for (auto& [key, value] : properties.items()) {
        it->second[key] = value;
    }
    
    // Update last seen timestamp
    it->second["lastSeen"] = astrocomm::core::getIsoTimestamp();
    
    // Notify callback if set
    if (propertyChangeCallback) {
        propertyChangeCallback(deviceId, properties);
    }
    
    return true;
}

json DeviceManager::getDeviceInfo(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        return json();
    }
    
    return it->second;
}

json DeviceManager::getAllDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    json result = json::object();
    for (const auto& [deviceId, deviceInfo] : devices) {
        result[deviceId] = deviceInfo;
    }
    
    return result;
}

bool DeviceManager::isDeviceRegistered(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    return devices.find(deviceId) != devices.end();
}

bool DeviceManager::setDeviceConnectionStatus(const std::string& deviceId, bool connected) {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        return false;
    }
    
    bool wasConnected = it->second.value("connected", false);
    it->second["connected"] = connected;
    it->second["lastSeen"] = astrocomm::core::getIsoTimestamp();
    
    if (connected) {
        it->second["lastConnected"] = astrocomm::core::getIsoTimestamp();
    } else {
        it->second["lastDisconnected"] = astrocomm::core::getIsoTimestamp();
    }
    
    // Notify callback if connection status changed
    if (connectionChangeCallback && wasConnected != connected) {
        connectionChangeCallback(deviceId, connected);
    }
    
    return true;
}

bool DeviceManager::isDeviceConnected(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        return false;
    }
    
    return it->second.value("connected", false);
}

std::vector<std::string> DeviceManager::getConnectedDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    std::vector<std::string> connectedDevices;
    for (const auto& [deviceId, deviceInfo] : devices) {
        if (deviceInfo.value("connected", false)) {
            connectedDevices.push_back(deviceId);
        }
    }
    
    return connectedDevices;
}

std::vector<std::string> DeviceManager::getDevicesByType(const std::string& deviceType) const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    std::vector<std::string> matchingDevices;
    for (const auto& [deviceId, deviceInfo] : devices) {
        if (deviceInfo.value("type", "") == deviceType) {
            matchingDevices.push_back(deviceId);
        }
    }
    
    return matchingDevices;
}

bool DeviceManager::saveConfiguration() {
    try {
        std::filesystem::create_directories(persistenceDirectory);
        
        std::string configFile = persistenceDirectory + "/devices.json";
        std::ofstream file(configFile);
        
        json config;
        {
            std::lock_guard<std::mutex> lock(devicesMutex);
            config["devices"] = devices;
        }
        config["timestamp"] = astrocomm::core::getIsoTimestamp();
        
        file << config.dump(4);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool DeviceManager::loadConfiguration() {
    try {
        std::string configFile = persistenceDirectory + "/devices.json";
        
        if (!std::filesystem::exists(configFile)) {
            return false;
        }
        
        std::ifstream file(configFile);
        json config;
        file >> config;
        
        if (config.contains("devices")) {
            std::lock_guard<std::mutex> lock(devicesMutex);
            devices.clear();
            
            for (const auto& [deviceId, deviceInfo] : config["devices"].items()) {
                devices[deviceId] = deviceInfo;
                // Mark all loaded devices as disconnected initially
                devices[deviceId]["connected"] = false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void DeviceManager::setAutosaveEnabled(bool enabled) {
    bool wasEnabled = autosaveEnabled.exchange(enabled);
    
    if (enabled && !wasEnabled) {
        startAutosaveThread();
    } else if (!enabled && wasEnabled) {
        stopAutosaveThread();
    }
}

void DeviceManager::setAutosaveInterval(int intervalSeconds) {
    autosaveIntervalSeconds.store(intervalSeconds);
}

json DeviceManager::getDeviceStatistics() const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    int totalDevices = devices.size();
    int connectedDevices = 0;
    std::map<std::string, int> deviceTypeCount;
    
    for (const auto& [deviceId, deviceInfo] : devices) {
        if (deviceInfo.value("connected", false)) {
            connectedDevices++;
        }
        
        std::string deviceType = deviceInfo.value("type", "unknown");
        deviceTypeCount[deviceType]++;
    }
    
    return json{
        {"totalDevices", totalDevices},
        {"connectedDevices", connectedDevices},
        {"disconnectedDevices", totalDevices - connectedDevices},
        {"deviceTypeCount", deviceTypeCount}
    };
}

void DeviceManager::setPropertyChangeCallback(
    std::function<void(const std::string&, const json&)> callback) {
    propertyChangeCallback = callback;
}

void DeviceManager::setConnectionChangeCallback(
    std::function<void(const std::string&, bool)> callback) {
    connectionChangeCallback = callback;
}

void DeviceManager::enableDistributedMode(bool enabled, const std::string& serverId,
                                         uint16_t discoveryPort, const std::string& multicastGroup) {
    distributedModeEnabled.store(enabled);
    this->serverId = serverId;
    this->discoveryPort = discoveryPort;
    this->multicastGroup = multicastGroup;
    
    if (enabled) {
        startDiscoveryThread();
    } else {
        stopDiscoveryThread();
    }
}

json DeviceManager::getRemoteDevices() const {
    std::lock_guard<std::mutex> lock(remoteDevicesMutex);
    return json(remoteDevices);
}

void DeviceManager::broadcastDeviceDiscovery() {
    // Implementation would send UDP multicast message
    // For now, this is a placeholder
}

void DeviceManager::handleRemoteDeviceDiscovery(const std::string& serverId, const json& devices) {
    std::lock_guard<std::mutex> lock(remoteDevicesMutex);
    remoteDevices[serverId] = devices;
}

// Private methods
void DeviceManager::startAutosaveThread() {
    if (autosaveThread.joinable()) {
        return; // Already running
    }
    
    shutdownRequested.store(false);
    autosaveThread = std::thread(&DeviceManager::autosaveThreadFunction, this);
}

void DeviceManager::stopAutosaveThread() {
    shutdownRequested.store(true);
    
    if (autosaveThread.joinable()) {
        autosaveThread.join();
    }
}

void DeviceManager::autosaveThreadFunction() {
    while (!shutdownRequested.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(autosaveIntervalSeconds.load()));
        
        if (!shutdownRequested.load()) {
            saveConfiguration();
        }
    }
}

void DeviceManager::startDiscoveryThread() {
    if (discoveryRunning.load()) {
        return;
    }
    
    discoveryRunning.store(true);
    discoveryThread = std::thread(&DeviceManager::discoveryThreadFunction, this);
}

void DeviceManager::stopDiscoveryThread() {
    discoveryRunning.store(false);
    
    if (discoveryThread.joinable()) {
        discoveryThread.join();
    }
}

void DeviceManager::discoveryThreadFunction() {
    while (discoveryRunning.load()) {
        // Broadcast discovery message periodically
        broadcastDeviceDiscovery();
        
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

void DeviceManager::notifyPropertyChange(const std::string& deviceId, const json& properties) {
    if (propertyChangeCallback) {
        propertyChangeCallback(deviceId, properties);
    }
}

void DeviceManager::notifyConnectionChange(const std::string& deviceId, bool connected) {
    if (connectionChangeCallback) {
        connectionChangeCallback(deviceId, connected);
    }
}

} // namespace server
} // namespace astrocomm

#include "astrocomm/core/enhanced_discovery.h"
#include "astrocomm/core/utils.h"
#include <algorithm>
#include <fstream>
#include <thread>
#include <chrono>

namespace astrocomm {
namespace core {

// DeviceCapability implementation
json DeviceCapability::toJson() const {
    return json{
        {"name", name},
        {"description", description},
        {"parameters", parameters},
        {"isRequired", isRequired}
    };
}

DeviceCapability DeviceCapability::fromJson(const json& j) {
    DeviceCapability capability;
    capability.name = j.value("name", "");
    capability.description = j.value("description", "");
    capability.parameters = j.value("parameters", json::object());
    capability.isRequired = j.value("isRequired", false);
    return capability;
}

// DiscoveredDevice implementation
json DiscoveredDevice::toJson() const {
    json capabilitiesArray = json::array();
    for (const auto& capability : capabilities) {
        capabilitiesArray.push_back(capability.toJson());
    }
    
    return json{
        {"deviceId", deviceId},
        {"deviceType", deviceType},
        {"name", name},
        {"manufacturer", manufacturer},
        {"model", model},
        {"serialNumber", serialNumber},
        {"firmwareVersion", firmwareVersion},
        {"discoveryMethod", discoveryMethodToString(discoveryMethod)},
        {"connectionString", connectionString},
        {"capabilities", capabilitiesArray},
        {"configuration", configuration},
        {"metadata", metadata},
        {"discoveryTime", getIsoTimestamp()},
        {"isConfigured", isConfigured},
        {"isConnectable", isConnectable}
    };
}

DiscoveredDevice DiscoveredDevice::fromJson(const json& j) {
    DiscoveredDevice device;
    device.deviceId = j.value("deviceId", "");
    device.deviceType = j.value("deviceType", "");
    device.name = j.value("name", "");
    device.manufacturer = j.value("manufacturer", "");
    device.model = j.value("model", "");
    device.serialNumber = j.value("serialNumber", "");
    device.firmwareVersion = j.value("firmwareVersion", "");
    device.discoveryMethod = stringToDiscoveryMethod(j.value("discoveryMethod", "MANUAL"));
    device.connectionString = j.value("connectionString", "");
    device.configuration = j.value("configuration", json::object());
    device.metadata = j.value("metadata", json::object());
    device.isConfigured = j.value("isConfigured", false);
    device.isConnectable = j.value("isConnectable", false);
    
    if (j.contains("capabilities") && j["capabilities"].is_array()) {
        for (const auto& capJson : j["capabilities"]) {
            device.capabilities.push_back(DeviceCapability::fromJson(capJson));
        }
    }
    
    if (j.contains("discoveryTime")) {
        device.discoveryTime = string_utils::parseIsoTimestamp(j["discoveryTime"]);
    }
    
    return device;
}

// ConfigurationTemplate implementation
json ConfigurationTemplate::toJson() const {
    return json{
        {"deviceType", deviceType},
        {"manufacturer", manufacturer},
        {"model", model},
        {"defaultConfiguration", defaultConfiguration},
        {"requiredParameters", requiredParameters},
        {"optionalParameters", optionalParameters},
        {"validationRules", validationRules}
    };
}

ConfigurationTemplate ConfigurationTemplate::fromJson(const json& j) {
    ConfigurationTemplate configTemplate;
    configTemplate.deviceType = j.value("deviceType", "");
    configTemplate.manufacturer = j.value("manufacturer", "");
    configTemplate.model = j.value("model", "");
    configTemplate.defaultConfiguration = j.value("defaultConfiguration", json::object());
    configTemplate.requiredParameters = j.value("requiredParameters", std::vector<std::string>{});
    configTemplate.optionalParameters = j.value("optionalParameters", std::vector<std::string>{});
    configTemplate.validationRules = j.value("validationRules", json::object());
    return configTemplate;
}

// DiscoveryFilter implementation
json DiscoveryFilter::toJson() const {
    json methodsArray = json::array();
    for (const auto& method : methods) {
        methodsArray.push_back(discoveryMethodToString(method));
    }
    
    return json{
        {"deviceTypes", deviceTypes},
        {"manufacturers", manufacturers},
        {"methods", methodsArray},
        {"includeConfigured", includeConfigured},
        {"includeUnconfigured", includeUnconfigured}
    };
}

DiscoveryFilter DiscoveryFilter::fromJson(const json& j) {
    DiscoveryFilter filter;
    filter.deviceTypes = j.value("deviceTypes", std::vector<std::string>{});
    filter.manufacturers = j.value("manufacturers", std::vector<std::string>{});
    filter.includeConfigured = j.value("includeConfigured", true);
    filter.includeUnconfigured = j.value("includeUnconfigured", true);
    
    if (j.contains("methods") && j["methods"].is_array()) {
        for (const auto& methodStr : j["methods"]) {
            filter.methods.push_back(stringToDiscoveryMethod(methodStr.get<std::string>()));
        }
    }
    
    return filter;
}

// EnhancedDeviceDiscovery implementation
EnhancedDeviceDiscovery::EnhancedDeviceDiscovery() {
    initializeBuiltinHandlers();
}

EnhancedDeviceDiscovery::~EnhancedDeviceDiscovery() {
    stopDiscovery();
}

bool EnhancedDeviceDiscovery::startDiscovery(const DiscoveryFilter& filter) {
    if (discoveryRunning_) {
        return false;
    }
    
    currentFilter_ = filter;
    discoveryRunning_ = true;
    
    discoveryThread_ = std::thread(&EnhancedDeviceDiscovery::discoveryThreadFunction, this);
    
    return true;
}

void EnhancedDeviceDiscovery::stopDiscovery() {
    if (!discoveryRunning_) {
        return;
    }
    
    discoveryRunning_ = false;
    
    if (discoveryThread_.joinable()) {
        discoveryThread_.join();
    }
}

std::vector<DiscoveredDevice> EnhancedDeviceDiscovery::getDiscoveredDevices(const DiscoveryFilter& filter) const {
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    
    std::vector<DiscoveredDevice> devices;
    for (const auto& [deviceId, deviceInfo] : discoveredDevices_) {
        if (deviceInfo.isActive && matchesFilter(deviceInfo.device, filter)) {
            devices.push_back(deviceInfo.device);
        }
    }
    
    return devices;
}

bool EnhancedDeviceDiscovery::autoConfigureDevice(const std::string& deviceId) {
    std::shared_lock<std::shared_mutex> devicesLock(devicesMutex_);
    
    auto deviceIt = discoveredDevices_.find(deviceId);
    if (deviceIt == discoveredDevices_.end()) {
        return false;
    }
    
    const auto& device = deviceIt->second.device;
    devicesLock.unlock();
    
    // Find configuration template
    auto configTemplate = getConfigurationTemplate(device.deviceType, device.manufacturer, device.model);
    if (!configTemplate) {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.autoConfigFailures++;
        return false;
    }
    
    // Apply configuration template
    json autoConfig = configTemplate->defaultConfiguration;
    
    // Merge with device-specific metadata
    for (const auto& [key, value] : device.metadata.items()) {
        if (autoConfig.contains(key)) {
            autoConfig[key] = value;
        }
    }
    
    // Update device configuration
    std::lock_guard<std::shared_mutex> devicesWriteLock(devicesMutex_);
    auto& deviceInfo = discoveredDevices_[deviceId];
    deviceInfo.device.configuration = autoConfig;
    deviceInfo.device.isConfigured = true;
    
    std::lock_guard<std::mutex> statsLock(statsMutex_);
    stats_.autoConfigSuccesses++;
    
    return true;
}

void EnhancedDeviceDiscovery::registerConfigurationTemplate(const ConfigurationTemplate& configTemplate) {
    std::lock_guard<std::shared_mutex> lock(templatesMutex_);
    
    std::string key = generateTemplateKey(configTemplate.deviceType, 
                                         configTemplate.manufacturer, 
                                         configTemplate.model);
    configurationTemplates_[key] = configTemplate;
}

std::optional<ConfigurationTemplate> EnhancedDeviceDiscovery::getConfigurationTemplate(
    const std::string& deviceType,
    const std::string& manufacturer,
    const std::string& model) const {
    
    std::shared_lock<std::shared_mutex> lock(templatesMutex_);
    
    // Try exact match first
    std::string exactKey = generateTemplateKey(deviceType, manufacturer, model);
    auto it = configurationTemplates_.find(exactKey);
    if (it != configurationTemplates_.end()) {
        return it->second;
    }
    
    // Try manufacturer + device type match
    std::string manufacturerKey = generateTemplateKey(deviceType, manufacturer, "");
    it = configurationTemplates_.find(manufacturerKey);
    if (it != configurationTemplates_.end()) {
        return it->second;
    }
    
    // Try device type only match
    std::string typeKey = generateTemplateKey(deviceType, "", "");
    it = configurationTemplates_.find(typeKey);
    if (it != configurationTemplates_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void EnhancedDeviceDiscovery::registerDiscoveryHandler(DiscoveryMethod method,
    std::function<std::vector<DiscoveredDevice>()> handler) {
    
    std::lock_guard<std::shared_mutex> lock(handlersMutex_);
    discoveryHandlers_[method] = handler;
}

void EnhancedDeviceDiscovery::setDeviceFoundCallback(std::function<void(const DiscoveredDevice&)> callback) {
    deviceFoundCallback_ = callback;
}

void EnhancedDeviceDiscovery::setDeviceLostCallback(std::function<void(const std::string&)> callback) {
    deviceLostCallback_ = callback;
}

int EnhancedDeviceDiscovery::refreshDiscovery() {
    performDiscoveryScan();
    
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    return std::count_if(discoveredDevices_.begin(), discoveredDevices_.end(),
                        [](const auto& pair) { return pair.second.isActive; });
}

bool EnhancedDeviceDiscovery::isDiscoveryRunning() const {
    return discoveryRunning_;
}

json EnhancedDeviceDiscovery::getDiscoveryStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    json methodCounts = json::object();
    for (const auto& [method, count] : stats_.methodCounts) {
        methodCounts[discoveryMethodToString(method)] = count;
    }
    
    return json{
        {"totalScans", stats_.totalScans},
        {"devicesFound", stats_.devicesFound},
        {"devicesLost", stats_.devicesLost},
        {"autoConfigSuccesses", stats_.autoConfigSuccesses},
        {"autoConfigFailures", stats_.autoConfigFailures},
        {"methodCounts", methodCounts},
        {"deviceTypeCounts", stats_.deviceTypeCounts},
        {"discoveryRunning", discoveryRunning_.load()},
        {"continuousDiscovery", continuousDiscovery_.load()}
    };
}

EnhancedDeviceDiscovery& EnhancedDeviceDiscovery::getInstance() {
    static EnhancedDeviceDiscovery instance;
    return instance;
}

void EnhancedDeviceDiscovery::setDiscoveryInterval(std::chrono::seconds interval) {
    discoveryInterval_ = interval;
}

void EnhancedDeviceDiscovery::setContinuousDiscovery(bool enabled) {
    continuousDiscovery_ = enabled;
}

void EnhancedDeviceDiscovery::setDeviceTimeout(std::chrono::seconds timeout) {
    deviceTimeout_ = timeout;
}

// Helper function implementations
std::string discoveryMethodToString(DiscoveryMethod method) {
    switch (method) {
        case DiscoveryMethod::NETWORK_SCAN: return "NETWORK_SCAN";
        case DiscoveryMethod::USB_SCAN: return "USB_SCAN";
        case DiscoveryMethod::SERIAL_SCAN: return "SERIAL_SCAN";
        case DiscoveryMethod::BLUETOOTH_SCAN: return "BLUETOOTH_SCAN";
        case DiscoveryMethod::ZEROCONF: return "ZEROCONF";
        case DiscoveryMethod::UPNP: return "UPNP";
        case DiscoveryMethod::MANUAL: return "MANUAL";
        case DiscoveryMethod::PLUGIN_SPECIFIC: return "PLUGIN_SPECIFIC";
        default: return "UNKNOWN";
    }
}

DiscoveryMethod stringToDiscoveryMethod(const std::string& method) {
    if (method == "NETWORK_SCAN") return DiscoveryMethod::NETWORK_SCAN;
    if (method == "USB_SCAN") return DiscoveryMethod::USB_SCAN;
    if (method == "SERIAL_SCAN") return DiscoveryMethod::SERIAL_SCAN;
    if (method == "BLUETOOTH_SCAN") return DiscoveryMethod::BLUETOOTH_SCAN;
    if (method == "ZEROCONF") return DiscoveryMethod::ZEROCONF;
    if (method == "UPNP") return DiscoveryMethod::UPNP;
    if (method == "MANUAL") return DiscoveryMethod::MANUAL;
    if (method == "PLUGIN_SPECIFIC") return DiscoveryMethod::PLUGIN_SPECIFIC;
    return DiscoveryMethod::MANUAL;
}

void EnhancedDeviceDiscovery::discoveryThreadFunction() {
    while (discoveryRunning_) {
        performDiscoveryScan();
        checkForLostDevices();
        
        if (!continuousDiscovery_) {
            break;
        }
        
        std::this_thread::sleep_for(discoveryInterval_);
    }
}

void EnhancedDeviceDiscovery::performDiscoveryScan() {
    std::shared_lock<std::shared_mutex> handlersLock(handlersMutex_);
    
    std::vector<DiscoveredDevice> allDiscovered;
    
    // Execute all registered discovery handlers
    for (const auto& [method, handler] : discoveryHandlers_) {
        // Check if this method is allowed by current filter
        if (!currentFilter_.methods.empty() && 
            std::find(currentFilter_.methods.begin(), currentFilter_.methods.end(), method) == currentFilter_.methods.end()) {
            continue;
        }
        
        try {
            auto devices = handler();
            allDiscovered.insert(allDiscovered.end(), devices.begin(), devices.end());
            
            std::lock_guard<std::mutex> statsLock(statsMutex_);
            stats_.methodCounts[method] += devices.size();
        } catch (const std::exception&) {
            // Log error but continue with other methods
        }
    }
    
    handlersLock.unlock();
    
    // Update discovered devices
    for (const auto& device : allDiscovered) {
        updateDeviceInfo(device);
    }
    
    std::lock_guard<std::mutex> statsLock(statsMutex_);
    stats_.totalScans++;
}

void EnhancedDeviceDiscovery::checkForLostDevices() {
    std::lock_guard<std::shared_mutex> lock(devicesMutex_);
    
    auto now = std::chrono::system_clock::now();
    auto it = discoveredDevices_.begin();
    
    while (it != discoveredDevices_.end()) {
        if (it->second.isActive && (now - it->second.lastSeen) > deviceTimeout_) {
            it->second.isActive = false;
            notifyDeviceLost(it->first);
            
            std::lock_guard<std::mutex> statsLock(statsMutex_);
            stats_.devicesLost++;
        }
        ++it;
    }
}

void EnhancedDeviceDiscovery::updateDeviceInfo(const DiscoveredDevice& device) {
    std::lock_guard<std::shared_mutex> lock(devicesMutex_);
    
    auto it = discoveredDevices_.find(device.deviceId);
    bool isNewDevice = (it == discoveredDevices_.end());
    
    if (isNewDevice) {
        DiscoveredDeviceInfo info;
        info.device = device;
        info.lastSeen = std::chrono::system_clock::now();
        info.isActive = true;
        
        discoveredDevices_[device.deviceId] = info;
        
        lock.unlock();
        notifyDeviceFound(device);
        updateStatistics(device, true);
    } else {
        // Update existing device info
        it->second.device = device;
        it->second.lastSeen = std::chrono::system_clock::now();
        if (!it->second.isActive) {
            it->second.isActive = true;
            lock.unlock();
            notifyDeviceFound(device);
        }
    }
}

bool EnhancedDeviceDiscovery::matchesFilter(const DiscoveredDevice& device, const DiscoveryFilter& filter) const {
    // Check device type filter
    if (!filter.deviceTypes.empty() && 
        std::find(filter.deviceTypes.begin(), filter.deviceTypes.end(), device.deviceType) == filter.deviceTypes.end()) {
        return false;
    }
    
    // Check manufacturer filter
    if (!filter.manufacturers.empty() && 
        std::find(filter.manufacturers.begin(), filter.manufacturers.end(), device.manufacturer) == filter.manufacturers.end()) {
        return false;
    }
    
    // Check discovery method filter
    if (!filter.methods.empty() && 
        std::find(filter.methods.begin(), filter.methods.end(), device.discoveryMethod) == filter.methods.end()) {
        return false;
    }
    
    // Check configuration status filter
    if (!filter.includeConfigured && device.isConfigured) {
        return false;
    }
    if (!filter.includeUnconfigured && !device.isConfigured) {
        return false;
    }
    
    return true;
}

std::string EnhancedDeviceDiscovery::generateTemplateKey(const std::string& deviceType, 
                                                        const std::string& manufacturer, 
                                                        const std::string& model) const {
    return deviceType + "::" + manufacturer + "::" + model;
}

void EnhancedDeviceDiscovery::initializeBuiltinHandlers() {
    // Register built-in discovery handlers
    registerDiscoveryHandler(DiscoveryMethod::NETWORK_SCAN, 
        [this]() { return performNetworkScan(); });
    
    registerDiscoveryHandler(DiscoveryMethod::USB_SCAN, 
        [this]() { return performUsbScan(); });
    
    registerDiscoveryHandler(DiscoveryMethod::SERIAL_SCAN, 
        [this]() { return performSerialScan(); });
}

std::vector<DiscoveredDevice> EnhancedDeviceDiscovery::performNetworkScan() {
    std::vector<DiscoveredDevice> devices;
    
    // Placeholder implementation - would implement actual network scanning
    // This would typically scan for devices on the network using various protocols
    
    return devices;
}

std::vector<DiscoveredDevice> EnhancedDeviceDiscovery::performUsbScan() {
    std::vector<DiscoveredDevice> devices;
    
    // Placeholder implementation - would implement actual USB device enumeration
    // This would scan for USB devices and identify astronomical equipment
    
    return devices;
}

std::vector<DiscoveredDevice> EnhancedDeviceDiscovery::performSerialScan() {
    std::vector<DiscoveredDevice> devices;
    
    // Placeholder implementation - would implement actual serial port scanning
    // This would scan available serial ports and probe for devices
    
    return devices;
}

void EnhancedDeviceDiscovery::notifyDeviceFound(const DiscoveredDevice& device) {
    if (deviceFoundCallback_) {
        deviceFoundCallback_(device);
    }
}

void EnhancedDeviceDiscovery::notifyDeviceLost(const std::string& deviceId) {
    if (deviceLostCallback_) {
        deviceLostCallback_(deviceId);
    }
}

void EnhancedDeviceDiscovery::updateStatistics(const DiscoveredDevice& device, bool isNew) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    if (isNew) {
        stats_.devicesFound++;
        stats_.deviceTypeCounts[device.deviceType]++;
    }
}

bool EnhancedDeviceDiscovery::loadConfigurationTemplates(const std::string& filename) {
    try {
        std::ifstream file(filename);
        json data;
        file >> data;
        
        std::lock_guard<std::shared_mutex> lock(templatesMutex_);
        configurationTemplates_.clear();
        
        if (data.contains("templates") && data["templates"].is_array()) {
            for (const auto& templateJson : data["templates"]) {
                ConfigurationTemplate configTemplate = ConfigurationTemplate::fromJson(templateJson);
                std::string key = generateTemplateKey(configTemplate.deviceType, 
                                                     configTemplate.manufacturer, 
                                                     configTemplate.model);
                configurationTemplates_[key] = configTemplate;
            }
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool EnhancedDeviceDiscovery::saveConfigurationTemplates(const std::string& filename) const {
    try {
        std::shared_lock<std::shared_mutex> lock(templatesMutex_);
        
        json data = json::object();
        json templatesArray = json::array();
        
        for (const auto& [key, configTemplate] : configurationTemplates_) {
            templatesArray.push_back(configTemplate.toJson());
        }
        
        data["templates"] = templatesArray;
        
        std::ofstream file(filename);
        file << data.dump(2);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace core
} // namespace astrocomm

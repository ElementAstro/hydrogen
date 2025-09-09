#include "hydrogen/core/infrastructure/discovery.h"
#include <algorithm>
#include <fstream>
#include <random>
#ifdef HYDROGEN_ENABLE_SPDLOG
#include <spdlog/spdlog.h>
#else
#include <hydrogen/core/logging/mock_spdlog.h>
#endif

namespace hydrogen {
namespace core {

using json = nlohmann::json;

// DeviceCapability implementation
json DeviceCapability::toJson() const {
  return json{{"name", name},
              {"description", description},
              {"parameters", parameters},
              {"isRequired", isRequired}};
}

DeviceCapability DeviceCapability::fromJson(const json &j) {
  DeviceCapability capability;
  capability.name = j.value("name", "");
  capability.description = j.value("description", "");
  capability.parameters = j.value("parameters", json{});
  capability.isRequired = j.value("isRequired", false);
  return capability;
}

// DiscoveredDevice implementation
json DiscoveredDevice::toJson() const {
  json capabilitiesJson = json::array();
  for (const auto &capability : capabilities) {
    capabilitiesJson.push_back(capability.toJson());
  }

  return json{
      {"deviceId", deviceId},
      {"deviceType", deviceType},
      {"name", name},
      {"manufacturer", manufacturer},
      {"model", model},
      {"serialNumber", serialNumber},
      {"firmwareVersion", firmwareVersion},
      {"discoveryMethod", static_cast<int>(discoveryMethod)},
      {"connectionString", connectionString},
      {"capabilities", capabilitiesJson},
      {"configuration", configuration},
      {"metadata", metadata},
      {"discoveryTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                            discoveryTime.time_since_epoch())
                            .count()},
      {"isConfigured", isConfigured},
      {"isConnectable", isConnectable}};
}

DiscoveredDevice DiscoveredDevice::fromJson(const json &j) {
  DiscoveredDevice device;
  device.deviceId = j.value("deviceId", "");
  device.deviceType = j.value("deviceType", "");
  device.name = j.value("name", "");
  device.manufacturer = j.value("manufacturer", "");
  device.model = j.value("model", "");
  device.serialNumber = j.value("serialNumber", "");
  device.firmwareVersion = j.value("firmwareVersion", "");
  device.discoveryMethod =
      static_cast<DiscoveryMethod>(j.value("discoveryMethod", 0));
  device.connectionString = j.value("connectionString", "");
  device.configuration = j.value("configuration", json{});
  device.metadata = j.value("metadata", json{});
  device.isConfigured = j.value("isConfigured", false);
  device.isConnectable = j.value("isConnectable", false);

  if (j.contains("capabilities") && j["capabilities"].is_array()) {
    for (const auto &capJson : j["capabilities"]) {
      device.capabilities.push_back(DeviceCapability::fromJson(capJson));
    }
  }

  if (j.contains("discoveryTime")) {
    auto ms = j["discoveryTime"].get<int64_t>();
    device.discoveryTime =
        std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
  }

  return device;
}

// ConfigurationTemplate implementation
json ConfigurationTemplate::toJson() const {
  return json{{"deviceType", deviceType},
              {"manufacturer", manufacturer},
              {"model", model},
              {"defaultConfiguration", defaultConfiguration},
              {"requiredParameters", requiredParameters},
              {"optionalParameters", optionalParameters},
              {"validationRules", validationRules}};
}

ConfigurationTemplate ConfigurationTemplate::fromJson(const json &j) {
  ConfigurationTemplate configTemplate;
  configTemplate.deviceType = j.value("deviceType", "");
  configTemplate.manufacturer = j.value("manufacturer", "");
  configTemplate.model = j.value("model", "");
  configTemplate.defaultConfiguration = j.value("defaultConfiguration", json{});
  configTemplate.requiredParameters =
      j.value("requiredParameters", std::vector<std::string>{});
  configTemplate.optionalParameters =
      j.value("optionalParameters", std::vector<std::string>{});
  configTemplate.validationRules = j.value("validationRules", json{});
  return configTemplate;
}

// DiscoveryFilter implementation
json DiscoveryFilter::toJson() const {
  return json{{"deviceTypes", deviceTypes},
              {"manufacturers", manufacturers},
              {"methods", methods},
              {"includeConfigured", includeConfigured},
              {"includeUnconfigured", includeUnconfigured}};
}

DiscoveryFilter DiscoveryFilter::fromJson(const json &j) {
  DiscoveryFilter filter;
  filter.deviceTypes = j.value("deviceTypes", std::vector<std::string>{});
  filter.manufacturers = j.value("manufacturers", std::vector<std::string>{});
  filter.methods = j.value("methods", std::vector<DiscoveryMethod>{});
  filter.includeConfigured = j.value("includeConfigured", true);
  filter.includeUnconfigured = j.value("includeUnconfigured", true);
  return filter;
}

// DeviceDiscovery implementation
std::unique_ptr<DeviceDiscovery> DeviceDiscovery::instance_;
std::mutex DeviceDiscovery::instanceMutex_;

DeviceDiscovery::DeviceDiscovery() {
  statistics_.discoveryStartTime = std::chrono::system_clock::now();
  // initializeBuiltinHandlers(); // TODO: Add declaration to header or remove
}

DeviceDiscovery::~DeviceDiscovery() { stopDiscovery(); }

DeviceDiscovery &DeviceDiscovery::getInstance() {
  std::lock_guard<std::mutex> lock(instanceMutex_);
  if (!instance_) {
    instance_ = std::unique_ptr<DeviceDiscovery>(new DeviceDiscovery());
  }
  return *instance_;
}

bool DeviceDiscovery::startDiscovery(const DiscoveryFilter &filter) {
  if (discoveryRunning_.load()) {
    SPDLOG_WARN("Discovery is already running");
    return true;
  }

  try {
    discoveryRunning_.store(true);

    if (continuousDiscovery_.load()) {
      discoveryThread_ = std::thread(&DeviceDiscovery::discoveryLoop, this);
    } else {
      // Perform single discovery scan
      performDiscovery();
    }

    SPDLOG_INFO("Device discovery started");
    return true;

  } catch (const std::exception &e) {
    SPDLOG_ERROR("Failed to start discovery: {}", e.what());
    discoveryRunning_.store(false);
    return false;
  }
}

void DeviceDiscovery::stopDiscovery() {
  if (!discoveryRunning_.load()) {
    return;
  }

  discoveryRunning_.store(false);

  if (discoveryThread_.joinable()) {
    discoveryThread_.join();
  }

  SPDLOG_INFO("Device discovery stopped");
}

std::vector<DiscoveredDevice>
DeviceDiscovery::getDiscoveredDevices(const DiscoveryFilter &filter) const {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  std::vector<DiscoveredDevice> result;
  for (const auto &[deviceId, device] : discoveredDevices_) {
    if (matchesFilter(device, filter)) {
      result.push_back(device);
    }
  }

  return result;
}

bool DeviceDiscovery::autoConfigureDevice(const std::string &deviceId) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto deviceIt = discoveredDevices_.find(deviceId);
  if (deviceIt == discoveredDevices_.end()) {
    SPDLOG_ERROR("Device {} not found for auto-configuration", deviceId);
    return false;
  }

  auto &device = deviceIt->second;

  // Find matching configuration template
  auto configTemplate = getConfigurationTemplate(
      device.deviceType, device.manufacturer, device.model);
  if (!configTemplate) {
    SPDLOG_WARN("No configuration template found for device {}", deviceId);
    return false;
  }

  try {
    // Apply default configuration
    device.configuration = configTemplate->defaultConfiguration;
    device.isConfigured = true;

    statistics_.autoConfigurationsAttempted++;
    statistics_.autoConfigurationsSuccessful++;

    SPDLOG_INFO("Auto-configured device {} successfully", deviceId);
    return true;

  } catch (const std::exception &e) {
    SPDLOG_ERROR("Failed to auto-configure device {}: {}", deviceId, e.what());
    statistics_.autoConfigurationsAttempted++;
    return false;
  }
}

void DeviceDiscovery::registerConfigurationTemplate(
    const ConfigurationTemplate &configTemplate) {
  std::lock_guard<std::mutex> lock(templatesMutex_);
  configurationTemplates_.push_back(configTemplate);
  SPDLOG_INFO("Registered configuration template for {}/{}/{}",
              configTemplate.deviceType, configTemplate.manufacturer,
              configTemplate.model);
}

std::optional<ConfigurationTemplate>
DeviceDiscovery::getConfigurationTemplate(const std::string &deviceType,
                                          const std::string &manufacturer,
                                          const std::string &model) const {

  std::lock_guard<std::mutex> lock(templatesMutex_);

  for (const auto &configTemplate : configurationTemplates_) {
    if (configTemplate.deviceType == deviceType &&
        configTemplate.manufacturer == manufacturer &&
        configTemplate.model == model) {
      return configTemplate;
    }
  }

  return std::nullopt;
}

void DeviceDiscovery::registerDiscoveryHandler(
    DiscoveryMethod method,
    std::function<std::vector<DiscoveredDevice>()> handler) {

  std::lock_guard<std::mutex> lock(handlersMutex_);
  discoveryHandlers_[method] = handler;
  SPDLOG_INFO("Registered discovery handler for method {}",
              static_cast<int>(method));
}

void DeviceDiscovery::setDeviceFoundCallback(
    std::function<void(const DiscoveredDevice &)> callback) {
  deviceFoundCallback_ = callback;
}

void DeviceDiscovery::setDeviceLostCallback(
    std::function<void(const std::string &)> callback) {
  deviceLostCallback_ = callback;
}

int DeviceDiscovery::refreshDiscovery() {
  performDiscovery();

  std::lock_guard<std::mutex> lock(devicesMutex_);
  return static_cast<int>(discoveredDevices_.size());
}

bool DeviceDiscovery::isDiscoveryRunning() const {
  return discoveryRunning_.load();
}

json DeviceDiscovery::getDiscoveryStatistics() const {
  std::lock_guard<std::mutex> lock(statsMutex_);

  return json{
      {"totalDevicesDiscovered", statistics_.totalDevicesDiscovered.load()},
      {"devicesCurrentlyDiscovered",
       statistics_.devicesCurrentlyDiscovered.load()},
      {"discoveryScansPerformed", statistics_.discoveryScansPerformed.load()},
      {"autoConfigurationsAttempted",
       statistics_.autoConfigurationsAttempted.load()},
      {"autoConfigurationsSuccessful",
       statistics_.autoConfigurationsSuccessful.load()},
      {"lastDiscoveryTime",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           statistics_.lastDiscoveryTime.time_since_epoch())
           .count()},
      {"discoveryStartTime",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           statistics_.discoveryStartTime.time_since_epoch())
           .count()}};
}

// Private methods
void DeviceDiscovery::discoveryLoop() {
  while (discoveryRunning_.load()) {
    performDiscovery();
    checkDeviceTimeouts();

    std::this_thread::sleep_for(discoveryInterval_);
  }
}

void DeviceDiscovery::performDiscovery() {
  std::lock_guard<std::mutex> lock(handlersMutex_);

  for (const auto &[method, handler] : discoveryHandlers_) {
    try {
      auto devices = handler();
      for (const auto &device : devices) {
        addDiscoveredDevice(device);
      }
    } catch (const std::exception &e) {
      SPDLOG_ERROR("Discovery handler for method {} failed: {}",
                   static_cast<int>(method), e.what());
    }
  }

  statistics_.discoveryScansPerformed++;
  statistics_.lastDiscoveryTime = std::chrono::system_clock::now();
}

void DeviceDiscovery::checkDeviceTimeouts() {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto now = std::chrono::system_clock::now();
  auto timeout = deviceTimeout_;

  for (auto it = discoveredDevices_.begin(); it != discoveredDevices_.end();) {
    if (now - it->second.discoveryTime > timeout) {
      if (deviceLostCallback_) {
        deviceLostCallback_(it->first);
      }
      it = discoveredDevices_.erase(it);
      statistics_.devicesCurrentlyDiscovered--;
    } else {
      ++it;
    }
  }
}

void DeviceDiscovery::addDiscoveredDevice(const DiscoveredDevice &device) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  bool isNew =
      discoveredDevices_.find(device.deviceId) == discoveredDevices_.end();

  discoveredDevices_[device.deviceId] = device;

  if (isNew) {
    statistics_.totalDevicesDiscovered++;
    statistics_.devicesCurrentlyDiscovered++;

    if (deviceFoundCallback_) {
      deviceFoundCallback_(device);
    }
  }
}

bool DeviceDiscovery::matchesFilter(const DiscoveredDevice &device,
                                    const DiscoveryFilter &filter) const {
  // Check device types
  if (!filter.deviceTypes.empty()) {
    if (std::find(filter.deviceTypes.begin(), filter.deviceTypes.end(),
                  device.deviceType) == filter.deviceTypes.end()) {
      return false;
    }
  }

  // Check manufacturers
  if (!filter.manufacturers.empty()) {
    if (std::find(filter.manufacturers.begin(), filter.manufacturers.end(),
                  device.manufacturer) == filter.manufacturers.end()) {
      return false;
    }
  }

  // Check discovery methods
  if (!filter.methods.empty()) {
    if (std::find(filter.methods.begin(), filter.methods.end(),
                  device.discoveryMethod) == filter.methods.end()) {
      return false;
    }
  }

  // Check configuration status
  if (!filter.includeConfigured && device.isConfigured) {
    return false;
  }

  if (!filter.includeUnconfigured && !device.isConfigured) {
    return false;
  }

  return true;
}

/* TODO: Add declaration to header or remove
void DeviceDiscovery::initializeBuiltinHandlers() {
  // Register built-in discovery handlers
  registerDiscoveryHandler(DiscoveryMethod::NETWORK_SCAN,
                           [this]() { return performNetworkScan(); });

  registerDiscoveryHandler(DiscoveryMethod::USB_SCAN,
                           [this]() { return performUsbScan(); });

  registerDiscoveryHandler(DiscoveryMethod::SERIAL_SCAN,
                           [this]() { return performSerialScan(); });
}

std::vector<DiscoveredDevice> DeviceDiscovery::performNetworkScan() {
  std::vector<DiscoveredDevice> devices;

  // Placeholder implementation - would implement actual network scanning
  // This would typically scan for devices on the network using various
  // protocols

  return devices;
}

std::vector<DiscoveredDevice> DeviceDiscovery::performUsbScan() {
  std::vector<DiscoveredDevice> devices;

  // Placeholder implementation - would implement actual USB device enumeration

  return devices;
}

std::vector<DiscoveredDevice> DeviceDiscovery::performSerialScan() {
  std::vector<DiscoveredDevice> devices;

  // Placeholder implementation - would implement actual serial port scanning

  return devices;
}
*/

} // namespace core
} // namespace hydrogen

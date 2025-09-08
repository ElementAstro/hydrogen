#include "hydrogen/core/device/device_interface.h"
#include "hydrogen/core/infrastructure/utils.h"
#include <algorithm>

namespace hydrogen {
namespace core {

// DeviceBase implementation
DeviceBase::DeviceBase(const std::string &deviceId,
                       const std::string &deviceType,
                       const std::string &manufacturer,
                       const std::string &model)
    : deviceId_(deviceId), deviceType_(deviceType), manufacturer_(manufacturer),
      model_(model), running_(false), connected_(false) {

  initializeProperties();
}

DeviceBase::~DeviceBase() {
  // Note: Cannot call pure virtual functions stop() and disconnect() from
  // destructor Derived classes should handle cleanup in their own destructors
  running_ = false;
  connected_ = false;
}

std::string DeviceBase::getDeviceId() const { return deviceId_; }

std::string DeviceBase::getDeviceType() const { return deviceType_; }

json DeviceBase::getDeviceInfo() const {
  std::lock_guard<std::mutex> lock(propertiesMutex_);

  json info = {{"id", deviceId_},
               {"type", deviceType_},
               {"manufacturer", manufacturer_},
               {"model", model_},
               {"firmwareVersion", firmwareVersion_},
               {"capabilities", capabilities_},
               {"running", running_},
               {"connected", connected_},
               {"timestamp", getIsoTimestamp()}};

  return info;
}

void DeviceBase::setProperty(const std::string &property, const json &value) {
  json previousValue;

  {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    auto it = properties_.find(property);
    if (it != properties_.end()) {
      previousValue = it->second;
    }
    properties_[property] = value;
  }

  // Send property changed event
  sendPropertyChangedEvent(property, value, previousValue);
}

json DeviceBase::getProperty(const std::string &property) const {
  std::lock_guard<std::mutex> lock(propertiesMutex_);
  auto it = properties_.find(property);
  if (it != properties_.end()) {
    return it->second;
  }
  return json();
}

json DeviceBase::getAllProperties() const {
  std::lock_guard<std::mutex> lock(propertiesMutex_);
  return json(properties_);
}

std::vector<std::string> DeviceBase::getCapabilities() const {
  std::lock_guard<std::mutex> lock(propertiesMutex_);
  return capabilities_;
}

bool DeviceBase::hasCapability(const std::string &capability) const {
  std::lock_guard<std::mutex> lock(propertiesMutex_);
  return std::find(capabilities_.begin(), capabilities_.end(), capability) !=
         capabilities_.end();
}

void DeviceBase::registerCommandHandler(const std::string &command,
                                        CommandHandler handler) {
  commandHandlers_[command] = handler;
}

void DeviceBase::addCapability(const std::string &capability) {
  std::lock_guard<std::mutex> lock(propertiesMutex_);
  if (std::find(capabilities_.begin(), capabilities_.end(), capability) ==
      capabilities_.end()) {
    capabilities_.push_back(capability);
  }
}

void DeviceBase::removeCapability(const std::string &capability) {
  std::lock_guard<std::mutex> lock(propertiesMutex_);
  capabilities_.erase(
      std::remove(capabilities_.begin(), capabilities_.end(), capability),
      capabilities_.end());
}

void DeviceBase::handleCommandMessage(const CommandMessage &cmd) {
  std::string command = cmd.getCommand();

  auto it = commandHandlers_.find(command);
  if (it != commandHandlers_.end()) {
    ResponseMessage response;
    response.setOriginalMessageId(cmd.getMessageId());
    response.setDeviceId(deviceId_);
    response.setCommand(command);

    try {
      it->second(cmd, response);
      if (response.getStatus().empty()) {
        response.setStatus("OK");
      }
    } catch (const std::exception &e) {
      response.setStatus("ERROR");
      response.setDetails({{"error", e.what()}});
    }

    sendResponse(response);
  } else {
    // Unknown command
    ResponseMessage response;
    response.setOriginalMessageId(cmd.getMessageId());
    response.setDeviceId(deviceId_);
    response.setCommand(command);
    response.setStatus("ERROR");
    response.setDetails({{"error", "Unknown command: " + command}});

    sendResponse(response);
  }
}

void DeviceBase::sendResponse(const ResponseMessage &response) {
  (void)response; // Suppress unused parameter warning

  // Default implementation - derived classes should override
  // to actually send the response through their communication channel
}

void DeviceBase::sendEvent(const EventMessage &event) {
  (void)event; // Suppress unused parameter warning

  // Default implementation - derived classes should override
  // to actually send the event through their communication channel
}

void DeviceBase::sendPropertyChangedEvent(const std::string &property,
                                          const json &value,
                                          const json &previousValue) {
  EventMessage event("property_changed");
  event.setDeviceId(deviceId_);
  event.setProperties({{"property", property},
                       {"value", value},
                       {"previousValue", previousValue}});

  sendEvent(event);
}

void DeviceBase::initializeProperties() {
  std::lock_guard<std::mutex> lock(propertiesMutex_);

  // Initialize common properties
  properties_["device_id"] = deviceId_;
  properties_["device_type"] = deviceType_;
  properties_["manufacturer"] = manufacturer_;
  properties_["model"] = model_;
  properties_["firmware_version"] = firmwareVersion_;
  properties_["running"] = running_;
  properties_["connected"] = connected_;

  // Add basic capabilities
  capabilities_.push_back("get_properties");
  capabilities_.push_back("set_properties");
  capabilities_.push_back("get_info");

  // Register basic command handlers
  registerCommandHandler("get_properties", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    auto params = cmd.getParameters();
    if (params.contains("properties") && params["properties"].is_array()) {
      json result = json::object();
      for (const auto &prop : params["properties"]) {
        std::string propName = prop;
        result[propName] = getProperty(propName);
      }
      response.setProperties(result);
    } else {
      response.setProperties(getAllProperties());
    }
  });

  registerCommandHandler("set_properties", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    auto params = cmd.getParameters();
    if (params.contains("properties") && params["properties"].is_object()) {
      for (const auto &[key, value] : params["properties"].items()) {
        setProperty(key, value);
      }
      response.setDetails({{"message", "Properties updated"}});
    } else {
      throw std::invalid_argument("Invalid properties parameter");
    }
  });

  registerCommandHandler(
      "get_info", [this](const CommandMessage &cmd, ResponseMessage &response) {
        (void)cmd; // Suppress unused parameter warning
        response.setDetails(getDeviceInfo());
      });
}

// DeviceRegistry implementation
DeviceRegistry &DeviceRegistry::getInstance() {
  static DeviceRegistry instance;
  return instance;
}

void DeviceRegistry::registerDeviceType(const std::string &deviceType,
                                        DeviceFactory factory) {
  std::lock_guard<std::mutex> lock(factoriesMutex_);
  factories_[deviceType] = factory;
}

std::unique_ptr<IDevice>
DeviceRegistry::createDevice(const std::string &deviceType,
                             const std::string &deviceId, const json &config) {
  std::lock_guard<std::mutex> lock(factoriesMutex_);

  auto it = factories_.find(deviceType);
  if (it != factories_.end()) {
    return it->second(deviceId, config);
  }

  return nullptr;
}

std::vector<std::string> DeviceRegistry::getRegisteredTypes() const {
  std::lock_guard<std::mutex> lock(factoriesMutex_);

  std::vector<std::string> types;
  for (const auto &[type, factory] : factories_) {
    types.push_back(type);
  }

  return types;
}

bool DeviceRegistry::isTypeRegistered(const std::string &deviceType) const {
  std::lock_guard<std::mutex> lock(factoriesMutex_);
  return factories_.find(deviceType) != factories_.end();
}

} // namespace core
} // namespace hydrogen

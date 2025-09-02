#include "client/subscription_manager.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>

namespace hydrogen {

SubscriptionManager::SubscriptionManager(MessageProcessor* messageProcessor)
    : messageProcessor(messageProcessor),
      propertySubscriptionCount(0), eventSubscriptionCount(0),
      propertyNotifications(0), eventNotifications(0), callbackErrors(0) {
  if (!messageProcessor) {
    throw std::invalid_argument("MessageProcessor cannot be null");
  }
  spdlog::debug("SubscriptionManager initialized");
}

SubscriptionManager::~SubscriptionManager() {
  clearAllSubscriptions();
  spdlog::debug("SubscriptionManager destroyed");
}

void SubscriptionManager::subscribeToProperty(const std::string& deviceId,
                                             const std::string& property,
                                             PropertyCallback callback) {
  if (!isValidDeviceId(deviceId)) {
    throw std::invalid_argument("Invalid device ID: " + deviceId);
  }

  if (!isValidPropertyName(property)) {
    throw std::invalid_argument("Invalid property name: " + property);
  }

  if (!callback) {
    throw std::invalid_argument("Callback cannot be null");
  }

  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  std::string key = makePropertyKey(deviceId, property);
  bool isNewSubscription = propertySubscriptions.find(key) == propertySubscriptions.end();
  
  propertySubscriptions[key] = std::move(callback);

  if (isNewSubscription) {
    updateStats(1, 0, 0, 0, 0);
  }

  spdlog::info("Subscribed to property '{}' for device '{}'", property, deviceId);
}

void SubscriptionManager::unsubscribeFromProperty(const std::string& deviceId,
                                                 const std::string& property) {
  if (!isValidDeviceId(deviceId)) {
    spdlog::warn("Invalid device ID for unsubscribe: {}", deviceId);
    return;
  }

  if (!isValidPropertyName(property)) {
    spdlog::warn("Invalid property name for unsubscribe: {}", property);
    return;
  }

  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  std::string key = makePropertyKey(deviceId, property);
  auto it = propertySubscriptions.find(key);

  if (it != propertySubscriptions.end()) {
    propertySubscriptions.erase(it);
    updateStats(-1, 0, 0, 0, 0);
    spdlog::info("Unsubscribed from property '{}' for device '{}'", property, deviceId);
  } else {
    spdlog::warn("Attempted to unsubscribe from non-existent property subscription: '{}' for device '{}'",
                 property, deviceId);
  }
}

void SubscriptionManager::subscribeToEvent(const std::string& deviceId,
                                          const std::string& event,
                                          EventCallback callback) {
  if (!isValidDeviceId(deviceId)) {
    throw std::invalid_argument("Invalid device ID: " + deviceId);
  }

  if (!isValidEventName(event)) {
    throw std::invalid_argument("Invalid event name: " + event);
  }

  if (!callback) {
    throw std::invalid_argument("Callback cannot be null");
  }

  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  std::string key = makeEventKey(deviceId, event);
  bool isNewSubscription = eventSubscriptions.find(key) == eventSubscriptions.end();
  
  eventSubscriptions[key] = std::move(callback);

  if (isNewSubscription) {
    updateStats(0, 1, 0, 0, 0);
  }

  spdlog::info("Subscribed to event '{}' for device '{}'", event, deviceId);
}

void SubscriptionManager::unsubscribeFromEvent(const std::string& deviceId,
                                              const std::string& event) {
  if (!isValidDeviceId(deviceId)) {
    spdlog::warn("Invalid device ID for unsubscribe: {}", deviceId);
    return;
  }

  if (!isValidEventName(event)) {
    spdlog::warn("Invalid event name for unsubscribe: {}", event);
    return;
  }

  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  std::string key = makeEventKey(deviceId, event);
  auto it = eventSubscriptions.find(key);

  if (it != eventSubscriptions.end()) {
    eventSubscriptions.erase(it);
    updateStats(0, -1, 0, 0, 0);
    spdlog::info("Unsubscribed from event '{}' for device '{}'", event, deviceId);
  } else {
    spdlog::warn("Attempted to unsubscribe from non-existent event subscription: '{}' for device '{}'",
                 event, deviceId);
  }
}

void SubscriptionManager::handlePropertyChange(const EventMessage& msg) {
  std::string deviceId = msg.getDeviceId();
  std::string eventName = msg.getEvent();

  if (eventName != "PROPERTY_CHANGED") {
    spdlog::warn("handlePropertyChange called with non-property-change event: {}", eventName);
    return;
  }

  if (!msg.getProperties().is_null() && msg.getProperties().is_object()) {
    json properties = msg.getProperties();
    std::lock_guard<std::mutex> lock(subscriptionsMutex);

    for (auto it = properties.begin(); it != properties.end(); ++it) {
      std::string propName = it.key();
      if (it.value().is_object() && it.value().contains("value")) {
        json propValue = it.value()["value"];
        std::string key = makePropertyKey(deviceId, propName);
        auto subIt = propertySubscriptions.find(key);

        if (subIt != propertySubscriptions.end()) {
          spdlog::debug("Invoking callback for property change: '{}' on device '{}'",
                        propName, deviceId);
          
          auto callbackCopy = subIt->second;
          executeCallbackSafely(callbackCopy, deviceId, propName, propValue);
          updateStats(0, 0, 1, 0, 0);
        } else {
          spdlog::trace("No subscription found for property change: '{}' on device '{}'",
                        propName, deviceId);
        }
      } else {
        spdlog::warn("Invalid property format in PROPERTY_CHANGED event for key '{}': {}",
                     propName, it.value().dump());
      }
    }
  } else {
    spdlog::warn("PROPERTY_CHANGED event received without valid properties field: {}",
                 msg.toJson().dump());
  }
}

void SubscriptionManager::handleEvent(const EventMessage& msg) {
  std::string deviceId = msg.getDeviceId();
  std::string eventName = msg.getEvent();
  json details = msg.getDetails();

  // Handle property changes separately
  if (eventName == "PROPERTY_CHANGED") {
    handlePropertyChange(msg);
    return;
  }

  // Handle other generic events
  std::lock_guard<std::mutex> lock(subscriptionsMutex);
  std::string key = makeEventKey(deviceId, eventName);
  auto it = eventSubscriptions.find(key);

  if (it != eventSubscriptions.end()) {
    spdlog::debug("Invoking callback for event: '{}' on device '{}'", eventName, deviceId);
    
    auto callbackCopy = it->second;
    executeCallbackSafely(callbackCopy, deviceId, eventName, details);
    updateStats(0, 0, 0, 1, 0);
  } else {
    spdlog::trace("No subscription found for event: '{}' on device '{}'", eventName, deviceId);
  }
}

std::vector<std::string> SubscriptionManager::getPropertySubscriptions(const std::string& deviceId) const {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);
  std::vector<std::string> properties;

  std::string prefix = deviceId + ":property:";
  for (const auto& [key, callback] : propertySubscriptions) {
    if (key.substr(0, prefix.length()) == prefix) {
      properties.push_back(key.substr(prefix.length()));
    }
  }

  return properties;
}

std::vector<std::string> SubscriptionManager::getEventSubscriptions(const std::string& deviceId) const {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);
  std::vector<std::string> events;

  std::string prefix = deviceId + ":event:";
  for (const auto& [key, callback] : eventSubscriptions) {
    if (key.substr(0, prefix.length()) == prefix) {
      events.push_back(key.substr(prefix.length()));
    }
  }

  return events;
}

void SubscriptionManager::clearDeviceSubscriptions(const std::string& deviceId) {
  if (!isValidDeviceId(deviceId)) {
    spdlog::warn("Invalid device ID for clearing subscriptions: {}", deviceId);
    return;
  }

  std::lock_guard<std::mutex> lock(subscriptionsMutex);

  // Clear property subscriptions
  std::string propPrefix = deviceId + ":property:";
  auto propIt = propertySubscriptions.begin();
  int removedProps = 0;
  while (propIt != propertySubscriptions.end()) {
    if (propIt->first.substr(0, propPrefix.length()) == propPrefix) {
      propIt = propertySubscriptions.erase(propIt);
      removedProps++;
    } else {
      ++propIt;
    }
  }

  // Clear event subscriptions
  std::string eventPrefix = deviceId + ":event:";
  auto eventIt = eventSubscriptions.begin();
  int removedEvents = 0;
  while (eventIt != eventSubscriptions.end()) {
    if (eventIt->first.substr(0, eventPrefix.length()) == eventPrefix) {
      eventIt = eventSubscriptions.erase(eventIt);
      removedEvents++;
    } else {
      ++eventIt;
    }
  }

  updateStats(-removedProps, -removedEvents, 0, 0, 0);
  spdlog::info("Cleared {} property and {} event subscriptions for device '{}'",
               removedProps, removedEvents, deviceId);
}

void SubscriptionManager::clearAllSubscriptions() {
  std::lock_guard<std::mutex> lock(subscriptionsMutex);
  
  size_t propCount = propertySubscriptions.size();
  size_t eventCount = eventSubscriptions.size();
  
  propertySubscriptions.clear();
  eventSubscriptions.clear();
  
  updateStats(-static_cast<int>(propCount), -static_cast<int>(eventCount), 0, 0, 0);
  spdlog::info("Cleared all subscriptions: {} properties, {} events", propCount, eventCount);
}

json SubscriptionManager::getSubscriptionStats() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  
  json stats;
  stats["propertySubscriptionCount"] = propertySubscriptionCount;
  stats["eventSubscriptionCount"] = eventSubscriptionCount;
  stats["propertyNotifications"] = propertyNotifications;
  stats["eventNotifications"] = eventNotifications;
  stats["callbackErrors"] = callbackErrors;
  
  {
    std::lock_guard<std::mutex> subLock(subscriptionsMutex);
    stats["activePropertySubscriptions"] = propertySubscriptions.size();
    stats["activeEventSubscriptions"] = eventSubscriptions.size();
  }
  
  return stats;
}

bool SubscriptionManager::isSubscribedToProperty(const std::string& deviceId,
                                                const std::string& property) const {
  if (!isValidDeviceId(deviceId) || !isValidPropertyName(property)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(subscriptionsMutex);
  std::string key = makePropertyKey(deviceId, property);
  return propertySubscriptions.find(key) != propertySubscriptions.end();
}

bool SubscriptionManager::isSubscribedToEvent(const std::string& deviceId,
                                             const std::string& event) const {
  if (!isValidDeviceId(deviceId) || !isValidEventName(event)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(subscriptionsMutex);
  std::string key = makeEventKey(deviceId, event);
  return eventSubscriptions.find(key) != eventSubscriptions.end();
}

std::string SubscriptionManager::makePropertyKey(const std::string& deviceId,
                                                const std::string& property) const {
  return deviceId + ":property:" + property;
}

std::string SubscriptionManager::makeEventKey(const std::string& deviceId,
                                             const std::string& event) const {
  return deviceId + ":event:" + event;
}

bool SubscriptionManager::isValidDeviceId(const std::string& deviceId) const {
  if (deviceId.empty() || deviceId.length() > 256) {
    return false;
  }

  return std::all_of(deviceId.begin(), deviceId.end(), [](char c) {
    return std::isalnum(c) || c == '_' || c == '-' || c == '.';
  });
}

bool SubscriptionManager::isValidPropertyName(const std::string& property) const {
  if (property.empty() || property.length() > 128) {
    return false;
  }

  return std::all_of(property.begin(), property.end(), [](char c) {
    return std::isalnum(c) || c == '_' || c == '-' || c == '.';
  });
}

bool SubscriptionManager::isValidEventName(const std::string& event) const {
  if (event.empty() || event.length() > 128) {
    return false;
  }

  return std::all_of(event.begin(), event.end(), [](char c) {
    return std::isalnum(c) || c == '_' || c == '-' || c == '.';
  });
}

void SubscriptionManager::updateStats(int propSubs, int eventSubs, size_t propNotifs,
                                     size_t eventNotifs, size_t errors) {
  std::lock_guard<std::mutex> lock(statsMutex);
  propertySubscriptionCount += propSubs;
  eventSubscriptionCount += eventSubs;
  propertyNotifications += propNotifs;
  eventNotifications += eventNotifs;
  callbackErrors += errors;
}

template<typename CallbackType>
void SubscriptionManager::executeCallbackSafely(CallbackType callback, const std::string& deviceId,
                                               const std::string& name, const json& data) {
  std::thread([this, callback, deviceId, name, data]() {
    try {
      callback(deviceId, name, data);
    } catch (const std::exception& e) {
      spdlog::error("Error in subscription callback for '{}' on device '{}': {}",
                    name, deviceId, e.what());
      updateStats(0, 0, 0, 0, 1);
    } catch (...) {
      spdlog::error("Unknown error in subscription callback for '{}' on device '{}'",
                    name, deviceId);
      updateStats(0, 0, 0, 0, 1);
    }
  }).detach();
}

} // namespace hydrogen

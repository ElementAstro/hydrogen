#pragma once

#include "client/message_processor.h"
#include "common/message.h"
#include <functional>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

namespace astrocomm {

using json = nlohmann::json;

/**
 * @brief Manages property and event subscriptions and callback handling
 * 
 * This class is responsible for:
 * - Managing property change subscriptions
 * - Managing event subscriptions
 * - Handling incoming property change notifications
 * - Handling incoming event notifications
 * - Callback lifecycle management
 * - Subscription statistics and monitoring
 */
class SubscriptionManager {
public:
    /**
     * @brief Callback function type for property changes
     * @param deviceId The ID of the device
     * @param property The name of the property that changed
     * @param value The new value of the property
     */
    using PropertyCallback = std::function<void(const std::string& deviceId,
                                               const std::string& property,
                                               const json& value)>;

    /**
     * @brief Callback function type for events
     * @param deviceId The ID of the device
     * @param event The name of the event
     * @param details Additional event details
     */
    using EventCallback = std::function<void(const std::string& deviceId,
                                            const std::string& event,
                                            const json& details)>;

    /**
     * @brief Constructor
     * @param messageProcessor Pointer to the message processor for communication
     */
    explicit SubscriptionManager(MessageProcessor* messageProcessor);

    /**
     * @brief Destructor
     */
    ~SubscriptionManager();

    /**
     * @brief Subscribe to property changes on a device
     * @param deviceId The ID of the device
     * @param property The name of the property to monitor
     * @param callback Function to call when the property changes
     */
    void subscribeToProperty(const std::string& deviceId, const std::string& property,
                            PropertyCallback callback);

    /**
     * @brief Unsubscribe from property changes
     * @param deviceId The ID of the device
     * @param property The name of the property to stop monitoring
     */
    void unsubscribeFromProperty(const std::string& deviceId, const std::string& property);

    /**
     * @brief Subscribe to events from a device
     * @param deviceId The ID of the device
     * @param event The name of the event to monitor
     * @param callback Function to call when the event occurs
     */
    void subscribeToEvent(const std::string& deviceId, const std::string& event,
                         EventCallback callback);

    /**
     * @brief Unsubscribe from events
     * @param deviceId The ID of the device
     * @param event The name of the event to stop monitoring
     */
    void unsubscribeFromEvent(const std::string& deviceId, const std::string& event);

    /**
     * @brief Handle incoming property change messages
     * @param msg The event message containing property changes
     */
    void handlePropertyChange(const EventMessage& msg);

    /**
     * @brief Handle incoming event messages
     * @param msg The event message
     */
    void handleEvent(const EventMessage& msg);

    /**
     * @brief Get all property subscriptions for a device
     * @param deviceId The ID of the device
     * @return Vector of property names being monitored
     */
    std::vector<std::string> getPropertySubscriptions(const std::string& deviceId) const;

    /**
     * @brief Get all event subscriptions for a device
     * @param deviceId The ID of the device
     * @return Vector of event names being monitored
     */
    std::vector<std::string> getEventSubscriptions(const std::string& deviceId) const;

    /**
     * @brief Clear all subscriptions for a device
     * @param deviceId The ID of the device
     */
    void clearDeviceSubscriptions(const std::string& deviceId);

    /**
     * @brief Clear all subscriptions
     */
    void clearAllSubscriptions();

    /**
     * @brief Get subscription statistics
     * @return JSON object with subscription statistics
     */
    json getSubscriptionStats() const;

    /**
     * @brief Check if subscribed to a specific property
     * @param deviceId The ID of the device
     * @param property The name of the property
     * @return true if subscribed, false otherwise
     */
    bool isSubscribedToProperty(const std::string& deviceId, const std::string& property) const;

    /**
     * @brief Check if subscribed to a specific event
     * @param deviceId The ID of the device
     * @param event The name of the event
     * @return true if subscribed, false otherwise
     */
    bool isSubscribedToEvent(const std::string& deviceId, const std::string& event) const;

private:
    // Message processor for communication
    MessageProcessor* messageProcessor;

    // Subscription storage
    mutable std::mutex subscriptionsMutex;
    std::map<std::string, PropertyCallback> propertySubscriptions;
    std::map<std::string, EventCallback> eventSubscriptions;

    // Statistics
    mutable std::mutex statsMutex;
    mutable size_t propertySubscriptionCount;
    mutable size_t eventSubscriptionCount;
    mutable size_t propertyNotifications;
    mutable size_t eventNotifications;
    mutable size_t callbackErrors;

    /**
     * @brief Generate a unique key for property subscriptions
     * @param deviceId The device ID
     * @param property The property name
     * @return Unique subscription key
     */
    std::string makePropertyKey(const std::string& deviceId, const std::string& property) const;

    /**
     * @brief Generate a unique key for event subscriptions
     * @param deviceId The device ID
     * @param event The event name
     * @return Unique subscription key
     */
    std::string makeEventKey(const std::string& deviceId, const std::string& event) const;

    /**
     * @brief Validate device ID format
     * @param deviceId The device ID to validate
     * @return true if valid, false otherwise
     */
    bool isValidDeviceId(const std::string& deviceId) const;

    /**
     * @brief Validate property name format
     * @param property The property name to validate
     * @return true if valid, false otherwise
     */
    bool isValidPropertyName(const std::string& property) const;

    /**
     * @brief Validate event name format
     * @param event The event name to validate
     * @return true if valid, false otherwise
     */
    bool isValidEventName(const std::string& event) const;

    /**
     * @brief Update subscription statistics
     * @param propSubs Change in property subscription count
     * @param eventSubs Change in event subscription count
     * @param propNotifs Number of property notifications processed
     * @param eventNotifs Number of event notifications processed
     * @param errors Number of callback errors
     */
    void updateStats(int propSubs = 0, int eventSubs = 0, size_t propNotifs = 0,
                    size_t eventNotifs = 0, size_t errors = 0);

    /**
     * @brief Execute a callback safely with error handling
     * @param callback The callback function to execute
     * @param deviceId Device ID parameter
     * @param name Property or event name parameter
     * @param data Data parameter (value for properties, details for events)
     */
    template<typename CallbackType>
    void executeCallbackSafely(CallbackType callback, const std::string& deviceId,
                              const std::string& name, const json& data);
};

} // namespace astrocomm

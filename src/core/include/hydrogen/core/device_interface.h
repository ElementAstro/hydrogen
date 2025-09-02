#pragma once

#include "message.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace hydrogen {
namespace core {

/**
 * @class IDevice
 * @brief Abstract interface for all astronomical devices
 * 
 * This interface defines the common functionality that all astronomical
 * devices must implement, including property management, command handling,
 * and communication with device servers.
 */
class IDevice {
public:
    virtual ~IDevice() = default;

    /**
     * @brief Get the unique device identifier
     * @return Device ID string
     */
    virtual std::string getDeviceId() const = 0;

    /**
     * @brief Get the device type
     * @return Device type string (e.g., "telescope", "camera", "focuser")
     */
    virtual std::string getDeviceType() const = 0;

    /**
     * @brief Get device information as JSON
     * @return JSON object containing device metadata
     */
    virtual json getDeviceInfo() const = 0;

    /**
     * @brief Set a device property
     * @param property Property name
     * @param value Property value as JSON
     */
    virtual void setProperty(const std::string& property, const json& value) = 0;

    /**
     * @brief Get a device property
     * @param property Property name
     * @return Property value as JSON
     */
    virtual json getProperty(const std::string& property) const = 0;

    /**
     * @brief Get all device properties
     * @return JSON object containing all properties
     */
    virtual json getAllProperties() const = 0;

    /**
     * @brief Get device capabilities
     * @return Vector of capability strings
     */
    virtual std::vector<std::string> getCapabilities() const = 0;

    /**
     * @brief Check if device has a specific capability
     * @param capability Capability name
     * @return True if device has the capability
     */
    virtual bool hasCapability(const std::string& capability) const = 0;

    /**
     * @brief Start the device
     * @return True if started successfully
     */
    virtual bool start() = 0;

    /**
     * @brief Stop the device
     */
    virtual void stop() = 0;

    /**
     * @brief Check if device is running
     * @return True if device is running
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief Connect to a device server
     * @param host Server hostname or IP
     * @param port Server port
     * @return True if connected successfully
     */
    virtual bool connect(const std::string& host, uint16_t port) = 0;

    /**
     * @brief Disconnect from server
     */
    virtual void disconnect() = 0;

    /**
     * @brief Check if connected to server
     * @return True if connected
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Register device with the server
     * @return True if registration successful
     */
    virtual bool registerDevice() = 0;
};

/**
 * @brief Command handler function type
 */
using CommandHandler = std::function<void(const CommandMessage&, ResponseMessage&)>;

/**
 * @class DeviceBase
 * @brief Base implementation for astronomical devices
 * 
 * This class provides a default implementation of the IDevice interface
 * with common functionality like property management, command handling,
 * and server communication.
 */
class DeviceBase : public IDevice {
public:
    /**
     * @brief Constructor
     * @param deviceId Unique device identifier
     * @param deviceType Device type string
     * @param manufacturer Device manufacturer
     * @param model Device model
     */
    DeviceBase(const std::string& deviceId, const std::string& deviceType,
               const std::string& manufacturer, const std::string& model);

    virtual ~DeviceBase();

    // IDevice interface implementation
    std::string getDeviceId() const override;
    std::string getDeviceType() const override;
    json getDeviceInfo() const override;
    void setProperty(const std::string& property, const json& value) override;
    json getProperty(const std::string& property) const override;
    json getAllProperties() const override;
    std::vector<std::string> getCapabilities() const override;
    bool hasCapability(const std::string& capability) const override;

    /**
     * @brief Register a command handler
     * @param command Command name
     * @param handler Handler function
     */
    void registerCommandHandler(const std::string& command, CommandHandler handler);

    /**
     * @brief Add a capability to the device
     * @param capability Capability name
     */
    void addCapability(const std::string& capability);

    /**
     * @brief Remove a capability from the device
     * @param capability Capability name
     */
    void removeCapability(const std::string& capability);

protected:
    /**
     * @brief Handle a command message
     * @param cmd Command message
     */
    virtual void handleCommandMessage(const CommandMessage& cmd);

    /**
     * @brief Send a response message
     * @param response Response message
     */
    virtual void sendResponse(const ResponseMessage& response);

    /**
     * @brief Send an event message
     * @param event Event message
     */
    virtual void sendEvent(const EventMessage& event);

    /**
     * @brief Send property changed event
     * @param property Property name
     * @param value New value
     * @param previousValue Previous value
     */
    virtual void sendPropertyChangedEvent(const std::string& property,
                                        const json& value,
                                        const json& previousValue);

    /**
     * @brief Initialize default properties
     */
    virtual void initializeProperties();

    // Device metadata
    std::string deviceId_;
    std::string deviceType_;
    std::string manufacturer_;
    std::string model_;
    std::string firmwareVersion_;

    // Property management
    mutable std::mutex propertiesMutex_;
    std::unordered_map<std::string, json> properties_;
    std::vector<std::string> capabilities_;

    // Command handling
    std::unordered_map<std::string, CommandHandler> commandHandlers_;

    // State management
    bool running_;
    bool connected_;
};

/**
 * @brief Factory function type for creating devices
 */
using DeviceFactory = std::function<std::unique_ptr<IDevice>(const std::string&, const json&)>;

/**
 * @class DeviceRegistry
 * @brief Registry for device types and their factories
 */
class DeviceRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the registry instance
     */
    static DeviceRegistry& getInstance();

    /**
     * @brief Register a device type
     * @param deviceType Device type name
     * @param factory Factory function for creating devices of this type
     */
    void registerDeviceType(const std::string& deviceType, DeviceFactory factory);

    /**
     * @brief Create a device instance
     * @param deviceType Device type
     * @param deviceId Device ID
     * @param config Device configuration
     * @return Unique pointer to created device, or nullptr if type not found
     */
    std::unique_ptr<IDevice> createDevice(const std::string& deviceType,
                                        const std::string& deviceId,
                                        const json& config = json::object());

    /**
     * @brief Get list of registered device types
     * @return Vector of device type names
     */
    std::vector<std::string> getRegisteredTypes() const;

    /**
     * @brief Check if a device type is registered
     * @param deviceType Device type name
     * @return True if type is registered
     */
    bool isTypeRegistered(const std::string& deviceType) const;

private:
    DeviceRegistry() = default;
    std::unordered_map<std::string, DeviceFactory> factories_;
    mutable std::mutex factoriesMutex_;
};

} // namespace core
} // namespace hydrogen

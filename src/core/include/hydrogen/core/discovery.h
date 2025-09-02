#pragma once

#include "device_interface.h"
#include "device_plugin.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Device discovery method
 */
enum class DiscoveryMethod {
    NETWORK_SCAN,        // Network-based discovery
    USB_SCAN,           // USB device enumeration
    SERIAL_SCAN,        // Serial port scanning
    BLUETOOTH_SCAN,     // Bluetooth device discovery
    ZEROCONF,           // Zero-configuration networking
    UPNP,               // Universal Plug and Play
    MANUAL,             // Manual device registration
    PLUGIN_SPECIFIC     // Plugin-defined discovery method
};

/**
 * @brief Device capability information
 */
struct DeviceCapability {
    std::string name;
    std::string description;
    json parameters;
    bool isRequired = false;
    
    json toJson() const;
    static DeviceCapability fromJson(const json& j);
};

/**
 * @brief Discovered device information
 */
struct DiscoveredDevice {
    std::string deviceId;
    std::string deviceType;
    std::string name;
    std::string manufacturer;
    std::string model;
    std::string serialNumber;
    std::string firmwareVersion;
    DiscoveryMethod discoveryMethod;
    std::string connectionString;
    std::vector<DeviceCapability> capabilities;
    json configuration;
    json metadata;
    std::chrono::system_clock::time_point discoveryTime;
    bool isConfigured = false;
    bool isConnectable = false;
    
    json toJson() const;
    static DiscoveredDevice fromJson(const json& j);
};

/**
 * @brief Auto-configuration template
 */
struct ConfigurationTemplate {
    std::string deviceType;
    std::string manufacturer;
    std::string model;
    json defaultConfiguration;
    std::vector<std::string> requiredParameters;
    std::vector<std::string> optionalParameters;
    json validationRules;
    
    json toJson() const;
    static ConfigurationTemplate fromJson(const json& j);
};

/**
 * @brief Device discovery filter
 */
struct DiscoveryFilter {
    std::vector<std::string> deviceTypes;
    std::vector<std::string> manufacturers;
    std::vector<DiscoveryMethod> methods;
    bool includeConfigured = true;
    bool includeUnconfigured = true;
    
    json toJson() const;
    static DiscoveryFilter fromJson(const json& j);
};

/**
 * @brief Device discovery interface
 */
class IDeviceDiscovery {
public:
    virtual ~IDeviceDiscovery() = default;
    
    /**
     * @brief Start device discovery
     * @param filter Discovery filter
     * @return True if discovery started successfully
     */
    virtual bool startDiscovery(const DiscoveryFilter& filter = DiscoveryFilter{}) = 0;
    
    /**
     * @brief Stop device discovery
     */
    virtual void stopDiscovery() = 0;
    
    /**
     * @brief Get discovered devices
     * @param filter Optional filter for results
     * @return Vector of discovered devices
     */
    virtual std::vector<DiscoveredDevice> getDiscoveredDevices(
        const DiscoveryFilter& filter = DiscoveryFilter{}) const = 0;
    
    /**
     * @brief Auto-configure discovered device
     * @param deviceId Device identifier
     * @return True if configuration successful
     */
    virtual bool autoConfigureDevice(const std::string& deviceId) = 0;
    
    /**
     * @brief Register configuration template
     * @param template Configuration template
     */
    virtual void registerConfigurationTemplate(const ConfigurationTemplate& configTemplate) = 0;
    
    /**
     * @brief Get configuration template for device
     * @param deviceType Device type
     * @param manufacturer Device manufacturer
     * @param model Device model
     * @return Configuration template if found
     */
    virtual std::optional<ConfigurationTemplate> getConfigurationTemplate(
        const std::string& deviceType,
        const std::string& manufacturer,
        const std::string& model) const = 0;
    
    /**
     * @brief Register discovery method handler
     * @param method Discovery method
     * @param handler Function to handle discovery for this method
     */
    virtual void registerDiscoveryHandler(DiscoveryMethod method,
        std::function<std::vector<DiscoveredDevice>()> handler) = 0;
    
    /**
     * @brief Register device found callback
     * @param callback Function to call when new device is discovered
     */
    virtual void setDeviceFoundCallback(std::function<void(const DiscoveredDevice&)> callback) = 0;
    
    /**
     * @brief Register device lost callback
     * @param callback Function to call when device is no longer discoverable
     */
    virtual void setDeviceLostCallback(std::function<void(const std::string&)> callback) = 0;
    
    /**
     * @brief Force refresh of device discovery
     * @return Number of devices discovered
     */
    virtual int refreshDiscovery() = 0;
    
    /**
     * @brief Check if discovery is running
     * @return True if discovery is active
     */
    virtual bool isDiscoveryRunning() const = 0;
    
    /**
     * @brief Get discovery statistics
     * @return JSON object with discovery statistics
     */
    virtual json getDiscoveryStatistics() const = 0;
};

/**
 * @brief Concrete implementation of device discovery
 */
class DeviceDiscovery : public IDeviceDiscovery {
public:
    DeviceDiscovery();
    virtual ~DeviceDiscovery();
    
    // IDeviceDiscovery implementation
    bool startDiscovery(const DiscoveryFilter& filter = DiscoveryFilter{}) override;
    void stopDiscovery() override;
    std::vector<DiscoveredDevice> getDiscoveredDevices(
        const DiscoveryFilter& filter = DiscoveryFilter{}) const override;
    bool autoConfigureDevice(const std::string& deviceId) override;
    void registerConfigurationTemplate(const ConfigurationTemplate& configTemplate) override;
    std::optional<ConfigurationTemplate> getConfigurationTemplate(
        const std::string& deviceType,
        const std::string& manufacturer,
        const std::string& model) const override;
    void registerDiscoveryHandler(DiscoveryMethod method,
        std::function<std::vector<DiscoveredDevice>()> handler) override;
    void setDeviceFoundCallback(std::function<void(const DiscoveredDevice&)> callback) override;
    void setDeviceLostCallback(std::function<void(const std::string&)> callback) override;
    int refreshDiscovery() override;
    bool isDiscoveryRunning() const override;
    json getDiscoveryStatistics() const override;
    
    /**
     * @brief Get singleton instance
     * @return Reference to singleton instance
     */
    static DeviceDiscovery& getInstance();
    
    /**
     * @brief Set discovery interval
     * @param interval Interval between discovery scans
     */
    void setDiscoveryInterval(std::chrono::seconds interval);
    
    /**
     * @brief Enable/disable continuous discovery
     * @param enabled Whether to enable continuous discovery
     */
    void setContinuousDiscovery(bool enabled);
    
    /**
     * @brief Set device timeout (how long before device is considered lost)
     * @param timeout Timeout duration
     */
    void setDeviceTimeout(std::chrono::seconds timeout);
    
    /**
     * @brief Load configuration templates from file
     * @param filename File to load templates from
     * @return True if successful
     */
    bool loadConfigurationTemplates(const std::string& filename);
    
    /**
     * @brief Save configuration templates to file
     * @param filename File to save templates to
     * @return True if successful
     */
    bool saveConfigurationTemplates(const std::string& filename) const;
    
    /**
     * @brief Clear all discovered devices
     */
    void clearDiscoveredDevices();
    
    /**
     * @brief Get device by ID
     * @param deviceId Device identifier
     * @return Discovered device if found
     */
    std::optional<DiscoveredDevice> getDevice(const std::string& deviceId) const;
    
    /**
     * @brief Remove device from discovered list
     * @param deviceId Device identifier
     * @return True if device was removed
     */
    bool removeDevice(const std::string& deviceId);
    
    /**
     * @brief Update device information
     * @param device Updated device information
     * @return True if device was updated
     */
    bool updateDevice(const DiscoveredDevice& device);

private:
    // Discovery thread management
    void discoveryLoop();
    void performDiscovery();
    void checkDeviceTimeouts();

    // Internal device management
    void addDiscoveredDevice(const DiscoveredDevice& device);
    bool matchesFilter(const DiscoveredDevice& device, const DiscoveryFilter& filter) const;

    // Configuration and state
    mutable std::mutex devicesMutex_;
    std::unordered_map<std::string, DiscoveredDevice> discoveredDevices_;

    mutable std::mutex templatesMutex_;
    std::vector<ConfigurationTemplate> configurationTemplates_;

    mutable std::mutex handlersMutex_;
    std::unordered_map<DiscoveryMethod, std::function<std::vector<DiscoveredDevice>()>> discoveryHandlers_;

    // Discovery control
    std::atomic<bool> discoveryRunning_{false};
    std::atomic<bool> continuousDiscovery_{true};
    std::thread discoveryThread_;
    std::chrono::seconds discoveryInterval_{30};
    std::chrono::seconds deviceTimeout_{300};

    // Callbacks
    std::function<void(const DiscoveredDevice&)> deviceFoundCallback_;
    std::function<void(const std::string&)> deviceLostCallback_;

    // Statistics
    mutable std::mutex statsMutex_;
    struct {
        std::atomic<uint64_t> totalDevicesDiscovered{0};
        std::atomic<uint64_t> devicesCurrentlyDiscovered{0};
        std::atomic<uint64_t> discoveryScansPerformed{0};
        std::atomic<uint64_t> autoConfigurationsAttempted{0};
        std::atomic<uint64_t> autoConfigurationsSuccessful{0};
        std::chrono::system_clock::time_point lastDiscoveryTime;
        std::chrono::system_clock::time_point discoveryStartTime;
    } statistics_;

    // Singleton instance
    static std::unique_ptr<DeviceDiscovery> instance_;
    static std::mutex instanceMutex_;
};

} // namespace core
} // namespace hydrogen

#pragma once

#include "../core/service_registry.h"
#include <hydrogen/core.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <memory>
#include <mutex>

namespace hydrogen {
namespace server {
namespace services {

/**
 * @brief Device connection status
 */
enum class DeviceConnectionStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    ERROR
};

/**
 * @brief Device health status
 */
enum class DeviceHealthStatus {
    UNKNOWN,
    HEALTHY,
    WARNING,
    CRITICAL,
    OFFLINE
};

/**
 * @brief Device information structure
 */
struct DeviceInfo {
    std::string deviceId;
    std::string deviceType;
    std::string deviceName;
    std::string manufacturer;
    std::string model;
    std::string firmwareVersion;
    std::string driverVersion;
    std::vector<std::string> capabilities;
    std::unordered_map<std::string, std::string> properties;
    DeviceConnectionStatus connectionStatus;
    DeviceHealthStatus healthStatus;
    std::chrono::system_clock::time_point lastSeen;
    std::chrono::system_clock::time_point registeredAt;
    std::string clientId;
    std::string remoteAddress;
};

/**
 * @brief Device command structure
 */
struct DeviceCommand {
    std::string commandId;
    std::string deviceId;
    std::string command;
    std::unordered_map<std::string, std::string> parameters;
    std::string clientId;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::seconds timeout{30};
    int priority = 0;
};

/**
 * @brief Device command result
 */
struct DeviceCommandResult {
    std::string commandId;
    std::string deviceId;
    bool success;
    std::string result;
    std::string errorMessage;
    std::chrono::system_clock::time_point completedAt;
    std::chrono::milliseconds executionTime;
};

/**
 * @brief Device group information
 */
struct DeviceGroup {
    std::string groupId;
    std::string groupName;
    std::string description;
    std::vector<std::string> deviceIds;
    std::unordered_map<std::string, std::string> groupProperties;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point modifiedAt;
};

/**
 * @brief Device service interface
 * 
 * Provides comprehensive device management functionality including
 * registration, discovery, health monitoring, command execution,
 * and group management.
 */
class IDeviceService : public virtual core::IService {
public:
    virtual ~IDeviceService() = default;

    // Device registration and discovery
    virtual bool registerDevice(const DeviceInfo& deviceInfo) = 0;
    virtual bool unregisterDevice(const std::string& deviceId) = 0;
    virtual std::vector<DeviceInfo> getAllDevices() const = 0;
    virtual DeviceInfo getDeviceInfo(const std::string& deviceId) const = 0;
    virtual std::vector<DeviceInfo> getDevicesByType(const std::string& deviceType) const = 0;
    virtual std::vector<DeviceInfo> getDevicesByCapability(const std::string& capability) const = 0;
    virtual bool isDeviceRegistered(const std::string& deviceId) const = 0;

    // Device connection management
    virtual bool connectDevice(const std::string& deviceId) = 0;
    virtual bool disconnectDevice(const std::string& deviceId) = 0;
    virtual DeviceConnectionStatus getDeviceConnectionStatus(const std::string& deviceId) const = 0;
    virtual std::vector<std::string> getConnectedDevices() const = 0;
    virtual std::vector<std::string> getDisconnectedDevices() const = 0;

    // Device properties
    virtual bool updateDeviceProperties(const std::string& deviceId, 
                                      const std::unordered_map<std::string, std::string>& properties) = 0;
    virtual std::unordered_map<std::string, std::string> getDeviceProperties(const std::string& deviceId) const = 0;
    virtual std::string getDeviceProperty(const std::string& deviceId, const std::string& property) const = 0;
    virtual bool setDeviceProperty(const std::string& deviceId, const std::string& property, const std::string& value) = 0;

    // Device commands
    virtual std::string executeCommand(const DeviceCommand& command) = 0;
    virtual DeviceCommandResult getCommandResult(const std::string& commandId) const = 0;
    virtual bool cancelCommand(const std::string& commandId) = 0;
    virtual std::vector<DeviceCommand> getPendingCommands(const std::string& deviceId = "") const = 0;
    virtual std::vector<DeviceCommandResult> getCommandHistory(const std::string& deviceId = "", 
                                                             size_t limit = 100) const = 0;

    // Bulk operations
    virtual std::vector<std::string> executeBulkCommand(const std::vector<std::string>& deviceIds,
                                                       const std::string& command,
                                                       const std::unordered_map<std::string, std::string>& parameters) = 0;
    virtual bool updateBulkProperties(const std::vector<std::string>& deviceIds,
                                    const std::unordered_map<std::string, std::string>& properties) = 0;

    // Device groups
    virtual bool createDeviceGroup(const DeviceGroup& group) = 0;
    virtual bool deleteDeviceGroup(const std::string& groupId) = 0;
    virtual DeviceGroup getDeviceGroup(const std::string& groupId) const = 0;
    virtual std::vector<DeviceGroup> getAllDeviceGroups() const = 0;
    virtual bool addDeviceToGroup(const std::string& groupId, const std::string& deviceId) = 0;
    virtual bool removeDeviceFromGroup(const std::string& groupId, const std::string& deviceId) = 0;
    virtual std::vector<std::string> getDeviceGroups(const std::string& deviceId) const = 0;

    // Health monitoring
    virtual DeviceHealthStatus getDeviceHealthStatus(const std::string& deviceId) const = 0;
    virtual std::string getDeviceHealthDetails(const std::string& deviceId) const = 0;
    virtual std::vector<std::string> getUnhealthyDevices() const = 0;
    virtual bool performHealthCheck(const std::string& deviceId) = 0;
    virtual void setHealthCheckInterval(std::chrono::seconds interval) = 0;

    // Device search and filtering
    virtual std::vector<DeviceInfo> searchDevices(const std::string& query) const = 0;
    virtual std::vector<DeviceInfo> filterDevices(
        const std::function<bool(const DeviceInfo&)>& filter) const = 0;

    // Statistics and monitoring
    virtual size_t getDeviceCount() const = 0;
    virtual size_t getConnectedDeviceCount() const = 0;
    virtual std::unordered_map<std::string, size_t> getDeviceCountByType() const = 0;
    virtual std::unordered_map<std::string, size_t> getDeviceCountByStatus() const = 0;

    // Event callbacks
    using DeviceEventCallback = std::function<void(const std::string& deviceId, const std::string& event, const std::string& data)>;
    using ConnectionEventCallback = std::function<void(const std::string& deviceId, DeviceConnectionStatus status)>;
    using CommandEventCallback = std::function<void(const DeviceCommandResult& result)>;
    using HealthEventCallback = std::function<void(const std::string& deviceId, DeviceHealthStatus status, const std::string& details)>;

    virtual void setDeviceEventCallback(DeviceEventCallback callback) = 0;
    virtual void setConnectionEventCallback(ConnectionEventCallback callback) = 0;
    virtual void setCommandEventCallback(CommandEventCallback callback) = 0;
    virtual void setHealthEventCallback(HealthEventCallback callback) = 0;

    // Configuration templates
    virtual bool saveDeviceTemplate(const std::string& templateName, const DeviceInfo& deviceInfo) = 0;
    virtual DeviceInfo loadDeviceTemplate(const std::string& templateName) const = 0;
    virtual std::vector<std::string> getAvailableTemplates() const = 0;
    virtual bool deleteDeviceTemplate(const std::string& templateName) = 0;

    // Import/Export
    virtual bool exportDeviceConfiguration(const std::string& deviceId, const std::string& filePath) const = 0;
    virtual bool importDeviceConfiguration(const std::string& filePath) = 0;
    virtual bool exportAllDevices(const std::string& filePath) const = 0;
    virtual bool importDevices(const std::string& filePath) = 0;
};

/**
 * @brief Device service factory
 */
class DeviceServiceFactory : public core::IServiceFactory {
public:
    std::unique_ptr<core::IService> createService(
        const std::string& serviceName,
        const std::unordered_map<std::string, std::string>& config = {}
    ) override;
    
    std::vector<std::string> getSupportedServices() const override;
    bool isServiceSupported(const std::string& serviceName) const override;
};

} // namespace services
} // namespace server
} // namespace hydrogen

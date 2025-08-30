#pragma once

#include "../services/device_service.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>

namespace astrocomm {
namespace server {
namespace repositories {

/**
 * @brief Device query criteria
 */
struct DeviceQuery {
    std::optional<std::string> deviceType;
    std::optional<std::string> manufacturer;
    std::optional<std::string> model;
    std::optional<services::DeviceConnectionStatus> connectionStatus;
    std::optional<services::DeviceHealthStatus> healthStatus;
    std::vector<std::string> capabilities;
    std::unordered_map<std::string, std::string> propertyFilters;
    std::optional<std::chrono::system_clock::time_point> registeredAfter;
    std::optional<std::chrono::system_clock::time_point> registeredBefore;
    std::optional<std::chrono::system_clock::time_point> lastSeenAfter;
    std::optional<std::chrono::system_clock::time_point> lastSeenBefore;
    size_t limit = 0;
    size_t offset = 0;
    std::string sortBy = "deviceId";
    bool sortAscending = true;
};

/**
 * @brief Device repository interface
 * 
 * Provides data access layer for device information, supporting
 * CRUD operations, querying, and persistence management.
 */
class IDeviceRepository {
public:
    virtual ~IDeviceRepository() = default;

    // Basic CRUD operations
    virtual bool create(const services::DeviceInfo& device) = 0;
    virtual std::optional<services::DeviceInfo> read(const std::string& deviceId) const = 0;
    virtual bool update(const services::DeviceInfo& device) = 0;
    virtual bool remove(const std::string& deviceId) = 0;
    virtual bool exists(const std::string& deviceId) const = 0;

    // Bulk operations
    virtual bool createBulk(const std::vector<services::DeviceInfo>& devices) = 0;
    virtual std::vector<services::DeviceInfo> readBulk(const std::vector<std::string>& deviceIds) const = 0;
    virtual bool updateBulk(const std::vector<services::DeviceInfo>& devices) = 0;
    virtual bool removeBulk(const std::vector<std::string>& deviceIds) = 0;

    // Query operations
    virtual std::vector<services::DeviceInfo> findAll() const = 0;
    virtual std::vector<services::DeviceInfo> findByQuery(const DeviceQuery& query) const = 0;
    virtual std::vector<services::DeviceInfo> findByType(const std::string& deviceType) const = 0;
    virtual std::vector<services::DeviceInfo> findByManufacturer(const std::string& manufacturer) const = 0;
    virtual std::vector<services::DeviceInfo> findByCapability(const std::string& capability) const = 0;
    virtual std::vector<services::DeviceInfo> findByConnectionStatus(services::DeviceConnectionStatus status) const = 0;
    virtual std::vector<services::DeviceInfo> findByHealthStatus(services::DeviceHealthStatus status) const = 0;

    // Property operations
    virtual bool updateProperty(const std::string& deviceId, const std::string& property, const std::string& value) = 0;
    virtual std::optional<std::string> getProperty(const std::string& deviceId, const std::string& property) const = 0;
    virtual std::unordered_map<std::string, std::string> getProperties(const std::string& deviceId) const = 0;
    virtual bool removeProperty(const std::string& deviceId, const std::string& property) = 0;

    // Status operations
    virtual bool updateConnectionStatus(const std::string& deviceId, services::DeviceConnectionStatus status) = 0;
    virtual bool updateHealthStatus(const std::string& deviceId, services::DeviceHealthStatus status) = 0;
    virtual bool updateLastSeen(const std::string& deviceId, std::chrono::system_clock::time_point timestamp) = 0;

    // Statistics
    virtual size_t count() const = 0;
    virtual size_t countByType(const std::string& deviceType) const = 0;
    virtual size_t countByStatus(services::DeviceConnectionStatus status) const = 0;
    virtual std::unordered_map<std::string, size_t> getTypeStatistics() const = 0;
    virtual std::unordered_map<services::DeviceConnectionStatus, size_t> getStatusStatistics() const = 0;

    // Search operations
    virtual std::vector<services::DeviceInfo> search(const std::string& searchTerm) const = 0;
    virtual std::vector<services::DeviceInfo> findSimilar(const services::DeviceInfo& device, double threshold = 0.8) const = 0;

    // Persistence management
    virtual bool save() = 0;
    virtual bool load() = 0;
    virtual bool backup(const std::string& backupPath) const = 0;
    virtual bool restore(const std::string& backupPath) = 0;
    virtual bool clear() = 0;

    // Transaction support
    virtual bool beginTransaction() = 0;
    virtual bool commitTransaction() = 0;
    virtual bool rollbackTransaction() = 0;
    virtual bool isInTransaction() const = 0;

    // Event callbacks
    using DeviceChangeCallback = std::function<void(const std::string& deviceId, const std::string& operation, const services::DeviceInfo& device)>;
    virtual void setChangeCallback(DeviceChangeCallback callback) = 0;
};

/**
 * @brief Device command repository interface
 * 
 * Manages persistence of device commands and their execution results.
 */
class IDeviceCommandRepository {
public:
    virtual ~IDeviceCommandRepository() = default;

    // Command operations
    virtual bool storeCommand(const services::DeviceCommand& command) = 0;
    virtual std::optional<services::DeviceCommand> getCommand(const std::string& commandId) const = 0;
    virtual bool updateCommand(const services::DeviceCommand& command) = 0;
    virtual bool removeCommand(const std::string& commandId) = 0;

    // Command result operations
    virtual bool storeCommandResult(const services::DeviceCommandResult& result) = 0;
    virtual std::optional<services::DeviceCommandResult> getCommandResult(const std::string& commandId) const = 0;
    virtual bool updateCommandResult(const services::DeviceCommandResult& result) = 0;

    // Query operations
    virtual std::vector<services::DeviceCommand> getPendingCommands(const std::string& deviceId = "") const = 0;
    virtual std::vector<services::DeviceCommand> getCommandHistory(const std::string& deviceId = "", size_t limit = 100) const = 0;
    virtual std::vector<services::DeviceCommandResult> getResultHistory(const std::string& deviceId = "", size_t limit = 100) const = 0;

    // Statistics
    virtual size_t getCommandCount(const std::string& deviceId = "") const = 0;
    virtual size_t getSuccessfulCommandCount(const std::string& deviceId = "") const = 0;
    virtual size_t getFailedCommandCount(const std::string& deviceId = "") const = 0;
    virtual std::chrono::milliseconds getAverageExecutionTime(const std::string& deviceId = "") const = 0;

    // Cleanup operations
    virtual bool cleanupOldCommands(std::chrono::hours maxAge) = 0;
    virtual bool cleanupOldResults(std::chrono::hours maxAge) = 0;
};

/**
 * @brief Device group repository interface
 * 
 * Manages persistence of device groups and their memberships.
 */
class IDeviceGroupRepository {
public:
    virtual ~IDeviceGroupRepository() = default;

    // Group operations
    virtual bool createGroup(const services::DeviceGroup& group) = 0;
    virtual std::optional<services::DeviceGroup> getGroup(const std::string& groupId) const = 0;
    virtual bool updateGroup(const services::DeviceGroup& group) = 0;
    virtual bool removeGroup(const std::string& groupId) = 0;
    virtual std::vector<services::DeviceGroup> getAllGroups() const = 0;

    // Membership operations
    virtual bool addDeviceToGroup(const std::string& groupId, const std::string& deviceId) = 0;
    virtual bool removeDeviceFromGroup(const std::string& groupId, const std::string& deviceId) = 0;
    virtual std::vector<std::string> getGroupDevices(const std::string& groupId) const = 0;
    virtual std::vector<std::string> getDeviceGroups(const std::string& deviceId) const = 0;

    // Query operations
    virtual std::vector<services::DeviceGroup> findGroupsByName(const std::string& name) const = 0;
    virtual std::vector<services::DeviceGroup> findGroupsContainingDevice(const std::string& deviceId) const = 0;

    // Statistics
    virtual size_t getGroupCount() const = 0;
    virtual size_t getGroupSize(const std::string& groupId) const = 0;
    virtual std::unordered_map<std::string, size_t> getGroupSizeStatistics() const = 0;
};

/**
 * @brief Repository factory interface
 */
class IRepositoryFactory {
public:
    virtual ~IRepositoryFactory() = default;
    
    virtual std::unique_ptr<IDeviceRepository> createDeviceRepository(const std::string& type, 
                                                                     const std::unordered_map<std::string, std::string>& config) = 0;
    virtual std::unique_ptr<IDeviceCommandRepository> createCommandRepository(const std::string& type,
                                                                             const std::unordered_map<std::string, std::string>& config) = 0;
    virtual std::unique_ptr<IDeviceGroupRepository> createGroupRepository(const std::string& type,
                                                                         const std::unordered_map<std::string, std::string>& config) = 0;
    
    virtual std::vector<std::string> getSupportedTypes() const = 0;
    virtual bool isTypeSupported(const std::string& type) const = 0;
};

} // namespace repositories
} // namespace server
} // namespace astrocomm

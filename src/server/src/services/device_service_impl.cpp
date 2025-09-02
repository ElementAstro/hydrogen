#include "hydrogen/server/services/device_service.h"
#include "hydrogen/server/core/service_registry.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>

namespace hydrogen {
namespace server {
namespace services {

/**
 * @brief Concrete implementation of the Device Service
 */
class DeviceServiceImpl : public core::BaseService, public IDeviceService {
public:
    explicit DeviceServiceImpl(const std::string& name = "DeviceService")
        : core::BaseService(name, "1.0.0") {
        description_ = "Device management service for Hydrogen server";
    }



    // IService implementation
    bool initialize() override {
        setState(core::ServiceState::INITIALIZING);
        
        spdlog::info("Initializing Device Service...");
        
        // Initialize internal data structures
        devices_.clear();
        deviceGroups_.clear();
        pendingCommands_.clear();
        commandHistory_.clear();
        
        // Set up health check interval
        healthCheckInterval_ = std::chrono::seconds(getConfigInt("health_check_interval", 30));
        
        setState(core::ServiceState::INITIALIZED);
        setHealthy(true);
        setHealthStatus("Device service initialized successfully");
        
        spdlog::info("Device Service initialized");
        return true;
    }

    bool start() override {
        if (core::BaseService::getState() != core::ServiceState::INITIALIZED) {
            if (!initialize()) {
                return false;
            }
        }
        
        setState(core::ServiceState::STARTING);
        spdlog::info("Starting Device Service...");
        
        // Start background health monitoring
        startHealthMonitoring();
        
        setState(core::ServiceState::RUNNING);
        setHealthStatus("Device service running");
        
        spdlog::info("Device Service started");
        return true;
    }

    bool stop() override {
        setState(core::ServiceState::STOPPING);
        spdlog::info("Stopping Device Service...");
        
        // Stop health monitoring
        stopHealthMonitoring();
        
        setState(core::ServiceState::STOPPED);
        setHealthStatus("Device service stopped");
        
        spdlog::info("Device Service stopped");
        return true;
    }

    bool shutdown() override {
        stop();
        
        // Clear all data
        std::lock_guard<std::mutex> lock(devicesMutex_);
        devices_.clear();
        deviceGroups_.clear();
        pendingCommands_.clear();
        commandHistory_.clear();
        
        spdlog::info("Device Service shutdown");
        return true;
    }

    std::vector<core::ServiceDependency> getDependencies() const override {
        return {}; // No dependencies for basic device service
    }

    bool areDependenciesSatisfied() const override {
        return true; // No dependencies
    }

    // IDeviceService implementation
    bool registerDevice(const DeviceInfo& deviceInfo) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        if (deviceInfo.deviceId.empty()) {
            spdlog::error("Cannot register device with empty ID");
            return false;
        }
        
        if (devices_.find(deviceInfo.deviceId) != devices_.end()) {
            spdlog::warn("Device already registered: {}", deviceInfo.deviceId);
        }
        
        DeviceInfo device = deviceInfo;
        device.registeredAt = std::chrono::system_clock::now();
        device.lastSeen = device.registeredAt;
        device.connectionStatus = DeviceConnectionStatus::DISCONNECTED;
        device.healthStatus = DeviceHealthStatus::UNKNOWN;
        
        devices_[device.deviceId] = device;
        
        spdlog::info("Registered device: {} ({})", device.deviceId, device.deviceName);
        
        if (deviceEventCallback_) {
            deviceEventCallback_(device.deviceId, "registered", "Device registered successfully");
        }
        
        updateMetric("total_devices", std::to_string(devices_.size()));
        return true;
    }

    bool unregisterDevice(const std::string& deviceId) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it == devices_.end()) {
            spdlog::warn("Device not found for unregistration: {}", deviceId);
            return false;
        }
        
        // Remove from all groups
        for (auto& groupPair : deviceGroups_) {
            auto& deviceIds = groupPair.second.deviceIds;
            deviceIds.erase(std::remove(deviceIds.begin(), deviceIds.end(), deviceId), deviceIds.end());
        }
        
        devices_.erase(it);
        
        spdlog::info("Unregistered device: {}", deviceId);
        
        if (deviceEventCallback_) {
            deviceEventCallback_(deviceId, "unregistered", "Device unregistered");
        }
        
        updateMetric("total_devices", std::to_string(devices_.size()));
        return true;
    }

    std::vector<DeviceInfo> getAllDevices() const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<DeviceInfo> result;
        result.reserve(devices_.size());
        
        for (const auto& pair : devices_) {
            result.push_back(pair.second);
        }
        
        return result;
    }

    DeviceInfo getDeviceInfo(const std::string& deviceId) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it != devices_.end()) {
            return it->second;
        }
        
        return DeviceInfo{}; // Return empty device info if not found
    }

    std::vector<DeviceInfo> getDevicesByType(const std::string& deviceType) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<DeviceInfo> result;
        
        for (const auto& pair : devices_) {
            if (pair.second.deviceType == deviceType) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    std::vector<DeviceInfo> getDevicesByCapability(const std::string& capability) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<DeviceInfo> result;
        
        for (const auto& pair : devices_) {
            const auto& capabilities = pair.second.capabilities;
            if (std::find(capabilities.begin(), capabilities.end(), capability) != capabilities.end()) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    bool isDeviceRegistered(const std::string& deviceId) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        return devices_.find(deviceId) != devices_.end();
    }

    bool connectDevice(const std::string& deviceId) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it == devices_.end()) {
            spdlog::error("Device not found: {}", deviceId);
            return false;
        }
        
        it->second.connectionStatus = DeviceConnectionStatus::CONNECTED;
        it->second.lastSeen = std::chrono::system_clock::now();
        
        spdlog::info("Device connected: {}", deviceId);
        
        if (connectionEventCallback_) {
            connectionEventCallback_(deviceId, DeviceConnectionStatus::CONNECTED);
        }
        
        updateConnectionMetrics();
        return true;
    }

    bool disconnectDevice(const std::string& deviceId) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it == devices_.end()) {
            spdlog::error("Device not found: {}", deviceId);
            return false;
        }
        
        it->second.connectionStatus = DeviceConnectionStatus::DISCONNECTED;
        
        spdlog::info("Device disconnected: {}", deviceId);
        
        if (connectionEventCallback_) {
            connectionEventCallback_(deviceId, DeviceConnectionStatus::DISCONNECTED);
        }
        
        updateConnectionMetrics();
        return true;
    }

    DeviceConnectionStatus getDeviceConnectionStatus(const std::string& deviceId) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it != devices_.end()) {
            return it->second.connectionStatus;
        }
        
        return DeviceConnectionStatus::DISCONNECTED;
    }

    std::vector<std::string> getConnectedDevices() const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<std::string> result;
        
        for (const auto& pair : devices_) {
            if (pair.second.connectionStatus == DeviceConnectionStatus::CONNECTED) {
                result.push_back(pair.first);
            }
        }
        
        return result;
    }

    std::vector<std::string> getDisconnectedDevices() const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<std::string> result;
        
        for (const auto& pair : devices_) {
            if (pair.second.connectionStatus == DeviceConnectionStatus::DISCONNECTED) {
                result.push_back(pair.first);
            }
        }
        
        return result;
    }

    bool updateDeviceProperties(const std::string& deviceId, 
                              const std::unordered_map<std::string, std::string>& properties) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it == devices_.end()) {
            spdlog::error("Device not found: {}", deviceId);
            return false;
        }
        
        for (const auto& prop : properties) {
            it->second.properties[prop.first] = prop.second;
        }
        
        spdlog::debug("Updated properties for device: {}", deviceId);
        return true;
    }

    std::unordered_map<std::string, std::string> getDeviceProperties(const std::string& deviceId) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it != devices_.end()) {
            return it->second.properties;
        }
        
        return {};
    }

    std::string getDeviceProperty(const std::string& deviceId, const std::string& property) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it != devices_.end()) {
            auto propIt = it->second.properties.find(property);
            if (propIt != it->second.properties.end()) {
                return propIt->second;
            }
        }
        
        return "";
    }

    bool setDeviceProperty(const std::string& deviceId, const std::string& property, const std::string& value) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it == devices_.end()) {
            spdlog::error("Device not found: {}", deviceId);
            return false;
        }
        
        it->second.properties[property] = value;
        spdlog::debug("Set property {} = {} for device: {}", property, value, deviceId);
        return true;
    }

    std::string executeCommand(const DeviceCommand& command) override {
        std::string commandId = generateCommandId();
        
        DeviceCommand cmd = command;
        cmd.commandId = commandId;
        cmd.timestamp = std::chrono::system_clock::now();
        
        {
            std::lock_guard<std::mutex> lock(commandsMutex_);
            pendingCommands_[commandId] = cmd;
        }
        
        spdlog::info("Queued command {} for device: {}", commandId, command.deviceId);
        
        // Simulate command execution (in real implementation, this would be async)
        executeCommandAsync(cmd);
        
        return commandId;
    }

    DeviceCommandResult getCommandResult(const std::string& commandId) const override {
        std::lock_guard<std::mutex> lock(commandsMutex_);
        
        auto it = commandHistory_.find(commandId);
        if (it != commandHistory_.end()) {
            return it->second;
        }
        
        return DeviceCommandResult{}; // Return empty result if not found
    }

    bool cancelCommand(const std::string& commandId) override {
        std::lock_guard<std::mutex> lock(commandsMutex_);
        
        auto it = pendingCommands_.find(commandId);
        if (it != pendingCommands_.end()) {
            pendingCommands_.erase(it);
            spdlog::info("Cancelled command: {}", commandId);
            return true;
        }
        
        return false;
    }

    std::vector<DeviceCommand> getPendingCommands(const std::string& deviceId) const override {
        std::lock_guard<std::mutex> lock(commandsMutex_);
        
        std::vector<DeviceCommand> result;
        
        for (const auto& pair : pendingCommands_) {
            if (deviceId.empty() || pair.second.deviceId == deviceId) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    std::vector<DeviceCommandResult> getCommandHistory(const std::string& deviceId, size_t limit) const override {
        std::lock_guard<std::mutex> lock(commandsMutex_);
        
        std::vector<DeviceCommandResult> result;
        
        for (const auto& pair : commandHistory_) {
            if (deviceId.empty() || pair.second.deviceId == deviceId) {
                result.push_back(pair.second);
                if (result.size() >= limit) {
                    break;
                }
            }
        }
        
        return result;
    }

    // Additional methods for bulk operations, groups, health monitoring, etc.
    std::vector<std::string> executeBulkCommand(const std::vector<std::string>& deviceIds,
                                               const std::string& command,
                                               const std::unordered_map<std::string, std::string>& parameters) override {
        std::vector<std::string> commandIds;
        
        for (const auto& deviceId : deviceIds) {
            DeviceCommand cmd;
            cmd.deviceId = deviceId;
            cmd.command = command;
            cmd.parameters = parameters;
            cmd.clientId = "bulk_operation";
            
            std::string commandId = executeCommand(cmd);
            commandIds.push_back(commandId);
        }
        
        spdlog::info("Executed bulk command '{}' on {} devices", command, deviceIds.size());
        return commandIds;
    }

    size_t getDeviceCount() const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        return devices_.size();
    }

    size_t getConnectedDeviceCount() const override {
        return getConnectedDevices().size();
    }

    void setDeviceEventCallback(DeviceEventCallback callback) override {
        deviceEventCallback_ = callback;
    }

    void setConnectionEventCallback(ConnectionEventCallback callback) override {
        connectionEventCallback_ = callback;
    }

    void setCommandEventCallback(CommandEventCallback callback) override {
        commandEventCallback_ = callback;
    }

    void setHealthEventCallback(HealthEventCallback callback) override {
        healthEventCallback_ = callback;
    }

    // Placeholder implementations for remaining methods
    bool updateBulkProperties(const std::vector<std::string>& deviceIds,
                            const std::unordered_map<std::string, std::string>& properties) override {
        for (const auto& deviceId : deviceIds) {
            updateDeviceProperties(deviceId, properties);
        }
        return true;
    }

    bool createDeviceGroup(const DeviceGroup& group) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        deviceGroups_[group.groupId] = group;
        return true;
    }

    bool deleteDeviceGroup(const std::string& groupId) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        return deviceGroups_.erase(groupId) > 0;
    }

    DeviceGroup getDeviceGroup(const std::string& groupId) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        auto it = deviceGroups_.find(groupId);
        return (it != deviceGroups_.end()) ? it->second : DeviceGroup{};
    }

    std::vector<DeviceGroup> getAllDeviceGroups() const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        std::vector<DeviceGroup> result;
        for (const auto& pair : deviceGroups_) {
            result.push_back(pair.second);
        }
        return result;
    }

    bool addDeviceToGroup(const std::string& groupId, const std::string& deviceId) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        auto it = deviceGroups_.find(groupId);
        if (it != deviceGroups_.end()) {
            it->second.deviceIds.push_back(deviceId);
            return true;
        }
        return false;
    }

    bool removeDeviceFromGroup(const std::string& groupId, const std::string& deviceId) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        auto it = deviceGroups_.find(groupId);
        if (it != deviceGroups_.end()) {
            auto& deviceIds = it->second.deviceIds;
            deviceIds.erase(std::remove(deviceIds.begin(), deviceIds.end(), deviceId), deviceIds.end());
            return true;
        }
        return false;
    }

    std::vector<std::string> getDeviceGroups(const std::string& deviceId) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        std::vector<std::string> result;
        for (const auto& pair : deviceGroups_) {
            const auto& deviceIds = pair.second.deviceIds;
            if (std::find(deviceIds.begin(), deviceIds.end(), deviceId) != deviceIds.end()) {
                result.push_back(pair.first);
            }
        }
        return result;
    }

    DeviceHealthStatus getDeviceHealthStatus(const std::string& deviceId) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        auto it = devices_.find(deviceId);
        return (it != devices_.end()) ? it->second.healthStatus : DeviceHealthStatus::UNKNOWN;
    }

    std::string getDeviceHealthDetails(const std::string& deviceId) const override {
        // Implementation would return detailed health information
        return "Health check passed";
    }

    std::vector<std::string> getUnhealthyDevices() const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        std::vector<std::string> result;
        for (const auto& pair : devices_) {
            if (pair.second.healthStatus == DeviceHealthStatus::CRITICAL ||
                pair.second.healthStatus == DeviceHealthStatus::WARNING) {
                result.push_back(pair.first);
            }
        }
        return result;
    }

    bool performHealthCheck(const std::string& deviceId) override {
        // Implementation would perform actual health check
        return true;
    }

    void setHealthCheckInterval(std::chrono::seconds interval) override {
        healthCheckInterval_ = interval;
    }

    std::vector<DeviceInfo> searchDevices(const std::string& query) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        std::vector<DeviceInfo> result;
        
        for (const auto& pair : devices_) {
            const auto& device = pair.second;
            if (device.deviceName.find(query) != std::string::npos ||
                device.deviceType.find(query) != std::string::npos ||
                device.manufacturer.find(query) != std::string::npos) {
                result.push_back(device);
            }
        }
        
        return result;
    }

    std::vector<DeviceInfo> filterDevices(const std::function<bool(const DeviceInfo&)>& filter) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        std::vector<DeviceInfo> result;
        
        for (const auto& pair : devices_) {
            if (filter(pair.second)) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    std::unordered_map<std::string, size_t> getDeviceCountByType() const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        std::unordered_map<std::string, size_t> result;
        
        for (const auto& pair : devices_) {
            result[pair.second.deviceType]++;
        }
        
        return result;
    }

    std::unordered_map<std::string, size_t> getDeviceCountByStatus() const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        std::unordered_map<std::string, size_t> result;
        
        for (const auto& pair : devices_) {
            std::string status;
            switch (pair.second.connectionStatus) {
                case DeviceConnectionStatus::CONNECTED: status = "connected"; break;
                case DeviceConnectionStatus::DISCONNECTED: status = "disconnected"; break;
                case DeviceConnectionStatus::CONNECTING: status = "connecting"; break;
                case DeviceConnectionStatus::RECONNECTING: status = "reconnecting"; break;
                case DeviceConnectionStatus::ERROR: status = "error"; break;
            }
            result[status]++;
        }
        
        return result;
    }

    // Placeholder implementations for remaining interface methods
    bool saveDeviceTemplate(const std::string& templateName, const DeviceInfo& deviceInfo) override { return true; }
    DeviceInfo loadDeviceTemplate(const std::string& templateName) const override { return DeviceInfo{}; }
    std::vector<std::string> getAvailableTemplates() const override { return {}; }
    bool deleteDeviceTemplate(const std::string& templateName) override { return true; }
    bool exportDeviceConfiguration(const std::string& deviceId, const std::string& filePath) const override { return true; }
    bool importDeviceConfiguration(const std::string& filePath) override { return true; }
    bool exportAllDevices(const std::string& filePath) const override { return true; }
    bool importDevices(const std::string& filePath) override { return true; }

private:
    mutable std::mutex devicesMutex_;
    mutable std::mutex commandsMutex_;
    
    std::unordered_map<std::string, DeviceInfo> devices_;
    std::unordered_map<std::string, DeviceGroup> deviceGroups_;
    std::unordered_map<std::string, DeviceCommand> pendingCommands_;
    std::unordered_map<std::string, DeviceCommandResult> commandHistory_;
    
    std::chrono::seconds healthCheckInterval_{30};
    std::thread healthMonitorThread_;
    std::atomic<bool> healthMonitorRunning_{false};
    
    DeviceEventCallback deviceEventCallback_;
    ConnectionEventCallback connectionEventCallback_;
    CommandEventCallback commandEventCallback_;
    HealthEventCallback healthEventCallback_;
    
    std::string description_;

    std::string generateCommandId() const {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        ss << "cmd_";
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << dis(gen);
        }
        return ss.str();
    }

    void executeCommandAsync(const DeviceCommand& command) {
        // Simulate async command execution
        std::thread([this, command]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate processing time
            
            DeviceCommandResult result;
            result.commandId = command.commandId;
            result.deviceId = command.deviceId;
            result.success = true; // Simulate success
            result.result = "Command executed successfully";
            result.completedAt = std::chrono::system_clock::now();
            result.executionTime = std::chrono::milliseconds(100);
            
            {
                std::lock_guard<std::mutex> lock(commandsMutex_);
                pendingCommands_.erase(command.commandId);
                commandHistory_[command.commandId] = result;
            }
            
            if (commandEventCallback_) {
                commandEventCallback_(result);
            }
        }).detach();
    }

    void startHealthMonitoring() {
        healthMonitorRunning_ = true;
        healthMonitorThread_ = std::thread([this]() {
            while (healthMonitorRunning_) {
                performHealthChecks();
                std::this_thread::sleep_for(healthCheckInterval_);
            }
        });
    }

    void stopHealthMonitoring() {
        healthMonitorRunning_ = false;
        if (healthMonitorThread_.joinable()) {
            healthMonitorThread_.join();
        }
    }

    void performHealthChecks() {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        for (auto& pair : devices_) {
            auto& device = pair.second;
            
            // Simple health check based on last seen time
            auto now = std::chrono::system_clock::now();
            auto timeSinceLastSeen = std::chrono::duration_cast<std::chrono::seconds>(now - device.lastSeen);
            
            DeviceHealthStatus oldStatus = device.healthStatus;
            
            if (device.connectionStatus == DeviceConnectionStatus::CONNECTED) {
                if (timeSinceLastSeen < std::chrono::seconds(60)) {
                    device.healthStatus = DeviceHealthStatus::HEALTHY;
                } else if (timeSinceLastSeen < std::chrono::seconds(300)) {
                    device.healthStatus = DeviceHealthStatus::WARNING;
                } else {
                    device.healthStatus = DeviceHealthStatus::CRITICAL;
                }
            } else {
                device.healthStatus = DeviceHealthStatus::OFFLINE;
            }
            
            if (oldStatus != device.healthStatus && healthEventCallback_) {
                healthEventCallback_(device.deviceId, device.healthStatus, "Health status updated");
            }
        }
    }

    void updateConnectionMetrics() {
        auto connectedCount = getConnectedDevices().size();
        auto totalCount = devices_.size();
        
        updateMetric("connected_devices", std::to_string(connectedCount));
        updateMetric("disconnected_devices", std::to_string(totalCount - connectedCount));
        updateMetric("total_devices", std::to_string(totalCount));
    }
};

// DeviceServiceFactory Implementation
std::unique_ptr<core::IService> DeviceServiceFactory::createService(
    const std::string& serviceName,
    const std::unordered_map<std::string, std::string>& config) {
    
    if (serviceName == "DeviceService") {
        auto service = std::make_unique<DeviceServiceImpl>();
        service->core::BaseService::setConfiguration(config);
        return std::unique_ptr<core::IService>(static_cast<core::BaseService*>(service.release()));
    }
    
    return nullptr;
}

std::vector<std::string> DeviceServiceFactory::getSupportedServices() const {
    return {"DeviceService"};
}

bool DeviceServiceFactory::isServiceSupported(const std::string& serviceName) const {
    return serviceName == "DeviceService";
}

} // namespace services
} // namespace server
} // namespace hydrogen

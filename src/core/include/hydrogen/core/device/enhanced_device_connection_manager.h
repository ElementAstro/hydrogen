#pragma once

#include "hydrogen/core/connection/unified_connection_architecture.h"
#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>

namespace hydrogen {
namespace core {
namespace device {

/**
 * @brief Device types supported by the connection manager
 */
enum class DeviceType {
    TELESCOPE = 0,
    CAMERA,
    FOCUSER,
    FILTER_WHEEL,
    ROTATOR,
    MOUNT,
    DOME,
    WEATHER_STATION,
    GENERIC
};

/**
 * @brief Device connection state
 */
enum class DeviceConnectionState {
    DISCONNECTED = 0,
    INITIALIZING,
    CONNECTING,
    CONNECTED,
    READY,
    BUSY,
    ERROR,
    RECONNECTING,
    SHUTTING_DOWN
};

/**
 * @brief Device initialization configuration
 */
struct DeviceInitConfig {
    DeviceType deviceType = DeviceType::GENERIC;
    std::string deviceId;
    std::string manufacturer;
    std::string model;
    std::string serialNumber;
    
    // Connection settings
    connection::ConnectionConfig connectionConfig;
    
    // Device-specific settings
    std::chrono::milliseconds initializationTimeout{30000};
    std::chrono::milliseconds commandTimeout{5000};
    bool enableStatusMonitoring = true;
    std::chrono::seconds statusUpdateInterval{1};
    
    // Validation settings
    bool validateOnConnect = true;
    bool performSelfTest = false;
    std::chrono::milliseconds selfTestTimeout{10000};
};

/**
 * @brief Device status information
 */
struct DeviceStatus {
    DeviceConnectionState connectionState = DeviceConnectionState::DISCONNECTED;
    std::string deviceId;
    std::string lastError;
    std::chrono::system_clock::time_point lastUpdate;
    
    // Health metrics
    bool isHealthy = false;
    double temperature = 0.0;
    double voltage = 0.0;
    std::string firmwareVersion;
    
    // Performance metrics
    uint64_t commandsExecuted = 0;
    uint64_t errorsEncountered = 0;
    std::chrono::milliseconds averageResponseTime{0};
};

/**
 * @brief Device command interface
 */
struct DeviceCommand {
    std::string commandId;
    std::string command;
    std::string parameters;
    std::chrono::milliseconds timeout{5000};
    bool requiresResponse = true;
    int priority = 0; // Higher values = higher priority
};

/**
 * @brief Device response interface
 */
struct DeviceResponse {
    std::string commandId;
    bool success = false;
    std::string response;
    std::string errorMessage;
    std::chrono::milliseconds executionTime{0};
};

/**
 * @brief Callback types for device events
 */
using DeviceStateCallback = std::function<void(DeviceConnectionState, const std::string&)>;
using DeviceStatusCallback = std::function<void(const DeviceStatus&)>;
using DeviceCommandCallback = std::function<void(const DeviceResponse&)>;
using DeviceErrorCallback = std::function<void(const std::string&, int)>;

/**
 * @brief Enhanced device connection manager
 */
class EnhancedDeviceConnectionManager {
public:
    explicit EnhancedDeviceConnectionManager(const DeviceInitConfig& config);
    ~EnhancedDeviceConnectionManager();
    
    // Connection management
    bool initialize();
    bool connect();
    void disconnect();
    bool isConnected() const;
    bool isReady() const;
    
    // Device operations
    bool sendCommand(const DeviceCommand& command);
    DeviceResponse sendCommandSync(const DeviceCommand& command);
    bool abortCommand(const std::string& commandId);
    void abortAllCommands();
    
    // Status and monitoring
    DeviceStatus getStatus() const;
    DeviceConnectionState getConnectionState() const;
    connection::ConnectionStatistics getConnectionStatistics() const;
    
    // Configuration
    void updateConfig(const DeviceInitConfig& config);
    DeviceInitConfig getConfig() const;
    
    // Callbacks
    void setStateCallback(DeviceStateCallback callback);
    void setStatusCallback(DeviceStatusCallback callback);
    void setCommandCallback(DeviceCommandCallback callback);
    void setErrorCallback(DeviceErrorCallback callback);
    
    // Health monitoring
    void enableHealthMonitoring(bool enable);
    bool isHealthy() const;
    void performSelfTest();
    
    // Device-specific operations
    bool validateDevice();
    bool resetDevice();
    std::string getDeviceInfo() const;
    std::vector<std::string> getSupportedCommands() const;

private:
    // Initialization methods
    bool initializeDevice();
    bool performDeviceValidation();
    bool performDeviceSelfTest();
    void setupDeviceSpecificSettings();
    
    // Connection management
    void handleConnectionStateChange(connection::ConnectionState state, const std::string& error);
    void handleConnectionMessage(const std::string& message);
    void handleConnectionError(const std::string& error, int code);
    
    // Command processing
    void processCommandQueue();
    void handleCommandResponse(const std::string& response);
    DeviceResponse parseResponse(const std::string& response, const DeviceCommand& command);
    
    // Status monitoring
    void startStatusMonitoring();
    void stopStatusMonitoring();
    void updateDeviceStatus();
    void statusMonitoringLoop();
    
    // Device-specific handlers
    void handleTelescopeSpecific();
    void handleCameraSpecific();
    void handleFocuserSpecific();
    void handleFilterWheelSpecific();
    void handleRotatorSpecific();
    
    // State management
    void updateConnectionState(DeviceConnectionState newState, const std::string& error = "");
    void notifyStateChange(DeviceConnectionState state, const std::string& error);
    void notifyStatusUpdate(const DeviceStatus& status);
    void notifyCommandComplete(const DeviceResponse& response);
    void notifyError(const std::string& error, int code = 0);
    
    // Configuration and state
    DeviceInitConfig config_;
    std::atomic<DeviceConnectionState> connectionState_{DeviceConnectionState::DISCONNECTED};
    DeviceStatus currentStatus_;
    mutable std::mutex statusMutex_;
    
    // Connection management
    std::unique_ptr<connection::UnifiedConnectionManager> connectionManager_;
    
    // Command processing
    std::queue<DeviceCommand> commandQueue_;
    std::unordered_map<std::string, DeviceCommand> pendingCommands_;
    std::mutex commandMutex_;
    std::condition_variable commandCondition_;
    std::thread commandProcessingThread_;
    
    // Status monitoring
    std::atomic<bool> statusMonitoringEnabled_{false};
    std::thread statusMonitoringThread_;
    
    // Callbacks
    DeviceStateCallback stateCallback_;
    DeviceStatusCallback statusCallback_;
    DeviceCommandCallback commandCallback_;
    DeviceErrorCallback errorCallback_;
    mutable std::mutex callbackMutex_;
    
    // Control flags
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    
    // Performance tracking
    std::chrono::system_clock::time_point lastCommandTime_;
    std::vector<std::chrono::milliseconds> responseTimeHistory_;
    mutable std::mutex performanceMutex_;
};

/**
 * @brief Factory for creating device-specific connection managers
 */
class DeviceConnectionManagerFactory {
public:
    static std::unique_ptr<EnhancedDeviceConnectionManager> createManager(
        DeviceType deviceType, 
        const std::string& deviceId,
        const connection::ConnectionConfig& connectionConfig);
    
    static DeviceInitConfig createDefaultConfig(
        DeviceType deviceType,
        const std::string& deviceId);
    
    static std::vector<DeviceType> getSupportedDeviceTypes();
    static std::string getDeviceTypeName(DeviceType deviceType);
    static DeviceType getDeviceTypeFromName(const std::string& name);
};

/**
 * @brief Device registry for managing multiple devices
 */
class DeviceRegistry {
public:
    DeviceRegistry();
    ~DeviceRegistry();
    
    // Device management
    bool registerDevice(std::unique_ptr<EnhancedDeviceConnectionManager> manager);
    bool unregisterDevice(const std::string& deviceId);
    EnhancedDeviceConnectionManager* getDevice(const std::string& deviceId);
    
    // Bulk operations
    void connectAllDevices();
    void disconnectAllDevices();
    std::vector<std::string> getConnectedDevices() const;
    std::vector<std::string> getAllDevices() const;
    
    // Status monitoring
    std::vector<DeviceStatus> getAllDeviceStatuses() const;
    bool areAllDevicesHealthy() const;
    
    // Event handling
    void setGlobalStateCallback(std::function<void(const std::string&, DeviceConnectionState)> callback);
    void setGlobalErrorCallback(std::function<void(const std::string&, const std::string&)> callback);

private:
    std::unordered_map<std::string, std::unique_ptr<EnhancedDeviceConnectionManager>> devices_;
    mutable std::mutex devicesMutex_;
    
    std::function<void(const std::string&, DeviceConnectionState)> globalStateCallback_;
    std::function<void(const std::string&, const std::string&)> globalErrorCallback_;
    mutable std::mutex callbackMutex_;
};

} // namespace device
} // namespace core
} // namespace hydrogen

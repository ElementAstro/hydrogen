#pragma once

#include "multi_protocol_communication_manager.h"
#include <astrocomm/core/device_health.h>
#include <astrocomm/core/performance_metrics.h>
#include <astrocomm/core/enhanced_error_recovery.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <thread>
#include <chrono>

namespace astrocomm {
namespace device {
namespace core {

using json = nlohmann::json;
using astrocomm::core::CommunicationProtocol;
using astrocomm::core::DeviceHealthStatus;
using astrocomm::core::DeviceMetrics;
using astrocomm::core::PerformanceMeasurement;
using astrocomm::core::MetricType;

/**
 * @brief Enhanced device base class with multi-protocol communication support
 * 
 * This class provides a modern, feature-rich foundation for device implementations:
 * - Multi-protocol communication (WebSocket, TCP, stdio)
 * - Health monitoring and diagnostics
 * - Performance metrics collection
 * - Automatic error recovery
 * - Self-healing capabilities
 * - Protocol selection and failover
 */
class EnhancedDeviceBase {
public:
    /**
     * @brief Device configuration structure
     */
    struct DeviceConfiguration {
        std::string deviceId;
        std::string deviceType;
        std::string manufacturer;
        std::string model;
        std::string firmwareVersion = "1.0.0";
        
        // Communication settings
        std::vector<MultiProtocolCommunicationManager::ProtocolConfiguration> protocols;
        CommunicationProtocol primaryProtocol = CommunicationProtocol::WEBSOCKET;
        std::vector<CommunicationProtocol> fallbackProtocols;
        
        // Health monitoring settings
        bool enableHealthMonitoring = true;
        std::chrono::milliseconds healthCheckInterval{5000};
        std::chrono::milliseconds metricsCollectionInterval{1000};
        
        // Error recovery settings
        bool enableAutoRecovery = true;
        int maxRecoveryAttempts = 3;
        std::chrono::milliseconds recoveryDelay{1000};
        
        // Performance settings
        bool enablePerformanceMetrics = true;
        size_t metricsHistorySize = 1000;
        bool enablePredictiveAnalysis = true;
    };

    /**
     * @brief Device capability enumeration
     */
    enum class DeviceCapability {
        CONNECT,
        DISCONNECT,
        GET_PROPERTIES,
        SET_PROPERTIES,
        EXECUTE_COMMANDS,
        STREAM_DATA,
        HEALTH_MONITORING,
        SELF_DIAGNOSTICS,
        AUTO_RECOVERY,
        PROTOCOL_SWITCHING,
        PERFORMANCE_METRICS
    };

    explicit EnhancedDeviceBase(const DeviceConfiguration& config);
    virtual ~EnhancedDeviceBase();

    // Core device lifecycle
    virtual bool initialize();
    virtual bool start();
    virtual void stop();
    virtual bool isRunning() const;

    // Connection management
    virtual bool connect();
    virtual bool connect(CommunicationProtocol protocol);
    virtual void disconnect();
    virtual void disconnect(CommunicationProtocol protocol);
    virtual bool isConnected() const;
    virtual bool isConnected(CommunicationProtocol protocol) const;

    // Protocol management
    virtual bool addProtocol(const MultiProtocolCommunicationManager::ProtocolConfiguration& protocolConfig);
    virtual bool removeProtocol(CommunicationProtocol protocol);
    virtual void setPrimaryProtocol(CommunicationProtocol protocol);
    virtual CommunicationProtocol getPrimaryProtocol() const;
    virtual std::vector<CommunicationProtocol> getActiveProtocols() const;

    // Device information
    virtual std::string getDeviceId() const;
    virtual std::string getDeviceType() const;
    virtual json getDeviceInfo() const;
    virtual std::vector<DeviceCapability> getCapabilities() const;

    // Property management
    virtual bool setProperty(const std::string& name, const json& value);
    virtual json getProperty(const std::string& name) const;
    virtual json getAllProperties() const;
    virtual bool hasProperty(const std::string& name) const;

    // Command handling
    using CommandHandler = std::function<json(const json& parameters)>;
    virtual bool registerCommand(const std::string& command, CommandHandler handler);
    virtual bool unregisterCommand(const std::string& command);
    virtual json executeCommand(const std::string& command, const json& parameters = json::object());
    virtual std::vector<std::string> getAvailableCommands() const;

    // Health monitoring
    virtual DeviceHealthStatus getHealthStatus() const;
    virtual DeviceMetrics getMetrics() const;
    virtual json getHealthReport() const;
    virtual bool performSelfDiagnostics();
    virtual bool performHealthCheck();

    // Performance monitoring
    virtual void recordMetric(MetricType type, const std::string& name, double value, const std::string& unit = "");
    virtual std::vector<PerformanceMeasurement> getPerformanceHistory(const std::string& metricName) const;
    virtual json getPerformanceReport() const;

    // Error handling and recovery
    virtual bool handleError(const std::string& errorCode, const std::string& errorMessage);
    virtual bool attemptRecovery(const std::string& errorCode);
    virtual void enableAutoRecovery(bool enable);

    // Event handling
    using PropertyChangeHandler = std::function<void(const std::string& property, const json& oldValue, const json& newValue)>;
    using HealthChangeHandler = std::function<void(DeviceHealthStatus oldStatus, DeviceHealthStatus newStatus)>;
    using ErrorHandler = std::function<void(const std::string& errorCode, const std::string& errorMessage)>;
    using MessageHandler = std::function<void(const std::string& message, CommunicationProtocol protocol)>;

    virtual void setPropertyChangeHandler(PropertyChangeHandler handler);
    virtual void setHealthChangeHandler(HealthChangeHandler handler);
    virtual void setErrorHandler(ErrorHandler handler);
    virtual void setMessageHandler(MessageHandler handler);

    // Configuration management
    virtual bool updateConfiguration(const DeviceConfiguration& config);
    virtual DeviceConfiguration getConfiguration() const;
    virtual bool saveConfiguration(const std::string& filename = "") const;
    virtual bool loadConfiguration(const std::string& filename = "");

protected:
    // Virtual methods for derived classes to implement
    virtual bool initializeDevice() = 0;
    virtual bool startDevice() = 0;
    virtual void stopDevice() = 0;
    virtual bool connectDevice() { return true; }
    virtual void disconnectDevice() {}
    virtual json getDeviceSpecificInfo() const { return json::object(); }
    virtual std::vector<DeviceCapability> getDeviceSpecificCapabilities() const { return {}; }
    virtual bool performDeviceSpecificDiagnostics() { return true; }
    virtual bool performDeviceSpecificHealthCheck() { return true; }

    // Utility methods for derived classes
    virtual void notifyPropertyChange(const std::string& property, const json& oldValue, const json& newValue);
    virtual void notifyHealthChange(DeviceHealthStatus oldStatus, DeviceHealthStatus newStatus);
    virtual void notifyError(const std::string& errorCode, const std::string& errorMessage);
    virtual void updateHealthStatus(DeviceHealthStatus status);
    virtual void updateMetrics();

private:
    DeviceConfiguration config_;
    std::unique_ptr<MultiProtocolCommunicationManager> communicationManager_;
    
    // Device state
    std::atomic<bool> initialized_;
    std::atomic<bool> running_;
    std::atomic<DeviceHealthStatus> healthStatus_;
    
    // Properties and commands
    mutable std::mutex propertiesMutex_;
    std::unordered_map<std::string, json> properties_;
    
    mutable std::mutex commandsMutex_;
    std::unordered_map<std::string, CommandHandler> commands_;
    
    // Health monitoring
    mutable std::mutex metricsMutex_;
    DeviceMetrics metrics_;
    std::vector<PerformanceMeasurement> performanceHistory_;
    
    // Threading
    std::thread healthMonitorThread_;
    std::thread metricsCollectionThread_;
    std::atomic<bool> monitoringActive_;
    
    // Event handlers
    PropertyChangeHandler propertyChangeHandler_;
    HealthChangeHandler healthChangeHandler_;
    ErrorHandler errorHandler_;
    MessageHandler messageHandler_;
    
    // Error recovery
    std::unique_ptr<astrocomm::core::EnhancedErrorRecoveryManager> errorRecoveryManager_;
    std::atomic<int> recoveryAttempts_;
    
    // Internal methods
    void initializeCommunication();
    void setupEventHandlers();
    void startHealthMonitoring();
    void stopHealthMonitoring();
    void startMetricsCollection();
    void stopMetricsCollection();
    void healthMonitorLoop();
    void metricsCollectionLoop();
    void handleCommunicationMessage(const std::string& message, CommunicationProtocol protocol);
    void handleConnectionStateChange(ConnectionState state, CommunicationProtocol protocol, const std::string& error);
    bool validateProperty(const std::string& name, const json& value) const;
    void initializeDefaultProperties();
    void initializeDefaultCommands();
    std::string getConfigurationFilePath(const std::string& filename) const;
    double getCurrentMemoryUsage() const;
    double getCurrentCpuUsage() const;
};

/**
 * @brief Factory for creating enhanced device instances
 */
class EnhancedDeviceFactory {
public:
    template<typename DeviceType>
    static std::unique_ptr<DeviceType> createDevice(const EnhancedDeviceBase::DeviceConfiguration& config) {
        static_assert(std::is_base_of_v<EnhancedDeviceBase, DeviceType>, 
                     "DeviceType must inherit from EnhancedDeviceBase");
        return std::make_unique<DeviceType>(config);
    }
    
    static EnhancedDeviceBase::DeviceConfiguration createDefaultConfiguration(
        const std::string& deviceId, const std::string& deviceType);
    
    static EnhancedDeviceBase::DeviceConfiguration createWebSocketConfiguration(
        const std::string& deviceId, const std::string& deviceType, 
        const std::string& host, uint16_t port);
    
    static EnhancedDeviceBase::DeviceConfiguration createTcpConfiguration(
        const std::string& deviceId, const std::string& deviceType,
        const std::string& host, uint16_t port, bool isServer = false);
    
    static EnhancedDeviceBase::DeviceConfiguration createMultiProtocolConfiguration(
        const std::string& deviceId, const std::string& deviceType,
        const std::vector<MultiProtocolCommunicationManager::ProtocolConfiguration>& protocols);
};

} // namespace core
} // namespace device
} // namespace astrocomm

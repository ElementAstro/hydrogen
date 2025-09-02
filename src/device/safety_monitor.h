#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"
#include "core/async_operation.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include <functional>
#include <chrono>

namespace hydrogen {
namespace device {

/**
 * @brief Safety monitor device implementation following ASCOM ISafetyMonitorV3 standard
 * 
 * Provides comprehensive safety monitoring and emergency shutdown coordination
 * using the modern ModernDeviceBase architecture.
 */
class SafetyMonitor : public core::ModernDeviceBase,
                      public interfaces::ISafetyMonitor,
                      public interfaces::IStateful,
                      public core::ASCOMAsyncMixin {
public:
    /**
     * @brief Safety condition callback function type
     */
    using SafetyCallback = std::function<void(bool isSafe, const std::string& reason)>;
    
    /**
     * @brief Emergency shutdown callback function type
     */
    using EmergencyCallback = std::function<void(const std::string& reason)>;

    /**
     * @brief Constructor
     * @param deviceId Device identifier
     * @param manufacturer Manufacturer name
     * @param model Model name
     */
    SafetyMonitor(const std::string& deviceId,
                  const std::string& manufacturer = "Generic",
                  const std::string& model = "SafetyMonitor");

    /**
     * @brief Virtual destructor
     */
    virtual ~SafetyMonitor();

    /**
     * @brief Get device type name
     */
    static std::string getDeviceTypeName() { return "SAFETY_MONITOR"; }

    /**
     * @brief Get supported manufacturers
     */
    static std::vector<std::string> getSupportedManufacturers() {
        return {"Generic", "Lunatico", "PegasusAstro", "AAG", "Boltwood", "Custom"};
    }

    // ISafetyMonitor interface implementation
    bool getIsSafe() const override;

    // IStateful interface implementation
    bool setProperty(const std::string& property, const json& value) override;
    json getProperty(const std::string& property) const override;
    json getAllProperties() const override;
    std::vector<std::string> getCapabilities() const override;

    // Additional safety monitor-specific methods
    void addSafetyCondition(const std::string& name, std::function<bool()> condition, const std::string& description = "");
    void removeSafetyCondition(const std::string& name);
    void setSafetyConditionEnabled(const std::string& name, bool enabled);
    bool isSafetyConditionEnabled(const std::string& name) const;
    
    // Callback management
    void setSafetyCallback(SafetyCallback callback);
    void setEmergencyCallback(EmergencyCallback callback);
    void addEmergencyShutdownDevice(const std::string& deviceId);
    void removeEmergencyShutdownDevice(const std::string& deviceId);
    
    // Safety status and alerts
    std::vector<std::string> getUnsafeConditions() const;
    std::vector<std::string> getActiveAlerts() const;
    std::chrono::system_clock::time_point getLastSafeTime() const;
    std::chrono::system_clock::time_point getLastUnsafeTime() const;
    int getUnsafeCount() const;
    
    // Configuration
    void setMonitoringInterval(int intervalSeconds);
    void setEmergencyDelay(int delaySeconds);
    void setAutoRecovery(bool enabled);
    void setAlertThresholds(const json& thresholds);
    void setSafetyLimits(const json& limits);
    
    // Manual control
    void triggerEmergencyShutdown(const std::string& reason);
    void acknowledgeUnsafeCondition();
    void resetSafetyMonitor();
    void testEmergencyProcedures();

protected:
    // ModernDeviceBase overrides
    bool initializeDevice() override;
    bool startDevice() override;
    void stopDevice() override;
    bool handleDeviceCommand(const std::string& command, const json& parameters, json& result) override;
    void updateDevice() override;

private:
    // Safety condition structure
    struct SafetyCondition {
        std::string name;
        std::string description;
        std::function<bool()> condition;
        std::atomic<bool> enabled;
        std::atomic<bool> lastResult;
        std::chrono::system_clock::time_point lastCheck;
        int failureCount;
        
        SafetyCondition(const std::string& n, std::function<bool()> c, const std::string& d = "")
            : name(n), description(d), condition(c), enabled(true), lastResult(true), failureCount(0) {}
    };

    // Control threads
    void safetyMonitorThread();
    void emergencyResponseThread();
    void alertProcessingThread();

    // Safety evaluation methods
    void evaluateSafetyConditions();
    void processUnsafeCondition(const std::string& conditionName);
    void processSafeCondition(const std::string& conditionName);
    void executeEmergencyShutdown(const std::string& reason);
    void notifyDevicesOfEmergency(const std::string& reason);
    
    // Built-in safety conditions
    bool checkSystemHealth();
    bool checkCommunication();
    bool checkPowerStatus();
    bool checkTemperatureLimits();
    bool checkWeatherConditions();
    bool checkDiskSpace();
    bool checkMemoryUsage();
    
    // Utility methods
    void updateSafetyStatus();
    void logSafetyEvent(const std::string& event, const std::string& details = "");
    void sendAlert(const std::string& alert, const std::string& severity = "WARNING");
    bool isInEmergencyState() const;

    // Device state
    std::atomic<bool> isSafe_;
    std::atomic<bool> emergencyShutdownActive_;
    std::atomic<bool> autoRecoveryEnabled_;
    std::atomic<int> unsafeCount_;
    std::chrono::system_clock::time_point lastSafeTime_;
    std::chrono::system_clock::time_point lastUnsafeTime_;

    // Configuration
    int monitoringInterval_;  // seconds
    int emergencyDelay_;      // seconds before emergency shutdown
    json alertThresholds_;
    json safetyLimits_;

    // Safety conditions
    std::vector<std::unique_ptr<SafetyCondition>> safetyConditions_;
    std::mutex conditionsMutex_;
    
    // Callbacks
    SafetyCallback safetyCallback_;
    EmergencyCallback emergencyCallback_;
    std::mutex callbackMutex_;
    
    // Emergency shutdown devices
    std::vector<std::string> emergencyDevices_;
    std::mutex emergencyDevicesMutex_;
    
    // Alerts and logging
    std::vector<std::string> activeAlerts_;
    std::vector<std::string> unsafeConditions_;
    std::mutex alertsMutex_;
    
    // Threading
    std::thread monitorThread_;
    std::thread emergencyThread_;
    std::thread alertThread_;
    std::atomic<bool> monitorRunning_;
    std::atomic<bool> emergencyRunning_;
    std::atomic<bool> alertRunning_;
    std::condition_variable monitorCV_;
    std::condition_variable emergencyCV_;
    std::condition_variable alertCV_;
    std::mutex monitorMutex_;
    std::mutex emergencyMutex_;
    std::mutex alertMutex_;

    // Emergency state management
    std::atomic<bool> emergencyTriggered_;
    std::atomic<bool> acknowledgmentRequired_;
    std::string lastEmergencyReason_;
    std::chrono::system_clock::time_point emergencyTriggerTime_;

    // Statistics
    std::atomic<int> totalChecks_;
    std::atomic<int> failedChecks_;
    std::atomic<int> emergencyCount_;

    // Manufacturer-specific initialization
    void initializeManufacturerSpecific();
    void initializeGeneric();
    void initializeLunatico();
    void initializePegasusAstro();
    void initializeAAG();
    void initializeBoltwood();
    void initializeCustom();
    
    // Initialize built-in safety conditions
    void initializeBuiltInConditions();
};

/**
 * @brief Factory function for creating modern safety monitor instances
 */
std::unique_ptr<SafetyMonitor> createModernSafetyMonitor(const std::string& deviceId,
                                                         const std::string& manufacturer = "Generic",
                                                         const std::string& model = "SafetyMonitor");

} // namespace device
} // namespace hydrogen

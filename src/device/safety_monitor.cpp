#include "safety_monitor.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace hydrogen {
namespace device {

SafetyMonitor::SafetyMonitor(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "SAFETY_MONITOR", manufacturer, model)
    , isSafe_(true)
    , emergencyShutdownActive_(false)
    , autoRecoveryEnabled_(true)
    , unsafeCount_(0)
    , lastSafeTime_(std::chrono::system_clock::now())
    , lastUnsafeTime_(std::chrono::system_clock::now())
    , monitoringInterval_(5)  // 5 seconds default
    , emergencyDelay_(30)     // 30 seconds delay before emergency shutdown
    , monitorRunning_(false)
    , emergencyRunning_(false)
    , alertRunning_(false)
    , emergencyTriggered_(false)
    , acknowledgmentRequired_(false)
    , totalChecks_(0)
    , failedChecks_(0)
    , emergencyCount_(0) {
    
    // Set default alert thresholds
    alertThresholds_ = json{
        {"consecutiveFailures", 3},
        {"emergencyDelaySeconds", 30},
        {"maxUnsafeTime", 300}  // 5 minutes
    };
    
    // Set default safety limits
    safetyLimits_ = json{
        {"maxTemperature", 50.0},
        {"minTemperature", -20.0},
        {"maxHumidity", 85.0},
        {"minDiskSpaceGB", 1.0},
        {"maxMemoryUsagePercent", 90.0}
    };
    
    initializeManufacturerSpecific();
    initializeBuiltInConditions();
}

SafetyMonitor::~SafetyMonitor() {
    stopDevice();
}

bool SafetyMonitor::initializeDevice() {
    SPDLOG_INFO("Initializing safety monitor device {}", getDeviceId());
    
    // Initialize properties
    setProperty("isSafe", isSafe_.load());
    setProperty("emergencyShutdownActive", emergencyShutdownActive_.load());
    setProperty("autoRecoveryEnabled", autoRecoveryEnabled_.load());
    setProperty("unsafeCount", unsafeCount_.load());
    setProperty("monitoringInterval", monitoringInterval_);
    setProperty("emergencyDelay", emergencyDelay_);
    setProperty("totalChecks", totalChecks_.load());
    setProperty("failedChecks", failedChecks_.load());
    setProperty("emergencyCount", emergencyCount_.load());
    setProperty("alertThresholds", alertThresholds_);
    setProperty("safetyLimits", safetyLimits_);
    
    // Initialize safety condition states
    {
        std::lock_guard<std::mutex> lock(conditionsMutex_);
        for (const auto& condition : safetyConditions_) {
            setProperty("condition_" + condition->name + "_enabled", condition->enabled.load());
            setProperty("condition_" + condition->name + "_description", condition->description);
        }
    }
    
    return true;
}

bool SafetyMonitor::startDevice() {
    SPDLOG_INFO("Starting safety monitor device {}", getDeviceId());
    
    // Start monitoring thread
    monitorRunning_ = true;
    monitorThread_ = std::thread(&SafetyMonitor::safetyMonitorThread, this);
    
    // Start emergency response thread
    emergencyRunning_ = true;
    emergencyThread_ = std::thread(&SafetyMonitor::emergencyResponseThread, this);
    
    // Start alert processing thread
    alertRunning_ = true;
    alertThread_ = std::thread(&SafetyMonitor::alertProcessingThread, this);
    
    return true;
}

void SafetyMonitor::stopDevice() {
    SPDLOG_INFO("Stopping safety monitor device {}", getDeviceId());
    
    // Stop all threads
    monitorRunning_ = false;
    emergencyRunning_ = false;
    alertRunning_ = false;
    
    monitorCV_.notify_all();
    emergencyCV_.notify_all();
    alertCV_.notify_all();
    
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    if (emergencyThread_.joinable()) {
        emergencyThread_.join();
    }
    if (alertThread_.joinable()) {
        alertThread_.join();
    }
}

// ISafetyMonitor interface implementation
bool SafetyMonitor::getIsSafe() const {
    return isSafe_.load();
}

// Additional methods
void SafetyMonitor::addSafetyCondition(const std::string& name, std::function<bool()> condition, const std::string& description) {
    std::lock_guard<std::mutex> lock(conditionsMutex_);
    
    // Check if condition already exists
    auto it = std::find_if(safetyConditions_.begin(), safetyConditions_.end(),
                          [&name](const auto& cond) { return cond->name == name; });
    
    if (it != safetyConditions_.end()) {
        SPDLOG_WARN("Safety monitor {} condition '{}' already exists, updating", getDeviceId(), name);
        (*it)->condition = condition;
        (*it)->description = description;
    } else {
        safetyConditions_.push_back(std::make_unique<SafetyCondition>(name, condition, description));
        setProperty("condition_" + name + "_enabled", true);
        setProperty("condition_" + name + "_description", description);
        SPDLOG_INFO("Safety monitor {} added condition '{}'", getDeviceId(), name);
    }
}

void SafetyMonitor::removeSafetyCondition(const std::string& name) {
    std::lock_guard<std::mutex> lock(conditionsMutex_);
    
    auto it = std::find_if(safetyConditions_.begin(), safetyConditions_.end(),
                          [&name](const auto& cond) { return cond->name == name; });
    
    if (it != safetyConditions_.end()) {
        safetyConditions_.erase(it);
        SPDLOG_INFO("Safety monitor {} removed condition '{}'", getDeviceId(), name);
    }
}

void SafetyMonitor::setSafetyConditionEnabled(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(conditionsMutex_);
    
    auto it = std::find_if(safetyConditions_.begin(), safetyConditions_.end(),
                          [&name](const auto& cond) { return cond->name == name; });
    
    if (it != safetyConditions_.end()) {
        (*it)->enabled = enabled;
        setProperty("condition_" + name + "_enabled", enabled);
        SPDLOG_DEBUG("Safety monitor {} condition '{}' {}", getDeviceId(), name, enabled ? "enabled" : "disabled");
    }
}

bool SafetyMonitor::isSafetyConditionEnabled(const std::string& name) const {
    std::lock_guard<std::mutex> lock(conditionsMutex_);
    
    auto it = std::find_if(safetyConditions_.begin(), safetyConditions_.end(),
                          [&name](const auto& cond) { return cond->name == name; });
    
    return (it != safetyConditions_.end()) ? (*it)->enabled.load() : false;
}

void SafetyMonitor::setSafetyCallback(SafetyCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    safetyCallback_ = callback;
}

void SafetyMonitor::setEmergencyCallback(EmergencyCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    emergencyCallback_ = callback;
}

void SafetyMonitor::addEmergencyShutdownDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(emergencyDevicesMutex_);
    
    auto it = std::find(emergencyDevices_.begin(), emergencyDevices_.end(), deviceId);
    if (it == emergencyDevices_.end()) {
        emergencyDevices_.push_back(deviceId);
        SPDLOG_INFO("Safety monitor {} added emergency shutdown device '{}'", getDeviceId(), deviceId);
    }
}

void SafetyMonitor::removeEmergencyShutdownDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(emergencyDevicesMutex_);
    
    auto it = std::find(emergencyDevices_.begin(), emergencyDevices_.end(), deviceId);
    if (it != emergencyDevices_.end()) {
        emergencyDevices_.erase(it);
        SPDLOG_INFO("Safety monitor {} removed emergency shutdown device '{}'", getDeviceId(), deviceId);
    }
}

std::vector<std::string> SafetyMonitor::getUnsafeConditions() const {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    return unsafeConditions_;
}

std::vector<std::string> SafetyMonitor::getActiveAlerts() const {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    return activeAlerts_;
}

std::chrono::system_clock::time_point SafetyMonitor::getLastSafeTime() const {
    return lastSafeTime_;
}

std::chrono::system_clock::time_point SafetyMonitor::getLastUnsafeTime() const {
    return lastUnsafeTime_;
}

int SafetyMonitor::getUnsafeCount() const {
    return unsafeCount_.load();
}

void SafetyMonitor::setMonitoringInterval(int intervalSeconds) {
    if (intervalSeconds > 0) {
        monitoringInterval_ = intervalSeconds;
        setProperty("monitoringInterval", intervalSeconds);
        monitorCV_.notify_one();
    }
}

void SafetyMonitor::setEmergencyDelay(int delaySeconds) {
    if (delaySeconds >= 0) {
        emergencyDelay_ = delaySeconds;
        setProperty("emergencyDelay", delaySeconds);
    }
}

void SafetyMonitor::setAutoRecovery(bool enabled) {
    autoRecoveryEnabled_ = enabled;
    setProperty("autoRecoveryEnabled", enabled);
}

void SafetyMonitor::setAlertThresholds(const json& thresholds) {
    alertThresholds_ = thresholds;
    setProperty("alertThresholds", thresholds);
}

void SafetyMonitor::setSafetyLimits(const json& limits) {
    safetyLimits_ = limits;
    setProperty("safetyLimits", limits);
}

void SafetyMonitor::triggerEmergencyShutdown(const std::string& reason) {
    SPDLOG_CRITICAL("Safety monitor {} emergency shutdown triggered: {}", getDeviceId(), reason);
    
    emergencyTriggered_ = true;
    lastEmergencyReason_ = reason;
    emergencyTriggerTime_ = std::chrono::system_clock::now();
    emergencyCount_++;
    
    setProperty("emergencyCount", emergencyCount_.load());
    emergencyCV_.notify_one();
    
    executeEmergencyShutdown(reason);
}

void SafetyMonitor::acknowledgeUnsafeCondition() {
    acknowledgmentRequired_ = false;
    SPDLOG_INFO("Safety monitor {} unsafe condition acknowledged", getDeviceId());
}

void SafetyMonitor::resetSafetyMonitor() {
    emergencyShutdownActive_ = false;
    emergencyTriggered_ = false;
    acknowledgmentRequired_ = false;
    unsafeCount_ = 0;
    
    {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        activeAlerts_.clear();
        unsafeConditions_.clear();
    }
    
    updateSafetyStatus();
    SPDLOG_INFO("Safety monitor {} reset", getDeviceId());
}

void SafetyMonitor::testEmergencyProcedures() {
    SPDLOG_INFO("Safety monitor {} testing emergency procedures", getDeviceId());
    
    // Simulate emergency without actually shutting down devices
    std::string testReason = "Emergency procedure test";
    logSafetyEvent("EMERGENCY_TEST", testReason);
    sendAlert("Emergency test completed", "INFO");
}

// Control threads
void SafetyMonitor::safetyMonitorThread() {
    while (monitorRunning_.load()) {
        evaluateSafetyConditions();
        updateSafetyStatus();

        totalChecks_++;
        setProperty("totalChecks", totalChecks_.load());

        std::unique_lock<std::mutex> lock(monitorMutex_);
        monitorCV_.wait_for(lock, std::chrono::seconds(monitoringInterval_),
                           [this] { return !monitorRunning_.load(); });
    }
}

void SafetyMonitor::emergencyResponseThread() {
    while (emergencyRunning_.load()) {
        std::unique_lock<std::mutex> lock(emergencyMutex_);
        emergencyCV_.wait(lock, [this] { return !emergencyRunning_.load() || emergencyTriggered_.load(); });

        if (!emergencyRunning_.load()) {
            break;
        }

        if (emergencyTriggered_.load()) {
            // Wait for emergency delay before executing shutdown
            std::this_thread::sleep_for(std::chrono::seconds(emergencyDelay_));

            if (emergencyTriggered_.load()) {  // Still triggered after delay
                executeEmergencyShutdown(lastEmergencyReason_);
                emergencyTriggered_ = false;
            }
        }
    }
}

void SafetyMonitor::alertProcessingThread() {
    while (alertRunning_.load()) {
        // Process alerts and notifications
        {
            std::lock_guard<std::mutex> lock(alertsMutex_);
            if (!activeAlerts_.empty()) {
                for (const auto& alert : activeAlerts_) {
                    SPDLOG_WARN("Safety monitor {} alert: {}", getDeviceId(), alert);
                }
            }
        }

        std::unique_lock<std::mutex> lock(alertMutex_);
        alertCV_.wait_for(lock, std::chrono::seconds(10),
                         [this] { return !alertRunning_.load(); });
    }
}

// Safety evaluation methods
void SafetyMonitor::evaluateSafetyConditions() {
    bool overallSafe = true;
    std::vector<std::string> currentUnsafeConditions;

    {
        std::lock_guard<std::mutex> lock(conditionsMutex_);
        for (auto& condition : safetyConditions_) {
            if (!condition->enabled.load()) {
                continue;
            }

            try {
                bool result = condition->condition();
                condition->lastCheck = std::chrono::system_clock::now();

                if (!result) {
                    overallSafe = false;
                    currentUnsafeConditions.push_back(condition->name);
                    condition->failureCount++;

                    if (condition->lastResult.load()) {
                        // Transition from safe to unsafe
                        processUnsafeCondition(condition->name);
                    }
                } else {
                    condition->failureCount = 0;

                    if (!condition->lastResult.load()) {
                        // Transition from unsafe to safe
                        processSafeCondition(condition->name);
                    }
                }

                condition->lastResult = result;

            } catch (const std::exception& e) {
                SPDLOG_ERROR("Safety monitor {} condition '{}' evaluation failed: {}",
                           getDeviceId(), condition->name, e.what());
                overallSafe = false;
                currentUnsafeConditions.push_back(condition->name + " (ERROR)");
                failedChecks_++;
            }
        }
    }

    // Update unsafe conditions list
    {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        unsafeConditions_ = currentUnsafeConditions;
    }

    // Update overall safety status
    bool previousSafe = isSafe_.load();
    isSafe_ = overallSafe;

    if (overallSafe && !previousSafe) {
        // Transition to safe
        lastSafeTime_ = std::chrono::system_clock::now();
        logSafetyEvent("SAFE_CONDITION_RESTORED");

        if (autoRecoveryEnabled_.load() && emergencyShutdownActive_.load()) {
            emergencyShutdownActive_ = false;
            setProperty("emergencyShutdownActive", false);
            SPDLOG_INFO("Safety monitor {} auto-recovery: emergency shutdown deactivated", getDeviceId());
        }

        // Notify callback
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (safetyCallback_) {
                safetyCallback_(true, "All safety conditions restored");
            }
        }

    } else if (!overallSafe && previousSafe) {
        // Transition to unsafe
        lastUnsafeTime_ = std::chrono::system_clock::now();
        unsafeCount_++;

        std::string reason = "Unsafe conditions: ";
        for (size_t i = 0; i < currentUnsafeConditions.size(); ++i) {
            if (i > 0) reason += ", ";
            reason += currentUnsafeConditions[i];
        }

        logSafetyEvent("UNSAFE_CONDITION_DETECTED", reason);

        // Check if emergency shutdown should be triggered
        if (alertThresholds_.contains("consecutiveFailures")) {
            int threshold = alertThresholds_["consecutiveFailures"];
            if (unsafeCount_.load() >= threshold) {
                triggerEmergencyShutdown("Consecutive safety failures exceeded threshold");
            }
        }

        // Notify callback
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (safetyCallback_) {
                safetyCallback_(false, reason);
            }
        }
    }

    setProperty("failedChecks", failedChecks_.load());
}

void SafetyMonitor::processUnsafeCondition(const std::string& conditionName) {
    SPDLOG_WARN("Safety monitor {} unsafe condition detected: {}", getDeviceId(), conditionName);
    sendAlert("Unsafe condition: " + conditionName, "WARNING");
}

void SafetyMonitor::processSafeCondition(const std::string& conditionName) {
    SPDLOG_INFO("Safety monitor {} condition restored: {}", getDeviceId(), conditionName);
    sendAlert("Condition restored: " + conditionName, "INFO");
}

void SafetyMonitor::executeEmergencyShutdown(const std::string& reason) {
    if (emergencyShutdownActive_.load()) {
        return;  // Already in emergency shutdown
    }

    emergencyShutdownActive_ = true;
    setProperty("emergencyShutdownActive", true);

    SPDLOG_CRITICAL("Safety monitor {} executing emergency shutdown: {}", getDeviceId(), reason);

    // Notify emergency callback
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (emergencyCallback_) {
            emergencyCallback_(reason);
        }
    }

    // Notify all registered devices
    notifyDevicesOfEmergency(reason);

    logSafetyEvent("EMERGENCY_SHUTDOWN", reason);
    sendAlert("EMERGENCY SHUTDOWN: " + reason, "CRITICAL");
}

void SafetyMonitor::notifyDevicesOfEmergency(const std::string& reason) {
    std::lock_guard<std::mutex> lock(emergencyDevicesMutex_);

    for (const auto& deviceId : emergencyDevices_) {
        SPDLOG_CRITICAL("Safety monitor {} notifying device {} of emergency: {}",
                       getDeviceId(), deviceId, reason);

        // In a real implementation, this would send emergency shutdown commands
        // to the registered devices through the device manager
    }
}

// Built-in safety conditions
bool SafetyMonitor::checkSystemHealth() {
    // Basic system health check
    return true;  // Simplified implementation
}

bool SafetyMonitor::checkCommunication() {
    // Check communication with critical devices
    return true;  // Simplified implementation
}

bool SafetyMonitor::checkPowerStatus() {
    // Check power supply status
    return true;  // Simplified implementation
}

bool SafetyMonitor::checkTemperatureLimits() {
    // Check system temperature limits
    if (safetyLimits_.contains("maxTemperature") && safetyLimits_.contains("minTemperature")) {
        // In real implementation, would read actual temperature
        double currentTemp = 25.0;  // Simulated
        return currentTemp >= safetyLimits_["minTemperature"] &&
               currentTemp <= safetyLimits_["maxTemperature"];
    }
    return true;
}

bool SafetyMonitor::checkWeatherConditions() {
    // Check weather conditions if weather station is available
    return true;  // Simplified implementation
}

bool SafetyMonitor::checkDiskSpace() {
    try {
        if (safetyLimits_.contains("minDiskSpaceGB")) {
            auto space = std::filesystem::space(".");
            double freeGB = static_cast<double>(space.free) / (1024.0 * 1024.0 * 1024.0);
            return freeGB >= safetyLimits_["minDiskSpaceGB"];
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Safety monitor {} disk space check failed: {}", getDeviceId(), e.what());
        return false;
    }
    return true;
}

bool SafetyMonitor::checkMemoryUsage() {
    // Check memory usage
    if (safetyLimits_.contains("maxMemoryUsagePercent")) {
        // Simplified implementation - would use actual memory monitoring
        double memoryUsage = 50.0;  // Simulated percentage
        return memoryUsage <= safetyLimits_["maxMemoryUsagePercent"];
    }
    return true;
}

// Utility methods
void SafetyMonitor::updateSafetyStatus() {
    setProperty("isSafe", isSafe_.load());
    setProperty("unsafeCount", unsafeCount_.load());
    setProperty("emergencyShutdownActive", emergencyShutdownActive_.load());
}

void SafetyMonitor::logSafetyEvent(const std::string& event, const std::string& details) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    SPDLOG_INFO("Safety monitor {} event: {} - {}", getDeviceId(), event, details);

    // In a real implementation, this would log to a persistent safety log
}

void SafetyMonitor::sendAlert(const std::string& alert, const std::string& severity) {
    {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        activeAlerts_.push_back("[" + severity + "] " + alert);

        // Keep only recent alerts (last 100)
        if (activeAlerts_.size() > 100) {
            activeAlerts_.erase(activeAlerts_.begin());
        }
    }

    alertCV_.notify_one();
}

bool SafetyMonitor::isInEmergencyState() const {
    return emergencyShutdownActive_.load() || emergencyTriggered_.load();
}

// IStateful interface implementation
bool SafetyMonitor::setProperty(const std::string& property, const json& value) {
    if (property == "monitoringInterval") {
        setMonitoringInterval(value.get<int>());
        return true;
    } else if (property == "emergencyDelay") {
        setEmergencyDelay(value.get<int>());
        return true;
    } else if (property == "autoRecoveryEnabled") {
        setAutoRecovery(value.get<bool>());
        return true;
    } else if (property == "alertThresholds") {
        setAlertThresholds(value);
        return true;
    } else if (property == "safetyLimits") {
        setSafetyLimits(value);
        return true;
    }

    return ModernDeviceBase::setProperty(property, value);
}

json SafetyMonitor::getProperty(const std::string& property) const {
    if (property == "isSafe") {
        return isSafe_.load();
    } else if (property == "emergencyShutdownActive") {
        return emergencyShutdownActive_.load();
    } else if (property == "autoRecoveryEnabled") {
        return autoRecoveryEnabled_.load();
    } else if (property == "unsafeCount") {
        return unsafeCount_.load();
    } else if (property == "totalChecks") {
        return totalChecks_.load();
    } else if (property == "failedChecks") {
        return failedChecks_.load();
    } else if (property == "emergencyCount") {
        return emergencyCount_.load();
    } else if (property == "activeAlerts") {
        return getActiveAlerts();
    } else if (property == "unsafeConditions") {
        return getUnsafeConditions();
    }

    return ModernDeviceBase::getProperty(property);
}

json SafetyMonitor::getAllProperties() const {
    json properties = ModernDeviceBase::getAllProperties();

    properties["isSafe"] = isSafe_.load();
    properties["emergencyShutdownActive"] = emergencyShutdownActive_.load();
    properties["autoRecoveryEnabled"] = autoRecoveryEnabled_.load();
    properties["unsafeCount"] = unsafeCount_.load();
    properties["totalChecks"] = totalChecks_.load();
    properties["failedChecks"] = failedChecks_.load();
    properties["emergencyCount"] = emergencyCount_.load();
    properties["activeAlerts"] = getActiveAlerts();
    properties["unsafeConditions"] = getUnsafeConditions();
    properties["monitoringInterval"] = monitoringInterval_;
    properties["emergencyDelay"] = emergencyDelay_;
    properties["alertThresholds"] = alertThresholds_;
    properties["safetyLimits"] = safetyLimits_;

    return properties;
}

std::vector<std::string> SafetyMonitor::getCapabilities() const {
    return {
        "SAFETY_MONITORING",
        "EMERGENCY_SHUTDOWN",
        "CONDITION_MANAGEMENT",
        "ALERT_SYSTEM",
        "AUTO_RECOVERY",
        "DEVICE_COORDINATION",
        "EVENT_LOGGING"
    };
}

// ModernDeviceBase overrides
bool SafetyMonitor::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "TRIGGER_EMERGENCY") {
        std::string reason = parameters.value("reason", "Manual trigger");
        triggerEmergencyShutdown(reason);
        result["success"] = true;
        return true;
    } else if (command == "ACKNOWLEDGE_UNSAFE") {
        acknowledgeUnsafeCondition();
        result["success"] = true;
        return true;
    } else if (command == "RESET_MONITOR") {
        resetSafetyMonitor();
        result["success"] = true;
        return true;
    } else if (command == "TEST_EMERGENCY") {
        testEmergencyProcedures();
        result["success"] = true;
        return true;
    } else if (command == "ADD_EMERGENCY_DEVICE") {
        std::string deviceId = parameters.at("deviceId").get<std::string>();
        addEmergencyShutdownDevice(deviceId);
        result["success"] = true;
        return true;
    } else if (command == "REMOVE_EMERGENCY_DEVICE") {
        std::string deviceId = parameters.at("deviceId").get<std::string>();
        removeEmergencyShutdownDevice(deviceId);
        result["success"] = true;
        return true;
    }

    return false;
}

void SafetyMonitor::updateDevice() {
    // Update properties periodically
    updateSafetyStatus();
}

// Manufacturer-specific initialization
void SafetyMonitor::initializeManufacturerSpecific() {
    std::string manufacturer = getProperty("manufacturer").get<std::string>();

    if (manufacturer == "Generic") {
        initializeGeneric();
    } else if (manufacturer == "Lunatico") {
        initializeLunatico();
    } else if (manufacturer == "PegasusAstro") {
        initializePegasusAstro();
    } else if (manufacturer == "AAG") {
        initializeAAG();
    } else if (manufacturer == "Boltwood") {
        initializeBoltwood();
    } else if (manufacturer == "Custom") {
        initializeCustom();
    } else {
        initializeGeneric();
    }
}

void SafetyMonitor::initializeGeneric() {
    monitoringInterval_ = 5;
    emergencyDelay_ = 30;
}

void SafetyMonitor::initializeLunatico() {
    monitoringInterval_ = 3;
    emergencyDelay_ = 15;
}

void SafetyMonitor::initializePegasusAstro() {
    monitoringInterval_ = 2;
    emergencyDelay_ = 10;
}

void SafetyMonitor::initializeAAG() {
    monitoringInterval_ = 5;
    emergencyDelay_ = 20;
}

void SafetyMonitor::initializeBoltwood() {
    monitoringInterval_ = 10;
    emergencyDelay_ = 60;
}

void SafetyMonitor::initializeCustom() {
    monitoringInterval_ = 1;
    emergencyDelay_ = 5;
}

void SafetyMonitor::initializeBuiltInConditions() {
    addSafetyCondition("SystemHealth", [this]() { return checkSystemHealth(); },
                      "Overall system health check");
    addSafetyCondition("Communication", [this]() { return checkCommunication(); },
                      "Communication with critical devices");
    addSafetyCondition("PowerStatus", [this]() { return checkPowerStatus(); },
                      "Power supply status");
    addSafetyCondition("TemperatureLimits", [this]() { return checkTemperatureLimits(); },
                      "System temperature within limits");
    addSafetyCondition("WeatherConditions", [this]() { return checkWeatherConditions(); },
                      "Weather conditions safe for operation");
    addSafetyCondition("DiskSpace", [this]() { return checkDiskSpace(); },
                      "Sufficient disk space available");
    addSafetyCondition("MemoryUsage", [this]() { return checkMemoryUsage(); },
                      "Memory usage within limits");
}

// Factory function
std::unique_ptr<SafetyMonitor> createModernSafetyMonitor(const std::string& deviceId,
                                                         const std::string& manufacturer,
                                                         const std::string& model) {
    return std::make_unique<SafetyMonitor>(deviceId, manufacturer, model);
}

} // namespace device
} // namespace hydrogen

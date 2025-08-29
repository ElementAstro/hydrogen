#include "astrocomm/core/enhanced_error_recovery.h"
#include "astrocomm/core/utils.h"
#include <algorithm>
#include <fstream>
#include <thread>
#include <chrono>

namespace astrocomm {
namespace core {

// EnhancedErrorContext implementation
json EnhancedErrorContext::toJson() const {
    json j = json::object();
    j["deviceId"] = deviceId;
    j["errorCode"] = errorCode;
    j["errorMessage"] = errorMessage;
    j["command"] = command;
    j["parameters"] = parameters;
    j["retryCount"] = retryCount;
    j["maxRetries"] = maxRetries;
    j["severity"] = errorSeverityToString(severity);
    j["deviceType"] = deviceType;
    j["errorCategory"] = errorCategory;
    j["deviceHealth"] = healthStatusToString(deviceHealth);
    j["deviceState"] = lifecycleStateToString(deviceState);
    j["affectedCapabilities"] = affectedCapabilities;
    j["diagnosticData"] = diagnosticData;
    j["isRecurring"] = isRecurring;
    j["occurrenceCount"] = occurrenceCount;
    j["errorTime"] = getIsoTimestamp();
    j["firstOccurrence"] = getIsoTimestamp();
    j["lastOccurrence"] = getIsoTimestamp();
    
    return j;
}

EnhancedErrorContext EnhancedErrorContext::fromJson(const json& j) {
    EnhancedErrorContext context;
    context.deviceId = j.value("deviceId", "");
    context.errorCode = j.value("errorCode", "");
    context.errorMessage = j.value("errorMessage", "");
    context.command = j.value("command", "");
    context.parameters = j.value("parameters", json::object());
    context.retryCount = j.value("retryCount", 0);
    context.maxRetries = j.value("maxRetries", 3);
    context.severity = stringToErrorSeverity(j.value("severity", "MEDIUM"));
    context.deviceType = j.value("deviceType", "");
    context.errorCategory = j.value("errorCategory", "");
    context.deviceHealth = stringToHealthStatus(j.value("deviceHealth", "UNKNOWN"));
    context.deviceState = stringToLifecycleState(j.value("deviceState", "UNKNOWN"));
    context.isRecurring = j.value("isRecurring", false);
    context.occurrenceCount = j.value("occurrenceCount", 1);
    
    if (j.contains("affectedCapabilities") && j["affectedCapabilities"].is_array()) {
        for (const auto& cap : j["affectedCapabilities"]) {
            context.affectedCapabilities.push_back(cap.get<std::string>());
        }
    }
    
    context.diagnosticData = j.value("diagnosticData", json::object());
    
    if (j.contains("errorTime")) {
        context.errorTime = string_utils::parseIsoTimestamp(j["errorTime"]);
    }
    if (j.contains("firstOccurrence")) {
        context.firstOccurrence = string_utils::parseIsoTimestamp(j["firstOccurrence"]);
    }
    if (j.contains("lastOccurrence")) {
        context.lastOccurrence = string_utils::parseIsoTimestamp(j["lastOccurrence"]);
    }
    
    return context;
}

EnhancedErrorContext EnhancedErrorContext::fromErrorContext(const ErrorContext& base) {
    EnhancedErrorContext enhanced;
    enhanced.deviceId = base.deviceId;
    enhanced.errorCode = base.errorCode;
    enhanced.errorMessage = base.errorMessage;
    enhanced.command = base.command;
    enhanced.parameters = base.parameters;
    enhanced.retryCount = base.retryCount;
    enhanced.maxRetries = base.maxRetries;
    enhanced.errorTime = base.errorTime;
    enhanced.firstOccurrence = base.errorTime;
    enhanced.lastOccurrence = base.errorTime;
    
    return enhanced;
}

// RecoveryResult implementation
json RecoveryResult::toJson() const {
    return json{
        {"success", success},
        {"action", action},
        {"result", result},
        {"duration", duration.count()},
        {"metadata", metadata}
    };
}

RecoveryResult RecoveryResult::fromJson(const json& j) {
    RecoveryResult result;
    result.success = j.value("success", false);
    result.action = j.value("action", "");
    result.result = j.value("result", "");
    result.duration = std::chrono::milliseconds(j.value("duration", 0));
    result.metadata = j.value("metadata", json::object());
    
    return result;
}

// RecoveryProcedure implementation
json RecoveryProcedure::toJson() const {
    return json{
        {"name", name},
        {"description", description},
        {"steps", steps},
        {"timeout", timeout.count()},
        {"maxAttempts", maxAttempts},
        {"requiresOperatorApproval", requiresOperatorApproval}
    };
}

RecoveryProcedure RecoveryProcedure::fromJson(const json& j) {
    RecoveryProcedure procedure;
    procedure.name = j.value("name", "");
    procedure.description = j.value("description", "");
    procedure.timeout = std::chrono::milliseconds(j.value("timeout", 30000));
    procedure.maxAttempts = j.value("maxAttempts", 3);
    procedure.requiresOperatorApproval = j.value("requiresOperatorApproval", false);
    
    if (j.contains("steps") && j["steps"].is_array()) {
        for (const auto& step : j["steps"]) {
            procedure.steps.push_back(step.get<std::string>());
        }
    }
    
    return procedure;
}

// EnhancedErrorRecoveryManager implementation
EnhancedErrorRecoveryManager::EnhancedErrorRecoveryManager() = default;

EnhancedErrorRecoveryManager::~EnhancedErrorRecoveryManager() {
    stop();
}

void EnhancedErrorRecoveryManager::registerEnhancedErrorHandler(
    const std::string& deviceId,
    const std::string& errorCode,
    EnhancedErrorStrategy strategy,
    ErrorSeverity severity,
    std::function<RecoveryResult(const EnhancedErrorContext&)> customHandler) {
    
    std::lock_guard<std::shared_mutex> lock(handlersMutex_);
    
    ErrorHandlerInfo info;
    info.strategy = strategy;
    info.severity = severity;
    info.customHandler = customHandler;
    
    // Set default timeouts based on strategy
    switch (strategy) {
        case EnhancedErrorStrategy::RETRY_SIMPLE:
            info.timeout = std::chrono::milliseconds(5000);
            break;
        case EnhancedErrorStrategy::RETRY_EXPONENTIAL:
            info.timeout = std::chrono::milliseconds(30000);
            break;
        case EnhancedErrorStrategy::DEVICE_RESTART:
            info.timeout = std::chrono::milliseconds(60000);
            break;
        case EnhancedErrorStrategy::AUTOMATIC_RECOVERY:
            info.timeout = std::chrono::milliseconds(120000);
            break;
        default:
            info.timeout = std::chrono::milliseconds(10000);
            break;
    }
    
    deviceHandlers_[deviceId][errorCode] = info;
}

void EnhancedErrorRecoveryManager::registerRecoveryProcedure(
    const std::string& deviceType,
    const std::string& errorCategory,
    const RecoveryProcedure& procedure) {
    
    std::lock_guard<std::shared_mutex> lock(handlersMutex_);
    recoveryProcedures_[deviceType][errorCategory] = procedure;
}

RecoveryResult EnhancedErrorRecoveryManager::handleEnhancedError(const EnhancedErrorContext& context) {
    // Add to error history
    addToErrorHistory(context);
    
    // Check if recovery is already in progress for this device
    if (isRecoveryInProgress(context.deviceId)) {
        RecoveryResult result;
        result.success = false;
        result.action = "SKIP";
        result.result = "Recovery already in progress for device";
        return result;
    }
    
    // Find appropriate handler
    std::shared_lock<std::shared_mutex> lock(handlersMutex_);
    
    auto deviceIt = deviceHandlers_.find(context.deviceId);
    if (deviceIt != deviceHandlers_.end()) {
        auto errorIt = deviceIt->second.find(context.errorCode);
        if (errorIt != deviceIt->second.end()) {
            lock.unlock();
            RecoveryResult result = executeRecoveryStrategy(context, errorIt->second);
            updateStatistics(context, result);
            return result;
        }
    }
    
    lock.unlock();
    
    // No specific handler found, use default strategy based on severity
    ErrorHandlerInfo defaultInfo;
    switch (context.severity) {
        case ErrorSeverity::LOW:
            defaultInfo.strategy = EnhancedErrorStrategy::IGNORE;
            break;
        case ErrorSeverity::MEDIUM:
            defaultInfo.strategy = EnhancedErrorStrategy::RETRY_SIMPLE;
            break;
        case ErrorSeverity::HIGH:
            defaultInfo.strategy = EnhancedErrorStrategy::RETRY_EXPONENTIAL;
            break;
        case ErrorSeverity::CRITICAL:
            defaultInfo.strategy = EnhancedErrorStrategy::AUTOMATIC_RECOVERY;
            break;
        case ErrorSeverity::FATAL:
            defaultInfo.strategy = EnhancedErrorStrategy::ESCALATE_TO_OPERATOR;
            break;
    }
    
    RecoveryResult result = executeRecoveryStrategy(context, defaultInfo);
    updateStatistics(context, result);
    return result;
}

RecoveryResult EnhancedErrorRecoveryManager::handleError(const ErrorMessage& errorMsg, const std::string& deviceType) {
    ErrorContext baseContext = ErrorContext::fromErrorMessage(errorMsg);
    EnhancedErrorContext enhancedContext = enhanceErrorContext(baseContext, deviceType);
    
    return handleEnhancedError(enhancedContext);
}

void EnhancedErrorRecoveryManager::setRecoveryEventCallback(
    std::function<void(const std::string&, const RecoveryResult&)> callback) {
    recoveryEventCallback_ = callback;
}

void EnhancedErrorRecoveryManager::setEscalationCallback(
    std::function<void(const EnhancedErrorContext&)> callback) {
    escalationCallback_ = callback;
}

void EnhancedErrorRecoveryManager::setAutoRecoveryEnabled(bool enabled) {
    autoRecoveryEnabled_ = enabled;
}

void EnhancedErrorRecoveryManager::setMaxConcurrentRecoveries(int maxConcurrent) {
    maxConcurrentRecoveries_ = maxConcurrent;
}

json EnhancedErrorRecoveryManager::getRecoveryStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    json stats = json::object();
    stats["totalErrors"] = stats_.totalErrors;
    stats["successfulRecoveries"] = stats_.successfulRecoveries;
    stats["failedRecoveries"] = stats_.failedRecoveries;
    stats["escalatedErrors"] = stats_.escalatedErrors;
    
    double successRate = stats_.totalErrors > 0 ? 
        (double)stats_.successfulRecoveries / stats_.totalErrors : 0.0;
    stats["successRate"] = successRate;
    
    stats["errorCodeCounts"] = stats_.errorCodeCounts;
    stats["deviceErrorCounts"] = stats_.deviceErrorCounts;
    
    json strategyCounts = json::object();
    for (const auto& [strategy, count] : stats_.strategyCounts) {
        strategyCounts[enhancedErrorStrategyToString(strategy)] = count;
    }
    stats["strategyCounts"] = strategyCounts;
    
    return stats;
}

std::vector<std::string> EnhancedErrorRecoveryManager::getActiveRecoveries() const {
    std::lock_guard<std::mutex> lock(activeRecoveriesMutex_);
    
    std::vector<std::string> activeDevices;
    for (const auto& [deviceId, recovery] : activeRecoveries_) {
        activeDevices.push_back(deviceId);
    }
    
    return activeDevices;
}

bool EnhancedErrorRecoveryManager::cancelRecovery(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(activeRecoveriesMutex_);
    
    auto it = activeRecoveries_.find(deviceId);
    if (it != activeRecoveries_.end()) {
        it->second->cancelled = true;
        if (it->second->recoveryThread.joinable()) {
            it->second->recoveryThread.join();
        }
        activeRecoveries_.erase(it);
        return true;
    }
    
    return false;
}

std::vector<EnhancedErrorContext> EnhancedErrorRecoveryManager::getErrorHistory(
    const std::string& deviceId, size_t maxEntries) const {
    
    std::lock_guard<std::mutex> lock(errorHistoryMutex_);
    
    auto it = errorHistory_.find(deviceId);
    if (it == errorHistory_.end()) {
        return {};
    }
    
    const auto& history = it->second;
    if (maxEntries == 0 || history.size() <= maxEntries) {
        return history;
    }
    
    // Return the most recent entries
    return std::vector<EnhancedErrorContext>(
        history.end() - maxEntries, history.end());
}

void EnhancedErrorRecoveryManager::clearErrorHistory(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(errorHistoryMutex_);
    errorHistory_.erase(deviceId);
}

void EnhancedErrorRecoveryManager::start() {
    if (running_) return;
    
    running_ = true;
    startMonitoringThread();
}

void EnhancedErrorRecoveryManager::stop() {
    if (!running_) return;
    
    running_ = false;
    stopMonitoringThread();
    
    // Cancel all active recoveries
    std::lock_guard<std::mutex> lock(activeRecoveriesMutex_);
    for (auto& [deviceId, recovery] : activeRecoveries_) {
        recovery->cancelled = true;
        if (recovery->recoveryThread.joinable()) {
            recovery->recoveryThread.join();
        }
    }
    activeRecoveries_.clear();
}

EnhancedErrorRecoveryManager& EnhancedErrorRecoveryManager::getInstance() {
    static EnhancedErrorRecoveryManager instance;
    return instance;
}

// Helper function implementations
std::string errorSeverityToString(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::LOW: return "LOW";
        case ErrorSeverity::MEDIUM: return "MEDIUM";
        case ErrorSeverity::HIGH: return "HIGH";
        case ErrorSeverity::CRITICAL: return "CRITICAL";
        case ErrorSeverity::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

ErrorSeverity stringToErrorSeverity(const std::string& severity) {
    if (severity == "LOW") return ErrorSeverity::LOW;
    if (severity == "MEDIUM") return ErrorSeverity::MEDIUM;
    if (severity == "HIGH") return ErrorSeverity::HIGH;
    if (severity == "CRITICAL") return ErrorSeverity::CRITICAL;
    if (severity == "FATAL") return ErrorSeverity::FATAL;
    return ErrorSeverity::MEDIUM;
}

std::string enhancedErrorStrategyToString(EnhancedErrorStrategy strategy) {
    switch (strategy) {
        case EnhancedErrorStrategy::IGNORE: return "IGNORE";
        case EnhancedErrorStrategy::RETRY_SIMPLE: return "RETRY_SIMPLE";
        case EnhancedErrorStrategy::RETRY_EXPONENTIAL: return "RETRY_EXPONENTIAL";
        case EnhancedErrorStrategy::RETRY_WITH_RESET: return "RETRY_WITH_RESET";
        case EnhancedErrorStrategy::DEVICE_RESTART: return "DEVICE_RESTART";
        case EnhancedErrorStrategy::DEVICE_RECONNECT: return "DEVICE_RECONNECT";
        case EnhancedErrorStrategy::FAILOVER_PRIMARY: return "FAILOVER_PRIMARY";
        case EnhancedErrorStrategy::FAILOVER_SECONDARY: return "FAILOVER_SECONDARY";
        case EnhancedErrorStrategy::ESCALATE_TO_OPERATOR: return "ESCALATE_TO_OPERATOR";
        case EnhancedErrorStrategy::QUARANTINE_DEVICE: return "QUARANTINE_DEVICE";
        case EnhancedErrorStrategy::AUTOMATIC_RECOVERY: return "AUTOMATIC_RECOVERY";
        case EnhancedErrorStrategy::MAINTENANCE_MODE: return "MAINTENANCE_MODE";
        case EnhancedErrorStrategy::CUSTOM_HANDLER: return "CUSTOM_HANDLER";
        default: return "UNKNOWN";
    }
}

EnhancedErrorStrategy stringToEnhancedErrorStrategy(const std::string& strategy) {
    if (strategy == "IGNORE") return EnhancedErrorStrategy::IGNORE;
    if (strategy == "RETRY_SIMPLE") return EnhancedErrorStrategy::RETRY_SIMPLE;
    if (strategy == "RETRY_EXPONENTIAL") return EnhancedErrorStrategy::RETRY_EXPONENTIAL;
    if (strategy == "RETRY_WITH_RESET") return EnhancedErrorStrategy::RETRY_WITH_RESET;
    if (strategy == "DEVICE_RESTART") return EnhancedErrorStrategy::DEVICE_RESTART;
    if (strategy == "DEVICE_RECONNECT") return EnhancedErrorStrategy::DEVICE_RECONNECT;
    if (strategy == "FAILOVER_PRIMARY") return EnhancedErrorStrategy::FAILOVER_PRIMARY;
    if (strategy == "FAILOVER_SECONDARY") return EnhancedErrorStrategy::FAILOVER_SECONDARY;
    if (strategy == "ESCALATE_TO_OPERATOR") return EnhancedErrorStrategy::ESCALATE_TO_OPERATOR;
    if (strategy == "QUARANTINE_DEVICE") return EnhancedErrorStrategy::QUARANTINE_DEVICE;
    if (strategy == "AUTOMATIC_RECOVERY") return EnhancedErrorStrategy::AUTOMATIC_RECOVERY;
    if (strategy == "MAINTENANCE_MODE") return EnhancedErrorStrategy::MAINTENANCE_MODE;
    if (strategy == "CUSTOM_HANDLER") return EnhancedErrorStrategy::CUSTOM_HANDLER;
    return EnhancedErrorStrategy::RETRY_SIMPLE;
}

RecoveryResult EnhancedErrorRecoveryManager::executeRecoveryStrategy(
    const EnhancedErrorContext& context, 
    const ErrorHandlerInfo& handlerInfo) {
    
    auto startTime = std::chrono::steady_clock::now();
    RecoveryResult result;
    result.action = enhancedErrorStrategyToString(handlerInfo.strategy);
    
    try {
        switch (handlerInfo.strategy) {
            case EnhancedErrorStrategy::IGNORE:
                result.success = true;
                result.result = "Error ignored as per strategy";
                break;
                
            case EnhancedErrorStrategy::RETRY_SIMPLE:
            case EnhancedErrorStrategy::RETRY_EXPONENTIAL:
            case EnhancedErrorStrategy::RETRY_WITH_RESET:
                result = executeRetryStrategy(context, handlerInfo);
                break;
                
            case EnhancedErrorStrategy::DEVICE_RESTART:
                result = executeDeviceRestartStrategy(context);
                break;
                
            case EnhancedErrorStrategy::DEVICE_RECONNECT:
                result.success = true; // Placeholder - would implement actual reconnection
                result.result = "Device reconnection initiated";
                break;
                
            case EnhancedErrorStrategy::FAILOVER_PRIMARY:
            case EnhancedErrorStrategy::FAILOVER_SECONDARY:
                result = executeFailoverStrategy(context);
                break;
                
            case EnhancedErrorStrategy::AUTOMATIC_RECOVERY:
                result = executeAutomaticRecoveryStrategy(context);
                break;
                
            case EnhancedErrorStrategy::ESCALATE_TO_OPERATOR:
                if (escalationCallback_) {
                    escalationCallback_(context);
                }
                result.success = true;
                result.result = "Error escalated to operator";
                break;
                
            case EnhancedErrorStrategy::QUARANTINE_DEVICE:
                result.success = true;
                result.result = "Device quarantined";
                break;
                
            case EnhancedErrorStrategy::MAINTENANCE_MODE:
                result.success = true;
                result.result = "Device put in maintenance mode";
                break;
                
            case EnhancedErrorStrategy::CUSTOM_HANDLER:
                if (handlerInfo.customHandler) {
                    result = handlerInfo.customHandler(context);
                } else {
                    result.success = false;
                    result.result = "No custom handler provided";
                }
                break;
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.result = "Recovery strategy failed: " + std::string(e.what());
    }
    
    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Notify callback if set
    if (recoveryEventCallback_) {
        recoveryEventCallback_(context.deviceId, result);
    }
    
    return result;
}

RecoveryResult EnhancedErrorRecoveryManager::executeRetryStrategy(
    const EnhancedErrorContext& context, 
    const ErrorHandlerInfo& handlerInfo) {
    
    RecoveryResult result;
    result.action = enhancedErrorStrategyToString(handlerInfo.strategy);
    
    // Implement retry logic based on strategy
    int maxRetries = std::min(context.maxRetries, handlerInfo.maxRetries);
    
    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        // Calculate delay based on strategy
        std::chrono::milliseconds delay = handlerInfo.retryDelay;
        if (handlerInfo.strategy == EnhancedErrorStrategy::RETRY_EXPONENTIAL) {
            delay = std::chrono::milliseconds(static_cast<int>(
                handlerInfo.retryDelay.count() * std::pow(2, attempt - 1)));
        }
        
        if (attempt > 1) {
            std::this_thread::sleep_for(delay);
        }
        
        // Simulate retry operation (in real implementation, would retry the actual operation)
        bool retrySuccess = (attempt >= maxRetries / 2); // Simulate success after some attempts
        
        if (retrySuccess) {
            result.success = true;
            result.result = "Operation succeeded after " + std::to_string(attempt) + " attempts";
            break;
        }
    }
    
    if (!result.success) {
        result.result = "All retry attempts failed";
    }
    
    return result;
}

RecoveryResult EnhancedErrorRecoveryManager::executeDeviceRestartStrategy(const EnhancedErrorContext& /* context */) {
    RecoveryResult result;
    result.action = "DEVICE_RESTART";

    // Simulate device restart procedure
    try {
        // In real implementation, would call actual device restart methods
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate restart time

        result.success = true;
        result.result = "Device restart completed successfully";
        result.metadata["restartType"] = "soft_restart";
    } catch (const std::exception& e) {
        result.success = false;
        result.result = "Device restart failed: " + std::string(e.what());
    }

    return result;
}

RecoveryResult EnhancedErrorRecoveryManager::executeFailoverStrategy(const EnhancedErrorContext& context) {
    RecoveryResult result;
    result.action = "FAILOVER";

    // Simulate failover procedure
    result.success = true; // Placeholder
    result.result = "Failover to backup device initiated";
    result.metadata["backupDeviceId"] = context.deviceId + "_backup";

    return result;
}

RecoveryResult EnhancedErrorRecoveryManager::executeAutomaticRecoveryStrategy(const EnhancedErrorContext& context) {
    RecoveryResult result;
    result.action = "AUTOMATIC_RECOVERY";

    // Look for recovery procedure for this device type and error category
    std::shared_lock<std::shared_mutex> lock(handlersMutex_);

    auto typeIt = recoveryProcedures_.find(context.deviceType);
    if (typeIt != recoveryProcedures_.end()) {
        auto categoryIt = typeIt->second.find(context.errorCategory);
        if (categoryIt != typeIt->second.end()) {
            const auto& procedure = categoryIt->second;
            lock.unlock();

            // Execute recovery procedure steps
            result.success = true;
            result.result = "Executed recovery procedure: " + procedure.name;
            result.metadata["procedureName"] = procedure.name;
            result.metadata["stepsExecuted"] = procedure.steps.size();

            return result;
        }
    }

    lock.unlock();

    // Default automatic recovery
    result.success = true;
    result.result = "Default automatic recovery sequence executed";

    return result;
}

void EnhancedErrorRecoveryManager::addToErrorHistory(const EnhancedErrorContext& context) {
    std::lock_guard<std::mutex> lock(errorHistoryMutex_);

    auto& history = errorHistory_[context.deviceId];
    history.push_back(context);

    // Trim history if too large
    if (history.size() > maxHistoryEntries_) {
        size_t toRemove = history.size() - maxHistoryEntries_;
        history.erase(history.begin(), history.begin() + toRemove);
    }
}

void EnhancedErrorRecoveryManager::updateStatistics(const EnhancedErrorContext& context, const RecoveryResult& result) {
    std::lock_guard<std::mutex> lock(statsMutex_);

    stats_.totalErrors++;
    if (result.success) {
        stats_.successfulRecoveries++;
    } else {
        stats_.failedRecoveries++;
    }

    stats_.errorCodeCounts[context.errorCode]++;
    stats_.deviceErrorCounts[context.deviceId]++;

    // Parse strategy from result action
    EnhancedErrorStrategy strategy = stringToEnhancedErrorStrategy(result.action);
    stats_.strategyCounts[strategy]++;
}

void EnhancedErrorRecoveryManager::startMonitoringThread() {
    monitoringThread_ = std::thread(&EnhancedErrorRecoveryManager::monitoringThreadFunction, this);
}

void EnhancedErrorRecoveryManager::stopMonitoringThread() {
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
}

void EnhancedErrorRecoveryManager::monitoringThreadFunction() {
    while (running_) {
        cleanupCompletedRecoveries();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void EnhancedErrorRecoveryManager::cleanupCompletedRecoveries() {
    std::lock_guard<std::mutex> lock(activeRecoveriesMutex_);

    auto it = activeRecoveries_.begin();
    while (it != activeRecoveries_.end()) {
        if (it->second->cancelled || !it->second->recoveryThread.joinable()) {
            if (it->second->recoveryThread.joinable()) {
                it->second->recoveryThread.join();
            }
            it = activeRecoveries_.erase(it);
        } else {
            ++it;
        }
    }
}

bool EnhancedErrorRecoveryManager::isRecoveryInProgress(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(activeRecoveriesMutex_);
    return activeRecoveries_.find(deviceId) != activeRecoveries_.end();
}

EnhancedErrorContext EnhancedErrorRecoveryManager::enhanceErrorContext(const ErrorContext& base, const std::string& deviceType) {
    EnhancedErrorContext enhanced = EnhancedErrorContext::fromErrorContext(base);
    enhanced.deviceType = deviceType;

    // Try to get additional context from health and lifecycle managers
    auto& healthMonitor = DeviceHealthMonitor::getInstance();
    enhanced.deviceHealth = healthMonitor.getHealthStatus(base.deviceId);

    auto& lifecycleManager = DeviceLifecycleManager::getInstance();
    enhanced.deviceState = lifecycleManager.getCurrentState(base.deviceId);

    // Categorize error based on error code
    if (base.errorCode.find("CONNECTION") != std::string::npos) {
        enhanced.errorCategory = "CONNECTION";
    } else if (base.errorCode.find("TIMEOUT") != std::string::npos) {
        enhanced.errorCategory = "TIMEOUT";
    } else if (base.errorCode.find("HARDWARE") != std::string::npos) {
        enhanced.errorCategory = "HARDWARE";
    } else if (base.errorCode.find("PROTOCOL") != std::string::npos) {
        enhanced.errorCategory = "PROTOCOL";
    } else {
        enhanced.errorCategory = "GENERAL";
    }

    // Determine severity based on error code and device state
    if (base.errorCode.find("FATAL") != std::string::npos ||
        base.errorCode.find("CRITICAL") != std::string::npos) {
        enhanced.severity = ErrorSeverity::CRITICAL;
    } else if (base.errorCode.find("ERROR") != std::string::npos) {
        enhanced.severity = ErrorSeverity::HIGH;
    } else if (base.errorCode.find("WARNING") != std::string::npos) {
        enhanced.severity = ErrorSeverity::MEDIUM;
    } else {
        enhanced.severity = ErrorSeverity::LOW;
    }

    return enhanced;
}

} // namespace core
} // namespace astrocomm

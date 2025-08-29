#pragma once

#include "error_recovery.h"
#include "device_health.h"
#include "device_lifecycle.h"
#include <chrono>
#include <queue>
#include <unordered_set>

namespace astrocomm {
namespace core {

/**
 * @brief Enhanced error handling strategies with device-specific behaviors
 */
enum class EnhancedErrorStrategy {
    IGNORE,                    // Ignore the error
    RETRY_SIMPLE,             // Simple retry with backoff
    RETRY_EXPONENTIAL,        // Exponential backoff retry
    RETRY_WITH_RESET,         // Retry after device reset
    DEVICE_RESTART,           // Restart the device
    DEVICE_RECONNECT,         // Reconnect the device
    FAILOVER_PRIMARY,         // Switch to primary backup
    FAILOVER_SECONDARY,       // Switch to secondary backup
    ESCALATE_TO_OPERATOR,     // Notify human operator
    QUARANTINE_DEVICE,        // Isolate device from system
    AUTOMATIC_RECOVERY,       // Attempt automatic recovery sequence
    MAINTENANCE_MODE,         // Put device in maintenance mode
    CUSTOM_HANDLER           // Use custom recovery logic
};

/**
 * @brief Error severity levels for prioritization
 */
enum class ErrorSeverity {
    LOW,        // Minor issues, continue operation
    MEDIUM,     // Moderate issues, may affect performance
    HIGH,       // Serious issues, may cause failures
    CRITICAL,   // Critical issues, immediate attention required
    FATAL       // Fatal errors, system shutdown may be required
};

/**
 * @brief Enhanced error context with additional metadata
 */
struct EnhancedErrorContext : public ErrorContext {
    ErrorSeverity severity = ErrorSeverity::MEDIUM;
    std::string deviceType;
    std::string errorCategory;
    DeviceHealthStatus deviceHealth = DeviceHealthStatus::UNKNOWN;
    DeviceLifecycleState deviceState = DeviceLifecycleState::UNKNOWN;
    std::vector<std::string> affectedCapabilities;
    json diagnosticData;
    bool isRecurring = false;
    int occurrenceCount = 1;
    std::chrono::system_clock::time_point firstOccurrence;
    std::chrono::system_clock::time_point lastOccurrence;
    
    // Convert to JSON
    json toJson() const;
    
    // Create from JSON
    static EnhancedErrorContext fromJson(const json& j);
    
    // Create from base ErrorContext
    static EnhancedErrorContext fromErrorContext(const ErrorContext& base);
};

/**
 * @brief Recovery action result
 */
struct RecoveryResult {
    bool success = false;
    std::string action;
    std::string result;
    std::chrono::milliseconds duration{0};
    json metadata;
    
    json toJson() const;
    static RecoveryResult fromJson(const json& j);
};

/**
 * @brief Recovery procedure definition
 */
struct RecoveryProcedure {
    std::string name;
    std::string description;
    std::vector<std::string> steps;
    std::chrono::milliseconds timeout{30000}; // 30 seconds default
    int maxAttempts = 3;
    bool requiresOperatorApproval = false;
    
    json toJson() const;
    static RecoveryProcedure fromJson(const json& j);
};

/**
 * @brief Enhanced error recovery manager with advanced capabilities
 */
class EnhancedErrorRecoveryManager {
public:
    EnhancedErrorRecoveryManager();
    virtual ~EnhancedErrorRecoveryManager();
    
    /**
     * @brief Register enhanced error handler for specific device and error
     * @param deviceId Device identifier
     * @param errorCode Error code
     * @param strategy Recovery strategy
     * @param severity Error severity
     * @param customHandler Optional custom handler function
     */
    void registerEnhancedErrorHandler(const std::string& deviceId,
                                     const std::string& errorCode,
                                     EnhancedErrorStrategy strategy,
                                     ErrorSeverity severity = ErrorSeverity::MEDIUM,
                                     std::function<RecoveryResult(const EnhancedErrorContext&)> customHandler = nullptr);
    
    /**
     * @brief Register recovery procedure for device type
     * @param deviceType Device type (e.g., "camera", "telescope")
     * @param errorCategory Error category
     * @param procedure Recovery procedure definition
     */
    void registerRecoveryProcedure(const std::string& deviceType,
                                  const std::string& errorCategory,
                                  const RecoveryProcedure& procedure);
    
    /**
     * @brief Handle enhanced error with full context
     * @param context Enhanced error context
     * @return Recovery result
     */
    RecoveryResult handleEnhancedError(const EnhancedErrorContext& context);
    
    /**
     * @brief Handle error from basic error message
     * @param errorMsg Error message
     * @param deviceType Device type for context
     * @return Recovery result
     */
    RecoveryResult handleError(const ErrorMessage& errorMsg, const std::string& deviceType = "");
    
    /**
     * @brief Register callback for recovery events
     * @param callback Function to call when recovery actions are taken
     */
    void setRecoveryEventCallback(std::function<void(const std::string&, const RecoveryResult&)> callback);
    
    /**
     * @brief Register callback for escalation events
     * @param callback Function to call when errors need operator attention
     */
    void setEscalationCallback(std::function<void(const EnhancedErrorContext&)> callback);
    
    /**
     * @brief Enable/disable automatic recovery
     * @param enabled Whether to enable automatic recovery
     */
    void setAutoRecoveryEnabled(bool enabled);
    
    /**
     * @brief Set maximum concurrent recovery operations
     * @param maxConcurrent Maximum number of concurrent recoveries
     */
    void setMaxConcurrentRecoveries(int maxConcurrent);
    
    /**
     * @brief Get recovery statistics
     * @return JSON object with recovery statistics
     */
    json getRecoveryStatistics() const;
    
    /**
     * @brief Get active recovery operations
     * @return Vector of device IDs with active recoveries
     */
    std::vector<std::string> getActiveRecoveries() const;
    
    /**
     * @brief Cancel recovery operation for device
     * @param deviceId Device identifier
     * @return True if cancellation successful
     */
    bool cancelRecovery(const std::string& deviceId);
    
    /**
     * @brief Get error history for device
     * @param deviceId Device identifier
     * @param maxEntries Maximum number of entries to return
     * @return Vector of enhanced error contexts
     */
    std::vector<EnhancedErrorContext> getErrorHistory(const std::string& deviceId, 
                                                     size_t maxEntries = 50) const;
    
    /**
     * @brief Clear error history for device
     * @param deviceId Device identifier
     */
    void clearErrorHistory(const std::string& deviceId);
    
    /**
     * @brief Start the enhanced error recovery manager
     */
    void start();
    
    /**
     * @brief Stop the enhanced error recovery manager
     */
    void stop();
    
    /**
     * @brief Get singleton instance
     * @return Reference to singleton instance
     */
    static EnhancedErrorRecoveryManager& getInstance();

private:
    struct ErrorHandlerInfo {
        EnhancedErrorStrategy strategy;
        ErrorSeverity severity;
        std::function<RecoveryResult(const EnhancedErrorContext&)> customHandler;
        int maxRetries = 3;
        std::chrono::milliseconds retryDelay{1000};
        std::chrono::milliseconds timeout{30000};
    };
    
    struct ActiveRecovery {
        std::string deviceId;
        std::string errorCode;
        EnhancedErrorStrategy strategy;
        std::chrono::system_clock::time_point startTime;
        std::thread recoveryThread;
        std::atomic<bool> cancelled{false};
    };
    
    // Thread-safe storage
    mutable std::shared_mutex handlersMutex_;
    std::unordered_map<std::string, std::unordered_map<std::string, ErrorHandlerInfo>> deviceHandlers_;
    std::unordered_map<std::string, std::unordered_map<std::string, RecoveryProcedure>> recoveryProcedures_;
    
    mutable std::mutex errorHistoryMutex_;
    std::unordered_map<std::string, std::vector<EnhancedErrorContext>> errorHistory_;
    
    mutable std::mutex activeRecoveriesMutex_;
    std::unordered_map<std::string, std::unique_ptr<ActiveRecovery>> activeRecoveries_;
    
    // Configuration
    std::atomic<bool> autoRecoveryEnabled_{true};
    std::atomic<int> maxConcurrentRecoveries_{5};
    std::atomic<size_t> maxHistoryEntries_{100};
    
    // Callbacks
    std::function<void(const std::string&, const RecoveryResult&)> recoveryEventCallback_;
    std::function<void(const EnhancedErrorContext&)> escalationCallback_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    struct RecoveryStats {
        uint64_t totalErrors = 0;
        uint64_t successfulRecoveries = 0;
        uint64_t failedRecoveries = 0;
        uint64_t escalatedErrors = 0;
        std::unordered_map<std::string, uint32_t> errorCodeCounts;
        std::unordered_map<std::string, uint32_t> deviceErrorCounts;
        std::unordered_map<EnhancedErrorStrategy, uint32_t> strategyCounts;
    } stats_;
    
    // Thread management
    std::atomic<bool> running_{false};
    std::thread monitoringThread_;
    
    // Helper methods
    RecoveryResult executeRecoveryStrategy(const EnhancedErrorContext& context, 
                                          const ErrorHandlerInfo& handlerInfo);
    RecoveryResult executeRetryStrategy(const EnhancedErrorContext& context, 
                                       const ErrorHandlerInfo& handlerInfo);
    RecoveryResult executeDeviceRestartStrategy(const EnhancedErrorContext& context);
    RecoveryResult executeFailoverStrategy(const EnhancedErrorContext& context);
    RecoveryResult executeAutomaticRecoveryStrategy(const EnhancedErrorContext& context);
    
    void addToErrorHistory(const EnhancedErrorContext& context);
    void updateStatistics(const EnhancedErrorContext& context, const RecoveryResult& result);
    void startMonitoringThread();
    void stopMonitoringThread();
    void monitoringThreadFunction();
    void cleanupCompletedRecoveries();
    bool isRecoveryInProgress(const std::string& deviceId) const;
    EnhancedErrorContext enhanceErrorContext(const ErrorContext& base, const std::string& deviceType);
};

/**
 * @brief Helper functions for enhanced error recovery
 */
std::string errorSeverityToString(ErrorSeverity severity);
ErrorSeverity stringToErrorSeverity(const std::string& severity);
std::string enhancedErrorStrategyToString(EnhancedErrorStrategy strategy);
EnhancedErrorStrategy stringToEnhancedErrorStrategy(const std::string& strategy);

} // namespace core
} // namespace astrocomm

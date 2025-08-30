#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <nlohmann/json.hpp>

namespace astrocomm {
namespace core {

using json = nlohmann::json;

/**
 * @brief Device health status enumeration
 */
enum class DeviceHealthStatus {
    EXCELLENT,    // 90-100% health score
    GOOD,         // 70-89% health score  
    FAIR,         // 50-69% health score
    POOR,         // 30-49% health score
    CRITICAL,     // 0-29% health score
    UNKNOWN       // No data available
};

/**
 * @brief Device performance metrics structure
 */
struct DeviceMetrics {
    // Response time metrics (milliseconds)
    double averageResponseTime = 0.0;
    double minResponseTime = 0.0;
    double maxResponseTime = 0.0;
    
    // Throughput metrics
    uint64_t totalCommands = 0;
    uint64_t successfulCommands = 0;
    uint64_t failedCommands = 0;
    
    // Connection metrics
    uint32_t connectionAttempts = 0;
    uint32_t successfulConnections = 0;
    uint32_t connectionFailures = 0;
    double averageConnectionTime = 0.0;
    
    // Error metrics
    uint32_t totalErrors = 0;
    uint32_t criticalErrors = 0;
    uint32_t recoverableErrors = 0;
    
    // Resource usage
    double memoryUsageMB = 0.0;
    double cpuUsagePercent = 0.0;
    
    // Timestamps
    std::chrono::system_clock::time_point lastUpdated;
    std::chrono::system_clock::time_point firstSeen;
    
    // Calculate success rate
    double getSuccessRate() const {
        return totalCommands > 0 ? (double)successfulCommands / totalCommands : 0.0;
    }
    
    // Calculate connection success rate
    double getConnectionSuccessRate() const {
        return connectionAttempts > 0 ? (double)successfulConnections / connectionAttempts : 0.0;
    }
    
    // Convert to JSON
    json toJson() const;
    
    // Create from JSON
    static DeviceMetrics fromJson(const json& j);
};

/**
 * @brief Device health report structure
 */
struct DeviceHealthReport {
    std::string deviceId;
    DeviceHealthStatus status;
    double healthScore;  // 0.0 - 100.0
    DeviceMetrics metrics;
    std::vector<std::string> issues;
    std::vector<std::string> recommendations;
    std::chrono::system_clock::time_point timestamp;
    
    // Convert to JSON
    json toJson() const;
    
    // Create from JSON
    static DeviceHealthReport fromJson(const json& j);
};

/**
 * @brief Interface for device health monitoring
 */
class IDeviceHealthMonitor {
public:
    virtual ~IDeviceHealthMonitor() = default;
    
    /**
     * @brief Record a command execution
     * @param deviceId Device identifier
     * @param command Command name
     * @param responseTimeMs Response time in milliseconds
     * @param success Whether the command succeeded
     */
    virtual void recordCommand(const std::string& deviceId, 
                              const std::string& command,
                              double responseTimeMs, 
                              bool success) = 0;
    
    /**
     * @brief Record a connection attempt
     * @param deviceId Device identifier
     * @param success Whether the connection succeeded
     * @param connectionTimeMs Connection time in milliseconds
     */
    virtual void recordConnection(const std::string& deviceId, 
                                 bool success, 
                                 double connectionTimeMs) = 0;
    
    /**
     * @brief Record an error
     * @param deviceId Device identifier
     * @param errorCode Error code
     * @param severity Error severity (critical, recoverable, etc.)
     */
    virtual void recordError(const std::string& deviceId, 
                            const std::string& errorCode,
                            const std::string& severity) = 0;
    
    /**
     * @brief Update resource usage metrics
     * @param deviceId Device identifier
     * @param memoryUsageMB Memory usage in MB
     * @param cpuUsagePercent CPU usage percentage
     */
    virtual void updateResourceUsage(const std::string& deviceId,
                                    double memoryUsageMB,
                                    double cpuUsagePercent) = 0;
    
    /**
     * @brief Get current health status for a device
     * @param deviceId Device identifier
     * @return Current health status
     */
    virtual DeviceHealthStatus getHealthStatus(const std::string& deviceId) const = 0;
    
    /**
     * @brief Get detailed health report for a device
     * @param deviceId Device identifier
     * @return Detailed health report
     */
    virtual DeviceHealthReport getHealthReport(const std::string& deviceId) const = 0;
    
    /**
     * @brief Get health reports for all monitored devices
     * @return Vector of health reports
     */
    virtual std::vector<DeviceHealthReport> getAllHealthReports() const = 0;
    
    /**
     * @brief Set health threshold for status calculation
     * @param status Health status level
     * @param threshold Threshold value (0.0 - 100.0)
     */
    virtual void setHealthThreshold(DeviceHealthStatus status, double threshold) = 0;
    
    /**
     * @brief Register callback for health status changes
     * @param callback Function to call when health status changes
     */
    virtual void setHealthChangeCallback(
        std::function<void(const std::string&, DeviceHealthStatus, DeviceHealthStatus)> callback) = 0;
    
    /**
     * @brief Start health monitoring for a device
     * @param deviceId Device identifier
     */
    virtual void startMonitoring(const std::string& deviceId) = 0;
    
    /**
     * @brief Stop health monitoring for a device
     * @param deviceId Device identifier
     */
    virtual void stopMonitoring(const std::string& deviceId) = 0;
    
    /**
     * @brief Clear all metrics for a device
     * @param deviceId Device identifier
     */
    virtual void clearMetrics(const std::string& deviceId) = 0;
};

/**
 * @brief Concrete implementation of device health monitor
 */
class DeviceHealthMonitor : public IDeviceHealthMonitor {
public:
    DeviceHealthMonitor();
    virtual ~DeviceHealthMonitor();
    
    // IDeviceHealthMonitor implementation
    void recordCommand(const std::string& deviceId, 
                      const std::string& command,
                      double responseTimeMs, 
                      bool success) override;
    
    void recordConnection(const std::string& deviceId, 
                         bool success, 
                         double connectionTimeMs) override;
    
    void recordError(const std::string& deviceId, 
                    const std::string& errorCode,
                    const std::string& severity) override;
    
    void updateResourceUsage(const std::string& deviceId,
                            double memoryUsageMB,
                            double cpuUsagePercent) override;
    
    DeviceHealthStatus getHealthStatus(const std::string& deviceId) const override;
    DeviceHealthReport getHealthReport(const std::string& deviceId) const override;
    std::vector<DeviceHealthReport> getAllHealthReports() const override;
    
    void setHealthThreshold(DeviceHealthStatus status, double threshold) override;
    void setHealthChangeCallback(
        std::function<void(const std::string&, DeviceHealthStatus, DeviceHealthStatus)> callback) override;
    
    void startMonitoring(const std::string& deviceId) override;
    void stopMonitoring(const std::string& deviceId) override;
    void clearMetrics(const std::string& deviceId) override;
    
    /**
     * @brief Get singleton instance
     * @return Reference to singleton instance
     */
    static DeviceHealthMonitor& getInstance();
    
    /**
     * @brief Enable/disable automatic cleanup of old metrics
     * @param enabled Whether to enable cleanup
     * @param maxAgeHours Maximum age of metrics in hours
     */
    void setAutoCleanup(bool enabled, int maxAgeHours = 24);
    
    /**
     * @brief Save metrics to file
     * @param filename File to save to
     * @return True if successful
     */
    bool saveMetrics(const std::string& filename) const;
    
    /**
     * @brief Load metrics from file
     * @param filename File to load from
     * @return True if successful
     */
    bool loadMetrics(const std::string& filename);

private:
    mutable std::mutex metricsMutex_;
    std::unordered_map<std::string, DeviceMetrics> deviceMetrics_;
    std::unordered_map<std::string, bool> monitoredDevices_;
    
    // Health thresholds
    std::unordered_map<DeviceHealthStatus, double> healthThresholds_;
    
    // Callback for health status changes
    std::function<void(const std::string&, DeviceHealthStatus, DeviceHealthStatus)> healthChangeCallback_;
    
    // Auto cleanup settings
    std::atomic<bool> autoCleanupEnabled_{false};
    std::atomic<int> maxMetricsAgeHours_{24};
    std::thread cleanupThread_;
    std::atomic<bool> cleanupRunning_{false};
    
    // Helper methods
    double calculateHealthScore(const DeviceMetrics& metrics) const;
    DeviceHealthStatus scoreToStatus(double score) const;
    void initializeDefaultThresholds();
    void startCleanupThread();
    void stopCleanupThread();
    void cleanupOldMetrics();
    std::vector<std::string> generateHealthIssues(const DeviceMetrics& metrics) const;
    std::vector<std::string> generateRecommendations(const DeviceMetrics& metrics) const;
};

/**
 * @brief Helper functions for health status
 */
std::string healthStatusToString(DeviceHealthStatus status);
DeviceHealthStatus stringToHealthStatus(const std::string& status);

} // namespace core
} // namespace astrocomm

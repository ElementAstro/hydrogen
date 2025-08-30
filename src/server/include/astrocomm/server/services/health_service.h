#pragma once

#include "../core/service_registry.h"
#include "../core/server_interface.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <memory>

namespace astrocomm {
namespace server {
namespace services {

/**
 * @brief Health check status enumeration
 */
enum class HealthStatus {
    HEALTHY,
    WARNING,
    CRITICAL,
    UNKNOWN,
    DEGRADED
};

/**
 * @brief Health check result
 */
struct HealthCheckResult {
    std::string checkId;
    std::string checkName;
    std::string component;
    HealthStatus status;
    std::string message;
    std::unordered_map<std::string, std::string> details;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds executionTime;
    std::string errorMessage;
};

/**
 * @brief System metrics structure
 */
struct SystemMetrics {
    // CPU metrics
    double cpuUsagePercent;
    double cpuLoadAverage1m;
    double cpuLoadAverage5m;
    double cpuLoadAverage15m;
    
    // Memory metrics
    size_t totalMemoryBytes;
    size_t usedMemoryBytes;
    size_t freeMemoryBytes;
    double memoryUsagePercent;
    
    // Disk metrics
    size_t totalDiskBytes;
    size_t usedDiskBytes;
    size_t freeDiskBytes;
    double diskUsagePercent;
    
    // Network metrics
    size_t networkBytesReceived;
    size_t networkBytesSent;
    size_t networkPacketsReceived;
    size_t networkPacketsSent;
    
    // Process metrics
    size_t processCount;
    size_t threadCount;
    size_t fileDescriptorCount;
    
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Performance metrics structure
 */
struct PerformanceMetrics {
    // Request metrics
    size_t totalRequests;
    size_t successfulRequests;
    size_t failedRequests;
    double requestSuccessRate;
    std::chrono::milliseconds averageResponseTime;
    std::chrono::milliseconds p95ResponseTime;
    std::chrono::milliseconds p99ResponseTime;
    
    // Connection metrics
    size_t activeConnections;
    size_t totalConnections;
    size_t connectionErrors;
    std::chrono::milliseconds averageConnectionTime;
    
    // Protocol-specific metrics
    std::unordered_map<core::CommunicationProtocol, size_t> protocolRequests;
    std::unordered_map<core::CommunicationProtocol, std::chrono::milliseconds> protocolLatency;
    
    // Service metrics
    std::unordered_map<std::string, size_t> serviceCallCounts;
    std::unordered_map<std::string, std::chrono::milliseconds> serviceLatency;
    
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point measurementPeriodStart;
};

/**
 * @brief Health check configuration
 */
struct HealthCheckConfig {
    std::string checkId;
    std::string checkName;
    std::string component;
    std::chrono::seconds interval;
    std::chrono::seconds timeout;
    int retryAttempts;
    std::chrono::milliseconds retryDelay;
    bool enabled;
    std::unordered_map<std::string, std::string> parameters;
};

/**
 * @brief Alert configuration
 */
struct AlertConfig {
    std::string alertId;
    std::string alertName;
    std::string component;
    HealthStatus triggerStatus;
    std::string condition;
    std::chrono::seconds cooldownPeriod;
    bool enabled;
    std::vector<std::string> notificationChannels;
    std::unordered_map<std::string, std::string> parameters;
};

/**
 * @brief Health alert
 */
struct HealthAlert {
    std::string alertId;
    std::string alertName;
    std::string component;
    HealthStatus status;
    std::string message;
    std::chrono::system_clock::time_point triggeredAt;
    std::chrono::system_clock::time_point resolvedAt;
    bool isResolved;
    std::unordered_map<std::string, std::string> context;
};

/**
 * @brief Health service interface
 * 
 * Provides comprehensive health monitoring, performance metrics collection,
 * alerting, and system diagnostics for the server infrastructure.
 */
class IHealthService : public virtual core::IService {
public:
    virtual ~IHealthService() = default;

    // Health checks
    virtual bool registerHealthCheck(const HealthCheckConfig& config,
                                   std::function<HealthCheckResult()> checkFunction) = 0;
    virtual bool unregisterHealthCheck(const std::string& checkId) = 0;
    virtual HealthCheckResult executeHealthCheck(const std::string& checkId) = 0;
    virtual std::vector<HealthCheckResult> executeAllHealthChecks() = 0;
    virtual std::vector<HealthCheckResult> getHealthCheckHistory(const std::string& checkId = "",
                                                               size_t limit = 100) const = 0;

    // Overall health status
    virtual HealthStatus getOverallHealthStatus() const = 0;
    virtual std::string getHealthSummary() const = 0;
    virtual std::unordered_map<std::string, HealthStatus> getComponentHealthStatus() const = 0;
    virtual bool isSystemHealthy() const = 0;

    // System metrics
    virtual SystemMetrics getSystemMetrics() const = 0;
    virtual std::vector<SystemMetrics> getSystemMetricsHistory(std::chrono::minutes duration) const = 0;
    virtual bool startSystemMetricsCollection(std::chrono::seconds interval) = 0;
    virtual bool stopSystemMetricsCollection() = 0;

    // Performance metrics
    virtual PerformanceMetrics getPerformanceMetrics() const = 0;
    virtual std::vector<PerformanceMetrics> getPerformanceMetricsHistory(std::chrono::minutes duration) const = 0;
    virtual void recordRequest(core::CommunicationProtocol protocol, std::chrono::milliseconds responseTime, bool success) = 0;
    virtual void recordConnection(core::CommunicationProtocol protocol, std::chrono::milliseconds connectionTime, bool success) = 0;
    virtual void recordServiceCall(const std::string& serviceName, std::chrono::milliseconds latency, bool success) = 0;

    // Custom metrics
    virtual void recordCustomMetric(const std::string& metricName, double value,
                                  const std::unordered_map<std::string, std::string>& tags = {}) = 0;
    virtual double getCustomMetric(const std::string& metricName) const = 0;
    virtual std::unordered_map<std::string, double> getAllCustomMetrics() const = 0;
    virtual std::vector<std::pair<std::chrono::system_clock::time_point, double>> 
            getCustomMetricHistory(const std::string& metricName, std::chrono::minutes duration) const = 0;

    // Alerting
    virtual bool registerAlert(const AlertConfig& config) = 0;
    virtual bool unregisterAlert(const std::string& alertId) = 0;
    virtual std::vector<HealthAlert> getActiveAlerts() const = 0;
    virtual std::vector<HealthAlert> getAlertHistory(const std::string& component = "",
                                                   size_t limit = 100) const = 0;
    virtual bool acknowledgeAlert(const std::string& alertId) = 0;
    virtual bool resolveAlert(const std::string& alertId) = 0;

    // Diagnostics
    virtual std::string generateDiagnosticReport() const = 0;
    virtual std::unordered_map<std::string, std::string> getSystemInfo() const = 0;
    virtual std::vector<std::string> getActiveProcesses() const = 0;
    virtual std::unordered_map<std::string, std::string> getEnvironmentVariables() const = 0;
    virtual std::vector<std::string> getNetworkInterfaces() const = 0;

    // Resource monitoring
    virtual bool setResourceThreshold(const std::string& resource, double warningThreshold, double criticalThreshold) = 0;
    virtual std::unordered_map<std::string, std::pair<double, double>> getResourceThresholds() const = 0;
    virtual std::unordered_map<std::string, double> getCurrentResourceUsage() const = 0;
    virtual std::vector<std::string> getResourceAlerts() const = 0;

    // Service dependency monitoring
    virtual bool addServiceDependency(const std::string& serviceName, const std::string& dependencyName) = 0;
    virtual bool removeServiceDependency(const std::string& serviceName, const std::string& dependencyName) = 0;
    virtual std::vector<std::string> getServiceDependencies(const std::string& serviceName) const = 0;
    virtual HealthStatus getDependencyHealth(const std::string& serviceName) const = 0;

    // Uptime monitoring
    virtual std::chrono::seconds getUptime() const = 0;
    virtual std::chrono::system_clock::time_point getStartTime() const = 0;
    virtual double getAvailabilityPercentage(std::chrono::hours period) const = 0;
    virtual std::vector<std::pair<std::chrono::system_clock::time_point, std::chrono::seconds>> 
            getDowntimeHistory(std::chrono::hours period) const = 0;

    // Configuration
    virtual void setHealthCheckInterval(std::chrono::seconds interval) = 0;
    virtual void setMetricsRetentionPeriod(std::chrono::hours period) = 0;
    virtual void setAlertCooldownPeriod(std::chrono::seconds period) = 0;
    virtual void enableHealthCheck(const std::string& checkId, bool enabled) = 0;
    virtual void enableAlert(const std::string& alertId, bool enabled) = 0;

    // Event callbacks
    using HealthEventCallback = std::function<void(const HealthCheckResult& result)>;
    using AlertEventCallback = std::function<void(const HealthAlert& alert, const std::string& event)>;
    using MetricsEventCallback = std::function<void(const std::string& metricName, double value)>;
    using ThresholdEventCallback = std::function<void(const std::string& resource, double value, double threshold)>;

    virtual void setHealthEventCallback(HealthEventCallback callback) = 0;
    virtual void setAlertEventCallback(AlertEventCallback callback) = 0;
    virtual void setMetricsEventCallback(MetricsEventCallback callback) = 0;
    virtual void setThresholdEventCallback(ThresholdEventCallback callback) = 0;

    // Export and reporting
    virtual bool exportMetrics(const std::string& filePath, const std::string& format = "json") const = 0;
    virtual std::string getMetricsInPrometheusFormat() const = 0;
    virtual std::string getHealthStatusInJsonFormat() const = 0;
    virtual bool generateHealthReport(const std::string& filePath) const = 0;
};

/**
 * @brief Health service factory
 */
class HealthServiceFactory : public core::IServiceFactory {
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
} // namespace astrocomm

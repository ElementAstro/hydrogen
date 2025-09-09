#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <queue>

#include <spdlog/spdlog.h>

#include "hydrogen/server/services/device_monitoring_service.h"
#include "hydrogen/server/services/monitoring_data_structures.h"

namespace hydrogen {
namespace server {
namespace services {

// Forward declarations
class MetricsCollector;
class HealthMonitor;

/**
 * @brief Implementation of the device monitoring service
 * 
 * This class provides real-time monitoring capabilities including metrics collection,
 * health monitoring, alerting, and diagnostics for devices in the Hydrogen system.
 */
class DeviceMonitoringServiceImpl : public IDeviceMonitoringService {
public:
    explicit DeviceMonitoringServiceImpl();
    ~DeviceMonitoringServiceImpl() override;

    // IDeviceMonitoringService implementation
    std::string subscribeToDeviceMetrics(
        const std::string& deviceId,
        const MetricsSubscription& subscription,
        MetricsCallback callback
    ) override;
    
    std::string subscribeToSystemMetrics(
        const SystemMetricsSubscription& subscription,
        SystemMetricsCallback callback
    ) override;
    
    bool unsubscribeFromMetrics(const std::string& subscriptionId) override;

    DevicePerformanceMetrics getDevicePerformanceMetrics(
        const std::string& deviceId,
        const TimeRange& timeRange,
        const MetricsAggregation& aggregation = MetricsAggregation{}
    ) const override;
    
    SystemPerformanceMetrics getSystemPerformanceMetrics(
        const TimeRange& timeRange,
        const MetricsAggregation& aggregation = MetricsAggregation{}
    ) const override;
    
    std::vector<PerformanceAlert> getPerformanceAlerts(
        const std::string& deviceId = "",
        const AlertFilter& filter = AlertFilter{}
    ) const override;

    DeviceHealthReport getDeviceHealthReport(
        const std::string& deviceId,
        const HealthReportOptions& options = HealthReportOptions{}
    ) const override;
    
    SystemHealthReport getSystemHealthReport(
        const HealthReportOptions& options = HealthReportOptions{}
    ) const override;
    
    std::string runDeviceDiagnostics(
        const std::string& deviceId,
        const DiagnosticsOptions& options = DiagnosticsOptions{}
    ) override;
    
    DiagnosticsResult getDiagnosticsResult(const std::string& diagnosticsId) const override;

    // Service lifecycle
    bool start() override;
    bool stop() override;
    bool isRunning() const override;
    
    // Service configuration
    void setConfiguration(const json& config) override;
    json getConfiguration() const override;

private:
    // Internal structures
    struct MetricsSubscriptionInfo {
        std::string subscriptionId;
        std::string deviceId;
        MetricsSubscription subscription;
        MetricsCallback callback;
        std::chrono::system_clock::time_point lastUpdate;
        bool isActive = true;
    };
    
    struct SystemMetricsSubscriptionInfo {
        std::string subscriptionId;
        SystemMetricsSubscription subscription;
        SystemMetricsCallback callback;
        std::chrono::system_clock::time_point lastUpdate;
        bool isActive = true;
    };

    // Core components
    std::unique_ptr<MetricsCollector> metricsCollector_;
    std::unique_ptr<HealthMonitor> healthMonitor_;

    // Service state
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    
    // Configuration
    json configuration_;
    mutable std::mutex configMutex_;

    // Subscription management
    std::unordered_map<std::string, std::unique_ptr<MetricsSubscriptionInfo>> deviceSubscriptions_;
    std::unordered_map<std::string, std::unique_ptr<SystemMetricsSubscriptionInfo>> systemSubscriptions_;
    mutable std::mutex subscriptionsMutex_;
    
    // Threading
    std::unique_ptr<std::thread> metricsThread_;
    std::unique_ptr<std::thread> healthThread_;
    std::unique_ptr<std::thread> alertThread_;
    std::condition_variable shutdownCondition_;
    std::mutex shutdownMutex_;

    // Internal methods
    bool initialize();
    void shutdown();
    
    // Thread functions
    void metricsThreadFunction();
    void healthThreadFunction();
    void alertThreadFunction();
    
    // Subscription management
    std::string generateSubscriptionId();
    void processDeviceSubscriptions();
    void processSystemSubscriptions();
    void cleanupInactiveSubscriptions();
    
    // Metrics processing
    void collectAndProcessMetrics();
    void updateDeviceMetrics(const std::string& deviceId);
    void updateSystemMetrics();
    
    // Health monitoring
    void performHealthChecks();
    void updateDeviceHealth(const std::string& deviceId);
    void updateSystemHealth();
    
    // Alert processing
    void processAlerts();
    void checkPerformanceAlerts();
    void checkHealthAlerts();
    
    // Utility methods
    bool isValidDeviceId(const std::string& deviceId) const;
    std::chrono::milliseconds getMetricsInterval() const;
    std::chrono::milliseconds getHealthCheckInterval() const;
    std::chrono::milliseconds getAlertCheckInterval() const;
    
    // Configuration helpers
    void loadDefaultConfiguration();
    bool validateConfiguration(const json& config) const;
    void applyConfiguration(const json& config);
};

/**
 * @brief Metrics collector component
 * 
 * Responsible for collecting performance metrics from devices and the system.
 */
class MetricsCollector {
public:
    explicit MetricsCollector();
    ~MetricsCollector();

    bool initialize(const json& config);
    void shutdown();
    
    // Metrics collection
    DevicePerformanceMetrics collectDeviceMetrics(const std::string& deviceId) const;
    SystemPerformanceMetrics collectSystemMetrics() const;
    
    // Historical metrics
    std::vector<DevicePerformanceMetrics> getDeviceMetricsHistory(
        const std::string& deviceId,
        const TimeRange& timeRange
    ) const;
    
    std::vector<SystemPerformanceMetrics> getSystemMetricsHistory(
        const TimeRange& timeRange
    ) const;
    
    // Metrics aggregation
    DevicePerformanceMetrics aggregateDeviceMetrics(
        const std::vector<DevicePerformanceMetrics>& metrics,
        const MetricsAggregation& aggregation
    ) const;
    
    SystemPerformanceMetrics aggregateSystemMetrics(
        const std::vector<SystemPerformanceMetrics>& metrics,
        const MetricsAggregation& aggregation
    ) const;

private:
    // Configuration
    json config_;
    
    // Metrics storage
    mutable std::mutex metricsMutex_;
    std::unordered_map<std::string, std::queue<DevicePerformanceMetrics>> deviceMetricsHistory_;
    std::queue<SystemPerformanceMetrics> systemMetricsHistory_;
    
    // Collection parameters
    size_t maxHistorySize_ = 10000;
    std::chrono::milliseconds metricsRetention_{24 * 60 * 60 * 1000}; // 24 hours
    
    // Internal methods
    ResponseTimeMetrics collectResponseTimeMetrics(const std::string& deviceId) const;
    ThroughputMetrics collectThroughputMetrics(const std::string& deviceId) const;
    ErrorMetrics collectErrorMetrics(const std::string& deviceId) const;
    ResourceMetrics collectResourceMetrics(const std::string& deviceId) const;
    
    void storeDeviceMetrics(const std::string& deviceId, const DevicePerformanceMetrics& metrics);
    void storeSystemMetrics(const SystemPerformanceMetrics& metrics);
    void cleanupOldMetrics();
    
    // Aggregation helpers
    double calculatePercentile(const std::vector<double>& values, double percentile) const;
    double calculateAverage(const std::vector<double>& values) const;
    double calculateSum(const std::vector<double>& values) const;
    double calculateMin(const std::vector<double>& values) const;
    double calculateMax(const std::vector<double>& values) const;
};

/**
 * @brief Health monitor component
 * 
 * Responsible for monitoring device and system health status.
 */
class HealthMonitor {
public:
    explicit HealthMonitor();
    ~HealthMonitor();

    bool initialize(const json& config);
    void shutdown();
    
    // Health monitoring
    DeviceHealthReport generateDeviceHealthReport(
        const std::string& deviceId,
        const HealthReportOptions& options
    ) const;
    
    SystemHealthReport generateSystemHealthReport(
        const HealthReportOptions& options
    ) const;
    
    // Health checks
    bool performDeviceHealthCheck(const std::string& deviceId);
    bool performSystemHealthCheck();
    
    // Health status
    HealthStatus getDeviceHealthStatus(const std::string& deviceId) const;
    HealthStatus getSystemHealthStatus() const;

private:
    // Configuration
    json config_;
    
    // Health data storage
    mutable std::mutex healthMutex_;
    std::unordered_map<std::string, HealthStatus> deviceHealthStatus_;
    std::unordered_map<std::string, std::vector<HealthIndicator>> deviceHealthIndicators_;
    HealthStatus systemHealthStatus_ = HealthStatus::UNKNOWN;
    std::vector<HealthIndicator> systemHealthIndicators_;
    
    // Health check parameters
    std::chrono::milliseconds healthCheckTimeout_{30000};
    double healthScoreThreshold_ = 0.8;
    
    // Internal methods
    std::vector<HealthIndicator> collectDeviceHealthIndicators(const std::string& deviceId) const;
    std::vector<HealthIndicator> collectSystemHealthIndicators() const;
    HealthStatus calculateHealthStatus(const std::vector<HealthIndicator>& indicators) const;
    double calculateHealthScore(const std::vector<HealthIndicator>& indicators) const;
    
    // Health indicator collectors
    HealthIndicator checkDeviceConnectivity(const std::string& deviceId) const;
    HealthIndicator checkDeviceResponseTime(const std::string& deviceId) const;
    HealthIndicator checkDeviceErrorRate(const std::string& deviceId) const;
    HealthIndicator checkDeviceResourceUsage(const std::string& deviceId) const;
    
    HealthIndicator checkSystemResourceUsage() const;
    HealthIndicator checkSystemConnectivity() const;
    HealthIndicator checkSystemPerformance() const;
};

} // namespace services
} // namespace server
} // namespace hydrogen

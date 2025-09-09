#pragma once

#include "unified_connection_architecture.h"
#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

namespace hydrogen {
namespace core {
namespace connection {

/**
 * @brief Health check result
 */
struct HealthCheckResult {
    bool isHealthy = false;
    std::chrono::milliseconds latency{0};
    std::string errorMessage;
    std::chrono::system_clock::time_point timestamp;
    
    // Performance metrics
    double packetLossRate = 0.0;
    double throughputMbps = 0.0;
    uint32_t consecutiveFailures = 0;
};

/**
 * @brief Connection performance metrics
 */
struct ConnectionPerformanceMetrics {
    // Latency metrics
    std::chrono::milliseconds averageLatency{0};
    std::chrono::milliseconds minLatency{std::chrono::milliseconds::max()};
    std::chrono::milliseconds maxLatency{0};
    std::chrono::milliseconds p95Latency{0};
    std::chrono::milliseconds p99Latency{0};
    
    // Throughput metrics
    double averageThroughputMbps = 0.0;
    double peakThroughputMbps = 0.0;
    uint64_t totalBytesTransferred = 0;
    
    // Reliability metrics
    double uptimePercentage = 100.0;
    uint32_t totalHealthChecks = 0;
    uint32_t failedHealthChecks = 0;
    uint32_t consecutiveSuccesses = 0;
    uint32_t maxConsecutiveFailures = 0;
    
    // Connection quality
    double connectionQualityScore = 100.0; // 0-100 scale
    std::string qualityGrade = "A"; // A, B, C, D, F
    
    std::chrono::system_clock::time_point lastUpdated;
};

/**
 * @brief Health monitoring configuration
 */
struct HealthMonitorConfig {
    // Check intervals
    std::chrono::seconds healthCheckInterval{10};
    std::chrono::seconds performanceUpdateInterval{30};
    std::chrono::seconds metricsReportInterval{300}; // 5 minutes
    
    // Thresholds
    std::chrono::milliseconds latencyWarningThreshold{1000};
    std::chrono::milliseconds latencyErrorThreshold{5000};
    double packetLossWarningThreshold = 0.01; // 1%
    double packetLossErrorThreshold = 0.05;   // 5%
    uint32_t consecutiveFailureThreshold = 3;
    
    // Performance tracking
    size_t latencyHistorySize = 1000;
    size_t throughputHistorySize = 100;
    bool enableDetailedMetrics = true;
    bool enableAutoRecovery = true;
    
    // Recovery settings
    std::chrono::seconds recoveryAttemptInterval{30};
    uint32_t maxRecoveryAttempts = 5;
    std::chrono::seconds recoveryBackoffMultiplier{2};
};

/**
 * @brief Callback types for health monitoring events
 */
using HealthStatusCallback = std::function<void(bool isHealthy, const HealthCheckResult&)>;
using PerformanceUpdateCallback = std::function<void(const ConnectionPerformanceMetrics&)>;
using RecoveryAttemptCallback = std::function<void(uint32_t attemptNumber, bool success)>;

/**
 * @brief Advanced connection health monitor
 */
class AdvancedConnectionHealthMonitor {
public:
    explicit AdvancedConnectionHealthMonitor(
        std::shared_ptr<IProtocolConnection> connection,
        const HealthMonitorConfig& config = HealthMonitorConfig{});
    
    ~AdvancedConnectionHealthMonitor();
    
    // Control methods
    void start();
    void stop();
    bool isRunning() const;
    
    // Health status
    bool isHealthy() const;
    HealthCheckResult getLastHealthCheck() const;
    std::vector<HealthCheckResult> getHealthHistory(size_t maxResults = 100) const;
    
    // Performance metrics
    ConnectionPerformanceMetrics getPerformanceMetrics() const;
    std::chrono::milliseconds getCurrentLatency() const;
    double getCurrentThroughput() const;
    double getConnectionQuality() const;
    
    // Configuration
    void updateConfig(const HealthMonitorConfig& config);
    HealthMonitorConfig getConfig() const;
    
    // Callbacks
    void setHealthStatusCallback(HealthStatusCallback callback);
    void setPerformanceUpdateCallback(PerformanceUpdateCallback callback);
    void setRecoveryAttemptCallback(RecoveryAttemptCallback callback);
    
    // Manual operations
    HealthCheckResult performHealthCheck();
    void triggerRecovery();
    void resetMetrics();

private:
    // Core monitoring loops
    void healthCheckLoop();
    void performanceUpdateLoop();
    void recoveryLoop();
    
    // Health check implementations
    HealthCheckResult performLatencyCheck();
    HealthCheckResult performThroughputCheck();
    HealthCheckResult performConnectivityCheck();
    
    // Performance calculation
    void updateLatencyMetrics(std::chrono::milliseconds latency);
    void updateThroughputMetrics(double throughputMbps);
    void calculatePerformanceMetrics();
    void calculateConnectionQuality();
    
    // Recovery mechanisms
    bool attemptConnectionRecovery();
    void handleRecoverySuccess();
    void handleRecoveryFailure();
    
    // Utility methods
    std::string generateHealthCheckId() const;
    void cleanupOldHistory();
    void notifyHealthStatusChange(bool isHealthy, const HealthCheckResult& result);
    void notifyPerformanceUpdate();
    void notifyRecoveryAttempt(uint32_t attemptNumber, bool success);
    
    // Connection and configuration
    std::shared_ptr<IProtocolConnection> connection_;
    HealthMonitorConfig config_;
    mutable std::mutex configMutex_;
    
    // State management
    std::atomic<bool> running_{false};
    std::atomic<bool> healthy_{false};
    std::atomic<bool> recoveryInProgress_{false};
    
    // Monitoring threads
    std::thread healthCheckThread_;
    std::thread performanceThread_;
    std::thread recoveryThread_;
    
    // Health check data
    std::vector<HealthCheckResult> healthHistory_;
    mutable std::mutex healthHistoryMutex_;
    HealthCheckResult lastHealthCheck_;
    
    // Performance data
    ConnectionPerformanceMetrics performanceMetrics_;
    std::vector<std::chrono::milliseconds> latencyHistory_;
    std::vector<double> throughputHistory_;
    mutable std::mutex performanceMutex_;
    
    // Recovery state
    std::atomic<uint32_t> recoveryAttempts_{0};
    std::chrono::system_clock::time_point lastRecoveryAttempt_;
    
    // Callbacks
    HealthStatusCallback healthStatusCallback_;
    PerformanceUpdateCallback performanceUpdateCallback_;
    RecoveryAttemptCallback recoveryAttemptCallback_;
    mutable std::mutex callbackMutex_;
    
    // Timing
    std::chrono::system_clock::time_point startTime_;
    std::chrono::system_clock::time_point lastHealthCheck_;
    std::chrono::system_clock::time_point lastPerformanceUpdate_;
};

/**
 * @brief Connection health aggregator for multiple connections
 */
class ConnectionHealthAggregator {
public:
    ConnectionHealthAggregator();
    ~ConnectionHealthAggregator();
    
    // Connection management
    void addConnection(const std::string& connectionId, 
                      std::shared_ptr<AdvancedConnectionHealthMonitor> monitor);
    void removeConnection(const std::string& connectionId);
    
    // Aggregate health status
    bool areAllConnectionsHealthy() const;
    double getOverallHealthScore() const;
    std::vector<std::string> getUnhealthyConnections() const;
    
    // Aggregate metrics
    ConnectionPerformanceMetrics getAggregateMetrics() const;
    std::map<std::string, ConnectionPerformanceMetrics> getAllConnectionMetrics() const;
    
    // Monitoring control
    void startAllMonitoring();
    void stopAllMonitoring();
    
    // Callbacks
    void setAggregateHealthCallback(std::function<void(double healthScore)> callback);
    void setConnectionEventCallback(std::function<void(const std::string&, bool)> callback);

private:
    void updateAggregateHealth();
    void handleConnectionHealthChange(const std::string& connectionId, bool isHealthy);
    
    std::map<std::string, std::shared_ptr<AdvancedConnectionHealthMonitor>> monitors_;
    mutable std::mutex monitorsMutex_;
    
    std::atomic<double> overallHealthScore_{100.0};
    
    std::function<void(double)> aggregateHealthCallback_;
    std::function<void(const std::string&, bool)> connectionEventCallback_;
    mutable std::mutex callbackMutex_;
};

/**
 * @brief Connection performance optimizer
 */
class ConnectionPerformanceOptimizer {
public:
    explicit ConnectionPerformanceOptimizer(std::shared_ptr<IProtocolConnection> connection);
    ~ConnectionPerformanceOptimizer();
    
    // Optimization control
    void enableOptimization(bool enable);
    bool isOptimizationEnabled() const;
    
    // Optimization strategies
    void optimizeForLatency();
    void optimizeForThroughput();
    void optimizeForReliability();
    void applyBalancedOptimization();
    
    // Performance tuning
    void adjustBufferSizes(size_t readBuffer, size_t writeBuffer);
    void adjustTimeouts(std::chrono::milliseconds connectTimeout, 
                       std::chrono::milliseconds readTimeout,
                       std::chrono::milliseconds writeTimeout);
    void enableCompression(bool enable);
    void enableKeepAlive(bool enable, std::chrono::seconds interval = std::chrono::seconds(30));
    
    // Monitoring integration
    void setHealthMonitor(std::shared_ptr<AdvancedConnectionHealthMonitor> monitor);
    void enableAdaptiveOptimization(bool enable);

private:
    void performAdaptiveOptimization();
    void analyzePerformanceMetrics();
    void applyOptimizations();
    
    std::shared_ptr<IProtocolConnection> connection_;
    std::shared_ptr<AdvancedConnectionHealthMonitor> healthMonitor_;
    
    std::atomic<bool> optimizationEnabled_{false};
    std::atomic<bool> adaptiveOptimizationEnabled_{false};
    
    // Current optimization settings
    size_t currentReadBufferSize_ = 8192;
    size_t currentWriteBufferSize_ = 8192;
    std::chrono::milliseconds currentConnectTimeout_{5000};
    std::chrono::milliseconds currentReadTimeout_{30000};
    std::chrono::milliseconds currentWriteTimeout_{5000};
    bool compressionEnabled_ = false;
    bool keepAliveEnabled_ = false;
    
    mutable std::mutex optimizationMutex_;
};

} // namespace connection
} // namespace core
} // namespace hydrogen

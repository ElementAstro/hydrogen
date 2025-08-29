#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <queue>
#include <thread>
#include <nlohmann/json.hpp>

namespace astrocomm {
namespace core {

using json = nlohmann::json;

/**
 * @brief Performance metric types
 */
enum class MetricType {
    RESPONSE_TIME,      // Command response time
    THROUGHPUT,         // Commands per second
    ERROR_RATE,         // Errors per command
    CONNECTION_TIME,    // Time to establish connection
    MEMORY_USAGE,       // Memory consumption
    CPU_USAGE,          // CPU utilization
    NETWORK_LATENCY,    // Network round-trip time
    QUEUE_DEPTH,        // Command queue depth
    CUSTOM              // Custom metric type
};

/**
 * @brief Individual performance measurement
 */
struct PerformanceMeasurement {
    MetricType type;
    std::string name;
    double value;
    std::string unit;
    std::chrono::system_clock::time_point timestamp;
    json metadata;
    
    json toJson() const;
    static PerformanceMeasurement fromJson(const json& j);
};

/**
 * @brief Aggregated performance statistics
 */
struct PerformanceStatistics {
    MetricType type;
    std::string name;
    double min = 0.0;
    double max = 0.0;
    double average = 0.0;
    double median = 0.0;
    double standardDeviation = 0.0;
    uint64_t sampleCount = 0;
    std::chrono::system_clock::time_point firstSample;
    std::chrono::system_clock::time_point lastSample;
    
    json toJson() const;
    static PerformanceStatistics fromJson(const json& j);
};

/**
 * @brief Performance alert configuration
 */
struct PerformanceAlert {
    std::string name;
    MetricType metricType;
    std::string condition; // "greater_than", "less_than", "equals", "not_equals"
    double threshold;
    std::chrono::milliseconds duration{0}; // How long condition must persist
    bool enabled = true;
    
    json toJson() const;
    static PerformanceAlert fromJson(const json& j);
};

/**
 * @brief Performance trend analysis result
 */
struct PerformanceTrend {
    std::string deviceId;
    MetricType metricType;
    std::string trendDirection; // "improving", "degrading", "stable"
    double trendSlope;
    double confidence; // 0.0 - 1.0
    std::chrono::system_clock::time_point analysisTime;
    
    json toJson() const;
    static PerformanceTrend fromJson(const json& j);
};

/**
 * @brief Interface for performance metrics collection
 */
class IPerformanceMetricsCollector {
public:
    virtual ~IPerformanceMetricsCollector() = default;
    
    /**
     * @brief Record a performance measurement
     * @param deviceId Device identifier
     * @param measurement Performance measurement
     */
    virtual void recordMeasurement(const std::string& deviceId, 
                                  const PerformanceMeasurement& measurement) = 0;
    
    /**
     * @brief Record a simple metric value
     * @param deviceId Device identifier
     * @param metricType Type of metric
     * @param name Metric name
     * @param value Metric value
     * @param unit Unit of measurement
     */
    virtual void recordMetric(const std::string& deviceId,
                             MetricType metricType,
                             const std::string& name,
                             double value,
                             const std::string& unit = "") = 0;
    
    /**
     * @brief Get performance statistics for a device and metric
     * @param deviceId Device identifier
     * @param metricType Type of metric
     * @param name Metric name
     * @return Performance statistics
     */
    virtual PerformanceStatistics getStatistics(const std::string& deviceId,
                                               MetricType metricType,
                                               const std::string& name) const = 0;
    
    /**
     * @brief Get all statistics for a device
     * @param deviceId Device identifier
     * @return Map of metric names to statistics
     */
    virtual std::unordered_map<std::string, PerformanceStatistics> 
        getAllStatistics(const std::string& deviceId) const = 0;
    
    /**
     * @brief Analyze performance trends
     * @param deviceId Device identifier
     * @param metricType Type of metric to analyze
     * @param timeWindow Time window for analysis
     * @return Performance trend analysis
     */
    virtual PerformanceTrend analyzeTrend(const std::string& deviceId,
                                         MetricType metricType,
                                         std::chrono::hours timeWindow = std::chrono::hours(24)) const = 0;
    
    /**
     * @brief Register performance alert
     * @param deviceId Device identifier
     * @param alert Alert configuration
     */
    virtual void registerAlert(const std::string& deviceId, const PerformanceAlert& alert) = 0;
    
    /**
     * @brief Remove performance alert
     * @param deviceId Device identifier
     * @param alertName Alert name
     */
    virtual void removeAlert(const std::string& deviceId, const std::string& alertName) = 0;
    
    /**
     * @brief Set callback for performance alerts
     * @param callback Function to call when alerts are triggered
     */
    virtual void setAlertCallback(
        std::function<void(const std::string&, const PerformanceAlert&, double)> callback) = 0;
    
    /**
     * @brief Start metrics collection
     */
    virtual void start() = 0;
    
    /**
     * @brief Stop metrics collection
     */
    virtual void stop() = 0;
    
    /**
     * @brief Clear all metrics for a device
     * @param deviceId Device identifier
     */
    virtual void clearMetrics(const std::string& deviceId) = 0;
    
    /**
     * @brief Export metrics to file
     * @param filename File to export to
     * @param format Export format ("json", "csv")
     * @return True if successful
     */
    virtual bool exportMetrics(const std::string& filename, const std::string& format = "json") const = 0;
};

/**
 * @brief Concrete implementation of performance metrics collector
 */
class PerformanceMetricsCollector : public IPerformanceMetricsCollector {
public:
    PerformanceMetricsCollector();
    virtual ~PerformanceMetricsCollector();
    
    // IPerformanceMetricsCollector implementation
    void recordMeasurement(const std::string& deviceId, 
                          const PerformanceMeasurement& measurement) override;
    void recordMetric(const std::string& deviceId,
                     MetricType metricType,
                     const std::string& name,
                     double value,
                     const std::string& unit = "") override;
    PerformanceStatistics getStatistics(const std::string& deviceId,
                                       MetricType metricType,
                                       const std::string& name) const override;
    std::unordered_map<std::string, PerformanceStatistics> 
        getAllStatistics(const std::string& deviceId) const override;
    PerformanceTrend analyzeTrend(const std::string& deviceId,
                                 MetricType metricType,
                                 std::chrono::hours timeWindow = std::chrono::hours(24)) const override;
    void registerAlert(const std::string& deviceId, const PerformanceAlert& alert) override;
    void removeAlert(const std::string& deviceId, const std::string& alertName) override;
    void setAlertCallback(
        std::function<void(const std::string&, const PerformanceAlert&, double)> callback) override;
    void start() override;
    void stop() override;
    void clearMetrics(const std::string& deviceId) override;
    bool exportMetrics(const std::string& filename, const std::string& format = "json") const override;
    
    /**
     * @brief Get singleton instance
     * @return Reference to singleton instance
     */
    static PerformanceMetricsCollector& getInstance();
    
    /**
     * @brief Set maximum number of measurements to keep per metric
     * @param maxMeasurements Maximum measurements to keep
     */
    void setMaxMeasurements(size_t maxMeasurements);
    
    /**
     * @brief Set metrics aggregation interval
     * @param interval Interval for calculating statistics
     */
    void setAggregationInterval(std::chrono::seconds interval);
    
    /**
     * @brief Enable/disable real-time statistics calculation
     * @param enabled Whether to calculate statistics in real-time
     */
    void setRealTimeStats(bool enabled);

private:
    struct MetricData {
        std::vector<PerformanceMeasurement> measurements;
        PerformanceStatistics statistics;
        std::chrono::system_clock::time_point lastAggregation;
    };
    
    // Thread-safe storage
    mutable std::mutex metricsMutex_;
    std::unordered_map<std::string, std::unordered_map<std::string, MetricData>> deviceMetrics_;
    
    mutable std::mutex alertsMutex_;
    std::unordered_map<std::string, std::vector<PerformanceAlert>> deviceAlerts_;
    
    // Configuration
    std::atomic<size_t> maxMeasurements_{1000};
    std::atomic<bool> realTimeStats_{true};
    std::chrono::seconds aggregationInterval_{60};
    
    // Callbacks
    std::function<void(const std::string&, const PerformanceAlert&, double)> alertCallback_;
    
    // Thread management
    std::atomic<bool> running_{false};
    std::thread aggregationThread_;
    std::thread alertThread_;
    
    // Helper methods
    void calculateStatistics(const std::string& deviceId, const std::string& metricKey);
    void checkAlerts(const std::string& deviceId);
    void startAggregationThread();
    void stopAggregationThread();
    void aggregationThreadFunction();
    void startAlertThread();
    void stopAlertThread();
    void alertThreadFunction();
    std::string getMetricKey(MetricType type, const std::string& name) const;
    void trimMeasurements(std::vector<PerformanceMeasurement>& measurements);
    double calculateMedian(std::vector<double> values) const;
    double calculateStandardDeviation(const std::vector<double>& values, double mean) const;
};

/**
 * @brief Helper functions for performance metrics
 */
std::string metricTypeToString(MetricType type);
MetricType stringToMetricType(const std::string& type);

} // namespace core
} // namespace astrocomm

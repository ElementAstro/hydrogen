#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>
#include <cstdint>
#include <vector>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace device {

/**
 * @brief Performance metrics structure
 */
struct PerformanceMetrics {
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastUpdateTime;
    
    // Timing metrics
    double averageUpdateTime = 0.0;     // Average update time in milliseconds
    double maxUpdateTime = 0.0;         // Maximum update time in milliseconds
    double minUpdateTime = 1000.0;      // Minimum update time in milliseconds
    uint64_t updateCount = 0;           // Total number of updates
    
    // Memory metrics
    size_t memoryUsage = 0;             // Current memory usage in bytes
    size_t peakMemoryUsage = 0;         // Peak memory usage in bytes
    
    // Communication metrics
    uint64_t messagesSent = 0;          // Total messages sent
    uint64_t messagesReceived = 0;      // Total messages received
    uint64_t bytesTransmitted = 0;      // Total bytes transmitted
    uint64_t bytesReceived = 0;         // Total bytes received
    
    // Error metrics
    uint64_t errorCount = 0;            // Total error count
    uint64_t reconnectCount = 0;        // Total reconnection attempts
    
    // Device-specific metrics
    std::unordered_map<std::string, double> customMetrics;
};

/**
 * @brief Performance monitor for device operations
 * 
 * This class provides comprehensive performance monitoring capabilities
 * for device operations, including timing, memory usage, and communication metrics.
 */
class PerformanceMonitor {
public:
    /**
     * @brief Constructor
     * @param deviceId Device identifier
     */
    explicit PerformanceMonitor(const std::string& deviceId);

    /**
     * @brief Destructor
     */
    ~PerformanceMonitor();

    /**
     * @brief Start timing an operation
     * @param operationName Name of the operation
     */
    void startTiming(const std::string& operationName);

    /**
     * @brief End timing an operation
     * @param operationName Name of the operation
     * @return Duration in milliseconds
     */
    double endTiming(const std::string& operationName);

    /**
     * @brief Record a custom metric
     * @param metricName Name of the metric
     * @param value Metric value
     */
    void recordMetric(const std::string& metricName, double value);

    /**
     * @brief Increment a counter metric
     * @param metricName Name of the metric
     * @param increment Increment value (default: 1)
     */
    void incrementCounter(const std::string& metricName, uint64_t increment = 1);

    /**
     * @brief Record memory usage
     * @param bytes Memory usage in bytes
     */
    void recordMemoryUsage(size_t bytes);

    /**
     * @brief Record message transmission
     * @param messageSize Size of the message in bytes
     * @param sent True if sent, false if received
     */
    void recordMessage(size_t messageSize, bool sent);

    /**
     * @brief Record an error
     * @param errorType Type of error
     */
    void recordError(const std::string& errorType);

    /**
     * @brief Get current performance metrics
     * @return Performance metrics structure
     */
    PerformanceMetrics getMetrics() const;

    /**
     * @brief Get performance summary as JSON
     * @return JSON object with performance data
     */
    nlohmann::json getPerformanceSummary() const;

    /**
     * @brief Reset all metrics
     */
    void reset();

    /**
     * @brief Enable/disable performance monitoring
     * @param enabled True to enable monitoring
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if monitoring is enabled
     * @return True if monitoring is enabled
     */
    bool isEnabled() const;

private:
    std::string deviceId_;
    mutable std::mutex metricsMutex_;
    PerformanceMetrics metrics_;
    std::atomic<bool> enabled_;
    
    // Timing operations
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> timingOperations_;
    
    // Error counters
    std::unordered_map<std::string, uint64_t> errorCounters_;
    
    /**
     * @brief Update timing statistics
     * @param duration Duration in milliseconds
     */
    void updateTimingStats(double duration);
};

/**
 * @brief RAII timing helper class
 */
class ScopedTimer {
public:
    /**
     * @brief Constructor
     * @param monitor Performance monitor
     * @param operationName Name of the operation to time
     */
    ScopedTimer(PerformanceMonitor& monitor, const std::string& operationName);

    /**
     * @brief Destructor - automatically ends timing
     */
    ~ScopedTimer();

private:
    PerformanceMonitor& monitor_;
    std::string operationName_;
};

/**
 * @brief Memory pool for efficient object allocation
 * 
 * This class provides a simple memory pool for frequently allocated objects
 * to reduce memory fragmentation and allocation overhead.
 */
template<typename T, size_t PoolSize = 100>
class ObjectPool {
public:
    /**
     * @brief Constructor
     */
    ObjectPool() {
        for (size_t i = 0; i < PoolSize; ++i) {
            pool_.push_back(std::make_unique<T>());
        }
    }

    /**
     * @brief Acquire an object from the pool
     * @return Unique pointer to object
     */
    std::unique_ptr<T> acquire() {
        std::lock_guard<std::mutex> lock(poolMutex_);
        
        if (!pool_.empty()) {
            auto obj = std::move(pool_.back());
            pool_.pop_back();
            return obj;
        }
        
        // Pool is empty, create new object
        return std::make_unique<T>();
    }

    /**
     * @brief Return an object to the pool
     * @param obj Object to return
     */
    void release(std::unique_ptr<T> obj) {
        if (!obj) return;
        
        std::lock_guard<std::mutex> lock(poolMutex_);
        
        if (pool_.size() < PoolSize) {
            pool_.push_back(std::move(obj));
        }
        // If pool is full, let the object be destroyed
    }

    /**
     * @brief Get current pool size
     * @return Number of available objects in pool
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(poolMutex_);
        return pool_.size();
    }

private:
    mutable std::mutex poolMutex_;
    std::vector<std::unique_ptr<T>> pool_;
};

/**
 * @brief Convenience macros for performance monitoring
 */
#define DEVICE_PERF_TIMER(monitor, operation) \
    hydrogen::device::ScopedTimer timer(monitor, operation)

#define DEVICE_PERF_RECORD(monitor, metric, value) \
    if (monitor.isEnabled()) monitor.recordMetric(metric, value)

#define DEVICE_PERF_INCREMENT(monitor, counter) \
    if (monitor.isEnabled()) monitor.incrementCounter(counter)

} // namespace device
} // namespace hydrogen

#include "hydrogen/device/performance_monitor.h"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace hydrogen {
namespace device {

PerformanceMonitor::PerformanceMonitor(const std::string& deviceId)
    : deviceId_(deviceId), enabled_(true) {
    metrics_.startTime = std::chrono::steady_clock::now();
    metrics_.lastUpdateTime = metrics_.startTime;
}

PerformanceMonitor::~PerformanceMonitor() = default;

void PerformanceMonitor::startTiming(const std::string& operationName) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(metricsMutex_);
    timingOperations_[operationName] = std::chrono::steady_clock::now();
}

double PerformanceMonitor::endTiming(const std::string& operationName) {
    if (!enabled_) return 0.0;
    
    auto endTime = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    auto it = timingOperations_.find(operationName);
    if (it == timingOperations_.end()) {
        return 0.0; // Operation not found
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - it->second).count() / 1000.0; // Convert to milliseconds
    
    timingOperations_.erase(it);
    updateTimingStats(duration);
    
    return duration;
}

void PerformanceMonitor::recordMetric(const std::string& metricName, double value) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(metricsMutex_);
    metrics_.customMetrics[metricName] = value;
}

void PerformanceMonitor::incrementCounter(const std::string& metricName, uint64_t increment) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(metricsMutex_);
    metrics_.customMetrics[metricName] += increment;
}

void PerformanceMonitor::recordMemoryUsage(size_t bytes) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(metricsMutex_);
    metrics_.memoryUsage = bytes;
    metrics_.peakMemoryUsage = std::max(metrics_.peakMemoryUsage, bytes);
}

void PerformanceMonitor::recordMessage(size_t messageSize, bool sent) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    if (sent) {
        metrics_.messagesSent++;
        metrics_.bytesTransmitted += messageSize;
    } else {
        metrics_.messagesReceived++;
        metrics_.bytesReceived += messageSize;
    }
}

void PerformanceMonitor::recordError(const std::string& errorType) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(metricsMutex_);
    metrics_.errorCount++;
    errorCounters_[errorType]++;
}

PerformanceMetrics PerformanceMonitor::getMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return metrics_;
}

nlohmann::json PerformanceMonitor::getPerformanceSummary() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::seconds>(
        now - metrics_.startTime).count();
    
    nlohmann::json summary = {
        {"device_id", deviceId_},
        {"enabled", enabled_.load()},
        {"total_runtime_seconds", totalTime},
        {"timing", {
            {"average_update_time_ms", metrics_.averageUpdateTime},
            {"max_update_time_ms", metrics_.maxUpdateTime},
            {"min_update_time_ms", metrics_.minUpdateTime},
            {"update_count", metrics_.updateCount}
        }},
        {"memory", {
            {"current_usage_bytes", metrics_.memoryUsage},
            {"peak_usage_bytes", metrics_.peakMemoryUsage}
        }},
        {"communication", {
            {"messages_sent", metrics_.messagesSent},
            {"messages_received", metrics_.messagesReceived},
            {"bytes_transmitted", metrics_.bytesTransmitted},
            {"bytes_received", metrics_.bytesReceived}
        }},
        {"errors", {
            {"total_errors", metrics_.errorCount},
            {"reconnect_count", metrics_.reconnectCount}
        }}
    };
    
    // Add custom metrics
    if (!metrics_.customMetrics.empty()) {
        summary["custom_metrics"] = metrics_.customMetrics;
    }
    
    // Add error breakdown
    if (!errorCounters_.empty()) {
        summary["error_breakdown"] = errorCounters_;
    }
    
    return summary;
}

void PerformanceMonitor::reset() {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    metrics_ = PerformanceMetrics{};
    metrics_.startTime = std::chrono::steady_clock::now();
    metrics_.lastUpdateTime = metrics_.startTime;
    
    timingOperations_.clear();
    errorCounters_.clear();
}

void PerformanceMonitor::setEnabled(bool enabled) {
    enabled_ = enabled;
}

bool PerformanceMonitor::isEnabled() const {
    return enabled_;
}

void PerformanceMonitor::updateTimingStats(double duration) {
    metrics_.updateCount++;
    
    if (metrics_.updateCount == 1) {
        metrics_.averageUpdateTime = duration;
        metrics_.maxUpdateTime = duration;
        metrics_.minUpdateTime = duration;
    } else {
        // Update running average
        metrics_.averageUpdateTime = (metrics_.averageUpdateTime * (metrics_.updateCount - 1) + duration) / metrics_.updateCount;
        metrics_.maxUpdateTime = std::max(metrics_.maxUpdateTime, duration);
        metrics_.minUpdateTime = std::min(metrics_.minUpdateTime, duration);
    }
    
    metrics_.lastUpdateTime = std::chrono::steady_clock::now();
}

// ScopedTimer implementation
ScopedTimer::ScopedTimer(PerformanceMonitor& monitor, const std::string& operationName)
    : monitor_(monitor), operationName_(operationName) {
    monitor_.startTiming(operationName_);
}

ScopedTimer::~ScopedTimer() {
    monitor_.endTiming(operationName_);
}

} // namespace device
} // namespace hydrogen

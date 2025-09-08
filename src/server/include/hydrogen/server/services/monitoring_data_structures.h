#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace hydrogen {
namespace server {
namespace services {

using json = nlohmann::json;

// Time series data point
struct TimeSeriesPoint {
    std::chrono::system_clock::time_point timestamp;
    double value;
    json metadata;
    
    json toJson() const {
        json j;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        j["value"] = value;
        j["metadata"] = metadata;
        return j;
    }
    
    static TimeSeriesPoint fromJson(const json& j) {
        TimeSeriesPoint point;
        if (j.contains("timestamp")) {
            point.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(j["timestamp"]));
        }
        if (j.contains("value")) point.value = j["value"];
        if (j.contains("metadata")) point.metadata = j["metadata"];
        return point;
    }
};

// Response time metrics
struct ResponseTimeMetrics {
    double averageMs = 0.0;
    double medianMs = 0.0;
    double p95Ms = 0.0;
    double p99Ms = 0.0;
    double minMs = 0.0;
    double maxMs = 0.0;
    uint64_t totalRequests = 0;
    std::vector<TimeSeriesPoint> timeSeries;
    
    json toJson() const {
        json j;
        j["averageMs"] = averageMs;
        j["medianMs"] = medianMs;
        j["p95Ms"] = p95Ms;
        j["p99Ms"] = p99Ms;
        j["minMs"] = minMs;
        j["maxMs"] = maxMs;
        j["totalRequests"] = totalRequests;
        
        json timeSeriesJson = json::array();
        for (const auto& point : timeSeries) {
            timeSeriesJson.push_back(point.toJson());
        }
        j["timeSeries"] = timeSeriesJson;
        return j;
    }
    
    static ResponseTimeMetrics fromJson(const json& j) {
        ResponseTimeMetrics metrics;
        if (j.contains("averageMs")) metrics.averageMs = j["averageMs"];
        if (j.contains("medianMs")) metrics.medianMs = j["medianMs"];
        if (j.contains("p95Ms")) metrics.p95Ms = j["p95Ms"];
        if (j.contains("p99Ms")) metrics.p99Ms = j["p99Ms"];
        if (j.contains("minMs")) metrics.minMs = j["minMs"];
        if (j.contains("maxMs")) metrics.maxMs = j["maxMs"];
        if (j.contains("totalRequests")) metrics.totalRequests = j["totalRequests"];
        
        if (j.contains("timeSeries")) {
            for (const auto& pointJson : j["timeSeries"]) {
                metrics.timeSeries.push_back(TimeSeriesPoint::fromJson(pointJson));
            }
        }
        return metrics;
    }
};

// Throughput metrics
struct ThroughputMetrics {
    double requestsPerSecond = 0.0;
    double messagesPerSecond = 0.0;
    double bytesPerSecond = 0.0;
    uint64_t totalRequests = 0;
    uint64_t totalMessages = 0;
    uint64_t totalBytes = 0;
    std::vector<TimeSeriesPoint> timeSeries;
    
    json toJson() const {
        json j;
        j["requestsPerSecond"] = requestsPerSecond;
        j["messagesPerSecond"] = messagesPerSecond;
        j["bytesPerSecond"] = bytesPerSecond;
        j["totalRequests"] = totalRequests;
        j["totalMessages"] = totalMessages;
        j["totalBytes"] = totalBytes;
        
        json timeSeriesJson = json::array();
        for (const auto& point : timeSeries) {
            timeSeriesJson.push_back(point.toJson());
        }
        j["timeSeries"] = timeSeriesJson;
        return j;
    }
    
    static ThroughputMetrics fromJson(const json& j) {
        ThroughputMetrics metrics;
        if (j.contains("requestsPerSecond")) metrics.requestsPerSecond = j["requestsPerSecond"];
        if (j.contains("messagesPerSecond")) metrics.messagesPerSecond = j["messagesPerSecond"];
        if (j.contains("bytesPerSecond")) metrics.bytesPerSecond = j["bytesPerSecond"];
        if (j.contains("totalRequests")) metrics.totalRequests = j["totalRequests"];
        if (j.contains("totalMessages")) metrics.totalMessages = j["totalMessages"];
        if (j.contains("totalBytes")) metrics.totalBytes = j["totalBytes"];
        
        if (j.contains("timeSeries")) {
            for (const auto& pointJson : j["timeSeries"]) {
                metrics.timeSeries.push_back(TimeSeriesPoint::fromJson(pointJson));
            }
        }
        return metrics;
    }
};

// Error metrics
struct ErrorMetrics {
    uint64_t totalErrors = 0;
    double errorRate = 0.0; // Errors per second
    double errorPercentage = 0.0; // Percentage of requests that failed
    std::unordered_map<std::string, uint64_t> errorsByType;
    std::unordered_map<std::string, uint64_t> errorsByCode;
    std::vector<TimeSeriesPoint> timeSeries;
    
    json toJson() const {
        json j;
        j["totalErrors"] = totalErrors;
        j["errorRate"] = errorRate;
        j["errorPercentage"] = errorPercentage;
        j["errorsByType"] = errorsByType;
        j["errorsByCode"] = errorsByCode;
        
        json timeSeriesJson = json::array();
        for (const auto& point : timeSeries) {
            timeSeriesJson.push_back(point.toJson());
        }
        j["timeSeries"] = timeSeriesJson;
        return j;
    }
    
    static ErrorMetrics fromJson(const json& j) {
        ErrorMetrics metrics;
        if (j.contains("totalErrors")) metrics.totalErrors = j["totalErrors"];
        if (j.contains("errorRate")) metrics.errorRate = j["errorRate"];
        if (j.contains("errorPercentage")) metrics.errorPercentage = j["errorPercentage"];
        if (j.contains("errorsByType")) metrics.errorsByType = j["errorsByType"];
        if (j.contains("errorsByCode")) metrics.errorsByCode = j["errorsByCode"];
        
        if (j.contains("timeSeries")) {
            for (const auto& pointJson : j["timeSeries"]) {
                metrics.timeSeries.push_back(TimeSeriesPoint::fromJson(pointJson));
            }
        }
        return metrics;
    }
};

// Resource metrics
struct ResourceMetrics {
    double cpuUsagePercent = 0.0;
    double memoryUsagePercent = 0.0;
    uint64_t memoryUsageBytes = 0;
    double networkBytesPerSecond = 0.0;
    uint64_t openConnections = 0;
    uint64_t threadCount = 0;
    std::unordered_map<std::string, double> customMetrics;
    
    json toJson() const {
        json j;
        j["cpuUsagePercent"] = cpuUsagePercent;
        j["memoryUsagePercent"] = memoryUsagePercent;
        j["memoryUsageBytes"] = memoryUsageBytes;
        j["networkBytesPerSecond"] = networkBytesPerSecond;
        j["openConnections"] = openConnections;
        j["threadCount"] = threadCount;
        j["customMetrics"] = customMetrics;
        return j;
    }
    
    static ResourceMetrics fromJson(const json& j) {
        ResourceMetrics metrics;
        if (j.contains("cpuUsagePercent")) metrics.cpuUsagePercent = j["cpuUsagePercent"];
        if (j.contains("memoryUsagePercent")) metrics.memoryUsagePercent = j["memoryUsagePercent"];
        if (j.contains("memoryUsageBytes")) metrics.memoryUsageBytes = j["memoryUsageBytes"];
        if (j.contains("networkBytesPerSecond")) metrics.networkBytesPerSecond = j["networkBytesPerSecond"];
        if (j.contains("openConnections")) metrics.openConnections = j["openConnections"];
        if (j.contains("threadCount")) metrics.threadCount = j["threadCount"];
        if (j.contains("customMetrics")) metrics.customMetrics = j["customMetrics"];
        return metrics;
    }
};

// Device performance metrics
struct DevicePerformanceMetrics {
    std::string deviceId;
    std::chrono::system_clock::time_point timestamp;
    
    // Core metrics
    ResponseTimeMetrics responseTime;
    ThroughputMetrics throughput;
    ErrorMetrics errors;
    ResourceMetrics resources;
    
    // Protocol-specific metrics
    std::unordered_map<std::string, json> protocolMetrics;
    
    // Custom metrics
    std::unordered_map<std::string, json> customMetrics;
    
    json toJson() const {
        json j;
        j["deviceId"] = deviceId;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        j["responseTime"] = responseTime.toJson();
        j["throughput"] = throughput.toJson();
        j["errors"] = errors.toJson();
        j["resources"] = resources.toJson();
        j["protocolMetrics"] = protocolMetrics;
        j["customMetrics"] = customMetrics;
        return j;
    }
    
    static DevicePerformanceMetrics fromJson(const json& j) {
        DevicePerformanceMetrics metrics;
        if (j.contains("deviceId")) metrics.deviceId = j["deviceId"];
        if (j.contains("timestamp")) {
            metrics.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(j["timestamp"]));
        }
        if (j.contains("responseTime")) metrics.responseTime = ResponseTimeMetrics::fromJson(j["responseTime"]);
        if (j.contains("throughput")) metrics.throughput = ThroughputMetrics::fromJson(j["throughput"]);
        if (j.contains("errors")) metrics.errors = ErrorMetrics::fromJson(j["errors"]);
        if (j.contains("resources")) metrics.resources = ResourceMetrics::fromJson(j["resources"]);
        if (j.contains("protocolMetrics")) metrics.protocolMetrics = j["protocolMetrics"];
        if (j.contains("customMetrics")) metrics.customMetrics = j["customMetrics"];
        return metrics;
    }
};

// System performance metrics
struct SystemPerformanceMetrics {
    std::chrono::system_clock::time_point timestamp;
    
    // Aggregate metrics
    ResponseTimeMetrics aggregateResponseTime;
    ThroughputMetrics aggregateThroughput;
    ErrorMetrics aggregateErrors;
    ResourceMetrics aggregateResources;
    
    // Per-device breakdown
    std::unordered_map<std::string, DevicePerformanceMetrics> deviceMetrics;
    
    // System-wide metrics
    uint64_t totalDevices = 0;
    uint64_t activeDevices = 0;
    uint64_t totalConnections = 0;
    double systemHealthScore = 0.0; // 0.0 to 1.0
    
    json toJson() const {
        json j;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        j["aggregateResponseTime"] = aggregateResponseTime.toJson();
        j["aggregateThroughput"] = aggregateThroughput.toJson();
        j["aggregateErrors"] = aggregateErrors.toJson();
        j["aggregateResources"] = aggregateResources.toJson();
        
        json deviceMetricsJson;
        for (const auto& [deviceId, metrics] : deviceMetrics) {
            deviceMetricsJson[deviceId] = metrics.toJson();
        }
        j["deviceMetrics"] = deviceMetricsJson;
        
        j["totalDevices"] = totalDevices;
        j["activeDevices"] = activeDevices;
        j["totalConnections"] = totalConnections;
        j["systemHealthScore"] = systemHealthScore;
        return j;
    }
    
    static SystemPerformanceMetrics fromJson(const json& j) {
        SystemPerformanceMetrics metrics;
        if (j.contains("timestamp")) {
            metrics.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(j["timestamp"]));
        }
        if (j.contains("aggregateResponseTime")) {
            metrics.aggregateResponseTime = ResponseTimeMetrics::fromJson(j["aggregateResponseTime"]);
        }
        if (j.contains("aggregateThroughput")) {
            metrics.aggregateThroughput = ThroughputMetrics::fromJson(j["aggregateThroughput"]);
        }
        if (j.contains("aggregateErrors")) {
            metrics.aggregateErrors = ErrorMetrics::fromJson(j["aggregateErrors"]);
        }
        if (j.contains("aggregateResources")) {
            metrics.aggregateResources = ResourceMetrics::fromJson(j["aggregateResources"]);
        }
        
        if (j.contains("deviceMetrics")) {
            for (const auto& [deviceId, deviceMetricsJson] : j["deviceMetrics"].items()) {
                metrics.deviceMetrics[deviceId] = DevicePerformanceMetrics::fromJson(deviceMetricsJson);
            }
        }
        
        if (j.contains("totalDevices")) metrics.totalDevices = j["totalDevices"];
        if (j.contains("activeDevices")) metrics.activeDevices = j["activeDevices"];
        if (j.contains("totalConnections")) metrics.totalConnections = j["totalConnections"];
        if (j.contains("systemHealthScore")) metrics.systemHealthScore = j["systemHealthScore"];
        return metrics;
    }
};

// Health status enumeration
enum class HealthStatus {
    HEALTHY,
    WARNING,
    CRITICAL,
    OFFLINE,
    UNKNOWN
};

// Health indicator
struct HealthIndicator {
    std::string name;
    std::string description;
    HealthStatus status;
    double value;
    double threshold;
    std::string unit;
    std::chrono::system_clock::time_point lastCheck;
    
    json toJson() const {
        json j;
        j["name"] = name;
        j["description"] = description;
        j["status"] = static_cast<int>(status);
        j["value"] = value;
        j["threshold"] = threshold;
        j["unit"] = unit;
        j["lastCheck"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            lastCheck.time_since_epoch()).count();
        return j;
    }
    
    static HealthIndicator fromJson(const json& j) {
        HealthIndicator indicator;
        if (j.contains("name")) indicator.name = j["name"];
        if (j.contains("description")) indicator.description = j["description"];
        if (j.contains("status")) indicator.status = static_cast<HealthStatus>(j["status"]);
        if (j.contains("value")) indicator.value = j["value"];
        if (j.contains("threshold")) indicator.threshold = j["threshold"];
        if (j.contains("unit")) indicator.unit = j["unit"];
        if (j.contains("lastCheck")) {
            indicator.lastCheck = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(j["lastCheck"]));
        }
        return indicator;
    }
};

// Health report structures
struct DeviceHealthReport {
    std::string deviceId;
    std::chrono::system_clock::time_point timestamp;
    HealthStatus overallHealth;
    std::vector<HealthIndicator> indicators;
    json metadata;

    json toJson() const {
        json j;
        j["deviceId"] = deviceId;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        j["overallHealth"] = static_cast<int>(overallHealth);

        json indicatorsJson = json::array();
        for (const auto& indicator : indicators) {
            indicatorsJson.push_back(indicator.toJson());
        }
        j["indicators"] = indicatorsJson;
        j["metadata"] = metadata;
        return j;
    }

    static DeviceHealthReport fromJson(const json& j) {
        DeviceHealthReport report;
        if (j.contains("deviceId")) report.deviceId = j["deviceId"];
        if (j.contains("timestamp")) {
            report.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(j["timestamp"]));
        }
        if (j.contains("overallHealth")) report.overallHealth = static_cast<HealthStatus>(j["overallHealth"]);
        if (j.contains("indicators")) {
            for (const auto& indicatorJson : j["indicators"]) {
                report.indicators.push_back(HealthIndicator::fromJson(indicatorJson));
            }
        }
        if (j.contains("metadata")) report.metadata = j["metadata"];
        return report;
    }
};

struct SystemHealthReport {
    std::chrono::system_clock::time_point timestamp;
    HealthStatus overallHealth;
    std::vector<HealthIndicator> indicators;
    std::unordered_map<std::string, DeviceHealthReport> deviceReports;
    json metadata;

    json toJson() const {
        json j;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        j["overallHealth"] = static_cast<int>(overallHealth);

        json indicatorsJson = json::array();
        for (const auto& indicator : indicators) {
            indicatorsJson.push_back(indicator.toJson());
        }
        j["indicators"] = indicatorsJson;

        json deviceReportsJson;
        for (const auto& [deviceId, report] : deviceReports) {
            deviceReportsJson[deviceId] = report.toJson();
        }
        j["deviceReports"] = deviceReportsJson;
        j["metadata"] = metadata;
        return j;
    }

    static SystemHealthReport fromJson(const json& j) {
        SystemHealthReport report;
        if (j.contains("timestamp")) {
            report.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(j["timestamp"]));
        }
        if (j.contains("overallHealth")) report.overallHealth = static_cast<HealthStatus>(j["overallHealth"]);
        if (j.contains("indicators")) {
            for (const auto& indicatorJson : j["indicators"]) {
                report.indicators.push_back(HealthIndicator::fromJson(indicatorJson));
            }
        }
        if (j.contains("deviceReports")) {
            for (const auto& [deviceId, reportJson] : j["deviceReports"].items()) {
                report.deviceReports[deviceId] = DeviceHealthReport::fromJson(reportJson);
            }
        }
        if (j.contains("metadata")) report.metadata = j["metadata"];
        return report;
    }
};

// Performance alert structure
struct PerformanceAlert {
    std::string alertId;
    std::string deviceId;
    std::string alertType;
    std::string severity;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    bool isActive = true;
    json metadata;

    json toJson() const {
        json j;
        j["alertId"] = alertId;
        j["deviceId"] = deviceId;
        j["alertType"] = alertType;
        j["severity"] = severity;
        j["message"] = message;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        j["isActive"] = isActive;
        j["metadata"] = metadata;
        return j;
    }

    static PerformanceAlert fromJson(const json& j) {
        PerformanceAlert alert;
        if (j.contains("alertId")) alert.alertId = j["alertId"];
        if (j.contains("deviceId")) alert.deviceId = j["deviceId"];
        if (j.contains("alertType")) alert.alertType = j["alertType"];
        if (j.contains("severity")) alert.severity = j["severity"];
        if (j.contains("message")) alert.message = j["message"];
        if (j.contains("timestamp")) {
            alert.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(j["timestamp"]));
        }
        if (j.contains("isActive")) alert.isActive = j["isActive"];
        if (j.contains("metadata")) alert.metadata = j["metadata"];
        return alert;
    }
};

// Diagnostics result structure
struct DiagnosticsResult {
    std::string diagnosticsId;
    std::string deviceId;
    std::string status;
    std::vector<std::string> testResults;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    json metadata;

    json toJson() const {
        json j;
        j["diagnosticsId"] = diagnosticsId;
        j["deviceId"] = deviceId;
        j["status"] = status;
        j["testResults"] = testResults;
        j["startTime"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            startTime.time_since_epoch()).count();
        j["endTime"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime.time_since_epoch()).count();
        j["metadata"] = metadata;
        return j;
    }

    static DiagnosticsResult fromJson(const json& j) {
        DiagnosticsResult result;
        if (j.contains("diagnosticsId")) result.diagnosticsId = j["diagnosticsId"];
        if (j.contains("deviceId")) result.deviceId = j["deviceId"];
        if (j.contains("status")) result.status = j["status"];
        if (j.contains("testResults")) result.testResults = j["testResults"];
        if (j.contains("startTime")) {
            result.startTime = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(j["startTime"]));
        }
        if (j.contains("endTime")) {
            result.endTime = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(j["endTime"]));
        }
        if (j.contains("metadata")) result.metadata = j["metadata"];
        return result;
    }
};

} // namespace services
} // namespace server
} // namespace hydrogen

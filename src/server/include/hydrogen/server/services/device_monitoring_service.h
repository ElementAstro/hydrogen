#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "monitoring_data_structures.h"

namespace hydrogen {
namespace server {
namespace services {

using json = nlohmann::json;

// Callback types
using MetricsCallback = std::function<void(const json& metrics)>;
using SystemMetricsCallback = std::function<void(const json& systemMetrics)>;

// Time range structure
struct TimeRange {
    std::chrono::system_clock::time_point start;
    std::chrono::system_clock::time_point end;
    
    TimeRange() = default;
    TimeRange(std::chrono::system_clock::time_point s, std::chrono::system_clock::time_point e)
        : start(s), end(e) {}
    
    // Helper constructors
    static TimeRange lastHour() {
        auto now = std::chrono::system_clock::now();
        return TimeRange(now - std::chrono::hours(1), now);
    }
    
    static TimeRange lastDay() {
        auto now = std::chrono::system_clock::now();
        return TimeRange(now - std::chrono::hours(24), now);
    }
    
    static TimeRange lastWeek() {
        auto now = std::chrono::system_clock::now();
        return TimeRange(now - std::chrono::hours(24 * 7), now);
    }
};

// Metrics subscription configuration
struct MetricsSubscription {
    std::vector<std::string> metrics;
    std::chrono::milliseconds interval{1000};
    bool includeTimestamp = true;
    bool includeMetadata = false;
    json filters;
    
    json toJson() const {
        json j;
        j["metrics"] = metrics;
        j["interval"] = interval.count();
        j["includeTimestamp"] = includeTimestamp;
        j["includeMetadata"] = includeMetadata;
        j["filters"] = filters;
        return j;
    }
    
    static MetricsSubscription fromJson(const json& j) {
        MetricsSubscription sub;
        if (j.contains("metrics")) sub.metrics = j["metrics"];
        if (j.contains("interval")) sub.interval = std::chrono::milliseconds(j["interval"]);
        if (j.contains("includeTimestamp")) sub.includeTimestamp = j["includeTimestamp"];
        if (j.contains("includeMetadata")) sub.includeMetadata = j["includeMetadata"];
        if (j.contains("filters")) sub.filters = j["filters"];
        return sub;
    }
};

struct SystemMetricsSubscription {
    std::vector<std::string> metricTypes;
    std::chrono::milliseconds interval{5000};
    bool includeDeviceBreakdown = true;
    bool includeProtocolBreakdown = true;
    
    json toJson() const {
        json j;
        j["metricTypes"] = metricTypes;
        j["interval"] = interval.count();
        j["includeDeviceBreakdown"] = includeDeviceBreakdown;
        j["includeProtocolBreakdown"] = includeProtocolBreakdown;
        return j;
    }
    
    static SystemMetricsSubscription fromJson(const json& j) {
        SystemMetricsSubscription sub;
        if (j.contains("metricTypes")) sub.metricTypes = j["metricTypes"];
        if (j.contains("interval")) sub.interval = std::chrono::milliseconds(j["interval"]);
        if (j.contains("includeDeviceBreakdown")) sub.includeDeviceBreakdown = j["includeDeviceBreakdown"];
        if (j.contains("includeProtocolBreakdown")) sub.includeProtocolBreakdown = j["includeProtocolBreakdown"];
        return sub;
    }
};

// Metrics aggregation options
struct MetricsAggregation {
    enum class Type {
        NONE,
        AVERAGE,
        SUM,
        MIN,
        MAX,
        COUNT,
        PERCENTILE
    };
    
    Type type = Type::NONE;
    std::chrono::milliseconds window{60000}; // 1 minute default
    double percentile = 95.0; // For percentile aggregation
    
    json toJson() const {
        json j;
        j["type"] = static_cast<int>(type);
        j["window"] = window.count();
        j["percentile"] = percentile;
        return j;
    }
    
    static MetricsAggregation fromJson(const json& j) {
        MetricsAggregation agg;
        if (j.contains("type")) agg.type = static_cast<Type>(j["type"]);
        if (j.contains("window")) agg.window = std::chrono::milliseconds(j["window"]);
        if (j.contains("percentile")) agg.percentile = j["percentile"];
        return agg;
    }
};

// Alert filter
struct AlertFilter {
    std::vector<std::string> deviceIds;
    std::vector<std::string> severities;
    std::vector<std::string> types;
    TimeRange timeRange;
    bool activeOnly = true;
    
    json toJson() const {
        json j;
        j["deviceIds"] = deviceIds;
        j["severities"] = severities;
        j["types"] = types;
        j["activeOnly"] = activeOnly;
        return j;
    }
    
    static AlertFilter fromJson(const json& j) {
        AlertFilter filter;
        if (j.contains("deviceIds")) filter.deviceIds = j["deviceIds"];
        if (j.contains("severities")) filter.severities = j["severities"];
        if (j.contains("types")) filter.types = j["types"];
        if (j.contains("activeOnly")) filter.activeOnly = j["activeOnly"];
        return filter;
    }
};

// Health report options
struct HealthReportOptions {
    bool includeHistory = false;
    bool includeRecommendations = true;
    bool includeDiagnostics = false;
    std::chrono::milliseconds maxAge{3600000}; // 1 hour
    
    json toJson() const {
        json j;
        j["includeHistory"] = includeHistory;
        j["includeRecommendations"] = includeRecommendations;
        j["includeDiagnostics"] = includeDiagnostics;
        j["maxAge"] = maxAge.count();
        return j;
    }
    
    static HealthReportOptions fromJson(const json& j) {
        HealthReportOptions options;
        if (j.contains("includeHistory")) options.includeHistory = j["includeHistory"];
        if (j.contains("includeRecommendations")) options.includeRecommendations = j["includeRecommendations"];
        if (j.contains("includeDiagnostics")) options.includeDiagnostics = j["includeDiagnostics"];
        if (j.contains("maxAge")) options.maxAge = std::chrono::milliseconds(j["maxAge"]);
        return options;
    }
};

// Diagnostics options
struct DiagnosticsOptions {
    std::vector<std::string> testTypes;
    bool includePerformanceTests = true;
    bool includeConnectivityTests = true;
    bool includeResourceTests = false;
    std::chrono::milliseconds timeout{30000};
    
    json toJson() const {
        json j;
        j["testTypes"] = testTypes;
        j["includePerformanceTests"] = includePerformanceTests;
        j["includeConnectivityTests"] = includeConnectivityTests;
        j["includeResourceTests"] = includeResourceTests;
        j["timeout"] = timeout.count();
        return j;
    }
    
    static DiagnosticsOptions fromJson(const json& j) {
        DiagnosticsOptions options;
        if (j.contains("testTypes")) options.testTypes = j["testTypes"];
        if (j.contains("includePerformanceTests")) options.includePerformanceTests = j["includePerformanceTests"];
        if (j.contains("includeConnectivityTests")) options.includeConnectivityTests = j["includeConnectivityTests"];
        if (j.contains("includeResourceTests")) options.includeResourceTests = j["includeResourceTests"];
        if (j.contains("timeout")) options.timeout = std::chrono::milliseconds(j["timeout"]);
        return options;
    }
};

/**
 * @brief Real-time device monitoring service interface
 * 
 * This service provides comprehensive real-time monitoring capabilities for devices
 * including metrics streaming, health monitoring, performance analysis, and alerting.
 */
class IDeviceMonitoringService {
public:
    virtual ~IDeviceMonitoringService() = default;

    // Real-time metrics streaming
    virtual std::string subscribeToDeviceMetrics(
        const std::string& deviceId,
        const MetricsSubscription& subscription,
        MetricsCallback callback
    ) = 0;
    
    virtual std::string subscribeToSystemMetrics(
        const SystemMetricsSubscription& subscription,
        SystemMetricsCallback callback
    ) = 0;
    
    virtual bool unsubscribeFromMetrics(const std::string& subscriptionId) = 0;

    // Performance monitoring
    virtual DevicePerformanceMetrics getDevicePerformanceMetrics(
        const std::string& deviceId,
        const TimeRange& timeRange,
        const MetricsAggregation& aggregation = MetricsAggregation{}
    ) const = 0;
    
    virtual SystemPerformanceMetrics getSystemPerformanceMetrics(
        const TimeRange& timeRange,
        const MetricsAggregation& aggregation = MetricsAggregation{}
    ) const = 0;
    
    virtual std::vector<PerformanceAlert> getPerformanceAlerts(
        const std::string& deviceId = "",
        const AlertFilter& filter = AlertFilter{}
    ) const = 0;

    // Health monitoring and diagnostics
    virtual DeviceHealthReport getDeviceHealthReport(
        const std::string& deviceId,
        const HealthReportOptions& options = HealthReportOptions{}
    ) const = 0;
    
    virtual SystemHealthReport getSystemHealthReport(
        const HealthReportOptions& options = HealthReportOptions{}
    ) const = 0;
    
    virtual std::string runDeviceDiagnostics(
        const std::string& deviceId,
        const DiagnosticsOptions& options = DiagnosticsOptions{}
    ) = 0;
    
    virtual DiagnosticsResult getDiagnosticsResult(const std::string& diagnosticsId) const = 0;

    // Service lifecycle
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool isRunning() const = 0;
    
    // Service configuration
    virtual void setConfiguration(const json& config) = 0;
    virtual json getConfiguration() const = 0;
};

} // namespace services
} // namespace server
} // namespace hydrogen

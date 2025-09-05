#include "hydrogen/core/performance_metrics.h"
#include "hydrogen/core/utils.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>

namespace hydrogen {
namespace core {

// PerformanceMeasurement implementation
json PerformanceMeasurement::toJson() const {
  return json{{"type", metricTypeToString(type)},
              {"name", name},
              {"value", value},
              {"unit", unit},
              {"timestamp", getIsoTimestamp()},
              {"metadata", metadata}};
}

PerformanceMeasurement PerformanceMeasurement::fromJson(const json &j) {
  PerformanceMeasurement measurement;
  measurement.type = stringToMetricType(j.value("type", "CUSTOM"));
  measurement.name = j.value("name", "");
  measurement.value = j.value("value", 0.0);
  measurement.unit = j.value("unit", "");
  measurement.metadata = j.value("metadata", json::object());

  if (j.contains("timestamp")) {
    measurement.timestamp = string_utils::parseIsoTimestamp(j["timestamp"]);
  }

  return measurement;
}

// PerformanceStatistics implementation
json PerformanceStatistics::toJson() const {
  return json{{"type", metricTypeToString(type)},
              {"name", name},
              {"min", min},
              {"max", max},
              {"average", average},
              {"median", median},
              {"standardDeviation", standardDeviation},
              {"sampleCount", sampleCount},
              {"firstSample", getIsoTimestamp()},
              {"lastSample", getIsoTimestamp()}};
}

PerformanceStatistics PerformanceStatistics::fromJson(const json &j) {
  PerformanceStatistics stats;
  stats.type = stringToMetricType(j.value("type", "CUSTOM"));
  stats.name = j.value("name", "");
  stats.min = j.value("min", 0.0);
  stats.max = j.value("max", 0.0);
  stats.average = j.value("average", 0.0);
  stats.median = j.value("median", 0.0);
  stats.standardDeviation = j.value("standardDeviation", 0.0);
  stats.sampleCount = j.value("sampleCount", 0ULL);

  if (j.contains("firstSample")) {
    stats.firstSample = string_utils::parseIsoTimestamp(j["firstSample"]);
  }
  if (j.contains("lastSample")) {
    stats.lastSample = string_utils::parseIsoTimestamp(j["lastSample"]);
  }

  return stats;
}

// PerformanceAlert implementation
json PerformanceAlert::toJson() const {
  return json{{"name", name},
              {"metricType", metricTypeToString(metricType)},
              {"condition", condition},
              {"threshold", threshold},
              {"duration", duration.count()},
              {"enabled", enabled}};
}

PerformanceAlert PerformanceAlert::fromJson(const json &j) {
  PerformanceAlert alert;
  alert.name = j.value("name", "");
  alert.metricType = stringToMetricType(j.value("metricType", "CUSTOM"));
  alert.condition = j.value("condition", "greater_than");
  alert.threshold = j.value("threshold", 0.0);
  alert.duration = std::chrono::milliseconds(j.value("duration", 0));
  alert.enabled = j.value("enabled", true);

  return alert;
}

// PerformanceTrend implementation
json PerformanceTrend::toJson() const {
  return json{{"deviceId", deviceId},
              {"metricType", metricTypeToString(metricType)},
              {"trendDirection", trendDirection},
              {"trendSlope", trendSlope},
              {"confidence", confidence},
              {"analysisTime", getIsoTimestamp()}};
}

PerformanceTrend PerformanceTrend::fromJson(const json &j) {
  PerformanceTrend trend;
  trend.deviceId = j.value("deviceId", "");
  trend.metricType = stringToMetricType(j.value("metricType", "CUSTOM"));
  trend.trendDirection = j.value("trendDirection", "stable");
  trend.trendSlope = j.value("trendSlope", 0.0);
  trend.confidence = j.value("confidence", 0.0);

  if (j.contains("analysisTime")) {
    trend.analysisTime = string_utils::parseIsoTimestamp(j["analysisTime"]);
  }

  return trend;
}

// PerformanceMetricsCollector implementation
PerformanceMetricsCollector::PerformanceMetricsCollector() = default;

PerformanceMetricsCollector::~PerformanceMetricsCollector() { stop(); }

void PerformanceMetricsCollector::recordMeasurement(
    const std::string &deviceId, const PerformanceMeasurement &measurement) {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  std::string metricKey = getMetricKey(measurement.type, measurement.name);
  auto &metricData = deviceMetrics_[deviceId][metricKey];

  metricData.measurements.push_back(measurement);
  trimMeasurements(metricData.measurements);

  if (realTimeStats_) {
    calculateStatistics(deviceId, metricKey);
  }
}

void PerformanceMetricsCollector::recordMetric(const std::string &deviceId,
                                               MetricType metricType,
                                               const std::string &name,
                                               double value,
                                               const std::string &unit) {
  PerformanceMeasurement measurement;
  measurement.type = metricType;
  measurement.name = name;
  measurement.value = value;
  measurement.unit = unit;
  measurement.timestamp = std::chrono::system_clock::now();

  recordMeasurement(deviceId, measurement);
}

PerformanceStatistics
PerformanceMetricsCollector::getStatistics(const std::string &deviceId,
                                           MetricType metricType,
                                           const std::string &name) const {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  std::string metricKey = getMetricKey(metricType, name);

  auto deviceIt = deviceMetrics_.find(deviceId);
  if (deviceIt == deviceMetrics_.end()) {
    return PerformanceStatistics{};
  }

  auto metricIt = deviceIt->second.find(metricKey);
  if (metricIt == deviceIt->second.end()) {
    return PerformanceStatistics{};
  }

  return metricIt->second.statistics;
}

std::unordered_map<std::string, PerformanceStatistics>
PerformanceMetricsCollector::getAllStatistics(
    const std::string &deviceId) const {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  std::unordered_map<std::string, PerformanceStatistics> allStats;

  auto deviceIt = deviceMetrics_.find(deviceId);
  if (deviceIt != deviceMetrics_.end()) {
    for (const auto &[metricKey, metricData] : deviceIt->second) {
      allStats[metricKey] = metricData.statistics;
    }
  }

  return allStats;
}

PerformanceTrend
PerformanceMetricsCollector::analyzeTrend(const std::string &deviceId,
                                          MetricType metricType,
                                          std::chrono::hours timeWindow) const {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  PerformanceTrend trend;
  trend.deviceId = deviceId;
  trend.metricType = metricType;
  trend.analysisTime = std::chrono::system_clock::now();

  // Find measurements within time window
  auto cutoffTime = trend.analysisTime - timeWindow;
  std::vector<double> values;
  std::vector<double> timePoints;

  auto deviceIt = deviceMetrics_.find(deviceId);
  if (deviceIt != deviceMetrics_.end()) {
    for (const auto &[metricKey, metricData] : deviceIt->second) {
      for (const auto &measurement : metricData.measurements) {
        if (measurement.type == metricType &&
            measurement.timestamp >= cutoffTime) {
          values.push_back(measurement.value);
          auto duration = measurement.timestamp.time_since_epoch();
          timePoints.push_back(std::chrono::duration<double>(duration).count());
        }
      }
    }
  }

  if (values.size() < 2) {
    trend.trendDirection = "stable";
    trend.trendSlope = 0.0;
    trend.confidence = 0.0;
    return trend;
  }

  // Simple linear regression for trend analysis
  double n = values.size();
  double sumX = std::accumulate(timePoints.begin(), timePoints.end(), 0.0);
  double sumY = std::accumulate(values.begin(), values.end(), 0.0);
  double sumXY = 0.0;
  double sumXX = 0.0;

  for (size_t i = 0; i < values.size(); ++i) {
    sumXY += timePoints[i] * values[i];
    sumXX += timePoints[i] * timePoints[i];
  }

  trend.trendSlope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);

  // Determine trend direction
  if (std::abs(trend.trendSlope) < 0.001) {
    trend.trendDirection = "stable";
  } else if (trend.trendSlope > 0) {
    trend.trendDirection = "improving";
  } else {
    trend.trendDirection = "degrading";
  }

  // Calculate confidence (simplified R-squared)
  double meanY = sumY / n;
  double ssRes = 0.0;
  double ssTot = 0.0;

  for (size_t i = 0; i < values.size(); ++i) {
    double predicted = trend.trendSlope * timePoints[i];
    ssRes += std::pow(values[i] - predicted, 2);
    ssTot += std::pow(values[i] - meanY, 2);
  }

  trend.confidence = ssTot > 0 ? 1.0 - (ssRes / ssTot) : 0.0;
  trend.confidence = std::max(0.0, std::min(1.0, trend.confidence));

  return trend;
}

void PerformanceMetricsCollector::registerAlert(const std::string &deviceId,
                                                const PerformanceAlert &alert) {
  std::lock_guard<std::mutex> lock(alertsMutex_);
  deviceAlerts_[deviceId].push_back(alert);
}

void PerformanceMetricsCollector::removeAlert(const std::string &deviceId,
                                              const std::string &alertName) {
  std::lock_guard<std::mutex> lock(alertsMutex_);

  auto deviceIt = deviceAlerts_.find(deviceId);
  if (deviceIt != deviceAlerts_.end()) {
    auto &alerts = deviceIt->second;
    alerts.erase(std::remove_if(alerts.begin(), alerts.end(),
                                [&alertName](const PerformanceAlert &alert) {
                                  return alert.name == alertName;
                                }),
                 alerts.end());
  }
}

void PerformanceMetricsCollector::setAlertCallback(
    std::function<void(const std::string &, const PerformanceAlert &, double)>
        callback) {
  alertCallback_ = callback;
}

void PerformanceMetricsCollector::start() {
  if (running_)
    return;

  running_ = true;
  startAggregationThread();
  startAlertThread();
}

void PerformanceMetricsCollector::stop() {
  if (!running_)
    return;

  running_ = false;
  stopAggregationThread();
  stopAlertThread();
}

void PerformanceMetricsCollector::clearMetrics(const std::string &deviceId) {
  std::lock_guard<std::mutex> lock(metricsMutex_);
  deviceMetrics_.erase(deviceId);
}

PerformanceMetricsCollector &PerformanceMetricsCollector::getInstance() {
  static PerformanceMetricsCollector instance;
  return instance;
}

void PerformanceMetricsCollector::setMaxMeasurements(size_t maxMeasurements) {
  maxMeasurements_ = maxMeasurements;
}

void PerformanceMetricsCollector::setAggregationInterval(
    std::chrono::seconds interval) {
  aggregationInterval_ = interval;
}

void PerformanceMetricsCollector::setRealTimeStats(bool enabled) {
  realTimeStats_ = enabled;
}

// Helper function implementations
std::string metricTypeToString(MetricType type) {
  switch (type) {
  case MetricType::RESPONSE_TIME:
    return "RESPONSE_TIME";
  case MetricType::THROUGHPUT:
    return "THROUGHPUT";
  case MetricType::ERROR_RATE:
    return "ERROR_RATE";
  case MetricType::CONNECTION_TIME:
    return "CONNECTION_TIME";
  case MetricType::MEMORY_USAGE:
    return "MEMORY_USAGE";
  case MetricType::CPU_USAGE:
    return "CPU_USAGE";
  case MetricType::NETWORK_LATENCY:
    return "NETWORK_LATENCY";
  case MetricType::QUEUE_DEPTH:
    return "QUEUE_DEPTH";
  case MetricType::CUSTOM:
    return "CUSTOM";
  default:
    return "UNKNOWN";
  }
}

MetricType stringToMetricType(const std::string &type) {
  if (type == "RESPONSE_TIME")
    return MetricType::RESPONSE_TIME;
  if (type == "THROUGHPUT")
    return MetricType::THROUGHPUT;
  if (type == "ERROR_RATE")
    return MetricType::ERROR_RATE;
  if (type == "CONNECTION_TIME")
    return MetricType::CONNECTION_TIME;
  if (type == "MEMORY_USAGE")
    return MetricType::MEMORY_USAGE;
  if (type == "CPU_USAGE")
    return MetricType::CPU_USAGE;
  if (type == "NETWORK_LATENCY")
    return MetricType::NETWORK_LATENCY;
  if (type == "QUEUE_DEPTH")
    return MetricType::QUEUE_DEPTH;
  return MetricType::CUSTOM;
}

void PerformanceMetricsCollector::calculateStatistics(
    const std::string &deviceId, const std::string &metricKey) {
  auto &metricData = deviceMetrics_[deviceId][metricKey];
  const auto &measurements = metricData.measurements;

  if (measurements.empty()) {
    return;
  }

  auto &stats = metricData.statistics;
  stats.type = measurements[0].type;
  stats.name = measurements[0].name;
  stats.sampleCount = measurements.size();
  stats.firstSample = measurements.front().timestamp;
  stats.lastSample = measurements.back().timestamp;

  // Extract values for calculation
  std::vector<double> values;
  values.reserve(measurements.size());
  for (const auto &measurement : measurements) {
    values.push_back(measurement.value);
  }

  // Calculate basic statistics
  stats.min = *std::min_element(values.begin(), values.end());
  stats.max = *std::max_element(values.begin(), values.end());
  stats.average =
      std::accumulate(values.begin(), values.end(), 0.0) / values.size();

  // Calculate median
  stats.median = calculateMedian(values);

  // Calculate standard deviation
  stats.standardDeviation = calculateStandardDeviation(values, stats.average);

  metricData.lastAggregation = std::chrono::system_clock::now();
}

void PerformanceMetricsCollector::checkAlerts(const std::string &deviceId) {
  std::lock_guard<std::mutex> alertLock(alertsMutex_);

  auto alertIt = deviceAlerts_.find(deviceId);
  if (alertIt == deviceAlerts_.end()) {
    return;
  }

  std::lock_guard<std::mutex> metricsLock(metricsMutex_);
  auto metricsIt = deviceMetrics_.find(deviceId);
  if (metricsIt == deviceMetrics_.end()) {
    return;
  }

  for (const auto &alert : alertIt->second) {
    if (!alert.enabled)
      continue;

    std::string metricKey = getMetricKey(alert.metricType, alert.name);
    auto metricIt = metricsIt->second.find(metricKey);
    if (metricIt == metricsIt->second.end()) {
      continue;
    }

    const auto &stats = metricIt->second.statistics;
    double currentValue = stats.average; // Use average for alert checking

    bool alertTriggered = false;
    if (alert.condition == "greater_than" && currentValue > alert.threshold) {
      alertTriggered = true;
    } else if (alert.condition == "less_than" &&
               currentValue < alert.threshold) {
      alertTriggered = true;
    } else if (alert.condition == "equals" &&
               std::abs(currentValue - alert.threshold) < 0.001) {
      alertTriggered = true;
    } else if (alert.condition == "not_equals" &&
               std::abs(currentValue - alert.threshold) >= 0.001) {
      alertTriggered = true;
    }

    if (alertTriggered && alertCallback_) {
      alertCallback_(deviceId, alert, currentValue);
    }
  }
}

void PerformanceMetricsCollector::startAggregationThread() {
  aggregationThread_ = std::thread(
      &PerformanceMetricsCollector::aggregationThreadFunction, this);
}

void PerformanceMetricsCollector::stopAggregationThread() {
  if (aggregationThread_.joinable()) {
    aggregationThread_.join();
  }
}

void PerformanceMetricsCollector::aggregationThreadFunction() {
  while (running_) {
    if (!realTimeStats_) {
      std::lock_guard<std::mutex> lock(metricsMutex_);
      for (auto &[deviceId, deviceMetrics] : deviceMetrics_) {
        for (auto &[metricKey, metricData] : deviceMetrics) {
          calculateStatistics(deviceId, metricKey);
        }
      }
    }

    std::this_thread::sleep_for(aggregationInterval_);
  }
}

void PerformanceMetricsCollector::startAlertThread() {
  alertThread_ =
      std::thread(&PerformanceMetricsCollector::alertThreadFunction, this);
}

void PerformanceMetricsCollector::stopAlertThread() {
  if (alertThread_.joinable()) {
    alertThread_.join();
  }
}

void PerformanceMetricsCollector::alertThreadFunction() {
  while (running_) {
    std::vector<std::string> deviceIds;

    {
      std::lock_guard<std::mutex> lock(metricsMutex_);
      for (const auto &[deviceId, metrics] : deviceMetrics_) {
        deviceIds.push_back(deviceId);
      }
    }

    for (const auto &deviceId : deviceIds) {
      checkAlerts(deviceId);
    }

    std::this_thread::sleep_for(
        std::chrono::seconds(10)); // Check alerts every 10 seconds
  }
}

std::string
PerformanceMetricsCollector::getMetricKey(MetricType type,
                                          const std::string &name) const {
  return metricTypeToString(type) + "::" + name;
}

void PerformanceMetricsCollector::trimMeasurements(
    std::vector<PerformanceMeasurement> &measurements) {
  if (measurements.size() > maxMeasurements_) {
    size_t toRemove = measurements.size() - maxMeasurements_;
    measurements.erase(measurements.begin(), measurements.begin() + toRemove);
  }
}

double
PerformanceMetricsCollector::calculateMedian(std::vector<double> values) const {
  if (values.empty())
    return 0.0;

  std::sort(values.begin(), values.end());
  size_t n = values.size();

  if (n % 2 == 0) {
    return (values[n / 2 - 1] + values[n / 2]) / 2.0;
  } else {
    return values[n / 2];
  }
}

double PerformanceMetricsCollector::calculateStandardDeviation(
    const std::vector<double> &values, double mean) const {
  if (values.size() <= 1)
    return 0.0;

  double variance = 0.0;
  for (double value : values) {
    variance += std::pow(value - mean, 2);
  }
  variance /= (values.size() - 1);

  return std::sqrt(variance);
}

bool PerformanceMetricsCollector::exportMetrics(
    const std::string &filename, const std::string &format) const {
  try {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    if (format == "json") {
      json exportData = json::object();

      for (const auto &[deviceId, deviceMetrics] : deviceMetrics_) {
        json deviceData = json::object();

        for (const auto &[metricKey, metricData] : deviceMetrics) {
          json metricJson = json::object();
          metricJson["statistics"] = metricData.statistics.toJson();

          json measurementsArray = json::array();
          for (const auto &measurement : metricData.measurements) {
            measurementsArray.push_back(measurement.toJson());
          }
          metricJson["measurements"] = measurementsArray;

          deviceData[metricKey] = metricJson;
        }

        exportData[deviceId] = deviceData;
      }

      std::ofstream file(filename);
      file << exportData.dump(2);
      return true;
    }

    return false; // Unsupported format
  } catch (const std::exception &) {
    return false;
  }
}

} // namespace core
} // namespace hydrogen

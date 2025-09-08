#include "hydrogen/core/device/device_health.h"
#include "hydrogen/core/infrastructure/utils.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

namespace hydrogen {
namespace core {

// DeviceMetrics implementation
json DeviceMetrics::toJson() const {
  return json{{"averageResponseTime", averageResponseTime},
              {"minResponseTime", minResponseTime},
              {"maxResponseTime", maxResponseTime},
              {"totalCommands", totalCommands},
              {"successfulCommands", successfulCommands},
              {"failedCommands", failedCommands},
              {"connectionAttempts", connectionAttempts},
              {"successfulConnections", successfulConnections},
              {"connectionFailures", connectionFailures},
              {"averageConnectionTime", averageConnectionTime},
              {"totalErrors", totalErrors},
              {"criticalErrors", criticalErrors},
              {"recoverableErrors", recoverableErrors},
              {"memoryUsageMB", memoryUsageMB},
              {"cpuUsagePercent", cpuUsagePercent},
              {"lastUpdated", getIsoTimestamp()},
              {"firstSeen", getIsoTimestamp()}};
}

DeviceMetrics DeviceMetrics::fromJson(const json &j) {
  DeviceMetrics metrics;
  metrics.averageResponseTime = j.value("averageResponseTime", 0.0);
  metrics.minResponseTime = j.value("minResponseTime", 0.0);
  metrics.maxResponseTime = j.value("maxResponseTime", 0.0);
  metrics.totalCommands = j.value("totalCommands", 0ULL);
  metrics.successfulCommands = j.value("successfulCommands", 0ULL);
  metrics.failedCommands = j.value("failedCommands", 0ULL);
  metrics.connectionAttempts = j.value("connectionAttempts", 0U);
  metrics.successfulConnections = j.value("successfulConnections", 0U);
  metrics.connectionFailures = j.value("connectionFailures", 0U);
  metrics.averageConnectionTime = j.value("averageConnectionTime", 0.0);
  metrics.totalErrors = j.value("totalErrors", 0U);
  metrics.criticalErrors = j.value("criticalErrors", 0U);
  metrics.recoverableErrors = j.value("recoverableErrors", 0U);
  metrics.memoryUsageMB = j.value("memoryUsageMB", 0.0);
  metrics.cpuUsagePercent = j.value("cpuUsagePercent", 0.0);

  if (j.contains("lastUpdated")) {
    metrics.lastUpdated = string_utils::parseIsoTimestamp(j["lastUpdated"]);
  }
  if (j.contains("firstSeen")) {
    metrics.firstSeen = string_utils::parseIsoTimestamp(j["firstSeen"]);
  }

  return metrics;
}

// DeviceHealthReport implementation
json DeviceHealthReport::toJson() const {
  json issuesArray = json::array();
  for (const auto &issue : issues) {
    issuesArray.push_back(issue);
  }

  json recommendationsArray = json::array();
  for (const auto &rec : recommendations) {
    recommendationsArray.push_back(rec);
  }

  return json{{"deviceId", deviceId},
              {"status", healthStatusToString(status)},
              {"healthScore", healthScore},
              {"metrics", metrics.toJson()},
              {"issues", issuesArray},
              {"recommendations", recommendationsArray},
              {"timestamp", getIsoTimestamp()}};
}

DeviceHealthReport DeviceHealthReport::fromJson(const json &j) {
  DeviceHealthReport report;
  report.deviceId = j.value("deviceId", "");
  report.status = stringToHealthStatus(j.value("status", "UNKNOWN"));
  report.healthScore = j.value("healthScore", 0.0);

  if (j.contains("metrics")) {
    report.metrics = DeviceMetrics::fromJson(j["metrics"]);
  }

  if (j.contains("issues") && j["issues"].is_array()) {
    for (const auto &issue : j["issues"]) {
      report.issues.push_back(issue.get<std::string>());
    }
  }

  if (j.contains("recommendations") && j["recommendations"].is_array()) {
    for (const auto &rec : j["recommendations"]) {
      report.recommendations.push_back(rec.get<std::string>());
    }
  }

  if (j.contains("timestamp")) {
    report.timestamp = string_utils::parseIsoTimestamp(j["timestamp"]);
  }

  return report;
}

// DeviceHealthMonitor implementation
DeviceHealthMonitor::DeviceHealthMonitor() { initializeDefaultThresholds(); }

DeviceHealthMonitor::~DeviceHealthMonitor() { stopCleanupThread(); }

void DeviceHealthMonitor::recordCommand(const std::string &deviceId,
                                        const std::string &command,
                                        double responseTimeMs, bool success) {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  auto &metrics = deviceMetrics_[deviceId];
  auto now = std::chrono::system_clock::now();

  // Initialize first seen if this is the first record
  if (metrics.totalCommands == 0) {
    metrics.firstSeen = now;
  }

  // Update command metrics
  metrics.totalCommands++;
  if (success) {
    metrics.successfulCommands++;
  } else {
    metrics.failedCommands++;
  }

  // Update response time metrics
  if (metrics.totalCommands == 1) {
    metrics.averageResponseTime = responseTimeMs;
    metrics.minResponseTime = responseTimeMs;
    metrics.maxResponseTime = responseTimeMs;
  } else {
    // Calculate running average
    metrics.averageResponseTime =
        (metrics.averageResponseTime * (metrics.totalCommands - 1) +
         responseTimeMs) /
        metrics.totalCommands;
    metrics.minResponseTime = std::min(metrics.minResponseTime, responseTimeMs);
    metrics.maxResponseTime = std::max(metrics.maxResponseTime, responseTimeMs);
  }

  metrics.lastUpdated = now;

  // Check for health status change
  auto oldStatus = scoreToStatus(calculateHealthScore(metrics));
  auto newScore = calculateHealthScore(metrics);
  auto newStatus = scoreToStatus(newScore);

  if (oldStatus != newStatus && healthChangeCallback_) {
    healthChangeCallback_(deviceId, oldStatus, newStatus);
  }
}

void DeviceHealthMonitor::recordConnection(const std::string &deviceId,
                                           bool success,
                                           double connectionTimeMs) {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  auto &metrics = deviceMetrics_[deviceId];
  auto now = std::chrono::system_clock::now();

  metrics.connectionAttempts++;
  if (success) {
    metrics.successfulConnections++;

    // Update average connection time
    if (metrics.successfulConnections == 1) {
      metrics.averageConnectionTime = connectionTimeMs;
    } else {
      metrics.averageConnectionTime =
          (metrics.averageConnectionTime * (metrics.successfulConnections - 1) +
           connectionTimeMs) /
          metrics.successfulConnections;
    }
  } else {
    metrics.connectionFailures++;
  }

  metrics.lastUpdated = now;
}

void DeviceHealthMonitor::recordError(const std::string &deviceId,
                                      const std::string &errorCode,
                                      const std::string &severity) {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  auto &metrics = deviceMetrics_[deviceId];
  auto now = std::chrono::system_clock::now();

  metrics.totalErrors++;
  if (severity == "critical") {
    metrics.criticalErrors++;
  } else if (severity == "recoverable") {
    metrics.recoverableErrors++;
  }

  metrics.lastUpdated = now;
}

void DeviceHealthMonitor::updateResourceUsage(const std::string &deviceId,
                                              double memoryUsageMB,
                                              double cpuUsagePercent) {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  auto &metrics = deviceMetrics_[deviceId];
  metrics.memoryUsageMB = memoryUsageMB;
  metrics.cpuUsagePercent = cpuUsagePercent;
  metrics.lastUpdated = std::chrono::system_clock::now();
}

DeviceHealthStatus
DeviceHealthMonitor::getHealthStatus(const std::string &deviceId) const {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  auto it = deviceMetrics_.find(deviceId);
  if (it == deviceMetrics_.end()) {
    return DeviceHealthStatus::UNKNOWN;
  }

  double score = calculateHealthScore(it->second);
  return scoreToStatus(score);
}

DeviceHealthReport
DeviceHealthMonitor::getHealthReport(const std::string &deviceId) const {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  DeviceHealthReport report;
  report.deviceId = deviceId;
  report.timestamp = std::chrono::system_clock::now();

  auto it = deviceMetrics_.find(deviceId);
  if (it == deviceMetrics_.end()) {
    report.status = DeviceHealthStatus::UNKNOWN;
    report.healthScore = 0.0;
    return report;
  }

  report.metrics = it->second;
  report.healthScore = calculateHealthScore(it->second);
  report.status = scoreToStatus(report.healthScore);
  report.issues = generateHealthIssues(it->second);
  report.recommendations = generateRecommendations(it->second);

  return report;
}

std::vector<DeviceHealthReport>
DeviceHealthMonitor::getAllHealthReports() const {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  std::vector<DeviceHealthReport> reports;
  for (const auto &[deviceId, metrics] : deviceMetrics_) {
    DeviceHealthReport report;
    report.deviceId = deviceId;
    report.metrics = metrics;
    report.healthScore = calculateHealthScore(metrics);
    report.status = scoreToStatus(report.healthScore);
    report.issues = generateHealthIssues(metrics);
    report.recommendations = generateRecommendations(metrics);
    report.timestamp = std::chrono::system_clock::now();

    reports.push_back(report);
  }

  return reports;
}

void DeviceHealthMonitor::setHealthThreshold(DeviceHealthStatus status,
                                             double threshold) {
  std::lock_guard<std::mutex> lock(metricsMutex_);
  healthThresholds_[status] = threshold;
}

void DeviceHealthMonitor::setHealthChangeCallback(
    std::function<void(const std::string &, DeviceHealthStatus,
                       DeviceHealthStatus)>
        callback) {
  healthChangeCallback_ = callback;
}

void DeviceHealthMonitor::startMonitoring(const std::string &deviceId) {
  std::lock_guard<std::mutex> lock(metricsMutex_);
  monitoredDevices_[deviceId] = true;

  // Initialize metrics if not exists
  if (deviceMetrics_.find(deviceId) == deviceMetrics_.end()) {
    deviceMetrics_[deviceId] = DeviceMetrics{};
    deviceMetrics_[deviceId].firstSeen = std::chrono::system_clock::now();
  }
}

void DeviceHealthMonitor::stopMonitoring(const std::string &deviceId) {
  std::lock_guard<std::mutex> lock(metricsMutex_);
  monitoredDevices_[deviceId] = false;
}

void DeviceHealthMonitor::clearMetrics(const std::string &deviceId) {
  std::lock_guard<std::mutex> lock(metricsMutex_);
  deviceMetrics_.erase(deviceId);
}

DeviceHealthMonitor &DeviceHealthMonitor::getInstance() {
  static DeviceHealthMonitor instance;
  return instance;
}

void DeviceHealthMonitor::setAutoCleanup(bool enabled, int maxAgeHours) {
  autoCleanupEnabled_ = enabled;
  maxMetricsAgeHours_ = maxAgeHours;

  if (enabled && !cleanupRunning_) {
    startCleanupThread();
  } else if (!enabled && cleanupRunning_) {
    stopCleanupThread();
  }
}

// Helper function implementations
std::string healthStatusToString(DeviceHealthStatus status) {
  switch (status) {
  case DeviceHealthStatus::EXCELLENT:
    return "EXCELLENT";
  case DeviceHealthStatus::GOOD:
    return "GOOD";
  case DeviceHealthStatus::FAIR:
    return "FAIR";
  case DeviceHealthStatus::POOR:
    return "POOR";
  case DeviceHealthStatus::CRITICAL:
    return "CRITICAL";
  case DeviceHealthStatus::UNKNOWN:
    return "UNKNOWN";
  default:
    return "UNKNOWN";
  }
}

DeviceHealthStatus stringToHealthStatus(const std::string &status) {
  if (status == "EXCELLENT")
    return DeviceHealthStatus::EXCELLENT;
  if (status == "GOOD")
    return DeviceHealthStatus::GOOD;
  if (status == "FAIR")
    return DeviceHealthStatus::FAIR;
  if (status == "POOR")
    return DeviceHealthStatus::POOR;
  if (status == "CRITICAL")
    return DeviceHealthStatus::CRITICAL;
  return DeviceHealthStatus::UNKNOWN;
}

double
DeviceHealthMonitor::calculateHealthScore(const DeviceMetrics &metrics) const {
  if (metrics.totalCommands == 0) {
    return 0.0; // No data available
  }

  double score = 100.0;

  // Success rate component (40% weight)
  double successRate = metrics.getSuccessRate();
  score *= (0.4 * successRate + 0.6);

  // Connection reliability component (25% weight)
  double connectionRate = metrics.getConnectionSuccessRate();
  if (metrics.connectionAttempts > 0) {
    score *= (0.25 * connectionRate + 0.75);
  }

  // Response time component (20% weight)
  if (metrics.averageResponseTime > 0) {
    double responseTimePenalty = std::min(metrics.averageResponseTime / 1000.0,
                                          1.0); // Normalize to 1 second
    score *= (1.0 - 0.2 * responseTimePenalty);
  }

  // Error rate component (15% weight)
  if (metrics.totalCommands > 0) {
    double errorRate = (double)metrics.totalErrors / metrics.totalCommands;
    score *= (1.0 - 0.15 * errorRate);
  }

  return std::max(0.0, std::min(100.0, score));
}

DeviceHealthStatus DeviceHealthMonitor::scoreToStatus(double score) const {
  if (score >= healthThresholds_.at(DeviceHealthStatus::EXCELLENT))
    return DeviceHealthStatus::EXCELLENT;
  if (score >= healthThresholds_.at(DeviceHealthStatus::GOOD))
    return DeviceHealthStatus::GOOD;
  if (score >= healthThresholds_.at(DeviceHealthStatus::FAIR))
    return DeviceHealthStatus::FAIR;
  if (score >= healthThresholds_.at(DeviceHealthStatus::POOR))
    return DeviceHealthStatus::POOR;
  return DeviceHealthStatus::CRITICAL;
}

void DeviceHealthMonitor::initializeDefaultThresholds() {
  healthThresholds_[DeviceHealthStatus::EXCELLENT] = 90.0;
  healthThresholds_[DeviceHealthStatus::GOOD] = 70.0;
  healthThresholds_[DeviceHealthStatus::FAIR] = 50.0;
  healthThresholds_[DeviceHealthStatus::POOR] = 30.0;
  healthThresholds_[DeviceHealthStatus::CRITICAL] = 0.0;
}

void DeviceHealthMonitor::startCleanupThread() {
  if (cleanupRunning_)
    return;

  cleanupRunning_ = true;
  cleanupThread_ = std::thread([this]() {
    while (cleanupRunning_) {
      cleanupOldMetrics();
      std::this_thread::sleep_for(std::chrono::hours(1));
    }
  });
}

void DeviceHealthMonitor::stopCleanupThread() {
  cleanupRunning_ = false;
  if (cleanupThread_.joinable()) {
    cleanupThread_.join();
  }
}

void DeviceHealthMonitor::cleanupOldMetrics() {
  std::lock_guard<std::mutex> lock(metricsMutex_);

  auto now = std::chrono::system_clock::now();
  auto maxAge = std::chrono::hours(maxMetricsAgeHours_);

  auto it = deviceMetrics_.begin();
  while (it != deviceMetrics_.end()) {
    if (now - it->second.lastUpdated > maxAge) {
      it = deviceMetrics_.erase(it);
    } else {
      ++it;
    }
  }
}

std::vector<std::string>
DeviceHealthMonitor::generateHealthIssues(const DeviceMetrics &metrics) const {
  std::vector<std::string> issues;

  if (metrics.getSuccessRate() < 0.8) {
    issues.push_back("Low command success rate: " +
                     std::to_string(metrics.getSuccessRate() * 100) + "%");
  }

  if (metrics.averageResponseTime > 1000.0) {
    issues.push_back("High average response time: " +
                     std::to_string(metrics.averageResponseTime) + "ms");
  }

  if (metrics.getConnectionSuccessRate() < 0.9) {
    issues.push_back("Connection reliability issues: " +
                     std::to_string(metrics.getConnectionSuccessRate() * 100) +
                     "%");
  }

  if (metrics.criticalErrors > 0) {
    issues.push_back("Critical errors detected: " +
                     std::to_string(metrics.criticalErrors));
  }

  if (metrics.memoryUsageMB > 100.0) {
    issues.push_back(
        "High memory usage: " + std::to_string(metrics.memoryUsageMB) + "MB");
  }

  if (metrics.cpuUsagePercent > 80.0) {
    issues.push_back(
        "High CPU usage: " + std::to_string(metrics.cpuUsagePercent) + "%");
  }

  return issues;
}

std::vector<std::string> DeviceHealthMonitor::generateRecommendations(
    const DeviceMetrics &metrics) const {
  std::vector<std::string> recommendations;

  if (metrics.getSuccessRate() < 0.8) {
    recommendations.push_back("Check device connection and configuration");
    recommendations.push_back(
        "Review command parameters and device compatibility");
  }

  if (metrics.averageResponseTime > 1000.0) {
    recommendations.push_back(
        "Consider optimizing device communication protocol");
    recommendations.push_back("Check network latency and bandwidth");
  }

  if (metrics.getConnectionSuccessRate() < 0.9) {
    recommendations.push_back(
        "Verify network stability and device availability");
    recommendations.push_back(
        "Consider implementing connection retry strategies");
  }

  if (metrics.criticalErrors > 0) {
    recommendations.push_back("Review error logs for critical issues");
    recommendations.push_back("Consider device firmware updates");
  }

  if (metrics.memoryUsageMB > 100.0) {
    recommendations.push_back("Monitor for memory leaks");
    recommendations.push_back(
        "Consider implementing memory optimization strategies");
  }

  return recommendations;
}

bool DeviceHealthMonitor::saveMetrics(const std::string &filename) const {
  try {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    json data = json::object();
    for (const auto &[deviceId, metrics] : deviceMetrics_) {
      data[deviceId] = metrics.toJson();
    }

    std::ofstream file(filename);
    file << data.dump(2);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

bool DeviceHealthMonitor::loadMetrics(const std::string &filename) {
  try {
    std::ifstream file(filename);
    json data;
    file >> data;

    std::lock_guard<std::mutex> lock(metricsMutex_);
    deviceMetrics_.clear();

    for (const auto &[deviceId, metricsJson] : data.items()) {
      deviceMetrics_[deviceId] = DeviceMetrics::fromJson(metricsJson);
    }

    return true;
  } catch (const std::exception &) {
    return false;
  }
}

} // namespace core
} // namespace hydrogen

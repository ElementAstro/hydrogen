#include "hydrogen/core/logging/fifo_logger.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

// Undefine Windows macros that conflict with our enum values
#ifdef ERROR
#undef ERROR
#endif

namespace hydrogen {
namespace core {

// FifoLogEntry implementation
json FifoLogEntry::toJson() const {
  json j;

  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()) %
            1000;

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
  oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';

  j["timestamp"] = oss.str();
  j["level"] = static_cast<int>(level);
  j["category"] = category;
  j["message"] = message;
  j["clientId"] = clientId;
  j["pipePath"] = pipePath;
  j["metadata"] = metadata;

  return j;
}

std::string FifoLogEntry::toString() const {
  std::ostringstream oss;

  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");

  std::string levelStr;
  switch (level) {
  case FifoLogLevel::TRACE:
    levelStr = "TRACE";
    break;
  case FifoLogLevel::DEBUG:
    levelStr = "DEBUG";
    break;
  case FifoLogLevel::INFO:
    levelStr = "INFO";
    break;
  case FifoLogLevel::WARN:
    levelStr = "WARN";
    break;
  case FifoLogLevel::ERROR:
    levelStr = "ERROR";
    break;
  case FifoLogLevel::CRITICAL:
    levelStr = "CRITICAL";
    break;
  default:
    levelStr = "UNKNOWN";
    break;
  }

  oss << " [" << levelStr << "] ";
  if (!category.empty()) {
    oss << "[" << category << "] ";
  }
  if (!clientId.empty()) {
    oss << "[" << clientId << "] ";
  }
  oss << message;

  return oss.str();
}

std::string FifoLogEntry::toFormattedString() const {
  std::ostringstream oss;

  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()) %
            1000;

  oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

  std::string levelStr;
  switch (level) {
  case FifoLogLevel::TRACE:
    levelStr = "TRACE";
    break;
  case FifoLogLevel::DEBUG:
    levelStr = "DEBUG";
    break;
  case FifoLogLevel::INFO:
    levelStr = "INFO ";
    break;
  case FifoLogLevel::WARN:
    levelStr = "WARN ";
    break;
  case FifoLogLevel::ERROR:
    levelStr = "ERROR";
    break;
  case FifoLogLevel::CRITICAL:
    levelStr = "CRIT ";
    break;
  default:
    levelStr = "UNK  ";
    break;
  }

  oss << " [" << levelStr << "]";

  if (!category.empty()) {
    oss << " [" << std::setw(12) << std::left << category.substr(0, 12) << "]";
  }

  if (!clientId.empty()) {
    oss << " [" << std::setw(10) << std::left << clientId.substr(0, 10) << "]";
  }

  oss << " " << message;

  if (!pipePath.empty()) {
    oss << " (pipe: " << pipePath << ")";
  }

  return oss.str();
}

// FifoMetrics implementation
double FifoMetrics::getMessagesPerSecond() const {
  auto now = std::chrono::system_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
  if (duration.count() == 0)
    return 0.0;
  return static_cast<double>(totalMessages.load()) / duration.count();
}

double FifoMetrics::getBytesPerSecond() const {
  auto now = std::chrono::system_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
  if (duration.count() == 0)
    return 0.0;
  return static_cast<double>(totalBytes.load()) / duration.count();
}

double FifoMetrics::getAverageLatencyMicros() const {
  uint64_t messages = totalMessages.load();
  if (messages == 0)
    return 0.0;
  return static_cast<double>(totalLatencyMicros.load()) / messages;
}

std::chrono::milliseconds FifoMetrics::getUptime() const {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
}

void FifoMetrics::reset() {
  totalMessages.store(0);
  totalBytes.store(0);
  totalErrors.store(0);
  totalConnections.store(0);
  totalDisconnections.store(0);
  totalReconnections.store(0);
  totalLatencyMicros.store(0);
  maxLatencyMicros.store(0);
  minLatencyMicros.store(UINT64_MAX);
  lastResetTime = std::chrono::system_clock::now();
}

json FifoMetrics::toJson() const {
  json j;
  j["totalMessages"] = totalMessages.load();
  j["totalBytes"] = totalBytes.load();
  j["totalErrors"] = totalErrors.load();
  j["totalConnections"] = totalConnections.load();
  j["totalDisconnections"] = totalDisconnections.load();
  j["totalReconnections"] = totalReconnections.load();
  j["messagesPerSecond"] = getMessagesPerSecond();
  j["bytesPerSecond"] = getBytesPerSecond();
  j["averageLatencyMicros"] = getAverageLatencyMicros();
  j["maxLatencyMicros"] = maxLatencyMicros.load();
  j["minLatencyMicros"] =
      minLatencyMicros.load() == UINT64_MAX ? 0 : minLatencyMicros.load();
  j["uptimeMs"] = getUptime().count();

  auto now = std::chrono::system_clock::now();
  auto timeSinceReset = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - lastResetTime);
  j["timeSinceResetMs"] = timeSinceReset.count();

  return j;
}

// FifoMessageTrace implementation
json FifoMessageTrace::toJson() const {
  json j;

  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()) %
            1000;

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
  oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';

  j["timestamp"] = oss.str();
  j["messageId"] = messageId;
  j["clientId"] = clientId;
  j["pipePath"] = pipePath;
  j["direction"] = direction;
  j["messageSize"] = messageSize;
  j["messageType"] = messageType;
  j["content"] = content;
  j["processingTimeMicros"] = processingTime.count();

  return j;
}

// FifoLoggerConfig implementation
json FifoLoggerConfig::toJson() const {
  json j;
  j["logLevel"] = static_cast<int>(logLevel);
  j["enableConsoleLogging"] = enableConsoleLogging;
  j["enableFileLogging"] = enableFileLogging;
  j["enableAsyncLogging"] = enableAsyncLogging;
  j["enableJsonLogging"] = enableJsonLogging;
  j["logFilePath"] = logFilePath;
  j["jsonLogFilePath"] = jsonLogFilePath;
  j["maxLogFileSize"] = maxLogFileSize;
  j["maxLogFiles"] = maxLogFiles;
  j["enableLogRotation"] = enableLogRotation;
  j["enableMessageTracing"] = enableMessageTracing;
  j["enableMessageContent"] = enableMessageContent;
  j["maxTraceEntries"] = maxTraceEntries;
  j["traceRetentionTime"] = traceRetentionTime.count();
  j["enablePerformanceMetrics"] = enablePerformanceMetrics;
  j["metricsUpdateInterval"] = metricsUpdateInterval.count();
  j["enableLatencyTracking"] = enableLatencyTracking;
  j["enableThroughputTracking"] = enableThroughputTracking;
  j["enableDebugMode"] = enableDebugMode;
  j["enableVerboseLogging"] = enableVerboseLogging;
  j["enableStackTraces"] = enableStackTraces;
  j["debugCategories"] = debugCategories;
  j["excludeCategories"] = excludeCategories;
  j["includeOnlyCategories"] = includeOnlyCategories;
  j["minLogLevel"] = static_cast<int>(minLogLevel);
  j["asyncQueueSize"] = asyncQueueSize;
  j["asyncFlushInterval"] = asyncFlushInterval.count();
  j["asyncWorkerThreads"] = asyncWorkerThreads;
  return j;
}

void FifoLoggerConfig::fromJson(const json &j) {
  if (j.contains("logLevel"))
    logLevel = static_cast<FifoLogLevel>(j["logLevel"]);
  if (j.contains("enableConsoleLogging"))
    enableConsoleLogging = j["enableConsoleLogging"];
  if (j.contains("enableFileLogging"))
    enableFileLogging = j["enableFileLogging"];
  if (j.contains("enableAsyncLogging"))
    enableAsyncLogging = j["enableAsyncLogging"];
  if (j.contains("enableJsonLogging"))
    enableJsonLogging = j["enableJsonLogging"];
  if (j.contains("logFilePath"))
    logFilePath = j["logFilePath"];
  if (j.contains("jsonLogFilePath"))
    jsonLogFilePath = j["jsonLogFilePath"];
  if (j.contains("maxLogFileSize"))
    maxLogFileSize = j["maxLogFileSize"];
  if (j.contains("maxLogFiles"))
    maxLogFiles = j["maxLogFiles"];
  if (j.contains("enableLogRotation"))
    enableLogRotation = j["enableLogRotation"];
  if (j.contains("enableMessageTracing"))
    enableMessageTracing = j["enableMessageTracing"];
  if (j.contains("enableMessageContent"))
    enableMessageContent = j["enableMessageContent"];
  if (j.contains("maxTraceEntries"))
    maxTraceEntries = j["maxTraceEntries"];
  if (j.contains("traceRetentionTime"))
    traceRetentionTime = std::chrono::milliseconds(j["traceRetentionTime"]);
  if (j.contains("enablePerformanceMetrics"))
    enablePerformanceMetrics = j["enablePerformanceMetrics"];
  if (j.contains("metricsUpdateInterval"))
    metricsUpdateInterval =
        std::chrono::milliseconds(j["metricsUpdateInterval"]);
  if (j.contains("enableLatencyTracking"))
    enableLatencyTracking = j["enableLatencyTracking"];
  if (j.contains("enableThroughputTracking"))
    enableThroughputTracking = j["enableThroughputTracking"];
  if (j.contains("enableDebugMode"))
    enableDebugMode = j["enableDebugMode"];
  if (j.contains("enableVerboseLogging"))
    enableVerboseLogging = j["enableVerboseLogging"];
  if (j.contains("enableStackTraces"))
    enableStackTraces = j["enableStackTraces"];
  if (j.contains("debugCategories"))
    debugCategories = j["debugCategories"];
  if (j.contains("excludeCategories"))
    excludeCategories = j["excludeCategories"];
  if (j.contains("includeOnlyCategories"))
    includeOnlyCategories = j["includeOnlyCategories"];
  if (j.contains("minLogLevel"))
    minLogLevel = static_cast<FifoLogLevel>(j["minLogLevel"]);
  if (j.contains("asyncQueueSize"))
    asyncQueueSize = j["asyncQueueSize"];
  if (j.contains("asyncFlushInterval"))
    asyncFlushInterval = std::chrono::milliseconds(j["asyncFlushInterval"]);
  if (j.contains("asyncWorkerThreads"))
    asyncWorkerThreads = j["asyncWorkerThreads"];
}

bool FifoLoggerConfig::validate() const {
  if (maxLogFileSize == 0)
    return false;
  if (maxLogFiles <= 0)
    return false;
  if (maxTraceEntries == 0)
    return false;
  if (asyncQueueSize == 0)
    return false;
  if (asyncWorkerThreads <= 0)
    return false;
  return true;
}

// FifoLogger implementation
FifoLogger::FifoLogger(const FifoLoggerConfig &config) : config_(config) {
  if (!config_.validate()) {
    throw std::invalid_argument("Invalid FIFO logger configuration");
  }

  initializeLogFiles();

  if (config_.enableAsyncLogging) {
    startAsyncLogging();
  }
}

FifoLogger::~FifoLogger() {
  stopAsyncLogging();
  closeLogFiles();
}

void FifoLogger::updateConfig(const FifoLoggerConfig &config) {
  std::lock_guard<std::mutex> lock(configMutex_);

  if (!config.validate()) {
    throw std::invalid_argument("Invalid FIFO logger configuration");
  }

  bool restartAsync = (config_.enableAsyncLogging != config.enableAsyncLogging);

  config_ = config;

  if (restartAsync) {
    if (config_.enableAsyncLogging) {
      startAsyncLogging();
    } else {
      stopAsyncLogging();
    }
  }

  // Reinitialize log files if paths changed
  initializeLogFiles();
}

void FifoLogger::log(FifoLogLevel level, const std::string &category,
                     const std::string &message, const std::string &clientId,
                     const std::string &pipePath) {
  if (!shouldLogLevel(level) || !shouldLogCategory(category)) {
    return;
  }

  FifoLogEntry entry;
  entry.level = level;
  entry.category = category;
  entry.message = message;
  entry.clientId = clientId;
  entry.pipePath = pipePath;

  if (config_.enableAsyncLogging && asyncRunning_.load()) {
    std::lock_guard<std::mutex> lock(asyncMutex_);
    if (asyncQueue_.size() < config_.asyncQueueSize) {
      asyncQueue_.push(entry);
      asyncCondition_.notify_one();
    }
  } else {
    writeLogEntry(entry);
  }
}

void FifoLogger::trace(const std::string &category, const std::string &message,
                       const std::string &clientId,
                       const std::string &pipePath) {
  log(FifoLogLevel::TRACE, category, message, clientId, pipePath);
}

void FifoLogger::debug(const std::string &category, const std::string &message,
                       const std::string &clientId,
                       const std::string &pipePath) {
  log(FifoLogLevel::DEBUG, category, message, clientId, pipePath);
}

void FifoLogger::info(const std::string &category, const std::string &message,
                      const std::string &clientId,
                      const std::string &pipePath) {
  log(FifoLogLevel::INFO, category, message, clientId, pipePath);
}

void FifoLogger::warn(const std::string &category, const std::string &message,
                      const std::string &clientId,
                      const std::string &pipePath) {
  log(FifoLogLevel::WARN, category, message, clientId, pipePath);
}

void FifoLogger::error(const std::string &category, const std::string &message,
                       const std::string &clientId,
                       const std::string &pipePath) {
  log(FifoLogLevel::ERROR, category, message, clientId, pipePath);
}

void FifoLogger::critical(const std::string &category,
                          const std::string &message,
                          const std::string &clientId,
                          const std::string &pipePath) {
  log(FifoLogLevel::CRITICAL, category, message, clientId, pipePath);
}

void FifoLogger::traceMessage(const std::string &messageId,
                              const std::string &clientId,
                              const std::string &pipePath,
                              const std::string &direction, size_t messageSize,
                              const std::string &messageType,
                              const std::string &content,
                              std::chrono::microseconds processingTime) {
  if (!config_.enableMessageTracing) {
    return;
  }

  FifoMessageTrace trace;
  trace.messageId = messageId;
  trace.clientId = clientId;
  trace.pipePath = pipePath;
  trace.timestamp = std::chrono::system_clock::now();
  trace.direction = direction;
  trace.messageSize = messageSize;
  trace.messageType = messageType;
  trace.content = config_.enableMessageContent ? content : "";
  trace.processingTime = processingTime;

  std::lock_guard<std::mutex> lock(tracesMutex_);

  // Remove old traces if we exceed the limit
  while (messageTraces_.size() >= config_.maxTraceEntries) {
    messageTraces_.pop();
  }

  messageTraces_.push(trace);

  // Clean up old traces periodically
  cleanupOldTraces();
}

std::vector<FifoMessageTrace>
FifoLogger::getMessageTraces(const std::string &clientId,
                             std::chrono::milliseconds maxAge) const {
  std::lock_guard<std::mutex> lock(tracesMutex_);

  std::vector<FifoMessageTrace> result;
  std::queue<FifoMessageTrace> tempQueue = messageTraces_;

  auto now = std::chrono::system_clock::now();

  while (!tempQueue.empty()) {
    const auto &trace = tempQueue.front();
    tempQueue.pop();

    // Filter by client ID if specified
    if (!clientId.empty() && trace.clientId != clientId) {
      continue;
    }

    // Filter by age if specified
    if (maxAge.count() > 0) {
      auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - trace.timestamp);
      if (age > maxAge) {
        continue;
      }
    }

    result.push_back(trace);
  }

  return result;
}

void FifoLogger::clearMessageTraces() {
  std::lock_guard<std::mutex> lock(tracesMutex_);
  while (!messageTraces_.empty()) {
    messageTraces_.pop();
  }
}

void FifoLogger::recordMessage(size_t bytes,
                               std::chrono::microseconds latency) {
  if (!config_.enablePerformanceMetrics) {
    return;
  }

  std::lock_guard<std::mutex> lock(metricsMutex_);

  metrics_.totalMessages.fetch_add(1);
  metrics_.totalBytes.fetch_add(bytes);

  if (config_.enableLatencyTracking && latency.count() > 0) {
    metrics_.totalLatencyMicros.fetch_add(latency.count());

    uint64_t currentMax = metrics_.maxLatencyMicros.load();
    while (latency.count() > currentMax &&
           !metrics_.maxLatencyMicros.compare_exchange_weak(currentMax,
                                                            latency.count())) {
      // Keep trying until we successfully update or find a larger value
    }

    uint64_t currentMin = metrics_.minLatencyMicros.load();
    while (latency.count() < currentMin &&
           !metrics_.minLatencyMicros.compare_exchange_weak(currentMin,
                                                            latency.count())) {
      // Keep trying until we successfully update or find a smaller value
    }
  }
}

void FifoLogger::recordConnection() {
  if (config_.enablePerformanceMetrics) {
    metrics_.totalConnections.fetch_add(1);
  }
}

void FifoLogger::recordDisconnection() {
  if (config_.enablePerformanceMetrics) {
    metrics_.totalDisconnections.fetch_add(1);
  }
}

void FifoLogger::recordReconnection() {
  if (config_.enablePerformanceMetrics) {
    metrics_.totalReconnections.fetch_add(1);
  }
}

void FifoLogger::recordError() {
  if (config_.enablePerformanceMetrics) {
    metrics_.totalErrors.fetch_add(1);
  }
}

FifoMetrics FifoLogger::getMetrics() const {
  std::lock_guard<std::mutex> lock(metricsMutex_);
  return metrics_;  // Now works with our custom copy constructor
}

void FifoLogger::resetMetrics() {
  std::lock_guard<std::mutex> lock(metricsMutex_);
  metrics_.reset();
}

void FifoLogger::flush() {
  if (config_.enableAsyncLogging && asyncRunning_.load()) {
    // Process any remaining async queue items
    processAsyncQueue();
  }

  std::lock_guard<std::mutex> lock(fileMutex_);
  if (logFile_ && logFile_->is_open()) {
    logFile_->flush();
  }
  if (jsonLogFile_ && jsonLogFile_->is_open()) {
    jsonLogFile_->flush();
  }
}

void FifoLogger::rotateLogs() {
  if (!config_.enableLogRotation) {
    return;
  }

  std::lock_guard<std::mutex> lock(fileMutex_);

  if (config_.enableFileLogging &&
      getFileSize(config_.logFilePath) > config_.maxLogFileSize) {
    rotateLogFile(config_.logFilePath);
  }

  if (config_.enableJsonLogging &&
      getFileSize(config_.jsonLogFilePath) > config_.maxLogFileSize) {
    rotateLogFile(config_.jsonLogFilePath);
  }
}

std::vector<FifoLogEntry> FifoLogger::getRecentLogs(size_t count) const {
  std::lock_guard<std::mutex> lock(logMutex_);

  std::vector<FifoLogEntry> result;
  std::queue<FifoLogEntry> tempQueue = logEntries_;

  // Get the last 'count' entries
  size_t skip = tempQueue.size() > count ? tempQueue.size() - count : 0;

  for (size_t i = 0; i < skip; ++i) {
    tempQueue.pop();
  }

  while (!tempQueue.empty()) {
    result.push_back(tempQueue.front());
    tempQueue.pop();
  }

  return result;
}

void FifoLogger::clearLogs() {
  std::lock_guard<std::mutex> lock(logMutex_);
  while (!logEntries_.empty()) {
    logEntries_.pop();
  }
}

bool FifoLogger::isLevelEnabled(FifoLogLevel level) const {
  return level >= config_.logLevel && level >= config_.minLogLevel;
}

std::string FifoLogger::formatLogLevel(FifoLogLevel level) const {
  switch (level) {
  case FifoLogLevel::TRACE:
    return "TRACE";
  case FifoLogLevel::DEBUG:
    return "DEBUG";
  case FifoLogLevel::INFO:
    return "INFO";
  case FifoLogLevel::WARN:
    return "WARN";
  case FifoLogLevel::ERROR:
    return "ERROR";
  case FifoLogLevel::CRITICAL:
    return "CRITICAL";
  case FifoLogLevel::OFF:
    return "OFF";
  default:
    return "UNKNOWN";
  }
}

std::string FifoLogger::getCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::ostringstream oss;
  oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

  return oss.str();
}

void FifoLogger::enableCategory(const std::string &category) {
  std::lock_guard<std::mutex> lock(categoryMutex_);
  categoryEnabled_[category] = true;
}

void FifoLogger::disableCategory(const std::string &category) {
  std::lock_guard<std::mutex> lock(categoryMutex_);
  categoryEnabled_[category] = false;
}

void FifoLogger::setLogLevelForCategory(const std::string &category,
                                        FifoLogLevel level) {
  std::lock_guard<std::mutex> lock(categoryMutex_);
  categoryLogLevels_[category] = level;
}

void FifoLogger::startAsyncLogging() {
  if (asyncRunning_.load()) {
    return;
  }

  asyncRunning_.store(true);
  asyncThread_ =
      std::make_unique<std::thread>(&FifoLogger::asyncLoggingThread, this);
}

void FifoLogger::stopAsyncLogging() {
  if (!asyncRunning_.load()) {
    return;
  }

  asyncRunning_.store(false);
  asyncCondition_.notify_all();

  if (asyncThread_ && asyncThread_->joinable()) {
    asyncThread_->join();
    asyncThread_.reset();
  }

  // Process any remaining items in the queue
  processAsyncQueue();
}

bool FifoLogger::isAsyncLoggingActive() const { return asyncRunning_.load(); }

// Private methods implementation
void FifoLogger::writeLogEntry(const FifoLogEntry &entry) {
  // Store in memory for recent logs
  {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (logEntries_.size() >= 1000) { // Keep last 1000 entries
      logEntries_.pop();
    }
    logEntries_.push(entry);
  }

  if (config_.enableConsoleLogging) {
    writeToConsole(entry);
  }

  if (config_.enableFileLogging) {
    writeToFile(entry);
  }

  if (config_.enableJsonLogging) {
    writeToJsonFile(entry);
  }
}

void FifoLogger::writeToConsole(const FifoLogEntry &entry) {
  std::string output = entry.toFormattedString();

  if (entry.level >= FifoLogLevel::ERROR) {
    std::cerr << output << std::endl;
  } else {
    std::cout << output << std::endl;
  }
}

void FifoLogger::writeToFile(const FifoLogEntry &entry) {
  std::lock_guard<std::mutex> lock(fileMutex_);

  if (logFile_ && logFile_->is_open()) {
    *logFile_ << entry.toFormattedString() << std::endl;

    // Check if rotation is needed
    if (config_.enableLogRotation &&
        getFileSize(config_.logFilePath) > config_.maxLogFileSize) {
      rotateLogFile(config_.logFilePath);
    }
  }
}

void FifoLogger::writeToJsonFile(const FifoLogEntry &entry) {
  std::lock_guard<std::mutex> lock(fileMutex_);

  if (jsonLogFile_ && jsonLogFile_->is_open()) {
    *jsonLogFile_ << entry.toJson().dump() << std::endl;

    // Check if rotation is needed
    if (config_.enableLogRotation &&
        getFileSize(config_.jsonLogFilePath) > config_.maxLogFileSize) {
      rotateLogFile(config_.jsonLogFilePath);
    }
  }
}

void FifoLogger::asyncLoggingThread() {
  while (asyncRunning_.load()) {
    std::unique_lock<std::mutex> lock(asyncMutex_);

    asyncCondition_.wait_for(lock, config_.asyncFlushInterval, [this] {
      return !asyncQueue_.empty() || !asyncRunning_.load();
    });

    processAsyncQueue();
  }
}

void FifoLogger::processAsyncQueue() {
  std::lock_guard<std::mutex> lock(asyncMutex_);

  while (!asyncQueue_.empty()) {
    const auto &entry = asyncQueue_.front();
    writeLogEntry(entry);
    asyncQueue_.pop();
  }
}

bool FifoLogger::shouldLogCategory(const std::string &category) const {
  std::lock_guard<std::mutex> lock(categoryMutex_);

  // Check if category is explicitly disabled
  auto enabledIt = categoryEnabled_.find(category);
  if (enabledIt != categoryEnabled_.end() && !enabledIt->second) {
    return false;
  }

  // Check exclude list
  if (std::find(config_.excludeCategories.begin(),
                config_.excludeCategories.end(),
                category) != config_.excludeCategories.end()) {
    return false;
  }

  // Check include-only list
  if (!config_.includeOnlyCategories.empty()) {
    return std::find(config_.includeOnlyCategories.begin(),
                     config_.includeOnlyCategories.end(),
                     category) != config_.includeOnlyCategories.end();
  }

  return true;
}

bool FifoLogger::shouldLogLevel(FifoLogLevel level) const {
  return isLevelEnabled(level);
}

void FifoLogger::initializeLogFiles() {
  std::lock_guard<std::mutex> lock(fileMutex_);

  if (config_.enableFileLogging) {
    logFile_ =
        std::make_unique<std::ofstream>(config_.logFilePath, std::ios::app);
    if (!logFile_->is_open()) {
      std::cerr << "Failed to open log file: " << config_.logFilePath
                << std::endl;
    }
  }

  if (config_.enableJsonLogging) {
    jsonLogFile_ =
        std::make_unique<std::ofstream>(config_.jsonLogFilePath, std::ios::app);
    if (!jsonLogFile_->is_open()) {
      std::cerr << "Failed to open JSON log file: " << config_.jsonLogFilePath
                << std::endl;
    }
  }
}

void FifoLogger::closeLogFiles() {
  std::lock_guard<std::mutex> lock(fileMutex_);

  if (logFile_ && logFile_->is_open()) {
    logFile_->close();
    logFile_.reset();
  }

  if (jsonLogFile_ && jsonLogFile_->is_open()) {
    jsonLogFile_->close();
    jsonLogFile_.reset();
  }
}

void FifoLogger::rotateLogFile(const std::string &filePath) {
  // Close current file
  if (filePath == config_.logFilePath && logFile_) {
    logFile_->close();
  } else if (filePath == config_.jsonLogFilePath && jsonLogFile_) {
    jsonLogFile_->close();
  }

  // Rotate existing files
  for (int i = config_.maxLogFiles - 1; i > 0; --i) {
    std::string oldFile = getLogFileName(filePath, i - 1);
    std::string newFile = getLogFileName(filePath, i);

    if (std::filesystem::exists(oldFile)) {
      std::filesystem::rename(oldFile, newFile);
    }
  }

  // Move current file to .1
  if (std::filesystem::exists(filePath)) {
    std::filesystem::rename(filePath, getLogFileName(filePath, 1));
  }

  // Reopen file
  if (filePath == config_.logFilePath) {
    logFile_ = std::make_unique<std::ofstream>(filePath, std::ios::app);
  } else if (filePath == config_.jsonLogFilePath) {
    jsonLogFile_ = std::make_unique<std::ofstream>(filePath, std::ios::app);
  }
}

void FifoLogger::cleanupOldTraces() {
  if (config_.traceRetentionTime.count() == 0) {
    return;
  }

  auto now = std::chrono::system_clock::now();
  std::queue<FifoMessageTrace> cleanedTraces;

  while (!messageTraces_.empty()) {
    const auto &trace = messageTraces_.front();
    messageTraces_.pop();

    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - trace.timestamp);
    if (age <= config_.traceRetentionTime) {
      cleanedTraces.push(trace);
    }
  }

  messageTraces_ = cleanedTraces;
}

void FifoLogger::cleanupOldLogs() {
  // Similar cleanup for log entries if needed
}

std::string FifoLogger::getLogFileName(const std::string &basePath,
                                       int index) const {
  if (index == 0) {
    return basePath;
  }

  std::filesystem::path path(basePath);
  std::string stem = path.stem().string();
  std::string extension = path.extension().string();
  std::string directory = path.parent_path().string();

  return directory + "/" + stem + "." + std::to_string(index) + extension;
}

size_t FifoLogger::getFileSize(const std::string &filePath) const {
  try {
    if (std::filesystem::exists(filePath)) {
      return std::filesystem::file_size(filePath);
    }
  } catch (const std::exception &) {
    // Ignore errors
  }
  return 0;
}

// Global instance
FifoLogger &getGlobalFifoLogger() {
  static FifoLogger instance;
  return instance;
}

} // namespace core
} // namespace hydrogen

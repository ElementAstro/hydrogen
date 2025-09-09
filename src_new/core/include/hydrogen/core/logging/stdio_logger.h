#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#endif

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Logging levels for stdio communication
 */
enum class StdioLogLevel {
  TRACE = 0,
  DEBUG = 1,
  INFO = 2,
  WARN = 3,
  ERR = 4,
  CRITICAL = 5,
  OFF = 6
};

/**
 * @brief Message trace information
 */
struct MessageTrace {
  std::string messageId;
  std::string clientId;
  std::string direction; // "INCOMING", "OUTGOING", "INTERNAL"
  std::string messageType;
  size_t messageSize;
  std::chrono::system_clock::time_point timestamp;
  std::chrono::microseconds processingTime;
  json messageContent;
  std::unordered_map<std::string, std::string> metadata;
  bool success = true;
  std::string errorMessage;
};

/**
 * @brief Performance metrics for stdio communication
 */
struct PerformanceMetrics {
  // Message statistics
  std::atomic<uint64_t> totalMessages{0};
  std::atomic<uint64_t> successfulMessages{0};
  std::atomic<uint64_t> failedMessages{0};
  std::atomic<uint64_t> totalBytes{0};

  // Timing statistics
  std::atomic<uint64_t> totalProcessingTime{0}; // microseconds
  std::atomic<uint64_t> minProcessingTime{UINT64_MAX};
  std::atomic<uint64_t> maxProcessingTime{0};

  // Connection statistics
  std::atomic<uint64_t> totalConnections{0};
  std::atomic<uint64_t> activeConnections{0};
  std::atomic<uint64_t> connectionErrors{0};

  // Error statistics
  std::atomic<uint64_t> protocolErrors{0};
  std::atomic<uint64_t> timeoutErrors{0};
  std::atomic<uint64_t> validationErrors{0};

  std::chrono::system_clock::time_point startTime;

  // Custom copy constructor to handle atomic members
  PerformanceMetrics() = default;
  PerformanceMetrics(const PerformanceMetrics& other) {
    totalMessages = other.totalMessages.load();
    successfulMessages = other.successfulMessages.load();
    failedMessages = other.failedMessages.load();
    totalBytes = other.totalBytes.load();
    totalProcessingTime = other.totalProcessingTime.load();
    minProcessingTime = other.minProcessingTime.load();
    maxProcessingTime = other.maxProcessingTime.load();
    totalConnections = other.totalConnections.load();
    activeConnections = other.activeConnections.load();
    connectionErrors = other.connectionErrors.load();
    protocolErrors = other.protocolErrors.load();
    timeoutErrors = other.timeoutErrors.load();
    validationErrors = other.validationErrors.load();
    startTime = other.startTime;
  }

  // Custom assignment operator to handle atomic members
  PerformanceMetrics& operator=(const PerformanceMetrics& other) {
    if (this != &other) {
      totalMessages = other.totalMessages.load();
      successfulMessages = other.successfulMessages.load();
      failedMessages = other.failedMessages.load();
      totalBytes = other.totalBytes.load();
      totalProcessingTime = other.totalProcessingTime.load();
      minProcessingTime = other.minProcessingTime.load();
      maxProcessingTime = other.maxProcessingTime.load();
      totalConnections = other.totalConnections.load();
      activeConnections = other.activeConnections.load();
      connectionErrors = other.connectionErrors.load();
      protocolErrors = other.protocolErrors.load();
      timeoutErrors = other.timeoutErrors.load();
      validationErrors = other.validationErrors.load();
      startTime = other.startTime;
    }
    return *this;
  }

  // Helper methods
  double getAverageProcessingTime() const;
  double getMessagesPerSecond() const;
  double getBytesPerSecond() const;
  double getSuccessRate() const;
  json toJson() const;
};

/**
 * @brief Comprehensive logging and debugging system for stdio communication
 */
class StdioLogger {
public:
  /**
   * @brief Logger configuration
   */
  struct LoggerConfig {
    StdioLogLevel logLevel;
    bool enableConsoleLogging;
    bool enableFileLogging;
    std::string logFileName;
    size_t maxFileSize; // 10MB
    size_t maxFiles;
    bool enableMessageTracing;
    bool enablePerformanceMetrics;
    bool enableDebugMode;
    std::string logPattern;
    bool enableAsyncLogging;
    size_t asyncQueueSize;
    bool enableJsonLogging;
    std::vector<std::string> trackedClients;      // Empty means track all
    std::vector<std::string> trackedMessageTypes; // Empty means track all

    // Default constructor
    LoggerConfig()
      : logLevel(StdioLogLevel::INFO)
      , enableConsoleLogging(true)
      , enableFileLogging(true)
      , logFileName("stdio_communication.log")
      , maxFileSize(10 * 1024 * 1024)
      , maxFiles(5)
      , enableMessageTracing(false)
      , enablePerformanceMetrics(true)
      , enableDebugMode(false)
      , logPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v")
      , enableAsyncLogging(false)
      , asyncQueueSize(8192)
      , enableJsonLogging(false)
    {}
  };

  explicit StdioLogger(const LoggerConfig &config = LoggerConfig());
  ~StdioLogger();

  // Configuration management
  void updateConfig(const LoggerConfig &config);
  LoggerConfig getConfig() const;

  // Basic logging methods
  void trace(const std::string &message, const std::string &clientId = "");
  void debug(const std::string &message, const std::string &clientId = "");
  void info(const std::string &message, const std::string &clientId = "");
  void warn(const std::string &message, const std::string &clientId = "");
  void error(const std::string &message, const std::string &clientId = "");
  void critical(const std::string &message, const std::string &clientId = "");

  // Message tracing
  void traceMessage(const MessageTrace &trace);
  void traceIncomingMessage(const std::string &messageId,
                            const std::string &clientId,
                            const std::string &messageType, const json &content,
                            size_t size);
  void traceOutgoingMessage(const std::string &messageId,
                            const std::string &clientId,
                            const std::string &messageType, const json &content,
                            size_t size);
  void traceMessageProcessing(const std::string &messageId,
                              std::chrono::microseconds processingTime,
                              bool success,
                              const std::string &errorMessage = "");

  // Connection logging
  void logClientConnect(
      const std::string &clientId,
      const std::unordered_map<std::string, std::string> &metadata = {});
  void logClientDisconnect(const std::string &clientId,
                           const std::string &reason = "");
  void logConnectionError(const std::string &clientId,
                          const std::string &error);

  // Protocol logging
  void logProtocolEvent(const std::string &event, const std::string &details,
                        const std::string &clientId = "");
  void logProtocolError(const std::string &error,
                        const std::string &clientId = "");
  void logValidationError(const std::string &messageId,
                          const std::string &error,
                          const std::string &clientId = "");

  // Performance metrics
  void recordMessage(bool success, size_t bytes,
                     std::chrono::microseconds processingTime);
  void recordConnection(bool success);
  void recordError(const std::string &errorType);
  PerformanceMetrics getMetrics() const;
  void resetMetrics();

  // Debug utilities
  void enableDebugMode(bool enable = true);
  void setDebugFilter(const std::vector<std::string> &clientIds,
                      const std::vector<std::string> &messageTypes);
  void dumpDebugInfo(const std::string &filename = "") const;

  // Message history (for debugging)
  void enableMessageHistory(size_t maxMessages = 1000);
  void disableMessageHistory();
  std::vector<MessageTrace>
  getMessageHistory(const std::string &clientId = "") const;
  void clearMessageHistory();

  // Log analysis
  json generateReport(std::chrono::system_clock::time_point startTime = {},
                      std::chrono::system_clock::time_point endTime = {}) const;
  std::vector<std::string> getTopErrors(size_t count = 10) const;
  std::vector<std::string> getMostActiveClients(size_t count = 10) const;

  // Utility methods
  static std::string formatMessage(const json &message, bool pretty = false);
  static std::string formatDuration(std::chrono::microseconds duration);
  static std::string formatBytes(size_t bytes);

private:
  LoggerConfig config_;
#ifdef HYDROGEN_HAS_SPDLOG
  std::shared_ptr<spdlog::logger> logger_;
  std::shared_ptr<spdlog::logger> traceLogger_;
#endif

  // Performance metrics
  PerformanceMetrics metrics_;

  // Message history for debugging
  mutable std::mutex historyMutex_;
  std::vector<MessageTrace> messageHistory_;
  size_t maxHistorySize_ = 0;
  bool historyEnabled_ = false;

  // Error tracking
  mutable std::mutex errorsMutex_;
  std::unordered_map<std::string, uint64_t> errorCounts_;

  // Client activity tracking
  mutable std::mutex clientsMutex_;
  std::unordered_map<std::string, uint64_t> clientActivity_;

  // Helper methods
  void initializeLoggers();
  bool shouldTrace(const std::string &clientId,
                   const std::string &messageType) const;
  void addToHistory(const MessageTrace &trace);
  std::string formatLogMessage(const std::string &message,
                               const std::string &clientId) const;
  void updateErrorStats(const std::string &error);
  void updateClientActivity(const std::string &clientId);

  // JSON formatting helpers
  json messageTraceToJson(const MessageTrace &trace) const;
  json configToJson() const;
};

/**
 * @brief Global stdio logger instance
 */
StdioLogger &getGlobalStdioLogger();

/**
 * @brief RAII class for automatic message tracing
 */
class MessageTracer {
public:
  MessageTracer(StdioLogger &logger, const std::string &messageId,
                const std::string &clientId, const std::string &operation);
  ~MessageTracer();

  void setSuccess(bool success);
  void setError(const std::string &error);
  void addMetadata(const std::string &key, const std::string &value);

private:
  StdioLogger &logger_;
  std::string messageId_;
  std::string clientId_;
  std::string operation_;
  std::chrono::system_clock::time_point startTime_;
  bool success_ = true;
  std::string error_;
  std::unordered_map<std::string, std::string> metadata_;
};

/**
 * @brief Macro helpers for convenient logging
 */
#define STDIO_LOG_TRACE(msg, clientId)                                         \
  getGlobalStdioLogger().trace(msg, clientId)
#define STDIO_LOG_DEBUG(msg, clientId)                                         \
  getGlobalStdioLogger().debug(msg, clientId)
#define STDIO_LOG_INFO(msg, clientId) getGlobalStdioLogger().info(msg, clientId)
#define STDIO_LOG_WARN(msg, clientId) getGlobalStdioLogger().warn(msg, clientId)
#define STDIO_LOG_ERROR(msg, clientId)                                         \
  getGlobalStdioLogger().error(msg, clientId)
#define STDIO_LOG_CRITICAL(msg, clientId)                                      \
  getGlobalStdioLogger().critical(msg, clientId)

// Note: STDIO_TRACE_MESSAGE macro removed due to variadic macro issues
// Use MessageTracer directly: MessageTracer tracer(getGlobalStdioLogger(), args...);

} // namespace core
} // namespace hydrogen

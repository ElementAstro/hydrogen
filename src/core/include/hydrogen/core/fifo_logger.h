#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>
#include <queue>
#include <thread>
#include <condition_variable>
#include <fstream>
#include <functional>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief FIFO log levels
 */
enum class FifoLogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6
};

/**
 * @brief FIFO log entry structure
 */
struct FifoLogEntry {
    std::chrono::system_clock::time_point timestamp;
    FifoLogLevel level;
    std::string category;
    std::string message;
    std::string clientId;
    std::string pipePath;
    std::unordered_map<std::string, std::string> metadata;
    
    FifoLogEntry() : timestamp(std::chrono::system_clock::now()) {}
    
    json toJson() const;
    std::string toString() const;
    std::string toFormattedString() const;
};

/**
 * @brief FIFO performance metrics
 */
struct FifoMetrics {
    std::atomic<uint64_t> totalMessages{0};
    std::atomic<uint64_t> totalBytes{0};
    std::atomic<uint64_t> totalErrors{0};
    std::atomic<uint64_t> totalConnections{0};
    std::atomic<uint64_t> totalDisconnections{0};
    std::atomic<uint64_t> totalReconnections{0};
    
    // Timing metrics
    std::atomic<uint64_t> totalLatencyMicros{0};
    std::atomic<uint64_t> maxLatencyMicros{0};
    std::atomic<uint64_t> minLatencyMicros{UINT64_MAX};
    
    // Throughput metrics
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point lastResetTime;
    
    FifoMetrics() : startTime(std::chrono::system_clock::now()),
                    lastResetTime(std::chrono::system_clock::now()) {}
    
    double getMessagesPerSecond() const;
    double getBytesPerSecond() const;
    double getAverageLatencyMicros() const;
    std::chrono::milliseconds getUptime() const;
    void reset();
    json toJson() const;
};

/**
 * @brief FIFO message trace entry
 */
struct FifoMessageTrace {
    std::string messageId;
    std::string clientId;
    std::string pipePath;
    std::chrono::system_clock::time_point timestamp;
    std::string direction; // "SENT" or "RECEIVED"
    size_t messageSize;
    std::string messageType;
    std::string content;
    std::chrono::microseconds processingTime;
    
    json toJson() const;
};

/**
 * @brief FIFO logger configuration
 */
struct FifoLoggerConfig {
    // Basic logging settings
    FifoLogLevel logLevel = FifoLogLevel::INFO;
    bool enableConsoleLogging = true;
    bool enableFileLogging = false;
    bool enableAsyncLogging = true;
    bool enableJsonLogging = false;
    
    // File logging settings
    std::string logFilePath = "fifo_communication.log";
    std::string jsonLogFilePath = "fifo_communication.json";
    size_t maxLogFileSize = 10 * 1024 * 1024; // 10MB
    int maxLogFiles = 5;
    bool enableLogRotation = true;
    
    // Message tracing
    bool enableMessageTracing = false;
    bool enableMessageContent = false;
    size_t maxTraceEntries = 10000;
    std::chrono::milliseconds traceRetentionTime{3600000}; // 1 hour
    
    // Performance metrics
    bool enablePerformanceMetrics = false;
    std::chrono::milliseconds metricsUpdateInterval{1000};
    bool enableLatencyTracking = false;
    bool enableThroughputTracking = true;
    
    // Debug features
    bool enableDebugMode = false;
    bool enableVerboseLogging = false;
    bool enableStackTraces = false;
    std::vector<std::string> debugCategories;
    
    // Filtering
    std::vector<std::string> excludeCategories;
    std::vector<std::string> includeOnlyCategories;
    FifoLogLevel minLogLevel = FifoLogLevel::TRACE;
    
    // Async logging settings
    size_t asyncQueueSize = 10000;
    std::chrono::milliseconds asyncFlushInterval{1000};
    int asyncWorkerThreads = 1;
    
    // Serialization support
    json toJson() const;
    void fromJson(const json& j);
    bool validate() const;
};

/**
 * @brief FIFO logger implementation
 */
class FifoLogger {
public:
    explicit FifoLogger(const FifoLoggerConfig& config = FifoLoggerConfig{});
    ~FifoLogger();
    
    // Configuration management
    void updateConfig(const FifoLoggerConfig& config);
    const FifoLoggerConfig& getConfig() const { return config_; }
    
    // Basic logging methods
    void log(FifoLogLevel level, const std::string& category, const std::string& message, 
             const std::string& clientId = "", const std::string& pipePath = "");
    void trace(const std::string& category, const std::string& message, 
               const std::string& clientId = "", const std::string& pipePath = "");
    void debug(const std::string& category, const std::string& message, 
               const std::string& clientId = "", const std::string& pipePath = "");
    void info(const std::string& category, const std::string& message, 
              const std::string& clientId = "", const std::string& pipePath = "");
    void warn(const std::string& category, const std::string& message, 
              const std::string& clientId = "", const std::string& pipePath = "");
    void error(const std::string& category, const std::string& message, 
               const std::string& clientId = "", const std::string& pipePath = "");
    void critical(const std::string& category, const std::string& message, 
                  const std::string& clientId = "", const std::string& pipePath = "");
    
    // Message tracing
    void traceMessage(const std::string& messageId, const std::string& clientId, 
                     const std::string& pipePath, const std::string& direction,
                     size_t messageSize, const std::string& messageType,
                     const std::string& content = "", 
                     std::chrono::microseconds processingTime = std::chrono::microseconds(0));
    std::vector<FifoMessageTrace> getMessageTraces(const std::string& clientId = "", 
                                                   std::chrono::milliseconds maxAge = std::chrono::milliseconds(0)) const;
    void clearMessageTraces();
    
    // Performance metrics
    void recordMessage(size_t bytes, std::chrono::microseconds latency = std::chrono::microseconds(0));
    void recordConnection();
    void recordDisconnection();
    void recordReconnection();
    void recordError();
    FifoMetrics getMetrics() const;
    void resetMetrics();
    
    // Log management
    void flush();
    void rotateLogs();
    std::vector<FifoLogEntry> getRecentLogs(size_t count = 100) const;
    void clearLogs();
    
    // Utility methods
    bool isLevelEnabled(FifoLogLevel level) const;
    std::string formatLogLevel(FifoLogLevel level) const;
    std::string getCurrentTimestamp() const;
    
    // Advanced features
    void enableCategory(const std::string& category);
    void disableCategory(const std::string& category);
    void setLogLevelForCategory(const std::string& category, FifoLogLevel level);
    
    // Async logging control
    void startAsyncLogging();
    void stopAsyncLogging();
    bool isAsyncLoggingActive() const;
    
private:
    FifoLoggerConfig config_;
    mutable std::mutex configMutex_;
    
    // Metrics
    mutable FifoMetrics metrics_;
    mutable std::mutex metricsMutex_;
    
    // Message tracing
    std::queue<FifoMessageTrace> messageTraces_;
    mutable std::mutex tracesMutex_;
    
    // Log entries
    std::queue<FifoLogEntry> logEntries_;
    mutable std::mutex logMutex_;
    
    // Async logging
    std::unique_ptr<std::thread> asyncThread_;
    std::queue<FifoLogEntry> asyncQueue_;
    std::mutex asyncMutex_;
    std::condition_variable asyncCondition_;
    std::atomic<bool> asyncRunning_{false};
    
    // File handles
    std::unique_ptr<std::ofstream> logFile_;
    std::unique_ptr<std::ofstream> jsonLogFile_;
    mutable std::mutex fileMutex_;
    
    // Category-specific settings
    std::unordered_map<std::string, FifoLogLevel> categoryLogLevels_;
    std::unordered_map<std::string, bool> categoryEnabled_;
    mutable std::mutex categoryMutex_;
    
    // Internal methods
    void writeLogEntry(const FifoLogEntry& entry);
    void writeToConsole(const FifoLogEntry& entry);
    void writeToFile(const FifoLogEntry& entry);
    void writeToJsonFile(const FifoLogEntry& entry);
    
    void asyncLoggingThread();
    void processAsyncQueue();
    
    bool shouldLogCategory(const std::string& category) const;
    bool shouldLogLevel(FifoLogLevel level) const;
    
    void initializeLogFiles();
    void closeLogFiles();
    void rotateLogFile(const std::string& filePath);
    
    void cleanupOldTraces();
    void cleanupOldLogs();
    
    std::string getLogFileName(const std::string& basePath, int index = 0) const;
    size_t getFileSize(const std::string& filePath) const;
};

/**
 * @brief FIFO debug utilities
 */
class FifoDebugUtils {
public:
    // Message analysis
    static json analyzeMessage(const std::string& message);
    static std::string formatMessage(const std::string& message, bool pretty = true);
    static bool validateMessage(const std::string& message);
    
    // Performance analysis
    static json analyzePerformance(const FifoMetrics& metrics);
    static std::string generatePerformanceReport(const FifoMetrics& metrics);
    static std::vector<std::string> identifyPerformanceIssues(const FifoMetrics& metrics);
    
    // Connection analysis
    static json analyzeConnections(const std::vector<FifoMessageTrace>& traces);
    static std::string generateConnectionReport(const std::vector<FifoMessageTrace>& traces);
    
    // Log analysis
    static json analyzeLogs(const std::vector<FifoLogEntry>& logs);
    static std::string generateLogSummary(const std::vector<FifoLogEntry>& logs);
    static std::vector<FifoLogEntry> filterLogs(const std::vector<FifoLogEntry>& logs, 
                                                FifoLogLevel minLevel = FifoLogLevel::INFO,
                                                const std::string& category = "",
                                                const std::string& clientId = "");
    
    // Diagnostic utilities
    static json runDiagnostics(const FifoLogger& logger);
    static std::string generateDiagnosticReport(const FifoLogger& logger);
    static bool checkLoggerHealth(const FifoLogger& logger);
};

/**
 * @brief Global FIFO logger instance
 */
FifoLogger& getGlobalFifoLogger();

/**
 * @brief Convenience macros for FIFO logging
 */
#define FIFO_LOG_TRACE(category, message, clientId) \
    getGlobalFifoLogger().trace(category, message, clientId)

#define FIFO_LOG_DEBUG(category, message, clientId) \
    getGlobalFifoLogger().debug(category, message, clientId)

#define FIFO_LOG_INFO(category, message, clientId) \
    getGlobalFifoLogger().info(category, message, clientId)

#define FIFO_LOG_WARN(category, message, clientId) \
    getGlobalFifoLogger().warn(category, message, clientId)

#define FIFO_LOG_ERROR(category, message, clientId) \
    getGlobalFifoLogger().error(category, message, clientId)

#define FIFO_LOG_CRITICAL(category, message, clientId) \
    getGlobalFifoLogger().critical(category, message, clientId)

#define FIFO_TRACE_MESSAGE(messageId, clientId, pipePath, direction, size, type, content, time) \
    getGlobalFifoLogger().traceMessage(messageId, clientId, pipePath, direction, size, type, content, time)

} // namespace core
} // namespace hydrogen

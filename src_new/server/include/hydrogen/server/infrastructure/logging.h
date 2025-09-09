#pragma once

#include "../core/service_registry.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <chrono>
#include <sstream>

namespace hydrogen {
namespace server {
namespace infrastructure {

/**
 * @brief Log level enumeration
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6
};

/**
 * @brief Log entry structure
 */
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string logger;
    std::string message;
    std::string file;
    int line;
    std::string function;
    std::string thread;
    std::unordered_map<std::string, std::string> context;
};

/**
 * @brief Log sink interface
 * 
 * Defines the interface for log output destinations (console, file, network, etc.).
 */
class ILogSink {
public:
    virtual ~ILogSink() = default;
    
    virtual bool write(const LogEntry& entry) = 0;
    virtual bool flush() = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
    virtual LogLevel getMinLevel() const = 0;
    virtual void setMinLevel(LogLevel level) = 0;
    virtual std::string getName() const = 0;
};

/**
 * @brief Log formatter interface
 * 
 * Defines the interface for formatting log entries into strings.
 */
class ILogFormatter {
public:
    virtual ~ILogFormatter() = default;
    
    virtual std::string format(const LogEntry& entry) = 0;
    virtual void setPattern(const std::string& pattern) = 0;
    virtual std::string getPattern() const = 0;
};

/**
 * @brief Logger interface
 * 
 * Provides the main logging interface for application components.
 */
class ILogger {
public:
    virtual ~ILogger() = default;
    
    // Basic logging methods
    virtual void log(LogLevel level, const std::string& message) = 0;
    virtual void trace(const std::string& message) = 0;
    virtual void debug(const std::string& message) = 0;
    virtual void info(const std::string& message) = 0;
    virtual void warn(const std::string& message) = 0;
    virtual void error(const std::string& message) = 0;
    virtual void critical(const std::string& message) = 0;
    
    // Formatted logging methods
    template<typename... Args>
    void trace(const std::string& format, Args&&... args);
    template<typename... Args>
    void debug(const std::string& format, Args&&... args);
    template<typename... Args>
    void info(const std::string& format, Args&&... args);
    template<typename... Args>
    void warn(const std::string& format, Args&&... args);
    template<typename... Args>
    void error(const std::string& format, Args&&... args);
    template<typename... Args>
    void critical(const std::string& format, Args&&... args);
    
    // Context logging
    virtual void logWithContext(LogLevel level, const std::string& message, 
                               const std::unordered_map<std::string, std::string>& context) = 0;
    virtual void setContext(const std::string& key, const std::string& value) = 0;
    virtual void removeContext(const std::string& key) = 0;
    virtual void clearContext() = 0;
    
    // Configuration
    virtual void setLevel(LogLevel level) = 0;
    virtual LogLevel getLevel() const = 0;
    virtual bool isEnabled(LogLevel level) const = 0;
    virtual std::string getName() const = 0;
    
    // Sink management
    virtual void addSink(std::shared_ptr<ILogSink> sink) = 0;
    virtual void removeSink(const std::string& sinkName) = 0;
    virtual std::vector<std::shared_ptr<ILogSink>> getSinks() const = 0;
};

/**
 * @brief Logging service interface
 * 
 * Provides centralized logging management for the entire application.
 */
class ILoggingService : public core::IService {
public:
    virtual ~ILoggingService() = default;
    
    // Logger management
    virtual std::shared_ptr<ILogger> getLogger(const std::string& name) = 0;
    virtual std::shared_ptr<ILogger> createLogger(const std::string& name) = 0;
    virtual bool removeLogger(const std::string& name) = 0;
    virtual std::vector<std::string> getLoggerNames() const = 0;
    
    // Global configuration
    virtual void setGlobalLevel(LogLevel level) = 0;
    virtual LogLevel getGlobalLevel() const = 0;
    virtual void setGlobalPattern(const std::string& pattern) = 0;
    virtual std::string getGlobalPattern() const = 0;
    
    // Sink management
    virtual bool addGlobalSink(std::shared_ptr<ILogSink> sink) = 0;
    virtual bool removeGlobalSink(const std::string& sinkName) = 0;
    virtual std::vector<std::shared_ptr<ILogSink>> getGlobalSinks() const = 0;
    
    // Sink factories
    virtual std::shared_ptr<ILogSink> createConsoleSink(const std::string& name = "console") = 0;
    virtual std::shared_ptr<ILogSink> createFileSink(const std::string& name, const std::string& filePath) = 0;
    virtual std::shared_ptr<ILogSink> createRotatingFileSink(const std::string& name, const std::string& filePath, 
                                                            size_t maxSize, size_t maxFiles) = 0;
    virtual std::shared_ptr<ILogSink> createDailyFileSink(const std::string& name, const std::string& filePath) = 0;
    virtual std::shared_ptr<ILogSink> createSyslogSink(const std::string& name, const std::string& ident) = 0;
    
    // Formatter factories
    virtual std::shared_ptr<ILogFormatter> createPatternFormatter(const std::string& pattern) = 0;
    virtual std::shared_ptr<ILogFormatter> createJsonFormatter() = 0;
    
    // Log filtering and processing
    virtual void addFilter(const std::string& name, std::function<bool(const LogEntry&)> filter) = 0;
    virtual void removeFilter(const std::string& name) = 0;
    virtual void addProcessor(const std::string& name, std::function<LogEntry(const LogEntry&)> processor) = 0;
    virtual void removeProcessor(const std::string& name) = 0;
    
    // Log archiving and cleanup
    virtual bool archiveLogs(const std::string& archivePath) = 0;
    virtual bool cleanupOldLogs(std::chrono::hours maxAge) = 0;
    virtual size_t getLogFileSize(const std::string& logFile) const = 0;
    virtual std::vector<std::string> getLogFiles() const = 0;
    
    // Statistics and monitoring
    virtual size_t getLogCount(LogLevel level = LogLevel::TRACE) const = 0;
    virtual std::unordered_map<LogLevel, size_t> getLogStatistics() const = 0;
    virtual void resetStatistics() = 0;
    
    // Performance monitoring
    virtual std::chrono::microseconds getAverageLogTime() const = 0;
    virtual size_t getDroppedLogCount() const = 0;
    virtual bool isAsyncLogging() const = 0;
    virtual void setAsyncLogging(bool enabled) = 0;
    
    // Configuration
    virtual bool loadConfiguration(const std::string& configPath) = 0;
    virtual bool saveConfiguration(const std::string& configPath) const = 0;
    virtual void setBufferSize(size_t size) = 0;
    virtual void setFlushInterval(std::chrono::milliseconds interval) = 0;
    
    // Event callbacks
    using LogEventCallback = std::function<void(const LogEntry& entry)>;
    using LogErrorCallback = std::function<void(const std::string& error, const std::string& context)>;
    
    virtual void setLogEventCallback(LogEventCallback callback) = 0;
    virtual void setLogErrorCallback(LogErrorCallback callback) = 0;
};

/**
 * @brief Logging service factory
 */
class LoggingServiceFactory : public core::IServiceFactory {
public:
    std::unique_ptr<core::IService> createService(
        const std::string& serviceName,
        const std::unordered_map<std::string, std::string>& config = {}
    ) override;
    
    std::vector<std::string> getSupportedServices() const override;
    bool isServiceSupported(const std::string& serviceName) const override;
};

/**
 * @brief Scoped logger for RAII-style logging
 */
class ScopedLogger {
public:
    ScopedLogger(std::shared_ptr<ILogger> logger, const std::string& scope);
    ~ScopedLogger();
    
    template<typename... Args>
    void trace(const std::string& format, Args&&... args);
    template<typename... Args>
    void debug(const std::string& format, Args&&... args);
    template<typename... Args>
    void info(const std::string& format, Args&&... args);
    template<typename... Args>
    void warn(const std::string& format, Args&&... args);
    template<typename... Args>
    void error(const std::string& format, Args&&... args);
    template<typename... Args>
    void critical(const std::string& format, Args&&... args);
    
private:
    std::shared_ptr<ILogger> logger_;
    std::string scope_;
    std::chrono::steady_clock::time_point startTime_;
};

/**
 * @brief Utility macros for convenient logging
 */
#define LOG_TRACE(logger, ...) logger->trace(__VA_ARGS__)
#define LOG_DEBUG(logger, ...) logger->debug(__VA_ARGS__)
#define LOG_INFO(logger, ...) logger->info(__VA_ARGS__)
#define LOG_WARN(logger, ...) logger->warn(__VA_ARGS__)
#define LOG_ERROR(logger, ...) logger->error(__VA_ARGS__)
#define LOG_CRITICAL(logger, ...) logger->critical(__VA_ARGS__)

#define LOG_SCOPE(logger, scope) ScopedLogger _scoped_logger(logger, scope)

/**
 * @brief Global logging functions
 */
std::shared_ptr<ILogger> getLogger(const std::string& name = "default");
void setGlobalLogLevel(LogLevel level);
LogLevel getGlobalLogLevel();

// Template implementations
template<typename... Args>
void ILogger::trace(const std::string& format, Args&&... args) {
    if (isEnabled(LogLevel::TRACE)) {
        std::ostringstream oss;
        ((oss << args << " "), ...);
        trace(format + " " + oss.str());
    }
}

template<typename... Args>
void ILogger::debug(const std::string& format, Args&&... args) {
    if (isEnabled(LogLevel::DEBUG)) {
        std::ostringstream oss;
        ((oss << args << " "), ...);
        debug(format + " " + oss.str());
    }
}

template<typename... Args>
void ILogger::info(const std::string& format, Args&&... args) {
    if (isEnabled(LogLevel::INFO)) {
        std::ostringstream oss;
        ((oss << args << " "), ...);
        info(format + " " + oss.str());
    }
}

template<typename... Args>
void ILogger::warn(const std::string& format, Args&&... args) {
    if (isEnabled(LogLevel::WARN)) {
        std::ostringstream oss;
        ((oss << args << " "), ...);
        warn(format + " " + oss.str());
    }
}

template<typename... Args>
void ILogger::error(const std::string& format, Args&&... args) {
    if (isEnabled(LogLevel::ERROR)) {
        std::ostringstream oss;
        ((oss << args << " "), ...);
        error(format + " " + oss.str());
    }
}

template<typename... Args>
void ILogger::critical(const std::string& format, Args&&... args) {
    if (isEnabled(LogLevel::CRITICAL)) {
        std::ostringstream oss;
        ((oss << args << " "), ...);
        critical(format + " " + oss.str());
    }
}

} // namespace infrastructure
} // namespace server
} // namespace hydrogen

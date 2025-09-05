#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <fstream>
#include <chrono>
#include <functional>

namespace hydrogen {
namespace device {

/**
 * @brief Log level enumeration
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5
};

/**
 * @brief Convert log level to string
 */
std::string logLevelToString(LogLevel level);

/**
 * @brief Device logger class for comprehensive logging
 * 
 * This class provides thread-safe logging capabilities specifically
 * designed for device operations with support for different log levels,
 * file output, and custom formatting.
 */
class DeviceLogger {
public:
    /**
     * @brief Get singleton instance
     */
    static DeviceLogger& getInstance();

    /**
     * @brief Set minimum log level
     * @param level Minimum level to log
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief Set log file path
     * @param filePath Path to log file (empty to disable file logging)
     */
    void setLogFile(const std::string& filePath);

    /**
     * @brief Set console logging enabled/disabled
     * @param enabled True to enable console logging
     */
    void setConsoleLogging(bool enabled);

    /**
     * @brief Set custom log callback
     * @param callback Custom logging function
     */
    void setLogCallback(std::function<void(LogLevel, const std::string&, const std::string&)> callback);

    /**
     * @brief Log a message
     * @param level Log level
     * @param deviceId Device identifier
     * @param message Log message
     */
    void log(LogLevel level, const std::string& deviceId, const std::string& message);

    /**
     * @brief Log trace message
     */
    void trace(const std::string& deviceId, const std::string& message);

    /**
     * @brief Log debug message
     */
    void debug(const std::string& deviceId, const std::string& message);

    /**
     * @brief Log info message
     */
    void info(const std::string& deviceId, const std::string& message);

    /**
     * @brief Log warning message
     */
    void warn(const std::string& deviceId, const std::string& message);

    /**
     * @brief Log error message
     */
    void error(const std::string& deviceId, const std::string& message);

    /**
     * @brief Log critical message
     */
    void critical(const std::string& deviceId, const std::string& message);

    /**
     * @brief Flush all pending log messages
     */
    void flush();

private:
    DeviceLogger() = default;
    ~DeviceLogger();

    /**
     * @brief Format log message
     */
    std::string formatMessage(LogLevel level, const std::string& deviceId, const std::string& message);

    /**
     * @brief Get current timestamp string
     */
    std::string getCurrentTimestamp();

    std::mutex logMutex_;
    LogLevel minLogLevel_ = LogLevel::INFO;
    bool consoleLogging_ = true;
    std::string logFilePath_;
    std::unique_ptr<std::ofstream> logFile_;
    std::function<void(LogLevel, const std::string&, const std::string&)> logCallback_;
};

/**
 * @brief Convenience macros for logging
 */
#define DEVICE_LOG_TRACE(deviceId, message) \
    hydrogen::device::DeviceLogger::getInstance().trace(deviceId, message)

#define DEVICE_LOG_DEBUG(deviceId, message) \
    hydrogen::device::DeviceLogger::getInstance().debug(deviceId, message)

#define DEVICE_LOG_INFO(deviceId, message) \
    hydrogen::device::DeviceLogger::getInstance().info(deviceId, message)

#define DEVICE_LOG_WARN(deviceId, message) \
    hydrogen::device::DeviceLogger::getInstance().warn(deviceId, message)

#define DEVICE_LOG_ERROR(deviceId, message) \
    hydrogen::device::DeviceLogger::getInstance().error(deviceId, message)

#define DEVICE_LOG_CRITICAL(deviceId, message) \
    hydrogen::device::DeviceLogger::getInstance().critical(deviceId, message)

/**
 * @brief Exception handling utility class
 */
class DeviceException : public std::exception {
public:
    DeviceException(const std::string& deviceId, const std::string& message);
    DeviceException(const std::string& deviceId, const std::string& operation, const std::string& message);

    const char* what() const noexcept override;
    const std::string& getDeviceId() const noexcept;
    const std::string& getOperation() const noexcept;
    const std::string& getMessage() const noexcept;

private:
    std::string deviceId_;
    std::string operation_;
    std::string message_;
    std::string fullMessage_;
};

/**
 * @brief Connection exception for WebSocket devices
 */
class ConnectionException : public DeviceException {
public:
    ConnectionException(const std::string& deviceId, const std::string& message);
};

/**
 * @brief Command execution exception
 */
class CommandException : public DeviceException {
public:
    CommandException(const std::string& deviceId, const std::string& command, const std::string& message);
};

/**
 * @brief Configuration exception
 */
class ConfigurationException : public DeviceException {
public:
    ConfigurationException(const std::string& deviceId, const std::string& parameter, const std::string& message);
};

} // namespace device
} // namespace hydrogen

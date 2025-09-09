#include "hydrogen/device/device_logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace hydrogen {
namespace device {

std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

DeviceLogger& DeviceLogger::getInstance() {
    static DeviceLogger instance;
    return instance;
}

DeviceLogger::~DeviceLogger() {
    flush();
}

void DeviceLogger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(logMutex_);
    minLogLevel_ = level;
}

void DeviceLogger::setLogFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (logFile_) {
        logFile_->close();
        logFile_.reset();
    }
    
    logFilePath_ = filePath;
    
    if (!filePath.empty()) {
        logFile_ = std::make_unique<std::ofstream>(filePath, std::ios::app);
        if (!logFile_->is_open()) {
            std::cerr << "Failed to open log file: " << filePath << std::endl;
            logFile_.reset();
        }
    }
}

void DeviceLogger::setConsoleLogging(bool enabled) {
    std::lock_guard<std::mutex> lock(logMutex_);
    consoleLogging_ = enabled;
}

void DeviceLogger::setLogCallback(std::function<void(LogLevel, const std::string&, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(logMutex_);
    logCallback_ = callback;
}

void DeviceLogger::log(LogLevel level, const std::string& deviceId, const std::string& message) {
    if (level < minLogLevel_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(logMutex_);
    
    std::string formattedMessage = formatMessage(level, deviceId, message);
    
    // Console logging
    if (consoleLogging_) {
        if (level >= LogLevel::ERROR) {
            std::cerr << formattedMessage << std::endl;
        } else {
            std::cout << formattedMessage << std::endl;
        }
    }
    
    // File logging
    if (logFile_ && logFile_->is_open()) {
        *logFile_ << formattedMessage << std::endl;
        logFile_->flush();
    }
    
    // Custom callback
    if (logCallback_) {
        logCallback_(level, deviceId, message);
    }
}

void DeviceLogger::trace(const std::string& deviceId, const std::string& message) {
    log(LogLevel::TRACE, deviceId, message);
}

void DeviceLogger::debug(const std::string& deviceId, const std::string& message) {
    log(LogLevel::DEBUG, deviceId, message);
}

void DeviceLogger::info(const std::string& deviceId, const std::string& message) {
    log(LogLevel::INFO, deviceId, message);
}

void DeviceLogger::warn(const std::string& deviceId, const std::string& message) {
    log(LogLevel::WARN, deviceId, message);
}

void DeviceLogger::error(const std::string& deviceId, const std::string& message) {
    log(LogLevel::ERROR, deviceId, message);
}

void DeviceLogger::critical(const std::string& deviceId, const std::string& message) {
    log(LogLevel::CRITICAL, deviceId, message);
}

void DeviceLogger::flush() {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (consoleLogging_) {
        std::cout.flush();
        std::cerr.flush();
    }
    
    if (logFile_ && logFile_->is_open()) {
        logFile_->flush();
    }
}

std::string DeviceLogger::formatMessage(LogLevel level, const std::string& deviceId, const std::string& message) {
    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "] "
        << "[" << logLevelToString(level) << "] "
        << "[" << deviceId << "] "
        << message;
    return oss.str();
}

std::string DeviceLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// Exception implementations
DeviceException::DeviceException(const std::string& deviceId, const std::string& message)
    : deviceId_(deviceId), message_(message) {
    fullMessage_ = "[" + deviceId_ + "] " + message_;
}

DeviceException::DeviceException(const std::string& deviceId, const std::string& operation, const std::string& message)
    : deviceId_(deviceId), operation_(operation), message_(message) {
    fullMessage_ = "[" + deviceId_ + "] " + operation_ + ": " + message_;
}

const char* DeviceException::what() const noexcept {
    return fullMessage_.c_str();
}

const std::string& DeviceException::getDeviceId() const noexcept {
    return deviceId_;
}

const std::string& DeviceException::getOperation() const noexcept {
    return operation_;
}

const std::string& DeviceException::getMessage() const noexcept {
    return message_;
}

ConnectionException::ConnectionException(const std::string& deviceId, const std::string& message)
    : DeviceException(deviceId, "Connection", message) {
}

CommandException::CommandException(const std::string& deviceId, const std::string& command, const std::string& message)
    : DeviceException(deviceId, "Command[" + command + "]", message) {
}

ConfigurationException::ConfigurationException(const std::string& deviceId, const std::string& parameter, const std::string& message)
    : DeviceException(deviceId, "Config[" + parameter + "]", message) {
}

} // namespace device
} // namespace hydrogen

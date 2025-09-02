#pragma once

#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#endif
#include <string>
#include <memory>
#include <iostream>

namespace hydrogen {
namespace common {

/**
 * @brief Log levels for the application
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERR = 4,  // Renamed from ERROR to avoid Windows macro conflict
    CRITICAL = 5
};

/**
 * @brief Initialize the logger with file output
 * @param filename Log file name
 * @param level Log level
 */
void initLogger(const std::string& filename, LogLevel level);

/**
 * @brief Log an info message
 * @param message The message to log
 * @param component Component name (optional)
 */
void logInfo(const std::string& message, const std::string& component = "");

/**
 * @brief Log a debug message
 * @param message The message to log
 * @param component Component name (optional)
 */
void logDebug(const std::string& message, const std::string& component = "");

/**
 * @brief Log a warning message
 * @param message The message to log
 * @param component Component name (optional)
 */
void logWarning(const std::string& message, const std::string& component = "");

/**
 * @brief Log an error message
 * @param message The message to log
 * @param component Component name (optional)
 */
void logError(const std::string& message, const std::string& component = "");

/**
 * @brief Log a critical message
 * @param message The message to log
 * @param component Component name (optional)
 */
void logCritical(const std::string& message, const std::string& component = "");

} // namespace common
} // namespace hydrogen

// Global convenience functions for backward compatibility
using LogLevel = hydrogen::common::LogLevel;
using hydrogen::common::initLogger;
using hydrogen::common::logInfo;
using hydrogen::common::logDebug;
using hydrogen::common::logWarning;
using hydrogen::common::logError;
using hydrogen::common::logCritical;

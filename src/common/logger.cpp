#include "logger.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <iostream>

namespace astrocomm {
namespace common {

static std::shared_ptr<spdlog::logger> g_logger;

void initLogger(const std::string& filename, LogLevel level) {
    try {
        // Create sinks
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
        
        // Create logger with both sinks
        std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
        g_logger = std::make_shared<spdlog::logger>("main", sinks.begin(), sinks.end());
        
        // Set log level
        spdlog::level::level_enum spdlog_level;
        switch (level) {
            case LogLevel::TRACE:
                spdlog_level = spdlog::level::trace;
                break;
            case LogLevel::DEBUG:
                spdlog_level = spdlog::level::debug;
                break;
            case LogLevel::INFO:
                spdlog_level = spdlog::level::info;
                break;
            case LogLevel::WARN:
                spdlog_level = spdlog::level::warn;
                break;
            case LogLevel::ERR:
                spdlog_level = spdlog::level::err;
                break;
            case LogLevel::CRITICAL:
                spdlog_level = spdlog::level::critical;
                break;
            default:
                spdlog_level = spdlog::level::info;
                break;
        }
        
        g_logger->set_level(spdlog_level);
        
        // Set as default logger
        spdlog::set_default_logger(g_logger);
        
        // Set pattern
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

void logInfo(const std::string& message, const std::string& component) {
    if (g_logger) {
        if (!component.empty()) {
            g_logger->info("[{}] {}", component, message);
        } else {
            g_logger->info(message);
        }
    } else {
        spdlog::info(component.empty() ? message : "[" + component + "] " + message);
    }
}

void logDebug(const std::string& message, const std::string& component) {
    if (g_logger) {
        if (!component.empty()) {
            g_logger->debug("[{}] {}", component, message);
        } else {
            g_logger->debug(message);
        }
    } else {
        spdlog::debug(component.empty() ? message : "[" + component + "] " + message);
    }
}

void logWarning(const std::string& message, const std::string& component) {
    if (g_logger) {
        if (!component.empty()) {
            g_logger->warn("[{}] {}", component, message);
        } else {
            g_logger->warn(message);
        }
    } else {
        spdlog::warn(component.empty() ? message : "[" + component + "] " + message);
    }
}

void logError(const std::string& message, const std::string& component) {
    if (g_logger) {
        if (!component.empty()) {
            g_logger->error("[{}] {}", component, message);
        } else {
            g_logger->error(message);
        }
    } else {
        spdlog::error(component.empty() ? message : "[" + component + "] " + message);
    }
}

void logCritical(const std::string& message, const std::string& component) {
    if (g_logger) {
        if (!component.empty()) {
            g_logger->critical("[{}] {}", component, message);
        } else {
            g_logger->critical(message);
        }
    } else {
        spdlog::critical(component.empty() ? message : "[" + component + "] " + message);
    }
}

} // namespace common
} // namespace astrocomm

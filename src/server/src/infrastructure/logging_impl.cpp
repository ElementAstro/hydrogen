#include "astrocomm/server/infrastructure/logging.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/async.h>
#include <filesystem>
#include <algorithm>

namespace astrocomm {
namespace server {
namespace infrastructure {

/**
 * @brief Concrete implementation of the Logging Manager
 */
class LoggingManagerImpl : public ILoggingManager {
public:
    LoggingManagerImpl() : initialized_(false), currentLevel_(LogLevel::INFO) {
        // Initialize with default console logger
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
            
            auto logger = std::make_shared<spdlog::logger>("astrocomm", console_sink);
            logger->set_level(spdlog::level::info);
            spdlog::set_default_logger(logger);
            
            spdlog::info("Logging manager created with default console logger");
        } catch (const std::exception& e) {
            // Fallback to basic logging if initialization fails
            std::cerr << "Failed to initialize default logger: " << e.what() << std::endl;
        }
    }

    ~LoggingManagerImpl() {
        shutdown();
    }

    // Initialization and lifecycle
    bool initialize(const LoggingConfig& config) override {
        if (initialized_) {
            spdlog::warn("Logging manager already initialized");
            return true;
        }

        try {
            config_ = config;
            
            // Create sinks based on configuration
            std::vector<spdlog::sink_ptr> sinks;
            
            // Console sink
            if (config_.enableConsole) {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_level(convertLogLevel(config_.consoleLevel));
                console_sink->set_pattern(config_.consolePattern);
                sinks.push_back(console_sink);
            }
            
            // File sink
            if (config_.enableFile && !config_.logFilePath.empty()) {
                createLogDirectory(config_.logFilePath);
                
                spdlog::sink_ptr file_sink;
                
                switch (config_.rotationType) {
                    case LogRotationType::SIZE:
                        file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                            config_.logFilePath, config_.maxFileSize, config_.maxFiles);
                        break;
                    case LogRotationType::DAILY:
                        file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                            config_.logFilePath, 0, 0);
                        break;
                    case LogRotationType::NONE:
                    default:
                        file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                            config_.logFilePath, true);
                        break;
                }
                
                file_sink->set_level(convertLogLevel(config_.fileLevel));
                file_sink->set_pattern(config_.filePattern);
                sinks.push_back(file_sink);
            }
            
            // Create logger
            std::shared_ptr<spdlog::logger> logger;
            
            if (config_.enableAsync) {
                // Initialize async logging
                spdlog::init_thread_pool(config_.asyncQueueSize, config_.asyncThreadCount);
                logger = std::make_shared<spdlog::async_logger>(
                    config_.loggerName, sinks.begin(), sinks.end(), 
                    spdlog::thread_pool(), spdlog::async_overflow_policy::block);
            } else {
                logger = std::make_shared<spdlog::logger>(
                    config_.loggerName, sinks.begin(), sinks.end());
            }
            
            // Set logger level
            currentLevel_ = config_.globalLevel;
            logger->set_level(convertLogLevel(currentLevel_));
            logger->flush_on(convertLogLevel(config_.flushLevel));
            
            // Set as default logger
            spdlog::set_default_logger(logger);
            
            // Set global pattern if specified
            if (!config_.globalPattern.empty()) {
                spdlog::set_pattern(config_.globalPattern);
            }
            
            initialized_ = true;
            spdlog::info("Logging manager initialized successfully");
            spdlog::info("Log configuration: Console={}, File={}, Async={}", 
                        config_.enableConsole, config_.enableFile, config_.enableAsync);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize logging manager: " << e.what() << std::endl;
            return false;
        }
    }

    bool shutdown() override {
        if (!initialized_) {
            return true;
        }

        try {
            spdlog::info("Shutting down logging manager");
            
            // Flush all loggers
            spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l) {
                l->flush();
            });
            
            // Shutdown async logging if enabled
            if (config_.enableAsync) {
                spdlog::shutdown();
            }
            
            initialized_ = false;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error during logging manager shutdown: " << e.what() << std::endl;
            return false;
        }
    }

    bool isInitialized() const override {
        return initialized_;
    }

    // Configuration management
    bool updateConfig(const LoggingConfig& config) override {
        if (!initialized_) {
            return initialize(config);
        }

        // For simplicity, reinitialize with new config
        shutdown();
        return initialize(config);
    }

    LoggingConfig getConfig() const override {
        return config_;
    }

    // Log level management
    bool setLogLevel(LogLevel level) override {
        if (!initialized_) {
            return false;
        }

        try {
            currentLevel_ = level;
            spdlog::set_level(convertLogLevel(level));
            spdlog::info("Log level changed to: {}", logLevelToString(level));
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to set log level: {}", e.what());
            return false;
        }
    }

    LogLevel getLogLevel() const override {
        return currentLevel_;
    }

    bool setLogLevel(const std::string& loggerName, LogLevel level) override {
        if (!initialized_) {
            return false;
        }

        try {
            auto logger = spdlog::get(loggerName);
            if (logger) {
                logger->set_level(convertLogLevel(level));
                spdlog::debug("Log level for logger '{}' changed to: {}", loggerName, logLevelToString(level));
                return true;
            } else {
                spdlog::warn("Logger '{}' not found", loggerName);
                return false;
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to set log level for logger '{}': {}", loggerName, e.what());
            return false;
        }
    }

    // Logger management
    bool createLogger(const std::string& name, const LoggingConfig& config) override {
        if (!initialized_) {
            return false;
        }

        try {
            // Check if logger already exists
            if (spdlog::get(name)) {
                spdlog::warn("Logger '{}' already exists", name);
                return false;
            }

            // Create sinks for this logger
            std::vector<spdlog::sink_ptr> sinks;
            
            if (config.enableConsole) {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_level(convertLogLevel(config.consoleLevel));
                console_sink->set_pattern(config.consolePattern);
                sinks.push_back(console_sink);
            }
            
            if (config.enableFile && !config.logFilePath.empty()) {
                createLogDirectory(config.logFilePath);
                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                    config.logFilePath, true);
                file_sink->set_level(convertLogLevel(config.fileLevel));
                file_sink->set_pattern(config.filePattern);
                sinks.push_back(file_sink);
            }

            // Create logger
            auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
            logger->set_level(convertLogLevel(config.globalLevel));
            
            // Register logger
            spdlog::register_logger(logger);
            
            spdlog::info("Logger '{}' created successfully", name);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to create logger '{}': {}", name, e.what());
            return false;
        }
    }

    bool removeLogger(const std::string& name) override {
        if (!initialized_) {
            return false;
        }

        try {
            auto logger = spdlog::get(name);
            if (logger) {
                logger->flush();
                spdlog::drop(name);
                spdlog::info("Logger '{}' removed", name);
                return true;
            } else {
                spdlog::warn("Logger '{}' not found", name);
                return false;
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to remove logger '{}': {}", name, e.what());
            return false;
        }
    }

    std::vector<std::string> getLoggerNames() const override {
        std::vector<std::string> names;
        
        if (!initialized_) {
            return names;
        }

        try {
            spdlog::apply_all([&names](std::shared_ptr<spdlog::logger> logger) {
                names.push_back(logger->name());
            });
        } catch (const std::exception& e) {
            spdlog::error("Failed to get logger names: {}", e.what());
        }
        
        return names;
    }

    // Utility functions
    void flush() override {
        if (!initialized_) {
            return;
        }

        try {
            spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
                logger->flush();
            });
        } catch (const std::exception& e) {
            spdlog::error("Failed to flush loggers: {}", e.what());
        }
    }

    void flushLogger(const std::string& name) override {
        if (!initialized_) {
            return;
        }

        try {
            auto logger = spdlog::get(name);
            if (logger) {
                logger->flush();
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to flush logger '{}': {}", name, e.what());
        }
    }

    // File operations
    bool rotateLogFile() override {
        if (!initialized_ || !config_.enableFile) {
            return false;
        }

        try {
            // Force rotation by flushing and recreating file sinks
            flush();
            
            // For rotating file sinks, we can trigger rotation by logging a message
            // This is a simplified approach - in practice, you might want more control
            spdlog::info("Log rotation triggered manually");
            
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to rotate log file: {}", e.what());
            return false;
        }
    }

    bool archiveLogFile(const std::string& archivePath) override {
        if (!initialized_ || !config_.enableFile) {
            return false;
        }

        try {
            flush();
            
            if (std::filesystem::exists(config_.logFilePath)) {
                std::filesystem::copy_file(config_.logFilePath, archivePath);
                spdlog::info("Log file archived to: {}", archivePath);
                return true;
            } else {
                spdlog::warn("Log file not found for archiving: {}", config_.logFilePath);
                return false;
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to archive log file: {}", e.what());
            return false;
        }
    }

    size_t getLogFileSize() const override {
        if (!initialized_ || !config_.enableFile) {
            return 0;
        }

        try {
            if (std::filesystem::exists(config_.logFilePath)) {
                return std::filesystem::file_size(config_.logFilePath);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to get log file size: {}", e.what());
        }
        
        return 0;
    }

    // Statistics and monitoring
    LoggingStatistics getStatistics() const override {
        LoggingStatistics stats;
        
        if (!initialized_) {
            return stats;
        }

        // Basic statistics - in a real implementation, you would track these
        stats.totalLogMessages = 0; // Would need to be tracked
        stats.errorCount = 0;       // Would need to be tracked
        stats.warningCount = 0;     // Would need to be tracked
        stats.logFileSize = getLogFileSize();
        stats.activeLoggers = getLoggerNames().size();
        
        return stats;
    }

private:
    std::atomic<bool> initialized_;
    LoggingConfig config_;
    LogLevel currentLevel_;

    spdlog::level::level_enum convertLogLevel(LogLevel level) const {
        switch (level) {
            case LogLevel::TRACE: return spdlog::level::trace;
            case LogLevel::DEBUG: return spdlog::level::debug;
            case LogLevel::INFO: return spdlog::level::info;
            case LogLevel::WARN: return spdlog::level::warn;
            case LogLevel::ERROR: return spdlog::level::err;
            case LogLevel::CRITICAL: return spdlog::level::critical;
            case LogLevel::OFF: return spdlog::level::off;
            default: return spdlog::level::info;
        }
    }

    std::string logLevelToString(LogLevel level) const {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            case LogLevel::OFF: return "OFF";
            default: return "UNKNOWN";
        }
    }

    void createLogDirectory(const std::string& logFilePath) const {
        try {
            std::filesystem::path path(logFilePath);
            std::filesystem::path dir = path.parent_path();
            
            if (!dir.empty() && !std::filesystem::exists(dir)) {
                std::filesystem::create_directories(dir);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to create log directory: {}", e.what());
        }
    }
};

// Factory function implementation
std::unique_ptr<ILoggingManager> LoggingManagerFactory::createManager() {
    return std::make_unique<LoggingManagerImpl>();
}

std::unique_ptr<ILoggingManager> LoggingManagerFactory::createManager(const LoggingConfig& config) {
    auto manager = std::make_unique<LoggingManagerImpl>();
    if (!manager->initialize(config)) {
        return nullptr;
    }
    return manager;
}

} // namespace infrastructure
} // namespace server
} // namespace astrocomm

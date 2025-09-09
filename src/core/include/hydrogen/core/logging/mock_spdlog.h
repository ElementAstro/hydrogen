#pragma once

// Mock spdlog header for when logging is disabled
// This provides minimal stubs to allow compilation without spdlog

#include <string>
#include <memory>

namespace spdlog {
    enum level_enum {
        trace = 0,
        debug = 1,
        info = 2,
        warn = 3,
        err = 4,
        critical = 5,
        off = 6
    };

    class logger {
    public:
        template<typename... Args>
        void trace(const char* fmt, const Args&... args) {}
        
        template<typename... Args>
        void debug(const char* fmt, const Args&... args) {}
        
        template<typename... Args>
        void info(const char* fmt, const Args&... args) {}
        
        template<typename... Args>
        void warn(const char* fmt, const Args&... args) {}
        
        template<typename... Args>
        void error(const char* fmt, const Args&... args) {}
        
        template<typename... Args>
        void critical(const char* fmt, const Args&... args) {}
        
        void set_level(level_enum level) {}
        level_enum level() const { return level_enum::info; }
    };

    inline std::shared_ptr<logger> default_logger() { return std::make_shared<logger>(); }
    inline void set_default_logger(std::shared_ptr<logger> logger) {}
    inline void set_level(level_enum level) {}
    
    template<typename... Args>
    inline std::shared_ptr<logger> stdout_color_mt(const std::string& name, const Args&... args) {
        return std::make_shared<logger>();
    }
    
    template<typename... Args>
    inline std::shared_ptr<logger> basic_logger_mt(const std::string& name, const Args&... args) {
        return std::make_shared<logger>();
    }
}

// Common macros
#define SPDLOG_TRACE(...)
#define SPDLOG_DEBUG(...)
#define SPDLOG_INFO(...)
#define SPDLOG_WARN(...)
#define SPDLOG_ERROR(...)
#define SPDLOG_CRITICAL(...)

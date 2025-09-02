#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace device {
namespace core {

using json = nlohmann::json;

/**
 * @brief 错误级别枚举
 */
enum class ErrorLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

/**
 * @brief 错误类别枚举
 */
enum class ErrorCategory {
    COMMUNICATION,      // 通信错误
    HARDWARE,          // 硬件错误
    SOFTWARE,          // 软件错误
    CONFIGURATION,     // 配置错误
    VALIDATION,        // 验证错误
    TIMEOUT,           // 超时错误
    PERMISSION,        // 权限错误
    RESOURCE,          // 资源错误
    UNKNOWN            // 未知错误
};

/**
 * @brief 错误信息结构
 */
struct ErrorInfo {
    std::string errorId;                    // 错误ID
    ErrorLevel level;                       // 错误级别
    ErrorCategory category;                 // 错误类别
    std::string message;                    // 错误消息
    std::string details;                    // 详细信息
    std::string source;                     // 错误源（设备ID、组件名等）
    std::chrono::system_clock::time_point timestamp; // 时间�?    json context;                          // 上下文信�?    std::string stackTrace;                // 堆栈跟踪（可选）
    
    /**
     * @brief 转换为JSON
     */
    json toJson() const;
    
    /**
     * @brief 从JSON创建
     */
    static ErrorInfo fromJson(const json& j);
};

/**
 * @brief 错误处理器回调函数类�? */
using ErrorHandler = std::function<void(const ErrorInfo& error)>;

/**
 * @brief 错误恢复策略回调函数类型
 */
using RecoveryStrategy = std::function<bool(const ErrorInfo& error)>;

/**
 * @brief 统一错误处理�? * 
 * 提供统一的错误处理、记录、报告和恢复机制
 */
class ErrorManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ErrorManager& getInstance();

    /**
     * @brief 析构函数
     */
    ~ErrorManager();

    /**
     * @brief 报告错误
     * @param level 错误级别
     * @param category 错误类别
     * @param message 错误消息
     * @param source 错误�?     * @param details 详细信息
     * @param context 上下文信�?     * @return 错误ID
     */
    std::string reportError(ErrorLevel level,
                           ErrorCategory category,
                           const std::string& message,
                           const std::string& source = "",
                           const std::string& details = "",
                           const json& context = json());

    /**
     * @brief 报告错误（使用ErrorInfo�?     * @param error 错误信息
     * @return 错误ID
     */
    std::string reportError(const ErrorInfo& error);

    /**
     * @brief 添加错误处理�?     * @param handler 错误处理�?     * @return 处理器ID
     */
    size_t addErrorHandler(ErrorHandler handler);

    /**
     * @brief 移除错误处理�?     * @param handlerId 处理器ID
     */
    void removeErrorHandler(size_t handlerId);

    /**
     * @brief 添加恢复策略
     * @param category 错误类别
     * @param strategy 恢复策略
     */
    void addRecoveryStrategy(ErrorCategory category, RecoveryStrategy strategy);

    /**
     * @brief 移除恢复策略
     * @param category 错误类别
     */
    void removeRecoveryStrategy(ErrorCategory category);

    /**
     * @brief 设置错误级别过滤
     * @param minLevel 最小错误级�?     */
    void setErrorLevelFilter(ErrorLevel minLevel);

    /**
     * @brief 获取错误历史
     * @param maxCount 最大数量（0表示全部�?     * @return 错误历史列表
     */
    std::vector<ErrorInfo> getErrorHistory(size_t maxCount = 0) const;

    /**
     * @brief 获取指定类别的错�?     * @param category 错误类别
     * @param maxCount 最大数�?     * @return 错误列表
     */
    std::vector<ErrorInfo> getErrorsByCategory(ErrorCategory category, size_t maxCount = 0) const;

    /**
     * @brief 获取指定源的错误
     * @param source 错误�?     * @param maxCount 最大数�?     * @return 错误列表
     */
    std::vector<ErrorInfo> getErrorsBySource(const std::string& source, size_t maxCount = 0) const;

    /**
     * @brief 清除错误历史
     */
    void clearErrorHistory();

    /**
     * @brief 获取错误统计
     * @return 错误统计信息
     */
    json getErrorStatistics() const;

    /**
     * @brief 设置最大错误历史数�?     * @param maxCount 最大数�?     */
    void setMaxErrorHistory(size_t maxCount);

    /**
     * @brief 启用/禁用自动恢复
     * @param enabled 是否启用
     */
    void setAutoRecoveryEnabled(bool enabled);

    /**
     * @brief 是否启用自动恢复
     */
    bool isAutoRecoveryEnabled() const;

    /**
     * @brief 导出错误日志
     * @param filename 文件�?     * @return 导出是否成功
     */
    bool exportErrorLog(const std::string& filename) const;

    /**
     * @brief 导入错误日志
     * @param filename 文件�?     * @return 导入是否成功
     */
    bool importErrorLog(const std::string& filename);

private:
    /**
     * @brief 私有构造函数（单例模式�?     */
    ErrorManager();

    /**
     * @brief 生成错误ID
     * @return 唯一错误ID
     */
    std::string generateErrorId();

    /**
     * @brief 处理错误
     * @param error 错误信息
     */
    void handleError(const ErrorInfo& error);

    /**
     * @brief 尝试错误恢复
     * @param error 错误信息
     * @return 恢复是否成功
     */
    bool attemptRecovery(const ErrorInfo& error);

    /**
     * @brief 添加到错误历�?     * @param error 错误信息
     */
    void addToHistory(const ErrorInfo& error);

    /**
     * @brief 更新错误统计
     * @param error 错误信息
     */
    void updateStatistics(const ErrorInfo& error);

private:
    // 错误处理器管�?    struct HandlerInfo {
        size_t id;
        ErrorHandler handler;
    };
    
    mutable std::mutex handlersMutex_;
    std::vector<HandlerInfo> errorHandlers_;
    std::atomic<size_t> nextHandlerId_;

    // 恢复策略管理
    mutable std::mutex strategiesMutex_;
    std::unordered_map<ErrorCategory, RecoveryStrategy> recoveryStrategies_;

    // 错误历史管理
    mutable std::mutex historyMutex_;
    std::vector<ErrorInfo> errorHistory_;
    std::atomic<size_t> maxHistorySize_;

    // 错误统计
    mutable std::mutex statisticsMutex_;
    std::unordered_map<ErrorLevel, size_t> levelCounts_;
    std::unordered_map<ErrorCategory, size_t> categoryCounts_;
    std::unordered_map<std::string, size_t> sourceCounts_;

    // 配置
    std::atomic<ErrorLevel> minErrorLevel_;
    std::atomic<bool> autoRecoveryEnabled_;
    std::atomic<size_t> errorIdCounter_;
};

/**
 * @brief 错误处理宏定�? */
#define REPORT_ERROR(level, category, message, source) \
    ErrorManager::getInstance().reportError(level, category, message, source, "", json())

#define REPORT_ERROR_WITH_DETAILS(level, category, message, source, details) \
    ErrorManager::getInstance().reportError(level, category, message, source, details, json())

#define REPORT_ERROR_WITH_CONTEXT(level, category, message, source, details, context) \
    ErrorManager::getInstance().reportError(level, category, message, source, details, context)

// 便捷�?#define REPORT_DEBUG(message, source) \
    REPORT_ERROR(ErrorLevel::DEBUG, ErrorCategory::SOFTWARE, message, source)

#define REPORT_INFO(message, source) \
    REPORT_ERROR(ErrorLevel::INFO, ErrorCategory::SOFTWARE, message, source)

#define REPORT_WARNING(message, source) \
    REPORT_ERROR(ErrorLevel::WARNING, ErrorCategory::SOFTWARE, message, source)

#define REPORT_ERROR_MSG(message, source) \
    REPORT_ERROR(ErrorLevel::ERROR, ErrorCategory::SOFTWARE, message, source)

#define REPORT_CRITICAL(message, source) \
    REPORT_ERROR(ErrorLevel::CRITICAL, ErrorCategory::SOFTWARE, message, source)

// 硬件错误�?#define REPORT_HARDWARE_ERROR(message, source) \
    REPORT_ERROR(ErrorLevel::ERROR, ErrorCategory::HARDWARE, message, source)

// 通信错误�?#define REPORT_COMMUNICATION_ERROR(message, source) \
    REPORT_ERROR(ErrorLevel::ERROR, ErrorCategory::COMMUNICATION, message, source)

// 配置错误�?#define REPORT_CONFIG_ERROR(message, source) \
    REPORT_ERROR(ErrorLevel::ERROR, ErrorCategory::CONFIGURATION, message, source)

} // namespace core
} // namespace device
} // namespace hydrogen

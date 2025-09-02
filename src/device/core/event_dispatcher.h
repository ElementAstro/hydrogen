#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <thread>
#include <condition_variable>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace device {
namespace core {

using json = nlohmann::json;

/**
 * @brief 事件优先级枚�? */
enum class EventPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * @brief 事件类型枚举
 */
enum class EventType {
    DEVICE_CONNECTED,       // 设备连接
    DEVICE_DISCONNECTED,    // 设备断开
    DEVICE_ERROR,          // 设备错误
    PROPERTY_CHANGED,      // 属性变�?    CONFIG_CHANGED,        // 配置变化
    COMMAND_EXECUTED,      // 命令执行
    STATUS_UPDATE,         // 状态更�?    MOVEMENT_COMPLETE,     // 移动完成
    TEMPERATURE_STABLE,    // 温度稳定
    EXPOSURE_COMPLETE,     // 曝光完成
    CUSTOM                 // 自定义事�?};

/**
 * @brief 事件信息结构
 */
struct EventInfo {
    std::string eventId;                    // 事件ID
    EventType type;                         // 事件类型
    EventPriority priority;                 // 事件优先�?    std::string source;                     // 事件�?    std::string name;                       // 事件名称
    json data;                             // 事件数据
    std::chrono::system_clock::time_point timestamp; // 时间�?    
    /**
     * @brief 转换为JSON
     */
    json toJson() const;
    
    /**
     * @brief 从JSON创建
     */
    static EventInfo fromJson(const json& j);
};

/**
 * @brief 事件监听器回调函数类�? */
using EventListener = std::function<void(const EventInfo& event)>;

/**
 * @brief 事件过滤器回调函数类�? */
using EventFilter = std::function<bool(const EventInfo& event)>;

/**
 * @brief 事件分发�? * 
 * 提供统一的事件分发、监听和管理机制
 */
class EventDispatcher {
public:
    /**
     * @brief 获取单例实例
     */
    static EventDispatcher& getInstance();

    /**
     * @brief 析构函数
     */
    ~EventDispatcher();

    /**
     * @brief 分发事件
     * @param type 事件类型
     * @param source 事件�?     * @param name 事件名称
     * @param data 事件数据
     * @param priority 事件优先�?     * @return 事件ID
     */
    std::string dispatchEvent(EventType type,
                             const std::string& source,
                             const std::string& name,
                             const json& data = json(),
                             EventPriority priority = EventPriority::NORMAL);

    /**
     * @brief 分发事件（使用EventInfo�?     * @param event 事件信息
     * @return 事件ID
     */
    std::string dispatchEvent(const EventInfo& event);

    /**
     * @brief 添加事件监听�?     * @param type 事件类型
     * @param listener 监听�?     * @return 监听器ID
     */
    size_t addEventListener(EventType type, EventListener listener);

    /**
     * @brief 添加事件监听器（指定源）
     * @param type 事件类型
     * @param source 事件�?     * @param listener 监听�?     * @return 监听器ID
     */
    size_t addEventListener(EventType type, const std::string& source, EventListener listener);

    /**
     * @brief 添加全局事件监听�?     * @param listener 监听�?     * @return 监听器ID
     */
    size_t addGlobalEventListener(EventListener listener);

    /**
     * @brief 移除事件监听�?     * @param listenerId 监听器ID
     */
    void removeEventListener(size_t listenerId);

    /**
     * @brief 添加事件过滤�?     * @param filter 过滤�?     * @return 过滤器ID
     */
    size_t addEventFilter(EventFilter filter);

    /**
     * @brief 移除事件过滤�?     * @param filterId 过滤器ID
     */
    void removeEventFilter(size_t filterId);

    /**
     * @brief 启动事件处理
     */
    void start();

    /**
     * @brief 停止事件处理
     */
    void stop();

    /**
     * @brief 是否正在运行
     */
    bool isRunning() const;

    /**
     * @brief 设置事件队列最大大�?     * @param maxSize 最大大�?     */
    void setMaxQueueSize(size_t maxSize);

    /**
     * @brief 获取事件队列大小
     */
    size_t getQueueSize() const;

    /**
     * @brief 获取事件历史
     * @param maxCount 最大数量（0表示全部�?     * @return 事件历史列表
     */
    std::vector<EventInfo> getEventHistory(size_t maxCount = 0) const;

    /**
     * @brief 获取指定类型的事�?     * @param type 事件类型
     * @param maxCount 最大数�?     * @return 事件列表
     */
    std::vector<EventInfo> getEventsByType(EventType type, size_t maxCount = 0) const;

    /**
     * @brief 获取指定源的事件
     * @param source 事件�?     * @param maxCount 最大数�?     * @return 事件列表
     */
    std::vector<EventInfo> getEventsBySource(const std::string& source, size_t maxCount = 0) const;

    /**
     * @brief 清除事件历史
     */
    void clearEventHistory();

    /**
     * @brief 获取事件统计
     * @return 事件统计信息
     */
    json getEventStatistics() const;

    /**
     * @brief 设置最大事件历史数�?     * @param maxCount 最大数�?     */
    void setMaxEventHistory(size_t maxCount);

    /**
     * @brief 导出事件日志
     * @param filename 文件�?     * @return 导出是否成功
     */
    bool exportEventLog(const std::string& filename) const;

    /**
     * @brief 导入事件日志
     * @param filename 文件�?     * @return 导入是否成功
     */
    bool importEventLog(const std::string& filename);

private:
    /**
     * @brief 私有构造函数（单例模式�?     */
    EventDispatcher();

    /**
     * @brief 生成事件ID
     * @return 唯一事件ID
     */
    std::string generateEventId();

    /**
     * @brief 事件处理线程函数
     */
    void eventProcessingLoop();

    /**
     * @brief 处理事件
     * @param event 事件信息
     */
    void processEvent(const EventInfo& event);

    /**
     * @brief 通知监听�?     * @param event 事件信息
     */
    void notifyListeners(const EventInfo& event);

    /**
     * @brief 应用事件过滤�?     * @param event 事件信息
     * @return 是否通过过滤
     */
    bool applyFilters(const EventInfo& event);

    /**
     * @brief 添加到事件历�?     * @param event 事件信息
     */
    void addToHistory(const EventInfo& event);

    /**
     * @brief 更新事件统计
     * @param event 事件信息
     */
    void updateStatistics(const EventInfo& event);

private:
    // 事件队列管理
    struct EventComparator {
        bool operator()(const EventInfo& a, const EventInfo& b) const {
            return static_cast<int>(a.priority) < static_cast<int>(b.priority);
        }
    };
    
    mutable std::mutex queueMutex_;
    std::priority_queue<EventInfo, std::vector<EventInfo>, EventComparator> eventQueue_;
    std::condition_variable queueCondition_;
    std::atomic<size_t> maxQueueSize_;

    // 监听器管�?    struct ListenerInfo {
        size_t id;
        EventType type;
        std::string source;
        EventListener listener;
        bool isGlobal;
    };
    
    mutable std::mutex listenersMutex_;
    std::vector<ListenerInfo> eventListeners_;
    std::atomic<size_t> nextListenerId_;

    // 过滤器管�?    struct FilterInfo {
        size_t id;
        EventFilter filter;
    };
    
    mutable std::mutex filtersMutex_;
    std::vector<FilterInfo> eventFilters_;
    std::atomic<size_t> nextFilterId_;

    // 事件历史管理
    mutable std::mutex historyMutex_;
    std::vector<EventInfo> eventHistory_;
    std::atomic<size_t> maxHistorySize_;

    // 事件统计
    mutable std::mutex statisticsMutex_;
    std::unordered_map<EventType, size_t> typeCounts_;
    std::unordered_map<EventPriority, size_t> priorityCounts_;
    std::unordered_map<std::string, size_t> sourceCounts_;

    // 线程管理
    std::thread processingThread_;
    std::atomic<bool> running_;
    std::atomic<size_t> eventIdCounter_;
};

/**
 * @brief 事件分发宏定�? */
#define DISPATCH_EVENT(type, source, name, data) \
    EventDispatcher::getInstance().dispatchEvent(type, source, name, data)

#define DISPATCH_HIGH_PRIORITY_EVENT(type, source, name, data) \
    EventDispatcher::getInstance().dispatchEvent(type, source, name, data, EventPriority::HIGH)

#define DISPATCH_CRITICAL_EVENT(type, source, name, data) \
    EventDispatcher::getInstance().dispatchEvent(type, source, name, data, EventPriority::CRITICAL)

// 便捷�?#define DISPATCH_DEVICE_CONNECTED(source) \
    DISPATCH_EVENT(EventType::DEVICE_CONNECTED, source, "connected", json())

#define DISPATCH_DEVICE_DISCONNECTED(source) \
    DISPATCH_EVENT(EventType::DEVICE_DISCONNECTED, source, "disconnected", json())

#define DISPATCH_PROPERTY_CHANGED(source, property, value) \
    DISPATCH_EVENT(EventType::PROPERTY_CHANGED, source, property, json{{"value", value}})

#define DISPATCH_CONFIG_CHANGED(source, config, value) \
    DISPATCH_EVENT(EventType::CONFIG_CHANGED, source, config, json{{"value", value}})

#define DISPATCH_STATUS_UPDATE(source, status) \
    DISPATCH_EVENT(EventType::STATUS_UPDATE, source, "status", status)

} // namespace core
} // namespace device
} // namespace hydrogen

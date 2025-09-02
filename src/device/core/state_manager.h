#pragma once

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>
#include <mutex>
#include <functional>
#include <vector>
#include <atomic>
#include <memory>

namespace hydrogen {
namespace device {
namespace core {

using json = nlohmann::json;

/**
 * @brief 属性变化事件信�? */
struct PropertyChangeEvent {
    std::string propertyName;
    json oldValue;
    json newValue;
    std::string timestamp;
    std::string deviceId;
};

/**
 * @brief 属性变化监听器类型
 */
using PropertyChangeListener = std::function<void(const PropertyChangeEvent& event)>;

/**
 * @brief 属性验证器类型
 */
using PropertyValidator = std::function<bool(const std::string& property, const json& value, std::string& error)>;

/**
 * @brief 状态管理器 - 统一管理设备状态和属�? * 
 * 该类提供了设备状态和属性的统一管理，支持：
 * - 线程安全的属性读�? * - 属性变化监�? * - 属性验�? * - 状态持久化
 * - 批量属性操�? */
class StateManager {
public:
    /**
     * @brief 构造函�?     * @param deviceId 设备ID
     */
    explicit StateManager(const std::string& deviceId);
    
    /**
     * @brief 析构函数
     */
    ~StateManager();

    /**
     * @brief 设置属性�?     * @param property 属性名
     * @param value 属性�?     * @param notify 是否触发变化通知（默认true�?     * @return 设置是否成功
     */
    bool setProperty(const std::string& property, const json& value, bool notify = true);

    /**
     * @brief 批量设置属�?     * @param properties 属性映�?     * @param notify 是否触发变化通知（默认true�?     * @return 成功设置的属性数�?     */
    size_t setProperties(const std::unordered_map<std::string, json>& properties, bool notify = true);

    /**
     * @brief 获取属性�?     * @param property 属性名
     * @return 属性值，如果不存在返回空JSON
     */
    json getProperty(const std::string& property) const;

    /**
     * @brief 获取属性值（带默认值）
     * @param property 属性名
     * @param defaultValue 默认�?     * @return 属性值，如果不存在返回默认�?     */
    json getProperty(const std::string& property, const json& defaultValue) const;

    /**
     * @brief 获取所有属�?     * @return 所有属性的映射
     */
    std::unordered_map<std::string, json> getAllProperties() const;

    /**
     * @brief 检查属性是否存�?     * @param property 属性名
     * @return 属性是否存�?     */
    bool hasProperty(const std::string& property) const;

    /**
     * @brief 删除属�?     * @param property 属性名
     * @return 删除是否成功
     */
    bool removeProperty(const std::string& property);

    /**
     * @brief 清空所有属�?     */
    void clearProperties();

    /**
     * @brief 添加属性变化监听器
     * @param property 属性名（空字符串表示监听所有属性）
     * @param listener 监听器函�?     * @return 监听器ID
     */
    size_t addPropertyChangeListener(const std::string& property, PropertyChangeListener listener);

    /**
     * @brief 移除属性变化监听器
     * @param listenerId 监听器ID
     */
    void removePropertyChangeListener(size_t listenerId);

    /**
     * @brief 设置属性验证器
     * @param property 属性名
     * @param validator 验证器函�?     */
    void setPropertyValidator(const std::string& property, PropertyValidator validator);

    /**
     * @brief 移除属性验证器
     * @param property 属性名
     */
    void removePropertyValidator(const std::string& property);

    /**
     * @brief 获取设备能力列表
     * @return 能力列表
     */
    std::vector<std::string> getCapabilities() const;

    /**
     * @brief 设置设备能力列表
     * @param capabilities 能力列表
     */
    void setCapabilities(const std::vector<std::string>& capabilities);

    /**
     * @brief 添加设备能力
     * @param capability 能力名称
     */
    void addCapability(const std::string& capability);

    /**
     * @brief 检查是否具有某项能�?     * @param capability 能力名称
     * @return 是否具有该能�?     */
    bool hasCapability(const std::string& capability) const;

    /**
     * @brief 保存状态到文件
     * @param filename 文件�?     * @return 保存是否成功
     */
    bool saveToFile(const std::string& filename) const;

    /**
     * @brief 从文件加载状�?     * @param filename 文件�?     * @return 加载是否成功
     */
    bool loadFromFile(const std::string& filename);

    /**
     * @brief 获取状态的JSON表示
     * @return JSON对象
     */
    json toJson() const;

    /**
     * @brief 从JSON加载状�?     * @param jsonData JSON数据
     * @return 加载是否成功
     */
    bool fromJson(const json& jsonData);

private:
    /**
     * @brief 触发属性变化事�?     * @param property 属性名
     * @param oldValue 旧�?     * @param newValue 新�?     */
    void notifyPropertyChange(const std::string& property, const json& oldValue, const json& newValue);

    /**
     * @brief 验证属性�?     * @param property 属性名
     * @param value 属性�?     * @param error 错误信息输出
     * @return 验证是否通过
     */
    bool validateProperty(const std::string& property, const json& value, std::string& error) const;

    /**
     * @brief 生成时间�?     * @return ISO格式时间�?     */
    std::string generateTimestamp() const;

private:
    std::string deviceId_;
    
    // Property storage
    mutable std::mutex propertiesMutex_;
    std::unordered_map<std::string, json> properties_;

    // Capabilities list
    mutable std::mutex capabilitiesMutex_;
    std::vector<std::string> capabilities_;

    // Listener management
    struct ListenerInfo {
        size_t id;
        std::string property;
        PropertyChangeListener listener;
    };
    
    mutable std::mutex listenersMutex_;
    std::vector<ListenerInfo> listeners_;
    std::atomic<size_t> nextListenerId_;

    // Validator management
    mutable std::mutex validatorsMutex_;
    std::unordered_map<std::string, PropertyValidator> validators_;
};

} // namespace core
} // namespace device
} // namespace hydrogen

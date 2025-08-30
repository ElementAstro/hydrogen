#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <memory>

namespace astrocomm {
namespace device {
namespace core {

using json = nlohmann::json;

/**
 * @brief 配置项类型枚举
 */
enum class ConfigType {
    STRING,
    INTEGER,
    DOUBLE,
    BOOLEAN,
    ARRAY,
    OBJECT
};

/**
 * @brief 配置项定义
 */
struct ConfigDefinition {
    std::string name;
    ConfigType type;
    json defaultValue;
    json minValue;      // 对数值类型有效
    json maxValue;      // 对数值类型有效
    std::string description;
    bool required;
    bool readOnly;
    std::function<bool(const json&)> validator; // 自定义验证器
};

/**
 * @brief 配置变化事件
 */
struct ConfigChangeEvent {
    std::string configName;
    json oldValue;
    json newValue;
    std::string timestamp;
    std::string deviceId;
};

/**
 * @brief 配置变化监听器类型
 */
using ConfigChangeListener = std::function<void(const ConfigChangeEvent& event)>;

/**
 * @brief 配置管理器 - 统一管理设备配置
 * 
 * 该类提供了设备配置的统一管理，支持：
 * - 配置定义和验证
 * - 配置持久化
 * - 配置变化监听
 * - 配置模板和预设
 * - 配置导入导出
 */
class ConfigManager {
public:
    /**
     * @brief 构造函数
     * @param deviceId 设备ID
     * @param configFile 配置文件路径（可选）
     */
    explicit ConfigManager(const std::string& deviceId, const std::string& configFile = "");
    
    /**
     * @brief 析构函数
     */
    ~ConfigManager();

    /**
     * @brief 定义配置项
     * @param definition 配置项定义
     * @return 定义是否成功
     */
    bool defineConfig(const ConfigDefinition& definition);

    /**
     * @brief 批量定义配置项
     * @param definitions 配置项定义列表
     * @return 成功定义的配置项数量
     */
    size_t defineConfigs(const std::vector<ConfigDefinition>& definitions);

    /**
     * @brief 设置配置值
     * @param name 配置名
     * @param value 配置值
     * @param persist 是否持久化（默认true）
     * @return 设置是否成功
     */
    bool setConfig(const std::string& name, const json& value, bool persist = true);

    /**
     * @brief 批量设置配置
     * @param configs 配置映射
     * @param persist 是否持久化（默认true）
     * @return 成功设置的配置数量
     */
    size_t setConfigs(const std::unordered_map<std::string, json>& configs, bool persist = true);

    /**
     * @brief 获取配置值
     * @param name 配置名
     * @return 配置值，如果不存在返回默认值或空JSON
     */
    json getConfig(const std::string& name) const;

    /**
     * @brief 获取配置值（带类型转换）
     * @tparam T 目标类型
     * @param name 配置名
     * @param defaultValue 默认值
     * @return 配置值
     */
    template<typename T>
    T getConfig(const std::string& name, const T& defaultValue) const {
        json value = getConfig(name);
        if (value.is_null()) {
            return defaultValue;
        }
        try {
            return value.get<T>();
        } catch (const std::exception&) {
            return defaultValue;
        }
    }

    /**
     * @brief 获取所有配置
     * @return 所有配置的映射
     */
    std::unordered_map<std::string, json> getAllConfigs() const;

    /**
     * @brief 检查配置是否存在
     * @param name 配置名
     * @return 配置是否存在
     */
    bool hasConfig(const std::string& name) const;

    /**
     * @brief 重置配置为默认值
     * @param name 配置名
     * @return 重置是否成功
     */
    bool resetConfig(const std::string& name);

    /**
     * @brief 重置所有配置为默认值
     */
    void resetAllConfigs();

    /**
     * @brief 获取配置定义
     * @param name 配置名
     * @return 配置定义，如果不存在返回nullptr
     */
    std::shared_ptr<ConfigDefinition> getConfigDefinition(const std::string& name) const;

    /**
     * @brief 获取所有配置定义
     * @return 配置定义映射
     */
    std::unordered_map<std::string, std::shared_ptr<ConfigDefinition>> getAllConfigDefinitions() const;

    /**
     * @brief 添加配置变化监听器
     * @param name 配置名（空字符串表示监听所有配置）
     * @param listener 监听器函数
     * @return 监听器ID
     */
    size_t addConfigChangeListener(const std::string& name, ConfigChangeListener listener);

    /**
     * @brief 移除配置变化监听器
     * @param listenerId 监听器ID
     */
    void removeConfigChangeListener(size_t listenerId);

    /**
     * @brief 保存配置到文件
     * @param filename 文件名（可选，使用默认配置文件）
     * @return 保存是否成功
     */
    bool saveToFile(const std::string& filename = "") const;

    /**
     * @brief 从文件加载配置
     * @param filename 文件名（可选，使用默认配置文件）
     * @return 加载是否成功
     */
    bool loadFromFile(const std::string& filename = "");

    /**
     * @brief 导出配置为JSON
     * @param includeDefaults 是否包含默认值
     * @return JSON对象
     */
    json exportToJson(bool includeDefaults = false) const;

    /**
     * @brief 从JSON导入配置
     * @param jsonData JSON数据
     * @param validate 是否验证配置
     * @return 导入是否成功
     */
    bool importFromJson(const json& jsonData, bool validate = true);

    /**
     * @brief 创建配置预设
     * @param presetName 预设名称
     * @param description 预设描述
     * @return 创建是否成功
     */
    bool createPreset(const std::string& presetName, const std::string& description = "");

    /**
     * @brief 应用配置预设
     * @param presetName 预设名称
     * @return 应用是否成功
     */
    bool applyPreset(const std::string& presetName);

    /**
     * @brief 获取所有预设名称
     * @return 预设名称列表
     */
    std::vector<std::string> getPresetNames() const;

    /**
     * @brief 删除配置预设
     * @param presetName 预设名称
     * @return 删除是否成功
     */
    bool deletePreset(const std::string& presetName);

private:
    /**
     * @brief 验证配置值
     * @param name 配置名
     * @param value 配置值
     * @param error 错误信息输出
     * @return 验证是否通过
     */
    bool validateConfig(const std::string& name, const json& value, std::string& error) const;

    /**
     * @brief 触发配置变化事件
     * @param name 配置名
     * @param oldValue 旧值
     * @param newValue 新值
     */
    void notifyConfigChange(const std::string& name, const json& oldValue, const json& newValue);

    /**
     * @brief 生成时间戳
     * @return ISO格式时间戳
     */
    std::string generateTimestamp() const;

    /**
     * @brief 获取配置文件路径
     * @param filename 文件名
     * @return 完整路径
     */
    std::string getConfigFilePath(const std::string& filename) const;

private:
    std::string deviceId_;
    std::string defaultConfigFile_;
    
    // 配置定义
    mutable std::mutex definitionsMutex_;
    std::unordered_map<std::string, std::shared_ptr<ConfigDefinition>> definitions_;
    
    // 配置值
    mutable std::mutex configsMutex_;
    std::unordered_map<std::string, json> configs_;
    
    // 监听器管理
    struct ListenerInfo {
        size_t id;
        std::string configName;
        ConfigChangeListener listener;
    };
    
    mutable std::mutex listenersMutex_;
    std::vector<ListenerInfo> listeners_;
    std::atomic<size_t> nextListenerId_;
    
    // 预设管理
    mutable std::mutex presetsMutex_;
    std::unordered_map<std::string, json> presets_; // presetName -> {description, configs}
};

} // namespace core
} // namespace device
} // namespace astrocomm

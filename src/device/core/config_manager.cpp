#include "config_manager.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace hydrogen {
namespace device {
namespace core {

ConfigManager::ConfigManager(const std::string& deviceId, const std::string& configFile)
    : deviceId_(deviceId)
    , defaultConfigFile_(configFile.empty() ? deviceId + "_config.json" : configFile)
    , nextListenerId_(1) {
    
    SPDLOG_DEBUG("ConfigManager created for device: {}", deviceId_);
}

ConfigManager::~ConfigManager() {
    // 自动保存配置
    saveToFile();
    SPDLOG_DEBUG("ConfigManager destroyed for device: {}", deviceId_);
}

bool ConfigManager::defineConfig(const ConfigDefinition& definition) {
    if (definition.name.empty()) {
        SPDLOG_WARN("Cannot define config with empty name for device {}", deviceId_);
        return false;
    }

    std::lock_guard<std::mutex> lock(definitionsMutex_);
    
    auto configDef = std::make_shared<ConfigDefinition>(definition);
    definitions_[definition.name] = configDef;
    
    // 如果配置值不存在且有默认值，设置默认�?    if (!hasConfig(definition.name) && !definition.defaultValue.is_null()) {
        std::lock_guard<std::mutex> configLock(configsMutex_);
        configs_[definition.name] = definition.defaultValue;
    }
    
    SPDLOG_DEBUG("Config definition added for device {} config {}", deviceId_, definition.name);
    return true;
}

size_t ConfigManager::defineConfigs(const std::vector<ConfigDefinition>& definitions) {
    size_t successCount = 0;
    
    for (const auto& definition : definitions) {
        if (defineConfig(definition)) {
            successCount++;
        }
    }
    
    SPDLOG_DEBUG("Defined {} configs for device {}", successCount, deviceId_);
    return successCount;
}

bool ConfigManager::setConfig(const std::string& name, const json& value, bool persist) {
    if (name.empty()) {
        SPDLOG_WARN("Cannot set config with empty name for device {}", deviceId_);
        return false;
    }

    // 验证配置�?    std::string error;
    std::string error;
    if (!validateConfig(name, value, error)) {
        SPDLOG_WARN("Config validation failed for device {} config {}: {}",
                   deviceId_, name, error);
        return false;
    }

    hydrogen::device::core::json oldValue;
    bool hasOldValue = false;

    {
        std::lock_guard<std::mutex> lock(configsMutex_);
        
        // 获取旧�?        auto it = configs_.find(name);
        if (it != configs_.end()) {
            oldValue = it->second;
            hasOldValue = true;
        }
        
        // 设置新�?        configs_[name] = value;
    }

    // 触发变化通知
    if (!hasOldValue || oldValue != value) {
        notifyConfigChange(name, oldValue, value);
    }

    // 持久�?    if (persist) {
        saveToFile();
    }

    SPDLOG_DEBUG("Config {} set for device {}: {}", name, deviceId_, value.dump());
    return true;
}

size_t ConfigManager::setConfigs(const std::unordered_map<std::string, hydrogen::device::core::json>& configs, bool persist) {
    size_t successCount = 0;
    std::vector<std::pair<std::string, std::pair<hydrogen::device::core::json, hydrogen::device::core::json>>> changes;

    {
        std::lock_guard<std::mutex> lock(configsMutex_);
        
        for (const auto& [name, value] : configs) {
            if (name.empty()) {
                continue;
            }

            // 验证配置�?            std::string error;
            if (!validateConfig(name, value, error)) {
                SPDLOG_WARN("Config validation failed for device {} config {}: {}", 
                           deviceId_, name, error);
                continue;
            }

            hydrogen::device::core::json oldValue;
            bool hasOldValue = false;
            
            auto it = configs_.find(name);
            if (it != configs_.end()) {
                oldValue = it->second;
                hasOldValue = true;
            }
            
            configs_[name] = value;
            successCount++;

            // 记录变化用于通知
            if (!hasOldValue || oldValue != value) {
                changes.emplace_back(name, std::make_pair(oldValue, value));
            }
        }
    }

    // 批量触发变化通知
    for (const auto& [name, valuePair] : changes) {
        notifyConfigChange(name, valuePair.first, valuePair.second);
    }

    // 持久�?    if (persist && successCount > 0) {
        saveToFile();
    }

    SPDLOG_DEBUG("Batch set {} configs for device {}", successCount, deviceId_);
    return successCount;
}

hydrogen::device::core::json ConfigManager::getConfig(const std::string& name) const {
    std::lock_guard<std::mutex> lock(configsMutex_);
    
    auto it = configs_.find(name);
    if (it != configs_.end()) {
        return it->second;
    }
    
    // 返回默认�?    std::lock_guard<std::mutex> defLock(definitionsMutex_);
    auto defIt = definitions_.find(name);
    if (defIt != definitions_.end()) {
        return defIt->second->defaultValue;
    }
    
    return json();
}

std::unordered_map<std::string, hydrogen::device::core::json> ConfigManager::getAllConfigs() const {
    std::lock_guard<std::mutex> lock(configsMutex_);
    return configs_;
}

bool ConfigManager::hasConfig(const std::string& name) const {
    std::lock_guard<std::mutex> lock(configsMutex_);
    return configs_.find(name) != configs_.end();
}

bool ConfigManager::resetConfig(const std::string& name) {
    std::lock_guard<std::mutex> defLock(definitionsMutex_);
    auto defIt = definitions_.find(name);
    if (defIt == definitions_.end()) {
        SPDLOG_WARN("Cannot reset undefined config {} for device {}", name, deviceId_);
        return false;
    }

    return setConfig(name, defIt->second->defaultValue, true);
}

void ConfigManager::resetAllConfigs() {
    std::lock_guard<std::mutex> defLock(definitionsMutex_);
    
    for (const auto& [name, definition] : definitions_) {
        setConfig(name, definition->defaultValue, false); // 批量操作，最后统一保存
    }
    
    saveToFile();
    SPDLOG_INFO("All configs reset to defaults for device {}", deviceId_);
}

std::shared_ptr<hydrogen::device::core::ConfigDefinition> ConfigManager::getConfigDefinition(const std::string& name) const {
    std::lock_guard<std::mutex> lock(definitionsMutex_);
    
    auto it = definitions_.find(name);
    if (it != definitions_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::unordered_map<std::string, std::shared_ptr<hydrogen::device::core::ConfigDefinition>> ConfigManager::getAllConfigDefinitions() const {
    std::lock_guard<std::mutex> lock(definitionsMutex_);
    return definitions_;
}

size_t ConfigManager::addConfigChangeListener(const std::string& name, hydrogen::device::core::ConfigChangeListener listener) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    
    size_t listenerId = nextListenerId_++;
    listeners_.push_back({listenerId, name, listener});
    
    SPDLOG_DEBUG("Added config change listener {} for device {} config '{}'", 
                listenerId, deviceId_, name.empty() ? "*" : name);
    
    return listenerId;
}

void ConfigManager::removeConfigChangeListener(size_t listenerId) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    
    auto it = std::remove_if(listeners_.begin(), listeners_.end(),
        [listenerId](const ListenerInfo& info) {
            return info.id == listenerId;
        });
    
    if (it != listeners_.end()) {
        listeners_.erase(it, listeners_.end());
        SPDLOG_DEBUG("Removed config change listener {} for device {}", listenerId, deviceId_);
    }
}

bool ConfigManager::saveToFile(const std::string& filename) const {
    std::string filepath = getConfigFilePath(filename.empty() ? defaultConfigFile_ : filename);
    
    try {
        // 确保目录存在
        std::filesystem::path path(filepath);
        std::filesystem::create_directories(path.parent_path());
        
        json data = exportToJson(false);
        std::ofstream file(filepath);
        file << data.dump(2);
        file.close();
        
        SPDLOG_DEBUG("Config saved to file {} for device {}", filepath, deviceId_);
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to save config to file {} for device {}: {}", filepath, deviceId_, e.what());
        return false;
    }
}

bool ConfigManager::loadFromFile(const std::string& filename) {
    std::string filepath = getConfigFilePath(filename.empty() ? defaultConfigFile_ : filename);
    
    try {
        if (!std::filesystem::exists(filepath)) {
            SPDLOG_INFO("Config file {} does not exist for device {}, using defaults", filepath, deviceId_);
            return true;
        }
        
        std::ifstream file(filepath);
        json data;
        file >> data;
        file.close();
        
        bool result = importFromJson(data, true);
        if (result) {
            SPDLOG_INFO("Config loaded from file {} for device {}", filepath, deviceId_);
        }
        return result;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to load config from file {} for device {}: {}", filepath, deviceId_, e.what());
        return false;
    }
}

hydrogen::device::core::json ConfigManager::exportToJson(bool includeDefaults) const {
    json data;
    data["deviceId"] = deviceId_;
    data["timestamp"] = generateTimestamp();
    
    json configsJson;
    {
        std::lock_guard<std::mutex> lock(configsMutex_);
        configsJson = configs_;
    }
    
    if (includeDefaults) {
        std::lock_guard<std::mutex> defLock(definitionsMutex_);
        for (const auto& [name, definition] : definitions_) {
            if (configsJson.find(name) == configsJson.end() && !definition->defaultValue.is_null()) {
                configsJson[name] = definition->defaultValue;
            }
        }
    }
    
    data["configs"] = configsJson;
    
    // 包含预设
    {
        std::lock_guard<std::mutex> presetLock(presetsMutex_);
        if (!presets_.empty()) {
            data["presets"] = presets_;
        }
    }
    
    return data;
}

bool ConfigManager::importFromJson(const json& jsonData, bool validate) {
    try {
        if (jsonData.contains("configs") && jsonData["configs"].is_object()) {
            std::unordered_map<std::string, json> newConfigs = jsonData["configs"];
            setConfigs(newConfigs, false); // 不立即持久化
        }
        
        if (jsonData.contains("presets") && jsonData["presets"].is_object()) {
            std::lock_guard<std::mutex> lock(presetsMutex_);
            presets_ = jsonData["presets"];
        }
        
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to import config from JSON for device {}: {}", deviceId_, e.what());
        return false;
    }
}

bool ConfigManager::validateConfig(const std::string& name, const json& value, std::string& error) const {
    std::lock_guard<std::mutex> lock(definitionsMutex_);
    
    auto it = definitions_.find(name);
    if (it == definitions_.end()) {
        // 未定义的配置项，允许设置
        return true;
    }
    
    const auto& definition = it->second;
    
    // 检查只读属�?    if (definition->readOnly) {
        error = "Config is read-only";
        return false;
    }
    
    // 类型检�?    switch (definition->type) {
        case ConfigType::STRING:
            if (!value.is_string()) {
                error = "Expected string value";
                return false;
            }
            break;
        case ConfigType::INTEGER:
            if (!value.is_number_integer()) {
                error = "Expected integer value";
                return false;
            }
            break;
        case ConfigType::DOUBLE:
            if (!value.is_number()) {
                error = "Expected numeric value";
                return false;
            }
            break;
        case ConfigType::BOOLEAN:
            if (!value.is_boolean()) {
                error = "Expected boolean value";
                return false;
            }
            break;
        case ConfigType::ARRAY:
            if (!value.is_array()) {
                error = "Expected array value";
                return false;
            }
            break;
        case ConfigType::OBJECT:
            if (!value.is_object()) {
                error = "Expected object value";
                return false;
            }
            break;
    }
    
    // 范围检�?    if (value.is_number() && !definition->minValue.is_null() && value < definition->minValue) {
        error = "Value below minimum: " + definition->minValue.dump();
        return false;
    }
    
    if (value.is_number() && !definition->maxValue.is_null() && value > definition->maxValue) {
        error = "Value above maximum: " + definition->maxValue.dump();
        return false;
    }
    
    // 自定义验证器
    if (definition->validator && !definition->validator(value)) {
        error = "Custom validation failed";
        return false;
    }
    
    return true;
}

void ConfigManager::notifyConfigChange(const std::string& name, const json& oldValue, const json& newValue) {
    ConfigChangeEvent event;
    event.configName = name;
    event.oldValue = oldValue;
    event.newValue = newValue;
    event.timestamp = generateTimestamp();
    event.deviceId = deviceId_;

    std::lock_guard<std::mutex> lock(listenersMutex_);
    
    for (const auto& listenerInfo : listeners_) {
        // 监听所有配置或特定配置
        if (listenerInfo.configName.empty() || listenerInfo.configName == name) {
            try {
                listenerInfo.listener(event);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Error in config change listener for device {}: {}", deviceId_, e.what());
            }
        }
    }
}

std::string ConfigManager::generateTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return ss.str();
}

std::string ConfigManager::getConfigFilePath(const std::string& filename) const {
    if (std::filesystem::path(filename).is_absolute()) {
        return filename;
    }
    
    // 相对于设备配置目�?    std::filesystem::path configDir = "config" / deviceId_;
    return (configDir / filename).string();
}

bool ConfigManager::createPreset(const std::string& presetName, const std::string& description) {
    if (presetName.empty()) {
        SPDLOG_WARN("Cannot create preset with empty name for device {}", deviceId_);
        return false;
    }

    json presetData;
    presetData["description"] = description;
    presetData["timestamp"] = generateTimestamp();
    presetData["configs"] = getAllConfigs();

    std::lock_guard<std::mutex> lock(presetsMutex_);
    presets_[presetName] = presetData;

    SPDLOG_INFO("Created config preset '{}' for device {}", presetName, deviceId_);
    return true;
}

bool ConfigManager::applyPreset(const std::string& presetName) {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    
    auto it = presets_.find(presetName);
    if (it == presets_.end()) {
        SPDLOG_WARN("Preset '{}' not found for device {}", presetName, deviceId_);
        return false;
    }

    if (it->second.contains("configs") && it->second["configs"].is_object()) {
        std::unordered_map<std::string, json> presetConfigs = it->second["configs"];
        size_t applied = setConfigs(presetConfigs, true);
        
        SPDLOG_INFO("Applied preset '{}' ({} configs) for device {}", presetName, applied, deviceId_);
        return applied > 0;
    }

    return false;
}

std::vector<std::string> ConfigManager::getPresetNames() const {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    
    std::vector<std::string> names;
    names.reserve(presets_.size());
    
    for (const auto& [name, _] : presets_) {
        names.push_back(name);
    }
    
    return names;
}

bool ConfigManager::deletePreset(const std::string& presetName) {
    std::lock_guard<std::mutex> lock(presetsMutex_);
    
    auto it = presets_.find(presetName);
    if (it != presets_.end()) {
        presets_.erase(it);
        SPDLOG_INFO("Deleted preset '{}' for device {}", presetName, deviceId_);
        return true;
    }
    
    return false;
}

} // namespace core
} // namespace device
} // namespace hydrogen

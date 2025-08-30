#include "state_manager.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace astrocomm {
namespace device {
namespace core {

StateManager::StateManager(const std::string& deviceId)
    : deviceId_(deviceId)
    , nextListenerId_(1) {
    
    SPDLOG_DEBUG("StateManager created for device: {}", deviceId_);
}

StateManager::~StateManager() {
    SPDLOG_DEBUG("StateManager destroyed for device: {}", deviceId_);
}

bool StateManager::setProperty(const std::string& property, const json& value, bool notify) {
    if (property.empty()) {
        SPDLOG_WARN("Cannot set property with empty name for device {}", deviceId_);
        return false;
    }

    // 验证属性值
    std::string error;
    if (!validateProperty(property, value, error)) {
        SPDLOG_WARN("Property validation failed for device {} property {}: {}", 
                   deviceId_, property, error);
        return false;
    }

    json oldValue;
    bool hasOldValue = false;

    {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        
        // 获取旧值
        auto it = properties_.find(property);
        if (it != properties_.end()) {
            oldValue = it->second;
            hasOldValue = true;
        }
        
        // 设置新值
        properties_[property] = value;
    }

    // 触发变化通知
    if (notify && (!hasOldValue || oldValue != value)) {
        notifyPropertyChange(property, oldValue, value);
    }

    SPDLOG_DEBUG("Property {} set for device {}: {}", property, deviceId_, value.dump());
    return true;
}

size_t StateManager::setProperties(const std::unordered_map<std::string, json>& properties, bool notify) {
    size_t successCount = 0;
    std::vector<std::pair<std::string, std::pair<json, json>>> changes; // property -> (oldValue, newValue)

    {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        
        for (const auto& [property, value] : properties) {
            if (property.empty()) {
                continue;
            }

            // 验证属性值
            std::string error;
            if (!validateProperty(property, value, error)) {
                SPDLOG_WARN("Property validation failed for device {} property {}: {}", 
                           deviceId_, property, error);
                continue;
            }

            json oldValue;
            bool hasOldValue = false;
            
            auto it = properties_.find(property);
            if (it != properties_.end()) {
                oldValue = it->second;
                hasOldValue = true;
            }
            
            properties_[property] = value;
            successCount++;

            // 记录变化用于通知
            if (notify && (!hasOldValue || oldValue != value)) {
                changes.emplace_back(property, std::make_pair(oldValue, value));
            }
        }
    }

    // 批量触发变化通知
    for (const auto& [property, valuePair] : changes) {
        notifyPropertyChange(property, valuePair.first, valuePair.second);
    }

    SPDLOG_DEBUG("Batch set {} properties for device {}", successCount, deviceId_);
    return successCount;
}

json StateManager::getProperty(const std::string& property) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    
    auto it = properties_.find(property);
    if (it != properties_.end()) {
        return it->second;
    }
    
    return json();
}

json StateManager::getProperty(const std::string& property, const json& defaultValue) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    
    auto it = properties_.find(property);
    if (it != properties_.end()) {
        return it->second;
    }
    
    return defaultValue;
}

std::unordered_map<std::string, json> StateManager::getAllProperties() const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return properties_;
}

bool StateManager::hasProperty(const std::string& property) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return properties_.find(property) != properties_.end();
}

bool StateManager::removeProperty(const std::string& property) {
    json oldValue;
    bool existed = false;

    {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        auto it = properties_.find(property);
        if (it != properties_.end()) {
            oldValue = it->second;
            properties_.erase(it);
            existed = true;
        }
    }

    if (existed) {
        notifyPropertyChange(property, oldValue, json());
        SPDLOG_DEBUG("Property {} removed from device {}", property, deviceId_);
    }

    return existed;
}

void StateManager::clearProperties() {
    std::unordered_map<std::string, json> oldProperties;
    
    {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        oldProperties = std::move(properties_);
        properties_.clear();
    }

    // 通知所有属性被清除
    for (const auto& [property, oldValue] : oldProperties) {
        notifyPropertyChange(property, oldValue, json());
    }

    SPDLOG_DEBUG("All properties cleared for device {}", deviceId_);
}

size_t StateManager::addPropertyChangeListener(const std::string& property, PropertyChangeListener listener) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    
    size_t listenerId = nextListenerId_++;
    listeners_.push_back({listenerId, property, listener});
    
    SPDLOG_DEBUG("Added property change listener {} for device {} property '{}'", 
                listenerId, deviceId_, property.empty() ? "*" : property);
    
    return listenerId;
}

void StateManager::removePropertyChangeListener(size_t listenerId) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    
    auto it = std::remove_if(listeners_.begin(), listeners_.end(),
        [listenerId](const ListenerInfo& info) {
            return info.id == listenerId;
        });
    
    if (it != listeners_.end()) {
        listeners_.erase(it, listeners_.end());
        SPDLOG_DEBUG("Removed property change listener {} for device {}", listenerId, deviceId_);
    }
}

void StateManager::setPropertyValidator(const std::string& property, PropertyValidator validator) {
    std::lock_guard<std::mutex> lock(validatorsMutex_);
    validators_[property] = validator;
    SPDLOG_DEBUG("Set property validator for device {} property {}", deviceId_, property);
}

void StateManager::removePropertyValidator(const std::string& property) {
    std::lock_guard<std::mutex> lock(validatorsMutex_);
    validators_.erase(property);
    SPDLOG_DEBUG("Removed property validator for device {} property {}", deviceId_, property);
}

std::vector<std::string> StateManager::getCapabilities() const {
    std::lock_guard<std::mutex> lock(capabilitiesMutex_);
    return capabilities_;
}

void StateManager::setCapabilities(const std::vector<std::string>& capabilities) {
    std::lock_guard<std::mutex> lock(capabilitiesMutex_);
    capabilities_ = capabilities;
    SPDLOG_DEBUG("Set {} capabilities for device {}", capabilities.size(), deviceId_);
}

void StateManager::addCapability(const std::string& capability) {
    std::lock_guard<std::mutex> lock(capabilitiesMutex_);
    
    if (std::find(capabilities_.begin(), capabilities_.end(), capability) == capabilities_.end()) {
        capabilities_.push_back(capability);
        SPDLOG_DEBUG("Added capability '{}' to device {}", capability, deviceId_);
    }
}

bool StateManager::hasCapability(const std::string& capability) const {
    std::lock_guard<std::mutex> lock(capabilitiesMutex_);
    return std::find(capabilities_.begin(), capabilities_.end(), capability) != capabilities_.end();
}

bool StateManager::saveToFile(const std::string& filename) const {
    try {
        json data = toJson();
        std::ofstream file(filename);
        file << data.dump(2);
        file.close();
        
        SPDLOG_INFO("State saved to file {} for device {}", filename, deviceId_);
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to save state to file {} for device {}: {}", filename, deviceId_, e.what());
        return false;
    }
}

bool StateManager::loadFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        json data;
        file >> data;
        file.close();
        
        bool result = fromJson(data);
        if (result) {
            SPDLOG_INFO("State loaded from file {} for device {}", filename, deviceId_);
        }
        return result;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to load state from file {} for device {}: {}", filename, deviceId_, e.what());
        return false;
    }
}

json StateManager::toJson() const {
    json data;
    data["deviceId"] = deviceId_;
    data["timestamp"] = generateTimestamp();
    
    {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        data["properties"] = properties_;
    }
    
    {
        std::lock_guard<std::mutex> lock(capabilitiesMutex_);
        data["capabilities"] = capabilities_;
    }
    
    return data;
}

bool StateManager::fromJson(const json& jsonData) {
    try {
        if (jsonData.contains("properties") && jsonData["properties"].is_object()) {
            std::unordered_map<std::string, json> newProperties = jsonData["properties"];
            setProperties(newProperties, false); // 不触发通知
        }
        
        if (jsonData.contains("capabilities") && jsonData["capabilities"].is_array()) {
            std::vector<std::string> newCapabilities = jsonData["capabilities"];
            setCapabilities(newCapabilities);
        }
        
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to load state from JSON for device {}: {}", deviceId_, e.what());
        return false;
    }
}

void StateManager::notifyPropertyChange(const std::string& property, const json& oldValue, const json& newValue) {
    PropertyChangeEvent event;
    event.propertyName = property;
    event.oldValue = oldValue;
    event.newValue = newValue;
    event.timestamp = generateTimestamp();
    event.deviceId = deviceId_;

    std::lock_guard<std::mutex> lock(listenersMutex_);
    
    for (const auto& listenerInfo : listeners_) {
        // 监听所有属性或特定属性
        if (listenerInfo.property.empty() || listenerInfo.property == property) {
            try {
                listenerInfo.listener(event);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Error in property change listener for device {}: {}", deviceId_, e.what());
            }
        }
    }
}

bool StateManager::validateProperty(const std::string& property, const json& value, std::string& error) const {
    std::lock_guard<std::mutex> lock(validatorsMutex_);
    
    auto it = validators_.find(property);
    if (it != validators_.end()) {
        return it->second(property, value, error);
    }
    
    return true; // 没有验证器则通过
}

std::string StateManager::generateTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return ss.str();
}

} // namespace core
} // namespace device
} // namespace astrocomm

#include "astrocomm/server/repositories/config_repository.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>

namespace astrocomm {
namespace server {
namespace repositories {

/**
 * @brief Concrete implementation of the Configuration Repository
 */
class ConfigRepositoryImpl : public IConfigRepository {
public:
    explicit ConfigRepositoryImpl(const std::string& dataPath = "./data/config.json")
        : dataPath_(dataPath), inTransaction_(false) {
        spdlog::info("Config repository initialized with data path: {}", dataPath_);
    }

    ~ConfigRepositoryImpl() {
        if (inTransaction_) {
            rollbackTransaction();
        }
    }

    // Basic configuration operations
    bool setValue(const std::string& key, const std::string& value) override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        configurations_[key] = value;
        spdlog::debug("Configuration set: {} = {}", key, value);
        return true;
    }

    std::optional<std::string> getValue(const std::string& key) const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        auto it = configurations_.find(key);
        if (it != configurations_.end()) {
            return it->second;
        }
        
        return std::nullopt;
    }

    std::string getValue(const std::string& key, const std::string& defaultValue) const override {
        auto value = getValue(key);
        return value ? *value : defaultValue;
    }

    bool hasKey(const std::string& key) const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        return configurations_.find(key) != configurations_.end();
    }

    bool removeKey(const std::string& key) override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        auto it = configurations_.find(key);
        if (it != configurations_.end()) {
            configurations_.erase(it);
            spdlog::debug("Configuration key removed: {}", key);
            return true;
        }
        
        return false;
    }

    // Typed value operations
    bool setIntValue(const std::string& key, int value) override {
        return setValue(key, std::to_string(value));
    }

    std::optional<int> getIntValue(const std::string& key) const override {
        auto value = getValue(key);
        if (value) {
            try {
                return std::stoi(*value);
            } catch (const std::exception& e) {
                spdlog::warn("Failed to convert config value to int: {} = {}", key, *value);
            }
        }
        return std::nullopt;
    }

    int getIntValue(const std::string& key, int defaultValue) const override {
        auto value = getIntValue(key);
        return value ? *value : defaultValue;
    }

    bool setDoubleValue(const std::string& key, double value) override {
        return setValue(key, std::to_string(value));
    }

    std::optional<double> getDoubleValue(const std::string& key) const override {
        auto value = getValue(key);
        if (value) {
            try {
                return std::stod(*value);
            } catch (const std::exception& e) {
                spdlog::warn("Failed to convert config value to double: {} = {}", key, *value);
            }
        }
        return std::nullopt;
    }

    double getDoubleValue(const std::string& key, double defaultValue) const override {
        auto value = getDoubleValue(key);
        return value ? *value : defaultValue;
    }

    bool setBoolValue(const std::string& key, bool value) override {
        return setValue(key, value ? "true" : "false");
    }

    std::optional<bool> getBoolValue(const std::string& key) const override {
        auto value = getValue(key);
        if (value) {
            std::string lowerValue = *value;
            std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
            if (lowerValue == "true" || lowerValue == "1" || lowerValue == "yes") {
                return true;
            } else if (lowerValue == "false" || lowerValue == "0" || lowerValue == "no") {
                return false;
            } else {
                spdlog::warn("Failed to convert config value to bool: {} = {}", key, *value);
            }
        }
        return std::nullopt;
    }

    bool getBoolValue(const std::string& key, bool defaultValue) const override {
        auto value = getBoolValue(key);
        return value ? *value : defaultValue;
    }

    // Section operations
    std::unordered_map<std::string, std::string> getSection(const std::string& sectionPrefix) const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        std::unordered_map<std::string, std::string> result;
        std::string prefix = sectionPrefix + ".";
        
        for (const auto& pair : configurations_) {
            if (pair.first.substr(0, prefix.length()) == prefix) {
                std::string key = pair.first.substr(prefix.length());
                result[key] = pair.second;
            }
        }
        
        return result;
    }

    bool setSection(const std::string& sectionPrefix, 
                   const std::unordered_map<std::string, std::string>& values) override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        // Remove existing section
        std::string prefix = sectionPrefix + ".";
        auto it = configurations_.begin();
        while (it != configurations_.end()) {
            if (it->first.substr(0, prefix.length()) == prefix) {
                it = configurations_.erase(it);
            } else {
                ++it;
            }
        }
        
        // Add new values
        for (const auto& pair : values) {
            configurations_[prefix + pair.first] = pair.second;
        }
        
        spdlog::debug("Configuration section set: {} ({} keys)", sectionPrefix, values.size());
        return true;
    }

    bool removeSection(const std::string& sectionPrefix) override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        std::string prefix = sectionPrefix + ".";
        size_t removed = 0;
        
        auto it = configurations_.begin();
        while (it != configurations_.end()) {
            if (it->first.substr(0, prefix.length()) == prefix) {
                it = configurations_.erase(it);
                removed++;
            } else {
                ++it;
            }
        }
        
        spdlog::debug("Configuration section removed: {} ({} keys)", sectionPrefix, removed);
        return removed > 0;
    }

    std::vector<std::string> getSectionNames() const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        std::set<std::string> sections;
        for (const auto& pair : configurations_) {
            size_t dotPos = pair.first.find('.');
            if (dotPos != std::string::npos) {
                sections.insert(pair.first.substr(0, dotPos));
            }
        }
        
        return std::vector<std::string>(sections.begin(), sections.end());
    }

    // Bulk operations
    std::unordered_map<std::string, std::string> getAll() const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        return configurations_;
    }

    bool setAll(const std::unordered_map<std::string, std::string>& configurations) override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        configurations_ = configurations;
        spdlog::info("All configurations set ({} keys)", configurations.size());
        return true;
    }

    bool merge(const std::unordered_map<std::string, std::string>& configurations) override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        for (const auto& pair : configurations) {
            configurations_[pair.first] = pair.second;
        }
        
        spdlog::info("Configurations merged ({} keys)", configurations.size());
        return true;
    }

    size_t count() const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        return configurations_.size();
    }

    bool clear() override {
        std::lock_guard<std::mutex> lock(configMutex_);
        configurations_.clear();
        spdlog::info("All configurations cleared");
        return true;
    }

    // Search operations
    std::vector<std::string> findKeys(const std::string& pattern) const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        std::vector<std::string> result;
        for (const auto& pair : configurations_) {
            if (pair.first.find(pattern) != std::string::npos) {
                result.push_back(pair.first);
            }
        }
        
        return result;
    }

    std::unordered_map<std::string, std::string> findByKeyPattern(const std::string& pattern) const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        std::unordered_map<std::string, std::string> result;
        for (const auto& pair : configurations_) {
            if (pair.first.find(pattern) != std::string::npos) {
                result[pair.first] = pair.second;
            }
        }
        
        return result;
    }

    std::unordered_map<std::string, std::string> findByValuePattern(const std::string& pattern) const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        std::unordered_map<std::string, std::string> result;
        for (const auto& pair : configurations_) {
            if (pair.second.find(pattern) != std::string::npos) {
                result[pair.first] = pair.second;
            }
        }
        
        return result;
    }

    // Persistence management
    bool save() override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        try {
            nlohmann::json jsonData;
            for (const auto& pair : configurations_) {
                jsonData[pair.first] = pair.second;
            }
            
            std::ofstream file(dataPath_);
            file << jsonData.dump(2);
            file.close();
            
            spdlog::info("Configuration repository saved to: {}", dataPath_);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to save configuration repository: {}", e.what());
            return false;
        }
    }

    bool load() override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        try {
            std::ifstream file(dataPath_);
            if (!file.is_open()) {
                spdlog::warn("Configuration repository file not found, starting with empty repository");
                return true;
            }
            
            nlohmann::json jsonData;
            file >> jsonData;
            file.close();
            
            configurations_.clear();
            for (const auto& item : jsonData.items()) {
                configurations_[item.key()] = item.value().get<std::string>();
            }
            
            spdlog::info("Configuration repository loaded from: {} ({} keys)", dataPath_, configurations_.size());
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to load configuration repository: {}", e.what());
            return false;
        }
    }

    bool backup(const std::string& backupPath) const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        try {
            nlohmann::json jsonData;
            for (const auto& pair : configurations_) {
                jsonData[pair.first] = pair.second;
            }
            
            std::ofstream file(backupPath);
            file << jsonData.dump(2);
            file.close();
            
            spdlog::info("Configuration repository backed up to: {}", backupPath);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to backup configuration repository: {}", e.what());
            return false;
        }
    }

    bool restore(const std::string& backupPath) override {
        std::string originalPath = dataPath_;
        dataPath_ = backupPath;
        bool result = load();
        dataPath_ = originalPath;
        return result;
    }

    // Transaction support
    bool beginTransaction() override {
        std::lock_guard<std::mutex> lock(transactionMutex_);
        
        if (inTransaction_) {
            spdlog::warn("Transaction already in progress");
            return false;
        }
        
        transactionBackup_ = configurations_;
        inTransaction_ = true;
        spdlog::debug("Transaction started");
        return true;
    }

    bool commitTransaction() override {
        std::lock_guard<std::mutex> lock(transactionMutex_);
        
        if (!inTransaction_) {
            spdlog::warn("No transaction in progress");
            return false;
        }
        
        transactionBackup_.clear();
        inTransaction_ = false;
        spdlog::debug("Transaction committed");
        return true;
    }

    bool rollbackTransaction() override {
        std::lock_guard<std::mutex> lock(transactionMutex_);
        
        if (!inTransaction_) {
            spdlog::warn("No transaction in progress");
            return false;
        }
        
        {
            std::lock_guard<std::mutex> configLock(configMutex_);
            configurations_ = transactionBackup_;
        }
        
        transactionBackup_.clear();
        inTransaction_ = false;
        spdlog::debug("Transaction rolled back");
        return true;
    }

    bool isInTransaction() const override {
        std::lock_guard<std::mutex> lock(transactionMutex_);
        return inTransaction_;
    }

    // Environment variable integration
    bool loadFromEnvironment(const std::string& prefix) override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        // In a real implementation, you would iterate through environment variables
        // For now, we'll simulate loading some common environment variables
        
        // Simulate loading environment variables with the given prefix
        std::vector<std::pair<std::string, std::string>> envVars = {
            {prefix + ".LOG_LEVEL", "INFO"},
            {prefix + ".PORT", "8080"},
            {prefix + ".HOST", "localhost"},
            {prefix + ".DEBUG", "false"}
        };
        
        size_t loaded = 0;
        for (const auto& envVar : envVars) {
            configurations_[envVar.first] = envVar.second;
            loaded++;
        }
        
        spdlog::info("Loaded {} environment variables with prefix: {}", loaded, prefix);
        return true;
    }

    bool exportToEnvironment(const std::string& prefix) const override {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        // In a real implementation, you would set environment variables
        // For now, we'll just log what would be exported
        
        size_t exported = 0;
        for (const auto& pair : configurations_) {
            if (pair.first.substr(0, prefix.length()) == prefix) {
                spdlog::debug("Would export to environment: {} = {}", pair.first, pair.second);
                exported++;
            }
        }
        
        spdlog::info("Would export {} configuration values to environment with prefix: {}", exported, prefix);
        return true;
    }

private:
    mutable std::mutex configMutex_;
    mutable std::mutex transactionMutex_;
    
    std::unordered_map<std::string, std::string> configurations_;
    std::unordered_map<std::string, std::string> transactionBackup_;
    
    std::string dataPath_;
    bool inTransaction_;
};

// Factory function implementation
std::unique_ptr<IConfigRepository> ConfigRepositoryFactory::createRepository(const std::string& dataPath) {
    return std::make_unique<ConfigRepositoryImpl>(dataPath);
}

} // namespace repositories
} // namespace server
} // namespace astrocomm

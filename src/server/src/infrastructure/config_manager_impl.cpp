#include "astrocomm/server/infrastructure/config_manager.h"
#include "astrocomm/server/repositories/config_repository.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <filesystem>

namespace astrocomm {
namespace server {
namespace infrastructure {

/**
 * @brief Concrete implementation of the Configuration Manager
 */
class ConfigManagerImpl : public IConfigManager {
public:
    explicit ConfigManagerImpl(std::unique_ptr<repositories::IConfigRepository> repository)
        : repository_(std::move(repository)), initialized_(false) {
        spdlog::info("Configuration manager initialized");
    }

    ~ConfigManagerImpl() = default;

    // Initialization and lifecycle
    bool initialize() override {
        if (initialized_) {
            spdlog::warn("Configuration manager already initialized");
            return true;
        }

        try {
            // Load configuration from repository
            if (!repository_->load()) {
                spdlog::error("Failed to load configuration from repository");
                return false;
            }

            // Load default configurations
            loadDefaultConfigurations();

            // Load environment variables
            loadEnvironmentVariables();

            // Validate critical configurations
            if (!validateCriticalConfigurations()) {
                spdlog::error("Critical configuration validation failed");
                return false;
            }

            initialized_ = true;
            spdlog::info("Configuration manager initialized successfully");
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to initialize configuration manager: {}", e.what());
            return false;
        }
    }

    bool shutdown() override {
        if (!initialized_) {
            return true;
        }

        try {
            // Save current configuration
            if (!repository_->save()) {
                spdlog::warn("Failed to save configuration during shutdown");
            }

            initialized_ = false;
            spdlog::info("Configuration manager shut down");
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Error during configuration manager shutdown: {}", e.what());
            return false;
        }
    }

    bool isInitialized() const override {
        return initialized_;
    }

    // Configuration access
    std::string getString(const std::string& key, const std::string& defaultValue) const override {
        if (!initialized_) {
            spdlog::warn("Configuration manager not initialized, returning default value for: {}", key);
            return defaultValue;
        }

        return repository_->getValue(key, defaultValue);
    }

    int getInt(const std::string& key, int defaultValue) const override {
        if (!initialized_) {
            spdlog::warn("Configuration manager not initialized, returning default value for: {}", key);
            return defaultValue;
        }

        return repository_->getIntValue(key, defaultValue);
    }

    double getDouble(const std::string& key, double defaultValue) const override {
        if (!initialized_) {
            spdlog::warn("Configuration manager not initialized, returning default value for: {}", key);
            return defaultValue;
        }

        return repository_->getDoubleValue(key, defaultValue);
    }

    bool getBool(const std::string& key, bool defaultValue) const override {
        if (!initialized_) {
            spdlog::warn("Configuration manager not initialized, returning default value for: {}", key);
            return defaultValue;
        }

        return repository_->getBoolValue(key, defaultValue);
    }

    // Configuration modification
    bool setString(const std::string& key, const std::string& value) override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        bool result = repository_->setValue(key, value);
        if (result) {
            notifyConfigurationChange(key, value);
        }
        return result;
    }

    bool setInt(const std::string& key, int value) override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        bool result = repository_->setIntValue(key, value);
        if (result) {
            notifyConfigurationChange(key, std::to_string(value));
        }
        return result;
    }

    bool setDouble(const std::string& key, double value) override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        bool result = repository_->setDoubleValue(key, value);
        if (result) {
            notifyConfigurationChange(key, std::to_string(value));
        }
        return result;
    }

    bool setBool(const std::string& key, bool value) override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        bool result = repository_->setBoolValue(key, value);
        if (result) {
            notifyConfigurationChange(key, value ? "true" : "false");
        }
        return result;
    }

    bool removeKey(const std::string& key) override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        bool result = repository_->removeKey(key);
        if (result) {
            notifyConfigurationChange(key, "");
        }
        return result;
    }

    bool hasKey(const std::string& key) const override {
        if (!initialized_) {
            return false;
        }

        return repository_->hasKey(key);
    }

    // Section operations
    std::unordered_map<std::string, std::string> getSection(const std::string& sectionName) const override {
        if (!initialized_) {
            spdlog::warn("Configuration manager not initialized");
            return {};
        }

        return repository_->getSection(sectionName);
    }

    bool setSection(const std::string& sectionName, 
                   const std::unordered_map<std::string, std::string>& values) override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        bool result = repository_->setSection(sectionName, values);
        if (result) {
            for (const auto& pair : values) {
                notifyConfigurationChange(sectionName + "." + pair.first, pair.second);
            }
        }
        return result;
    }

    bool removeSection(const std::string& sectionName) override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        return repository_->removeSection(sectionName);
    }

    std::vector<std::string> getSectionNames() const override {
        if (!initialized_) {
            return {};
        }

        return repository_->getSectionNames();
    }

    // File operations
    bool loadFromFile(const std::string& filePath) override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        try {
            if (!std::filesystem::exists(filePath)) {
                spdlog::error("Configuration file not found: {}", filePath);
                return false;
            }

            std::ifstream file(filePath);
            if (!file.is_open()) {
                spdlog::error("Failed to open configuration file: {}", filePath);
                return false;
            }

            nlohmann::json jsonData;
            file >> jsonData;
            file.close();

            std::unordered_map<std::string, std::string> configurations;
            for (const auto& item : jsonData.items()) {
                configurations[item.key()] = item.value().get<std::string>();
            }

            bool result = repository_->merge(configurations);
            if (result) {
                spdlog::info("Configuration loaded from file: {} ({} keys)", filePath, configurations.size());
            }
            return result;
        } catch (const std::exception& e) {
            spdlog::error("Failed to load configuration from file {}: {}", filePath, e.what());
            return false;
        }
    }

    bool saveToFile(const std::string& filePath) const override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        try {
            auto configurations = repository_->getAll();
            
            nlohmann::json jsonData;
            for (const auto& pair : configurations) {
                jsonData[pair.first] = pair.second;
            }

            std::ofstream file(filePath);
            if (!file.is_open()) {
                spdlog::error("Failed to create configuration file: {}", filePath);
                return false;
            }

            file << jsonData.dump(2);
            file.close();

            spdlog::info("Configuration saved to file: {} ({} keys)", filePath, configurations.size());
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to save configuration to file {}: {}", filePath, e.what());
            return false;
        }
    }

    bool backup(const std::string& backupPath) const override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        return repository_->backup(backupPath);
    }

    bool restore(const std::string& backupPath) override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return false;
        }

        return repository_->restore(backupPath);
    }

    // Change notification
    void registerChangeListener(const std::string& key, ConfigChangeCallback callback) override {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        changeListeners_[key].push_back(callback);
        spdlog::debug("Configuration change listener registered for key: {}", key);
    }

    void unregisterChangeListener(const std::string& key) override {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        auto it = changeListeners_.find(key);
        if (it != changeListeners_.end()) {
            changeListeners_.erase(it);
            spdlog::debug("Configuration change listeners unregistered for key: {}", key);
        }
    }

    // Validation
    bool validateConfiguration() const override {
        if (!initialized_) {
            return false;
        }

        return validateCriticalConfigurations();
    }

    std::vector<std::string> getValidationErrors() const override {
        std::vector<std::string> errors;

        if (!initialized_) {
            errors.push_back("Configuration manager not initialized");
            return errors;
        }

        // Check critical configurations
        std::vector<std::string> criticalKeys = {
            "server.port",
            "server.host",
            "logging.level",
            "database.path"
        };

        for (const auto& key : criticalKeys) {
            if (!repository_->hasKey(key)) {
                errors.push_back("Missing critical configuration: " + key);
            }
        }

        // Validate port range
        int port = repository_->getIntValue("server.port", 0);
        if (port <= 0 || port > 65535) {
            errors.push_back("Invalid server port: " + std::to_string(port));
        }

        return errors;
    }

    // Statistics and monitoring
    size_t getConfigurationCount() const override {
        if (!initialized_) {
            return 0;
        }

        return repository_->count();
    }

    std::unordered_map<std::string, std::string> getAllConfigurations() const override {
        if (!initialized_) {
            return {};
        }

        return repository_->getAll();
    }

    void clearAllConfigurations() override {
        if (!initialized_) {
            spdlog::error("Configuration manager not initialized");
            return;
        }

        repository_->clear();
        spdlog::warn("All configurations cleared");
    }

private:
    std::unique_ptr<repositories::IConfigRepository> repository_;
    std::atomic<bool> initialized_;
    
    mutable std::mutex listenersMutex_;
    std::unordered_map<std::string, std::vector<ConfigChangeCallback>> changeListeners_;

    void loadDefaultConfigurations() {
        // Set default configurations
        std::unordered_map<std::string, std::string> defaults = {
            {"server.host", "localhost"},
            {"server.port", "8080"},
            {"server.threads", "4"},
            {"logging.level", "INFO"},
            {"logging.file", "astrocomm.log"},
            {"database.path", "./data"},
            {"security.enable_ssl", "false"},
            {"security.session_timeout", "3600"},
            {"communication.timeout", "30"},
            {"communication.retry_count", "3"},
            {"device.scan_interval", "60"},
            {"device.connection_timeout", "10"}
        };

        for (const auto& pair : defaults) {
            if (!repository_->hasKey(pair.first)) {
                repository_->setValue(pair.first, pair.second);
            }
        }

        spdlog::debug("Default configurations loaded");
    }

    void loadEnvironmentVariables() {
        // Load environment variables with ASTROCOMM_ prefix
        repository_->loadFromEnvironment("ASTROCOMM");
        spdlog::debug("Environment variables loaded");
    }

    bool validateCriticalConfigurations() const {
        auto errors = getValidationErrors();
        if (!errors.empty()) {
            for (const auto& error : errors) {
                spdlog::error("Configuration validation error: {}", error);
            }
            return false;
        }
        return true;
    }

    void notifyConfigurationChange(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        
        auto it = changeListeners_.find(key);
        if (it != changeListeners_.end()) {
            for (const auto& callback : it->second) {
                try {
                    callback(key, value);
                } catch (const std::exception& e) {
                    spdlog::error("Error in configuration change callback for key {}: {}", key, e.what());
                }
            }
        }
    }
};

// Factory function implementation
std::unique_ptr<IConfigManager> ConfigManagerFactory::createManager(
    std::unique_ptr<repositories::IConfigRepository> repository) {
    return std::make_unique<ConfigManagerImpl>(std::move(repository));
}

} // namespace infrastructure
} // namespace server
} // namespace astrocomm

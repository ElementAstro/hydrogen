#pragma once

#include "../repositories/config_repository.h"
#include "../core/service_registry.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>

namespace hydrogen {
namespace server {
namespace infrastructure {

/**
 * @brief Configuration source enumeration
 */
enum class ConfigSource {
    FILE,           // Configuration file (JSON, YAML, INI, etc.)
    ENVIRONMENT,    // Environment variables
    COMMAND_LINE,   // Command line arguments
    DATABASE,       // Database storage
    REMOTE,         // Remote configuration service
    MEMORY          // In-memory configuration
};

/**
 * @brief Configuration priority levels
 */
enum class ConfigPriority {
    LOWEST = 0,
    LOW = 1,
    NORMAL = 2,
    HIGH = 3,
    HIGHEST = 4
};

/**
 * @brief Configuration source information
 */
struct ConfigSourceInfo {
    ConfigSource source;
    std::string location;
    ConfigPriority priority;
    bool isReadOnly;
    bool isWatched;
    std::chrono::system_clock::time_point lastLoaded;
    std::string format;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Configuration manager interface
 * 
 * Provides centralized configuration management with support for multiple sources,
 * hierarchical configurations, hot reloading, and validation.
 */
class IConfigManager : public virtual core::IService {
public:
    virtual ~IConfigManager() = default;

    // Configuration source management
    virtual bool addSource(const ConfigSourceInfo& sourceInfo) = 0;
    virtual bool removeSource(ConfigSource source, const std::string& location = "") = 0;
    virtual std::vector<ConfigSourceInfo> getSources() const = 0;
    virtual bool reloadSource(ConfigSource source, const std::string& location = "") = 0;
    virtual bool reloadAllSources() = 0;

    // Basic configuration operations
    virtual bool set(const std::string& key, const std::string& value, 
                    const std::string& category = "default") = 0;
    virtual std::string get(const std::string& key, const std::string& defaultValue = "",
                          const std::string& category = "default") const = 0;
    virtual bool remove(const std::string& key, const std::string& category = "default") = 0;
    virtual bool exists(const std::string& key, const std::string& category = "default") const = 0;

    // Typed configuration operations
    virtual bool setInt(const std::string& key, int value, const std::string& category = "default") = 0;
    virtual int getInt(const std::string& key, int defaultValue = 0, const std::string& category = "default") const = 0;
    
    virtual bool setDouble(const std::string& key, double value, const std::string& category = "default") = 0;
    virtual double getDouble(const std::string& key, double defaultValue = 0.0, const std::string& category = "default") const = 0;
    
    virtual bool setBool(const std::string& key, bool value, const std::string& category = "default") = 0;
    virtual bool getBool(const std::string& key, bool defaultValue = false, const std::string& category = "default") const = 0;

    // Array and object operations
    virtual std::vector<std::string> getArray(const std::string& key, const std::string& category = "default") const = 0;
    virtual bool setArray(const std::string& key, const std::vector<std::string>& values, const std::string& category = "default") = 0;
    virtual std::unordered_map<std::string, std::string> getObject(const std::string& key, const std::string& category = "default") const = 0;
    virtual bool setObject(const std::string& key, const std::unordered_map<std::string, std::string>& object, const std::string& category = "default") = 0;

    // Hierarchical configuration
    virtual std::string getHierarchical(const std::string& keyPath, const std::string& defaultValue = "") const = 0;
    virtual bool setHierarchical(const std::string& keyPath, const std::string& value) = 0;
    virtual std::unordered_map<std::string, std::string> getSection(const std::string& sectionPath) const = 0;

    // Environment and profile support
    virtual bool setEnvironment(const std::string& environment) = 0;
    virtual std::string getCurrentEnvironment() const = 0;
    virtual bool loadProfile(const std::string& profileName) = 0;
    virtual std::string getCurrentProfile() const = 0;

    // Template operations
    virtual bool applyTemplate(const std::string& templateName, const std::unordered_map<std::string, std::string>& variables = {}) = 0;
    virtual bool saveAsTemplate(const std::string& templateName, const std::string& category = "") = 0;
    virtual std::vector<std::string> getAvailableTemplates() const = 0;

    // Validation
    virtual bool validate() const = 0;
    virtual std::vector<std::string> getValidationErrors() const = 0;
    virtual bool addValidator(const std::string& key, std::function<bool(const std::string&)> validator) = 0;
    virtual bool removeValidator(const std::string& key) = 0;

    // Hot reloading and watching
    virtual bool enableHotReload(bool enabled) = 0;
    virtual bool isHotReloadEnabled() const = 0;
    virtual bool watchFile(const std::string& filePath) = 0;
    virtual bool unwatchFile(const std::string& filePath) = 0;
    virtual std::vector<std::string> getWatchedFiles() const = 0;

    // Encryption and security
    virtual bool encryptValue(const std::string& key, const std::string& category = "default") = 0;
    virtual bool decryptValue(const std::string& key, const std::string& category = "default") = 0;
    virtual bool isValueEncrypted(const std::string& key, const std::string& category = "default") const = 0;
    virtual bool setEncryptionKey(const std::string& encryptionKey) = 0;

    // Import/Export
    virtual bool exportConfig(const std::string& filePath, const std::string& format = "json", 
                            const std::string& category = "") const = 0;
    virtual bool importConfig(const std::string& filePath, const std::string& format = "json",
                            const std::string& category = "") = 0;

    // Backup and restore
    virtual bool backup(const std::string& backupPath) const = 0;
    virtual bool restore(const std::string& backupPath) = 0;
    virtual std::vector<std::string> getAvailableBackups() const = 0;

    // Statistics and monitoring
    virtual size_t getConfigCount(const std::string& category = "") const = 0;
    virtual std::vector<std::string> getCategories() const = 0;
    virtual std::unordered_map<std::string, size_t> getCategoryStatistics() const = 0;
    virtual std::chrono::system_clock::time_point getLastModified() const = 0;

    // Event callbacks
    using ConfigChangeCallback = std::function<void(const std::string& key, const std::string& oldValue, 
                                                   const std::string& newValue, const std::string& category)>;
    using ConfigReloadCallback = std::function<void(ConfigSource source, const std::string& location, bool success)>;
    using ConfigErrorCallback = std::function<void(const std::string& error, const std::string& context)>;

    virtual void setChangeCallback(ConfigChangeCallback callback) = 0;
    virtual void setReloadCallback(ConfigReloadCallback callback) = 0;
    virtual void setErrorCallback(ConfigErrorCallback callback) = 0;

    // Repository integration
    virtual void setRepository(std::shared_ptr<repositories::IConfigRepository> repository) = 0;
    virtual std::shared_ptr<repositories::IConfigRepository> getRepository() const = 0;

    // Utility methods
    virtual std::string expandVariables(const std::string& value) const = 0;
    virtual bool hasRequiredConfigs() const = 0;
    virtual std::vector<std::string> getMissingRequiredConfigs() const = 0;
    virtual std::string getConfigSummary() const = 0;
};

/**
 * @brief Configuration manager factory
 */
class ConfigManagerFactory : public core::IServiceFactory {
public:
    std::unique_ptr<core::IService> createService(
        const std::string& serviceName,
        const std::unordered_map<std::string, std::string>& config = {}
    ) override;
    
    std::vector<std::string> getSupportedServices() const override;
    bool isServiceSupported(const std::string& serviceName) const override;
};

/**
 * @brief Default configuration manager implementation
 * 
 * Provides a concrete implementation of the configuration manager interface
 * with support for multiple sources and hierarchical configurations.
 */
class ConfigManager : public core::BaseService, public virtual IConfigManager {
public:
    explicit ConfigManager(const std::string& name = "ConfigManager");
    virtual ~ConfigManager() = default;

    // IService implementation
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool shutdown() override;
    std::vector<core::ServiceDependency> getDependencies() const override;
    bool areDependenciesSatisfied() const override;

    // IConfigManager implementation
    bool addSource(const ConfigSourceInfo& sourceInfo) override;
    bool removeSource(ConfigSource source, const std::string& location = "") override;
    std::vector<ConfigSourceInfo> getSources() const override;
    bool reloadSource(ConfigSource source, const std::string& location = "") override;
    bool reloadAllSources() override;

    bool set(const std::string& key, const std::string& value, const std::string& category = "default") override;
    std::string get(const std::string& key, const std::string& defaultValue = "", const std::string& category = "default") const override;
    bool remove(const std::string& key, const std::string& category = "default") override;
    bool exists(const std::string& key, const std::string& category = "default") const override;

    // Additional implementation methods...

private:
    mutable std::mutex configMutex_;
    std::shared_ptr<repositories::IConfigRepository> repository_;
    std::vector<ConfigSourceInfo> sources_;
    std::unordered_map<std::string, std::function<bool(const std::string&)>> validators_;
    
    std::atomic<bool> hotReloadEnabled_{false};
    std::vector<std::string> watchedFiles_;
    
    ConfigChangeCallback changeCallback_;
    ConfigReloadCallback reloadCallback_;
    ConfigErrorCallback errorCallback_;
    
    std::string currentEnvironment_{"default"};
    std::string currentProfile_{"default"};
    
    // Helper methods
    bool loadFromSource(const ConfigSourceInfo& sourceInfo);
    bool validateConfiguration() const;
    void notifyChange(const std::string& key, const std::string& oldValue, 
                     const std::string& newValue, const std::string& category);
    std::string resolveKey(const std::string& key, const std::string& category) const;
};

} // namespace infrastructure
} // namespace server
} // namespace hydrogen

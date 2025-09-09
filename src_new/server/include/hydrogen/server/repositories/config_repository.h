#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>
#include <chrono>
#include <any>

namespace hydrogen {
namespace server {
namespace repositories {

/**
 * @brief Configuration value type enumeration
 */
enum class ConfigValueType {
    STRING,
    INTEGER,
    DOUBLE,
    BOOLEAN,
    JSON,
    ARRAY,
    OBJECT
};

/**
 * @brief Configuration entry structure
 */
struct ConfigEntry {
    std::string key;
    std::string value;
    ConfigValueType type;
    std::string description;
    std::string category;
    bool isRequired;
    bool isSecret;
    std::string defaultValue;
    std::vector<std::string> allowedValues;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point modifiedAt;
    std::string modifiedBy;
};

/**
 * @brief Configuration template structure
 */
struct ConfigTemplate {
    std::string templateId;
    std::string name;
    std::string description;
    std::string category;
    std::vector<ConfigEntry> entries;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point modifiedAt;
    std::string version;
};

/**
 * @brief Configuration change record
 */
struct ConfigChange {
    std::string changeId;
    std::string key;
    std::string oldValue;
    std::string newValue;
    std::string changedBy;
    std::string reason;
    std::chrono::system_clock::time_point timestamp;
    std::string category;
};

/**
 * @brief Configuration repository interface
 * 
 * Provides data access layer for configuration management,
 * supporting hierarchical configurations, templates, and change tracking.
 */
class IConfigRepository {
public:
    virtual ~IConfigRepository() = default;

    // Basic configuration operations
    virtual bool set(const std::string& key, const std::string& value, 
                    const std::string& category = "default") = 0;
    virtual std::optional<std::string> get(const std::string& key, 
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

    // Bulk operations
    virtual bool setBulk(const std::unordered_map<std::string, std::string>& configs, 
                        const std::string& category = "default") = 0;
    virtual std::unordered_map<std::string, std::string> getBulk(const std::vector<std::string>& keys,
                                                               const std::string& category = "default") const = 0;
    virtual bool removeBulk(const std::vector<std::string>& keys, const std::string& category = "default") = 0;

    // Category operations
    virtual std::unordered_map<std::string, std::string> getCategory(const std::string& category) const = 0;
    virtual std::vector<std::string> getCategories() const = 0;
    virtual bool removeCategory(const std::string& category) = 0;
    virtual bool categoryExists(const std::string& category) const = 0;

    // Configuration entry operations
    virtual bool setEntry(const ConfigEntry& entry) = 0;
    virtual std::optional<ConfigEntry> getEntry(const std::string& key, const std::string& category = "default") const = 0;
    virtual std::vector<ConfigEntry> getAllEntries(const std::string& category = "") const = 0;
    virtual bool updateEntryMetadata(const std::string& key, const std::string& category,
                                   const std::string& description, bool isRequired = false, bool isSecret = false) = 0;

    // Search and filtering
    virtual std::vector<ConfigEntry> findByPattern(const std::string& pattern, const std::string& category = "") const = 0;
    virtual std::vector<ConfigEntry> findByType(ConfigValueType type, const std::string& category = "") const = 0;
    virtual std::vector<ConfigEntry> findRequired(const std::string& category = "") const = 0;
    virtual std::vector<ConfigEntry> findSecrets(const std::string& category = "") const = 0;

    // Template operations
    virtual bool saveTemplate(const ConfigTemplate& template_) = 0;
    virtual std::optional<ConfigTemplate> loadTemplate(const std::string& templateId) const = 0;
    virtual bool removeTemplate(const std::string& templateId) = 0;
    virtual std::vector<ConfigTemplate> getAllTemplates() const = 0;
    virtual bool applyTemplate(const std::string& templateId, const std::string& targetCategory) = 0;

    // Validation operations
    virtual bool validate() const = 0;
    virtual std::vector<std::string> getValidationErrors() const = 0;
    virtual bool validateEntry(const ConfigEntry& entry) const = 0;
    virtual bool validateCategory(const std::string& category) const = 0;

    // Change tracking
    virtual bool enableChangeTracking(bool enabled) = 0;
    virtual bool isChangeTrackingEnabled() const = 0;
    virtual std::vector<ConfigChange> getChangeHistory(const std::string& key = "", size_t limit = 100) const = 0;
    virtual bool recordChange(const std::string& key, const std::string& oldValue, 
                            const std::string& newValue, const std::string& changedBy, 
                            const std::string& reason = "") = 0;

    // Backup and restore operations
    virtual bool backup(const std::string& backupPath, const std::string& category = "") const = 0;
    virtual bool restore(const std::string& backupPath, const std::string& category = "") = 0;
    virtual bool export_(const std::string& filePath, const std::string& format = "json", 
                        const std::string& category = "") const = 0;
    virtual bool import(const std::string& filePath, const std::string& format = "json",
                       const std::string& category = "") = 0;

    // Environment and profile support
    virtual bool setEnvironment(const std::string& environment) = 0;
    virtual std::string getCurrentEnvironment() const = 0;
    virtual std::vector<std::string> getAvailableEnvironments() const = 0;
    virtual bool createEnvironment(const std::string& environment, const std::string& baseEnvironment = "") = 0;
    virtual bool removeEnvironment(const std::string& environment) = 0;

    // Configuration profiles
    virtual bool saveProfile(const std::string& profileName, const std::string& category = "") = 0;
    virtual bool loadProfile(const std::string& profileName, const std::string& category = "") = 0;
    virtual bool removeProfile(const std::string& profileName) = 0;
    virtual std::vector<std::string> getAvailableProfiles() const = 0;

    // Encryption and security
    virtual bool encryptSecrets(bool enabled) = 0;
    virtual bool isSecretsEncrypted() const = 0;
    virtual bool setEncryptionKey(const std::string& key) = 0;
    virtual bool rotateEncryptionKey(const std::string& newKey) = 0;

    // Persistence management
    virtual bool save() = 0;
    virtual bool load() = 0;
    virtual bool reload() = 0;
    virtual bool isModified() const = 0;
    virtual std::chrono::system_clock::time_point getLastModified() const = 0;

    // Statistics
    virtual size_t getConfigCount(const std::string& category = "") const = 0;
    virtual size_t getCategoryCount() const = 0;
    virtual size_t getSecretCount(const std::string& category = "") const = 0;
    virtual std::unordered_map<std::string, size_t> getCategoryStatistics() const = 0;

    // Event callbacks
    using ConfigChangeCallback = std::function<void(const std::string& key, const std::string& oldValue, 
                                                   const std::string& newValue, const std::string& category)>;
    using ConfigValidationCallback = std::function<bool(const std::string& key, const std::string& value, 
                                                       const std::string& category)>;

    virtual void setChangeCallback(ConfigChangeCallback callback) = 0;
    virtual void setValidationCallback(ConfigValidationCallback callback) = 0;

    // Watch operations
    virtual bool watchKey(const std::string& key, const std::string& category = "default") = 0;
    virtual bool unwatchKey(const std::string& key, const std::string& category = "default") = 0;
    virtual bool watchCategory(const std::string& category) = 0;
    virtual bool unwatchCategory(const std::string& category) = 0;
    virtual std::vector<std::string> getWatchedKeys() const = 0;
    virtual std::vector<std::string> getWatchedCategories() const = 0;
};

/**
 * @brief Configuration repository factory interface
 */
class IConfigRepositoryFactory {
public:
    virtual ~IConfigRepositoryFactory() = default;
    
    virtual std::unique_ptr<IConfigRepository> createRepository(const std::string& type,
                                                               const std::unordered_map<std::string, std::string>& config) = 0;
    virtual std::vector<std::string> getSupportedTypes() const = 0;
    virtual bool isTypeSupported(const std::string& type) const = 0;
};

} // namespace repositories
} // namespace server
} // namespace hydrogen

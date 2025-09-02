#pragma once

#include "message.h"
#include "message_transformer.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Configuration validation result
 */
struct ConfigValidationResult {
    bool isValid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    void addError(const std::string& error) {
        errors.push_back(error);
        isValid = false;
    }
    
    void addWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
    
    std::string toString() const;
};

/**
 * @brief Base configuration interface
 */
class ConfigurationBase {
public:
    virtual ~ConfigurationBase() = default;
    
    // Serialization
    virtual json toJson() const = 0;
    virtual void fromJson(const json& j) = 0;
    
    // Validation
    virtual ConfigValidationResult validate() const = 0;
    
    // Configuration management
    virtual std::string getConfigType() const = 0;
    virtual std::string getVersion() const { return "1.0.0"; }
};

/**
 * @brief Network configuration settings
 */
struct NetworkConfig {
    std::string host = "localhost";
    uint16_t port = 8080;
    std::string endpoint = "/ws";
    bool useTls = false;
    std::string tlsCertPath;
    std::string tlsKeyPath;
    std::string tlsCaPath;
    bool verifyTlsCertificate = true;
    
    // Timeouts
    std::chrono::milliseconds connectTimeout{5000};
    std::chrono::milliseconds readTimeout{30000};
    std::chrono::milliseconds writeTimeout{30000};
    std::chrono::milliseconds keepAliveInterval{30000};
    
    // Connection limits
    int maxConnections = 100;
    int maxReconnectAttempts = 0; // 0 = unlimited
    std::chrono::milliseconds reconnectInterval{5000};
    
    // Protocol-specific settings
    json protocolSettings;
    
    json toJson() const;
    void fromJson(const json& j);
    ConfigValidationResult validate() const;
};

/**
 * @brief Authentication configuration
 */
struct AuthConfig {
    enum class AuthType {
        NONE,
        BASIC,
        BEARER_TOKEN,
        API_KEY,
        OAUTH2,
        CERTIFICATE,
        CUSTOM
    };
    
    AuthType type = AuthType::NONE;
    std::string username;
    std::string password;
    std::string token;
    std::string apiKey;
    std::string apiKeyHeader = "X-API-Key";
    
    // OAuth2 settings
    std::string clientId;
    std::string clientSecret;
    std::string authUrl;
    std::string tokenUrl;
    std::vector<std::string> scopes;
    
    // Certificate settings
    std::string certPath;
    std::string keyPath;
    std::string keyPassword;
    
    // Custom auth settings
    json customSettings;
    
    json toJson() const;
    void fromJson(const json& j);
    ConfigValidationResult validate() const;
};

/**
 * @brief Logging configuration
 */
struct LoggingConfig {
    enum class LogLevel {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERR,  // Renamed from ERROR to avoid Windows macro conflict
        CRITICAL,
        OFF
    };
    
    LogLevel level = LogLevel::INFO;
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] %v";
    bool enableConsole = true;
    bool enableFile = false;
    std::string logFile = "hydrogen_client.log";
    size_t maxFileSize = 10 * 1024 * 1024; // 10MB
    size_t maxFiles = 5;
    bool enableRotation = true;
    
    json toJson() const;
    void fromJson(const json& j);
    ConfigValidationResult validate() const;
};

/**
 * @brief Performance and resource configuration
 */
struct PerformanceConfig {
    // Threading
    size_t workerThreads = 0; // 0 = auto-detect
    size_t ioThreads = 1;
    
    // Memory management
    size_t maxMessageQueueSize = 1000;
    size_t maxCacheSize = 100 * 1024 * 1024; // 100MB
    std::chrono::milliseconds cacheExpiry{300000}; // 5 minutes
    
    // Rate limiting
    size_t maxRequestsPerSecond = 0; // 0 = unlimited
    size_t burstSize = 10;
    
    // Compression
    bool enableCompression = false;
    std::string compressionAlgorithm = "gzip";
    int compressionLevel = 6;
    
    json toJson() const;
    void fromJson(const json& j);
    ConfigValidationResult validate() const;
};

/**
 * @brief Device discovery configuration
 */
struct DiscoveryConfig {
    bool enableAutoDiscovery = true;
    std::chrono::milliseconds discoveryInterval{30000};
    std::vector<std::string> deviceTypes;
    std::vector<std::string> excludeDevices;
    json discoveryFilters;
    
    // Network discovery
    bool enableNetworkScan = false;
    std::vector<std::string> scanRanges;
    std::vector<uint16_t> scanPorts;
    
    json toJson() const;
    void fromJson(const json& j);
    ConfigValidationResult validate() const;
};

/**
 * @brief Unified client configuration
 */
class ClientConfiguration : public ConfigurationBase {
public:
    ClientConfiguration();
    ~ClientConfiguration() override = default;
    
    // Configuration sections
    NetworkConfig network;
    AuthConfig authentication;
    LoggingConfig logging;
    PerformanceConfig performance;
    DiscoveryConfig discovery;
    
    // Protocol settings
    MessageFormat defaultProtocol = MessageFormat::HTTP_JSON;
    std::unordered_map<MessageFormat, json> protocolConfigs;
    
    // Feature flags
    std::unordered_map<std::string, bool> features;
    
    // Custom settings
    json customSettings;
    
    // ConfigurationBase interface
    json toJson() const override;
    void fromJson(const json& j) override;
    ConfigValidationResult validate() const override;
    std::string getConfigType() const override { return "ClientConfiguration"; }
    
    // Configuration management
    bool loadFromFile(const std::string& filePath);
    bool saveToFile(const std::string& filePath) const;
    bool loadFromString(const std::string& jsonString);
    std::string saveToString() const;
    
    // Configuration merging
    void merge(const ClientConfiguration& other);
    void mergeFromJson(const json& j);
    
    // Feature management
    void enableFeature(const std::string& feature, bool enable = true);
    bool isFeatureEnabled(const std::string& feature) const;
    std::vector<std::string> getEnabledFeatures() const;
    
    // Protocol configuration
    void setProtocolConfig(MessageFormat protocol, const json& config);
    json getProtocolConfig(MessageFormat protocol) const;
    
    // Environment variable support
    void loadFromEnvironment(const std::string& prefix = "HYDROGEN_");
    
    // Configuration templates
    static ClientConfiguration createDefault();
    static ClientConfiguration createSecure();
    static ClientConfiguration createHighPerformance();
    static ClientConfiguration createDebug();
    
private:
    void applyDefaults();
    void processEnvironmentVariables(const std::string& prefix);
    std::string expandEnvironmentVariables(const std::string& value) const;
};

/**
 * @brief Configuration manager for handling multiple configurations
 */
class ConfigurationManager {
public:
    static ConfigurationManager& getInstance();
    
    // Configuration registration
    void registerConfiguration(const std::string& name, std::shared_ptr<ClientConfiguration> config);
    void unregisterConfiguration(const std::string& name);
    
    // Configuration access
    std::shared_ptr<ClientConfiguration> getConfiguration(const std::string& name) const;
    std::shared_ptr<ClientConfiguration> getDefaultConfiguration() const;
    void setDefaultConfiguration(const std::string& name);
    
    // Configuration management
    std::vector<std::string> getConfigurationNames() const;
    bool hasConfiguration(const std::string& name) const;
    
    // File operations
    bool loadConfiguration(const std::string& name, const std::string& filePath);
    bool saveConfiguration(const std::string& name, const std::string& filePath) const;
    
    // Validation
    std::unordered_map<std::string, ConfigValidationResult> validateAllConfigurations() const;
    
    // Configuration watching
    using ConfigChangeCallback = std::function<void(const std::string&, const ClientConfiguration&)>;
    void setConfigChangeCallback(ConfigChangeCallback callback);
    void watchConfigurationFile(const std::string& name, const std::string& filePath);
    void stopWatching(const std::string& name);

private:
    ConfigurationManager() = default;
    
    mutable std::mutex configMutex_;
    std::unordered_map<std::string, std::shared_ptr<ClientConfiguration>> configurations_;
    std::string defaultConfigName_ = "default";
    
    ConfigChangeCallback changeCallback_;
    std::unordered_map<std::string, std::string> watchedFiles_;
    std::thread fileWatcherThread_;
    std::atomic<bool> stopWatching_{false};
    
    void fileWatcherLoop();
};

/**
 * @brief Configuration builder for fluent configuration creation
 */
class ConfigurationBuilder {
public:
    ConfigurationBuilder();
    
    // Network configuration
    ConfigurationBuilder& withHost(const std::string& host);
    ConfigurationBuilder& withPort(uint16_t port);
    ConfigurationBuilder& withEndpoint(const std::string& endpoint);
    ConfigurationBuilder& withTls(bool enable = true);
    ConfigurationBuilder& withTimeout(std::chrono::milliseconds timeout);
    
    // Authentication
    ConfigurationBuilder& withBasicAuth(const std::string& username, const std::string& password);
    ConfigurationBuilder& withBearerToken(const std::string& token);
    ConfigurationBuilder& withApiKey(const std::string& key, const std::string& header = "X-API-Key");
    
    // Protocol
    ConfigurationBuilder& withProtocol(MessageFormat protocol);
    ConfigurationBuilder& withProtocolConfig(MessageFormat protocol, const json& config);
    
    // Features
    ConfigurationBuilder& withFeature(const std::string& feature, bool enable = true);
    
    // Performance
    ConfigurationBuilder& withWorkerThreads(size_t threads);
    ConfigurationBuilder& withMaxQueueSize(size_t size);
    
    // Logging
    ConfigurationBuilder& withLogLevel(LoggingConfig::LogLevel level);
    ConfigurationBuilder& withLogFile(const std::string& filePath);
    
    // Build
    std::unique_ptr<ClientConfiguration> build();
    
private:
    std::unique_ptr<ClientConfiguration> config_;
};

} // namespace core
} // namespace hydrogen

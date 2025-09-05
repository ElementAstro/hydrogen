#include "hydrogen/core/client_configuration.h"
#include "hydrogen/core/message_transformer.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>

namespace hydrogen {
namespace core {

// ConfigValidationResult implementation
std::string ConfigValidationResult::toString() const {
  std::stringstream ss;
  ss << "Validation " << (isValid ? "PASSED" : "FAILED") << "\n";

  if (!errors.empty()) {
    ss << "Errors:\n";
    for (const auto &error : errors) {
      ss << "  - " << error << "\n";
    }
  }

  if (!warnings.empty()) {
    ss << "Warnings:\n";
    for (const auto &warning : warnings) {
      ss << "  - " << warning << "\n";
    }
  }

  return ss.str();
}

// NetworkConfig implementation
json NetworkConfig::toJson() const {
  json j;
  j["host"] = host;
  j["port"] = port;
  j["endpoint"] = endpoint;
  j["useTls"] = useTls;
  j["tlsCertPath"] = tlsCertPath;
  j["tlsKeyPath"] = tlsKeyPath;
  j["tlsCaPath"] = tlsCaPath;
  j["verifyTlsCertificate"] = verifyTlsCertificate;
  j["connectTimeout"] = connectTimeout.count();
  j["readTimeout"] = readTimeout.count();
  j["writeTimeout"] = writeTimeout.count();
  j["keepAliveInterval"] = keepAliveInterval.count();
  j["maxConnections"] = maxConnections;
  j["maxReconnectAttempts"] = maxReconnectAttempts;
  j["reconnectInterval"] = reconnectInterval.count();
  j["protocolSettings"] = protocolSettings;
  return j;
}

void NetworkConfig::fromJson(const json &j) {
  host = j.value("host", "localhost");
  port = j.value("port", 8080);
  endpoint = j.value("endpoint", "/ws");
  useTls = j.value("useTls", false);
  tlsCertPath = j.value("tlsCertPath", "");
  tlsKeyPath = j.value("tlsKeyPath", "");
  tlsCaPath = j.value("tlsCaPath", "");
  verifyTlsCertificate = j.value("verifyTlsCertificate", true);
  connectTimeout = std::chrono::milliseconds(j.value("connectTimeout", 5000));
  readTimeout = std::chrono::milliseconds(j.value("readTimeout", 30000));
  writeTimeout = std::chrono::milliseconds(j.value("writeTimeout", 30000));
  keepAliveInterval =
      std::chrono::milliseconds(j.value("keepAliveInterval", 30000));
  maxConnections = j.value("maxConnections", 100);
  maxReconnectAttempts = j.value("maxReconnectAttempts", 0);
  reconnectInterval =
      std::chrono::milliseconds(j.value("reconnectInterval", 5000));
  protocolSettings = j.value("protocolSettings", json::object());
}

ConfigValidationResult NetworkConfig::validate() const {
  ConfigValidationResult result;

  if (host.empty()) {
    result.addError("Host cannot be empty");
  }

  if (port == 0 || port > 65535) {
    result.addError("Port must be between 1 and 65535");
  }

  if (useTls) {
    if (tlsCertPath.empty() && tlsKeyPath.empty()) {
      result.addWarning("TLS enabled but no certificate/key paths specified");
    }
  }

  if (connectTimeout.count() <= 0) {
    result.addError("Connect timeout must be positive");
  }

  if (maxConnections <= 0) {
    result.addError("Max connections must be positive");
  }

  return result;
}

// AuthConfig implementation
json AuthConfig::toJson() const {
  json j;
  j["type"] = static_cast<int>(type);
  j["username"] = username;
  j["password"] = password;
  j["token"] = token;
  j["apiKey"] = apiKey;
  j["apiKeyHeader"] = apiKeyHeader;
  j["clientId"] = clientId;
  j["clientSecret"] = clientSecret;
  j["authUrl"] = authUrl;
  j["tokenUrl"] = tokenUrl;
  j["scopes"] = scopes;
  j["certPath"] = certPath;
  j["keyPath"] = keyPath;
  j["keyPassword"] = keyPassword;
  j["customSettings"] = customSettings;
  return j;
}

void AuthConfig::fromJson(const json &j) {
  type = static_cast<AuthType>(j.value("type", 0));
  username = j.value("username", "");
  password = j.value("password", "");
  token = j.value("token", "");
  apiKey = j.value("apiKey", "");
  apiKeyHeader = j.value("apiKeyHeader", "X-API-Key");
  clientId = j.value("clientId", "");
  clientSecret = j.value("clientSecret", "");
  authUrl = j.value("authUrl", "");
  tokenUrl = j.value("tokenUrl", "");
  scopes = j.value("scopes", std::vector<std::string>{});
  certPath = j.value("certPath", "");
  keyPath = j.value("keyPath", "");
  keyPassword = j.value("keyPassword", "");
  customSettings = j.value("customSettings", json::object());
}

ConfigValidationResult AuthConfig::validate() const {
  ConfigValidationResult result;

  switch (type) {
  case AuthType::BASIC:
    if (username.empty() || password.empty()) {
      result.addError("Basic auth requires username and password");
    }
    break;
  case AuthType::BEARER_TOKEN:
    if (token.empty()) {
      result.addError("Bearer token auth requires token");
    }
    break;
  case AuthType::API_KEY:
    if (apiKey.empty()) {
      result.addError("API key auth requires apiKey");
    }
    if (apiKeyHeader.empty()) {
      result.addError("API key auth requires apiKeyHeader");
    }
    break;
  case AuthType::OAUTH2:
    if (clientId.empty() || clientSecret.empty()) {
      result.addError("OAuth2 requires clientId and clientSecret");
    }
    if (authUrl.empty() || tokenUrl.empty()) {
      result.addError("OAuth2 requires authUrl and tokenUrl");
    }
    break;
  case AuthType::CERTIFICATE:
    if (certPath.empty() || keyPath.empty()) {
      result.addError("Certificate auth requires certPath and keyPath");
    }
    break;
  default:
    break;
  }

  return result;
}

// LoggingConfig implementation
json LoggingConfig::toJson() const {
  json j;
  j["level"] = static_cast<int>(level);
  j["pattern"] = pattern;
  j["enableConsole"] = enableConsole;
  j["enableFile"] = enableFile;
  j["logFile"] = logFile;
  j["maxFileSize"] = maxFileSize;
  j["maxFiles"] = maxFiles;
  j["enableRotation"] = enableRotation;
  return j;
}

void LoggingConfig::fromJson(const json &j) {
  level = static_cast<LogLevel>(j.value("level", 2)); // INFO
  pattern = j.value("pattern", "[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
  enableConsole = j.value("enableConsole", true);
  enableFile = j.value("enableFile", false);
  logFile = j.value("logFile", "hydrogen_client.log");
  maxFileSize = j.value("maxFileSize", 10 * 1024 * 1024);
  maxFiles = j.value("maxFiles", 5);
  enableRotation = j.value("enableRotation", true);
}

ConfigValidationResult LoggingConfig::validate() const {
  ConfigValidationResult result;

  if (!enableConsole && !enableFile) {
    result.addWarning("Both console and file logging are disabled");
  }

  if (enableFile && logFile.empty()) {
    result.addError("File logging enabled but no log file specified");
  }

  if (maxFileSize == 0) {
    result.addError("Max file size must be positive");
  }

  if (maxFiles == 0) {
    result.addError("Max files must be positive");
  }

  return result;
}

// PerformanceConfig implementation
json PerformanceConfig::toJson() const {
  json j;
  j["workerThreads"] = workerThreads;
  j["ioThreads"] = ioThreads;
  j["maxMessageQueueSize"] = maxMessageQueueSize;
  j["maxCacheSize"] = maxCacheSize;
  j["cacheExpiry"] = cacheExpiry.count();
  j["maxRequestsPerSecond"] = maxRequestsPerSecond;
  j["burstSize"] = burstSize;
  j["enableCompression"] = enableCompression;
  j["compressionAlgorithm"] = compressionAlgorithm;
  j["compressionLevel"] = compressionLevel;
  return j;
}

void PerformanceConfig::fromJson(const json &j) {
  workerThreads = j.value("workerThreads", 0);
  ioThreads = j.value("ioThreads", 1);
  maxMessageQueueSize = j.value("maxMessageQueueSize", 1000);
  maxCacheSize = j.value("maxCacheSize", 100 * 1024 * 1024);
  cacheExpiry = std::chrono::milliseconds(j.value("cacheExpiry", 300000));
  maxRequestsPerSecond = j.value("maxRequestsPerSecond", 0);
  burstSize = j.value("burstSize", 10);
  enableCompression = j.value("enableCompression", false);
  compressionAlgorithm = j.value("compressionAlgorithm", "gzip");
  compressionLevel = j.value("compressionLevel", 6);
}

ConfigValidationResult PerformanceConfig::validate() const {
  ConfigValidationResult result;

  if (ioThreads == 0) {
    result.addError("IO threads must be at least 1");
  }

  if (maxMessageQueueSize == 0) {
    result.addError("Max message queue size must be positive");
  }

  if (compressionLevel < 1 || compressionLevel > 9) {
    result.addError("Compression level must be between 1 and 9");
  }

  return result;
}

// DiscoveryConfig implementation
json DiscoveryConfig::toJson() const {
  json j;
  j["enableAutoDiscovery"] = enableAutoDiscovery;
  j["discoveryInterval"] = discoveryInterval.count();
  j["deviceTypes"] = deviceTypes;
  j["excludeDevices"] = excludeDevices;
  j["discoveryFilters"] = discoveryFilters;
  j["enableNetworkScan"] = enableNetworkScan;
  j["scanRanges"] = scanRanges;
  j["scanPorts"] = scanPorts;
  return j;
}

void DiscoveryConfig::fromJson(const json &j) {
  enableAutoDiscovery = j.value("enableAutoDiscovery", true);
  discoveryInterval =
      std::chrono::milliseconds(j.value("discoveryInterval", 30000));
  deviceTypes = j.value("deviceTypes", std::vector<std::string>{});
  excludeDevices = j.value("excludeDevices", std::vector<std::string>{});
  discoveryFilters = j.value("discoveryFilters", json::object());
  enableNetworkScan = j.value("enableNetworkScan", false);
  scanRanges = j.value("scanRanges", std::vector<std::string>{});
  scanPorts = j.value("scanPorts", std::vector<uint16_t>{});
}

ConfigValidationResult DiscoveryConfig::validate() const {
  ConfigValidationResult result;

  if (discoveryInterval.count() <= 0) {
    result.addError("Discovery interval must be positive");
  }

  if (enableNetworkScan && scanRanges.empty()) {
    result.addWarning("Network scan enabled but no scan ranges specified");
  }

  return result;
}

// ClientConfiguration implementation
ClientConfiguration::ClientConfiguration() { applyDefaults(); }

json ClientConfiguration::toJson() const {
  json j;
  j["network"] = network.toJson();
  j["authentication"] = authentication.toJson();
  j["logging"] = logging.toJson();
  j["performance"] = performance.toJson();
  j["discovery"] = discovery.toJson();
  j["defaultProtocol"] = static_cast<int>(defaultProtocol);

  json protocolConfigsJson = json::object();
  for (const auto &[protocol, config] : protocolConfigs) {
    protocolConfigsJson[std::to_string(static_cast<int>(protocol))] = config;
  }
  j["protocolConfigs"] = protocolConfigsJson;

  j["features"] = features;
  j["customSettings"] = customSettings;
  j["version"] = ClientConfiguration::getVersion();
  j["type"] = ClientConfiguration::getConfigType();

  return j;
}

void ClientConfiguration::fromJson(const json &j) {
  if (j.contains("network")) {
    network.fromJson(j["network"]);
  }
  if (j.contains("authentication")) {
    authentication.fromJson(j["authentication"]);
  }
  if (j.contains("logging")) {
    logging.fromJson(j["logging"]);
  }
  if (j.contains("performance")) {
    performance.fromJson(j["performance"]);
  }
  if (j.contains("discovery")) {
    discovery.fromJson(j["discovery"]);
  }

  defaultProtocol = static_cast<MessageFormat>(j.value("defaultProtocol", 0));

  if (j.contains("protocolConfigs")) {
    protocolConfigs.clear();
    for (const auto &[key, value] : j["protocolConfigs"].items()) {
      int protocolInt = std::stoi(key);
      protocolConfigs[static_cast<MessageFormat>(protocolInt)] = value;
    }
  }

  features = j.value("features", std::unordered_map<std::string, bool>{});
  customSettings = j.value("customSettings", json::object());
}

ConfigValidationResult ClientConfiguration::validate() const {
  ConfigValidationResult result;

  // Validate individual sections
  auto networkResult = network.validate();
  auto authResult = authentication.validate();
  auto loggingResult = logging.validate();
  auto performanceResult = performance.validate();
  auto discoveryResult = discovery.validate();

  // Merge results
  result.errors.insert(result.errors.end(), networkResult.errors.begin(),
                       networkResult.errors.end());
  result.errors.insert(result.errors.end(), authResult.errors.begin(),
                       authResult.errors.end());
  result.errors.insert(result.errors.end(), loggingResult.errors.begin(),
                       loggingResult.errors.end());
  result.errors.insert(result.errors.end(), performanceResult.errors.begin(),
                       performanceResult.errors.end());
  result.errors.insert(result.errors.end(), discoveryResult.errors.begin(),
                       discoveryResult.errors.end());

  result.warnings.insert(result.warnings.end(), networkResult.warnings.begin(),
                         networkResult.warnings.end());
  result.warnings.insert(result.warnings.end(), authResult.warnings.begin(),
                         authResult.warnings.end());
  result.warnings.insert(result.warnings.end(), loggingResult.warnings.begin(),
                         loggingResult.warnings.end());
  result.warnings.insert(result.warnings.end(),
                         performanceResult.warnings.begin(),
                         performanceResult.warnings.end());
  result.warnings.insert(result.warnings.end(),
                         discoveryResult.warnings.begin(),
                         discoveryResult.warnings.end());

  result.isValid = result.errors.empty();

  // Cross-validation checks
  if (network.useTls && authentication.type == AuthConfig::AuthType::BASIC) {
    result.addWarning("Using basic authentication over TLS - consider using "
                      "stronger authentication");
  }

  if (performance.enableCompression && network.useTls) {
    result.addWarning(
        "Compression with TLS may be vulnerable to CRIME/BREACH attacks");
  }

  return result;
}

bool ClientConfiguration::loadFromFile(const std::string &filePath) {
  try {
    std::ifstream file(filePath);
    if (!file.is_open()) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::error("ClientConfiguration: Cannot open file: {}", filePath);
#endif
      return false;
    }

    json j;
    file >> j;
    fromJson(j);

#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("ClientConfiguration: Loaded configuration from {}", filePath);
#endif
    return true;

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("ClientConfiguration: Failed to load from file {}: {}",
                  filePath, e.what());
#endif
    return false;
  }
}

bool ClientConfiguration::saveToFile(const std::string &filePath) const {
  try {
    std::ofstream file(filePath);
    if (!file.is_open()) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::error("ClientConfiguration: Cannot create file: {}", filePath);
#endif
      return false;
    }

    json j = toJson();
    file << j.dump(2);

#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("ClientConfiguration: Saved configuration to {}", filePath);
#endif
    return true;

  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("ClientConfiguration: Failed to save to file {}: {}",
                  filePath, e.what());
#endif
    return false;
  }
}

bool ClientConfiguration::loadFromString(const std::string &jsonString) {
  try {
    json j = json::parse(jsonString);
    fromJson(j);
    return true;
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("ClientConfiguration: Failed to parse JSON string: {}",
                  e.what());
#endif
    return false;
  }
}

std::string ClientConfiguration::saveToString() const {
  try {
    json j = toJson();
    return j.dump(2);
  } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("ClientConfiguration: Failed to serialize to JSON: {}",
                  e.what());
#endif
    return "{}";
  }
}

void ClientConfiguration::merge(const ClientConfiguration &other) {
  // Merge network config
  if (!other.network.host.empty())
    network.host = other.network.host;
  if (other.network.port != 8080)
    network.port = other.network.port;
  if (!other.network.endpoint.empty())
    network.endpoint = other.network.endpoint;
  network.useTls = other.network.useTls;

  // Merge authentication
  if (other.authentication.type != AuthConfig::AuthType::NONE) {
    authentication = other.authentication;
  }

  // Merge features
  for (const auto &[feature, enabled] : other.features) {
    features[feature] = enabled;
  }

  // Merge protocol configs
  for (const auto &[protocol, config] : other.protocolConfigs) {
    protocolConfigs[protocol] = config;
  }

  // Merge custom settings
  for (const auto &[key, value] : other.customSettings.items()) {
    customSettings[key] = value;
  }
}

void ClientConfiguration::mergeFromJson(const json &j) {
  ClientConfiguration temp;
  temp.fromJson(j);
  merge(temp);
}

void ClientConfiguration::enableFeature(const std::string &feature,
                                        bool enable) {
  features[feature] = enable;
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug("ClientConfiguration: Feature '{}' {}", feature,
                enable ? "enabled" : "disabled");
#endif
}

bool ClientConfiguration::isFeatureEnabled(const std::string &feature) const {
  auto it = features.find(feature);
  return (it != features.end()) ? it->second : false;
}

std::vector<std::string> ClientConfiguration::getEnabledFeatures() const {
  std::vector<std::string> enabledFeatures;
  for (const auto &[feature, enabled] : features) {
    if (enabled) {
      enabledFeatures.push_back(feature);
    }
  }
  return enabledFeatures;
}

void ClientConfiguration::setProtocolConfig(MessageFormat protocol,
                                            const json &config) {
  protocolConfigs[protocol] = config;
}

json ClientConfiguration::getProtocolConfig(MessageFormat protocol) const {
  auto it = protocolConfigs.find(protocol);
  return (it != protocolConfigs.end()) ? it->second : json::object();
}

void ClientConfiguration::loadFromEnvironment(const std::string &prefix) {
  processEnvironmentVariables(prefix);
}

void ClientConfiguration::applyDefaults() {
  // Set default features
  features["auto_reconnect"] = true;
  features["message_compression"] = false;
  features["device_discovery"] = true;
  features["heartbeat"] = true;
  features["statistics"] = true;
}

void ClientConfiguration::processEnvironmentVariables(
    const std::string &prefix) {
  // Network settings
  if (const char *host = std::getenv((prefix + "HOST").c_str())) {
    network.host = host;
  }
  if (const char *port = std::getenv((prefix + "PORT").c_str())) {
    network.port = static_cast<uint16_t>(std::stoi(port));
  }
  if (const char *useTls = std::getenv((prefix + "USE_TLS").c_str())) {
    network.useTls =
        (std::string(useTls) == "true" || std::string(useTls) == "1");
  }

  // Authentication settings
  if (const char *username = std::getenv((prefix + "USERNAME").c_str())) {
    authentication.username = username;
    authentication.type = AuthConfig::AuthType::BASIC;
  }
  if (const char *password = std::getenv((prefix + "PASSWORD").c_str())) {
    authentication.password = password;
  }
  if (const char *token = std::getenv((prefix + "TOKEN").c_str())) {
    authentication.token = token;
    authentication.type = AuthConfig::AuthType::BEARER_TOKEN;
  }

  // Logging settings
  if (const char *logLevel = std::getenv((prefix + "LOG_LEVEL").c_str())) {
    std::string level = logLevel;
    if (level == "TRACE")
      logging.level = LoggingConfig::LogLevel::TRACE;
    else if (level == "DEBUG")
      logging.level = LoggingConfig::LogLevel::DEBUG;
    else if (level == "INFO")
      logging.level = LoggingConfig::LogLevel::INFO;
    else if (level == "WARN")
      logging.level = LoggingConfig::LogLevel::WARN;
    else if (level == "ERROR")
      logging.level = LoggingConfig::LogLevel::ERR;
    else if (level == "CRITICAL")
      logging.level = LoggingConfig::LogLevel::CRITICAL;
  }
}

std::string ClientConfiguration::expandEnvironmentVariables(
    const std::string &value) const {
  std::string result = value;
  std::regex envVarRegex(R"(\$\{([^}]+)\})");
  std::smatch match;

  while (std::regex_search(result, match, envVarRegex)) {
    std::string envVar = match[1].str();
    const char *envValue = std::getenv(envVar.c_str());
    std::string replacement = envValue ? envValue : "";
    result.replace(match.position(), match.length(), replacement);
  }

  return result;
}

// Static factory methods
ClientConfiguration ClientConfiguration::createDefault() {
  ClientConfiguration config;
  config.network.host = "localhost";
  config.network.port = 8080;
  config.network.useTls = false;
  config.authentication.type = AuthConfig::AuthType::NONE;
  config.logging.level = LoggingConfig::LogLevel::INFO;
  config.logging.enableConsole = true;
  config.logging.enableFile = false;
  config.performance.workerThreads = 0; // Auto-detect
  config.discovery.enableAutoDiscovery = true;
  return config;
}

ClientConfiguration ClientConfiguration::createSecure() {
  ClientConfiguration config = createDefault();
  config.network.useTls = true;
  config.network.port = 8443;
  config.network.verifyTlsCertificate = true;
  config.authentication.type = AuthConfig::AuthType::BEARER_TOKEN;
  config.logging.level = LoggingConfig::LogLevel::WARN;
  config.enableFeature("message_compression", true);
  return config;
}

ClientConfiguration ClientConfiguration::createHighPerformance() {
  ClientConfiguration config = createDefault();
  config.network.connectTimeout = std::chrono::milliseconds{2000};
  config.network.readTimeout = std::chrono::milliseconds{10000};
  config.network.keepAliveInterval = std::chrono::milliseconds{10000};
  config.performance.workerThreads = std::thread::hardware_concurrency();
  config.performance.ioThreads = 2;
  config.performance.maxMessageQueueSize = 5000;
  config.performance.enableCompression = true;
  config.logging.level = LoggingConfig::LogLevel::ERR;
  config.discovery.discoveryInterval = std::chrono::milliseconds{10000};
  return config;
}

ClientConfiguration ClientConfiguration::createDebug() {
  ClientConfiguration config = createDefault();
  config.logging.level = LoggingConfig::LogLevel::DEBUG;
  config.logging.enableFile = true;
  config.logging.logFile = "hydrogen_debug.log";
  config.enableFeature("statistics", true);
  config.enableFeature("heartbeat", true);
  config.performance.maxMessageQueueSize = 100; // Smaller for debugging
  return config;
}

// ConfigurationManager implementation
ConfigurationManager &ConfigurationManager::getInstance() {
  static ConfigurationManager instance;
  return instance;
}

void ConfigurationManager::registerConfiguration(
    const std::string &name, std::shared_ptr<ClientConfiguration> config) {
  std::lock_guard<std::mutex> lock(configMutex_);
  configurations_[name] = config;
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("ConfigurationManager: Registered configuration '{}'", name);
#endif
}

void ConfigurationManager::unregisterConfiguration(const std::string &name) {
  std::lock_guard<std::mutex> lock(configMutex_);
  auto it = configurations_.find(name);
  if (it != configurations_.end()) {
    configurations_.erase(it);
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("ConfigurationManager: Unregistered configuration '{}'", name);
#endif
  }
}

std::shared_ptr<ClientConfiguration>
ConfigurationManager::getConfiguration(const std::string &name) const {
  std::lock_guard<std::mutex> lock(configMutex_);
  auto it = configurations_.find(name);
  return (it != configurations_.end()) ? it->second : nullptr;
}

std::shared_ptr<ClientConfiguration>
ConfigurationManager::getDefaultConfiguration() const {
  return getConfiguration(defaultConfigName_);
}

void ConfigurationManager::setDefaultConfiguration(const std::string &name) {
  std::lock_guard<std::mutex> lock(configMutex_);
  if (configurations_.find(name) != configurations_.end()) {
    defaultConfigName_ = name;
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("ConfigurationManager: Set default configuration to '{}'",
                 name);
#endif
  } else {
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::warn("ConfigurationManager: Cannot set default to non-existent "
                 "configuration '{}'",
                 name);
#endif
  }
}

std::vector<std::string> ConfigurationManager::getConfigurationNames() const {
  std::lock_guard<std::mutex> lock(configMutex_);
  std::vector<std::string> names;
  for (const auto &[name, config] : configurations_) {
    names.push_back(name);
  }
  return names;
}

bool ConfigurationManager::hasConfiguration(const std::string &name) const {
  std::lock_guard<std::mutex> lock(configMutex_);
  return configurations_.find(name) != configurations_.end();
}

bool ConfigurationManager::loadConfiguration(const std::string &name,
                                             const std::string &filePath) {
  auto config = std::make_shared<ClientConfiguration>();
  if (config->loadFromFile(filePath)) {
    registerConfiguration(name, config);
    return true;
  }
  return false;
}

bool ConfigurationManager::saveConfiguration(
    const std::string &name, const std::string &filePath) const {
  auto config = getConfiguration(name);
  if (config) {
    return config->saveToFile(filePath);
  }
  return false;
}

std::unordered_map<std::string, ConfigValidationResult>
ConfigurationManager::validateAllConfigurations() const {
  std::lock_guard<std::mutex> lock(configMutex_);
  std::unordered_map<std::string, ConfigValidationResult> results;

  for (const auto &[name, config] : configurations_) {
    results[name] = config->validate();
  }

  return results;
}

void ConfigurationManager::setConfigChangeCallback(
    ConfigChangeCallback callback) {
  changeCallback_ = callback;
}

void ConfigurationManager::watchConfigurationFile(const std::string &name,
                                                  const std::string &filePath) {
  std::lock_guard<std::mutex> lock(configMutex_);
  watchedFiles_[name] = filePath;

  if (!fileWatcherThread_.joinable()) {
    stopWatching_.store(false);
    fileWatcherThread_ =
        std::thread(&ConfigurationManager::fileWatcherLoop, this);
  }
}

void ConfigurationManager::stopWatching(const std::string &name) {
  std::lock_guard<std::mutex> lock(configMutex_);
  watchedFiles_.erase(name);
}

void ConfigurationManager::fileWatcherLoop() {
  // Simple file watching implementation
  // In a real implementation, you would use platform-specific file watching
  // APIs
  std::unordered_map<std::string, std::time_t> lastModified;

  while (!stopWatching_.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::unordered_map<std::string, std::string> currentWatchedFiles;
    {
      std::lock_guard<std::mutex> lock(configMutex_);
      currentWatchedFiles = watchedFiles_;
    }

    for (const auto &[name, filePath] : currentWatchedFiles) {
      try {
        std::ifstream file(filePath);
        if (file.good()) {
          // Get file modification time (simplified)
          auto currentTime = std::chrono::system_clock::now();
          auto timeT = std::chrono::system_clock::to_time_t(currentTime);

          if (lastModified.find(filePath) == lastModified.end()) {
            lastModified[filePath] = timeT;
            continue;
          }

          if (timeT > lastModified[filePath]) {
            lastModified[filePath] = timeT;

            // Reload configuration
            auto config = getConfiguration(name);
            if (config && config->loadFromFile(filePath)) {
              if (changeCallback_) {
                changeCallback_(name, *config);
              }
#ifdef HYDROGEN_HAS_SPDLOG
              spdlog::info(
                  "ConfigurationManager: Reloaded configuration '{}' from {}",
                  name, filePath);
#endif
            }
          }
        }
      } catch (const std::exception &e) {
#ifdef HYDROGEN_HAS_SPDLOG
        spdlog::error("ConfigurationManager: Error watching file {}: {}",
                      filePath, e.what());
#endif
      }
    }
  }
}

// ConfigurationBuilder implementation
ConfigurationBuilder::ConfigurationBuilder()
    : config_(std::make_unique<ClientConfiguration>()) {}

ConfigurationBuilder &ConfigurationBuilder::withHost(const std::string &host) {
  config_->network.host = host;
  return *this;
}

ConfigurationBuilder &ConfigurationBuilder::withPort(uint16_t port) {
  config_->network.port = port;
  return *this;
}

ConfigurationBuilder &
ConfigurationBuilder::withEndpoint(const std::string &endpoint) {
  config_->network.endpoint = endpoint;
  return *this;
}

ConfigurationBuilder &ConfigurationBuilder::withTls(bool enable) {
  config_->network.useTls = enable;
  return *this;
}

ConfigurationBuilder &
ConfigurationBuilder::withTimeout(std::chrono::milliseconds timeout) {
  config_->network.connectTimeout = timeout;
  config_->network.readTimeout = timeout;
  config_->network.writeTimeout = timeout;
  return *this;
}

ConfigurationBuilder &
ConfigurationBuilder::withBasicAuth(const std::string &username,
                                    const std::string &password) {
  config_->authentication.type = AuthConfig::AuthType::BASIC;
  config_->authentication.username = username;
  config_->authentication.password = password;
  return *this;
}

ConfigurationBuilder &
ConfigurationBuilder::withBearerToken(const std::string &token) {
  config_->authentication.type = AuthConfig::AuthType::BEARER_TOKEN;
  config_->authentication.token = token;
  return *this;
}

ConfigurationBuilder &
ConfigurationBuilder::withApiKey(const std::string &key,
                                 const std::string &header) {
  config_->authentication.type = AuthConfig::AuthType::API_KEY;
  config_->authentication.apiKey = key;
  config_->authentication.apiKeyHeader = header;
  return *this;
}

ConfigurationBuilder &
ConfigurationBuilder::withProtocol(MessageFormat protocol) {
  config_->defaultProtocol = protocol;
  return *this;
}

ConfigurationBuilder &
ConfigurationBuilder::withProtocolConfig(MessageFormat protocol,
                                         const json &configJson) {
  config_->protocolConfigs[protocol] = configJson;
  return *this;
}

ConfigurationBuilder &
ConfigurationBuilder::withFeature(const std::string &feature, bool enable) {
  config_->enableFeature(feature, enable);
  return *this;
}

ConfigurationBuilder &ConfigurationBuilder::withWorkerThreads(size_t threads) {
  config_->performance.workerThreads = threads;
  return *this;
}

ConfigurationBuilder &ConfigurationBuilder::withMaxQueueSize(size_t size) {
  config_->performance.maxMessageQueueSize = size;
  return *this;
}

ConfigurationBuilder &
ConfigurationBuilder::withLogLevel(LoggingConfig::LogLevel level) {
  config_->logging.level = level;
  return *this;
}

ConfigurationBuilder &
ConfigurationBuilder::withLogFile(const std::string &filePath) {
  config_->logging.enableFile = true;
  config_->logging.logFile = filePath;
  return *this;
}

std::unique_ptr<ClientConfiguration> ConfigurationBuilder::build() {
  return std::move(config_);
}

} // namespace core
} // namespace hydrogen

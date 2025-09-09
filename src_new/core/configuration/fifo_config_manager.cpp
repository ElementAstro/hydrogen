#include "hydrogen/core/configuration/fifo_config_manager.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#endif

namespace hydrogen {
namespace core {

// FifoConfig implementation
json FifoConfig::toJson() const {
  json j;

  // Basic configuration
  j["pipeName"] = pipeName;
  j["pipeDirectory"] = pipeDirectory;
  j["pipeType"] = static_cast<int>(pipeType);
  j["accessMode"] = static_cast<int>(accessMode);
  j["unixPipePath"] = unixPipePath;
  j["windowsPipePath"] = windowsPipePath;

  // Message framing
  j["framingMode"] = static_cast<int>(framingMode);
  j["customDelimiter"] = customDelimiter;
  j["lineTerminator"] = lineTerminator;
  j["messagePrefix"] = messagePrefix;
  j["messageSuffix"] = messageSuffix;

  // Buffer settings
  j["bufferSize"] = bufferSize;
  j["maxMessageSize"] = maxMessageSize;
  j["readBufferSize"] = readBufferSize;
  j["writeBufferSize"] = writeBufferSize;
  j["maxQueueSize"] = maxQueueSize;

  // Timeouts
  j["connectTimeout"] = connectTimeout.count();
  j["readTimeout"] = readTimeout.count();
  j["writeTimeout"] = writeTimeout.count();
  j["reconnectDelay"] = reconnectDelay.count();
  j["keepAliveInterval"] = keepAliveInterval.count();

  // Connection management
  j["maxReconnectAttempts"] = maxReconnectAttempts;
  j["enableAutoReconnect"] = enableAutoReconnect;
  j["enableKeepAlive"] = enableKeepAlive;
  j["enableNonBlocking"] = enableNonBlocking;
  j["enableBidirectional"] = enableBidirectional;

  // Unix permissions
  j["pipePermissions"] = pipePermissions;
  j["pipeOwner"] = pipeOwner;
  j["pipeGroup"] = pipeGroup;

  // Windows settings
  j["windowsPipeInstances"] = windowsPipeInstances;
  j["windowsOutBufferSize"] = windowsOutBufferSize;
  j["windowsInBufferSize"] = windowsInBufferSize;
  j["windowsDefaultTimeout"] = windowsDefaultTimeout;

  // Message processing
  j["enableMessageValidation"] = enableMessageValidation;
  j["enableMessageLogging"] = enableMessageLogging;
  j["enableMessageTracing"] = enableMessageTracing;
  j["enableBinaryMode"] = enableBinaryMode;
  j["enableFlowControl"] = enableFlowControl;
  j["enableBackpressure"] = enableBackpressure;

  // Compression
  j["compressionType"] = static_cast<int>(compressionType);
  j["compressionLevel"] = compressionLevel;
  j["compressionThreshold"] = compressionThreshold;
  j["enableCompressionForSmallMessages"] = enableCompressionForSmallMessages;

  // Security
  j["authMethod"] = static_cast<int>(authMethod);
  j["authToken"] = authToken;
  j["certificatePath"] = certificatePath;
  j["privateKeyPath"] = privateKeyPath;
  j["enableEncryption"] = enableEncryption;
  j["encryptionKey"] = encryptionKey;

  // Error handling
  j["enableCircuitBreaker"] = enableCircuitBreaker;
  j["circuitBreakerThreshold"] = circuitBreakerThreshold;
  j["circuitBreakerTimeout"] = circuitBreakerTimeout.count();
  j["enableRetryOnError"] = enableRetryOnError;
  j["maxRetryAttempts"] = maxRetryAttempts;
  j["retryDelay"] = retryDelay.count();

  // Monitoring
  j["enablePerformanceMetrics"] = enablePerformanceMetrics;
  j["enableHealthChecking"] = enableHealthChecking;
  j["healthCheckInterval"] = healthCheckInterval.count();
  j["enableDebugLogging"] = enableDebugLogging;
  j["logLevel"] = logLevel;

  // Advanced features
  j["enableMultiplexing"] = enableMultiplexing;
  j["maxConcurrentConnections"] = maxConcurrentConnections;
  j["enableMessagePrioritization"] = enableMessagePrioritization;
  j["enableMessageDeduplication"] = enableMessageDeduplication;
  j["deduplicationWindow"] = deduplicationWindow.count();

  // Platform optimizations
  j["enablePlatformOptimizations"] = enablePlatformOptimizations;
  j["useMemoryMappedFiles"] = useMemoryMappedFiles;
  j["enableZeroCopy"] = enableZeroCopy;
  j["ioVectorSize"] = ioVectorSize;

  return j;
}

void FifoConfig::fromJson(const json &j) {
  if (j.contains("pipeName"))
    pipeName = j["pipeName"];
  if (j.contains("pipeDirectory"))
    pipeDirectory = j["pipeDirectory"];
  if (j.contains("pipeType"))
    pipeType = static_cast<FifoPipeType>(j["pipeType"]);
  if (j.contains("accessMode"))
    accessMode = static_cast<FifoAccessMode>(j["accessMode"]);
  if (j.contains("unixPipePath"))
    unixPipePath = j["unixPipePath"];
  if (j.contains("windowsPipePath"))
    windowsPipePath = j["windowsPipePath"];

  if (j.contains("framingMode"))
    framingMode = static_cast<FifoFramingMode>(j["framingMode"]);
  if (j.contains("customDelimiter"))
    customDelimiter = j["customDelimiter"];
  if (j.contains("lineTerminator"))
    lineTerminator = j["lineTerminator"];
  if (j.contains("messagePrefix"))
    messagePrefix = j["messagePrefix"];
  if (j.contains("messageSuffix"))
    messageSuffix = j["messageSuffix"];

  if (j.contains("bufferSize"))
    bufferSize = j["bufferSize"];
  if (j.contains("maxMessageSize"))
    maxMessageSize = j["maxMessageSize"];
  if (j.contains("readBufferSize"))
    readBufferSize = j["readBufferSize"];
  if (j.contains("writeBufferSize"))
    writeBufferSize = j["writeBufferSize"];
  if (j.contains("maxQueueSize"))
    maxQueueSize = j["maxQueueSize"];

  if (j.contains("connectTimeout"))
    connectTimeout = std::chrono::milliseconds(j["connectTimeout"]);
  if (j.contains("readTimeout"))
    readTimeout = std::chrono::milliseconds(j["readTimeout"]);
  if (j.contains("writeTimeout"))
    writeTimeout = std::chrono::milliseconds(j["writeTimeout"]);
  if (j.contains("reconnectDelay"))
    reconnectDelay = std::chrono::milliseconds(j["reconnectDelay"]);
  if (j.contains("keepAliveInterval"))
    keepAliveInterval = std::chrono::milliseconds(j["keepAliveInterval"]);

  if (j.contains("maxReconnectAttempts"))
    maxReconnectAttempts = j["maxReconnectAttempts"];
  if (j.contains("enableAutoReconnect"))
    enableAutoReconnect = j["enableAutoReconnect"];
  if (j.contains("enableKeepAlive"))
    enableKeepAlive = j["enableKeepAlive"];
  if (j.contains("enableNonBlocking"))
    enableNonBlocking = j["enableNonBlocking"];
  if (j.contains("enableBidirectional"))
    enableBidirectional = j["enableBidirectional"];

  if (j.contains("pipePermissions"))
    pipePermissions = j["pipePermissions"];
  if (j.contains("pipeOwner"))
    pipeOwner = j["pipeOwner"];
  if (j.contains("pipeGroup"))
    pipeGroup = j["pipeGroup"];

  if (j.contains("windowsPipeInstances"))
    windowsPipeInstances = j["windowsPipeInstances"];
  if (j.contains("windowsOutBufferSize"))
    windowsOutBufferSize = j["windowsOutBufferSize"];
  if (j.contains("windowsInBufferSize"))
    windowsInBufferSize = j["windowsInBufferSize"];
  if (j.contains("windowsDefaultTimeout"))
    windowsDefaultTimeout = j["windowsDefaultTimeout"];

  if (j.contains("enableMessageValidation"))
    enableMessageValidation = j["enableMessageValidation"];
  if (j.contains("enableMessageLogging"))
    enableMessageLogging = j["enableMessageLogging"];
  if (j.contains("enableMessageTracing"))
    enableMessageTracing = j["enableMessageTracing"];
  if (j.contains("enableBinaryMode"))
    enableBinaryMode = j["enableBinaryMode"];
  if (j.contains("enableFlowControl"))
    enableFlowControl = j["enableFlowControl"];
  if (j.contains("enableBackpressure"))
    enableBackpressure = j["enableBackpressure"];

  if (j.contains("compressionType"))
    compressionType = static_cast<FifoCompressionType>(j["compressionType"]);
  if (j.contains("compressionLevel"))
    compressionLevel = j["compressionLevel"];
  if (j.contains("compressionThreshold"))
    compressionThreshold = j["compressionThreshold"];
  if (j.contains("enableCompressionForSmallMessages"))
    enableCompressionForSmallMessages = j["enableCompressionForSmallMessages"];

  if (j.contains("authMethod"))
    authMethod = static_cast<FifoAuthMethod>(j["authMethod"]);
  if (j.contains("authToken"))
    authToken = j["authToken"];
  if (j.contains("certificatePath"))
    certificatePath = j["certificatePath"];
  if (j.contains("privateKeyPath"))
    privateKeyPath = j["privateKeyPath"];
  if (j.contains("enableEncryption"))
    enableEncryption = j["enableEncryption"];
  if (j.contains("encryptionKey"))
    encryptionKey = j["encryptionKey"];

  if (j.contains("enableCircuitBreaker"))
    enableCircuitBreaker = j["enableCircuitBreaker"];
  if (j.contains("circuitBreakerThreshold"))
    circuitBreakerThreshold = j["circuitBreakerThreshold"];
  if (j.contains("circuitBreakerTimeout"))
    circuitBreakerTimeout =
        std::chrono::milliseconds(j["circuitBreakerTimeout"]);
  if (j.contains("enableRetryOnError"))
    enableRetryOnError = j["enableRetryOnError"];
  if (j.contains("maxRetryAttempts"))
    maxRetryAttempts = j["maxRetryAttempts"];
  if (j.contains("retryDelay"))
    retryDelay = std::chrono::milliseconds(j["retryDelay"]);

  if (j.contains("enablePerformanceMetrics"))
    enablePerformanceMetrics = j["enablePerformanceMetrics"];
  if (j.contains("enableHealthChecking"))
    enableHealthChecking = j["enableHealthChecking"];
  if (j.contains("healthCheckInterval"))
    healthCheckInterval = std::chrono::milliseconds(j["healthCheckInterval"]);
  if (j.contains("enableDebugLogging"))
    enableDebugLogging = j["enableDebugLogging"];
  if (j.contains("logLevel"))
    logLevel = j["logLevel"];

  if (j.contains("enableMultiplexing"))
    enableMultiplexing = j["enableMultiplexing"];
  if (j.contains("maxConcurrentConnections"))
    maxConcurrentConnections = j["maxConcurrentConnections"];
  if (j.contains("enableMessagePrioritization"))
    enableMessagePrioritization = j["enableMessagePrioritization"];
  if (j.contains("enableMessageDeduplication"))
    enableMessageDeduplication = j["enableMessageDeduplication"];
  if (j.contains("deduplicationWindow"))
    deduplicationWindow = std::chrono::milliseconds(j["deduplicationWindow"]);

  if (j.contains("enablePlatformOptimizations"))
    enablePlatformOptimizations = j["enablePlatformOptimizations"];
  if (j.contains("useMemoryMappedFiles"))
    useMemoryMappedFiles = j["useMemoryMappedFiles"];
  if (j.contains("enableZeroCopy"))
    enableZeroCopy = j["enableZeroCopy"];
  if (j.contains("ioVectorSize"))
    ioVectorSize = j["ioVectorSize"];
}

bool FifoConfig::validate() const {
  // Basic validation
  if (pipeName.empty())
    return false;
  if (bufferSize == 0 || maxMessageSize == 0)
    return false;
  if (readBufferSize == 0 || writeBufferSize == 0)
    return false;
  if (maxQueueSize == 0)
    return false;

  // Timeout validation
  if (connectTimeout.count() <= 0 || readTimeout.count() <= 0 ||
      writeTimeout.count() <= 0)
    return false;

  // Path validation
  if (unixPipePath.empty() && windowsPipePath.empty())
    return false;

  return true;
}

std::string FifoConfig::toString() const { return toJson().dump(2); }

// FifoConfigManager implementation
FifoConfigManager::FifoConfigManager() {
  // Initialize any required resources
}

FifoConfig FifoConfigManager::createConfig(ConfigPreset preset) const {
  switch (preset) {
  case ConfigPreset::DEFAULT:
    return createDefaultConfig();
  case ConfigPreset::HIGH_PERFORMANCE:
    return createHighPerformanceConfig();
  case ConfigPreset::LOW_LATENCY:
    return createLowLatencyConfig();
  case ConfigPreset::RELIABLE:
    return createReliableConfig();
  case ConfigPreset::SECURE:
    return createSecureConfig();
  case ConfigPreset::DEBUG:
    return createDebugConfig();
  case ConfigPreset::EMBEDDED:
    return createEmbeddedConfig();
  case ConfigPreset::BIDIRECTIONAL:
    return createBidirectionalConfig();
  case ConfigPreset::BROADCAST:
    return createBroadcastConfig();
  case ConfigPreset::STREAMING:
    return createStreamingConfig();
  default:
    return createDefaultConfig();
  }
}

FifoConfig
FifoConfigManager::createCustomConfig(const json &customSettings) const {
  FifoConfig config = createDefaultConfig();
  config.fromJson(customSettings);
  return config;
}

FifoConfig FifoConfigManager::mergeConfigs(const FifoConfig &base,
                                           const FifoConfig &override) const {
  FifoConfig merged = base;
  json overrideJson = override.toJson();
  merged.fromJson(overrideJson);
  return merged;
}

FifoConfigValidationResult
FifoConfigManager::validateConfig(const FifoConfig &config) const {
  FifoConfigValidationResult result;

  // Basic validation
  if (config.pipeName.empty()) {
    result.errors.push_back("Pipe name cannot be empty");
    result.isValid = false;
  }

  if (config.bufferSize == 0) {
    result.errors.push_back("Buffer size must be greater than 0");
    result.isValid = false;
  }

  if (config.maxMessageSize == 0) {
    result.errors.push_back("Max message size must be greater than 0");
    result.isValid = false;
  }

  if (config.maxMessageSize > config.bufferSize * 100) {
    result.warnings.push_back("Max message size is much larger than buffer "
                              "size, may cause performance issues");
  }

  // Platform-specific validation
  if (!validatePlatformCompatibility(config)) {
    result.errors.push_back(
        "Configuration is not compatible with current platform");
    result.isValid = false;
  }

  // Path validation
  if (!validatePipePath(config.unixPipePath) &&
      !validatePipePath(config.windowsPipePath)) {
    result.errors.push_back("Invalid pipe paths specified");
    result.isValid = false;
  }

  // Buffer size validation
  if (!validateBufferSizes(config)) {
    result.errors.push_back("Invalid buffer size configuration");
    result.isValid = false;
  }

  // Timeout validation
  if (!validateTimeouts(config)) {
    result.errors.push_back("Invalid timeout configuration");
    result.isValid = false;
  }

  // Generate summary
  if (result.isValid) {
    result.summary = "Configuration is valid";
  } else {
    result.summary =
        "Configuration has " + std::to_string(result.errors.size()) + " errors";
  }

  return result;
}

FifoConfig FifoConfigManager::createDefaultConfig() const {
  FifoConfig config;

  // Set platform-appropriate paths
  if (isWindows()) {
    config.pipeType = FifoPipeType::WINDOWS_NAMED_PIPE;
    config.windowsPipePath = "\\\\.\\pipe\\hydrogen_fifo";
  } else {
    config.pipeType = FifoPipeType::UNIX_FIFO;
    config.unixPipePath = "/tmp/hydrogen_fifo";
  }

  // Balanced settings for general use
  config.framingMode = FifoFramingMode::JSON_LINES;
  config.accessMode = FifoAccessMode::READ_WRITE;
  config.bufferSize = 8192;
  config.maxMessageSize = 1024 * 1024; // 1MB
  config.connectTimeout = std::chrono::milliseconds(5000);
  config.readTimeout = std::chrono::milliseconds(1000);
  config.writeTimeout = std::chrono::milliseconds(1000);
  config.enableAutoReconnect = true;
  config.maxReconnectAttempts = 5;
  config.enableMessageValidation = true;

  return config;
}

FifoConfig FifoConfigManager::createHighPerformanceConfig() const {
  FifoConfig config = createDefaultConfig();

  // Optimize for throughput
  config.bufferSize = 65536; // 64KB
  config.readBufferSize = 32768;
  config.writeBufferSize = 32768;
  config.maxQueueSize = 10000;
  config.enableNonBlocking = true;
  config.enableFlowControl = true;
  config.enableBackpressure = true;
  config.enablePlatformOptimizations = true;
  config.enableZeroCopy = true;
  config.ioVectorSize = 32;
  config.maxConcurrentConnections = 10;
  config.enableMultiplexing = true;

  // Reduce validation overhead
  config.enableMessageValidation = false;
  config.enableMessageLogging = false;

  return config;
}

FifoConfig FifoConfigManager::createLowLatencyConfig() const {
  FifoConfig config = createDefaultConfig();

  // Optimize for minimal latency
  config.bufferSize = 4096; // Smaller buffers
  config.readBufferSize = 2048;
  config.writeBufferSize = 2048;
  config.readTimeout = std::chrono::milliseconds(100);
  config.writeTimeout = std::chrono::milliseconds(100);
  config.enableNonBlocking = true;
  config.enableFlowControl = false; // Reduce overhead
  config.enableMessageValidation = false;
  config.enablePlatformOptimizations = true;
  config.enableZeroCopy = true;

  // Immediate processing
  config.maxQueueSize = 100;
  config.enableBackpressure = false;

  return config;
}

FifoConfig FifoConfigManager::createReliableConfig() const {
  FifoConfig config = createDefaultConfig();

  // Optimize for reliability
  config.enableAutoReconnect = true;
  config.maxReconnectAttempts = 10;
  config.reconnectDelay = std::chrono::milliseconds(2000);
  config.enableKeepAlive = true;
  config.keepAliveInterval = std::chrono::milliseconds(15000);
  config.enableCircuitBreaker = true;
  config.circuitBreakerThreshold = 3;
  config.circuitBreakerTimeout = std::chrono::milliseconds(60000);
  config.enableRetryOnError = true;
  config.maxRetryAttempts = 5;
  config.retryDelay = std::chrono::milliseconds(1000);
  config.enableMessageValidation = true;
  config.enableMessageDeduplication = true;
  config.deduplicationWindow = std::chrono::milliseconds(10000);
  config.enableHealthChecking = true;
  config.healthCheckInterval = std::chrono::milliseconds(5000);

  return config;
}

FifoConfig FifoConfigManager::createSecureConfig() const {
  FifoConfig config = createDefaultConfig();

  // Security-focused settings
  config.authMethod = FifoAuthMethod::TOKEN_BASED;
  config.enableEncryption = true;
  config.enableMessageValidation = true;
  config.enableMessageLogging = true; // For audit trails
  config.enableMessageTracing = true;

  // Restrictive permissions (Unix)
  config.pipePermissions = 0600; // Owner read/write only

  // Windows ACL would be configured separately
  config.windowsPipeInstances = 1; // Limit connections
  config.maxConcurrentConnections = 1;

  return config;
}

FifoConfig FifoConfigManager::createDebugConfig() const {
  FifoConfig config = createDefaultConfig();

  // Debug and development settings
  config.enableDebugLogging = true;
  config.logLevel = "DEBUG";
  config.enableMessageLogging = true;
  config.enableMessageTracing = true;
  config.enablePerformanceMetrics = true;
  config.enableHealthChecking = true;
  config.healthCheckInterval = std::chrono::milliseconds(2000);

  // Longer timeouts for debugging
  config.connectTimeout = std::chrono::milliseconds(30000);
  config.readTimeout = std::chrono::milliseconds(10000);
  config.writeTimeout = std::chrono::milliseconds(10000);

  // Smaller buffers for easier debugging
  config.bufferSize = 1024;
  config.maxMessageSize = 10240; // 10KB
  config.maxQueueSize = 100;

  return config;
}

FifoConfig FifoConfigManager::createEmbeddedConfig() const {
  FifoConfig config = createDefaultConfig();

  // Resource-constrained settings
  config.bufferSize = 1024; // 1KB
  config.readBufferSize = 512;
  config.writeBufferSize = 512;
  config.maxMessageSize = 4096; // 4KB
  config.maxQueueSize = 50;
  config.maxReconnectAttempts = 3;
  config.enablePerformanceMetrics = false;
  config.enableMessageLogging = false;
  config.enableMessageTracing = false;
  config.enableDebugLogging = false;
  config.enableMultiplexing = false;
  config.maxConcurrentConnections = 1;
  config.enablePlatformOptimizations = false;
  config.enableZeroCopy = false;
  config.useMemoryMappedFiles = false;

  return config;
}

FifoConfig FifoConfigManager::createBidirectionalConfig() const {
  FifoConfig config = createDefaultConfig();

  // Full duplex communication
  config.accessMode = FifoAccessMode::DUPLEX;
  config.enableBidirectional = true;
  config.bufferSize = 16384; // Larger buffers for duplex
  config.readBufferSize = 8192;
  config.writeBufferSize = 8192;
  config.maxQueueSize = 1000;
  config.enableFlowControl = true;
  config.enableBackpressure = true;

  return config;
}

FifoConfig FifoConfigManager::createBroadcastConfig() const {
  FifoConfig config = createDefaultConfig();

  // One-to-many communication
  config.accessMode = FifoAccessMode::WRITE_ONLY;
  config.maxConcurrentConnections = 100;
  config.enableMultiplexing = true;
  config.bufferSize = 32768; // Large buffers for broadcast
  config.writeBufferSize = 16384;
  config.maxQueueSize = 5000;
  config.enableBackpressure = false;         // Don't block on slow readers
  config.enableMessageDeduplication = false; // Each client gets all messages

  return config;
}

FifoConfig FifoConfigManager::createStreamingConfig() const {
  FifoConfig config = createDefaultConfig();

  // Continuous data streaming
  config.framingMode = FifoFramingMode::LENGTH_PREFIXED;
  config.bufferSize = 131072; // 128KB
  config.readBufferSize = 65536;
  config.writeBufferSize = 65536;
  config.maxMessageSize = 10 * 1024 * 1024; // 10MB
  config.maxQueueSize = 10000;
  config.enableNonBlocking = true;
  config.enableFlowControl = true;
  config.enableBackpressure = true;
  config.enableZeroCopy = true;
  config.useMemoryMappedFiles = true;
  config.ioVectorSize = 64;
  config.enableMessageValidation = false; // Reduce overhead for streaming

  return config;
}

// Platform detection methods
bool FifoConfigManager::isWindows() const {
#ifdef _WIN32
  return true;
#else
  return false;
#endif
}

bool FifoConfigManager::isUnix() const { return !isWindows(); }

std::string FifoConfigManager::getCurrentPlatform() const {
#ifdef _WIN32
  return "Windows";
#elif defined(__linux__)
  return "Linux";
#elif defined(__APPLE__)
  return "macOS";
#elif defined(__unix__)
  return "Unix";
#else
  return "Unknown";
#endif
}

// Validation helper methods
bool FifoConfigManager::validatePipePath(const std::string &path) const {
  if (path.empty())
    return false;

  // Basic path validation
  if (isWindows()) {
    return path.find("\\\\.\\pipe\\") == 0;
  } else {
    return path[0] == '/' && path.find("..") == std::string::npos;
  }
}

bool FifoConfigManager::validateBufferSizes(const FifoConfig &config) const {
  return config.bufferSize > 0 && config.readBufferSize > 0 &&
         config.writeBufferSize > 0 && config.maxMessageSize > 0 &&
         config.maxQueueSize > 0;
}

bool FifoConfigManager::validateTimeouts(const FifoConfig &config) const {
  return config.connectTimeout.count() > 0 && config.readTimeout.count() > 0 &&
         config.writeTimeout.count() > 0 && config.reconnectDelay.count() >= 0;
}

bool FifoConfigManager::validatePermissions(const FifoConfig &config) const {
  if (isUnix()) {
    // Unix permissions should be valid octal
    return config.pipePermissions <= 0777;
  }
  return true; // Windows permissions handled differently
}

bool FifoConfigManager::validatePlatformCompatibility(
    const FifoConfig &config) const {
  if (isWindows() && config.pipeType == FifoPipeType::UNIX_FIFO) {
    return false;
  }
  if (isUnix() && config.pipeType == FifoPipeType::WINDOWS_NAMED_PIPE) {
    return false;
  }
  return true;
}

// Configuration optimization
FifoConfig FifoConfigManager::optimizeConfig(const FifoConfig &config) const {
  FifoConfig optimized = config;

  // Platform-specific optimizations
  if (isWindows()) {
    // Windows optimizations
    if (optimized.pipeType == FifoPipeType::AUTO_DETECT) {
      optimized.pipeType = FifoPipeType::WINDOWS_NAMED_PIPE;
    }
    optimized.windowsPipePath = getPlatformDefaultPipePath(optimized.pipeName);
    optimized.enablePlatformOptimizations = true;
  } else {
    // Unix optimizations
    if (optimized.pipeType == FifoPipeType::AUTO_DETECT) {
      optimized.pipeType = FifoPipeType::UNIX_FIFO;
    }
    optimized.unixPipePath = getPlatformDefaultPipePath(optimized.pipeName);
    optimized.enablePlatformOptimizations = true;
  }

  // Buffer size optimizations
  if (optimized.bufferSize < 4096) {
    optimized.bufferSize = 4096;
  }
  if (optimized.readBufferSize < 2048) {
    optimized.readBufferSize = 2048;
  }
  if (optimized.writeBufferSize < 2048) {
    optimized.writeBufferSize = 2048;
  }

  // Timeout optimizations
  if (optimized.readTimeout.count() < 100) {
    optimized.readTimeout = std::chrono::milliseconds(100);
  }
  if (optimized.writeTimeout.count() < 100) {
    optimized.writeTimeout = std::chrono::milliseconds(100);
  }
  if (optimized.connectTimeout.count() < 1000) {
    optimized.connectTimeout = std::chrono::milliseconds(1000);
  }

  return optimized;
}

FifoConfig FifoConfigManager::createPlatformOptimizedConfig(ConfigPreset preset) const {
  FifoConfig config = createConfig(preset);
  return optimizeConfig(config);
}

// Configuration persistence
bool FifoConfigManager::saveConfig(const FifoConfig &config, const std::string &filePath) const {
  try {
    json j = config.toJson();
    std::ofstream file(filePath);
    if (!file.is_open()) {
      return false;
    }
    file << std::setw(2) << j << std::endl;
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

FifoConfig FifoConfigManager::loadConfig(const std::string &filePath) const {
  try {
    std::ifstream file(filePath);
    if (!file.is_open()) {
      return createDefaultConfig();
    }
    json j;
    file >> j;
    FifoConfig config;
    config.fromJson(j);
    return config;
  } catch (const std::exception &) {
    return createDefaultConfig();
  }
}

// Preset management
std::vector<FifoConfigManager::ConfigPreset> FifoConfigManager::getAvailablePresets() const {
  return {
    ConfigPreset::DEFAULT,
    ConfigPreset::HIGH_PERFORMANCE,
    ConfigPreset::LOW_LATENCY,
    ConfigPreset::RELIABLE,
    ConfigPreset::SECURE,
    ConfigPreset::DEBUG,
    ConfigPreset::EMBEDDED,
    ConfigPreset::BIDIRECTIONAL,
    ConfigPreset::BROADCAST,
    ConfigPreset::STREAMING
  };
}

std::string FifoConfigManager::getPresetDescription(ConfigPreset preset) const {
  switch (preset) {
    case ConfigPreset::DEFAULT:
      return "Balanced configuration suitable for most applications";
    case ConfigPreset::HIGH_PERFORMANCE:
      return "Optimized for maximum throughput and performance";
    case ConfigPreset::LOW_LATENCY:
      return "Minimizes communication latency";
    case ConfigPreset::RELIABLE:
      return "Enhanced reliability with error recovery";
    case ConfigPreset::SECURE:
      return "Security-focused with authentication and encryption";
    case ConfigPreset::DEBUG:
      return "Debug-friendly with extensive logging";
    case ConfigPreset::EMBEDDED:
      return "Resource-constrained embedded systems";
    case ConfigPreset::BIDIRECTIONAL:
      return "Full bidirectional communication support";
    case ConfigPreset::BROADCAST:
      return "One-to-many broadcast communication";
    case ConfigPreset::STREAMING:
      return "Optimized for streaming data";
    default:
      return "Unknown preset";
  }
}

FifoConfig FifoConfigManager::getPresetConfig(ConfigPreset preset) const {
  return createConfig(preset);
}

// Configuration comparison and analysis
json FifoConfigManager::compareConfigs(const FifoConfig &config1, const FifoConfig &config2) const {
  json comparison;
  json config1Json = config1.toJson();
  json config2Json = config2.toJson();

  comparison["config1"] = config1Json;
  comparison["config2"] = config2Json;
  comparison["differences"] = json::object();

  for (auto it = config1Json.begin(); it != config1Json.end(); ++it) {
    const std::string& key = it.key();
    if (config2Json.contains(key)) {
      if (config1Json[key] != config2Json[key]) {
        comparison["differences"][key] = {
          {"config1", config1Json[key]},
          {"config2", config2Json[key]}
        };
      }
    } else {
      comparison["differences"][key] = {
        {"config1", config1Json[key]},
        {"config2", nullptr}
      };
    }
  }

  for (auto it = config2Json.begin(); it != config2Json.end(); ++it) {
    const std::string& key = it.key();
    if (!config1Json.contains(key)) {
      comparison["differences"][key] = {
        {"config1", nullptr},
        {"config2", config2Json[key]}
      };
    }
  }

  return comparison;
}

std::vector<std::string> FifoConfigManager::getConfigDifferences(const FifoConfig &config1, const FifoConfig &config2) const {
  std::vector<std::string> differences;
  json comparison = compareConfigs(config1, config2);

  if (comparison.contains("differences")) {
    for (auto it = comparison["differences"].begin(); it != comparison["differences"].end(); ++it) {
      differences.push_back(it.key());
    }
  }

  return differences;
}

// Runtime configuration updates
bool FifoConfigManager::updateConfig(FifoConfig &config, const json &updates) const {
  try {
    // Pre-validate the updates for reasonable values
    for (const auto& [key, value] : updates.items()) {
      if (key == "bufferSize" || key == "maxMessageSize" ||
          key == "readBufferSize" || key == "writeBufferSize" ||
          key == "maxQueueSize") {
        if (value.is_number_integer()) {
          int64_t intValue = value.get<int64_t>();
          if (intValue < 0 || intValue > 1000000000) { // 1GB max
            return false;
          }
        }
      }
    }

    json configJson = config.toJson();
    configJson.update(updates);
    config.fromJson(configJson);
    return validateConfig(config).isValid;
  } catch (const std::exception &) {
    return false;
  }
}

json FifoConfigManager::getConfigSchema() const {
  json schema;
  schema["type"] = "object";
  schema["properties"] = {
    {"pipeName", {{"type", "string"}, {"description", "Name of the FIFO pipe"}}},
    {"pipeDirectory", {{"type", "string"}, {"description", "Directory for pipe files"}}},
    {"pipeType", {{"type", "integer"}, {"description", "Type of pipe (0=Auto, 1=Unix, 2=Windows)"}}},
    {"accessMode", {{"type", "integer"}, {"description", "Access mode for the pipe"}}},
    {"bufferSize", {{"type", "integer"}, {"minimum", 1024}, {"description", "Buffer size in bytes"}}},
    {"maxMessageSize", {{"type", "integer"}, {"minimum", 1}, {"description", "Maximum message size"}}},
    {"readTimeout", {{"type", "integer"}, {"minimum", 0}, {"description", "Read timeout in milliseconds"}}},
    {"writeTimeout", {{"type", "integer"}, {"minimum", 0}, {"description", "Write timeout in milliseconds"}}},
    {"enableNonBlocking", {{"type", "boolean"}, {"description", "Enable non-blocking I/O"}}},
    {"enableFlowControl", {{"type", "boolean"}, {"description", "Enable flow control"}}}
  };
  schema["required"] = {"pipeName", "pipeType", "bufferSize", "maxMessageSize"};
  return schema;
}

// Platform-specific methods
std::string FifoConfigManager::getPlatformDefaultPipePath(const std::string &pipeName) const {
  if (isWindows()) {
    return "\\\\.\\pipe\\" + pipeName;
  } else {
    return "/tmp/" + pipeName;
  }
}

// Global instance
FifoConfigManager &getGlobalFifoConfigManager() {
  static FifoConfigManager instance;
  return instance;
}

} // namespace core
} // namespace hydrogen

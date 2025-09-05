#include "hydrogen/core/stdio_config_manager.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <spdlog/spdlog.h>

namespace hydrogen {
namespace core {

StdioConfigManager::StdioConfigManager() {
  // Initialize with default presets
  spdlog::debug("StdioConfigManager initialized");
}

StdioConfig StdioConfigManager::createConfig(ConfigPreset preset) const {
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
  case ConfigPreset::CUSTOM:
  default:
    return createDefaultConfig();
  }
}

bool StdioConfigManager::validateConfig(const StdioConfig &config) const {
  std::string error;
  return validateBasicSettings(config, error) &&
         validateFramingSettings(config, error) &&
         validateCompressionSettings(config, error) &&
         validateSecuritySettings(config, error) &&
         validateConnectionSettings(config, error) &&
         validatePerformanceSettings(config, error);
}

std::string
StdioConfigManager::getValidationError(const StdioConfig &config) const {
  std::string error;

  if (!validateBasicSettings(config, error))
    return error;
  if (!validateFramingSettings(config, error))
    return error;
  if (!validateCompressionSettings(config, error))
    return error;
  if (!validateSecuritySettings(config, error))
    return error;
  if (!validateConnectionSettings(config, error))
    return error;
  if (!validatePerformanceSettings(config, error))
    return error;

  return "";
}

json StdioConfigManager::configToJson(const StdioConfig &config) const {
  json j;

  // Basic I/O settings
  j["enableLineBuffering"] = config.enableLineBuffering;
  j["enableBinaryMode"] = config.enableBinaryMode;
  j["readTimeout"] = config.readTimeout.count();
  j["writeTimeout"] = config.writeTimeout.count();
  j["bufferSize"] = config.bufferSize;
  j["lineTerminator"] = config.lineTerminator;
  j["enableEcho"] = config.enableEcho;
  j["enableFlush"] = config.enableFlush;
  j["encoding"] = config.encoding;
  j["enableErrorRedirection"] = config.enableErrorRedirection;

  // Message framing
  j["framingMode"] = framingModeToJson(config.framingMode);
  j["frameDelimiter"] = config.frameDelimiter;
  j["enableFrameEscaping"] = config.enableFrameEscaping;
  j["escapeSequence"] = config.escapeSequence;
  j["maxFrameSize"] = config.maxFrameSize;

  // Compression
  j["enableCompression"] = config.enableCompression;
  j["compressionType"] = compressionTypeToJson(config.compressionType);
  j["compressionLevel"] = config.compressionLevel;
  j["compressionThreshold"] = config.compressionThreshold;

  // Authentication and security
  j["enableAuthentication"] = config.enableAuthentication;
  j["authenticationMethod"] = config.authenticationMethod;
  j["authToken"] = config.authToken;
  j["username"] = config.username;
  j["password"] = config.password;
  j["enableEncryption"] = config.enableEncryption;
  j["encryptionKey"] = config.encryptionKey;
  j["encryptionAlgorithm"] = config.encryptionAlgorithm;

  // Connection management
  j["enableHeartbeat"] = config.enableHeartbeat;
  j["heartbeatInterval"] = config.heartbeatInterval.count();
  j["heartbeatTimeout"] = config.heartbeatTimeout.count();
  j["enableReconnect"] = config.enableReconnect;
  j["maxReconnectAttempts"] = config.maxReconnectAttempts;
  j["reconnectDelay"] = config.reconnectDelay.count();
  j["maxReconnectDelay"] = config.maxReconnectDelay.count();
  j["enableBackoffStrategy"] = config.enableBackoffStrategy;

  // Flow control
  j["enableFlowControl"] = config.enableFlowControl;
  j["sendBufferSize"] = config.sendBufferSize;
  j["receiveBufferSize"] = config.receiveBufferSize;
  j["maxPendingMessages"] = config.maxPendingMessages;
  j["enableBackpressure"] = config.enableBackpressure;

  // Protocol-specific settings
  j["enableMessageValidation"] = config.enableMessageValidation;
  j["enableMessageTransformation"] = config.enableMessageTransformation;
  j["enableMessageLogging"] = config.enableMessageLogging;
  j["logLevel"] = config.logLevel;
  j["enableMetrics"] = config.enableMetrics;
  j["enableTracing"] = config.enableTracing;

  // Performance tuning
  j["ioThreads"] = config.ioThreads;
  j["enableNonBlockingIO"] = config.enableNonBlockingIO;
  j["enableZeroCopy"] = config.enableZeroCopy;
  j["readChunkSize"] = config.readChunkSize;
  j["writeChunkSize"] = config.writeChunkSize;

  // Error handling
  j["enableErrorRecovery"] = config.enableErrorRecovery;
  j["maxConsecutiveErrors"] = config.maxConsecutiveErrors;
  j["errorResetInterval"] = config.errorResetInterval.count();
  j["enableCircuitBreaker"] = config.enableCircuitBreaker;
  j["circuitBreakerThreshold"] = config.circuitBreakerThreshold;
  j["circuitBreakerTimeout"] = config.circuitBreakerTimeout.count();

  return j;
}

StdioConfig StdioConfigManager::configFromJson(const json &j) const {
  StdioConfig config;

  // Basic I/O settings
  if (j.contains("enableLineBuffering"))
    config.enableLineBuffering = j["enableLineBuffering"];
  if (j.contains("enableBinaryMode"))
    config.enableBinaryMode = j["enableBinaryMode"];
  if (j.contains("readTimeout"))
    config.readTimeout = std::chrono::milliseconds(j["readTimeout"]);
  if (j.contains("writeTimeout"))
    config.writeTimeout = std::chrono::milliseconds(j["writeTimeout"]);
  if (j.contains("bufferSize"))
    config.bufferSize = j["bufferSize"];
  if (j.contains("lineTerminator"))
    config.lineTerminator = j["lineTerminator"];
  if (j.contains("enableEcho"))
    config.enableEcho = j["enableEcho"];
  if (j.contains("enableFlush"))
    config.enableFlush = j["enableFlush"];
  if (j.contains("encoding"))
    config.encoding = j["encoding"];
  if (j.contains("enableErrorRedirection"))
    config.enableErrorRedirection = j["enableErrorRedirection"];

  // Message framing
  if (j.contains("framingMode"))
    config.framingMode = framingModeFromJson(j["framingMode"]);
  if (j.contains("frameDelimiter"))
    config.frameDelimiter = j["frameDelimiter"];
  if (j.contains("enableFrameEscaping"))
    config.enableFrameEscaping = j["enableFrameEscaping"];
  if (j.contains("escapeSequence"))
    config.escapeSequence = j["escapeSequence"];
  if (j.contains("maxFrameSize"))
    config.maxFrameSize = j["maxFrameSize"];

  // Compression
  if (j.contains("enableCompression"))
    config.enableCompression = j["enableCompression"];
  if (j.contains("compressionType"))
    config.compressionType = compressionTypeFromJson(j["compressionType"]);
  if (j.contains("compressionLevel"))
    config.compressionLevel = j["compressionLevel"];
  if (j.contains("compressionThreshold"))
    config.compressionThreshold = j["compressionThreshold"];

  // Authentication and security
  if (j.contains("enableAuthentication"))
    config.enableAuthentication = j["enableAuthentication"];
  if (j.contains("authenticationMethod"))
    config.authenticationMethod = j["authenticationMethod"];
  if (j.contains("authToken"))
    config.authToken = j["authToken"];
  if (j.contains("username"))
    config.username = j["username"];
  if (j.contains("password"))
    config.password = j["password"];
  if (j.contains("enableEncryption"))
    config.enableEncryption = j["enableEncryption"];
  if (j.contains("encryptionKey"))
    config.encryptionKey = j["encryptionKey"];
  if (j.contains("encryptionAlgorithm"))
    config.encryptionAlgorithm = j["encryptionAlgorithm"];

  // Connection management
  if (j.contains("enableHeartbeat"))
    config.enableHeartbeat = j["enableHeartbeat"];
  if (j.contains("heartbeatInterval"))
    config.heartbeatInterval =
        std::chrono::milliseconds(j["heartbeatInterval"]);
  if (j.contains("heartbeatTimeout"))
    config.heartbeatTimeout = std::chrono::milliseconds(j["heartbeatTimeout"]);
  if (j.contains("enableReconnect"))
    config.enableReconnect = j["enableReconnect"];
  if (j.contains("maxReconnectAttempts"))
    config.maxReconnectAttempts = j["maxReconnectAttempts"];
  if (j.contains("reconnectDelay"))
    config.reconnectDelay = std::chrono::milliseconds(j["reconnectDelay"]);
  if (j.contains("maxReconnectDelay"))
    config.maxReconnectDelay =
        std::chrono::milliseconds(j["maxReconnectDelay"]);
  if (j.contains("enableBackoffStrategy"))
    config.enableBackoffStrategy = j["enableBackoffStrategy"];

  // Flow control
  if (j.contains("enableFlowControl"))
    config.enableFlowControl = j["enableFlowControl"];
  if (j.contains("sendBufferSize"))
    config.sendBufferSize = j["sendBufferSize"];
  if (j.contains("receiveBufferSize"))
    config.receiveBufferSize = j["receiveBufferSize"];
  if (j.contains("maxPendingMessages"))
    config.maxPendingMessages = j["maxPendingMessages"];
  if (j.contains("enableBackpressure"))
    config.enableBackpressure = j["enableBackpressure"];

  // Protocol-specific settings
  if (j.contains("enableMessageValidation"))
    config.enableMessageValidation = j["enableMessageValidation"];
  if (j.contains("enableMessageTransformation"))
    config.enableMessageTransformation = j["enableMessageTransformation"];
  if (j.contains("enableMessageLogging"))
    config.enableMessageLogging = j["enableMessageLogging"];
  if (j.contains("logLevel"))
    config.logLevel = j["logLevel"];
  if (j.contains("enableMetrics"))
    config.enableMetrics = j["enableMetrics"];
  if (j.contains("enableTracing"))
    config.enableTracing = j["enableTracing"];

  // Performance tuning
  if (j.contains("ioThreads"))
    config.ioThreads = j["ioThreads"];
  if (j.contains("enableNonBlockingIO"))
    config.enableNonBlockingIO = j["enableNonBlockingIO"];
  if (j.contains("enableZeroCopy"))
    config.enableZeroCopy = j["enableZeroCopy"];
  if (j.contains("readChunkSize"))
    config.readChunkSize = j["readChunkSize"];
  if (j.contains("writeChunkSize"))
    config.writeChunkSize = j["writeChunkSize"];

  // Error handling
  if (j.contains("enableErrorRecovery"))
    config.enableErrorRecovery = j["enableErrorRecovery"];
  if (j.contains("maxConsecutiveErrors"))
    config.maxConsecutiveErrors = j["maxConsecutiveErrors"];
  if (j.contains("errorResetInterval"))
    config.errorResetInterval =
        std::chrono::milliseconds(j["errorResetInterval"]);
  if (j.contains("enableCircuitBreaker"))
    config.enableCircuitBreaker = j["enableCircuitBreaker"];
  if (j.contains("circuitBreakerThreshold"))
    config.circuitBreakerThreshold = j["circuitBreakerThreshold"];
  if (j.contains("circuitBreakerTimeout"))
    config.circuitBreakerTimeout =
        std::chrono::milliseconds(j["circuitBreakerTimeout"]);

  return config;
}

bool StdioConfigManager::saveConfigToFile(const StdioConfig &config,
                                          const std::string &filename) const {
  try {
    json j = configToJson(config);
    std::ofstream file(filename);
    file << j.dump(4);
    file.close();
    spdlog::info("Stdio configuration saved to: {}", filename);
    return true;
  } catch (const std::exception &e) {
    spdlog::error("Failed to save stdio configuration to {}: {}", filename,
                  e.what());
    return false;
  }
}

StdioConfig
StdioConfigManager::loadConfigFromFile(const std::string &filename) const {
  try {
    std::ifstream file(filename);
    json j;
    file >> j;
    file.close();
    spdlog::info("Stdio configuration loaded from: {}", filename);
    return configFromJson(j);
  } catch (const std::exception &e) {
    spdlog::error("Failed to load stdio configuration from {}: {}", filename,
                  e.what());
    return createDefaultConfig();
  }
}

// Preset creation helpers
StdioConfig StdioConfigManager::createDefaultConfig() const {
  StdioConfig config;
  // Default values are already set in the struct definition
  return config;
}

StdioConfig StdioConfigManager::createHighPerformanceConfig() const {
  StdioConfig config = createDefaultConfig();
  config.bufferSize = 64 * 1024; // 64KB
  config.enableFlowControl = true;
  config.sendBufferSize = 256 * 1024;    // 256KB
  config.receiveBufferSize = 256 * 1024; // 256KB
  config.ioThreads = 4;
  config.enableNonBlockingIO = true;
  config.readChunkSize = 16 * 1024;  // 16KB
  config.writeChunkSize = 16 * 1024; // 16KB
  config.framingMode = StdioConfig::FramingMode::LENGTH_PREFIX;
  config.enableCompression = true;
  config.compressionType = StdioConfig::CompressionType::LZ4;
  config.compressionThreshold = 512;
  return config;
}

StdioConfig StdioConfigManager::createLowLatencyConfig() const {
  StdioConfig config = createDefaultConfig();
  config.enableLineBuffering = false;
  config.enableFlush = true;
  config.bufferSize = 1024; // Small buffer
  config.readTimeout = std::chrono::milliseconds(100);
  config.writeTimeout = std::chrono::milliseconds(100);
  config.framingMode = StdioConfig::FramingMode::NONE;
  config.enableCompression = false;
  config.ioThreads = 1;
  config.enableNonBlockingIO = true;
  config.readChunkSize = 512;
  config.writeChunkSize = 512;
  return config;
}

StdioConfig StdioConfigManager::createReliableConfig() const {
  StdioConfig config = createDefaultConfig();
  config.enableHeartbeat = true;
  config.heartbeatInterval = std::chrono::milliseconds(10000);
  config.enableReconnect = true;
  config.maxReconnectAttempts = 10;
  config.enableBackoffStrategy = true;
  config.enableErrorRecovery = true;
  config.maxConsecutiveErrors = 5;
  config.enableCircuitBreaker = true;
  config.circuitBreakerThreshold = 3;
  config.enableMessageValidation = true;
  config.framingMode = StdioConfig::FramingMode::JSON_LINES;
  config.enableFrameEscaping = true;
  return config;
}

StdioConfig StdioConfigManager::createSecureConfig() const {
  StdioConfig config = createDefaultConfig();
  config.enableAuthentication = true;
  config.authenticationMethod = "token";
  config.enableEncryption = true;
  config.encryptionAlgorithm = "AES-256-GCM";
  config.enableMessageValidation = true;
  config.framingMode = StdioConfig::FramingMode::JSON_LINES;
  config.enableFrameEscaping = true;
  config.enableMessageLogging = true;
  config.logLevel = "DEBUG";
  return config;
}

StdioConfig StdioConfigManager::createDebugConfig() const {
  StdioConfig config = createDefaultConfig();
  config.enableEcho = true;
  config.enableMessageLogging = true;
  config.logLevel = "DEBUG";
  config.enableMetrics = true;
  config.enableTracing = true;
  config.framingMode = StdioConfig::FramingMode::JSON_LINES;
  config.enableMessageValidation = true;
  return config;
}

StdioConfig StdioConfigManager::createEmbeddedConfig() const {
  StdioConfig config = createDefaultConfig();
  config.bufferSize = 1024; // Small buffer for memory-constrained systems
  config.sendBufferSize = 4 * 1024;    // 4KB
  config.receiveBufferSize = 4 * 1024; // 4KB
  config.ioThreads = 1;
  config.enableCompression = false;
  config.enableEncryption = false;
  config.enableMetrics = false;
  config.enableTracing = false;
  config.maxPendingMessages = 100;
  config.framingMode = StdioConfig::FramingMode::DELIMITER;
  return config;
}

// JSON conversion helpers
json StdioConfigManager::framingModeToJson(
    StdioConfig::FramingMode mode) const {
  switch (mode) {
  case StdioConfig::FramingMode::NONE:
    return "NONE";
  case StdioConfig::FramingMode::LENGTH_PREFIX:
    return "LENGTH_PREFIX";
  case StdioConfig::FramingMode::DELIMITER:
    return "DELIMITER";
  case StdioConfig::FramingMode::JSON_LINES:
    return "JSON_LINES";
  case StdioConfig::FramingMode::CUSTOM:
    return "CUSTOM";
  default:
    return "DELIMITER";
  }
}

StdioConfig::FramingMode
StdioConfigManager::framingModeFromJson(const json &j) const {
  std::string mode = j.get<std::string>();
  if (mode == "NONE")
    return StdioConfig::FramingMode::NONE;
  if (mode == "LENGTH_PREFIX")
    return StdioConfig::FramingMode::LENGTH_PREFIX;
  if (mode == "DELIMITER")
    return StdioConfig::FramingMode::DELIMITER;
  if (mode == "JSON_LINES")
    return StdioConfig::FramingMode::JSON_LINES;
  if (mode == "CUSTOM")
    return StdioConfig::FramingMode::CUSTOM;
  return StdioConfig::FramingMode::DELIMITER;
}

json StdioConfigManager::compressionTypeToJson(
    StdioConfig::CompressionType type) const {
  switch (type) {
  case StdioConfig::CompressionType::NONE:
    return "NONE";
  case StdioConfig::CompressionType::GZIP:
    return "GZIP";
  case StdioConfig::CompressionType::ZLIB:
    return "ZLIB";
  case StdioConfig::CompressionType::LZ4:
    return "LZ4";
  default:
    return "NONE";
  }
}

StdioConfig::CompressionType
StdioConfigManager::compressionTypeFromJson(const json &j) const {
  std::string type = j.get<std::string>();
  if (type == "NONE")
    return StdioConfig::CompressionType::NONE;
  if (type == "GZIP")
    return StdioConfig::CompressionType::GZIP;
  if (type == "ZLIB")
    return StdioConfig::CompressionType::ZLIB;
  if (type == "LZ4")
    return StdioConfig::CompressionType::LZ4;
  return StdioConfig::CompressionType::NONE;
}

// Global instance
StdioConfigManager &getGlobalStdioConfigManager() {
  static StdioConfigManager instance;
  return instance;
}

} // namespace core
} // namespace hydrogen

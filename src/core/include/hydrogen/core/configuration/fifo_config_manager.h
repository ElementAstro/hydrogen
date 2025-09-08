#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief FIFO framing modes for message delimiting
 */
enum class FifoFramingMode {
  NEWLINE_DELIMITED,      // Messages separated by newlines
  LENGTH_PREFIXED,        // Messages prefixed with length
  JSON_LINES,             // JSON Lines format (JSONL)
  CUSTOM_DELIMITER,       // Custom delimiter string
  BINARY_LENGTH_PREFIXED, // Binary length prefix (4 bytes)
  NULL_TERMINATED         // Null-terminated messages
};

/**
 * @brief FIFO pipe types for cross-platform compatibility
 */
enum class FifoPipeType {
  UNIX_FIFO,          // Unix named pipe (mkfifo)
  WINDOWS_NAMED_PIPE, // Windows named pipe
  AUTO_DETECT         // Automatically detect based on platform
};

/**
 * @brief FIFO access modes
 */
enum class FifoAccessMode {
  READ_ONLY,  // Read-only access
  WRITE_ONLY, // Write-only access
  READ_WRITE, // Bidirectional access
  DUPLEX      // Full duplex using paired pipes
};

/**
 * @brief FIFO compression algorithms
 */
enum class FifoCompressionType {
  NONE,  // No compression
  GZIP,  // GZIP compression
  ZLIB,  // ZLIB compression
  LZ4,   // LZ4 fast compression
  SNAPPY // Snappy compression
};

/**
 * @brief FIFO authentication methods
 */
enum class FifoAuthMethod {
  NONE,                   // No authentication
  TOKEN_BASED,            // Token-based authentication
  CERTIFICATE,            // Certificate-based authentication
  FILESYSTEM_PERMISSIONS, // Unix filesystem permissions
  WINDOWS_ACL             // Windows Access Control Lists
};

/**
 * @brief Comprehensive FIFO configuration structure
 */
struct FifoConfig {
  // Basic pipe configuration
  std::string pipeName = "hydrogen_fifo";
  std::string pipeDirectory = "/tmp"; // Unix: /tmp, Windows: \\.\pipe
  FifoPipeType pipeType = FifoPipeType::AUTO_DETECT;
  FifoAccessMode accessMode = FifoAccessMode::READ_WRITE;

  // Cross-platform paths
  std::string unixPipePath = "/tmp/hydrogen_fifo";
  std::string windowsPipePath = "\\\\.\\pipe\\hydrogen_fifo";

  // Message framing and formatting
  FifoFramingMode framingMode = FifoFramingMode::JSON_LINES;
  std::string customDelimiter = "\n";
  std::string lineTerminator = "\n";
  std::string messagePrefix = "";
  std::string messageSuffix = "";

  // Buffer and performance settings
  size_t bufferSize = 8192;
  size_t maxMessageSize = 1024 * 1024; // 1MB
  size_t readBufferSize = 4096;
  size_t writeBufferSize = 4096;
  size_t maxQueueSize = 1000;

  // Timeout settings
  std::chrono::milliseconds connectTimeout{5000};
  std::chrono::milliseconds readTimeout{1000};
  std::chrono::milliseconds writeTimeout{1000};
  std::chrono::milliseconds reconnectDelay{1000};
  std::chrono::milliseconds keepAliveInterval{30000};

  // Connection management
  int maxReconnectAttempts = 5;
  bool enableAutoReconnect = true;
  bool enableKeepAlive = true;
  bool enableNonBlocking = false;
  bool enableBidirectional = true;

  // Pipe permissions (Unix)
#ifdef _WIN32
  uint32_t pipePermissions = 0666;
#else
  mode_t pipePermissions = 0666;
#endif
  std::string pipeOwner = "";
  std::string pipeGroup = "";

  // Windows-specific settings
  uint32_t windowsPipeInstances = 10;
  uint32_t windowsOutBufferSize = 8192;
  uint32_t windowsInBufferSize = 8192;
  uint32_t windowsDefaultTimeout = 5000;

  // Message processing
  bool enableMessageValidation = true;
  bool enableMessageLogging = false;
  bool enableMessageTracing = false;
  bool enableBinaryMode = false;
  bool enableFlowControl = true;
  bool enableBackpressure = true;

  // Compression settings
  FifoCompressionType compressionType = FifoCompressionType::NONE;
  int compressionLevel = 6;
  size_t compressionThreshold = 1024;
  bool enableCompressionForSmallMessages = false;

  // Security settings
  FifoAuthMethod authMethod = FifoAuthMethod::FILESYSTEM_PERMISSIONS;
  std::string authToken = "";
  std::string certificatePath = "";
  std::string privateKeyPath = "";
  bool enableEncryption = false;
  std::string encryptionKey = "";

  // Error handling
  bool enableCircuitBreaker = true;
  int circuitBreakerThreshold = 5;
  std::chrono::milliseconds circuitBreakerTimeout{30000};
  bool enableRetryOnError = true;
  int maxRetryAttempts = 3;
  std::chrono::milliseconds retryDelay{1000};

  // Monitoring and debugging
  bool enablePerformanceMetrics = false;
  bool enableHealthChecking = true;
  std::chrono::milliseconds healthCheckInterval{10000};
  bool enableDebugLogging = false;
  std::string logLevel = "INFO";

  // Advanced features
  bool enableMultiplexing = false;
  int maxConcurrentConnections = 1;
  bool enableMessagePrioritization = false;
  bool enableMessageDeduplication = false;
  std::chrono::milliseconds deduplicationWindow{5000};

  // Platform-specific optimizations
  bool enablePlatformOptimizations = true;
  bool useMemoryMappedFiles = false;
  bool enableZeroCopy = false;
  size_t ioVectorSize = 16;

  // Serialization support
  json toJson() const;
  void fromJson(const json &j);
  bool validate() const;
  std::string toString() const;
};

/**
 * @brief FIFO configuration validation result
 */
struct FifoConfigValidationResult {
  bool isValid = true;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  std::string summary;
};

/**
 * @brief FIFO configuration manager with presets and validation
 */
class FifoConfigManager {
public:
  /**
   * @brief Configuration presets for different use cases
   */
  enum class ConfigPreset {
    DEFAULT,          // Balanced configuration
    HIGH_PERFORMANCE, // Optimized for throughput
    LOW_LATENCY,      // Optimized for minimal latency
    RELIABLE,         // Optimized for reliability
    SECURE,           // Security-focused configuration
    DEBUG,            // Development and debugging
    EMBEDDED,         // Resource-constrained environments
    BIDIRECTIONAL,    // Full duplex communication
    BROADCAST,        // One-to-many communication
    STREAMING         // Continuous data streaming
  };

  FifoConfigManager();
  ~FifoConfigManager() = default;

  // Configuration creation and management
  FifoConfig createConfig(ConfigPreset preset = ConfigPreset::DEFAULT) const;
  FifoConfig createCustomConfig(const json &customSettings) const;
  FifoConfig mergeConfigs(const FifoConfig &base,
                          const FifoConfig &override) const;

  // Configuration validation
  FifoConfigValidationResult validateConfig(const FifoConfig &config) const;
  FifoConfig sanitizeConfig(const FifoConfig &config) const;
  FifoConfig optimizeConfig(const FifoConfig &config) const;

  // Platform-specific configuration
  FifoConfig createPlatformOptimizedConfig(
      ConfigPreset preset = ConfigPreset::DEFAULT) const;
  std::string getPlatformDefaultPipePath(const std::string &pipeName) const;
  FifoPipeType detectPlatformPipeType() const;

  // Configuration persistence
  bool saveConfig(const FifoConfig &config, const std::string &filePath) const;
  FifoConfig loadConfig(const std::string &filePath) const;

  // Preset management
  std::vector<ConfigPreset> getAvailablePresets() const;
  std::string getPresetDescription(ConfigPreset preset) const;
  FifoConfig getPresetConfig(ConfigPreset preset) const;

  // Configuration comparison and analysis
  json compareConfigs(const FifoConfig &config1,
                      const FifoConfig &config2) const;
  std::vector<std::string>
  getConfigDifferences(const FifoConfig &config1,
                       const FifoConfig &config2) const;

  // Runtime configuration updates
  bool updateConfig(FifoConfig &config, const json &updates) const;
  json getConfigSchema() const;

private:
  // Preset creation methods
  FifoConfig createDefaultConfig() const;
  FifoConfig createHighPerformanceConfig() const;
  FifoConfig createLowLatencyConfig() const;
  FifoConfig createReliableConfig() const;
  FifoConfig createSecureConfig() const;
  FifoConfig createDebugConfig() const;
  FifoConfig createEmbeddedConfig() const;
  FifoConfig createBidirectionalConfig() const;
  FifoConfig createBroadcastConfig() const;
  FifoConfig createStreamingConfig() const;

  // Validation helpers
  bool validatePipePath(const std::string &path) const;
  bool validateBufferSizes(const FifoConfig &config) const;
  bool validateTimeouts(const FifoConfig &config) const;
  bool validatePermissions(const FifoConfig &config) const;
  bool validatePlatformCompatibility(const FifoConfig &config) const;

  // Platform detection
  bool isWindows() const;
  bool isUnix() const;
  std::string getCurrentPlatform() const;

  // Configuration optimization
  FifoConfig optimizeForPlatform(const FifoConfig &config) const;
  FifoConfig optimizeBufferSizes(const FifoConfig &config) const;
  FifoConfig optimizeTimeouts(const FifoConfig &config) const;
};

/**
 * @brief Global FIFO configuration manager instance
 */
FifoConfigManager &getGlobalFifoConfigManager();

} // namespace core
} // namespace hydrogen

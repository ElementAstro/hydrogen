#pragma once

#include "device_communicator.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Configuration manager for stdio communication
 *
 * This class provides advanced configuration management for stdio
 * communication, including validation, serialization, and preset management.
 */
class StdioConfigManager {
public:
  /**
   * @brief Configuration presets for common use cases
   */
  enum class ConfigPreset {
    DEFAULT,          // Basic stdio configuration
    HIGH_PERFORMANCE, // Optimized for high throughput
    LOW_LATENCY,      // Optimized for low latency
    RELIABLE,         // Maximum reliability with error recovery
    SECURE,           // Security-focused configuration
    DEBUG,            // Debug-friendly configuration
    EMBEDDED,         // Optimized for embedded systems
    CUSTOM            // User-defined configuration
  };

  StdioConfigManager();
  ~StdioConfigManager() = default;

  // Configuration management
  StdioConfig createConfig(ConfigPreset preset = ConfigPreset::DEFAULT) const;
  bool validateConfig(const StdioConfig &config) const;
  std::string getValidationError(const StdioConfig &config) const;

  // Serialization
  json configToJson(const StdioConfig &config) const;
  StdioConfig configFromJson(const json &jsonConfig) const;

  // File operations
  bool saveConfigToFile(const StdioConfig &config,
                        const std::string &filename) const;
  StdioConfig loadConfigFromFile(const std::string &filename) const;

  // Configuration merging and inheritance
  StdioConfig mergeConfigs(const StdioConfig &base,
                           const StdioConfig &override) const;
  StdioConfig inheritFromPreset(
      ConfigPreset preset,
      const std::unordered_map<std::string, std::string> &overrides) const;

  // Preset management
  void registerCustomPreset(const std::string &name, const StdioConfig &config);
  StdioConfig getCustomPreset(const std::string &name) const;
  std::vector<std::string> getAvailablePresets() const;

  // Configuration optimization
  StdioConfig optimizeForUseCase(const StdioConfig &baseConfig,
                                 const std::string &useCase) const;
  StdioConfig
  autoTuneConfig(const StdioConfig &baseConfig,
                 const std::unordered_map<std::string, double> &metrics) const;

  // Validation helpers
  bool isValidFramingMode(StdioConfig::FramingMode mode) const;
  bool isValidCompressionType(StdioConfig::CompressionType type) const;
  bool isValidEncryptionAlgorithm(const std::string &algorithm) const;
  bool isValidAuthenticationMethod(const std::string &method) const;

  // Configuration comparison
  bool areConfigsEqual(const StdioConfig &config1,
                       const StdioConfig &config2) const;
  std::vector<std::string>
  getConfigDifferences(const StdioConfig &config1,
                       const StdioConfig &config2) const;

  // Performance estimation
  struct PerformanceEstimate {
    double estimatedThroughput; // Messages per second
    double estimatedLatency;    // Milliseconds
    double memoryUsage;         // Bytes
    double cpuUsage;            // Percentage
    std::string bottleneck;     // Identified bottleneck
  };
  PerformanceEstimate estimatePerformance(const StdioConfig &config) const;

private:
  std::unordered_map<std::string, StdioConfig> customPresets_;

  // Preset creation helpers
  StdioConfig createDefaultConfig() const;
  StdioConfig createHighPerformanceConfig() const;
  StdioConfig createLowLatencyConfig() const;
  StdioConfig createReliableConfig() const;
  StdioConfig createSecureConfig() const;
  StdioConfig createDebugConfig() const;
  StdioConfig createEmbeddedConfig() const;

  // Validation helpers
  bool validateBasicSettings(const StdioConfig &config,
                             std::string &error) const;
  bool validateFramingSettings(const StdioConfig &config,
                               std::string &error) const;
  bool validateCompressionSettings(const StdioConfig &config,
                                   std::string &error) const;
  bool validateSecuritySettings(const StdioConfig &config,
                                std::string &error) const;
  bool validateConnectionSettings(const StdioConfig &config,
                                  std::string &error) const;
  bool validatePerformanceSettings(const StdioConfig &config,
                                   std::string &error) const;

  // JSON conversion helpers
  json framingModeToJson(StdioConfig::FramingMode mode) const;
  StdioConfig::FramingMode framingModeFromJson(const json &j) const;
  json compressionTypeToJson(StdioConfig::CompressionType type) const;
  StdioConfig::CompressionType compressionTypeFromJson(const json &j) const;

  // Configuration merging helpers
  template <typename T>
  T mergeValue(const T &base, const T &override,
               bool useOverride = true) const {
    return useOverride ? override : base;
  }

  // Optimization helpers
  StdioConfig optimizeForThroughput(const StdioConfig &config) const;
  StdioConfig optimizeForLatency(const StdioConfig &config) const;
  StdioConfig optimizeForReliability(const StdioConfig &config) const;
  StdioConfig optimizeForMemoryUsage(const StdioConfig &config) const;
};

/**
 * @brief Global stdio configuration manager instance
 */
StdioConfigManager &getGlobalStdioConfigManager();

/**
 * @brief Configuration builder for fluent API
 */
class StdioConfigBuilder {
public:
  StdioConfigBuilder();
  explicit StdioConfigBuilder(const StdioConfig &baseConfig);

  // Basic settings
  StdioConfigBuilder &withBufferSize(size_t size);
  StdioConfigBuilder &withTimeout(std::chrono::milliseconds read,
                                  std::chrono::milliseconds write);
  StdioConfigBuilder &withEncoding(const std::string &encoding);
  StdioConfigBuilder &withLineTerminator(const std::string &terminator);

  // Framing
  StdioConfigBuilder &withFraming(StdioConfig::FramingMode mode);
  StdioConfigBuilder &withFrameDelimiter(const std::string &delimiter);
  StdioConfigBuilder &withMaxFrameSize(size_t size);

  // Compression
  StdioConfigBuilder &withCompression(StdioConfig::CompressionType type,
                                      int level = 6);
  StdioConfigBuilder &withCompressionThreshold(size_t threshold);

  // Security
  StdioConfigBuilder &withAuthentication(const std::string &method,
                                         const std::string &token);
  StdioConfigBuilder &withEncryption(const std::string &algorithm,
                                     const std::string &key);

  // Connection management
  StdioConfigBuilder &withHeartbeat(std::chrono::milliseconds interval);
  StdioConfigBuilder &withReconnect(int maxAttempts,
                                    std::chrono::milliseconds delay);

  // Performance
  StdioConfigBuilder &withFlowControl(size_t sendBuffer, size_t receiveBuffer);
  StdioConfigBuilder &withThreads(int ioThreads);

  // Build the configuration
  StdioConfig build() const;

private:
  StdioConfig config_;
};

} // namespace core
} // namespace hydrogen

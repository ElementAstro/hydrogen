#pragma once

#include "device_interface.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Plugin API version for compatibility checking
 */
struct PluginApiVersion {
  int major = 1;
  int minor = 0;
  int patch = 0;

  bool isCompatible(const PluginApiVersion &other) const {
    return major == other.major && minor >= other.minor;
  }

  std::string toString() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(patch);
  }

  json toJson() const;
  static PluginApiVersion fromJson(const json &j);
};

/**
 * @brief Plugin metadata information
 */
struct PluginMetadata {
  std::string name;
  std::string version;
  std::string description;
  std::string author;
  std::string license;
  std::vector<std::string> supportedDeviceTypes;
  std::vector<std::string> dependencies;
  PluginApiVersion apiVersion;
  json configuration;

  json toJson() const;
  static PluginMetadata fromJson(const json &j);
};

/**
 * @brief Plugin security permissions
 */
enum class PluginPermission {
  DEVICE_ACCESS,       // Access to device hardware
  NETWORK_ACCESS,      // Network communication
  FILE_SYSTEM_READ,    // Read file system
  FILE_SYSTEM_WRITE,   // Write file system
  SYSTEM_COMMANDS,     // Execute system commands
  CONFIGURATION_READ,  // Read system configuration
  CONFIGURATION_WRITE, // Modify system configuration
  LOGGING_ACCESS,      // Access to logging system
  METRICS_ACCESS       // Access to metrics system
};

/**
 * @brief Plugin security context
 */
struct PluginSecurityContext {
  std::string pluginId;
  std::vector<PluginPermission> permissions;
  bool sandboxed = true;
  std::string workingDirectory;
  std::vector<std::string> allowedPaths;
  std::vector<std::string> allowedNetworkHosts;

  bool hasPermission(PluginPermission permission) const;
  json toJson() const;
  static PluginSecurityContext fromJson(const json &j);
};

/**
 * @brief Plugin validation result
 */
struct PluginValidationResult {
  bool isValid = false;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  PluginSecurityContext securityContext;

  json toJson() const;
  static PluginValidationResult fromJson(const json &j);
};

/**
 * @brief Base interface for device plugins
 */
class IDevicePlugin {
public:
  virtual ~IDevicePlugin() = default;

  /**
   * @brief Get plugin metadata
   * @return Plugin metadata
   */
  virtual PluginMetadata getMetadata() const = 0;

  /**
   * @brief Initialize the plugin
   * @param context Security context for the plugin
   * @param config Plugin configuration
   * @return True if initialization successful
   */
  virtual bool initialize(const PluginSecurityContext &context,
                          const json &config) = 0;

  /**
   * @brief Shutdown the plugin
   */
  virtual void shutdown() = 0;

  /**
   * @brief Create device instance
   * @param deviceType Type of device to create
   * @param deviceId Unique device identifier
   * @param config Device configuration
   * @return Shared pointer to device instance
   */
  virtual std::shared_ptr<IDevice> createDevice(const std::string &deviceType,
                                                const std::string &deviceId,
                                                const json &config) = 0;

  /**
   * @brief Get supported device types
   * @return Vector of supported device type names
   */
  virtual std::vector<std::string> getSupportedDeviceTypes() const = 0;

  /**
   * @brief Validate device configuration
   * @param deviceType Type of device
   * @param config Device configuration
   * @return True if configuration is valid
   */
  virtual bool validateDeviceConfig(const std::string &deviceType,
                                    const json &config) const = 0;

  /**
   * @brief Get default configuration for device type
   * @param deviceType Type of device
   * @return Default configuration JSON
   */
  virtual json getDefaultDeviceConfig(const std::string &deviceType) const = 0;

  /**
   * @brief Handle plugin-specific commands
   * @param command Command name
   * @param parameters Command parameters
   * @return Command result
   */
  virtual json handleCommand(const std::string &command,
                             const json &parameters) = 0;

  /**
   * @brief Get plugin health status
   * @return Health information as JSON
   */
  virtual json getHealthStatus() const = 0;
};

/**
 * @brief Plugin factory function type
 */
using PluginFactory = std::function<std::unique_ptr<IDevicePlugin>()>;

/**
 * @brief Plugin manager for loading and managing device plugins
 */
class DevicePluginManager {
public:
  DevicePluginManager();
  virtual ~DevicePluginManager();

  /**
   * @brief Load plugin from shared library
   * @param pluginPath Path to plugin shared library
   * @return True if loaded successfully
   */
  bool loadPlugin(const std::string &pluginPath);

  /**
   * @brief Unload plugin
   * @param pluginId Plugin identifier
   * @return True if unloaded successfully
   */
  bool unloadPlugin(const std::string &pluginId);

  /**
   * @brief Register plugin factory function
   * @param pluginId Plugin identifier
   * @param factory Factory function
   * @param metadata Plugin metadata
   * @return True if registered successfully
   */
  bool registerPlugin(const std::string &pluginId, PluginFactory factory,
                      const PluginMetadata &metadata);

  /**
   * @brief Create device using plugin
   * @param pluginId Plugin identifier
   * @param deviceType Device type
   * @param deviceId Device identifier
   * @param config Device configuration
   * @return Shared pointer to device instance
   */
  std::shared_ptr<IDevice> createDevice(const std::string &pluginId,
                                        const std::string &deviceType,
                                        const std::string &deviceId,
                                        const json &config);

  /**
   * @brief Get list of loaded plugins
   * @return Vector of plugin IDs
   */
  std::vector<std::string> getLoadedPlugins() const;

  /**
   * @brief Get plugin metadata
   * @param pluginId Plugin identifier
   * @return Plugin metadata
   */
  PluginMetadata getPluginMetadata(const std::string &pluginId) const;

  /**
   * @brief Get supported device types for plugin
   * @param pluginId Plugin identifier
   * @return Vector of supported device types
   */
  std::vector<std::string>
  getSupportedDeviceTypes(const std::string &pluginId) const;

  /**
   * @brief Validate plugin
   * @param pluginPath Path to plugin
   * @return Validation result
   */
  PluginValidationResult validatePlugin(const std::string &pluginPath) const;

  /**
   * @brief Set plugin security policy
   * @param policy Security policy configuration
   */
  void setSecurityPolicy(const json &policy);

  /**
   * @brief Enable/disable plugin sandboxing
   * @param enabled Whether to enable sandboxing
   */
  void setSandboxingEnabled(bool enabled);

  /**
   * @brief Get plugin health status
   * @param pluginId Plugin identifier
   * @return Health status as JSON
   */
  json getPluginHealth(const std::string &pluginId) const;

  /**
   * @brief Get all plugin health statuses
   * @return Map of plugin IDs to health status
   */
  std::unordered_map<std::string, json> getAllPluginHealth() const;

  /**
   * @brief Reload plugin
   * @param pluginId Plugin identifier
   * @return True if reloaded successfully
   */
  bool reloadPlugin(const std::string &pluginId);

  /**
   * @brief Set plugin event callback
   * @param callback Function to call for plugin events
   */
  void
  setPluginEventCallback(std::function<void(const std::string &,
                                            const std::string &, const json &)>
                             callback);

  /**
   * @brief Get singleton instance
   * @return Reference to singleton instance
   */
  static DevicePluginManager &getInstance();

private:
  struct LoadedPlugin {
    std::string pluginId;
    std::string pluginPath;
    PluginMetadata metadata;
    PluginFactory factory;
    std::unique_ptr<IDevicePlugin> instance;
    PluginSecurityContext securityContext;
    void *libraryHandle = nullptr; // For dynamic loading
    std::chrono::system_clock::time_point loadTime;
    bool isActive = false;
  };

  mutable std::mutex pluginsMutex_;
  std::unordered_map<std::string, std::unique_ptr<LoadedPlugin>> loadedPlugins_;

  // Security configuration
  json securityPolicy_;
  std::atomic<bool> sandboxingEnabled_{true};

  // Event callback
  std::function<void(const std::string &, const std::string &, const json &)>
      eventCallback_;

  // Helper methods
  PluginSecurityContext
  createSecurityContext(const PluginMetadata &metadata) const;
  bool validateSecurity(const PluginMetadata &metadata,
                        const PluginSecurityContext &context) const;
  std::string generatePluginId(const PluginMetadata &metadata) const;
  bool loadPluginLibrary(const std::string &pluginPath, LoadedPlugin &plugin);
  void unloadPluginLibrary(LoadedPlugin &plugin);
  void notifyPluginEvent(const std::string &pluginId, const std::string &event,
                         const json &data);
};

/**
 * @brief Helper functions for plugin system
 */
std::string pluginPermissionToString(PluginPermission permission);
PluginPermission stringToPluginPermission(const std::string &permission);

} // namespace core
} // namespace hydrogen

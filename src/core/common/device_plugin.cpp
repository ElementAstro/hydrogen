#include "hydrogen/core/device_plugin.h"
#include "hydrogen/core/utils.h"
#include <algorithm>
#include <fstream>
#include <dlfcn.h> // For dynamic loading on Unix-like systems
#ifdef _WIN32
#include <windows.h>
#endif

namespace hydrogen {
namespace core {

// PluginApiVersion implementation
json PluginApiVersion::toJson() const {
    return json{
        {"major", major},
        {"minor", minor},
        {"patch", patch}
    };
}

PluginApiVersion PluginApiVersion::fromJson(const json& j) {
    PluginApiVersion version;
    version.major = j.value("major", 1);
    version.minor = j.value("minor", 0);
    version.patch = j.value("patch", 0);
    return version;
}

// PluginMetadata implementation
json PluginMetadata::toJson() const {
    return json{
        {"name", name},
        {"version", version},
        {"description", description},
        {"author", author},
        {"license", license},
        {"supportedDeviceTypes", supportedDeviceTypes},
        {"dependencies", dependencies},
        {"apiVersion", apiVersion.toJson()},
        {"configuration", configuration}
    };
}

PluginMetadata PluginMetadata::fromJson(const json& j) {
    PluginMetadata metadata;
    metadata.name = j.value("name", "");
    metadata.version = j.value("version", "1.0.0");
    metadata.description = j.value("description", "");
    metadata.author = j.value("author", "");
    metadata.license = j.value("license", "");
    metadata.supportedDeviceTypes = j.value("supportedDeviceTypes", std::vector<std::string>{});
    metadata.dependencies = j.value("dependencies", std::vector<std::string>{});
    metadata.configuration = j.value("configuration", json::object());
    
    if (j.contains("apiVersion")) {
        metadata.apiVersion = PluginApiVersion::fromJson(j["apiVersion"]);
    }
    
    return metadata;
}

// PluginSecurityContext implementation
bool PluginSecurityContext::hasPermission(PluginPermission permission) const {
    return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
}

json PluginSecurityContext::toJson() const {
    json permissionsArray = json::array();
    for (const auto& permission : permissions) {
        permissionsArray.push_back(pluginPermissionToString(permission));
    }
    
    return json{
        {"pluginId", pluginId},
        {"permissions", permissionsArray},
        {"sandboxed", sandboxed},
        {"workingDirectory", workingDirectory},
        {"allowedPaths", allowedPaths},
        {"allowedNetworkHosts", allowedNetworkHosts}
    };
}

PluginSecurityContext PluginSecurityContext::fromJson(const json& j) {
    PluginSecurityContext context;
    context.pluginId = j.value("pluginId", "");
    context.sandboxed = j.value("sandboxed", true);
    context.workingDirectory = j.value("workingDirectory", "");
    context.allowedPaths = j.value("allowedPaths", std::vector<std::string>{});
    context.allowedNetworkHosts = j.value("allowedNetworkHosts", std::vector<std::string>{});
    
    if (j.contains("permissions") && j["permissions"].is_array()) {
        for (const auto& permissionStr : j["permissions"]) {
            context.permissions.push_back(stringToPluginPermission(permissionStr.get<std::string>()));
        }
    }
    
    return context;
}

// PluginValidationResult implementation
json PluginValidationResult::toJson() const {
    return json{
        {"isValid", isValid},
        {"errors", errors},
        {"warnings", warnings},
        {"securityContext", securityContext.toJson()}
    };
}

PluginValidationResult PluginValidationResult::fromJson(const json& j) {
    PluginValidationResult result;
    result.isValid = j.value("isValid", false);
    result.errors = j.value("errors", std::vector<std::string>{});
    result.warnings = j.value("warnings", std::vector<std::string>{});
    
    if (j.contains("securityContext")) {
        result.securityContext = PluginSecurityContext::fromJson(j["securityContext"]);
    }
    
    return result;
}

// DevicePluginManager implementation
DevicePluginManager::DevicePluginManager() {
    // Initialize default security policy
    securityPolicy_ = json{
        {"defaultPermissions", json::array({"DEVICE_ACCESS", "LOGGING_ACCESS"})},
        {"maxMemoryUsage", 100}, // MB
        {"maxCpuUsage", 10},     // Percentage
        {"networkTimeout", 30},   // Seconds
        {"allowUnsignedPlugins", false}
    };
}

DevicePluginManager::~DevicePluginManager() {
    // Unload all plugins
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    for (auto& [pluginId, plugin] : loadedPlugins_) {
        if (plugin->instance) {
            plugin->instance->shutdown();
        }
        unloadPluginLibrary(*plugin);
    }
    loadedPlugins_.clear();
}

bool DevicePluginManager::loadPlugin(const std::string& pluginPath) {
    // Validate plugin first
    auto validationResult = validatePlugin(pluginPath);
    if (!validationResult.isValid) {
        notifyPluginEvent("", "VALIDATION_FAILED", validationResult.toJson());
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    auto plugin = std::make_unique<LoadedPlugin>();
    plugin->pluginPath = pluginPath;
    plugin->loadTime = std::chrono::system_clock::now();
    
    // Load the plugin library
    if (!loadPluginLibrary(pluginPath, *plugin)) {
        notifyPluginEvent("", "LOAD_FAILED", json{{"path", pluginPath}});
        return false;
    }
    
    // Initialize the plugin
    if (!plugin->instance->initialize(plugin->securityContext, plugin->metadata.configuration)) {
        notifyPluginEvent(plugin->pluginId, "INIT_FAILED", json{});
        unloadPluginLibrary(*plugin);
        return false;
    }
    
    plugin->isActive = true;
    std::string pluginId = plugin->pluginId;
    loadedPlugins_[pluginId] = std::move(plugin);
    
    notifyPluginEvent(pluginId, "LOADED", json{{"path", pluginPath}});
    return true;
}

bool DevicePluginManager::unloadPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    auto it = loadedPlugins_.find(pluginId);
    if (it == loadedPlugins_.end()) {
        return false;
    }
    
    auto& plugin = it->second;
    
    // Shutdown plugin
    if (plugin->instance) {
        plugin->instance->shutdown();
    }
    
    // Unload library
    unloadPluginLibrary(*plugin);
    
    loadedPlugins_.erase(it);
    
    notifyPluginEvent(pluginId, "UNLOADED", json{});
    return true;
}

bool DevicePluginManager::registerPlugin(const std::string& pluginId,
                                        PluginFactory factory,
                                        const PluginMetadata& metadata) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    if (loadedPlugins_.find(pluginId) != loadedPlugins_.end()) {
        return false; // Already registered
    }
    
    auto plugin = std::make_unique<LoadedPlugin>();
    plugin->pluginId = pluginId;
    plugin->metadata = metadata;
    plugin->factory = factory;
    plugin->securityContext = createSecurityContext(metadata);
    plugin->loadTime = std::chrono::system_clock::now();
    
    // Create plugin instance
    plugin->instance = factory();
    if (!plugin->instance) {
        return false;
    }
    
    // Initialize plugin
    if (!plugin->instance->initialize(plugin->securityContext, metadata.configuration)) {
        return false;
    }
    
    plugin->isActive = true;
    loadedPlugins_[pluginId] = std::move(plugin);
    
    notifyPluginEvent(pluginId, "REGISTERED", json{});
    return true;
}

std::shared_ptr<IDevice> DevicePluginManager::createDevice(const std::string& pluginId,
                                                          const std::string& deviceType,
                                                          const std::string& deviceId,
                                                          const json& config) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    auto it = loadedPlugins_.find(pluginId);
    if (it == loadedPlugins_.end() || !it->second->isActive) {
        return nullptr;
    }
    
    auto& plugin = it->second;
    
    // Validate device configuration
    if (!plugin->instance->validateDeviceConfig(deviceType, config)) {
        notifyPluginEvent(pluginId, "DEVICE_CONFIG_INVALID", 
                         json{{"deviceType", deviceType}, {"deviceId", deviceId}});
        return nullptr;
    }
    
    try {
        auto device = plugin->instance->createDevice(deviceType, deviceId, config);
        if (device) {
            notifyPluginEvent(pluginId, "DEVICE_CREATED", 
                             json{{"deviceType", deviceType}, {"deviceId", deviceId}});
        }
        return device;
    } catch (const std::exception& e) {
        notifyPluginEvent(pluginId, "DEVICE_CREATION_FAILED", 
                         json{{"deviceType", deviceType}, {"deviceId", deviceId}, {"error", e.what()}});
        return nullptr;
    }
}

std::vector<std::string> DevicePluginManager::getLoadedPlugins() const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    std::vector<std::string> pluginIds;
    for (const auto& [pluginId, plugin] : loadedPlugins_) {
        if (plugin->isActive) {
            pluginIds.push_back(pluginId);
        }
    }
    
    return pluginIds;
}

PluginMetadata DevicePluginManager::getPluginMetadata(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end()) {
        return it->second->metadata;
    }
    
    return PluginMetadata{};
}

std::vector<std::string> DevicePluginManager::getSupportedDeviceTypes(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end() && it->second->instance) {
        return it->second->instance->getSupportedDeviceTypes();
    }
    
    return {};
}

PluginValidationResult DevicePluginManager::validatePlugin(const std::string& pluginPath) const {
    PluginValidationResult result;
    
    // Basic file existence check
    std::ifstream file(pluginPath);
    if (!file.good()) {
        result.errors.push_back("Plugin file not found: " + pluginPath);
        return result;
    }
    
    // TODO: Add more sophisticated validation
    // - Check plugin signature
    // - Validate metadata
    // - Security scan
    // - API compatibility check
    
    result.isValid = true;
    return result;
}

void DevicePluginManager::setSecurityPolicy(const json& policy) {
    securityPolicy_ = policy;
}

void DevicePluginManager::setSandboxingEnabled(bool enabled) {
    sandboxingEnabled_ = enabled;
}

json DevicePluginManager::getPluginHealth(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    auto it = loadedPlugins_.find(pluginId);
    if (it != loadedPlugins_.end() && it->second->instance) {
        return it->second->instance->getHealthStatus();
    }
    
    return json{{"status", "not_found"}};
}

std::unordered_map<std::string, json> DevicePluginManager::getAllPluginHealth() const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    std::unordered_map<std::string, json> healthMap;
    for (const auto& [pluginId, plugin] : loadedPlugins_) {
        if (plugin->instance) {
            healthMap[pluginId] = plugin->instance->getHealthStatus();
        }
    }
    
    return healthMap;
}

bool DevicePluginManager::reloadPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    auto it = loadedPlugins_.find(pluginId);
    if (it == loadedPlugins_.end()) {
        return false;
    }
    
    std::string pluginPath = it->second->pluginPath;
    
    // Unload current plugin
    if (it->second->instance) {
        it->second->instance->shutdown();
    }
    unloadPluginLibrary(*it->second);
    loadedPlugins_.erase(it);
    
    // Reload plugin
    lock.~lock_guard();
    return loadPlugin(pluginPath);
}

void DevicePluginManager::setPluginEventCallback(
    std::function<void(const std::string&, const std::string&, const json&)> callback) {
    eventCallback_ = callback;
}

DevicePluginManager& DevicePluginManager::getInstance() {
    static DevicePluginManager instance;
    return instance;
}

PluginSecurityContext DevicePluginManager::createSecurityContext(const PluginMetadata& metadata) const {
    PluginSecurityContext context;
    context.pluginId = generatePluginId(metadata);
    context.sandboxed = sandboxingEnabled_;
    
    // Set default permissions from security policy
    if (securityPolicy_.contains("defaultPermissions")) {
        for (const auto& permissionStr : securityPolicy_["defaultPermissions"]) {
            context.permissions.push_back(stringToPluginPermission(permissionStr.get<std::string>()));
        }
    }
    
    // Set working directory
    context.workingDirectory = "./plugins/" + context.pluginId;
    
    // Set allowed paths (restricted to plugin directory)
    context.allowedPaths = {context.workingDirectory};
    
    return context;
}

bool DevicePluginManager::validateSecurity(const PluginMetadata& metadata, 
                                          const PluginSecurityContext& context) const {
    // Check if plugin requires permissions that are not allowed
    // This is a simplified security check
    
    if (!securityPolicy_.value("allowUnsignedPlugins", false)) {
        // In a real implementation, would check plugin signature
    }
    
    // Check memory and CPU limits
    // In a real implementation, would enforce resource limits
    
    return true;
}

std::string DevicePluginManager::generatePluginId(const PluginMetadata& metadata) const {
    return metadata.name + "_" + metadata.version;
}

bool DevicePluginManager::loadPluginLibrary(const std::string& pluginPath, LoadedPlugin& plugin) {
    // Platform-specific dynamic library loading
#ifdef _WIN32
    HMODULE handle = LoadLibraryA(pluginPath.c_str());
    if (!handle) {
        return false;
    }
    
    // Get plugin factory function
    typedef IDevicePlugin* (*CreatePluginFunc)();
    CreatePluginFunc createPlugin = (CreatePluginFunc)GetProcAddress(handle, "createPlugin");
    
    if (!createPlugin) {
        FreeLibrary(handle);
        return false;
    }
    
    plugin.libraryHandle = handle;
#else
    void* handle = dlopen(pluginPath.c_str(), RTLD_LAZY);
    if (!handle) {
        return false;
    }
    
    // Get plugin factory function
    typedef IDevicePlugin* (*CreatePluginFunc)();
    CreatePluginFunc createPlugin = (CreatePluginFunc)dlsym(handle, "createPlugin");
    
    if (!createPlugin) {
        dlclose(handle);
        return false;
    }
    
    plugin.libraryHandle = handle;
#endif
    
    // Create plugin instance
    try {
        plugin.instance = std::unique_ptr<IDevicePlugin>(createPlugin());
        if (!plugin.instance) {
            unloadPluginLibrary(plugin);
            return false;
        }
        
        // Get metadata from plugin
        plugin.metadata = plugin.instance->getMetadata();
        plugin.pluginId = generatePluginId(plugin.metadata);
        plugin.securityContext = createSecurityContext(plugin.metadata);
        
        return true;
    } catch (const std::exception&) {
        unloadPluginLibrary(plugin);
        return false;
    }
}

void DevicePluginManager::unloadPluginLibrary(LoadedPlugin& plugin) {
    if (plugin.libraryHandle) {
#ifdef _WIN32
        FreeLibrary((HMODULE)plugin.libraryHandle);
#else
        dlclose(plugin.libraryHandle);
#endif
        plugin.libraryHandle = nullptr;
    }
}

void DevicePluginManager::notifyPluginEvent(const std::string& pluginId, 
                                           const std::string& event, 
                                           const json& data) {
    if (eventCallback_) {
        eventCallback_(pluginId, event, data);
    }
}

// Helper function implementations
std::string pluginPermissionToString(PluginPermission permission) {
    switch (permission) {
        case PluginPermission::DEVICE_ACCESS: return "DEVICE_ACCESS";
        case PluginPermission::NETWORK_ACCESS: return "NETWORK_ACCESS";
        case PluginPermission::FILE_SYSTEM_READ: return "FILE_SYSTEM_READ";
        case PluginPermission::FILE_SYSTEM_WRITE: return "FILE_SYSTEM_WRITE";
        case PluginPermission::SYSTEM_COMMANDS: return "SYSTEM_COMMANDS";
        case PluginPermission::CONFIGURATION_READ: return "CONFIGURATION_READ";
        case PluginPermission::CONFIGURATION_WRITE: return "CONFIGURATION_WRITE";
        case PluginPermission::LOGGING_ACCESS: return "LOGGING_ACCESS";
        case PluginPermission::METRICS_ACCESS: return "METRICS_ACCESS";
        default: return "UNKNOWN";
    }
}

PluginPermission stringToPluginPermission(const std::string& permission) {
    if (permission == "DEVICE_ACCESS") return PluginPermission::DEVICE_ACCESS;
    if (permission == "NETWORK_ACCESS") return PluginPermission::NETWORK_ACCESS;
    if (permission == "FILE_SYSTEM_READ") return PluginPermission::FILE_SYSTEM_READ;
    if (permission == "FILE_SYSTEM_WRITE") return PluginPermission::FILE_SYSTEM_WRITE;
    if (permission == "SYSTEM_COMMANDS") return PluginPermission::SYSTEM_COMMANDS;
    if (permission == "CONFIGURATION_READ") return PluginPermission::CONFIGURATION_READ;
    if (permission == "CONFIGURATION_WRITE") return PluginPermission::CONFIGURATION_WRITE;
    if (permission == "LOGGING_ACCESS") return PluginPermission::LOGGING_ACCESS;
    if (permission == "METRICS_ACCESS") return PluginPermission::METRICS_ACCESS;
    return PluginPermission::DEVICE_ACCESS;
}

} // namespace core
} // namespace hydrogen

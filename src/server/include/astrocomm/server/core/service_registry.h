#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <mutex>
#include <typeindex>
#include <any>

namespace astrocomm {
namespace server {
namespace core {

/**
 * @brief Service lifecycle states
 */
enum class ServiceState {
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED,
    STARTING,
    RUNNING,
    STOPPING,
    STOPPED,
    ERROR
};

/**
 * @brief Service dependency information
 */
struct ServiceDependency {
    std::string serviceName;
    std::type_index serviceType;
    bool required;
    std::string version;
};

/**
 * @brief Base interface for all services
 * 
 * Defines the common contract that all services must implement,
 * including lifecycle management, dependency resolution, and health monitoring.
 */
class IService {
public:
    virtual ~IService() = default;

    // Service identification
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::string getDescription() const = 0;

    // Lifecycle management
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool shutdown() = 0;
    virtual ServiceState getState() const = 0;

    // Dependency management
    virtual std::vector<ServiceDependency> getDependencies() const = 0;
    virtual bool areDependenciesSatisfied() const = 0;

    // Health monitoring
    virtual bool isHealthy() const = 0;
    virtual std::string getHealthStatus() const = 0;
    virtual std::unordered_map<std::string, std::string> getMetrics() const = 0;

    // Configuration
    virtual void setConfiguration(const std::unordered_map<std::string, std::string>& config) = 0;
    virtual std::unordered_map<std::string, std::string> getConfiguration() const = 0;

    // Event callbacks
    using StateChangeCallback = std::function<void(ServiceState oldState, ServiceState newState)>;
    virtual void setStateChangeCallback(StateChangeCallback callback) = 0;
};

/**
 * @brief Service factory interface
 * 
 * Provides a way to create service instances with proper dependency injection.
 */
class IServiceFactory {
public:
    virtual ~IServiceFactory() = default;
    
    virtual std::unique_ptr<IService> createService(
        const std::string& serviceName,
        const std::unordered_map<std::string, std::string>& config = {}
    ) = 0;
    
    virtual std::vector<std::string> getSupportedServices() const = 0;
    virtual bool isServiceSupported(const std::string& serviceName) const = 0;
};

/**
 * @brief Service registry for dependency injection and service discovery
 * 
 * Manages service registration, dependency resolution, and lifecycle coordination.
 * Implements the Service Locator pattern with dependency injection capabilities.
 */
class ServiceRegistry {
public:
    static ServiceRegistry& getInstance();

    // Service registration
    template<typename T>
    void registerService(std::unique_ptr<T> service);
    
    template<typename T>
    void registerService(std::shared_ptr<T> service);
    
    void registerService(const std::string& name, std::shared_ptr<IService> service);
    void unregisterService(const std::string& name);

    // Service lookup
    template<typename T>
    std::shared_ptr<T> getService() const;
    
    template<typename T>
    std::shared_ptr<T> getService(const std::string& name) const;
    
    std::shared_ptr<IService> getService(const std::string& name) const;
    
    // Service discovery
    std::vector<std::string> getRegisteredServices() const;
    std::vector<std::string> getServicesByType(const std::type_index& type) const;
    bool isServiceRegistered(const std::string& name) const;
    
    template<typename T>
    bool isServiceRegistered() const;

    // Lifecycle management
    bool initializeAllServices();
    bool startAllServices();
    bool stopAllServices();
    bool shutdownAllServices();
    
    bool initializeService(const std::string& name);
    bool startService(const std::string& name);
    bool stopService(const std::string& name);
    bool shutdownService(const std::string& name);

    // Dependency resolution
    bool resolveDependencies();
    std::vector<std::string> getServiceDependencies(const std::string& name) const;
    std::vector<std::string> getServiceDependents(const std::string& name) const;
    std::vector<std::string> getStartupOrder() const;

    // Health monitoring
    std::unordered_map<std::string, ServiceState> getServiceStates() const;
    std::unordered_map<std::string, bool> getServiceHealthStatus() const;
    bool areAllServicesHealthy() const;

    // Configuration
    void setGlobalConfiguration(const std::unordered_map<std::string, std::string>& config);
    void setServiceConfiguration(const std::string& name, 
                               const std::unordered_map<std::string, std::string>& config);

    // Event handling
    using ServiceEventCallback = std::function<void(const std::string&, ServiceState, ServiceState)>;
    void setServiceEventCallback(ServiceEventCallback callback);

    // Factory management
    void registerFactory(std::unique_ptr<IServiceFactory> factory);
    std::unique_ptr<IService> createService(const std::string& serviceName,
                                          const std::unordered_map<std::string, std::string>& config = {});

private:
    ServiceRegistry() = default;
    ~ServiceRegistry() = default;
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;

    struct ServiceInfo {
        std::shared_ptr<IService> service;
        std::type_index type = std::type_index(typeid(void));
        std::vector<std::string> dependencies;
        std::vector<std::string> dependents;
        std::unordered_map<std::string, std::string> config;
    };

    mutable std::mutex servicesMutex_;
    std::unordered_map<std::string, ServiceInfo> services_;
    std::unordered_map<std::type_index, std::vector<std::string>> servicesByType_;
    
    std::vector<std::unique_ptr<IServiceFactory>> factories_;
    std::unordered_map<std::string, std::string> globalConfig_;
    
    ServiceEventCallback eventCallback_;
    
    // Helper methods
    bool validateDependencies() const;
    bool hasCyclicDependency(const std::string& serviceName,
                           std::unordered_set<std::string>& visited,
                           std::unordered_set<std::string>& recursionStack) const;
    std::vector<std::string> topologicalSort() const;
    void notifyStateChange(const std::string& serviceName, ServiceState oldState, ServiceState newState);
};

/**
 * @brief Base service implementation
 * 
 * Provides common functionality for service implementations,
 * reducing boilerplate code and ensuring consistent behavior.
 */
class BaseService : public virtual IService {
public:
    explicit BaseService(const std::string& name, const std::string& version = "1.0.0");
    virtual ~BaseService() = default;

    // IService implementation
    std::string getName() const override;
    std::string getVersion() const override;
    std::string getDescription() const override;
    
    ServiceState getState() const override;
    bool isHealthy() const override;
    std::string getHealthStatus() const override;
    std::unordered_map<std::string, std::string> getMetrics() const override;
    
    void setConfiguration(const std::unordered_map<std::string, std::string>& config) override;
    std::unordered_map<std::string, std::string> getConfiguration() const override;
    
    void setStateChangeCallback(StateChangeCallback callback) override;

protected:
    // Helper methods for derived classes
    void setState(ServiceState newState);
    void setHealthy(bool healthy);
    void setHealthStatus(const std::string& status);
    void updateMetric(const std::string& name, const std::string& value);
    
    std::string getConfigValue(const std::string& key, const std::string& defaultValue = "") const;
    bool getConfigBool(const std::string& key, bool defaultValue = false) const;
    int getConfigInt(const std::string& key, int defaultValue = 0) const;

private:
    std::string name_;
    std::string version_;
    std::string description_;
    
    mutable std::mutex stateMutex_;
    ServiceState state_;
    bool healthy_;
    std::string healthStatus_;
    
    mutable std::mutex configMutex_;
    std::unordered_map<std::string, std::string> config_;
    
    mutable std::mutex metricsMutex_;
    std::unordered_map<std::string, std::string> metrics_;
    
    StateChangeCallback stateChangeCallback_;
};

// Template implementations
template<typename T>
void ServiceRegistry::registerService(std::unique_ptr<T> service) {
    static_assert(std::is_base_of_v<IService, T>, "T must inherit from IService");
    registerService(std::shared_ptr<T>(service.release()));
}

template<typename T>
void ServiceRegistry::registerService(std::shared_ptr<T> service) {
    static_assert(std::is_base_of_v<IService, T>, "T must inherit from IService");
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    std::string name = service->getName();
    std::type_index type = std::type_index(typeid(T));
    
    services_[name] = {service, type, {}, {}, {}};
    servicesByType_[type].push_back(name);
}

template<typename T>
std::shared_ptr<T> ServiceRegistry::getService() const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    std::type_index type = std::type_index(typeid(T));
    
    auto it = servicesByType_.find(type);
    if (it != servicesByType_.end() && !it->second.empty()) {
        const std::string& name = it->second[0];
        auto serviceIt = services_.find(name);
        if (serviceIt != services_.end()) {
            return std::static_pointer_cast<T>(serviceIt->second.service);
        }
    }
    return nullptr;
}

template<typename T>
std::shared_ptr<T> ServiceRegistry::getService(const std::string& name) const {
    auto service = getService(name);
    return std::dynamic_pointer_cast<T>(service);
}

template<typename T>
bool ServiceRegistry::isServiceRegistered() const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    std::type_index type = std::type_index(typeid(T));
    auto it = servicesByType_.find(type);
    return it != servicesByType_.end() && !it->second.empty();
}

} // namespace core
} // namespace server
} // namespace astrocomm

#include "astrocomm/server/core/service_registry.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <queue>
#include <unordered_set>

namespace astrocomm {
namespace server {
namespace core {

// ServiceRegistry Implementation
ServiceRegistry& ServiceRegistry::getInstance() {
    static ServiceRegistry instance;
    return instance;
}

void ServiceRegistry::registerService(const std::string& name, std::shared_ptr<IService> service) {
    if (!service) {
        spdlog::error("Cannot register null service: {}", name);
        return;
    }
    
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    if (services_.find(name) != services_.end()) {
        spdlog::warn("Replacing existing service: {}", name);
    }
    
    ServiceInfo info;
    info.service = service;
    info.type = std::type_index(typeid(*service));
    info.config = globalConfig_;
    
    services_[name] = info;
    servicesByType_[info.type].push_back(name);
    
    spdlog::info("Registered service: {} ({})", name, service->getVersion());
}

void ServiceRegistry::unregisterService(const std::string& name) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(name);
    if (it != services_.end()) {
        // Remove from type index
        auto typeIt = servicesByType_.find(it->second.type);
        if (typeIt != servicesByType_.end()) {
            auto& names = typeIt->second;
            names.erase(std::remove(names.begin(), names.end(), name), names.end());
            if (names.empty()) {
                servicesByType_.erase(typeIt);
            }
        }
        
        spdlog::info("Unregistered service: {}", name);
        services_.erase(it);
    }
}

std::shared_ptr<IService> ServiceRegistry::getService(const std::string& name) const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(name);
    if (it != services_.end()) {
        return it->second.service;
    }
    
    return nullptr;
}

std::vector<std::string> ServiceRegistry::getRegisteredServices() const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    std::vector<std::string> names;
    names.reserve(services_.size());
    
    for (const auto& pair : services_) {
        names.push_back(pair.first);
    }
    
    return names;
}

std::vector<std::string> ServiceRegistry::getServicesByType(const std::type_index& type) const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = servicesByType_.find(type);
    if (it != servicesByType_.end()) {
        return it->second;
    }
    
    return {};
}

bool ServiceRegistry::isServiceRegistered(const std::string& name) const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    return services_.find(name) != services_.end();
}

bool ServiceRegistry::initializeAllServices() {
    auto services = getRegisteredServices();
    auto startupOrder = getStartupOrder();
    
    for (const auto& serviceName : startupOrder) {
        if (!initializeService(serviceName)) {
            spdlog::error("Failed to initialize service: {}", serviceName);
            return false;
        }
    }
    
    spdlog::info("All services initialized successfully");
    return true;
}

bool ServiceRegistry::startAllServices() {
    auto startupOrder = getStartupOrder();
    
    for (const auto& serviceName : startupOrder) {
        if (!startService(serviceName)) {
            spdlog::error("Failed to start service: {}", serviceName);
            return false;
        }
    }
    
    spdlog::info("All services started successfully");
    return true;
}

bool ServiceRegistry::stopAllServices() {
    auto startupOrder = getStartupOrder();
    
    // Stop in reverse order
    for (auto it = startupOrder.rbegin(); it != startupOrder.rend(); ++it) {
        if (!stopService(*it)) {
            spdlog::error("Failed to stop service: {}", *it);
            // Continue stopping other services
        }
    }
    
    spdlog::info("All services stopped");
    return true;
}

bool ServiceRegistry::shutdownAllServices() {
    auto startupOrder = getStartupOrder();
    
    // Shutdown in reverse order
    for (auto it = startupOrder.rbegin(); it != startupOrder.rend(); ++it) {
        if (!shutdownService(*it)) {
            spdlog::error("Failed to shutdown service: {}", *it);
            // Continue shutting down other services
        }
    }
    
    spdlog::info("All services shutdown");
    return true;
}

bool ServiceRegistry::initializeService(const std::string& name) {
    auto service = getService(name);
    if (!service) {
        spdlog::error("Service not found: {}", name);
        return false;
    }
    
    if (service->getState() != ServiceState::UNINITIALIZED) {
        spdlog::debug("Service {} already initialized", name);
        return true;
    }
    
    spdlog::info("Initializing service: {}", name);
    
    if (!service->initialize()) {
        spdlog::error("Failed to initialize service: {}", name);
        return false;
    }
    
    notifyStateChange(name, ServiceState::UNINITIALIZED, service->getState());
    return true;
}

bool ServiceRegistry::startService(const std::string& name) {
    auto service = getService(name);
    if (!service) {
        spdlog::error("Service not found: {}", name);
        return false;
    }
    
    if (service->getState() == ServiceState::RUNNING) {
        spdlog::debug("Service {} already running", name);
        return true;
    }
    
    if (service->getState() != ServiceState::INITIALIZED) {
        if (!initializeService(name)) {
            return false;
        }
    }
    
    spdlog::info("Starting service: {}", name);
    
    if (!service->start()) {
        spdlog::error("Failed to start service: {}", name);
        return false;
    }
    
    notifyStateChange(name, ServiceState::INITIALIZED, service->getState());
    return true;
}

bool ServiceRegistry::stopService(const std::string& name) {
    auto service = getService(name);
    if (!service) {
        spdlog::error("Service not found: {}", name);
        return false;
    }
    
    if (service->getState() != ServiceState::RUNNING) {
        spdlog::debug("Service {} not running", name);
        return true;
    }
    
    spdlog::info("Stopping service: {}", name);
    
    auto oldState = service->getState();
    if (!service->stop()) {
        spdlog::error("Failed to stop service: {}", name);
        return false;
    }
    
    notifyStateChange(name, oldState, service->getState());
    return true;
}

bool ServiceRegistry::shutdownService(const std::string& name) {
    auto service = getService(name);
    if (!service) {
        spdlog::error("Service not found: {}", name);
        return false;
    }
    
    spdlog::info("Shutting down service: {}", name);
    
    auto oldState = service->getState();
    if (!service->shutdown()) {
        spdlog::error("Failed to shutdown service: {}", name);
        return false;
    }
    
    notifyStateChange(name, oldState, service->getState());
    return true;
}

bool ServiceRegistry::resolveDependencies() {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    // Build dependency graph
    for (auto& pair : services_) {
        const auto& serviceName = pair.first;
        auto& serviceInfo = pair.second;
        
        auto dependencies = serviceInfo.service->getDependencies();
        for (const auto& dep : dependencies) {
            serviceInfo.dependencies.push_back(dep.serviceName);
            
            // Add reverse dependency
            auto depIt = services_.find(dep.serviceName);
            if (depIt != services_.end()) {
                depIt->second.dependents.push_back(serviceName);
            }
        }
    }
    
    return validateDependencies();
}

std::vector<std::string> ServiceRegistry::getServiceDependencies(const std::string& name) const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(name);
    if (it != services_.end()) {
        return it->second.dependencies;
    }
    
    return {};
}

std::vector<std::string> ServiceRegistry::getServiceDependents(const std::string& name) const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(name);
    if (it != services_.end()) {
        return it->second.dependents;
    }
    
    return {};
}

std::vector<std::string> ServiceRegistry::getStartupOrder() const {
    return topologicalSort();
}

std::unordered_map<std::string, ServiceState> ServiceRegistry::getServiceStates() const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    std::unordered_map<std::string, ServiceState> states;
    for (const auto& pair : services_) {
        states[pair.first] = pair.second.service->getState();
    }
    
    return states;
}

std::unordered_map<std::string, bool> ServiceRegistry::getServiceHealthStatus() const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    std::unordered_map<std::string, bool> healthStatus;
    for (const auto& pair : services_) {
        healthStatus[pair.first] = pair.second.service->isHealthy();
    }
    
    return healthStatus;
}

bool ServiceRegistry::areAllServicesHealthy() const {
    auto healthStatus = getServiceHealthStatus();
    
    for (const auto& pair : healthStatus) {
        if (!pair.second) {
            return false;
        }
    }
    
    return true;
}

void ServiceRegistry::setGlobalConfiguration(const std::unordered_map<std::string, std::string>& config) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    globalConfig_ = config;
}

void ServiceRegistry::setServiceConfiguration(const std::string& name, 
                                             const std::unordered_map<std::string, std::string>& config) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(name);
    if (it != services_.end()) {
        it->second.config = config;
        it->second.service->setConfiguration(config);
    }
}

void ServiceRegistry::setServiceEventCallback(ServiceEventCallback callback) {
    eventCallback_ = callback;
}

void ServiceRegistry::registerFactory(std::unique_ptr<IServiceFactory> factory) {
    if (factory) {
        factories_.push_back(std::move(factory));
    }
}

std::unique_ptr<IService> ServiceRegistry::createService(const std::string& serviceName,
                                                        const std::unordered_map<std::string, std::string>& config) {
    for (auto& factory : factories_) {
        if (factory->isServiceSupported(serviceName)) {
            return factory->createService(serviceName, config);
        }
    }
    
    spdlog::error("No factory found for service: {}", serviceName);
    return nullptr;
}

bool ServiceRegistry::validateDependencies() const {
    // Check for circular dependencies using DFS
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursionStack;
    
    for (const auto& pair : services_) {
        if (visited.find(pair.first) == visited.end()) {
            if (hasCyclicDependency(pair.first, visited, recursionStack)) {
                spdlog::error("Circular dependency detected involving service: {}", pair.first);
                return false;
            }
        }
    }
    
    return true;
}

bool ServiceRegistry::hasCyclicDependency(const std::string& serviceName,
                                         std::unordered_set<std::string>& visited,
                                         std::unordered_set<std::string>& recursionStack) const {
    visited.insert(serviceName);
    recursionStack.insert(serviceName);
    
    auto it = services_.find(serviceName);
    if (it != services_.end()) {
        for (const auto& dependency : it->second.dependencies) {
            if (recursionStack.find(dependency) != recursionStack.end()) {
                return true; // Cycle detected
            }
            
            if (visited.find(dependency) == visited.end()) {
                if (hasCyclicDependency(dependency, visited, recursionStack)) {
                    return true;
                }
            }
        }
    }
    
    recursionStack.erase(serviceName);
    return false;
}

std::vector<std::string> ServiceRegistry::topologicalSort() const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    std::unordered_map<std::string, int> inDegree;
    std::queue<std::string> queue;
    std::vector<std::string> result;
    
    // Initialize in-degree count
    for (const auto& pair : services_) {
        inDegree[pair.first] = pair.second.dependencies.size();
        if (inDegree[pair.first] == 0) {
            queue.push(pair.first);
        }
    }
    
    // Process queue
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        result.push_back(current);
        
        auto it = services_.find(current);
        if (it != services_.end()) {
            for (const auto& dependent : it->second.dependents) {
                inDegree[dependent]--;
                if (inDegree[dependent] == 0) {
                    queue.push(dependent);
                }
            }
        }
    }
    
    return result;
}

void ServiceRegistry::notifyStateChange(const std::string& serviceName, ServiceState oldState, ServiceState newState) {
    if (eventCallback_) {
        eventCallback_(serviceName, oldState, newState);
    }
}

// BaseService Implementation
BaseService::BaseService(const std::string& name, const std::string& version)
    : name_(name), version_(version), state_(ServiceState::UNINITIALIZED), healthy_(true) {
}

std::string BaseService::getName() const {
    return name_;
}

std::string BaseService::getVersion() const {
    return version_;
}

std::string BaseService::getDescription() const {
    return description_;
}

ServiceState BaseService::getState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

bool BaseService::isHealthy() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return healthy_;
}

std::string BaseService::getHealthStatus() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return healthStatus_;
}

std::unordered_map<std::string, std::string> BaseService::getMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return metrics_;
}

void BaseService::setConfiguration(const std::unordered_map<std::string, std::string>& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

std::unordered_map<std::string, std::string> BaseService::getConfiguration() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void BaseService::setStateChangeCallback(StateChangeCallback callback) {
    stateChangeCallback_ = callback;
}

void BaseService::setState(ServiceState newState) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto oldState = state_;
    state_ = newState;
    
    if (stateChangeCallback_) {
        stateChangeCallback_(oldState, newState);
    }
}

void BaseService::setHealthy(bool healthy) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    healthy_ = healthy;
}

void BaseService::setHealthStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    healthStatus_ = status;
}

void BaseService::updateMetric(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    metrics_[name] = value;
}

std::string BaseService::getConfigValue(const std::string& key, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    auto it = config_.find(key);
    return (it != config_.end()) ? it->second : defaultValue;
}

bool BaseService::getConfigBool(const std::string& key, bool defaultValue) const {
    auto value = getConfigValue(key);
    if (value.empty()) {
        return defaultValue;
    }
    
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

int BaseService::getConfigInt(const std::string& key, int defaultValue) const {
    auto value = getConfigValue(key);
    if (value.empty()) {
        return defaultValue;
    }
    
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        return defaultValue;
    }
}

} // namespace core
} // namespace server
} // namespace astrocomm

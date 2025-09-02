#include "hydrogen/server/core/service_registry.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>

namespace hydrogen {
namespace server {
namespace core {

// BaseService implementation
BaseService::BaseService(const std::string& name, const std::string& version)
    : name_(name), version_(version), state_(ServiceState::STOPPED) {
    spdlog::debug("BaseService created: {} v{}", name_, version_);
}

BaseService::~BaseService() {
    spdlog::debug("BaseService destroyed: {}", name_);
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
    return state_;
}

bool BaseService::isHealthy() const {
    return state_ == ServiceState::RUNNING;
}

std::string BaseService::getHealthStatus() const {
    switch (state_) {
        case ServiceState::STOPPED: return "Stopped";
        case ServiceState::STARTING: return "Starting";
        case ServiceState::RUNNING: return "Running";
        case ServiceState::STOPPING: return "Stopping";
        case ServiceState::ERROR: return "Error";
        case ServiceState::INITIALIZED: return "Initialized";
        default: return "Unknown";
    }
}

std::unordered_map<std::string, std::string> BaseService::getMetrics() const {
    std::unordered_map<std::string, std::string> metrics;
    metrics["state"] = getHealthStatus();
    metrics["uptime"] = std::to_string(getUptime());
    return metrics;
}

void BaseService::setConfiguration(const std::unordered_map<std::string, std::string>& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
    spdlog::debug("Configuration updated for service: {}", name_);
}

std::unordered_map<std::string, std::string> BaseService::getConfiguration() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void BaseService::setStateChangeCallback(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    stateChangeCallback_ = callback;
}

std::vector<ServiceDependency> BaseService::getDependencies() const {
    return dependencies_;
}

bool BaseService::areDependenciesSatisfied() const {
    return true; // Base implementation assumes no dependencies
}

bool BaseService::shutdown() {
    return stop();
}

int64_t BaseService::getUptime() const {
    if (state_ != ServiceState::RUNNING) {
        return 0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime_);
    return duration.count();
}

void BaseService::setState(ServiceState newState) {
    ServiceState oldState = state_;
    state_ = newState;
    
    if (newState == ServiceState::RUNNING) {
        startTime_ = std::chrono::steady_clock::now();
    }
    
    // Notify state change
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (stateChangeCallback_) {
            try {
                stateChangeCallback_(name_, oldState, newState);
            } catch (const std::exception& e) {
                spdlog::error("Error in state change callback for {}: {}", name_, e.what());
            }
        }
    }
    
    spdlog::info("Service {} state changed: {} -> {}", name_, 
                 static_cast<int>(oldState), static_cast<int>(newState));
}

void BaseService::setDescription(const std::string& description) {
    description_ = description;
}

void BaseService::addDependency(const ServiceDependency& dependency) {
    dependencies_.push_back(dependency);
}

void BaseService::removeDependency(const std::string& serviceName) {
    dependencies_.erase(
        std::remove_if(dependencies_.begin(), dependencies_.end(),
            [&serviceName](const ServiceDependency& dep) {
                return dep.serviceName == serviceName;
            }),
        dependencies_.end());
}

// ServiceRegistry implementation
ServiceRegistry::ServiceRegistry() {
    spdlog::info("Service registry created");
}

ServiceRegistry::~ServiceRegistry() {
    stopAll();
    spdlog::info("Service registry destroyed");
}

bool ServiceRegistry::registerService(const std::string& name, std::shared_ptr<IService> service) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    if (services_.find(name) != services_.end()) {
        spdlog::warn("Service already registered: {}", name);
        return false;
    }
    
    ServiceInfo info;
    info.service = service;
    info.type = std::type_index(typeid(*service));
    
    services_[name] = info;
    spdlog::info("Service registered: {}", name);
    return true;
}

bool ServiceRegistry::unregisterService(const std::string& name) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(name);
    if (it == services_.end()) {
        spdlog::warn("Service not found for unregistration: {}", name);
        return false;
    }
    
    // Stop service if running
    if (it->second.service->getState() == ServiceState::RUNNING) {
        it->second.service->stop();
    }
    
    services_.erase(it);
    spdlog::info("Service unregistered: {}", name);
    return true;
}

std::shared_ptr<IService> ServiceRegistry::getService(const std::string& name) const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(name);
    if (it != services_.end()) {
        return it->second.service;
    }
    
    return nullptr;
}

std::vector<std::string> ServiceRegistry::getServiceNames() const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : services_) {
        names.push_back(pair.first);
    }
    return names;
}

bool ServiceRegistry::startService(const std::string& name) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(name);
    if (it == services_.end()) {
        spdlog::error("Service not found: {}", name);
        return false;
    }
    
    try {
        return it->second.service->start();
    } catch (const std::exception& e) {
        spdlog::error("Failed to start service {}: {}", name, e.what());
        return false;
    }
}

bool ServiceRegistry::stopService(const std::string& name) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    auto it = services_.find(name);
    if (it == services_.end()) {
        spdlog::error("Service not found: {}", name);
        return false;
    }
    
    try {
        return it->second.service->stop();
    } catch (const std::exception& e) {
        spdlog::error("Failed to stop service {}: {}", name, e.what());
        return false;
    }
}

bool ServiceRegistry::restartService(const std::string& name) {
    return stopService(name) && startService(name);
}

bool ServiceRegistry::startAll() {
    auto startupOrder = getStartupOrder();
    
    for (const auto& serviceName : startupOrder) {
        if (!startService(serviceName)) {
            spdlog::error("Failed to start service in startup sequence: {}", serviceName);
            return false;
        }
    }
    
    spdlog::info("All services started successfully");
    return true;
}

bool ServiceRegistry::stopAll() {
    auto startupOrder = getStartupOrder();
    
    // Stop in reverse order
    for (auto it = startupOrder.rbegin(); it != startupOrder.rend(); ++it) {
        stopService(*it);
    }
    
    spdlog::info("All services stopped");
    return true;
}

bool ServiceRegistry::validateDependencies() const {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    
    for (const auto& pair : services_) {
        const std::string& serviceName = pair.first;
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> recursionStack;
        
        if (hasCyclicDependency(serviceName, visited, recursionStack)) {
            spdlog::error("Cyclic dependency detected for service: {}", serviceName);
            return false;
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
        auto dependencies = it->second.service->getDependencies();
        
        for (const auto& dep : dependencies) {
            if (recursionStack.find(dep.serviceName) != recursionStack.end()) {
                return true; // Cycle detected
            }
            
            if (visited.find(dep.serviceName) == visited.end()) {
                if (hasCyclicDependency(dep.serviceName, visited, recursionStack)) {
                    return true;
                }
            }
        }
    }
    
    recursionStack.erase(serviceName);
    return false;
}

std::vector<std::string> ServiceRegistry::getStartupOrder() const {
    // Simple implementation - in a real system, this would do topological sorting
    std::vector<std::string> order;
    
    std::lock_guard<std::mutex> lock(servicesMutex_);
    for (const auto& pair : services_) {
        order.push_back(pair.first);
    }
    
    return order;
}

} // namespace core
} // namespace server
} // namespace hydrogen

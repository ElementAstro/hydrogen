#include "hydrogen/server/services/health_service.h"
#include "hydrogen/server/core/service_registry.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>

namespace hydrogen {
namespace server {
namespace services {

/**
 * @brief Concrete implementation of the Health Service
 */
class HealthServiceImpl : public core::BaseService, public IHealthService {
public:
    explicit HealthServiceImpl(const std::string& name = "HealthService")
        : core::BaseService(name, "1.0.0") {
        description_ = "Health monitoring service for Hydrogen server";
    }

    // IService implementation
    bool initialize() override {
        setState(core::ServiceState::INITIALIZING);
        
        spdlog::info("Initializing Health Service...");
        
        // Initialize internal data structures
        healthChecks_.clear();
        systemMetrics_ = SystemMetrics{};
        
        // Set up default health check interval
        healthCheckInterval_ = std::chrono::seconds(30);
        
        setState(core::ServiceState::INITIALIZED);
        setHealthy(true);
        setHealthStatus("Health service initialized successfully");
        
        spdlog::info("Health Service initialized");
        return true;
    }

    bool start() override {
        if (getState() != core::ServiceState::INITIALIZED) {
            spdlog::error("Health Service not initialized");
            return false;
        }

        setState(core::ServiceState::STARTING);
        spdlog::info("Starting Health Service...");

        // Start health monitoring
        running_ = true;
        healthMonitorThread_ = std::thread(&HealthServiceImpl::healthMonitoringLoop, this);

        setState(core::ServiceState::RUNNING);
        spdlog::info("Health Service started");
        return true;
    }

    bool stop() override {
        if (getState() != core::ServiceState::RUNNING) {
            return true;
        }

        setState(core::ServiceState::STOPPING);
        spdlog::info("Stopping Health Service...");

        // Stop health monitoring
        running_ = false;
        if (healthMonitorThread_.joinable()) {
            healthMonitorThread_.join();
        }

        setState(core::ServiceState::STOPPED);
        spdlog::info("Health Service stopped");
        return true;
    }

    bool restart() {
        return stop() && start();
    }

    // IHealthService implementation
    HealthStatus getOverallHealthStatus() const override {
        std::lock_guard<std::mutex> lock(healthMutex_);
        
        if (healthChecks_.empty()) {
            return HealthStatus::UNKNOWN;
        }
        
        bool hasHealthy = false;
        bool hasWarning = false;
        bool hasCritical = false;
        
        for (const auto& pair : healthChecks_) {
            const auto& check = pair.second;
            switch (check.status) {
                case HealthStatus::HEALTHY:
                    hasHealthy = true;
                    break;
                case HealthStatus::WARNING:
                    hasWarning = true;
                    break;
                case HealthStatus::CRITICAL:
                case HealthStatus::UNHEALTHY:
                    hasCritical = true;
                    break;
                default:
                    break;
            }
        }
        
        if (hasCritical) return HealthStatus::CRITICAL;
        if (hasWarning) return HealthStatus::WARNING;
        if (hasHealthy) return HealthStatus::HEALTHY;
        
        return HealthStatus::UNKNOWN;
    }

    std::vector<HealthCheck> getAllHealthChecks() const {
        std::lock_guard<std::mutex> lock(healthMutex_);
        
        std::vector<HealthCheck> result;
        for (const auto& pair : healthChecks_) {
            result.push_back(pair.second);
        }
        
        return result;
    }

    HealthCheck getHealthCheck(const std::string& componentId) const {
        std::lock_guard<std::mutex> lock(healthMutex_);
        
        auto it = healthChecks_.find(componentId);
        if (it != healthChecks_.end()) {
            return it->second;
        }
        
        // Return empty health check if not found
        HealthCheck emptyCheck;
        emptyCheck.componentId = componentId;
        emptyCheck.status = HealthStatus::UNKNOWN;
        emptyCheck.message = "Component not found";
        return emptyCheck;
    }

    bool registerHealthCheck(const std::string& componentId,
                           std::function<HealthCheck()> checkFunction) {
        std::lock_guard<std::mutex> lock(healthMutex_);
        
        HealthCheck check;
        check.componentId = componentId;
        check.status = HealthStatus::UNKNOWN;
        check.message = "Registered but not yet checked";
        check.lastChecked = std::chrono::system_clock::now();
        
        healthChecks_[componentId] = check;
        healthCheckFunctions_[componentId] = checkFunction;
        
        spdlog::info("Registered health check for component: {}", componentId);
        return true;
    }

    bool unregisterHealthCheck(const std::string& componentId) override {
        std::lock_guard<std::mutex> lock(healthMutex_);
        
        auto it = healthChecks_.find(componentId);
        if (it != healthChecks_.end()) {
            healthChecks_.erase(it);
            healthCheckFunctions_.erase(componentId);
            spdlog::info("Unregistered health check for component: {}", componentId);
            return true;
        }
        
        return false;
    }

    bool performHealthCheck(const std::string& componentId) {
        std::lock_guard<std::mutex> lock(healthMutex_);
        
        auto funcIt = healthCheckFunctions_.find(componentId);
        if (funcIt != healthCheckFunctions_.end()) {
            try {
                HealthCheck result = funcIt->second();
                result.lastChecked = std::chrono::system_clock::now();
                healthChecks_[componentId] = result;
                return true;
            } catch (const std::exception& e) {
                HealthCheck errorCheck;
                errorCheck.componentId = componentId;
                errorCheck.status = HealthStatus::CRITICAL;
                errorCheck.message = "Health check failed: " + std::string(e.what());
                errorCheck.lastChecked = std::chrono::system_clock::now();
                healthChecks_[componentId] = errorCheck;
                return false;
            }
        }
        
        return false;
    }

    void performAllHealthChecks() {
        std::lock_guard<std::mutex> lock(healthMutex_);
        
        for (const auto& pair : healthCheckFunctions_) {
            const std::string& componentId = pair.first;
            try {
                HealthCheck result = pair.second();
                result.lastChecked = std::chrono::system_clock::now();
                healthChecks_[componentId] = result;
            } catch (const std::exception& e) {
                HealthCheck errorCheck;
                errorCheck.componentId = componentId;
                errorCheck.status = HealthStatus::CRITICAL;
                errorCheck.message = "Health check failed: " + std::string(e.what());
                errorCheck.lastChecked = std::chrono::system_clock::now();
                healthChecks_[componentId] = errorCheck;
            }
        }
    }

    SystemMetrics getSystemMetrics() const override {
        return systemMetrics_;
    }

    void updateSystemMetrics() {
        // Simulate system metrics collection
        systemMetrics_.cpuUsagePercent = 25.5;
        systemMetrics_.usedMemoryBytes = 512 * 1024 * 1024; // 512 MB
        systemMetrics_.usedDiskBytes = 10LL * 1024 * 1024 * 1024; // 10 GB
        systemMetrics_.networkBytesReceived = 1024 * 1024; // 1 MB
        systemMetrics_.networkBytesSent = 2 * 1024 * 1024; // 2 MB
        systemMetrics_.uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime_);
        systemMetrics_.timestamp = std::chrono::system_clock::now();
    }

    void setHealthCheckInterval(std::chrono::seconds interval) override {
        healthCheckInterval_ = interval;
        spdlog::info("Health check interval set to {} seconds", interval.count());
    }

    std::chrono::seconds getHealthCheckInterval() const {
        return healthCheckInterval_;
    }

    std::vector<HealthAlert> getActiveAlerts() const override {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        
        std::vector<HealthAlert> result;
        for (const auto& pair : activeAlerts_) {
            result.push_back(pair.second);
        }
        
        return result;
    }

    bool acknowledgeAlert(const std::string& alertId) override {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        
        auto it = activeAlerts_.find(alertId);
        if (it != activeAlerts_.end()) {
            it->second.acknowledged = true;
            it->second.acknowledgedAt = std::chrono::system_clock::now();
            spdlog::info("Alert acknowledged: {}", alertId);
            return true;
        }
        
        return false;
    }

    bool clearAlert(const std::string& alertId) {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        
        auto it = activeAlerts_.find(alertId);
        if (it != activeAlerts_.end()) {
            activeAlerts_.erase(it);
            spdlog::info("Alert cleared: {}", alertId);
            return true;
        }
        
        return false;
    }

private:
    mutable std::mutex healthMutex_;
    mutable std::mutex alertsMutex_;
    
    std::unordered_map<std::string, HealthCheck> healthChecks_;
    std::unordered_map<std::string, std::function<HealthCheck()>> healthCheckFunctions_;
    std::unordered_map<std::string, HealthAlert> activeAlerts_;
    
    SystemMetrics systemMetrics_;
    std::chrono::seconds healthCheckInterval_{30};
    
    std::atomic<bool> running_{false};
    std::thread healthMonitorThread_;
    std::chrono::steady_clock::time_point startTime_{std::chrono::steady_clock::now()};
    
    std::string description_;

    void healthMonitoringLoop() {
        while (running_) {
            performAllHealthChecks();
            updateSystemMetrics();
            checkForAlerts();
            std::this_thread::sleep_for(healthCheckInterval_);
        }
    }

    void checkForAlerts() {
        std::lock_guard<std::mutex> lock(healthMutex_);
        
        for (const auto& pair : healthChecks_) {
            const auto& check = pair.second;
            if (check.status == HealthStatus::CRITICAL || check.status == HealthStatus::UNHEALTHY) {
                createAlert(check);
            }
        }
    }

    void createAlert(const HealthCheck& check) {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        
        std::string alertId = "alert_" + check.componentId + "_" + 
                             std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::system_clock::now().time_since_epoch()).count());
        
        HealthAlert alert;
        alert.id = alertId;
        alert.componentId = check.componentId;
        alert.severity = (check.status == HealthStatus::CRITICAL) ? AlertSeverity::CRITICAL : AlertSeverity::WARNING;
        alert.message = check.message;
        alert.createdAt = std::chrono::system_clock::now();
        alert.acknowledged = false;
        
        activeAlerts_[alertId] = alert;
        spdlog::warn("Health alert created: {} for component {}", alertId, check.componentId);
    }
};

// Factory function implementation
std::unique_ptr<core::IService> HealthServiceFactory::createService(
    const std::string& serviceName,
    const std::unordered_map<std::string, std::string>& config) {
    // TODO: Fix abstract class issue before enabling instantiation
    // if (serviceName == "health") {
    //     return std::make_unique<HealthServiceImpl>();
    // }
    return nullptr;
}

std::vector<std::string> HealthServiceFactory::getSupportedServices() const {
    return {"health"};
}

bool HealthServiceFactory::isServiceSupported(const std::string& serviceName) const {
    return serviceName == "health";
}

} // namespace services
} // namespace server
} // namespace hydrogen

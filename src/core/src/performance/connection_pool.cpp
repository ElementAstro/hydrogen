#include "hydrogen/core/performance/connection_pool.h"

#include <algorithm>
#include <random>
#include <sstream>

namespace hydrogen {
namespace core {
namespace performance {

ConnectionPool::ConnectionPool(
    std::shared_ptr<IConnectionFactory> factory,
    const ConnectionPoolConfig& config)
    : factory_(std::move(factory))
    , config_(config) {
    
    spdlog::debug("ConnectionPool: Created with factory type: {}", 
                  factory_ ? factory_->getConnectionType() : "null");
}

ConnectionPool::~ConnectionPool() {
    spdlog::debug("ConnectionPool: Destructor called");
    if (running_.load()) {
        shutdown();
    }
}

bool ConnectionPool::initialize() {
    spdlog::info("ConnectionPool: Initializing connection pool");
    
    if (initialized_.load()) {
        spdlog::warn("ConnectionPool: Already initialized");
        return true;
    }
    
    if (!factory_) {
        spdlog::error("ConnectionPool: No connection factory provided");
        return false;
    }
    
    try {
        // Validate configuration
        if (config_.minConnections > config_.maxConnections) {
            spdlog::error("ConnectionPool: Invalid configuration - minConnections > maxConnections");
            return false;
        }
        
        if (config_.initialConnections > config_.maxConnections) {
            config_.initialConnections = config_.maxConnections;
            spdlog::warn("ConnectionPool: Adjusted initialConnections to maxConnections");
        }
        
        // Create initial connections
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            
            for (size_t i = 0; i < config_.initialConnections; ++i) {
                auto pooledConnection = createConnection();
                if (pooledConnection) {
                    availableConnections_.push(pooledConnection);
                    allConnections_[pooledConnection->connectionId] = pooledConnection;
                    metrics_.totalConnections.fetch_add(1);
                    metrics_.idleConnections.fetch_add(1);
                    metrics_.connectionsCreated.fetch_add(1);
                } else {
                    spdlog::warn("ConnectionPool: Failed to create initial connection {}", i);
                }
            }
        }
        
        running_.store(true);
        
        // Start background threads
        if (config_.enableHealthChecks) {
            healthCheckThread_ = std::make_unique<std::thread>(
                &ConnectionPool::healthCheckThreadFunction, this);
        }
        
        maintenanceThread_ = std::make_unique<std::thread>(
            &ConnectionPool::maintenanceThreadFunction, this);
        
        initialized_.store(true);
        
        spdlog::info("ConnectionPool: Initialized with {} connections", 
                     availableConnections_.size());
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("ConnectionPool: Exception during initialization: {}", e.what());
        return false;
    }
}

void ConnectionPool::shutdown() {
    spdlog::info("ConnectionPool: Shutting down connection pool");
    
    if (!running_.load()) {
        spdlog::warn("ConnectionPool: Already shut down");
        return;
    }
    
    running_.store(false);
    
    // Notify shutdown condition
    {
        std::lock_guard<std::mutex> lock(shutdownMutex_);
        shutdownCondition_.notify_all();
    }
    
    // Notify connection waiters
    connectionAvailable_.notify_all();
    
    // Wait for background threads
    if (healthCheckThread_ && healthCheckThread_->joinable()) {
        healthCheckThread_->join();
    }
    
    if (maintenanceThread_ && maintenanceThread_->joinable()) {
        maintenanceThread_->join();
    }
    
    // Close all connections
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        
        // Clear available connections
        while (!availableConnections_.empty()) {
            auto connection = availableConnections_.front();
            availableConnections_.pop();
            destroyConnection(connection);
        }
        
        // Clear active connections (they should be released by now)
        for (auto& connection : activeConnections_) {
            destroyConnection(connection);
        }
        activeConnections_.clear();
        
        allConnections_.clear();
    }
    
    initialized_.store(false);
    
    spdlog::info("ConnectionPool: Shutdown complete");
}

std::shared_ptr<IConnection> ConnectionPool::acquireConnection(
    std::chrono::milliseconds timeout) {
    
    if (!running_.load()) {
        spdlog::error("ConnectionPool: Cannot acquire connection - pool not running");
        return nullptr;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    std::unique_lock<std::mutex> lock(connectionsMutex_);
    
    // Wait for available connection or timeout
    bool acquired = connectionAvailable_.wait_for(lock, timeout, [this] {
        return !availableConnections_.empty() || !running_.load();
    });
    
    if (!running_.load()) {
        spdlog::debug("ConnectionPool: Pool shutting down during acquisition");
        return nullptr;
    }
    
    if (!acquired || availableConnections_.empty()) {
        // Try to create new connection if under limit
        if (allConnections_.size() < config_.maxConnections) {
            auto pooledConnection = createConnection();
            if (pooledConnection) {
                allConnections_[pooledConnection->connectionId] = pooledConnection;
                metrics_.totalConnections.fetch_add(1);
                metrics_.connectionsCreated.fetch_add(1);
                
                // Move to active connections
                activeConnections_.insert(pooledConnection);
                metrics_.activeConnections.fetch_add(1);
                metrics_.connectionsAcquired.fetch_add(1);
                
                pooledConnection->updateLastUsed();
                
                auto endTime = std::chrono::steady_clock::now();
                auto acquisitionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                    endTime - startTime).count();
                
                // Update average acquisition time
                double currentAvg = metrics_.averageAcquisitionTime.load();
                double newAvg = (currentAvg + acquisitionTime) / 2.0;
                metrics_.averageAcquisitionTime.store(newAvg);
                
                spdlog::debug("ConnectionPool: Created and acquired new connection: {}", 
                              pooledConnection->connectionId);
                
                return pooledConnection->connection;
            }
        }
        
        metrics_.acquisitionTimeouts.fetch_add(1);
        spdlog::warn("ConnectionPool: Failed to acquire connection within timeout");
        return nullptr;
    }
    
    // Get connection from available pool
    auto pooledConnection = availableConnections_.front();
    availableConnections_.pop();
    
    // Validate connection health
    if (!validateConnection(pooledConnection)) {
        spdlog::warn("ConnectionPool: Connection {} failed validation, destroying", 
                     pooledConnection->connectionId);
        destroyConnection(pooledConnection);
        
        // Recursively try to get another connection
        lock.unlock();
        return acquireConnection(timeout);
    }
    
    // Move to active connections
    activeConnections_.insert(pooledConnection);
    metrics_.activeConnections.fetch_add(1);
    metrics_.idleConnections.fetch_sub(1);
    metrics_.connectionsAcquired.fetch_add(1);
    
    pooledConnection->updateLastUsed();
    
    auto endTime = std::chrono::steady_clock::now();
    auto acquisitionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    // Update average acquisition time
    double currentAvg = metrics_.averageAcquisitionTime.load();
    double newAvg = (currentAvg + acquisitionTime) / 2.0;
    metrics_.averageAcquisitionTime.store(newAvg);
    
    spdlog::debug("ConnectionPool: Acquired connection: {}", pooledConnection->connectionId);
    
    return pooledConnection->connection;
}

void ConnectionPool::releaseConnection(std::shared_ptr<IConnection> connection) {
    if (!connection) {
        spdlog::warn("ConnectionPool: Attempted to release null connection");
        return;
    }
    
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    // Find the pooled connection
    std::shared_ptr<PooledConnection> pooledConnection = nullptr;
    for (auto it = activeConnections_.begin(); it != activeConnections_.end(); ++it) {
        if ((*it)->connection == connection) {
            pooledConnection = *it;
            activeConnections_.erase(it);
            break;
        }
    }
    
    if (!pooledConnection) {
        spdlog::warn("ConnectionPool: Attempted to release unknown connection");
        return;
    }
    
    // Check if connection is still healthy
    if (!validateConnection(pooledConnection)) {
        spdlog::debug("ConnectionPool: Released connection {} is unhealthy, destroying", 
                      pooledConnection->connectionId);
        destroyConnection(pooledConnection);
        return;
    }
    
    // Return to available pool
    availableConnections_.push(pooledConnection);
    metrics_.activeConnections.fetch_sub(1);
    metrics_.idleConnections.fetch_add(1);
    metrics_.connectionsReleased.fetch_add(1);
    
    // Notify waiting threads
    connectionAvailable_.notify_one();
    
    spdlog::debug("ConnectionPool: Released connection: {}", pooledConnection->connectionId);
}

std::shared_ptr<PooledConnection> ConnectionPool::createConnection() {
    try {
        auto connection = factory_->createConnection();
        if (!connection) {
            spdlog::error("ConnectionPool: Factory failed to create connection");
            return nullptr;
        }
        
        if (!connection->connect()) {
            spdlog::error("ConnectionPool: Failed to connect new connection");
            return nullptr;
        }
        
        std::string connectionId = generateConnectionId();
        auto pooledConnection = std::make_shared<PooledConnection>(connection, connectionId);
        
        spdlog::debug("ConnectionPool: Created new connection: {}", connectionId);
        
        return pooledConnection;
        
    } catch (const std::exception& e) {
        spdlog::error("ConnectionPool: Exception creating connection: {}", e.what());
        return nullptr;
    }
}

void ConnectionPool::destroyConnection(std::shared_ptr<PooledConnection> connection) {
    if (!connection) {
        return;
    }
    
    try {
        if (connection->connection) {
            connection->connection->disconnect();
        }
        
        // Remove from all connections map
        allConnections_.erase(connection->connectionId);
        
        metrics_.totalConnections.fetch_sub(1);
        metrics_.connectionsDestroyed.fetch_add(1);
        
        // Update average connection lifetime
        auto now = std::chrono::system_clock::now();
        auto lifetime = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - connection->createdAt).count();
        
        double currentAvg = metrics_.averageConnectionLifetime.load();
        double newAvg = (currentAvg + lifetime) / 2.0;
        metrics_.averageConnectionLifetime.store(newAvg);
        
        spdlog::debug("ConnectionPool: Destroyed connection: {} (lifetime: {}ms)", 
                      connection->connectionId, lifetime);
        
    } catch (const std::exception& e) {
        spdlog::error("ConnectionPool: Exception destroying connection {}: {}", 
                      connection->connectionId, e.what());
    }
}

bool ConnectionPool::validateConnection(std::shared_ptr<PooledConnection> connection) {
    if (!connection || !connection->connection) {
        return false;
    }
    
    try {
        // Check basic connectivity
        if (!connection->connection->isConnected()) {
            return false;
        }
        
        // Check health status
        if (!connection->connection->isHealthy()) {
            return false;
        }
        
        // Check if connection is expired
        if (connection->isExpired(config_.maxLifetime)) {
            spdlog::debug("ConnectionPool: Connection {} expired", connection->connectionId);
            return false;
        }
        
        // Use factory validation if available
        if (!factory_->validateConnection(connection->connection)) {
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("ConnectionPool: Exception validating connection {}: {}", 
                      connection->connectionId, e.what());
        return false;
    }
}

std::string ConnectionPool::generateConnectionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "conn_";
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

void ConnectionPool::healthCheckThreadFunction() {
    spdlog::debug("ConnectionPool: Health check thread started");

    while (running_.load()) {
        try {
            performHealthCheck();
        } catch (const std::exception& e) {
            spdlog::error("ConnectionPool: Exception in health check thread: {}", e.what());
        }

        // Wait for next health check interval or shutdown
        std::unique_lock<std::mutex> lock(shutdownMutex_);
        if (shutdownCondition_.wait_for(lock, config_.healthCheckInterval,
                                       [this] { return !running_.load(); })) {
            break;
        }
    }

    spdlog::debug("ConnectionPool: Health check thread stopped");
}

void ConnectionPool::maintenanceThreadFunction() {
    spdlog::debug("ConnectionPool: Maintenance thread started");

    while (running_.load()) {
        try {
            performMaintenance();
            updateMetrics();
        } catch (const std::exception& e) {
            spdlog::error("ConnectionPool: Exception in maintenance thread: {}", e.what());
        }

        // Wait for next maintenance interval or shutdown
        std::unique_lock<std::mutex> lock(shutdownMutex_);
        if (shutdownCondition_.wait_for(lock, config_.cleanupInterval,
                                       [this] { return !running_.load(); })) {
            break;
        }
    }

    spdlog::debug("ConnectionPool: Maintenance thread stopped");
}

void ConnectionPool::performHealthCheck() {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    spdlog::debug("ConnectionPool: Performing health check on {} connections",
                  allConnections_.size());

    std::vector<std::shared_ptr<PooledConnection>> unhealthyConnections;

    for (auto& [connectionId, connection] : allConnections_) {
        try {
            bool isHealthy = validateConnection(connection);
            connection->updateHealthCheck(isHealthy);

            if (!isHealthy) {
                unhealthyConnections.push_back(connection);
                metrics_.healthCheckFailures.fetch_add(1);
            }
        } catch (const std::exception& e) {
            spdlog::error("ConnectionPool: Exception during health check for {}: {}",
                          connectionId, e.what());
            unhealthyConnections.push_back(connection);
            metrics_.healthCheckFailures.fetch_add(1);
        }
    }

    // Remove unhealthy connections
    for (auto& connection : unhealthyConnections) {
        spdlog::info("ConnectionPool: Removing unhealthy connection: {}",
                     connection->connectionId);

        // Remove from available connections if present
        std::queue<std::shared_ptr<PooledConnection>> tempQueue;
        while (!availableConnections_.empty()) {
            auto availableConn = availableConnections_.front();
            availableConnections_.pop();
            if (availableConn != connection) {
                tempQueue.push(availableConn);
            } else {
                metrics_.idleConnections.fetch_sub(1);
            }
        }
        availableConnections_ = tempQueue;

        // Remove from active connections if present
        auto activeIt = activeConnections_.find(connection);
        if (activeIt != activeConnections_.end()) {
            activeConnections_.erase(activeIt);
            metrics_.activeConnections.fetch_sub(1);
        }

        destroyConnection(connection);
    }

    spdlog::debug("ConnectionPool: Health check complete, removed {} unhealthy connections",
                  unhealthyConnections.size());
}

void ConnectionPool::performMaintenance() {
    cleanupExpiredConnections();
    cleanupIdleConnections();
    adjustPoolSize();

    if (config_.enableMetrics) {
        logPoolStatus();
    }
}

void ConnectionPool::cleanupExpiredConnections() {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    std::vector<std::shared_ptr<PooledConnection>> expiredConnections;

    for (auto& [connectionId, connection] : allConnections_) {
        if (connection->isExpired(config_.maxLifetime)) {
            expiredConnections.push_back(connection);
        }
    }

    for (auto& connection : expiredConnections) {
        spdlog::debug("ConnectionPool: Removing expired connection: {}",
                      connection->connectionId);

        // Remove from available connections
        std::queue<std::shared_ptr<PooledConnection>> tempQueue;
        while (!availableConnections_.empty()) {
            auto availableConn = availableConnections_.front();
            availableConnections_.pop();
            if (availableConn != connection) {
                tempQueue.push(availableConn);
            } else {
                metrics_.idleConnections.fetch_sub(1);
            }
        }
        availableConnections_ = tempQueue;

        destroyConnection(connection);
    }
}

void ConnectionPool::cleanupIdleConnections() {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    // Don't cleanup if we're at minimum connections
    if (allConnections_.size() <= config_.minConnections) {
        return;
    }

    std::vector<std::shared_ptr<PooledConnection>> idleConnections;

    // Check available connections for idle timeout
    std::queue<std::shared_ptr<PooledConnection>> tempQueue;
    while (!availableConnections_.empty()) {
        auto connection = availableConnections_.front();
        availableConnections_.pop();

        if (connection->isIdle(config_.idleTimeout) &&
            allConnections_.size() > config_.minConnections) {
            idleConnections.push_back(connection);
        } else {
            tempQueue.push(connection);
        }
    }
    availableConnections_ = tempQueue;

    for (auto& connection : idleConnections) {
        spdlog::debug("ConnectionPool: Removing idle connection: {}",
                      connection->connectionId);
        metrics_.idleConnections.fetch_sub(1);
        destroyConnection(connection);
    }
}

void ConnectionPool::adjustPoolSize() {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    size_t totalConnections = allConnections_.size();
    size_t activeConnections = activeConnections_.size();

    if (totalConnections == 0) {
        return;
    }

    double utilization = static_cast<double>(activeConnections) / totalConnections;

    // Expand pool if utilization is high
    if (utilization > 0.8 && totalConnections < config_.maxConnections) {
        size_t connectionsToAdd = std::min(
            static_cast<size_t>(totalConnections * (config_.growthFactor - 1.0)),
            config_.maxConnections - totalConnections
        );

        spdlog::info("ConnectionPool: Expanding pool by {} connections (utilization: {:.2f})",
                     connectionsToAdd, utilization);

        for (size_t i = 0; i < connectionsToAdd; ++i) {
            auto pooledConnection = createConnection();
            if (pooledConnection) {
                availableConnections_.push(pooledConnection);
                allConnections_[pooledConnection->connectionId] = pooledConnection;
                metrics_.totalConnections.fetch_add(1);
                metrics_.idleConnections.fetch_add(1);
                metrics_.connectionsCreated.fetch_add(1);
            }
        }
    }

    // Shrink pool if utilization is low (handled by idle cleanup)
    if (utilization < config_.shrinkThreshold && totalConnections > config_.minConnections) {
        spdlog::debug("ConnectionPool: Low utilization detected: {:.2f}, idle cleanup will handle shrinking",
                      utilization);
    }
}

void ConnectionPool::updateMetrics() {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    size_t totalConnections = allConnections_.size();
    size_t activeConnections = activeConnections_.size();
    size_t idleConnections = availableConnections_.size();

    metrics_.totalConnections.store(totalConnections);
    metrics_.activeConnections.store(activeConnections);
    metrics_.idleConnections.store(idleConnections);

    if (totalConnections > 0) {
        double utilization = static_cast<double>(activeConnections) / totalConnections;
        metrics_.poolUtilization.store(utilization);
    } else {
        metrics_.poolUtilization.store(0.0);
    }
}

void ConnectionPool::logPoolStatus() const {
    auto metrics = getMetrics();

    spdlog::info("ConnectionPool Status: Total={}, Active={}, Idle={}, Utilization={:.2f}%, "
                 "Created={}, Destroyed={}, Acquired={}, Released={}, Timeouts={}, HealthFailures={}",
                 metrics.totalConnections.load(),
                 metrics.activeConnections.load(),
                 metrics.idleConnections.load(),
                 metrics.poolUtilization.load() * 100.0,
                 metrics.connectionsCreated.load(),
                 metrics.connectionsDestroyed.load(),
                 metrics.connectionsAcquired.load(),
                 metrics.connectionsReleased.load(),
                 metrics.acquisitionTimeouts.load(),
                 metrics.healthCheckFailures.load());
}

const ConnectionPoolMetrics& ConnectionPool::getMetrics() const {
    return metrics_;
}

json ConnectionPool::getDetailedMetrics() const {
    const auto& metrics = getMetrics();
    json j = metrics.toJson();

    std::lock_guard<std::mutex> lock(connectionsMutex_);
    j["connectionDetails"] = json::array();

    for (const auto& [connectionId, connection] : allConnections_) {
        json connJson;
        connJson["id"] = connectionId;
        connJson["createdAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            connection->createdAt.time_since_epoch()).count();
        connJson["lastUsed"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            connection->lastUsed.time_since_epoch()).count();
        connJson["usageCount"] = connection->usageCount.load();
        connJson["isHealthy"] = connection->isHealthy.load();
        connJson["isActive"] = activeConnections_.find(connection) != activeConnections_.end();

        j["connectionDetails"].push_back(connJson);
    }

    return j;
}

void ConnectionPool::updateConfiguration(const ConnectionPoolConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
    spdlog::info("ConnectionPool: Configuration updated");
}

ConnectionPoolConfig ConnectionPool::getConfiguration() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

size_t ConnectionPool::getActiveConnectionCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return activeConnections_.size();
}

size_t ConnectionPool::getTotalConnectionCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return allConnections_.size();
}

} // namespace performance
} // namespace core
} // namespace hydrogen

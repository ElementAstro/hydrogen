#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace hydrogen {
namespace core {
namespace performance {

using json = nlohmann::json;

// Forward declarations
class IConnection;
class ConnectionFactory;

/**
 * @brief Connection pool configuration
 */
struct ConnectionPoolConfig {
    size_t minConnections = 5;           // Minimum connections to maintain
    size_t maxConnections = 50;          // Maximum connections allowed
    size_t initialConnections = 10;      // Initial connections to create
    std::chrono::milliseconds acquireTimeout{30000};  // Timeout for acquiring connection
    std::chrono::milliseconds idleTimeout{300000};    // Idle timeout (5 minutes)
    std::chrono::milliseconds maxLifetime{3600000};   // Max connection lifetime (1 hour)
    std::chrono::milliseconds healthCheckInterval{60000}; // Health check interval (1 minute)
    std::chrono::milliseconds cleanupInterval{30000}; // Cleanup interval (30 seconds)
    bool enableHealthChecks = true;      // Enable periodic health checks
    bool enableMetrics = true;           // Enable connection pool metrics
    double growthFactor = 1.5;           // Pool growth factor when expanding
    double shrinkThreshold = 0.3;        // Shrink when utilization below this threshold
    
    json toJson() const {
        json j;
        j["minConnections"] = minConnections;
        j["maxConnections"] = maxConnections;
        j["initialConnections"] = initialConnections;
        j["acquireTimeout"] = acquireTimeout.count();
        j["idleTimeout"] = idleTimeout.count();
        j["maxLifetime"] = maxLifetime.count();
        j["healthCheckInterval"] = healthCheckInterval.count();
        j["cleanupInterval"] = cleanupInterval.count();
        j["enableHealthChecks"] = enableHealthChecks;
        j["enableMetrics"] = enableMetrics;
        j["growthFactor"] = growthFactor;
        j["shrinkThreshold"] = shrinkThreshold;
        return j;
    }
    
    static ConnectionPoolConfig fromJson(const json& j) {
        ConnectionPoolConfig config;
        if (j.contains("minConnections")) config.minConnections = j["minConnections"];
        if (j.contains("maxConnections")) config.maxConnections = j["maxConnections"];
        if (j.contains("initialConnections")) config.initialConnections = j["initialConnections"];
        if (j.contains("acquireTimeout")) config.acquireTimeout = std::chrono::milliseconds(j["acquireTimeout"]);
        if (j.contains("idleTimeout")) config.idleTimeout = std::chrono::milliseconds(j["idleTimeout"]);
        if (j.contains("maxLifetime")) config.maxLifetime = std::chrono::milliseconds(j["maxLifetime"]);
        if (j.contains("healthCheckInterval")) config.healthCheckInterval = std::chrono::milliseconds(j["healthCheckInterval"]);
        if (j.contains("cleanupInterval")) config.cleanupInterval = std::chrono::milliseconds(j["cleanupInterval"]);
        if (j.contains("enableHealthChecks")) config.enableHealthChecks = j["enableHealthChecks"];
        if (j.contains("enableMetrics")) config.enableMetrics = j["enableMetrics"];
        if (j.contains("growthFactor")) config.growthFactor = j["growthFactor"];
        if (j.contains("shrinkThreshold")) config.shrinkThreshold = j["shrinkThreshold"];
        return config;
    }
};

/**
 * @brief Connection pool metrics
 */
struct ConnectionPoolMetrics {
    std::atomic<size_t> totalConnections{0};
    std::atomic<size_t> activeConnections{0};
    std::atomic<size_t> idleConnections{0};
    std::atomic<size_t> connectionsCreated{0};
    std::atomic<size_t> connectionsDestroyed{0};
    std::atomic<size_t> connectionsAcquired{0};
    std::atomic<size_t> connectionsReleased{0};
    std::atomic<size_t> acquisitionTimeouts{0};
    std::atomic<size_t> healthCheckFailures{0};
    std::atomic<double> averageAcquisitionTime{0.0};
    std::atomic<double> averageConnectionLifetime{0.0};
    std::atomic<double> poolUtilization{0.0};

    // Copy constructor
    ConnectionPoolMetrics(const ConnectionPoolMetrics& other) {
        totalConnections.store(other.totalConnections.load());
        activeConnections.store(other.activeConnections.load());
        idleConnections.store(other.idleConnections.load());
        connectionsCreated.store(other.connectionsCreated.load());
        connectionsDestroyed.store(other.connectionsDestroyed.load());
        connectionsAcquired.store(other.connectionsAcquired.load());
        connectionsReleased.store(other.connectionsReleased.load());
        acquisitionTimeouts.store(other.acquisitionTimeouts.load());
        healthCheckFailures.store(other.healthCheckFailures.load());
        averageAcquisitionTime.store(other.averageAcquisitionTime.load());
        averageConnectionLifetime.store(other.averageConnectionLifetime.load());
        poolUtilization.store(other.poolUtilization.load());
    }

    // Assignment operator
    ConnectionPoolMetrics& operator=(const ConnectionPoolMetrics& other) {
        if (this != &other) {
            totalConnections.store(other.totalConnections.load());
            activeConnections.store(other.activeConnections.load());
            idleConnections.store(other.idleConnections.load());
            connectionsCreated.store(other.connectionsCreated.load());
            connectionsDestroyed.store(other.connectionsDestroyed.load());
            connectionsAcquired.store(other.connectionsAcquired.load());
            connectionsReleased.store(other.connectionsReleased.load());
            acquisitionTimeouts.store(other.acquisitionTimeouts.load());
            healthCheckFailures.store(other.healthCheckFailures.load());
            averageAcquisitionTime.store(other.averageAcquisitionTime.load());
            averageConnectionLifetime.store(other.averageConnectionLifetime.load());
            poolUtilization.store(other.poolUtilization.load());
        }
        return *this;
    }

    // Default constructor
    ConnectionPoolMetrics() = default;

    json toJson() const {
        json j;
        j["totalConnections"] = totalConnections.load();
        j["activeConnections"] = activeConnections.load();
        j["idleConnections"] = idleConnections.load();
        j["connectionsCreated"] = connectionsCreated.load();
        j["connectionsDestroyed"] = connectionsDestroyed.load();
        j["connectionsAcquired"] = connectionsAcquired.load();
        j["connectionsReleased"] = connectionsReleased.load();
        j["acquisitionTimeouts"] = acquisitionTimeouts.load();
        j["healthCheckFailures"] = healthCheckFailures.load();
        j["averageAcquisitionTime"] = averageAcquisitionTime.load();
        j["averageConnectionLifetime"] = averageConnectionLifetime.load();
        j["poolUtilization"] = poolUtilization.load();
        return j;
    }
};

/**
 * @brief Connection wrapper with metadata
 */
struct PooledConnection {
    std::shared_ptr<IConnection> connection;
    std::string connectionId;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastUsed;
    std::chrono::system_clock::time_point lastHealthCheck;
    std::atomic<bool> isHealthy{true};
    std::atomic<size_t> usageCount{0};
    
    PooledConnection(std::shared_ptr<IConnection> conn, const std::string& id)
        : connection(std::move(conn))
        , connectionId(id)
        , createdAt(std::chrono::system_clock::now())
        , lastUsed(std::chrono::system_clock::now())
        , lastHealthCheck(std::chrono::system_clock::now()) {}
    
    bool isExpired(std::chrono::milliseconds maxLifetime) const {
        auto now = std::chrono::system_clock::now();
        return (now - createdAt) > maxLifetime;
    }
    
    bool isIdle(std::chrono::milliseconds idleTimeout) const {
        auto now = std::chrono::system_clock::now();
        return (now - lastUsed) > idleTimeout;
    }
    
    void updateLastUsed() {
        lastUsed = std::chrono::system_clock::now();
        usageCount.fetch_add(1);
    }
    
    void updateHealthCheck(bool healthy) {
        lastHealthCheck = std::chrono::system_clock::now();
        isHealthy.store(healthy);
    }
};

/**
 * @brief Generic connection interface
 */
class IConnection {
public:
    virtual ~IConnection() = default;
    
    virtual bool isConnected() const = 0;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isHealthy() const = 0;
    virtual std::string getId() const = 0;
    virtual json getMetadata() const = 0;
};

/**
 * @brief Connection factory interface
 */
class IConnectionFactory {
public:
    virtual ~IConnectionFactory() = default;
    
    virtual std::shared_ptr<IConnection> createConnection() = 0;
    virtual bool validateConnection(const std::shared_ptr<IConnection>& connection) = 0;
    virtual std::string getConnectionType() const = 0;
};

/**
 * @brief High-performance connection pool with advanced features
 */
class ConnectionPool {
public:
    explicit ConnectionPool(
        std::shared_ptr<IConnectionFactory> factory,
        const ConnectionPoolConfig& config = ConnectionPoolConfig{}
    );
    
    ~ConnectionPool();
    
    // Pool lifecycle
    bool initialize();
    void shutdown();
    bool isRunning() const { return running_.load(); }
    
    // Connection management
    std::shared_ptr<IConnection> acquireConnection(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{30000}
    );
    
    void releaseConnection(std::shared_ptr<IConnection> connection);
    
    // Pool management
    void expandPool(size_t additionalConnections);
    void shrinkPool(size_t connectionsToRemove);
    void flushPool();
    
    // Configuration and metrics
    void updateConfiguration(const ConnectionPoolConfig& config);
    ConnectionPoolConfig getConfiguration() const;
    const ConnectionPoolMetrics& getMetrics() const;
    json getDetailedMetrics() const;
    
    // Health and maintenance
    void performHealthCheck();
    void performMaintenance();
    size_t getActiveConnectionCount() const;
    size_t getTotalConnectionCount() const;
    double getUtilizationRate() const;

private:
    // Configuration and factory
    std::shared_ptr<IConnectionFactory> factory_;
    ConnectionPoolConfig config_;
    mutable std::mutex configMutex_;
    
    // Connection storage
    std::queue<std::shared_ptr<PooledConnection>> availableConnections_;
    std::unordered_set<std::shared_ptr<PooledConnection>> activeConnections_;
    std::unordered_map<std::string, std::shared_ptr<PooledConnection>> allConnections_;
    mutable std::mutex connectionsMutex_;
    std::condition_variable connectionAvailable_;
    
    // Pool state
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    
    // Metrics
    mutable ConnectionPoolMetrics metrics_;
    
    // Background threads
    std::unique_ptr<std::thread> healthCheckThread_;
    std::unique_ptr<std::thread> maintenanceThread_;
    std::condition_variable shutdownCondition_;
    std::mutex shutdownMutex_;
    
    // Internal methods
    std::shared_ptr<PooledConnection> createConnection();
    void destroyConnection(std::shared_ptr<PooledConnection> connection);
    bool validateConnection(std::shared_ptr<PooledConnection> connection);
    void cleanupExpiredConnections();
    void cleanupIdleConnections();
    void adjustPoolSize();
    
    // Thread functions
    void healthCheckThreadFunction();
    void maintenanceThreadFunction();
    
    // Utility methods
    std::string generateConnectionId();
    void updateMetrics();
    void logPoolStatus() const;
};

/**
 * @brief Connection pool manager for multiple pools
 */
class ConnectionPoolManager {
public:
    static ConnectionPoolManager& getInstance();
    
    // Pool management
    void registerPool(const std::string& poolName, std::shared_ptr<ConnectionPool> pool);
    void unregisterPool(const std::string& poolName);
    std::shared_ptr<ConnectionPool> getPool(const std::string& poolName);
    
    // Global operations
    void shutdownAllPools();
    json getAllPoolMetrics() const;
    void performGlobalMaintenance();
    
    // Configuration
    void setGlobalConfig(const json& config);
    json getGlobalConfig() const;

private:
    ConnectionPoolManager() = default;
    ~ConnectionPoolManager() = default;
    
    std::unordered_map<std::string, std::shared_ptr<ConnectionPool>> pools_;
    mutable std::mutex poolsMutex_;
    json globalConfig_;
    mutable std::mutex configMutex_;
};

} // namespace performance
} // namespace core
} // namespace hydrogen

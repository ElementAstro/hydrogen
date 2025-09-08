#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <queue>
#include <stack>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace hydrogen {
namespace core {
namespace performance {

using json = nlohmann::json;

/**
 * @brief Memory pool configuration
 */
struct MemoryPoolConfig {
    size_t initialPoolSize = 100;           // Initial number of objects to pre-allocate
    size_t maxPoolSize = 1000;              // Maximum number of objects in pool
    size_t growthIncrement = 50;            // Number of objects to add when expanding
    double growthThreshold = 0.8;           // Expand when utilization > threshold
    double shrinkThreshold = 0.3;           // Shrink when utilization < threshold
    std::chrono::milliseconds cleanupInterval{60000}; // Cleanup interval (1 minute)
    std::chrono::milliseconds objectTimeout{300000};  // Object timeout (5 minutes)
    bool enableMetrics = true;              // Enable pool metrics
    bool enableAutoCleanup = true;          // Enable automatic cleanup
    bool enableThreadSafety = true;         // Enable thread-safe operations
    
    json toJson() const {
        json j;
        j["initialPoolSize"] = initialPoolSize;
        j["maxPoolSize"] = maxPoolSize;
        j["growthIncrement"] = growthIncrement;
        j["growthThreshold"] = growthThreshold;
        j["shrinkThreshold"] = shrinkThreshold;
        j["cleanupInterval"] = cleanupInterval.count();
        j["objectTimeout"] = objectTimeout.count();
        j["enableMetrics"] = enableMetrics;
        j["enableAutoCleanup"] = enableAutoCleanup;
        j["enableThreadSafety"] = enableThreadSafety;
        return j;
    }
    
    static MemoryPoolConfig fromJson(const json& j) {
        MemoryPoolConfig config;
        if (j.contains("initialPoolSize")) config.initialPoolSize = j["initialPoolSize"];
        if (j.contains("maxPoolSize")) config.maxPoolSize = j["maxPoolSize"];
        if (j.contains("growthIncrement")) config.growthIncrement = j["growthIncrement"];
        if (j.contains("growthThreshold")) config.growthThreshold = j["growthThreshold"];
        if (j.contains("shrinkThreshold")) config.shrinkThreshold = j["shrinkThreshold"];
        if (j.contains("cleanupInterval")) config.cleanupInterval = std::chrono::milliseconds(j["cleanupInterval"]);
        if (j.contains("objectTimeout")) config.objectTimeout = std::chrono::milliseconds(j["objectTimeout"]);
        if (j.contains("enableMetrics")) config.enableMetrics = j["enableMetrics"];
        if (j.contains("enableAutoCleanup")) config.enableAutoCleanup = j["enableAutoCleanup"];
        if (j.contains("enableThreadSafety")) config.enableThreadSafety = j["enableThreadSafety"];
        return config;
    }
};

/**
 * @brief Memory pool metrics
 */
struct MemoryPoolMetrics {
    std::atomic<size_t> totalAllocations{0};
    std::atomic<size_t> totalDeallocations{0};
    std::atomic<size_t> poolHits{0};
    std::atomic<size_t> poolMisses{0};
    std::atomic<size_t> currentPoolSize{0};
    std::atomic<size_t> currentActiveObjects{0};
    std::atomic<size_t> peakPoolSize{0};
    std::atomic<size_t> peakActiveObjects{0};
    std::atomic<double> hitRatio{0.0};
    std::atomic<double> memoryUtilization{0.0};
    std::atomic<size_t> totalMemoryAllocated{0};
    std::atomic<size_t> totalMemoryInUse{0};

    // Copy constructor
    MemoryPoolMetrics(const MemoryPoolMetrics& other) {
        totalAllocations.store(other.totalAllocations.load());
        totalDeallocations.store(other.totalDeallocations.load());
        poolHits.store(other.poolHits.load());
        poolMisses.store(other.poolMisses.load());
        currentPoolSize.store(other.currentPoolSize.load());
        currentActiveObjects.store(other.currentActiveObjects.load());
        peakPoolSize.store(other.peakPoolSize.load());
        peakActiveObjects.store(other.peakActiveObjects.load());
        hitRatio.store(other.hitRatio.load());
        memoryUtilization.store(other.memoryUtilization.load());
        totalMemoryAllocated.store(other.totalMemoryAllocated.load());
        totalMemoryInUse.store(other.totalMemoryInUse.load());
    }

    // Assignment operator
    MemoryPoolMetrics& operator=(const MemoryPoolMetrics& other) {
        if (this != &other) {
            totalAllocations.store(other.totalAllocations.load());
            totalDeallocations.store(other.totalDeallocations.load());
            poolHits.store(other.poolHits.load());
            poolMisses.store(other.poolMisses.load());
            currentPoolSize.store(other.currentPoolSize.load());
            currentActiveObjects.store(other.currentActiveObjects.load());
            peakPoolSize.store(other.peakPoolSize.load());
            peakActiveObjects.store(other.peakActiveObjects.load());
            hitRatio.store(other.hitRatio.load());
            memoryUtilization.store(other.memoryUtilization.load());
            totalMemoryAllocated.store(other.totalMemoryAllocated.load());
            totalMemoryInUse.store(other.totalMemoryInUse.load());
        }
        return *this;
    }

    // Default constructor
    MemoryPoolMetrics() = default;

    json toJson() const {
        json j;
        j["totalAllocations"] = totalAllocations.load();
        j["totalDeallocations"] = totalDeallocations.load();
        j["poolHits"] = poolHits.load();
        j["poolMisses"] = poolMisses.load();
        j["currentPoolSize"] = currentPoolSize.load();
        j["currentActiveObjects"] = currentActiveObjects.load();
        j["peakPoolSize"] = peakPoolSize.load();
        j["peakActiveObjects"] = peakActiveObjects.load();
        j["hitRatio"] = hitRatio.load();
        j["memoryUtilization"] = memoryUtilization.load();
        j["totalMemoryAllocated"] = totalMemoryAllocated.load();
        j["totalMemoryInUse"] = totalMemoryInUse.load();
        return j;
    }
};

/**
 * @brief Pooled object wrapper with metadata
 */
template<typename T>
struct PooledObject {
    std::unique_ptr<T> object;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastUsed;
    std::atomic<size_t> usageCount{0};
    bool isActive = false;
    
    PooledObject() : createdAt(std::chrono::system_clock::now()), lastUsed(createdAt) {}
    
    explicit PooledObject(std::unique_ptr<T> obj) 
        : object(std::move(obj))
        , createdAt(std::chrono::system_clock::now())
        , lastUsed(createdAt) {}
    
    void updateLastUsed() {
        lastUsed = std::chrono::system_clock::now();
        usageCount.fetch_add(1);
    }
    
    bool isExpired(std::chrono::milliseconds timeout) const {
        auto now = std::chrono::system_clock::now();
        return (now - lastUsed) > timeout;
    }
    
    std::chrono::milliseconds getAge() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - createdAt);
    }
};

/**
 * @brief High-performance memory pool for object reuse
 */
template<typename T>
class MemoryPool {
public:
    explicit MemoryPool(const MemoryPoolConfig& config = MemoryPoolConfig{});
    ~MemoryPool();

    // Pool lifecycle
    bool initialize();
    void shutdown();
    bool isRunning() const { return running_.load(); }

    // Object management
    std::shared_ptr<T> acquire();
    void release(std::shared_ptr<T> object);
    
    // Pool management
    void expandPool(size_t additionalObjects);
    void shrinkPool(size_t objectsToRemove);
    void clearPool();
    
    // Configuration and metrics
    void updateConfiguration(const MemoryPoolConfig& config);
    MemoryPoolConfig getConfiguration() const;
    MemoryPoolMetrics getMetrics() const;
    json getDetailedMetrics() const;
    
    // Pool status
    size_t getPoolSize() const;
    size_t getActiveObjectCount() const;
    double getHitRatio() const;
    double getUtilizationRate() const;

private:
    // Configuration
    MemoryPoolConfig config_;
    mutable std::mutex configMutex_;
    
    // Pool state
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    
    // Object storage
    std::stack<std::shared_ptr<PooledObject<T>>> availableObjects_;
    std::unordered_map<T*, std::shared_ptr<PooledObject<T>>> activeObjects_;
    mutable std::mutex poolMutex_;
    
    // Metrics
    mutable MemoryPoolMetrics metrics_;
    
    // Background cleanup thread
    std::unique_ptr<std::thread> cleanupThread_;
    std::atomic<bool> stopCleanup_{false};
    
    // Internal methods
    std::shared_ptr<PooledObject<T>> createObject();
    void performCleanup();
    void updateMetrics();
    void adjustPoolSize();
    
    // Thread function
    void cleanupThreadFunction();
    
    // Custom deleter for shared_ptr
    class PoolDeleter {
    public:
        explicit PoolDeleter(MemoryPool<T>* pool) : pool_(pool) {}
        
        void operator()(T* obj) {
            if (pool_) {
                // Find the pooled object and return it to pool
                std::lock_guard<std::mutex> lock(pool_->poolMutex_);
                auto it = pool_->activeObjects_.find(obj);
                if (it != pool_->activeObjects_.end()) {
                    auto pooledObj = it->second;
                    pool_->activeObjects_.erase(it);
                    
                    pooledObj->isActive = false;
                    pooledObj->updateLastUsed();
                    
                    if (pool_->availableObjects_.size() < pool_->config_.maxPoolSize) {
                        pool_->availableObjects_.push(pooledObj);
                    }
                    
                    pool_->metrics_.totalDeallocations.fetch_add(1);
                    pool_->metrics_.currentActiveObjects.fetch_sub(1);
                }
            }
        }
        
    private:
        MemoryPool<T>* pool_;
    };
};

/**
 * @brief Specialized memory pools for common types
 */
using StringPool = MemoryPool<std::string>;
using JsonPool = MemoryPool<json>;
using VectorPool = MemoryPool<std::vector<uint8_t>>;

/**
 * @brief Memory pool manager for multiple pools
 */
class MemoryPoolManager {
public:
    static MemoryPoolManager& getInstance();
    
    // Pool registration
    template<typename T>
    void registerPool(const std::string& name, std::shared_ptr<MemoryPool<T>> pool);
    
    void unregisterPool(const std::string& name);
    
    template<typename T>
    std::shared_ptr<MemoryPool<T>> getPool(const std::string& name);
    
    // Global operations
    void initializeAllPools();
    void shutdownAllPools();
    json getAllPoolMetrics() const;
    void performGlobalCleanup();
    
    // Configuration
    void setGlobalConfig(const json& config);
    json getGlobalConfig() const;
    
    // Convenience methods for common pools
    std::shared_ptr<StringPool> getStringPool();
    std::shared_ptr<JsonPool> getJsonPool();
    std::shared_ptr<VectorPool> getVectorPool();

private:
    MemoryPoolManager();
    ~MemoryPoolManager() = default;
    
    std::unordered_map<std::string, std::shared_ptr<void>> pools_;
    mutable std::mutex poolsMutex_;
    json globalConfig_;
    mutable std::mutex configMutex_;
    
    // Default pools
    std::shared_ptr<StringPool> stringPool_;
    std::shared_ptr<JsonPool> jsonPool_;
    std::shared_ptr<VectorPool> vectorPool_;
    
    void initializeDefaultPools();
};

/**
 * @brief RAII wrapper for pooled objects
 */
template<typename T>
class PooledResource {
public:
    explicit PooledResource(std::shared_ptr<MemoryPool<T>> pool)
        : pool_(pool), resource_(pool->acquire()) {}
    
    ~PooledResource() = default;
    
    // Non-copyable but movable
    PooledResource(const PooledResource&) = delete;
    PooledResource& operator=(const PooledResource&) = delete;
    
    PooledResource(PooledResource&&) = default;
    PooledResource& operator=(PooledResource&&) = default;
    
    T* get() const { return resource_.get(); }
    T& operator*() const { return *resource_; }
    T* operator->() const { return resource_.get(); }
    
    bool isValid() const { return resource_ != nullptr; }
    
    void reset() { resource_.reset(); }

private:
    std::shared_ptr<MemoryPool<T>> pool_;
    std::shared_ptr<T> resource_;
};

/**
 * @brief Memory pool factory for easy creation
 */
class MemoryPoolFactory {
public:
    template<typename T>
    static std::shared_ptr<MemoryPool<T>> createPool(const MemoryPoolConfig& config = MemoryPoolConfig{});
    
    template<typename T>
    static std::shared_ptr<MemoryPool<T>> createPool(
        size_t initialSize,
        size_t maxSize,
        bool enableMetrics = true
    );
    
    static MemoryPoolConfig createDefaultConfig();
    static MemoryPoolConfig createHighPerformanceConfig();
    static MemoryPoolConfig createLowMemoryConfig();
};

} // namespace performance
} // namespace core
} // namespace hydrogen

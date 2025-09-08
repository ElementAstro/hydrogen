#include "hydrogen/core/performance/memory_pool.h"

#include <algorithm>

namespace hydrogen {
namespace core {
namespace performance {

// MemoryPool implementation

template<typename T>
MemoryPool<T>::MemoryPool(const MemoryPoolConfig& config)
    : config_(config) {
    spdlog::debug("MemoryPool: Created memory pool for type: {}", typeid(T).name());
}

template<typename T>
MemoryPool<T>::~MemoryPool() {
    spdlog::debug("MemoryPool: Destructor called for type: {}", typeid(T).name());
    if (running_.load()) {
        shutdown();
    }
}

template<typename T>
bool MemoryPool<T>::initialize() {
    spdlog::info("MemoryPool: Initializing memory pool for type: {}", typeid(T).name());
    
    if (initialized_.load()) {
        spdlog::warn("MemoryPool: Already initialized");
        return true;
    }
    
    try {
        // Validate configuration
        if (config_.initialPoolSize > config_.maxPoolSize) {
            spdlog::error("MemoryPool: Invalid configuration - initialPoolSize > maxPoolSize");
            return false;
        }
        
        // Pre-allocate initial objects
        {
            std::lock_guard<std::mutex> lock(poolMutex_);
            
            for (size_t i = 0; i < config_.initialPoolSize; ++i) {
                auto pooledObj = createObject();
                if (pooledObj) {
                    availableObjects_.push(pooledObj);
                    metrics_.currentPoolSize.fetch_add(1);
                    metrics_.totalMemoryAllocated.fetch_add(sizeof(T));
                }
            }
        }
        
        running_.store(true);
        
        // Start cleanup thread if enabled
        if (config_.enableAutoCleanup) {
            cleanupThread_ = std::make_unique<std::thread>(&MemoryPool<T>::cleanupThreadFunction, this);
        }
        
        initialized_.store(true);
        
        spdlog::info("MemoryPool: Initialized with {} objects for type: {}", 
                     availableObjects_.size(), typeid(T).name());
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("MemoryPool: Exception during initialization: {}", e.what());
        return false;
    }
}

template<typename T>
void MemoryPool<T>::shutdown() {
    spdlog::info("MemoryPool: Shutting down memory pool for type: {}", typeid(T).name());
    
    if (!running_.load()) {
        spdlog::warn("MemoryPool: Not running");
        return;
    }
    
    running_.store(false);
    stopCleanup_.store(true);
    
    // Wait for cleanup thread
    if (cleanupThread_ && cleanupThread_->joinable()) {
        cleanupThread_->join();
    }
    
    // Clear all objects
    clearPool();
    
    initialized_.store(false);
    
    spdlog::info("MemoryPool: Shutdown complete for type: {}", typeid(T).name());
}

template<typename T>
std::shared_ptr<T> MemoryPool<T>::acquire() {
    if (!running_.load()) {
        spdlog::error("MemoryPool: Cannot acquire object - pool not running");
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    std::shared_ptr<PooledObject<T>> pooledObj = nullptr;
    
    // Try to get from available objects
    if (!availableObjects_.empty()) {
        pooledObj = availableObjects_.top();
        availableObjects_.pop();
        metrics_.poolHits.fetch_add(1);
        metrics_.currentPoolSize.fetch_sub(1);
    } else {
        // Create new object if under limit
        if (activeObjects_.size() < config_.maxPoolSize) {
            pooledObj = createObject();
            if (pooledObj) {
                metrics_.poolMisses.fetch_add(1);
                metrics_.totalMemoryAllocated.fetch_add(sizeof(T));
            }
        }
    }
    
    if (!pooledObj) {
        spdlog::warn("MemoryPool: Failed to acquire object - pool exhausted");
        return nullptr;
    }
    
    // Mark as active
    pooledObj->isActive = true;
    pooledObj->updateLastUsed();
    
    // Track active object
    T* rawPtr = pooledObj->object.get();
    activeObjects_[rawPtr] = pooledObj;
    
    metrics_.totalAllocations.fetch_add(1);
    metrics_.currentActiveObjects.fetch_add(1);
    metrics_.totalMemoryInUse.fetch_add(sizeof(T));
    
    // Update peak metrics
    size_t currentActive = metrics_.currentActiveObjects.load();
    size_t currentPeak = metrics_.peakActiveObjects.load();
    if (currentActive > currentPeak) {
        metrics_.peakActiveObjects.store(currentActive);
    }
    
    // Create shared_ptr with custom deleter
    std::shared_ptr<T> result(rawPtr, PoolDeleter(this));
    
    spdlog::debug("MemoryPool: Acquired object from pool");
    
    return result;
}

template<typename T>
void MemoryPool<T>::release(std::shared_ptr<T> object) {
    if (!object) {
        spdlog::warn("MemoryPool: Attempted to release null object");
        return;
    }
    
    // The custom deleter will handle the actual release
    object.reset();
}

template<typename T>
std::shared_ptr<PooledObject<T>> MemoryPool<T>::createObject() {
    try {
        auto obj = std::make_unique<T>();
        auto pooledObj = std::make_shared<PooledObject<T>>(std::move(obj));
        
        spdlog::debug("MemoryPool: Created new object");
        
        return pooledObj;
        
    } catch (const std::exception& e) {
        spdlog::error("MemoryPool: Exception creating object: {}", e.what());
        return nullptr;
    }
}

template<typename T>
void MemoryPool<T>::expandPool(size_t additionalObjects) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    size_t currentSize = availableObjects_.size() + activeObjects_.size();
    size_t targetSize = std::min(currentSize + additionalObjects, config_.maxPoolSize);
    size_t objectsToCreate = targetSize - currentSize;
    
    spdlog::info("MemoryPool: Expanding pool by {} objects", objectsToCreate);
    
    for (size_t i = 0; i < objectsToCreate; ++i) {
        auto pooledObj = createObject();
        if (pooledObj) {
            availableObjects_.push(pooledObj);
            metrics_.currentPoolSize.fetch_add(1);
            metrics_.totalMemoryAllocated.fetch_add(sizeof(T));
        }
    }
    
    // Update peak pool size
    size_t newPoolSize = metrics_.currentPoolSize.load();
    size_t currentPeak = metrics_.peakPoolSize.load();
    if (newPoolSize > currentPeak) {
        metrics_.peakPoolSize.store(newPoolSize);
    }
}

template<typename T>
void MemoryPool<T>::shrinkPool(size_t objectsToRemove) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    size_t objectsRemoved = 0;
    
    while (!availableObjects_.empty() && objectsRemoved < objectsToRemove) {
        availableObjects_.pop();
        objectsRemoved++;
        metrics_.currentPoolSize.fetch_sub(1);
        metrics_.totalMemoryAllocated.fetch_sub(sizeof(T));
    }
    
    spdlog::info("MemoryPool: Shrunk pool by {} objects", objectsRemoved);
}

template<typename T>
void MemoryPool<T>::clearPool() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    // Clear available objects
    size_t availableCount = availableObjects_.size();
    while (!availableObjects_.empty()) {
        availableObjects_.pop();
    }
    
    // Clear active objects (they should be released by now)
    size_t activeCount = activeObjects_.size();
    activeObjects_.clear();
    
    metrics_.currentPoolSize.store(0);
    metrics_.currentActiveObjects.store(0);
    metrics_.totalMemoryInUse.store(0);
    
    spdlog::info("MemoryPool: Cleared pool - removed {} available and {} active objects", 
                 availableCount, activeCount);
}

template<typename T>
void MemoryPool<T>::performCleanup() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    // Remove expired objects from available pool
    std::stack<std::shared_ptr<PooledObject<T>>> tempStack;
    size_t expiredCount = 0;
    
    while (!availableObjects_.empty()) {
        auto obj = availableObjects_.top();
        availableObjects_.pop();
        
        if (obj->isExpired(config_.objectTimeout)) {
            expiredCount++;
            metrics_.currentPoolSize.fetch_sub(1);
            metrics_.totalMemoryAllocated.fetch_sub(sizeof(T));
        } else {
            tempStack.push(obj);
        }
    }
    
    // Restore non-expired objects
    availableObjects_ = tempStack;
    
    if (expiredCount > 0) {
        spdlog::debug("MemoryPool: Cleaned up {} expired objects", expiredCount);
    }
    
    // Adjust pool size based on utilization
    adjustPoolSize();
}

template<typename T>
void MemoryPool<T>::adjustPoolSize() {
    size_t totalObjects = availableObjects_.size() + activeObjects_.size();
    size_t activeObjects = activeObjects_.size();
    
    if (totalObjects == 0) {
        return;
    }
    
    double utilization = static_cast<double>(activeObjects) / totalObjects;
    
    // Expand if utilization is high
    if (utilization > config_.growthThreshold && totalObjects < config_.maxPoolSize) {
        size_t objectsToAdd = std::min(config_.growthIncrement, 
                                      config_.maxPoolSize - totalObjects);
        
        spdlog::debug("MemoryPool: High utilization ({:.2f}), expanding by {} objects", 
                      utilization, objectsToAdd);
        
        for (size_t i = 0; i < objectsToAdd; ++i) {
            auto pooledObj = createObject();
            if (pooledObj) {
                availableObjects_.push(pooledObj);
                metrics_.currentPoolSize.fetch_add(1);
                metrics_.totalMemoryAllocated.fetch_add(sizeof(T));
            }
        }
    }
    
    // Shrink if utilization is low
    else if (utilization < config_.shrinkThreshold && totalObjects > config_.initialPoolSize) {
        size_t objectsToRemove = std::min(availableObjects_.size(), 
                                         totalObjects - config_.initialPoolSize);
        
        if (objectsToRemove > 0) {
            spdlog::debug("MemoryPool: Low utilization ({:.2f}), shrinking by {} objects", 
                          utilization, objectsToRemove);
            
            for (size_t i = 0; i < objectsToRemove && !availableObjects_.empty(); ++i) {
                availableObjects_.pop();
                metrics_.currentPoolSize.fetch_sub(1);
                metrics_.totalMemoryAllocated.fetch_sub(sizeof(T));
            }
        }
    }
}

template<typename T>
void MemoryPool<T>::cleanupThreadFunction() {
    spdlog::debug("MemoryPool: Cleanup thread started");
    
    while (!stopCleanup_.load()) {
        try {
            performCleanup();
            updateMetrics();
            
        } catch (const std::exception& e) {
            spdlog::error("MemoryPool: Exception in cleanup thread: {}", e.what());
        }
        
        // Wait for next cleanup interval
        std::this_thread::sleep_for(config_.cleanupInterval);
    }
    
    spdlog::debug("MemoryPool: Cleanup thread stopped");
}

template<typename T>
void MemoryPool<T>::updateMetrics() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    // Update hit ratio
    size_t totalRequests = metrics_.poolHits.load() + metrics_.poolMisses.load();
    if (totalRequests > 0) {
        double hitRatio = static_cast<double>(metrics_.poolHits.load()) / totalRequests;
        metrics_.hitRatio.store(hitRatio);
    }
    
    // Update memory utilization
    size_t totalMemory = metrics_.totalMemoryAllocated.load();
    size_t usedMemory = metrics_.totalMemoryInUse.load();
    if (totalMemory > 0) {
        double utilization = static_cast<double>(usedMemory) / totalMemory;
        metrics_.memoryUtilization.store(utilization);
    }
}

template<typename T>
MemoryPoolMetrics MemoryPool<T>::getMetrics() const {
    return metrics_;
}

template<typename T>
json MemoryPool<T>::getDetailedMetrics() const {
    auto metrics = getMetrics();
    json j = metrics.toJson();
    
    std::lock_guard<std::mutex> lock(poolMutex_);
    j["poolDetails"] = json::object();
    j["poolDetails"]["availableObjects"] = availableObjects_.size();
    j["poolDetails"]["activeObjects"] = activeObjects_.size();
    j["poolDetails"]["totalObjects"] = availableObjects_.size() + activeObjects_.size();
    j["poolDetails"]["objectType"] = typeid(T).name();
    j["poolDetails"]["objectSize"] = sizeof(T);
    
    return j;
}

// MemoryPoolManager implementation

MemoryPoolManager::MemoryPoolManager() {
    initializeDefaultPools();
}

MemoryPoolManager& MemoryPoolManager::getInstance() {
    static MemoryPoolManager instance;
    return instance;
}

void MemoryPoolManager::initializeDefaultPools() {
    // Create default pools for common types
    MemoryPoolConfig config;
    config.initialPoolSize = 50;
    config.maxPoolSize = 500;

    stringPool_ = std::make_shared<StringPool>(config);
    jsonPool_ = std::make_shared<JsonPool>(config);
    vectorPool_ = std::make_shared<VectorPool>(config);

    registerPool("string", stringPool_);
    registerPool("json", jsonPool_);
    registerPool("vector", vectorPool_);
}

template<typename T>
void MemoryPoolManager::registerPool(const std::string& name, std::shared_ptr<MemoryPool<T>> pool) {
    std::lock_guard<std::mutex> lock(poolsMutex_);
    pools_[name] = std::static_pointer_cast<void>(pool);
    spdlog::info("MemoryPoolManager: Registered pool: {}", name);
}

void MemoryPoolManager::unregisterPool(const std::string& name) {
    std::lock_guard<std::mutex> lock(poolsMutex_);
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        pools_.erase(it);
        spdlog::info("MemoryPoolManager: Unregistered pool: {}", name);
    }
}

template<typename T>
std::shared_ptr<MemoryPool<T>> MemoryPoolManager::getPool(const std::string& name) {
    std::lock_guard<std::mutex> lock(poolsMutex_);
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        return std::static_pointer_cast<MemoryPool<T>>(it->second);
    }
    return nullptr;
}

std::shared_ptr<StringPool> MemoryPoolManager::getStringPool() {
    return stringPool_;
}

std::shared_ptr<JsonPool> MemoryPoolManager::getJsonPool() {
    return jsonPool_;
}

std::shared_ptr<VectorPool> MemoryPoolManager::getVectorPool() {
    return vectorPool_;
}

void MemoryPoolManager::initializeAllPools() {
    spdlog::info("MemoryPoolManager: Initializing all pools");

    if (stringPool_) stringPool_->initialize();
    if (jsonPool_) jsonPool_->initialize();
    if (vectorPool_) vectorPool_->initialize();

    // Initialize other registered pools
    std::lock_guard<std::mutex> lock(poolsMutex_);
    for (const auto& [name, pool] : pools_) {
        // Note: We can't call initialize on generic void* pools
        // Each pool type needs to be initialized separately
        spdlog::debug("MemoryPoolManager: Pool {} registered", name);
    }
}

void MemoryPoolManager::shutdownAllPools() {
    spdlog::info("MemoryPoolManager: Shutting down all pools");

    if (stringPool_) stringPool_->shutdown();
    if (jsonPool_) jsonPool_->shutdown();
    if (vectorPool_) vectorPool_->shutdown();
}

json MemoryPoolManager::getAllPoolMetrics() const {
    json metrics = json::object();

    if (stringPool_) {
        metrics["string"] = stringPool_->getDetailedMetrics();
    }
    if (jsonPool_) {
        metrics["json"] = jsonPool_->getDetailedMetrics();
    }
    if (vectorPool_) {
        metrics["vector"] = vectorPool_->getDetailedMetrics();
    }

    return metrics;
}

// MemoryPoolFactory implementation

template<typename T>
std::shared_ptr<MemoryPool<T>> MemoryPoolFactory::createPool(const MemoryPoolConfig& config) {
    auto pool = std::make_shared<MemoryPool<T>>(config);
    spdlog::info("MemoryPoolFactory: Created pool for type: {}", typeid(T).name());
    return pool;
}

template<typename T>
std::shared_ptr<MemoryPool<T>> MemoryPoolFactory::createPool(
    size_t initialSize, size_t maxSize, bool enableMetrics) {

    MemoryPoolConfig config;
    config.initialPoolSize = initialSize;
    config.maxPoolSize = maxSize;
    config.enableMetrics = enableMetrics;

    return createPool<T>(config);
}

MemoryPoolConfig MemoryPoolFactory::createDefaultConfig() {
    return MemoryPoolConfig{};
}

MemoryPoolConfig MemoryPoolFactory::createHighPerformanceConfig() {
    MemoryPoolConfig config;
    config.initialPoolSize = 200;
    config.maxPoolSize = 2000;
    config.growthIncrement = 100;
    config.growthThreshold = 0.9;
    config.shrinkThreshold = 0.2;
    config.cleanupInterval = std::chrono::milliseconds{30000};
    config.enableMetrics = true;
    config.enableAutoCleanup = true;
    return config;
}

MemoryPoolConfig MemoryPoolFactory::createLowMemoryConfig() {
    MemoryPoolConfig config;
    config.initialPoolSize = 10;
    config.maxPoolSize = 100;
    config.growthIncrement = 10;
    config.growthThreshold = 0.7;
    config.shrinkThreshold = 0.4;
    config.cleanupInterval = std::chrono::milliseconds{120000};
    config.enableMetrics = false;
    config.enableAutoCleanup = true;
    return config;
}

// Template method implementations for explicit instantiations
template<typename T>
void MemoryPool<T>::updateConfiguration(const MemoryPoolConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
    spdlog::info("MemoryPool<{}>: Configuration updated", typeid(T).name());
}

template<typename T>
MemoryPoolConfig MemoryPool<T>::getConfiguration() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

template<typename T>
size_t MemoryPool<T>::getPoolSize() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    return availableObjects_.size();
}

template<typename T>
size_t MemoryPool<T>::getActiveObjectCount() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    return activeObjects_.size();
}

template<typename T>
double MemoryPool<T>::getHitRatio() const {
    size_t hits = metrics_.poolHits.load();
    size_t misses = metrics_.poolMisses.load();
    size_t total = hits + misses;
    return total > 0 ? static_cast<double>(hits) / total : 0.0;
}

template<typename T>
double MemoryPool<T>::getUtilizationRate() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    size_t totalObjects = availableObjects_.size() + activeObjects_.size();
    return totalObjects > 0 ? static_cast<double>(activeObjects_.size()) / totalObjects : 0.0;
}

// Explicit template instantiations for common types
template class MemoryPool<std::string>;
template class MemoryPool<json>;
template class MemoryPool<std::vector<uint8_t>>;

// Explicit template instantiations for manager methods
template void MemoryPoolManager::registerPool<std::string>(const std::string&, std::shared_ptr<MemoryPool<std::string>>);
template void MemoryPoolManager::registerPool<json>(const std::string&, std::shared_ptr<MemoryPool<json>>);
template void MemoryPoolManager::registerPool<std::vector<uint8_t>>(const std::string&, std::shared_ptr<MemoryPool<std::vector<uint8_t>>>);

template std::shared_ptr<MemoryPool<std::string>> MemoryPoolManager::getPool<std::string>(const std::string&);
template std::shared_ptr<MemoryPool<json>> MemoryPoolManager::getPool<json>(const std::string&);
template std::shared_ptr<MemoryPool<std::vector<uint8_t>>> MemoryPoolManager::getPool<std::vector<uint8_t>>(const std::string&);

// Explicit template instantiations for factory methods
template std::shared_ptr<MemoryPool<std::string>> MemoryPoolFactory::createPool<std::string>(const MemoryPoolConfig&);
template std::shared_ptr<MemoryPool<json>> MemoryPoolFactory::createPool<json>(const MemoryPoolConfig&);
template std::shared_ptr<MemoryPool<std::vector<uint8_t>>> MemoryPoolFactory::createPool<std::vector<uint8_t>>(const MemoryPoolConfig&);

template std::shared_ptr<MemoryPool<std::string>> MemoryPoolFactory::createPool<std::string>(size_t, size_t, bool);
template std::shared_ptr<MemoryPool<json>> MemoryPoolFactory::createPool<json>(size_t, size_t, bool);
template std::shared_ptr<MemoryPool<std::vector<uint8_t>>> MemoryPoolFactory::createPool<std::vector<uint8_t>>(size_t, size_t, bool);

} // namespace performance
} // namespace core
} // namespace hydrogen

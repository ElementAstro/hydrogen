#include "hydrogen/core/performance/serialization_optimizer.h"

#include <algorithm>
#include <future>
#include <sstream>

#ifdef HYDROGEN_ENABLE_COMPRESSION
#include <zlib.h>
#endif

namespace hydrogen {
namespace core {
namespace performance {

SerializationOptimizer::SerializationOptimizer(const SerializationOptimizerConfig& config)
    : config_(config) {
    spdlog::debug("SerializationOptimizer: Created with caching: {}, compression: {}, async: {}",
                  config_.enableCaching, config_.enableCompression, config_.enableAsyncSerialization);
}

SerializationOptimizer::~SerializationOptimizer() {
    spdlog::debug("SerializationOptimizer: Destructor called");
    if (running_.load()) {
        stop();
    }
}

bool SerializationOptimizer::start() {
    spdlog::info("SerializationOptimizer: Starting serialization optimizer");
    
    if (running_.load()) {
        spdlog::warn("SerializationOptimizer: Already running");
        return true;
    }
    
    if (!initialize()) {
        spdlog::error("SerializationOptimizer: Failed to initialize");
        return false;
    }
    
    running_.store(true);
    
    // Start worker threads for async operations
    if (config_.enableAsyncSerialization) {
        for (int i = 0; i < config_.workerThreads; ++i) {
            workerThreads_.emplace_back(
                std::make_unique<std::thread>(&SerializationOptimizer::workerThreadFunction, this)
            );
        }
    }
    
    spdlog::info("SerializationOptimizer: Started with {} worker threads", config_.workerThreads);
    return true;
}

void SerializationOptimizer::stop() {
    spdlog::info("SerializationOptimizer: Stopping serialization optimizer");
    
    if (!running_.load()) {
        spdlog::warn("SerializationOptimizer: Not running");
        return;
    }
    
    running_.store(false);
    
    // Notify all worker threads
    taskAvailable_.notify_all();
    
    // Wait for worker threads to finish
    for (auto& thread : workerThreads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    workerThreads_.clear();
    
    shutdown();
    
    spdlog::info("SerializationOptimizer: Stopped");
}

std::string SerializationOptimizer::serialize(const json& data) {
    if (!running_.load()) {
        spdlog::error("SerializationOptimizer: Cannot serialize - optimizer not running");
        return "";
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::string result;
    
    // Check cache first if enabled
    if (config_.enableCaching) {
        std::string cacheKey = generateCacheKey(data);
        auto cacheEntry = getCacheEntry(cacheKey);
        
        if (cacheEntry) {
            cacheEntry->updateAccess();
            result = cacheEntry->serializedData;
            metrics_.cacheHits.fetch_add(1);
            
            // Decompress if needed
            if (cacheEntry->isCompressed) {
                result = decompressData(result);
            }
            
            spdlog::debug("SerializationOptimizer: Cache hit for serialization");
        } else {
            // Cache miss - perform serialization
            result = serializeInternal(data);
            
            // Store in cache
            setCacheEntry(cacheKey, result, result.size());
            metrics_.cacheMisses.fetch_add(1);
            
            spdlog::debug("SerializationOptimizer: Cache miss - serialized and cached");
        }
    } else {
        // No caching - direct serialization
        result = serializeInternal(data);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    recordSerializationTime(duration.count() / 1000.0);
    
    metrics_.totalSerializations.fetch_add(1);
    metrics_.totalBytesProcessed.fetch_add(result.size());
    
    return result;
}

json SerializationOptimizer::deserialize(const std::string& serializedData) {
    if (!running_.load()) {
        spdlog::error("SerializationOptimizer: Cannot deserialize - optimizer not running");
        return json{};
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    json result = deserializeInternal(serializedData);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    recordDeserializationTime(duration.count() / 1000.0);
    
    metrics_.totalDeserializations.fetch_add(1);
    metrics_.totalBytesProcessed.fetch_add(serializedData.size());
    
    return result;
}

std::future<std::string> SerializationOptimizer::serializeAsync(const json& data) {
    if (!running_.load() || !config_.enableAsyncSerialization) {
        // Fallback to synchronous operation
        std::promise<std::string> promise;
        promise.set_value(serialize(data));
        return promise.get_future();
    }
    
    auto task = std::make_shared<SerializationTask>(SerializationTask::Type::SERIALIZE, data);
    auto future = task->serializePromise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(taskQueueMutex_);
        taskQueue_.push(task);
    }
    
    taskAvailable_.notify_one();
    metrics_.asyncOperations.fetch_add(1);
    
    return future;
}

std::future<json> SerializationOptimizer::deserializeAsync(const std::string& serializedData) {
    if (!running_.load() || !config_.enableAsyncSerialization) {
        // Fallback to synchronous operation
        std::promise<json> promise;
        promise.set_value(deserialize(serializedData));
        return promise.get_future();
    }
    
    auto task = std::make_shared<SerializationTask>(SerializationTask::Type::DESERIALIZE, serializedData);
    auto future = task->deserializePromise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(taskQueueMutex_);
        taskQueue_.push(task);
    }
    
    taskAvailable_.notify_one();
    metrics_.asyncOperations.fetch_add(1);
    
    return future;
}

std::vector<std::string> SerializationOptimizer::serializeBatch(const std::vector<json>& dataList) {
    std::vector<std::string> results;
    results.reserve(dataList.size());
    
    for (const auto& data : dataList) {
        results.push_back(serialize(data));
    }
    
    return results;
}

std::vector<json> SerializationOptimizer::deserializeBatch(const std::vector<std::string>& serializedList) {
    std::vector<json> results;
    results.reserve(serializedList.size());
    
    for (const auto& serializedData : serializedList) {
        results.push_back(deserialize(serializedData));
    }
    
    return results;
}

bool SerializationOptimizer::initialize() {
    spdlog::debug("SerializationOptimizer: Initializing serialization optimizer");
    
    try {
        // Initialize memory pools
        auto& poolManager = MemoryPoolManager::getInstance();
        stringPool_ = poolManager.getStringPool();
        jsonPool_ = poolManager.getJsonPool();
        
        if (!stringPool_ || !jsonPool_) {
            spdlog::error("SerializationOptimizer: Failed to get memory pools");
            return false;
        }
        
        initialized_.store(true);
        spdlog::info("SerializationOptimizer: Initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("SerializationOptimizer: Exception during initialization: {}", e.what());
        return false;
    }
}

void SerializationOptimizer::shutdown() {
    spdlog::debug("SerializationOptimizer: Shutting down serialization optimizer");
    
    // Clear cache
    clearCache();
    
    // Clear task queue
    {
        std::lock_guard<std::mutex> lock(taskQueueMutex_);
        while (!taskQueue_.empty()) {
            taskQueue_.pop();
        }
    }
    
    initialized_.store(false);
    spdlog::info("SerializationOptimizer: Shutdown complete");
}

std::string SerializationOptimizer::serializeInternal(const json& data) {
    try {
        std::string result = data.dump();
        
        // Apply compression if enabled and data is large enough
        if (config_.enableCompression && shouldCompress(result.size())) {
            std::string compressed = compressData(result);
            if (compressed.size() < result.size()) {
                metrics_.compressionOperations.fetch_add(1);
                metrics_.totalBytesCompressed.fetch_add(result.size() - compressed.size());
                
                // Update compression ratio
                double ratio = static_cast<double>(compressed.size()) / result.size();
                double currentRatio = metrics_.compressionRatio.load();
                double newRatio = (currentRatio + ratio) / 2.0;
                metrics_.compressionRatio.store(newRatio);
                
                return compressed;
            }
        }
        
        return result;
        
    } catch (const std::exception& e) {
        spdlog::error("SerializationOptimizer: Exception during serialization: {}", e.what());
        return "";
    }
}

json SerializationOptimizer::deserializeInternal(const std::string& serializedData) {
    try {
        std::string data = serializedData;
        
        // Try decompression if data appears compressed
        if (config_.enableCompression && data.size() > 0) {
            try {
                std::string decompressed = decompressData(data);
                if (!decompressed.empty()) {
                    data = decompressed;
                }
            } catch (...) {
                // Not compressed or decompression failed, use original data
            }
        }
        
        return json::parse(data);
        
    } catch (const std::exception& e) {
        spdlog::error("SerializationOptimizer: Exception during deserialization: {}", e.what());
        return json{};
    }
}

std::string SerializationOptimizer::generateCacheKey(const json& data) {
    // Simple hash-based cache key generation
    std::string dataStr = data.dump();
    std::hash<std::string> hasher;
    size_t hash = hasher(dataStr);
    
    std::stringstream ss;
    ss << "cache_" << std::hex << hash;
    return ss.str();
}

std::shared_ptr<CacheEntry> SerializationOptimizer::getCacheEntry(const std::string& key) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        auto entry = it->second;
        
        // Check if entry is expired
        if (entry->isExpired(config_.cacheTimeout)) {
            cache_.erase(it);
            return nullptr;
        }
        
        return entry;
    }
    
    return nullptr;
}

void SerializationOptimizer::setCacheEntry(const std::string& key, const std::string& data, size_t originalSize) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    // Check cache size limit
    if (cache_.size() >= config_.cacheMaxSize) {
        evictLeastRecentlyUsed();
    }
    
    std::string cacheData = data;
    bool compressed = false;
    
    // Compress cache entry if beneficial
    if (config_.enableCompression && shouldCompress(data.size())) {
        std::string compressedData = compressData(data);
        if (compressedData.size() < data.size()) {
            cacheData = compressedData;
            compressed = true;
        }
    }
    
    auto entry = std::make_shared<CacheEntry>(cacheData, originalSize);
    entry->isCompressed = compressed;
    
    cache_[key] = entry;
}

void SerializationOptimizer::evictLeastRecentlyUsed() {
    if (cache_.empty()) {
        return;
    }
    
    auto oldestIt = cache_.begin();
    auto oldestTime = oldestIt->second->lastAccessed;
    
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second->lastAccessed < oldestTime) {
            oldestTime = it->second->lastAccessed;
            oldestIt = it;
        }
    }
    
    cache_.erase(oldestIt);
}

void SerializationOptimizer::clearCache() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cache_.clear();
    spdlog::debug("SerializationOptimizer: Cache cleared");
}

std::string SerializationOptimizer::compressData(const std::string& data) {
#ifdef HYDROGEN_ENABLE_COMPRESSION
    // Placeholder compression implementation
    // In a real implementation, you would use zlib or another compression library
    return data; // Return uncompressed for now
#else
    return data;
#endif
}

std::string SerializationOptimizer::decompressData(const std::string& compressedData) {
#ifdef HYDROGEN_ENABLE_COMPRESSION
    // Placeholder decompression implementation
    return compressedData; // Return as-is for now
#else
    return compressedData;
#endif
}

bool SerializationOptimizer::shouldCompress(size_t dataSize) const {
    return config_.enableCompression && dataSize >= config_.compressionThreshold;
}

void SerializationOptimizer::workerThreadFunction() {
    spdlog::debug("SerializationOptimizer: Worker thread started");
    
    while (running_.load()) {
        std::shared_ptr<SerializationTask> task = nullptr;
        
        {
            std::unique_lock<std::mutex> lock(taskQueueMutex_);
            taskAvailable_.wait(lock, [this] {
                return !taskQueue_.empty() || !running_.load();
            });
            
            if (!running_.load()) {
                break;
            }
            
            if (!taskQueue_.empty()) {
                task = taskQueue_.front();
                taskQueue_.pop();
            }
        }
        
        if (task) {
            processTask(task);
        }
    }
    
    spdlog::debug("SerializationOptimizer: Worker thread stopped");
}

void SerializationOptimizer::processTask(std::shared_ptr<SerializationTask> task) {
    try {
        if (task->type == SerializationTask::Type::SERIALIZE) {
            std::string result = serialize(task->data);
            task->serializePromise.set_value(result);
        } else {
            json result = deserialize(task->serializedData);
            task->deserializePromise.set_value(result);
        }
    } catch (const std::exception& e) {
        spdlog::error("SerializationOptimizer: Exception processing task: {}", e.what());
        
        if (task->type == SerializationTask::Type::SERIALIZE) {
            task->serializePromise.set_exception(std::current_exception());
        } else {
            task->deserializePromise.set_exception(std::current_exception());
        }
    }
}

void SerializationOptimizer::recordSerializationTime(double timeMs) {
    double currentAvg = metrics_.averageSerializationTime.load();
    double newAvg = (currentAvg + timeMs) / 2.0;
    metrics_.averageSerializationTime.store(newAvg);
}

void SerializationOptimizer::recordDeserializationTime(double timeMs) {
    double currentAvg = metrics_.averageDeserializationTime.load();
    double newAvg = (currentAvg + timeMs) / 2.0;
    metrics_.averageDeserializationTime.store(newAvg);
}

SerializationMetrics SerializationOptimizer::getMetrics() const {
    // Update cache hit ratio
    size_t totalCacheRequests = metrics_.cacheHits.load() + metrics_.cacheMisses.load();
    if (totalCacheRequests > 0) {
        double hitRatio = static_cast<double>(metrics_.cacheHits.load()) / totalCacheRequests;
        const_cast<SerializationMetrics&>(metrics_).cacheHitRatio.store(hitRatio);
    }
    
    return metrics_;
}

// SerializationOptimizerManager implementation
SerializationOptimizerManager::SerializationOptimizerManager() {
    spdlog::debug("SerializationOptimizerManager: Constructor called");
}

SerializationOptimizerManager& SerializationOptimizerManager::getInstance() {
    static SerializationOptimizerManager instance;
    return instance;
}

std::shared_ptr<SerializationOptimizer> SerializationOptimizerManager::getDefaultOptimizer() {
    std::lock_guard<std::mutex> lock(optimizersMutex_);

    if (!defaultOptimizer_) {
        SerializationOptimizerConfig config;
        config.enableCaching = true;
        config.enableCompression = true;
        config.enableAsyncSerialization = true;
        config.cacheMaxSize = 1000;
        config.compressionThreshold = 1024;
        config.workerThreads = 2;

        defaultOptimizer_ = std::make_shared<SerializationOptimizer>(config);
        defaultOptimizer_->start();
    }

    return defaultOptimizer_;
}

std::shared_ptr<SerializationOptimizer> SerializationOptimizerManager::getOptimizer(const std::string& name) {
    std::lock_guard<std::mutex> lock(optimizersMutex_);

    auto it = optimizers_.find(name);
    if (it != optimizers_.end()) {
        return it->second;
    }

    return nullptr;
}

void SerializationOptimizerManager::registerOptimizer(const std::string& name, std::shared_ptr<SerializationOptimizer> optimizer) {
    std::lock_guard<std::mutex> lock(optimizersMutex_);
    optimizers_[name] = optimizer;
    spdlog::info("SerializationOptimizerManager: Registered optimizer: {}", name);
}

void SerializationOptimizerManager::unregisterOptimizer(const std::string& name) {
    std::lock_guard<std::mutex> lock(optimizersMutex_);

    auto it = optimizers_.find(name);
    if (it != optimizers_.end()) {
        if (it->second) {
            it->second->stop();
        }
        optimizers_.erase(it);
        spdlog::info("SerializationOptimizerManager: Unregistered optimizer: {}", name);
    }
}

void SerializationOptimizerManager::stopAllOptimizers() {
    std::lock_guard<std::mutex> lock(optimizersMutex_);

    spdlog::info("SerializationOptimizerManager: Stopping all optimizers");

    if (defaultOptimizer_) {
        defaultOptimizer_->stop();
        defaultOptimizer_.reset();
    }

    for (auto& [name, optimizer] : optimizers_) {
        if (optimizer) {
            optimizer->stop();
        }
    }

    optimizers_.clear();
    spdlog::info("SerializationOptimizerManager: All optimizers stopped");
}

void SerializationOptimizerManager::startAllOptimizers() {
    std::lock_guard<std::mutex> lock(optimizersMutex_);

    spdlog::info("SerializationOptimizerManager: Starting all optimizers");

    for (auto& [name, optimizer] : optimizers_) {
        if (optimizer) {
            optimizer->start();
        }
    }

    spdlog::info("SerializationOptimizerManager: All optimizers started");
}

json SerializationOptimizerManager::getAllOptimizerMetrics() const {
    std::lock_guard<std::mutex> lock(optimizersMutex_);

    json metrics = json::object();

    if (defaultOptimizer_) {
        metrics["default"] = defaultOptimizer_->getMetrics().toJson();
    }

    for (const auto& [name, optimizer] : optimizers_) {
        if (optimizer) {
            metrics[name] = optimizer->getMetrics().toJson();
        }
    }

    return metrics;
}

void SerializationOptimizerManager::setGlobalConfig(const json& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    globalConfig_ = config;
}

json SerializationOptimizerManager::getGlobalConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return globalConfig_;
}

} // namespace performance
} // namespace core
} // namespace hydrogen

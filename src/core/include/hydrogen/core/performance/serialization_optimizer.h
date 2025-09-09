#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <queue>

#include <nlohmann/json.hpp>
#ifdef HYDROGEN_ENABLE_SPDLOG
#include <spdlog/spdlog.h>
#endif

#include "hydrogen/core/performance/memory_pool.h"

namespace hydrogen {
namespace core {
namespace performance {

using json = nlohmann::json;

/**
 * @brief Serialization optimization configuration
 */
struct SerializationOptimizerConfig {
    bool enableCaching = true;                    // Enable serialization caching
    bool enableCompression = false;               // Enable compression for large payloads
    bool enableAsyncSerialization = true;        // Enable async serialization
    size_t cacheMaxSize = 10000;                 // Maximum cache entries
    size_t compressionThreshold = 1024;          // Compress payloads larger than this
    std::chrono::milliseconds cacheTimeout{300000}; // Cache entry timeout (5 minutes)
    std::chrono::milliseconds asyncTimeout{5000};   // Async operation timeout
    int workerThreads = 4;                       // Number of worker threads
    bool enableMetrics = true;                   // Enable performance metrics
    
    json toJson() const {
        json j;
        j["enableCaching"] = enableCaching;
        j["enableCompression"] = enableCompression;
        j["enableAsyncSerialization"] = enableAsyncSerialization;
        j["cacheMaxSize"] = cacheMaxSize;
        j["compressionThreshold"] = compressionThreshold;
        j["cacheTimeout"] = cacheTimeout.count();
        j["asyncTimeout"] = asyncTimeout.count();
        j["workerThreads"] = workerThreads;
        j["enableMetrics"] = enableMetrics;
        return j;
    }
    
    static SerializationOptimizerConfig fromJson(const json& j) {
        SerializationOptimizerConfig config;
        if (j.contains("enableCaching")) config.enableCaching = j["enableCaching"];
        if (j.contains("enableCompression")) config.enableCompression = j["enableCompression"];
        if (j.contains("enableAsyncSerialization")) config.enableAsyncSerialization = j["enableAsyncSerialization"];
        if (j.contains("cacheMaxSize")) config.cacheMaxSize = j["cacheMaxSize"];
        if (j.contains("compressionThreshold")) config.compressionThreshold = j["compressionThreshold"];
        if (j.contains("cacheTimeout")) config.cacheTimeout = std::chrono::milliseconds(j["cacheTimeout"]);
        if (j.contains("asyncTimeout")) config.asyncTimeout = std::chrono::milliseconds(j["asyncTimeout"]);
        if (j.contains("workerThreads")) config.workerThreads = j["workerThreads"];
        if (j.contains("enableMetrics")) config.enableMetrics = j["enableMetrics"];
        return config;
    }
};

/**
 * @brief Serialization metrics
 */
struct SerializationMetrics {
    std::atomic<size_t> totalSerializations{0};
    std::atomic<size_t> totalDeserializations{0};
    std::atomic<size_t> cacheHits{0};
    std::atomic<size_t> cacheMisses{0};
    std::atomic<size_t> compressionOperations{0};
    std::atomic<size_t> asyncOperations{0};
    std::atomic<double> averageSerializationTime{0.0};
    std::atomic<double> averageDeserializationTime{0.0};
    std::atomic<double> cacheHitRatio{0.0};
    std::atomic<double> compressionRatio{0.0};
    std::atomic<size_t> totalBytesProcessed{0};
    std::atomic<size_t> totalBytesCompressed{0};

    // Copy constructor
    SerializationMetrics(const SerializationMetrics& other) {
        totalSerializations.store(other.totalSerializations.load());
        totalDeserializations.store(other.totalDeserializations.load());
        cacheHits.store(other.cacheHits.load());
        cacheMisses.store(other.cacheMisses.load());
        compressionOperations.store(other.compressionOperations.load());
        asyncOperations.store(other.asyncOperations.load());
        averageSerializationTime.store(other.averageSerializationTime.load());
        averageDeserializationTime.store(other.averageDeserializationTime.load());
        cacheHitRatio.store(other.cacheHitRatio.load());
        compressionRatio.store(other.compressionRatio.load());
        totalBytesProcessed.store(other.totalBytesProcessed.load());
        totalBytesCompressed.store(other.totalBytesCompressed.load());
    }

    // Assignment operator
    SerializationMetrics& operator=(const SerializationMetrics& other) {
        if (this != &other) {
            totalSerializations.store(other.totalSerializations.load());
            totalDeserializations.store(other.totalDeserializations.load());
            cacheHits.store(other.cacheHits.load());
            cacheMisses.store(other.cacheMisses.load());
            compressionOperations.store(other.compressionOperations.load());
            asyncOperations.store(other.asyncOperations.load());
            averageSerializationTime.store(other.averageSerializationTime.load());
            averageDeserializationTime.store(other.averageDeserializationTime.load());
            cacheHitRatio.store(other.cacheHitRatio.load());
            compressionRatio.store(other.compressionRatio.load());
            totalBytesProcessed.store(other.totalBytesProcessed.load());
            totalBytesCompressed.store(other.totalBytesCompressed.load());
        }
        return *this;
    }

    // Default constructor
    SerializationMetrics() = default;

    json toJson() const {
        json j;
        j["totalSerializations"] = totalSerializations.load();
        j["totalDeserializations"] = totalDeserializations.load();
        j["cacheHits"] = cacheHits.load();
        j["cacheMisses"] = cacheMisses.load();
        j["compressionOperations"] = compressionOperations.load();
        j["asyncOperations"] = asyncOperations.load();
        j["averageSerializationTime"] = averageSerializationTime.load();
        j["averageDeserializationTime"] = averageDeserializationTime.load();
        j["cacheHitRatio"] = cacheHitRatio.load();
        j["compressionRatio"] = compressionRatio.load();
        j["totalBytesProcessed"] = totalBytesProcessed.load();
        j["totalBytesCompressed"] = totalBytesCompressed.load();
        return j;
    }
};

/**
 * @brief Cached serialization entry
 */
struct CacheEntry {
    std::string serializedData;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastAccessed;
    std::atomic<size_t> accessCount{0};
    size_t originalSize;
    bool isCompressed = false;
    
    CacheEntry(const std::string& data, size_t size)
        : serializedData(data)
        , createdAt(std::chrono::system_clock::now())
        , lastAccessed(createdAt)
        , originalSize(size) {}
    
    void updateAccess() {
        lastAccessed = std::chrono::system_clock::now();
        accessCount.fetch_add(1);
    }
    
    bool isExpired(std::chrono::milliseconds timeout) const {
        auto now = std::chrono::system_clock::now();
        return (now - lastAccessed) > timeout;
    }
};

/**
 * @brief Async serialization task
 */
struct SerializationTask {
    enum class Type { SERIALIZE, DESERIALIZE };
    
    Type type;
    json data;
    std::string serializedData;
    std::promise<std::string> serializePromise;
    std::promise<json> deserializePromise;
    std::chrono::system_clock::time_point createdAt;
    
    SerializationTask(Type t, const json& d)
        : type(t), data(d), createdAt(std::chrono::system_clock::now()) {}
    
    SerializationTask(Type t, const std::string& s)
        : type(t), serializedData(s), createdAt(std::chrono::system_clock::now()) {}
};

/**
 * @brief High-performance JSON serialization optimizer
 */
class SerializationOptimizer {
public:
    explicit SerializationOptimizer(const SerializationOptimizerConfig& config = SerializationOptimizerConfig{});
    ~SerializationOptimizer();

    // Lifecycle management
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // Synchronous operations
    std::string serialize(const json& data);
    json deserialize(const std::string& serializedData);
    
    // Asynchronous operations
    std::future<std::string> serializeAsync(const json& data);
    std::future<json> deserializeAsync(const std::string& serializedData);
    
    // Batch operations
    std::vector<std::string> serializeBatch(const std::vector<json>& dataList);
    std::vector<json> deserializeBatch(const std::vector<std::string>& serializedList);
    
    // Cache management
    void clearCache();
    void cleanupExpiredEntries();
    size_t getCacheSize() const;
    
    // Configuration and metrics
    void updateConfiguration(const SerializationOptimizerConfig& config);
    SerializationOptimizerConfig getConfiguration() const;
    SerializationMetrics getMetrics() const;
    json getDetailedMetrics() const;

private:
    // Configuration
    SerializationOptimizerConfig config_;
    mutable std::mutex configMutex_;
    
    // State
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    
    // Caching
    std::unordered_map<std::string, std::shared_ptr<CacheEntry>> cache_;
    mutable std::mutex cacheMutex_;
    
    // Async processing
    std::queue<std::shared_ptr<SerializationTask>> taskQueue_;
    std::mutex taskQueueMutex_;
    std::condition_variable taskAvailable_;
    std::vector<std::unique_ptr<std::thread>> workerThreads_;
    
    // Memory pools
    std::shared_ptr<MemoryPool<std::string>> stringPool_;
    std::shared_ptr<MemoryPool<json>> jsonPool_;
    
    // Metrics
    mutable SerializationMetrics metrics_;
    
    // Internal methods
    bool initialize();
    void shutdown();
    
    // Serialization core
    std::string serializeInternal(const json& data);
    json deserializeInternal(const std::string& serializedData);
    
    // Caching
    std::string generateCacheKey(const json& data);
    std::shared_ptr<CacheEntry> getCacheEntry(const std::string& key);
    void setCacheEntry(const std::string& key, const std::string& data, size_t originalSize);
    void evictLeastRecentlyUsed();
    
    // Compression
    std::string compressData(const std::string& data);
    std::string decompressData(const std::string& compressedData);
    bool shouldCompress(size_t dataSize) const;
    
    // Async processing
    void workerThreadFunction();
    void processTask(std::shared_ptr<SerializationTask> task);
    
    // Metrics
    void updateMetrics();
    void recordSerializationTime(double timeMs);
    void recordDeserializationTime(double timeMs);
};

/**
 * @brief Serialization optimizer manager
 */
class SerializationOptimizerManager {
public:
    static SerializationOptimizerManager& getInstance();
    
    // Optimizer management
    void registerOptimizer(const std::string& name, std::shared_ptr<SerializationOptimizer> optimizer);
    void unregisterOptimizer(const std::string& name);
    std::shared_ptr<SerializationOptimizer> getOptimizer(const std::string& name);
    
    // Default optimizer
    std::shared_ptr<SerializationOptimizer> getDefaultOptimizer();
    
    // Global operations
    void startAllOptimizers();
    void stopAllOptimizers();
    json getAllOptimizerMetrics() const;
    
    // Configuration
    void setGlobalConfig(const json& config);
    json getGlobalConfig() const;

private:
    SerializationOptimizerManager();
    ~SerializationOptimizerManager() = default;
    
    std::unordered_map<std::string, std::shared_ptr<SerializationOptimizer>> optimizers_;
    mutable std::mutex optimizersMutex_;
    std::shared_ptr<SerializationOptimizer> defaultOptimizer_;
    json globalConfig_;
    mutable std::mutex configMutex_;
};

/**
 * @brief RAII wrapper for optimized serialization
 */
class OptimizedSerializer {
public:
    explicit OptimizedSerializer(std::shared_ptr<SerializationOptimizer> optimizer = nullptr);
    
    // Serialization operations
    std::string operator()(const json& data);
    json parse(const std::string& serializedData);
    
    // Async operations
    std::future<std::string> serializeAsync(const json& data);
    std::future<json> parseAsync(const std::string& serializedData);
    
    // Batch operations
    std::vector<std::string> serializeBatch(const std::vector<json>& dataList);
    std::vector<json> parseBatch(const std::vector<std::string>& serializedList);

private:
    std::shared_ptr<SerializationOptimizer> optimizer_;
};

} // namespace performance
} // namespace core
} // namespace hydrogen

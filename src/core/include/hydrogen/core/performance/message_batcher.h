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
#include <vector>

#include <nlohmann/json.hpp>
#ifdef HYDROGEN_ENABLE_SPDLOG
#include <spdlog/spdlog.h>
#else
#include <hydrogen/core/logging/mock_spdlog.h>
#endif

namespace hydrogen {
namespace core {
namespace performance {

using json = nlohmann::json;

// Forward declarations
struct Message;
struct MessageBatch;
class MessageBatcher;

/**
 * @brief Message structure for batching
 */
struct Message {
    std::string id;
    std::string type;
    std::string destination;
    json payload;
    std::chrono::system_clock::time_point timestamp;
    int priority = 0;  // Higher values = higher priority
    size_t size = 0;   // Message size in bytes
    json metadata;
    
    Message() : timestamp(std::chrono::system_clock::now()) {}
    
    Message(const std::string& msgId, const std::string& msgType, 
            const std::string& dest, const json& data)
        : id(msgId), type(msgType), destination(dest), payload(data)
        , timestamp(std::chrono::system_clock::now()) {
        calculateSize();
    }
    
    void calculateSize() {
        size = id.size() + type.size() + destination.size() + 
               payload.dump().size() + metadata.dump().size();
    }
    
    json toJson() const {
        json j;
        j["id"] = id;
        j["type"] = type;
        j["destination"] = destination;
        j["payload"] = payload;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        j["priority"] = priority;
        j["size"] = size;
        j["metadata"] = metadata;
        return j;
    }
    
    static Message fromJson(const json& j) {
        Message msg;
        if (j.contains("id")) msg.id = j["id"];
        if (j.contains("type")) msg.type = j["type"];
        if (j.contains("destination")) msg.destination = j["destination"];
        if (j.contains("payload")) msg.payload = j["payload"];
        if (j.contains("timestamp")) {
            msg.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(j["timestamp"]));
        }
        if (j.contains("priority")) msg.priority = j["priority"];
        if (j.contains("size")) msg.size = j["size"];
        if (j.contains("metadata")) msg.metadata = j["metadata"];
        return msg;
    }
};

/**
 * @brief Message batch structure
 */
struct MessageBatch {
    std::string batchId;
    std::vector<Message> messages;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point scheduledAt;
    size_t totalSize = 0;
    int averagePriority = 0;
    std::string destination;
    json metadata;
    
    MessageBatch() : createdAt(std::chrono::system_clock::now()) {}
    
    void addMessage(const Message& message) {
        messages.push_back(message);
        totalSize += message.size;
        
        // Update average priority
        if (!messages.empty()) {
            int totalPriority = 0;
            for (const auto& msg : messages) {
                totalPriority += msg.priority;
            }
            averagePriority = totalPriority / static_cast<int>(messages.size());
        }
        
        // Set destination from first message if not set
        if (destination.empty() && !message.destination.empty()) {
            destination = message.destination;
        }
    }
    
    bool isEmpty() const {
        return messages.empty();
    }
    
    size_t getMessageCount() const {
        return messages.size();
    }
    
    json toJson() const {
        json j;
        j["batchId"] = batchId;
        j["messageCount"] = messages.size();
        j["totalSize"] = totalSize;
        j["averagePriority"] = averagePriority;
        j["destination"] = destination;
        j["createdAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            createdAt.time_since_epoch()).count();
        j["scheduledAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            scheduledAt.time_since_epoch()).count();
        j["metadata"] = metadata;
        
        json messagesJson = json::array();
        for (const auto& message : messages) {
            messagesJson.push_back(message.toJson());
        }
        j["messages"] = messagesJson;
        
        return j;
    }
};

/**
 * @brief Message batching configuration
 */
struct MessageBatcherConfig {
    size_t maxBatchSize = 100;                    // Maximum messages per batch
    size_t maxBatchSizeBytes = 1024 * 1024;       // Maximum batch size in bytes (1MB)
    std::chrono::milliseconds batchTimeout{100};   // Maximum time to wait for batch
    std::chrono::milliseconds flushInterval{50};   // How often to check for ready batches
    bool enablePriorityBatching = true;            // Group by priority
    bool enableDestinationBatching = true;        // Group by destination
    bool enableCompression = false;               // Compress batch payload
    int maxConcurrentBatches = 10;               // Maximum concurrent batches being processed
    double compressionThreshold = 0.7;           // Compress if ratio > threshold
    
    json toJson() const {
        json j;
        j["maxBatchSize"] = maxBatchSize;
        j["maxBatchSizeBytes"] = maxBatchSizeBytes;
        j["batchTimeout"] = batchTimeout.count();
        j["flushInterval"] = flushInterval.count();
        j["enablePriorityBatching"] = enablePriorityBatching;
        j["enableDestinationBatching"] = enableDestinationBatching;
        j["enableCompression"] = enableCompression;
        j["maxConcurrentBatches"] = maxConcurrentBatches;
        j["compressionThreshold"] = compressionThreshold;
        return j;
    }
    
    static MessageBatcherConfig fromJson(const json& j) {
        MessageBatcherConfig config;
        if (j.contains("maxBatchSize")) config.maxBatchSize = j["maxBatchSize"];
        if (j.contains("maxBatchSizeBytes")) config.maxBatchSizeBytes = j["maxBatchSizeBytes"];
        if (j.contains("batchTimeout")) config.batchTimeout = std::chrono::milliseconds(j["batchTimeout"]);
        if (j.contains("flushInterval")) config.flushInterval = std::chrono::milliseconds(j["flushInterval"]);
        if (j.contains("enablePriorityBatching")) config.enablePriorityBatching = j["enablePriorityBatching"];
        if (j.contains("enableDestinationBatching")) config.enableDestinationBatching = j["enableDestinationBatching"];
        if (j.contains("enableCompression")) config.enableCompression = j["enableCompression"];
        if (j.contains("maxConcurrentBatches")) config.maxConcurrentBatches = j["maxConcurrentBatches"];
        if (j.contains("compressionThreshold")) config.compressionThreshold = j["compressionThreshold"];
        return config;
    }
};

/**
 * @brief Message batching metrics
 */
struct MessageBatcherMetrics {
    std::atomic<size_t> messagesReceived{0};
    std::atomic<size_t> messagesBatched{0};
    std::atomic<size_t> batchesCreated{0};
    std::atomic<size_t> batchesProcessed{0};
    std::atomic<size_t> batchesFailed{0};
    std::atomic<double> averageBatchSize{0.0};
    std::atomic<double> averageBatchingLatency{0.0};
    std::atomic<double> compressionRatio{0.0};
    std::atomic<size_t> bytesProcessed{0};
    std::atomic<size_t> bytesCompressed{0};

    // Copy constructor
    MessageBatcherMetrics(const MessageBatcherMetrics& other) {
        messagesReceived.store(other.messagesReceived.load());
        messagesBatched.store(other.messagesBatched.load());
        batchesCreated.store(other.batchesCreated.load());
        batchesProcessed.store(other.batchesProcessed.load());
        batchesFailed.store(other.batchesFailed.load());
        averageBatchSize.store(other.averageBatchSize.load());
        averageBatchingLatency.store(other.averageBatchingLatency.load());
        compressionRatio.store(other.compressionRatio.load());
        bytesProcessed.store(other.bytesProcessed.load());
        bytesCompressed.store(other.bytesCompressed.load());
    }

    // Assignment operator
    MessageBatcherMetrics& operator=(const MessageBatcherMetrics& other) {
        if (this != &other) {
            messagesReceived.store(other.messagesReceived.load());
            messagesBatched.store(other.messagesBatched.load());
            batchesCreated.store(other.batchesCreated.load());
            batchesProcessed.store(other.batchesProcessed.load());
            batchesFailed.store(other.batchesFailed.load());
            averageBatchSize.store(other.averageBatchSize.load());
            averageBatchingLatency.store(other.averageBatchingLatency.load());
            compressionRatio.store(other.compressionRatio.load());
            bytesProcessed.store(other.bytesProcessed.load());
            bytesCompressed.store(other.bytesCompressed.load());
        }
        return *this;
    }

    // Default constructor
    MessageBatcherMetrics() = default;

    json toJson() const {
        json j;
        j["messagesReceived"] = messagesReceived.load();
        j["messagesBatched"] = messagesBatched.load();
        j["batchesCreated"] = batchesCreated.load();
        j["batchesProcessed"] = batchesProcessed.load();
        j["batchesFailed"] = batchesFailed.load();
        j["averageBatchSize"] = averageBatchSize.load();
        j["averageBatchingLatency"] = averageBatchingLatency.load();
        j["compressionRatio"] = compressionRatio.load();
        j["bytesProcessed"] = bytesProcessed.load();
        j["bytesCompressed"] = bytesCompressed.load();
        return j;
    }
};

// Callback types
using BatchReadyCallback = std::function<void(const MessageBatch& batch)>;
using BatchProcessedCallback = std::function<void(const std::string& batchId, bool success, const std::string& error)>;

/**
 * @brief High-performance message batcher for improved network efficiency
 */
class MessageBatcher {
public:
    explicit MessageBatcher(const MessageBatcherConfig& config = MessageBatcherConfig{});
    ~MessageBatcher();

    // Lifecycle management
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // Message batching
    bool addMessage(const Message& message);
    bool addMessages(const std::vector<Message>& messages);
    void flushAll();
    void flushDestination(const std::string& destination);

    // Callback management
    void setBatchReadyCallback(BatchReadyCallback callback);
    void setBatchProcessedCallback(BatchProcessedCallback callback);

    // Configuration and metrics
    void updateConfiguration(const MessageBatcherConfig& config);
    MessageBatcherConfig getConfiguration() const;
    MessageBatcherMetrics getMetrics() const;
    json getDetailedMetrics() const;

    // Batch management
    std::vector<std::string> getPendingBatchIds() const;
    size_t getPendingMessageCount() const;
    size_t getPendingBatchCount() const;

private:
    // Configuration
    MessageBatcherConfig config_;
    mutable std::mutex configMutex_;

    // State
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};

    // Message storage
    struct BatchingQueue {
        std::queue<Message> messages;
        std::chrono::system_clock::time_point oldestMessageTime;
        size_t totalSize = 0;
        
        void addMessage(const Message& message) {
            if (messages.empty()) {
                oldestMessageTime = message.timestamp;
            }
            messages.push(message);
            totalSize += message.size;
        }
        
        bool isEmpty() const {
            return messages.empty();
        }
        
        void clear() {
            while (!messages.empty()) {
                messages.pop();
            }
            totalSize = 0;
        }
    };

    // Batching queues organized by destination and priority
    std::unordered_map<std::string, std::unordered_map<int, BatchingQueue>> batchingQueues_;
    mutable std::mutex queuesMutex_;

    // Ready batches
    std::queue<MessageBatch> readyBatches_;
    mutable std::mutex readyBatchesMutex_;
    std::condition_variable batchReady_;

    // Callbacks
    BatchReadyCallback batchReadyCallback_;
    BatchProcessedCallback batchProcessedCallback_;
    mutable std::mutex callbacksMutex_;

    // Metrics
    mutable MessageBatcherMetrics metrics_;

    // Background threads
    std::unique_ptr<std::thread> batchingThread_;
    std::unique_ptr<std::thread> processingThread_;
    std::condition_variable shutdownCondition_;
    std::mutex shutdownMutex_;

    // Internal methods
    bool initialize();
    void shutdown();

    // Thread functions
    void batchingThreadFunction();
    void processingThreadFunction();

    // Batching logic
    void processBatchingQueues();
    MessageBatch createBatch(const std::string& destination, int priority);
    bool shouldCreateBatch(const BatchingQueue& queue) const;
    std::string generateBatchId();

    // Compression
    std::string compressData(const std::string& data);
    std::string decompressData(const std::string& compressedData);

    // Utility methods
    void updateMetrics();
    std::string getBatchingKey(const std::string& destination, int priority) const;
};

/**
 * @brief Message batch processor interface
 */
class IBatchProcessor {
public:
    virtual ~IBatchProcessor() = default;
    virtual bool processBatch(const MessageBatch& batch) = 0;
    virtual std::string getProcessorType() const = 0;
};

/**
 * @brief Message batcher manager for multiple batchers
 */
class MessageBatcherManager {
public:
    static MessageBatcherManager& getInstance();

    // Batcher management
    void registerBatcher(const std::string& name, std::shared_ptr<MessageBatcher> batcher);
    void unregisterBatcher(const std::string& name);
    std::shared_ptr<MessageBatcher> getBatcher(const std::string& name);

    // Global operations
    void startAllBatchers();
    void stopAllBatchers();
    json getAllBatcherMetrics() const;

    // Configuration
    void setGlobalConfig(const json& config);
    json getGlobalConfig() const;

private:
    MessageBatcherManager() = default;
    ~MessageBatcherManager() = default;

    std::unordered_map<std::string, std::shared_ptr<MessageBatcher>> batchers_;
    mutable std::mutex batchersMutex_;
    json globalConfig_;
    mutable std::mutex configMutex_;
};

} // namespace performance
} // namespace core
} // namespace hydrogen

#include "hydrogen/core/performance/message_batcher.h"

#include <algorithm>
#include <random>
#include <sstream>

#ifdef HYDROGEN_ENABLE_COMPRESSION
#include <zlib.h>
#endif

namespace hydrogen {
namespace core {
namespace performance {

MessageBatcher::MessageBatcher(const MessageBatcherConfig& config)
    : config_(config) {
    spdlog::debug("MessageBatcher: Created with max batch size: {}", config_.maxBatchSize);
}

MessageBatcher::~MessageBatcher() {
    spdlog::debug("MessageBatcher: Destructor called");
    if (running_.load()) {
        stop();
    }
}

bool MessageBatcher::start() {
    spdlog::info("MessageBatcher: Starting message batcher");
    
    if (running_.load()) {
        spdlog::warn("MessageBatcher: Already running");
        return true;
    }
    
    if (!initialize()) {
        spdlog::error("MessageBatcher: Failed to initialize");
        return false;
    }
    
    running_.store(true);
    
    // Start background threads
    batchingThread_ = std::make_unique<std::thread>(&MessageBatcher::batchingThreadFunction, this);
    processingThread_ = std::make_unique<std::thread>(&MessageBatcher::processingThreadFunction, this);
    
    spdlog::info("MessageBatcher: Started successfully");
    return true;
}

void MessageBatcher::stop() {
    spdlog::info("MessageBatcher: Stopping message batcher");
    
    if (!running_.load()) {
        spdlog::warn("MessageBatcher: Not running");
        return;
    }
    
    running_.store(false);
    
    // Notify shutdown condition
    {
        std::lock_guard<std::mutex> lock(shutdownMutex_);
        shutdownCondition_.notify_all();
    }
    
    // Notify batch processing
    batchReady_.notify_all();
    
    // Wait for threads to finish
    if (batchingThread_ && batchingThread_->joinable()) {
        batchingThread_->join();
    }
    
    if (processingThread_ && processingThread_->joinable()) {
        processingThread_->join();
    }
    
    // Flush remaining messages
    flushAll();
    
    shutdown();
    
    spdlog::info("MessageBatcher: Stopped");
}

bool MessageBatcher::addMessage(const Message& message) {
    if (!running_.load()) {
        spdlog::error("MessageBatcher: Cannot add message - batcher not running");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(queuesMutex_);
    
    // Determine batching key
    std::string destination = config_.enableDestinationBatching ? message.destination : "default";
    int priority = config_.enablePriorityBatching ? message.priority : 0;
    
    // Add to appropriate queue
    batchingQueues_[destination][priority].addMessage(message);
    
    metrics_.messagesReceived.fetch_add(1);
    
    spdlog::debug("MessageBatcher: Added message {} to queue [{}:{}]", 
                  message.id, destination, priority);
    
    return true;
}

bool MessageBatcher::addMessages(const std::vector<Message>& messages) {
    if (!running_.load()) {
        spdlog::error("MessageBatcher: Cannot add messages - batcher not running");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(queuesMutex_);
    
    for (const auto& message : messages) {
        std::string destination = config_.enableDestinationBatching ? message.destination : "default";
        int priority = config_.enablePriorityBatching ? message.priority : 0;
        
        batchingQueues_[destination][priority].addMessage(message);
        metrics_.messagesReceived.fetch_add(1);
    }
    
    spdlog::debug("MessageBatcher: Added {} messages to queues", messages.size());
    
    return true;
}

void MessageBatcher::flushAll() {
    spdlog::debug("MessageBatcher: Flushing all pending messages");
    
    std::lock_guard<std::mutex> lock(queuesMutex_);
    
    for (auto& [destination, priorityQueues] : batchingQueues_) {
        for (auto& [priority, queue] : priorityQueues) {
            if (!queue.isEmpty()) {
                MessageBatch batch = createBatch(destination, priority);
                
                // Move all messages from queue to batch
                while (!queue.messages.empty()) {
                    batch.addMessage(queue.messages.front());
                    queue.messages.pop();
                }
                queue.totalSize = 0;
                
                if (!batch.isEmpty()) {
                    batch.batchId = generateBatchId();
                    batch.scheduledAt = std::chrono::system_clock::now();
                    
                    {
                        std::lock_guard<std::mutex> readyLock(readyBatchesMutex_);
                        readyBatches_.push(batch);
                    }
                    
                    batchReady_.notify_one();
                    metrics_.batchesCreated.fetch_add(1);
                    
                    spdlog::debug("MessageBatcher: Flushed batch {} with {} messages", 
                                  batch.batchId, batch.getMessageCount());
                }
            }
        }
    }
}

void MessageBatcher::flushDestination(const std::string& destination) {
    spdlog::debug("MessageBatcher: Flushing messages for destination: {}", destination);
    
    std::lock_guard<std::mutex> lock(queuesMutex_);
    
    auto destIt = batchingQueues_.find(destination);
    if (destIt != batchingQueues_.end()) {
        for (auto& [priority, queue] : destIt->second) {
            if (!queue.isEmpty()) {
                MessageBatch batch = createBatch(destination, priority);
                
                // Move all messages from queue to batch
                while (!queue.messages.empty()) {
                    batch.addMessage(queue.messages.front());
                    queue.messages.pop();
                }
                queue.totalSize = 0;
                
                if (!batch.isEmpty()) {
                    batch.batchId = generateBatchId();
                    batch.scheduledAt = std::chrono::system_clock::now();
                    
                    {
                        std::lock_guard<std::mutex> readyLock(readyBatchesMutex_);
                        readyBatches_.push(batch);
                    }
                    
                    batchReady_.notify_one();
                    metrics_.batchesCreated.fetch_add(1);
                }
            }
        }
    }
}

void MessageBatcher::setBatchReadyCallback(BatchReadyCallback callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    batchReadyCallback_ = callback;
}

void MessageBatcher::setBatchProcessedCallback(BatchProcessedCallback callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    batchProcessedCallback_ = callback;
}

bool MessageBatcher::initialize() {
    spdlog::debug("MessageBatcher: Initializing message batcher");
    
    try {
        // Validate configuration
        if (config_.maxBatchSize == 0) {
            spdlog::error("MessageBatcher: Invalid configuration - maxBatchSize cannot be 0");
            return false;
        }
        
        if (config_.maxBatchSizeBytes == 0) {
            spdlog::error("MessageBatcher: Invalid configuration - maxBatchSizeBytes cannot be 0");
            return false;
        }
        
        initialized_.store(true);
        spdlog::info("MessageBatcher: Initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("MessageBatcher: Exception during initialization: {}", e.what());
        return false;
    }
}

void MessageBatcher::shutdown() {
    spdlog::debug("MessageBatcher: Shutting down message batcher");
    
    // Clear all queues
    {
        std::lock_guard<std::mutex> lock(queuesMutex_);
        for (auto& [destination, priorityQueues] : batchingQueues_) {
            for (auto& [priority, queue] : priorityQueues) {
                queue.clear();
            }
        }
        batchingQueues_.clear();
    }
    
    // Clear ready batches
    {
        std::lock_guard<std::mutex> lock(readyBatchesMutex_);
        while (!readyBatches_.empty()) {
            readyBatches_.pop();
        }
    }
    
    initialized_.store(false);
    spdlog::info("MessageBatcher: Shutdown complete");
}

void MessageBatcher::batchingThreadFunction() {
    spdlog::debug("MessageBatcher: Batching thread started");
    
    while (running_.load()) {
        try {
            processBatchingQueues();
            updateMetrics();
            
        } catch (const std::exception& e) {
            spdlog::error("MessageBatcher: Exception in batching thread: {}", e.what());
        }
        
        // Wait for next flush interval or shutdown
        std::unique_lock<std::mutex> lock(shutdownMutex_);
        if (shutdownCondition_.wait_for(lock, config_.flushInterval, 
                                       [this] { return !running_.load(); })) {
            break;
        }
    }
    
    spdlog::debug("MessageBatcher: Batching thread stopped");
}

void MessageBatcher::processingThreadFunction() {
    spdlog::debug("MessageBatcher: Processing thread started");
    
    while (running_.load()) {
        try {
            std::unique_lock<std::mutex> lock(readyBatchesMutex_);
            
            // Wait for ready batches or shutdown
            batchReady_.wait(lock, [this] {
                return !readyBatches_.empty() || !running_.load();
            });
            
            if (!running_.load()) {
                break;
            }
            
            // Process all ready batches
            while (!readyBatches_.empty()) {
                MessageBatch batch = readyBatches_.front();
                readyBatches_.pop();
                lock.unlock();
                
                // Call batch ready callback
                {
                    std::lock_guard<std::mutex> callbackLock(callbacksMutex_);
                    if (batchReadyCallback_) {
                        try {
                            batchReadyCallback_(batch);
                            metrics_.batchesProcessed.fetch_add(1);
                            metrics_.messagesBatched.fetch_add(batch.getMessageCount());
                            
                            // Call processed callback
                            if (batchProcessedCallback_) {
                                batchProcessedCallback_(batch.batchId, true, "");
                            }
                            
                        } catch (const std::exception& e) {
                            spdlog::error("MessageBatcher: Exception in batch callback: {}", e.what());
                            metrics_.batchesFailed.fetch_add(1);
                            
                            if (batchProcessedCallback_) {
                                batchProcessedCallback_(batch.batchId, false, e.what());
                            }
                        }
                    }
                }
                
                lock.lock();
            }
            
        } catch (const std::exception& e) {
            spdlog::error("MessageBatcher: Exception in processing thread: {}", e.what());
        }
    }
    
    spdlog::debug("MessageBatcher: Processing thread stopped");
}

void MessageBatcher::processBatchingQueues() {
    std::lock_guard<std::mutex> lock(queuesMutex_);
    
    auto now = std::chrono::system_clock::now();
    
    for (auto& [destination, priorityQueues] : batchingQueues_) {
        for (auto& [priority, queue] : priorityQueues) {
            if (shouldCreateBatch(queue)) {
                MessageBatch batch = createBatch(destination, priority);
                
                // Move messages from queue to batch
                size_t messagesToMove = std::min(queue.messages.size(), config_.maxBatchSize);
                size_t bytesToMove = 0;
                
                for (size_t i = 0; i < messagesToMove && bytesToMove < config_.maxBatchSizeBytes; ++i) {
                    const auto& message = queue.messages.front();
                    if (bytesToMove + message.size <= config_.maxBatchSizeBytes) {
                        batch.addMessage(message);
                        bytesToMove += message.size;
                        queue.messages.pop();
                        queue.totalSize -= message.size;
                    } else {
                        break;
                    }
                }
                
                if (!batch.isEmpty()) {
                    batch.batchId = generateBatchId();
                    batch.scheduledAt = now;
                    
                    {
                        std::lock_guard<std::mutex> readyLock(readyBatchesMutex_);
                        readyBatches_.push(batch);
                    }
                    
                    batchReady_.notify_one();
                    metrics_.batchesCreated.fetch_add(1);
                    
                    spdlog::debug("MessageBatcher: Created batch {} with {} messages for [{}:{}]", 
                                  batch.batchId, batch.getMessageCount(), destination, priority);
                }
            }
        }
    }
}

MessageBatch MessageBatcher::createBatch(const std::string& destination, int priority) {
    MessageBatch batch;
    batch.destination = destination;
    batch.createdAt = std::chrono::system_clock::now();
    return batch;
}

bool MessageBatcher::shouldCreateBatch(const BatchingQueue& queue) const {
    if (queue.isEmpty()) {
        return false;
    }
    
    // Check size limits
    if (queue.messages.size() >= config_.maxBatchSize) {
        return true;
    }
    
    if (queue.totalSize >= config_.maxBatchSizeBytes) {
        return true;
    }
    
    // Check timeout
    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - queue.oldestMessageTime);
    if (age >= config_.batchTimeout) {
        return true;
    }
    
    return false;
}

std::string MessageBatcher::generateBatchId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "batch_";
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

void MessageBatcher::updateMetrics() {
    // Update average batch size
    size_t totalBatches = metrics_.batchesCreated.load();
    size_t totalMessages = metrics_.messagesBatched.load();
    
    if (totalBatches > 0) {
        double avgBatchSize = static_cast<double>(totalMessages) / totalBatches;
        metrics_.averageBatchSize.store(avgBatchSize);
    }
    
    // Update other metrics as needed
    // TODO: Add more sophisticated metrics calculation
}

MessageBatcherMetrics MessageBatcher::getMetrics() const {
    return metrics_;
}

json MessageBatcher::getDetailedMetrics() const {
    auto metrics = getMetrics();
    json j = metrics.toJson();
    
    // Add queue status
    std::lock_guard<std::mutex> lock(queuesMutex_);
    j["queueStatus"] = json::object();
    
    for (const auto& [destination, priorityQueues] : batchingQueues_) {
        json destJson = json::object();
        for (const auto& [priority, queue] : priorityQueues) {
            json queueJson;
            queueJson["messageCount"] = queue.messages.size();
            queueJson["totalSize"] = queue.totalSize;
            queueJson["isEmpty"] = queue.isEmpty();
            
            destJson[std::to_string(priority)] = queueJson;
        }
        j["queueStatus"][destination] = destJson;
    }
    
    return j;
}

void MessageBatcher::updateConfiguration(const MessageBatcherConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
    spdlog::info("MessageBatcher: Configuration updated");
}

MessageBatcherConfig MessageBatcher::getConfiguration() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

} // namespace performance
} // namespace core
} // namespace hydrogen

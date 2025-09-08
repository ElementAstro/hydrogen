#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>

#include "hydrogen/core/performance/message_batcher.h"

using namespace hydrogen::core::performance;
using namespace std::chrono_literals;

class MessageBatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.maxBatchSize = 5;
        config_.maxBatchSizeBytes = 1024;
        config_.batchTimeout = 100ms;
        config_.flushInterval = 50ms;
        config_.enablePriorityBatching = true;
        config_.enableDestinationBatching = true;
        
        batcher_ = std::make_unique<MessageBatcher>(config_);
        
        // Set up callbacks
        batchesReceived_.clear();
        batchesProcessed_.clear();
        
        batcher_->setBatchReadyCallback([this](const MessageBatch& batch) {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            batchesReceived_.push_back(batch);
        });
        
        batcher_->setBatchProcessedCallback([this](const std::string& batchId, bool success, const std::string& error) {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            batchesProcessed_.push_back({batchId, success, error});
        });
    }
    
    void TearDown() override {
        if (batcher_ && batcher_->isRunning()) {
            batcher_->stop();
        }
        batcher_.reset();
    }
    
    Message createTestMessage(const std::string& id, const std::string& destination = "test_dest", int priority = 0) {
        Message msg(id, "test_type", destination, json{{"data", "test_payload_" + id}});
        msg.priority = priority;
        msg.calculateSize();
        return msg;
    }
    
    void waitForBatches(size_t expectedCount, std::chrono::milliseconds timeout = 1000ms) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (batchesReceived_.size() >= expectedCount) {
                    return;
                }
            }
            std::this_thread::sleep_for(10ms);
        }
    }
    
    MessageBatcherConfig config_;
    std::unique_ptr<MessageBatcher> batcher_;
    
    // Callback tracking
    std::vector<MessageBatch> batchesReceived_;
    std::vector<std::tuple<std::string, bool, std::string>> batchesProcessed_;
    std::mutex callbackMutex_;
};

TEST_F(MessageBatcherTest, StartAndStop) {
    // Test initial state
    EXPECT_FALSE(batcher_->isRunning());
    
    // Test start
    EXPECT_TRUE(batcher_->start());
    EXPECT_TRUE(batcher_->isRunning());
    
    // Test double start
    EXPECT_TRUE(batcher_->start()); // Should succeed (already running)
    
    // Test stop
    batcher_->stop();
    EXPECT_FALSE(batcher_->isRunning());
}

TEST_F(MessageBatcherTest, BasicMessageBatching) {
    ASSERT_TRUE(batcher_->start());
    
    // Add messages to trigger batching by size
    for (int i = 0; i < static_cast<int>(config_.maxBatchSize); ++i) {
        Message msg = createTestMessage("msg_" + std::to_string(i));
        EXPECT_TRUE(batcher_->addMessage(msg));
    }
    
    // Wait for batch to be created
    waitForBatches(1);
    
    // Check that batch was created
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        ASSERT_EQ(batchesReceived_.size(), 1);
        
        const auto& batch = batchesReceived_[0];
        EXPECT_EQ(batch.getMessageCount(), config_.maxBatchSize);
        EXPECT_EQ(batch.destination, "test_dest");
        EXPECT_FALSE(batch.batchId.empty());
    }
    
    // Check metrics
    auto metrics = batcher_->getMetrics();
    EXPECT_EQ(metrics.messagesReceived.load(), config_.maxBatchSize);
    EXPECT_EQ(metrics.batchesCreated.load(), 1);
    EXPECT_EQ(metrics.batchesProcessed.load(), 1);
}

TEST_F(MessageBatcherTest, TimeoutBasedBatching) {
    ASSERT_TRUE(batcher_->start());
    
    // Add fewer messages than batch size
    for (int i = 0; i < 2; ++i) {
        Message msg = createTestMessage("timeout_msg_" + std::to_string(i));
        EXPECT_TRUE(batcher_->addMessage(msg));
    }
    
    // Wait for timeout-based batching
    waitForBatches(1, config_.batchTimeout + 200ms);
    
    // Check that batch was created due to timeout
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        ASSERT_EQ(batchesReceived_.size(), 1);
        
        const auto& batch = batchesReceived_[0];
        EXPECT_EQ(batch.getMessageCount(), 2);
        EXPECT_EQ(batch.destination, "test_dest");
    }
}

TEST_F(MessageBatcherTest, DestinationBasedBatching) {
    ASSERT_TRUE(batcher_->start());
    
    // Add messages to different destinations
    std::vector<std::string> destinations = {"dest_a", "dest_b", "dest_c"};
    
    for (const auto& dest : destinations) {
        for (int i = 0; i < 3; ++i) {
            Message msg = createTestMessage("msg_" + dest + "_" + std::to_string(i), dest);
            EXPECT_TRUE(batcher_->addMessage(msg));
        }
    }
    
    // Flush to create batches
    batcher_->flushAll();
    
    // Wait for batches
    waitForBatches(3);
    
    // Check that separate batches were created for each destination
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        EXPECT_EQ(batchesReceived_.size(), 3);
        
        std::set<std::string> receivedDestinations;
        for (const auto& batch : batchesReceived_) {
            receivedDestinations.insert(batch.destination);
            EXPECT_EQ(batch.getMessageCount(), 3);
        }
        
        EXPECT_EQ(receivedDestinations.size(), 3);
        for (const auto& dest : destinations) {
            EXPECT_TRUE(receivedDestinations.count(dest) > 0);
        }
    }
}

TEST_F(MessageBatcherTest, PriorityBasedBatching) {
    ASSERT_TRUE(batcher_->start());
    
    // Add messages with different priorities
    std::vector<int> priorities = {1, 2, 3};
    
    for (int priority : priorities) {
        for (int i = 0; i < 2; ++i) {
            Message msg = createTestMessage("msg_p" + std::to_string(priority) + "_" + std::to_string(i));
            msg.priority = priority;
            EXPECT_TRUE(batcher_->addMessage(msg));
        }
    }
    
    // Flush to create batches
    batcher_->flushAll();
    
    // Wait for batches
    waitForBatches(3);
    
    // Check that separate batches were created for each priority
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        EXPECT_EQ(batchesReceived_.size(), 3);
        
        std::set<int> receivedPriorities;
        for (const auto& batch : batchesReceived_) {
            receivedPriorities.insert(batch.averagePriority);
            EXPECT_EQ(batch.getMessageCount(), 2);
        }
        
        EXPECT_EQ(receivedPriorities.size(), 3);
    }
}

TEST_F(MessageBatcherTest, SizeBasedBatching) {
    // Configure small byte limit
    config_.maxBatchSizeBytes = 200;
    batcher_ = std::make_unique<MessageBatcher>(config_);
    
    // Reset callbacks
    batcher_->setBatchReadyCallback([this](const MessageBatch& batch) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        batchesReceived_.push_back(batch);
    });
    
    ASSERT_TRUE(batcher_->start());
    
    // Add messages that will exceed byte limit
    for (int i = 0; i < 10; ++i) {
        Message msg = createTestMessage("large_msg_" + std::to_string(i));
        // Make payload larger
        msg.payload = json{{"data", std::string(50, 'x')}};
        msg.calculateSize();
        EXPECT_TRUE(batcher_->addMessage(msg));
    }
    
    // Wait for batches to be created
    waitForBatches(1, 500ms);
    
    // Should have created batches based on size limit
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        EXPECT_GE(batchesReceived_.size(), 1);
        
        for (const auto& batch : batchesReceived_) {
            EXPECT_LE(batch.totalSize, config_.maxBatchSizeBytes);
        }
    }
}

TEST_F(MessageBatcherTest, FlushOperations) {
    ASSERT_TRUE(batcher_->start());
    
    // Add messages to different destinations
    Message msg1 = createTestMessage("msg1", "dest_a");
    Message msg2 = createTestMessage("msg2", "dest_b");
    Message msg3 = createTestMessage("msg3", "dest_a");
    
    EXPECT_TRUE(batcher_->addMessage(msg1));
    EXPECT_TRUE(batcher_->addMessage(msg2));
    EXPECT_TRUE(batcher_->addMessage(msg3));
    
    // Flush specific destination
    batcher_->flushDestination("dest_a");
    
    waitForBatches(1);
    
    // Should have one batch for dest_a
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        ASSERT_EQ(batchesReceived_.size(), 1);
        EXPECT_EQ(batchesReceived_[0].destination, "dest_a");
        EXPECT_EQ(batchesReceived_[0].getMessageCount(), 2);
    }
    
    // Flush all remaining
    batcher_->flushAll();
    
    waitForBatches(2);
    
    // Should now have batch for dest_b as well
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        EXPECT_EQ(batchesReceived_.size(), 2);
    }
}

TEST_F(MessageBatcherTest, ConcurrentMessageAddition) {
    ASSERT_TRUE(batcher_->start());
    
    const int numThreads = 5;
    const int messagesPerThread = 20;
    std::vector<std::thread> threads;
    std::atomic<int> totalMessagesAdded{0};
    
    // Launch concurrent threads adding messages
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, messagesPerThread, &totalMessagesAdded]() {
            for (int i = 0; i < messagesPerThread; ++i) {
                Message msg = createTestMessage("thread_" + std::to_string(t) + "_msg_" + std::to_string(i));
                if (batcher_->addMessage(msg)) {
                    totalMessagesAdded.fetch_add(1);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Flush all messages
    batcher_->flushAll();
    
    // Wait for processing
    waitForBatches(1, 1000ms);
    
    // Check that all messages were processed
    auto metrics = batcher_->getMetrics();
    EXPECT_EQ(metrics.messagesReceived.load(), totalMessagesAdded.load());
    EXPECT_GT(metrics.batchesCreated.load(), 0);
}

TEST_F(MessageBatcherTest, MessageSerialization) {
    // Test message JSON serialization
    Message msg = createTestMessage("test_msg", "test_dest", 5);
    msg.metadata = json{{"custom", "value"}};
    
    json msgJson = msg.toJson();
    Message deserializedMsg = Message::fromJson(msgJson);
    
    EXPECT_EQ(msg.id, deserializedMsg.id);
    EXPECT_EQ(msg.type, deserializedMsg.type);
    EXPECT_EQ(msg.destination, deserializedMsg.destination);
    EXPECT_EQ(msg.payload, deserializedMsg.payload);
    EXPECT_EQ(msg.priority, deserializedMsg.priority);
    EXPECT_EQ(msg.metadata, deserializedMsg.metadata);
}

TEST_F(MessageBatcherTest, BatchSerialization) {
    MessageBatch batch;
    batch.batchId = "test_batch_123";
    batch.destination = "test_dest";
    batch.addMessage(createTestMessage("msg1"));
    batch.addMessage(createTestMessage("msg2"));
    
    json batchJson = batch.toJson();
    
    EXPECT_EQ(batchJson["batchId"], "test_batch_123");
    EXPECT_EQ(batchJson["destination"], "test_dest");
    EXPECT_EQ(batchJson["messageCount"], 2);
    EXPECT_TRUE(batchJson.contains("messages"));
    EXPECT_EQ(batchJson["messages"].size(), 2);
}

TEST_F(MessageBatcherTest, ConfigurationUpdate) {
    ASSERT_TRUE(batcher_->start());
    
    // Update configuration
    MessageBatcherConfig newConfig = config_;
    newConfig.maxBatchSize = 10;
    newConfig.batchTimeout = 200ms;
    
    batcher_->updateConfiguration(newConfig);
    
    auto retrievedConfig = batcher_->getConfiguration();
    EXPECT_EQ(retrievedConfig.maxBatchSize, 10);
    EXPECT_EQ(retrievedConfig.batchTimeout, 200ms);
}

TEST_F(MessageBatcherTest, Metrics) {
    ASSERT_TRUE(batcher_->start());
    
    // Add some messages and create batches
    for (int i = 0; i < 10; ++i) {
        Message msg = createTestMessage("metrics_msg_" + std::to_string(i));
        EXPECT_TRUE(batcher_->addMessage(msg));
    }
    
    batcher_->flushAll();
    waitForBatches(1);
    
    // Check metrics
    auto metrics = batcher_->getMetrics();
    EXPECT_EQ(metrics.messagesReceived.load(), 10);
    EXPECT_GE(metrics.batchesCreated.load(), 1);
    EXPECT_GE(metrics.batchesProcessed.load(), 1);
    
    // Check detailed metrics
    auto detailedMetrics = batcher_->getDetailedMetrics();
    EXPECT_TRUE(detailedMetrics.contains("queueStatus"));
    EXPECT_TRUE(detailedMetrics.contains("messagesReceived"));
    EXPECT_TRUE(detailedMetrics.contains("batchesCreated"));
}

TEST_F(MessageBatcherTest, ErrorHandling) {
    ASSERT_TRUE(batcher_->start());
    
    // Test adding message when not running
    batcher_->stop();
    
    Message msg = createTestMessage("error_msg");
    EXPECT_FALSE(batcher_->addMessage(msg));
    
    // Test empty message handling
    std::vector<Message> emptyMessages;
    EXPECT_FALSE(batcher_->addMessages(emptyMessages));
}

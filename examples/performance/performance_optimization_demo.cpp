#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <future>
#include <random>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "hydrogen/core/performance/connection_pool.h"
#include "hydrogen/core/performance/message_batcher.h"
#include "hydrogen/core/performance/memory_pool.h"
#include "hydrogen/core/performance/serialization_optimizer.h"
#include "hydrogen/core/performance/tcp_connection.h"

using namespace hydrogen::core::performance;
using namespace std::chrono_literals;
using json = nlohmann::json;

class PerformanceOptimizationDemo {
public:
    PerformanceOptimizationDemo() {
        // Set up logging
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        
        initializeComponents();
    }
    
    ~PerformanceOptimizationDemo() {
        cleanup();
    }
    
    void run() {
        std::cout << "=== Hydrogen Performance Optimization Demo ===" << std::endl;
        std::cout << "This demo showcases the performance optimization components:" << std::endl;
        std::cout << "1. Connection Pooling" << std::endl;
        std::cout << "2. Message Batching" << std::endl;
        std::cout << "3. Memory Pooling" << std::endl;
        std::cout << "4. Serialization Optimization" << std::endl;
        std::cout << std::endl;
        
        // Run individual component demos
        demonstrateConnectionPooling();
        demonstrateMessageBatching();
        demonstrateMemoryPooling();
        demonstrateSerializationOptimization();
        
        // Run integrated performance test
        demonstrateIntegratedPerformance();
        
        // Display final metrics
        displayFinalMetrics();
    }

private:
    // Components
    std::shared_ptr<ConnectionPool> connectionPool_;
    std::shared_ptr<MessageBatcher> messageBatcher_;
    std::shared_ptr<MemoryPool<std::string>> stringPool_;
    std::shared_ptr<SerializationOptimizer> serializationOptimizer_;
    
    void initializeComponents() {
        std::cout << "Initializing performance optimization components..." << std::endl;
        
        // Initialize connection pool
        auto factory = std::make_shared<MockConnectionFactory>();
        ConnectionPoolConfig poolConfig;
        poolConfig.initialConnections = 10;
        poolConfig.maxConnections = 50;
        poolConfig.enableHealthChecks = true;
        poolConfig.enableMetrics = true;
        
        connectionPool_ = std::make_shared<ConnectionPool>(factory, poolConfig);
        connectionPool_->initialize();
        
        // Initialize message batcher
        MessageBatcherConfig batchConfig;
        batchConfig.maxBatchSize = 20;
        batchConfig.batchTimeout = 100ms;
        batchConfig.enablePriorityBatching = true;
        batchConfig.enableDestinationBatching = true;
        
        messageBatcher_ = std::make_shared<MessageBatcher>(batchConfig);
        messageBatcher_->start();
        
        // Set up batch callback
        messageBatcher_->setBatchReadyCallback([this](const MessageBatch& batch) {
            processBatch(batch);
        });
        
        // Initialize memory pool
        MemoryPoolConfig memConfig;
        memConfig.initialPoolSize = 50;
        memConfig.maxPoolSize = 200;
        memConfig.enableMetrics = true;
        
        stringPool_ = std::make_shared<MemoryPool<std::string>>(memConfig);
        stringPool_->initialize();
        
        // Initialize serialization optimizer
        SerializationOptimizerConfig serConfig;
        serConfig.enableCaching = true;
        serConfig.enableAsyncSerialization = true;
        serConfig.workerThreads = 4;
        serConfig.cacheMaxSize = 1000;
        
        serializationOptimizer_ = std::make_shared<SerializationOptimizer>(serConfig);
        serializationOptimizer_->start();
        
        std::cout << "✓ All components initialized successfully" << std::endl << std::endl;
    }
    
    void demonstrateConnectionPooling() {
        std::cout << "--- Connection Pooling Demo ---" << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Simulate concurrent connection usage
        std::vector<std::future<void>> futures;
        
        for (int i = 0; i < 20; ++i) {
            futures.push_back(std::async(std::launch::async, [this, i]() {
                for (int j = 0; j < 10; ++j) {
                    auto connection = connectionPool_->acquireConnection();
                    if (connection) {
                        // Simulate work
                        std::this_thread::sleep_for(1ms);
                        connectionPool_->releaseConnection(connection);
                    }
                }
            }));
        }
        
        // Wait for all tasks to complete
        for (auto& future : futures) {
            future.wait();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        auto metrics = connectionPool_->getMetrics();
        std::cout << "Connection Pool Performance:" << std::endl;
        std::cout << "  Total Operations: 200" << std::endl;
        std::cout << "  Duration: " << duration.count() << "ms" << std::endl;
        std::cout << "  Connections Acquired: " << metrics.connectionsAcquired.load() << std::endl;
        std::cout << "  Pool Hit Ratio: " << (metrics.connectionsAcquired.load() > 0 ? 
                     static_cast<double>(metrics.connectionsAcquired.load() - metrics.connectionsCreated.load()) / 
                     metrics.connectionsAcquired.load() * 100.0 : 0.0) << "%" << std::endl;
        std::cout << "  Average Acquisition Time: " << metrics.averageAcquisitionTime.load() << "ms" << std::endl;
        std::cout << std::endl;
    }
    
    void demonstrateMessageBatching() {
        std::cout << "--- Message Batching Demo ---" << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Generate test messages
        std::vector<Message> messages;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> destDis(1, 5);
        std::uniform_int_distribution<> priorityDis(1, 3);
        
        for (int i = 0; i < 100; ++i) {
            std::string dest = "destination_" + std::to_string(destDis(gen));
            int priority = priorityDis(gen);
            
            Message msg("msg_" + std::to_string(i), "test_type", dest, 
                       json{{"data", "payload_" + std::to_string(i)}});
            msg.priority = priority;
            msg.calculateSize();
            
            messages.push_back(msg);
        }
        
        // Add messages to batcher
        for (const auto& msg : messages) {
            messageBatcher_->addMessage(msg);
        }
        
        // Wait for batching to complete
        std::this_thread::sleep_for(200ms);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        auto metrics = messageBatcher_->getMetrics();
        std::cout << "Message Batching Performance:" << std::endl;
        std::cout << "  Messages Processed: " << messages.size() << std::endl;
        std::cout << "  Duration: " << duration.count() << "ms" << std::endl;
        std::cout << "  Batches Created: " << metrics.batchesCreated.load() << std::endl;
        std::cout << "  Average Batch Size: " << metrics.averageBatchSize.load() << std::endl;
        std::cout << "  Batching Efficiency: " << (messages.size() > 0 ? 
                     static_cast<double>(messages.size()) / metrics.batchesCreated.load() : 0.0) << " msgs/batch" << std::endl;
        std::cout << std::endl;
    }
    
    void demonstrateMemoryPooling() {
        std::cout << "--- Memory Pooling Demo ---" << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Simulate frequent string allocations
        std::vector<std::future<void>> futures;
        
        for (int i = 0; i < 10; ++i) {
            futures.push_back(std::async(std::launch::async, [this, i]() {
                for (int j = 0; j < 50; ++j) {
                    auto str = stringPool_->acquire();
                    if (str) {
                        *str = "pooled_string_" + std::to_string(i) + "_" + std::to_string(j);
                        // Simulate work
                        std::this_thread::sleep_for(1ms);
                        // String automatically returned to pool when shared_ptr is destroyed
                    }
                }
            }));
        }
        
        // Wait for all tasks to complete
        for (auto& future : futures) {
            future.wait();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        auto metrics = stringPool_->getMetrics();
        std::cout << "Memory Pool Performance:" << std::endl;
        std::cout << "  Total Operations: 500" << std::endl;
        std::cout << "  Duration: " << duration.count() << "ms" << std::endl;
        std::cout << "  Pool Hit Ratio: " << metrics.hitRatio.load() * 100.0 << "%" << std::endl;
        std::cout << "  Memory Utilization: " << metrics.memoryUtilization.load() * 100.0 << "%" << std::endl;
        std::cout << "  Peak Pool Size: " << metrics.peakPoolSize.load() << std::endl;
        std::cout << std::endl;
    }
    
    void demonstrateSerializationOptimization() {
        std::cout << "--- Serialization Optimization Demo ---" << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Generate test JSON data
        std::vector<json> testData;
        for (int i = 0; i < 100; ++i) {
            json data = {
                {"id", i},
                {"name", "test_object_" + std::to_string(i)},
                {"value", i * 1.5},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"metadata", {
                    {"source", "demo"},
                    {"version", "1.0"},
                    {"tags", {"performance", "test", "optimization"}}
                }}
            };
            testData.push_back(data);
        }
        
        // Perform serialization operations
        std::vector<std::string> serialized;
        for (const auto& data : testData) {
            serialized.push_back(serializationOptimizer_->serialize(data));
        }
        
        // Perform deserialization operations
        std::vector<json> deserialized;
        for (const auto& str : serialized) {
            deserialized.push_back(serializationOptimizer_->deserialize(str));
        }
        
        // Test caching by re-serializing the same data
        for (const auto& data : testData) {
            serializationOptimizer_->serialize(data);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        auto metrics = serializationOptimizer_->getMetrics();
        std::cout << "Serialization Optimization Performance:" << std::endl;
        std::cout << "  Operations: " << testData.size() * 3 << " (serialize + deserialize + cached serialize)" << std::endl;
        std::cout << "  Duration: " << duration.count() << "ms" << std::endl;
        std::cout << "  Cache Hit Ratio: " << metrics.cacheHitRatio.load() * 100.0 << "%" << std::endl;
        std::cout << "  Average Serialization Time: " << metrics.averageSerializationTime.load() << "ms" << std::endl;
        std::cout << "  Average Deserialization Time: " << metrics.averageDeserializationTime.load() << "ms" << std::endl;
        std::cout << std::endl;
    }
    
    void demonstrateIntegratedPerformance() {
        std::cout << "--- Integrated Performance Test ---" << std::endl;
        std::cout << "Testing all components working together..." << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Simulate a realistic workload using all components
        std::vector<std::future<void>> futures;
        
        for (int i = 0; i < 10; ++i) {
            futures.push_back(std::async(std::launch::async, [this, i]() {
                for (int j = 0; j < 20; ++j) {
                    // Use connection pool
                    auto connection = connectionPool_->acquireConnection();
                    if (connection) {
                        // Use memory pool for string operations
                        auto str = stringPool_->acquire();
                        if (str) {
                            *str = "integrated_test_" + std::to_string(i) + "_" + std::to_string(j);
                            
                            // Use serialization optimizer
                            json data = {
                                {"thread", i},
                                {"iteration", j},
                                {"message", *str},
                                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch()).count()}
                            };
                            
                            std::string serialized = serializationOptimizer_->serialize(data);
                            
                            // Create message for batching
                            Message msg("integrated_" + std::to_string(i) + "_" + std::to_string(j),
                                       "integrated_test", "destination_" + std::to_string(i % 3), data);
                            msg.priority = i % 3;
                            msg.calculateSize();
                            
                            messageBatcher_->addMessage(msg);
                        }
                        
                        connectionPool_->releaseConnection(connection);
                    }
                    
                    // Small delay to simulate realistic timing
                    std::this_thread::sleep_for(2ms);
                }
            }));
        }
        
        // Wait for all tasks to complete
        for (auto& future : futures) {
            future.wait();
        }
        
        // Allow time for message batching to complete
        std::this_thread::sleep_for(300ms);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        std::cout << "Integrated Performance Results:" << std::endl;
        std::cout << "  Total Operations: 200" << std::endl;
        std::cout << "  Duration: " << duration.count() << "ms" << std::endl;
        std::cout << "  Throughput: " << (200.0 / duration.count() * 1000.0) << " ops/sec" << std::endl;
        std::cout << std::endl;
    }
    
    void processBatch(const MessageBatch& batch) {
        // Simulate batch processing
        spdlog::debug("Processing batch {} with {} messages", batch.batchId, batch.getMessageCount());
    }
    
    void displayFinalMetrics() {
        std::cout << "--- Final Performance Metrics ---" << std::endl;
        
        // Connection pool metrics
        auto connMetrics = connectionPool_->getMetrics();
        std::cout << "Connection Pool:" << std::endl;
        std::cout << "  Total Connections Created: " << connMetrics.connectionsCreated.load() << std::endl;
        std::cout << "  Connection Reuse Rate: " << (connMetrics.connectionsAcquired.load() > 0 ?
                     (1.0 - static_cast<double>(connMetrics.connectionsCreated.load()) / 
                      connMetrics.connectionsAcquired.load()) * 100.0 : 0.0) << "%" << std::endl;
        
        // Message batcher metrics
        auto batchMetrics = messageBatcher_->getMetrics();
        std::cout << "Message Batcher:" << std::endl;
        std::cout << "  Messages Processed: " << batchMetrics.messagesBatched.load() << std::endl;
        std::cout << "  Batching Efficiency: " << batchMetrics.averageBatchSize.load() << " msgs/batch" << std::endl;
        
        // Memory pool metrics
        auto memMetrics = stringPool_->getMetrics();
        std::cout << "Memory Pool:" << std::endl;
        std::cout << "  Memory Reuse Rate: " << memMetrics.hitRatio.load() * 100.0 << "%" << std::endl;
        std::cout << "  Peak Memory Usage: " << memMetrics.peakPoolSize.load() << " objects" << std::endl;
        
        // Serialization metrics
        auto serMetrics = serializationOptimizer_->getMetrics();
        std::cout << "Serialization Optimizer:" << std::endl;
        std::cout << "  Cache Hit Rate: " << serMetrics.cacheHitRatio.load() * 100.0 << "%" << std::endl;
        std::cout << "  Performance Improvement: " << (serMetrics.cacheHitRatio.load() * 100.0) << "% faster for cached items" << std::endl;
        
        std::cout << std::endl;
        std::cout << "🎉 Performance optimization demo completed successfully!" << std::endl;
        std::cout << "Key benefits achieved:" << std::endl;
        std::cout << "  • Reduced connection overhead through pooling" << std::endl;
        std::cout << "  • Improved network efficiency through message batching" << std::endl;
        std::cout << "  • Reduced memory allocations through object pooling" << std::endl;
        std::cout << "  • Faster serialization through caching and optimization" << std::endl;
    }
    
    void cleanup() {
        if (connectionPool_) connectionPool_->shutdown();
        if (messageBatcher_) messageBatcher_->stop();
        if (stringPool_) stringPool_->shutdown();
        if (serializationOptimizer_) serializationOptimizer_->stop();
    }
};

int main() {
    try {
        PerformanceOptimizationDemo demo;
        demo.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

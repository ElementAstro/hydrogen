#include "comprehensive_test_framework.h"
#include "mock_objects.h"
#include <hydrogen/core/message_transformer.h>
#include <hydrogen/core/protocol_converters.h>
#include <gtest/gtest.h>

using namespace hydrogen::testing;
using namespace hydrogen::core;

/**
 * @brief Performance testing fixture for core components
 */
HYDROGEN_TEST_FIXTURE(PerformanceBenchmarkTest) {
protected:
    void SetUp() override {
        ComprehensiveTestFixture::SetUp();
        
        // Enable performance testing
        getConfig().enablePerformanceTesting = true;
        getConfig().maxResponseTime = std::chrono::milliseconds{100};
        getConfig().stressTestIterations = 10000;
        
        // Initialize test data
        setupTestData();
    }
    
    void setupTestData() {
        // Create various message sizes for testing
        smallMessage_ = generateTestData();
        
        mediumMessage_ = json::object();
        mediumMessage_["type"] = "device_data";
        mediumMessage_["deviceId"] = "perf_test_device";
        mediumMessage_["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        mediumMessage_["data"] = json::array();
        for (int i = 0; i < 100; ++i) {
            json dataPoint;
            dataPoint["sensor_" + std::to_string(i)] = i * 1.5;
            mediumMessage_["data"].push_back(dataPoint);
        }
        
        largeMessage_ = json::object();
        largeMessage_["type"] = "bulk_data";
        largeMessage_["deviceId"] = "perf_test_device";
        largeMessage_["data"] = json::array();
        for (int i = 0; i < 10000; ++i) {
            json record;
            record["id"] = i;
            record["value"] = generateRandomString(50);
            record["metadata"] = generateTestData();
            largeMessage_["data"].push_back(record);
        }
    }

protected:
    json smallMessage_;
    json mediumMessage_;
    json largeMessage_;
};

// Message transformation performance tests
HYDROGEN_PERFORMANCE_TEST(PerformanceBenchmarkTest, MessageTransformationSpeed) {
    auto& transformer = getGlobalMessageTransformer();
    
    // Test small message transformation
    benchmarkOperation([&]() {
        auto message = std::make_unique<CommandMessage>("test_command");
        message->setParameters(smallMessage_);
        auto result = transformer.transform(*message, MessageFormat::HTTP_JSON);
        EXPECT_TRUE(result.success);
    }, 1000, "small_message_transform");
    
    // Test medium message transformation
    benchmarkOperation([&]() {
        auto message = std::make_unique<DataMessage>("sensor_data");
        message->setData(mediumMessage_);
        auto result = transformer.transform(*message, MessageFormat::HTTP_JSON);
        EXPECT_TRUE(result.success);
    }, 500, "medium_message_transform");
    
    // Test large message transformation
    benchmarkOperation([&]() {
        auto message = std::make_unique<DataMessage>("bulk_data");
        message->setData(largeMessage_);
        auto result = transformer.transform(*message, MessageFormat::HTTP_JSON);
        EXPECT_TRUE(result.success);
    }, 100, "large_message_transform");
}

// Protocol conversion performance
HYDROGEN_PERFORMANCE_TEST(PerformanceBenchmarkTest, ProtocolConversionPerformance) {
    auto& registry = ConverterRegistry::getInstance();
    
    // Benchmark JSON to different formats
    std::vector<std::pair<std::string, std::function<void()>>> conversions = {
        {"json_to_protobuf", [&]() {
            auto converter = registry.getConverter(MessageFormat::PROTOBUF);
            if (converter) {
                converter->convertFromJson(mediumMessage_);
            }
        }},
        {"json_to_msgpack", [&]() {
            // Simulate MessagePack conversion
            std::string serialized = mediumMessage_.dump();
            json parsed = json::parse(serialized);
        }},
        {"json_serialization", [&]() {
            std::string serialized = mediumMessage_.dump();
            json parsed = json::parse(serialized);
        }}
    };
    
    PerformanceTester::comparePerformance(conversions, 1000);
}

// Memory allocation performance
HYDROGEN_PERFORMANCE_TEST(PerformanceBenchmarkTest, MemoryAllocationPerformance) {
    size_t initialMemory = PerformanceTester::getCurrentMemoryUsage();
    
    // Test memory usage during message creation
    size_t memoryUsed = PerformanceTester::measureMemoryUsage([&]() {
        std::vector<std::unique_ptr<Message>> messages;
        messages.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            auto message = std::make_unique<DataMessage>("test_data_" + std::to_string(i));
            message->setData(smallMessage_);
            messages.push_back(std::move(message));
        }
        
        // Messages will be destroyed when vector goes out of scope
    });
    
    logTestInfo("Memory used for 1000 messages: " + std::to_string(memoryUsed) + " bytes");
    
    // Verify memory usage is reasonable (less than 10MB for 1000 small messages)
    EXPECT_LT(memoryUsed, 10 * 1024 * 1024);
}

// Concurrent message processing performance
TEST_F(PerformanceBenchmarkTest, ConcurrentMessageProcessing) {
    if (!getConfig().enableConcurrencyTesting) {
        GTEST_SKIP() << "Concurrency testing disabled";
    }
    
    std::atomic<size_t> messagesProcessed{0};
    std::atomic<size_t> processingErrors{0};
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    runConcurrentTest([&](int threadId) {
        auto& transformer = getGlobalMessageTransformer();
        
        for (int i = 0; i < 100; ++i) {
            try {
                auto message = std::make_unique<DataMessage>("thread_" + std::to_string(threadId) + "_msg_" + std::to_string(i));
                message->setData(smallMessage_);
                
                auto result = transformer.transform(*message, MessageFormat::HTTP_JSON);
                if (result.success) {
                    messagesProcessed.fetch_add(1);
                } else {
                    processingErrors.fetch_add(1);
                }
            } catch (const std::exception& e) {
                processingErrors.fetch_add(1);
            }
        }
    }, 8); // 8 threads
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double messagesPerSecond = static_cast<double>(messagesProcessed.load()) / (duration.count() / 1000.0);
    
    logTestInfo("Concurrent processing: " + std::to_string(messagesProcessed.load()) + 
                " messages in " + std::to_string(duration.count()) + "ms");
    logTestInfo("Throughput: " + std::to_string(messagesPerSecond) + " messages/second");
    logTestInfo("Error rate: " + std::to_string(processingErrors.load()) + " errors");
    
    // Verify performance meets requirements
    EXPECT_GT(messagesPerSecond, 1000.0) << "Should process at least 1000 messages per second";
    EXPECT_LT(processingErrors.load(), messagesProcessed.load() / 100) << "Error rate should be less than 1%";
}

// Stress test for sustained load
HYDROGEN_STRESS_TEST(PerformanceBenchmarkTest, SustainedLoadTest) {
    auto& transformer = getGlobalMessageTransformer();
    std::atomic<size_t> successCount{0};
    std::atomic<size_t> errorCount{0};
    
    // Each iteration processes a message
    auto testFunction = [&](int iteration) {
        try {
            auto message = std::make_unique<DataMessage>("stress_test_" + std::to_string(iteration));
            message->setData(smallMessage_);
            
            auto result = transformer.transform(*message, MessageFormat::HTTP_JSON);
            if (result.success) {
                successCount.fetch_add(1);
            } else {
                errorCount.fetch_add(1);
            }
        } catch (const std::exception& e) {
            errorCount.fetch_add(1);
        }
    };
    
    // Run the stress test (framework handles the iteration)
    runStressTest(testFunction, getConfig().stressTestIterations);
    
    // Verify results
    size_t totalOperations = successCount.load() + errorCount.load();
    double successRate = static_cast<double>(successCount.load()) / totalOperations * 100.0;
    
    EXPECT_GT(successRate, 95.0) << "Success rate should be above 95%";
    
    logTestInfo("Stress test completed: " + std::to_string(successRate) + "% success rate");
}

// Latency measurement test
HYDROGEN_PERFORMANCE_TEST(PerformanceBenchmarkTest, LatencyMeasurement) {
    auto& transformer = getGlobalMessageTransformer();
    
    std::vector<std::chrono::microseconds> latencies;
    latencies.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        auto message = std::make_unique<CommandMessage>("latency_test");
        message->setParameters(smallMessage_);
        auto result = transformer.transform(*message, MessageFormat::HTTP_JSON);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        latencies.push_back(latency);
    }
    
    // Calculate statistics
    auto minLatency = *std::min_element(latencies.begin(), latencies.end());
    auto maxLatency = *std::max_element(latencies.begin(), latencies.end());
    
    auto totalLatency = std::accumulate(latencies.begin(), latencies.end(), std::chrono::microseconds{0});
    auto avgLatency = totalLatency / latencies.size();
    
    // Calculate 95th percentile
    std::sort(latencies.begin(), latencies.end());
    auto p95Index = static_cast<size_t>(latencies.size() * 0.95);
    auto p95Latency = latencies[p95Index];
    
    logTestInfo("Latency statistics:");
    logTestInfo("  Min: " + std::to_string(minLatency.count()) + "μs");
    logTestInfo("  Avg: " + std::to_string(avgLatency.count()) + "μs");
    logTestInfo("  Max: " + std::to_string(maxLatency.count()) + "μs");
    logTestInfo("  P95: " + std::to_string(p95Latency.count()) + "μs");
    
    // Verify latency requirements
    EXPECT_LT(avgLatency.count(), 1000) << "Average latency should be less than 1ms";
    EXPECT_LT(p95Latency.count(), 5000) << "95th percentile latency should be less than 5ms";
}

// Resource utilization test
TEST_F(PerformanceBenchmarkTest, ResourceUtilization) {
    size_t initialMemory = PerformanceTester::getCurrentMemoryUsage();
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Simulate sustained workload
    for (int i = 0; i < 10000; ++i) {
        auto message = std::make_unique<DataMessage>("resource_test_" + std::to_string(i));
        message->setData(smallMessage_);
        
        auto& transformer = getGlobalMessageTransformer();
        auto result = transformer.transform(*message, MessageFormat::HTTP_JSON);
        
        // Occasionally check memory usage
        if (i % 1000 == 0) {
            size_t currentMemory = PerformanceTester::getCurrentMemoryUsage();
            size_t memoryGrowth = currentMemory > initialMemory ? currentMemory - initialMemory : 0;
            
            // Memory growth should be reasonable
            EXPECT_LT(memoryGrowth, 50 * 1024 * 1024) << "Memory growth should be less than 50MB";
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    size_t finalMemory = PerformanceTester::getCurrentMemoryUsage();
    
    logTestInfo("Resource utilization test completed in " + std::to_string(duration.count()) + "ms");
    logTestInfo("Memory growth: " + std::to_string(finalMemory - initialMemory) + " bytes");
}

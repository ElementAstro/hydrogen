#include <gtest/gtest.h>
#include <hydrogen/device/performance_monitor.h>
#include <thread>
#include <chrono>
#include <vector>

using namespace hydrogen::device;

class PerformanceMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor = std::make_unique<PerformanceMonitor>("test_device");
    }

    std::unique_ptr<PerformanceMonitor> monitor;
};

TEST_F(PerformanceMonitorTest, BasicFunctionality) {
    EXPECT_TRUE(monitor->isEnabled());
    
    // Test enabling/disabling
    monitor->setEnabled(false);
    EXPECT_FALSE(monitor->isEnabled());
    
    monitor->setEnabled(true);
    EXPECT_TRUE(monitor->isEnabled());
}

TEST_F(PerformanceMonitorTest, TimingOperations) {
    monitor->startTiming("test_operation");
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    double duration = monitor->endTiming("test_operation");
    
    EXPECT_GT(duration, 8.0);  // Should be at least 8ms
    EXPECT_LT(duration, 50.0); // Should be less than 50ms (allowing for system variance)
}

TEST_F(PerformanceMonitorTest, ScopedTimer) {
    {
        DEVICE_PERF_TIMER(*monitor, "scoped_operation");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } // Timer should automatically end here
    
    auto metrics = monitor->getMetrics();
    EXPECT_GT(metrics.updateCount, 0);
    EXPECT_GT(metrics.averageUpdateTime, 0.0);
}

TEST_F(PerformanceMonitorTest, CustomMetrics) {
    monitor->recordMetric("temperature", 25.5);
    monitor->recordMetric("voltage", 12.0);
    
    auto metrics = monitor->getMetrics();
    
    EXPECT_EQ(metrics.customMetrics["temperature"], 25.5);
    EXPECT_EQ(metrics.customMetrics["voltage"], 12.0);
}

TEST_F(PerformanceMonitorTest, CounterMetrics) {
    monitor->incrementCounter("requests");
    monitor->incrementCounter("requests");
    monitor->incrementCounter("requests", 5);
    
    auto metrics = monitor->getMetrics();
    
    EXPECT_EQ(metrics.customMetrics["requests"], 7.0); // 1 + 1 + 5
}

TEST_F(PerformanceMonitorTest, MemoryTracking) {
    monitor->recordMemoryUsage(1024);
    monitor->recordMemoryUsage(2048);
    monitor->recordMemoryUsage(1536);
    
    auto metrics = monitor->getMetrics();
    
    EXPECT_EQ(metrics.memoryUsage, 1536);      // Current usage
    EXPECT_EQ(metrics.peakMemoryUsage, 2048);  // Peak usage
}

TEST_F(PerformanceMonitorTest, MessageTracking) {
    monitor->recordMessage(100, true);  // Sent
    monitor->recordMessage(200, true);  // Sent
    monitor->recordMessage(150, false); // Received
    
    auto metrics = monitor->getMetrics();
    
    EXPECT_EQ(metrics.messagesSent, 2);
    EXPECT_EQ(metrics.messagesReceived, 1);
    EXPECT_EQ(metrics.bytesTransmitted, 300);  // 100 + 200
    EXPECT_EQ(metrics.bytesReceived, 150);
}

TEST_F(PerformanceMonitorTest, ErrorTracking) {
    monitor->recordError("connection_error");
    monitor->recordError("timeout_error");
    monitor->recordError("connection_error");
    
    auto metrics = monitor->getMetrics();
    
    EXPECT_EQ(metrics.errorCount, 3);
}

TEST_F(PerformanceMonitorTest, PerformanceSummary) {
    monitor->recordMetric("custom_value", 42.0);
    monitor->recordMemoryUsage(1024);
    monitor->recordMessage(100, true);
    monitor->recordError("test_error");
    
    auto summary = monitor->getPerformanceSummary();
    
    EXPECT_TRUE(summary.contains("device_id"));
    EXPECT_TRUE(summary.contains("enabled"));
    EXPECT_TRUE(summary.contains("timing"));
    EXPECT_TRUE(summary.contains("memory"));
    EXPECT_TRUE(summary.contains("communication"));
    EXPECT_TRUE(summary.contains("errors"));
    EXPECT_TRUE(summary.contains("custom_metrics"));
    
    EXPECT_EQ(summary["device_id"], "test_device");
    EXPECT_TRUE(summary["enabled"]);
    EXPECT_EQ(summary["custom_metrics"]["custom_value"], 42.0);
    EXPECT_EQ(summary["memory"]["current_usage_bytes"], 1024);
    EXPECT_EQ(summary["communication"]["messages_sent"], 1);
    EXPECT_EQ(summary["errors"]["total_errors"], 1);
}

TEST_F(PerformanceMonitorTest, Reset) {
    monitor->recordMetric("test_metric", 100.0);
    monitor->recordMemoryUsage(2048);
    monitor->recordMessage(500, true);
    monitor->recordError("test_error");
    
    auto metrics = monitor->getMetrics();
    EXPECT_GT(metrics.customMetrics.size(), 0);
    EXPECT_GT(metrics.memoryUsage, 0);
    EXPECT_GT(metrics.messagesSent, 0);
    EXPECT_GT(metrics.errorCount, 0);
    
    monitor->reset();
    
    metrics = monitor->getMetrics();
    EXPECT_EQ(metrics.customMetrics.size(), 0);
    EXPECT_EQ(metrics.memoryUsage, 0);
    EXPECT_EQ(metrics.messagesSent, 0);
    EXPECT_EQ(metrics.errorCount, 0);
}

TEST_F(PerformanceMonitorTest, DisabledMonitoring) {
    monitor->setEnabled(false);
    
    monitor->recordMetric("disabled_metric", 50.0);
    monitor->recordMemoryUsage(1024);
    monitor->recordMessage(100, true);
    monitor->recordError("disabled_error");
    
    auto metrics = monitor->getMetrics();
    
    // When disabled, metrics should not be recorded
    EXPECT_EQ(metrics.customMetrics.size(), 0);
    EXPECT_EQ(metrics.memoryUsage, 0);
    EXPECT_EQ(metrics.messagesSent, 0);
    EXPECT_EQ(metrics.errorCount, 0);
}

TEST_F(PerformanceMonitorTest, ThreadSafety) {
    const int numThreads = 10;
    const int operationsPerThread = 100;
    std::vector<std::thread> threads;
    
    // Create multiple threads that perform operations concurrently
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, operationsPerThread]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                monitor->recordMetric("thread_" + std::to_string(t), i);
                monitor->incrementCounter("thread_counter");
                monitor->recordMemoryUsage(1024 + i);
                monitor->recordMessage(100 + i, i % 2 == 0);
                
                if (i % 10 == 0) {
                    monitor->recordError("thread_error_" + std::to_string(t));
                }
                
                // Test timing operations
                monitor->startTiming("thread_op_" + std::to_string(t));
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                monitor->endTiming("thread_op_" + std::to_string(t));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto metrics = monitor->getMetrics();
    
    // Check that all operations were recorded
    EXPECT_EQ(metrics.customMetrics["thread_counter"], numThreads * operationsPerThread);
    EXPECT_GT(metrics.updateCount, 0); // Timing operations should have been recorded
    EXPECT_GT(metrics.messagesSent + metrics.messagesReceived, 0);
    EXPECT_GT(metrics.errorCount, 0);
}

// Object pool tests
class ObjectPoolTest : public ::testing::Test {
protected:
    struct TestObject {
        int value = 0;
        TestObject() = default;
        explicit TestObject(int v) : value(v) {}
    };
    
    void SetUp() override {
        pool = std::make_unique<ObjectPool<TestObject, 5>>();
    }
    
    std::unique_ptr<ObjectPool<TestObject, 5>> pool;
};

TEST_F(ObjectPoolTest, BasicAcquireRelease) {
    EXPECT_EQ(pool->size(), 5); // Pool should be full initially
    
    auto obj = pool->acquire();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool->size(), 4); // One object taken
    
    pool->release(std::move(obj));
    EXPECT_EQ(pool->size(), 5); // Object returned
}

TEST_F(ObjectPoolTest, PoolExhaustion) {
    std::vector<std::unique_ptr<TestObject>> objects;
    
    // Acquire all objects from pool
    for (int i = 0; i < 5; ++i) {
        objects.push_back(pool->acquire());
    }
    
    EXPECT_EQ(pool->size(), 0); // Pool should be empty
    
    // Acquiring another object should create a new one
    auto extraObj = pool->acquire();
    EXPECT_NE(extraObj, nullptr);
    EXPECT_EQ(pool->size(), 0); // Pool still empty
    
    // Return objects to pool
    for (auto& obj : objects) {
        pool->release(std::move(obj));
    }
    
    EXPECT_EQ(pool->size(), 5); // Pool full again
    
    // Extra object won't fit in pool when returned
    pool->release(std::move(extraObj));
    EXPECT_EQ(pool->size(), 5); // Pool size unchanged (object was destroyed)
}

TEST_F(ObjectPoolTest, ThreadSafety) {
    const int numThreads = 10;
    const int operationsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> totalAcquisitions{0};
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &totalAcquisitions, operationsPerThread]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                auto obj = pool->acquire();
                totalAcquisitions++;
                
                // Do some work with the object
                obj->value = i;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                
                pool->release(std::move(obj));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(totalAcquisitions.load(), numThreads * operationsPerThread);
    // Pool should have some objects available (exact number depends on timing)
    EXPECT_GT(pool->size(), 0);
    EXPECT_LE(pool->size(), 5);
}

// Performance tests
class PerformanceMonitorPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor = std::make_unique<PerformanceMonitor>("perf_test_device");
    }
    
    std::unique_ptr<PerformanceMonitor> monitor;
};

TEST_F(PerformanceMonitorPerformanceTest, MetricRecordingThroughput) {
    const int numOperations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numOperations; ++i) {
        monitor->recordMetric("test_metric", i);
        monitor->incrementCounter("test_counter");
        monitor->recordMemoryUsage(1024 + i);
        monitor->recordMessage(100, i % 2 == 0);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double operationsPerSecond = (numOperations * 4 * 1000000.0) / duration.count();
    
    std::cout << "Performance monitoring throughput: " << operationsPerSecond << " operations/second" << std::endl;
    
    EXPECT_GT(operationsPerSecond, 1000000.0); // Should handle at least 1M operations/second
}

TEST_F(PerformanceMonitorPerformanceTest, DisabledPerformance) {
    monitor->setEnabled(false);
    
    const int numOperations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numOperations; ++i) {
        monitor->recordMetric("test_metric", i);
        monitor->incrementCounter("test_counter");
        monitor->recordMemoryUsage(1024 + i);
        monitor->recordMessage(100, i % 2 == 0);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double operationsPerSecond = (numOperations * 4 * 1000000.0) / duration.count();
    
    std::cout << "Disabled monitoring throughput: " << operationsPerSecond << " operations/second" << std::endl;
    
    // Should be much faster when disabled
    EXPECT_GT(operationsPerSecond, 5000000.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

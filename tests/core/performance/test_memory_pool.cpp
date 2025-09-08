#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <vector>
#include <future>

#include "hydrogen/core/performance/memory_pool.h"

using namespace hydrogen::core::performance;
using namespace std::chrono_literals;

class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.initialPoolSize = 5;
        config_.maxPoolSize = 20;
        config_.growthIncrement = 5;
        config_.growthThreshold = 0.8;
        config_.shrinkThreshold = 0.3;
        config_.cleanupInterval = 100ms;
        config_.objectTimeout = 1000ms;
        config_.enableMetrics = true;
        config_.enableAutoCleanup = false; // Disable for predictable testing
        
        pool_ = std::make_shared<MemoryPool<std::string>>(config_);
    }
    
    void TearDown() override {
        if (pool_ && pool_->isRunning()) {
            pool_->shutdown();
        }
        pool_.reset();
    }
    
    MemoryPoolConfig config_;
    std::shared_ptr<MemoryPool<std::string>> pool_;
};

TEST_F(MemoryPoolTest, InitializationAndShutdown) {
    // Test initial state
    EXPECT_FALSE(pool_->isRunning());
    EXPECT_EQ(pool_->getPoolSize(), 0);
    
    // Test initialization
    EXPECT_TRUE(pool_->initialize());
    EXPECT_TRUE(pool_->isRunning());
    EXPECT_EQ(pool_->getPoolSize(), config_.initialPoolSize);
    
    // Test double initialization
    EXPECT_TRUE(pool_->initialize()); // Should succeed (already initialized)
    
    // Test shutdown
    pool_->shutdown();
    EXPECT_FALSE(pool_->isRunning());
    EXPECT_EQ(pool_->getPoolSize(), 0);
}

TEST_F(MemoryPoolTest, BasicObjectAcquisitionAndRelease) {
    ASSERT_TRUE(pool_->initialize());
    
    // Acquire object
    auto obj = pool_->acquire();
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(pool_->getActiveObjectCount(), 1);
    
    // Use the object
    *obj = "test_string";
    EXPECT_EQ(*obj, "test_string");
    
    // Release object (automatic when shared_ptr goes out of scope)
    obj.reset();
    
    // Check pool state after release
    EXPECT_EQ(pool_->getActiveObjectCount(), 0);
}

TEST_F(MemoryPoolTest, MultipleObjectAcquisition) {
    ASSERT_TRUE(pool_->initialize());
    
    std::vector<std::shared_ptr<std::string>> objects;
    
    // Acquire multiple objects
    for (size_t i = 0; i < config_.initialPoolSize; ++i) {
        auto obj = pool_->acquire();
        ASSERT_NE(obj, nullptr);
        *obj = "string_" + std::to_string(i);
        objects.push_back(obj);
    }
    
    EXPECT_EQ(pool_->getActiveObjectCount(), config_.initialPoolSize);
    
    // Verify object contents
    for (size_t i = 0; i < objects.size(); ++i) {
        EXPECT_EQ(*objects[i], "string_" + std::to_string(i));
    }
    
    // Release all objects
    objects.clear();
    
    EXPECT_EQ(pool_->getActiveObjectCount(), 0);
}

TEST_F(MemoryPoolTest, PoolExpansion) {
    ASSERT_TRUE(pool_->initialize());
    
    std::vector<std::shared_ptr<std::string>> objects;
    
    // Acquire more objects than initial pool size
    size_t objectsToAcquire = config_.initialPoolSize + 3;
    for (size_t i = 0; i < objectsToAcquire; ++i) {
        auto obj = pool_->acquire();
        ASSERT_NE(obj, nullptr);
        objects.push_back(obj);
    }
    
    // Pool should have expanded
    EXPECT_EQ(pool_->getActiveObjectCount(), objectsToAcquire);
    
    // Release all objects
    objects.clear();
}

TEST_F(MemoryPoolTest, MaxPoolSizeLimit) {
    ASSERT_TRUE(pool_->initialize());
    
    std::vector<std::shared_ptr<std::string>> objects;
    
    // Try to acquire more than max pool size
    for (size_t i = 0; i < config_.maxPoolSize + 5; ++i) {
        auto obj = pool_->acquire();
        if (obj) {
            objects.push_back(obj);
        }
    }
    
    // Should not exceed max pool size
    EXPECT_LE(pool_->getActiveObjectCount(), config_.maxPoolSize);
    EXPECT_LE(objects.size(), config_.maxPoolSize);
    
    // Release all objects
    objects.clear();
}

TEST_F(MemoryPoolTest, ObjectReuse) {
    ASSERT_TRUE(pool_->initialize());
    
    // Acquire and release an object multiple times
    std::string* firstPtr = nullptr;
    
    for (int i = 0; i < 5; ++i) {
        auto obj = pool_->acquire();
        ASSERT_NE(obj, nullptr);
        
        if (i == 0) {
            firstPtr = obj.get();
        }
        
        *obj = "reuse_test_" + std::to_string(i);
        
        // Release object
        obj.reset();
    }
    
    // Should have reused objects from pool
    EXPECT_EQ(pool_->getActiveObjectCount(), 0);
    
    // Check metrics for reuse
    auto metrics = pool_->getMetrics();
    EXPECT_GT(metrics.poolHits.load(), 0);
}

TEST_F(MemoryPoolTest, ConcurrentAccess) {
    ASSERT_TRUE(pool_->initialize());
    
    const int numThreads = 10;
    const int operationsPerThread = 50;
    std::vector<std::future<int>> futures;
    
    // Launch concurrent threads
    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [this, operationsPerThread]() {
            int successfulOperations = 0;
            
            for (int j = 0; j < operationsPerThread; ++j) {
                auto obj = pool_->acquire();
                if (obj) {
                    *obj = "concurrent_test";
                    // Simulate some work
                    std::this_thread::sleep_for(1ms);
                    successfulOperations++;
                    // Object automatically released when going out of scope
                }
            }
            
            return successfulOperations;
        }));
    }
    
    // Wait for all threads to complete
    int totalSuccessfulOperations = 0;
    for (auto& future : futures) {
        totalSuccessfulOperations += future.get();
    }
    
    // Should have completed most operations successfully
    EXPECT_GT(totalSuccessfulOperations, numThreads * operationsPerThread * 0.8);
    
    // Pool should be back to initial state
    EXPECT_EQ(pool_->getActiveObjectCount(), 0);
}

TEST_F(MemoryPoolTest, Metrics) {
    ASSERT_TRUE(pool_->initialize());
    
    // Perform some operations
    auto obj1 = pool_->acquire();
    auto obj2 = pool_->acquire();
    
    obj1.reset();
    obj2.reset();
    
    // Check metrics
    auto metrics = pool_->getMetrics();
    
    EXPECT_GE(metrics.totalAllocations.load(), 2);
    EXPECT_GE(metrics.totalDeallocations.load(), 2);
    EXPECT_EQ(metrics.currentActiveObjects.load(), 0);
    EXPECT_GT(metrics.hitRatio.load(), 0.0);
    
    // Check detailed metrics
    auto detailedMetrics = pool_->getDetailedMetrics();
    EXPECT_TRUE(detailedMetrics.contains("poolDetails"));
    EXPECT_TRUE(detailedMetrics["poolDetails"].contains("objectType"));
    EXPECT_TRUE(detailedMetrics["poolDetails"].contains("objectSize"));
}

TEST_F(MemoryPoolTest, PooledResource) {
    ASSERT_TRUE(pool_->initialize());
    
    // Test RAII wrapper
    {
        PooledResource<std::string> resource(pool_);
        EXPECT_TRUE(resource.isValid());
        
        *resource = "raii_test";
        EXPECT_EQ(*resource, "raii_test");
        
        EXPECT_EQ(pool_->getActiveObjectCount(), 1);
    } // Resource should be automatically released here
    
    EXPECT_EQ(pool_->getActiveObjectCount(), 0);
}

TEST_F(MemoryPoolTest, MemoryPoolFactory) {
    // Test factory creation
    auto factoryPool = MemoryPoolFactory::createPool<std::string>(10, 100, true);
    ASSERT_NE(factoryPool, nullptr);
    
    EXPECT_TRUE(factoryPool->initialize());
    EXPECT_EQ(factoryPool->getPoolSize(), 10);
    
    factoryPool->shutdown();
    
    // Test configuration factories
    auto defaultConfig = MemoryPoolFactory::createDefaultConfig();
    EXPECT_GT(defaultConfig.initialPoolSize, 0);
    
    auto highPerfConfig = MemoryPoolFactory::createHighPerformanceConfig();
    EXPECT_GT(highPerfConfig.initialPoolSize, defaultConfig.initialPoolSize);
    
    auto lowMemConfig = MemoryPoolFactory::createLowMemoryConfig();
    EXPECT_LT(lowMemConfig.initialPoolSize, defaultConfig.initialPoolSize);
}

TEST_F(MemoryPoolTest, MemoryPoolManager) {
    auto& manager = MemoryPoolManager::getInstance();
    
    // Test default pools
    auto stringPool = manager.getStringPool();
    auto jsonPool = manager.getJsonPool();
    auto vectorPool = manager.getVectorPool();
    
    ASSERT_NE(stringPool, nullptr);
    ASSERT_NE(jsonPool, nullptr);
    ASSERT_NE(vectorPool, nullptr);
    
    // Initialize pools
    manager.initializeAllPools();
    
    // Test string pool
    auto str = stringPool->acquire();
    ASSERT_NE(str, nullptr);
    *str = "manager_test";
    EXPECT_EQ(*str, "manager_test");
    
    // Test json pool
    auto jsonObj = jsonPool->acquire();
    ASSERT_NE(jsonObj, nullptr);
    (*jsonObj)["test"] = "value";
    EXPECT_EQ((*jsonObj)["test"], "value");
    
    // Test vector pool
    auto vec = vectorPool->acquire();
    ASSERT_NE(vec, nullptr);
    vec->push_back(42);
    EXPECT_EQ(vec->at(0), 42);
    
    // Get metrics
    auto allMetrics = manager.getAllPoolMetrics();
    EXPECT_TRUE(allMetrics.contains("string"));
    EXPECT_TRUE(allMetrics.contains("json"));
    EXPECT_TRUE(allMetrics.contains("vector"));
    
    // Shutdown pools
    manager.shutdownAllPools();
}

TEST_F(MemoryPoolTest, ConfigurationUpdate) {
    ASSERT_TRUE(pool_->initialize());
    
    // Update configuration
    MemoryPoolConfig newConfig = config_;
    newConfig.maxPoolSize = 50;
    newConfig.growthIncrement = 10;
    
    pool_->updateConfiguration(newConfig);
    
    auto retrievedConfig = pool_->getConfiguration();
    EXPECT_EQ(retrievedConfig.maxPoolSize, 50);
    EXPECT_EQ(retrievedConfig.growthIncrement, 10);
}

TEST_F(MemoryPoolTest, PoolExpansionAndShrinking) {
    ASSERT_TRUE(pool_->initialize());
    
    size_t initialSize = pool_->getPoolSize();
    
    // Manually expand pool
    pool_->expandPool(5);
    EXPECT_GT(pool_->getPoolSize(), initialSize);
    
    size_t expandedSize = pool_->getPoolSize();
    
    // Manually shrink pool
    pool_->shrinkPool(3);
    EXPECT_LT(pool_->getPoolSize(), expandedSize);
}

TEST_F(MemoryPoolTest, PoolClear) {
    ASSERT_TRUE(pool_->initialize());
    
    // Acquire some objects
    auto obj1 = pool_->acquire();
    auto obj2 = pool_->acquire();
    
    EXPECT_GT(pool_->getActiveObjectCount(), 0);
    
    // Clear pool
    pool_->clearPool();
    
    EXPECT_EQ(pool_->getPoolSize(), 0);
    EXPECT_EQ(pool_->getActiveObjectCount(), 0);
}

TEST_F(MemoryPoolTest, NullObjectHandling) {
    ASSERT_TRUE(pool_->initialize());
    
    // Try to release null object
    pool_->release(nullptr); // Should not crash
    
    // Pool state should be unchanged
    EXPECT_EQ(pool_->getActiveObjectCount(), 0);
}

TEST_F(MemoryPoolTest, HitRatioCalculation) {
    ASSERT_TRUE(pool_->initialize());
    
    // First acquisition should be a miss (new object created)
    auto obj1 = pool_->acquire();
    obj1.reset();
    
    // Second acquisition should be a hit (reused object)
    auto obj2 = pool_->acquire();
    obj2.reset();
    
    auto metrics = pool_->getMetrics();
    EXPECT_GT(metrics.poolHits.load(), 0);
    EXPECT_GT(metrics.hitRatio.load(), 0.0);
    EXPECT_LE(metrics.hitRatio.load(), 1.0);
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <vector>
#include <future>

#include "hydrogen/core/performance/connection_pool.h"
#include "hydrogen/core/performance/tcp_connection.h"

using namespace hydrogen::core::performance;
using namespace std::chrono_literals;

class ConnectionPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory_ = std::make_shared<MockConnectionFactory>();
        
        config_.minConnections = 2;
        config_.maxConnections = 10;
        config_.initialConnections = 3;
        config_.acquireTimeout = 1000ms;
        config_.idleTimeout = 5000ms;
        config_.maxLifetime = 30000ms;
        config_.healthCheckInterval = 1000ms;
        config_.cleanupInterval = 500ms;
        config_.enableHealthChecks = true;
        config_.enableMetrics = true;
        
        pool_ = std::make_shared<ConnectionPool>(factory_, config_);
    }
    
    void TearDown() override {
        if (pool_ && pool_->isRunning()) {
            pool_->shutdown();
        }
        pool_.reset();
        factory_.reset();
    }
    
    std::shared_ptr<MockConnectionFactory> factory_;
    ConnectionPoolConfig config_;
    std::shared_ptr<ConnectionPool> pool_;
};

TEST_F(ConnectionPoolTest, InitializationAndShutdown) {
    // Test initial state
    EXPECT_FALSE(pool_->isRunning());
    EXPECT_EQ(pool_->getTotalConnectionCount(), 0);
    
    // Test initialization
    EXPECT_TRUE(pool_->initialize());
    EXPECT_TRUE(pool_->isRunning());
    EXPECT_EQ(pool_->getTotalConnectionCount(), config_.initialConnections);
    
    // Test double initialization
    EXPECT_TRUE(pool_->initialize()); // Should succeed (already initialized)
    
    // Test shutdown
    pool_->shutdown();
    EXPECT_FALSE(pool_->isRunning());
    EXPECT_EQ(pool_->getTotalConnectionCount(), 0);
}

TEST_F(ConnectionPoolTest, BasicConnectionAcquisitionAndRelease) {
    ASSERT_TRUE(pool_->initialize());
    
    // Acquire connection
    auto connection = pool_->acquireConnection();
    ASSERT_NE(connection, nullptr);
    EXPECT_TRUE(connection->isConnected());
    EXPECT_TRUE(connection->isHealthy());
    
    // Check pool state
    EXPECT_EQ(pool_->getActiveConnectionCount(), 1);
    EXPECT_EQ(pool_->getTotalConnectionCount(), config_.initialConnections);
    
    // Release connection
    pool_->releaseConnection(connection);
    
    // Check pool state after release
    EXPECT_EQ(pool_->getActiveConnectionCount(), 0);
    EXPECT_EQ(pool_->getTotalConnectionCount(), config_.initialConnections);
}

TEST_F(ConnectionPoolTest, MultipleConnectionAcquisition) {
    ASSERT_TRUE(pool_->initialize());
    
    std::vector<std::shared_ptr<IConnection>> connections;
    
    // Acquire multiple connections
    for (size_t i = 0; i < config_.initialConnections; ++i) {
        auto connection = pool_->acquireConnection();
        ASSERT_NE(connection, nullptr);
        connections.push_back(connection);
    }
    
    EXPECT_EQ(pool_->getActiveConnectionCount(), config_.initialConnections);
    EXPECT_EQ(pool_->getTotalConnectionCount(), config_.initialConnections);
    
    // Release all connections
    for (auto& connection : connections) {
        pool_->releaseConnection(connection);
    }
    
    EXPECT_EQ(pool_->getActiveConnectionCount(), 0);
    EXPECT_EQ(pool_->getTotalConnectionCount(), config_.initialConnections);
}

TEST_F(ConnectionPoolTest, PoolExpansion) {
    ASSERT_TRUE(pool_->initialize());
    
    std::vector<std::shared_ptr<IConnection>> connections;
    
    // Acquire more connections than initial pool size
    size_t connectionsToAcquire = config_.initialConnections + 2;
    for (size_t i = 0; i < connectionsToAcquire; ++i) {
        auto connection = pool_->acquireConnection();
        ASSERT_NE(connection, nullptr);
        connections.push_back(connection);
    }
    
    // Pool should have expanded
    EXPECT_EQ(pool_->getActiveConnectionCount(), connectionsToAcquire);
    EXPECT_EQ(pool_->getTotalConnectionCount(), connectionsToAcquire);
    
    // Release all connections
    for (auto& connection : connections) {
        pool_->releaseConnection(connection);
    }
}

TEST_F(ConnectionPoolTest, MaxConnectionsLimit) {
    ASSERT_TRUE(pool_->initialize());
    
    std::vector<std::shared_ptr<IConnection>> connections;
    
    // Try to acquire more than max connections
    for (size_t i = 0; i < config_.maxConnections + 5; ++i) {
        auto connection = pool_->acquireConnection(100ms); // Short timeout
        if (connection) {
            connections.push_back(connection);
        }
    }
    
    // Should not exceed max connections
    EXPECT_LE(pool_->getTotalConnectionCount(), config_.maxConnections);
    EXPECT_LE(connections.size(), config_.maxConnections);
    
    // Release all connections
    for (auto& connection : connections) {
        pool_->releaseConnection(connection);
    }
}

TEST_F(ConnectionPoolTest, AcquisitionTimeout) {
    // Set very low max connections
    config_.maxConnections = 2;
    config_.initialConnections = 2;
    pool_ = std::make_shared<ConnectionPool>(factory_, config_);
    
    ASSERT_TRUE(pool_->initialize());
    
    // Acquire all available connections
    auto conn1 = pool_->acquireConnection();
    auto conn2 = pool_->acquireConnection();
    
    ASSERT_NE(conn1, nullptr);
    ASSERT_NE(conn2, nullptr);
    
    // Try to acquire another connection with short timeout
    auto start = std::chrono::steady_clock::now();
    auto conn3 = pool_->acquireConnection(100ms);
    auto end = std::chrono::steady_clock::now();
    
    EXPECT_EQ(conn3, nullptr);
    EXPECT_GE(end - start, 100ms);
    EXPECT_LT(end - start, 200ms); // Should timeout quickly
    
    // Release connections
    pool_->releaseConnection(conn1);
    pool_->releaseConnection(conn2);
}

TEST_F(ConnectionPoolTest, ConcurrentAccess) {
    ASSERT_TRUE(pool_->initialize());
    
    const int numThreads = 10;
    const int operationsPerThread = 50;
    std::vector<std::future<int>> futures;
    
    // Launch concurrent threads
    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [this, operationsPerThread]() {
            int successfulOperations = 0;
            
            for (int j = 0; j < operationsPerThread; ++j) {
                auto connection = pool_->acquireConnection(1000ms);
                if (connection) {
                    // Simulate some work
                    std::this_thread::sleep_for(1ms);
                    pool_->releaseConnection(connection);
                    successfulOperations++;
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
    EXPECT_EQ(pool_->getActiveConnectionCount(), 0);
}

TEST_F(ConnectionPoolTest, HealthChecks) {
    ASSERT_TRUE(pool_->initialize());
    
    // Get initial metrics
    auto initialMetrics = pool_->getMetrics();
    
    // Wait for at least one health check cycle
    std::this_thread::sleep_for(config_.healthCheckInterval + 100ms);
    
    // Health checks should have been performed
    auto currentMetrics = pool_->getMetrics();
    
    // Pool should still be healthy
    EXPECT_EQ(pool_->getTotalConnectionCount(), config_.initialConnections);
}

TEST_F(ConnectionPoolTest, UnhealthyConnectionRemoval) {
    ASSERT_TRUE(pool_->initialize());
    
    // Acquire a connection
    auto connection = pool_->acquireConnection();
    ASSERT_NE(connection, nullptr);
    
    // Make the connection unhealthy
    auto mockConnection = std::dynamic_pointer_cast<MockConnection>(connection);
    ASSERT_NE(mockConnection, nullptr);
    mockConnection->setHealthCheckShouldFail(true);
    
    // Release the connection
    pool_->releaseConnection(connection);
    
    // Wait for health check to detect and remove unhealthy connection
    std::this_thread::sleep_for(config_.healthCheckInterval + 100ms);
    
    // The unhealthy connection should have been removed
    // Pool might create new connections to maintain minimum
    EXPECT_GE(pool_->getTotalConnectionCount(), config_.minConnections);
}

TEST_F(ConnectionPoolTest, Metrics) {
    ASSERT_TRUE(pool_->initialize());
    
    // Perform some operations
    auto connection1 = pool_->acquireConnection();
    auto connection2 = pool_->acquireConnection();
    
    pool_->releaseConnection(connection1);
    pool_->releaseConnection(connection2);
    
    // Check metrics
    auto metrics = pool_->getMetrics();
    
    EXPECT_EQ(metrics.totalConnections.load(), config_.initialConnections);
    EXPECT_EQ(metrics.activeConnections.load(), 0);
    EXPECT_EQ(metrics.idleConnections.load(), config_.initialConnections);
    EXPECT_GE(metrics.connectionsAcquired.load(), 2);
    EXPECT_GE(metrics.connectionsReleased.load(), 2);
    
    // Check detailed metrics
    auto detailedMetrics = pool_->getDetailedMetrics();
    EXPECT_TRUE(detailedMetrics.contains("connectionDetails"));
    EXPECT_EQ(detailedMetrics["connectionDetails"].size(), config_.initialConnections);
}

TEST_F(ConnectionPoolTest, ConfigurationUpdate) {
    ASSERT_TRUE(pool_->initialize());
    
    // Update configuration
    ConnectionPoolConfig newConfig = config_;
    newConfig.maxConnections = 20;
    newConfig.idleTimeout = 10000ms;
    
    pool_->updateConfiguration(newConfig);
    
    auto retrievedConfig = pool_->getConfiguration();
    EXPECT_EQ(retrievedConfig.maxConnections, 20);
    EXPECT_EQ(retrievedConfig.idleTimeout, 10000ms);
}

TEST_F(ConnectionPoolTest, ConnectionPoolBuilder) {
    auto pool = ConnectionPoolBuilder()
        .withMockFactory()
        .withMinConnections(3)
        .withMaxConnections(15)
        .withInitialConnections(5)
        .withAcquireTimeout(2000ms)
        .withHealthChecks(true)
        .withMetrics(true)
        .build();
    
    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(pool->initialize());
    
    EXPECT_EQ(pool->getTotalConnectionCount(), 5);
    
    auto config = pool->getConfiguration();
    EXPECT_EQ(config.minConnections, 3);
    EXPECT_EQ(config.maxConnections, 15);
    EXPECT_EQ(config.initialConnections, 5);
    EXPECT_EQ(config.acquireTimeout, 2000ms);
    
    pool->shutdown();
}

TEST_F(ConnectionPoolTest, NullConnectionHandling) {
    ASSERT_TRUE(pool_->initialize());
    
    // Try to release null connection
    pool_->releaseConnection(nullptr); // Should not crash
    
    // Pool state should be unchanged
    EXPECT_EQ(pool_->getActiveConnectionCount(), 0);
    EXPECT_EQ(pool_->getTotalConnectionCount(), config_.initialConnections);
}

TEST_F(ConnectionPoolTest, FactoryFailure) {
    // Configure factory to fail connection creation
    factory_->setCreateShouldFail(true);
    
    // Pool initialization should handle factory failures gracefully
    EXPECT_TRUE(pool_->initialize()); // Should still succeed
    
    // But total connections might be less than expected
    EXPECT_LE(pool_->getTotalConnectionCount(), config_.initialConnections);
    
    // Reset factory
    factory_->setCreateShouldFail(false);
    
    // Should be able to acquire connections now
    auto connection = pool_->acquireConnection();
    if (connection) {
        pool_->releaseConnection(connection);
    }
}

// Test connection reuse
TEST_F(ConnectionPoolTest, ConnectionReuse) {
    ASSERT_TRUE(pool_->initialize());
    
    // Acquire and release a connection multiple times
    std::string firstConnectionId;
    
    for (int i = 0; i < 5; ++i) {
        auto connection = pool_->acquireConnection();
        ASSERT_NE(connection, nullptr);
        
        if (i == 0) {
            firstConnectionId = connection->getId();
        }
        
        pool_->releaseConnection(connection);
    }
    
    // Should have reused connections (total count shouldn't increase much)
    EXPECT_EQ(pool_->getTotalConnectionCount(), config_.initialConnections);
    
    // Check metrics for reuse
    auto metrics = pool_->getMetrics();
    EXPECT_GE(metrics.connectionsAcquired.load(), 5);
    EXPECT_GE(metrics.connectionsReleased.load(), 5);
}

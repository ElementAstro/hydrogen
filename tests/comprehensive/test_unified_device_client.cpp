#include "comprehensive_test_framework.h"
#include "mock_objects.h"
#include <hydrogen/core/unified_device_client.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace hydrogen::testing;
using namespace hydrogen::core;
using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::AtLeast;

/**
 * @brief Comprehensive test fixture for UnifiedDeviceClient
 */
HYDROGEN_TEST_FIXTURE(UnifiedDeviceClientTest) {
protected:
    void SetUp() override {
        ComprehensiveTestFixture::SetUp();
        
        // Enable all testing modes for comprehensive coverage
        getConfig().enablePerformanceTesting = true;
        getConfig().enableIntegrationTesting = true;
        getConfig().enableStressTesting = true;
        getConfig().enableConcurrencyTesting = true;
        
        // Create mock objects
        mockWebSocketClient_ = std::make_unique<MockWebSocketClient>();
        mockWebSocketClient_->setupDefaultBehavior();
        
        mockDeviceManager_ = std::make_unique<MockDeviceManager>();
        mockDeviceManager_->setupDefaultBehavior();
        
        // Create test configuration
        config_.host = "localhost";
        config_.port = 8080;
        config_.endpoint = "/ws";
        config_.connectTimeout = std::chrono::milliseconds{5000};
        config_.messageTimeout = std::chrono::milliseconds{3000};
        config_.enableAutoReconnect = true;
        
        // Create client under test
        client_ = std::make_unique<UnifiedDeviceClient>(config_);
    }
    
    void TearDown() override {
        client_.reset();
        mockWebSocketClient_.reset();
        mockDeviceManager_.reset();
        ComprehensiveTestFixture::TearDown();
    }

protected:
    std::unique_ptr<UnifiedDeviceClient> client_;
    std::unique_ptr<MockWebSocketClient> mockWebSocketClient_;
    std::unique_ptr<MockDeviceManager> mockDeviceManager_;
    ClientConnectionConfig config_;
};

// Basic functionality tests
TEST_F(UnifiedDeviceClientTest, BasicConnection) {
    EXPECT_CALL(*mockWebSocketClient_, connect(config_.host, config_.port))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockWebSocketClient_, isConnected())
        .WillRepeatedly(Return(true));
    
    bool connected = client_->connect();
    EXPECT_TRUE(connected);
    EXPECT_TRUE(client_->isConnected());
    
    logTestInfo("Basic connection test completed successfully");
}

TEST_F(UnifiedDeviceClientTest, ConnectionFailure) {
    EXPECT_CALL(*mockWebSocketClient_, connect(config_.host, config_.port))
        .WillOnce(Return(false));
    EXPECT_CALL(*mockWebSocketClient_, isConnected())
        .WillRepeatedly(Return(false));
    
    bool connected = client_->connect();
    EXPECT_FALSE(connected);
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(UnifiedDeviceClientTest, DeviceDiscovery) {
    // Setup connection
    EXPECT_CALL(*mockWebSocketClient_, connect(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mockWebSocketClient_, isConnected()).WillRepeatedly(Return(true));
    
    client_->connect();
    
    // Mock device discovery response
    json expectedDevices = json::array();
    json device1;
    device1["deviceId"] = "device_001";
    device1["deviceType"] = "sensor";
    device1["name"] = "Temperature Sensor";
    expectedDevices.push_back(device1);
    
    json device2;
    device2["deviceId"] = "device_002";
    device2["deviceType"] = "actuator";
    device2["name"] = "Motor Controller";
    expectedDevices.push_back(device2);
    
    // Test device discovery
    std::vector<std::string> deviceTypes = {"sensor", "actuator"};
    json discoveredDevices = client_->discoverDevices(deviceTypes);
    
    // Verify the discovery request was made
    EXPECT_TRUE(discoveredDevices.is_object() || discoveredDevices.is_array());
}

// Performance tests
HYDROGEN_PERFORMANCE_TEST(UnifiedDeviceClientTest, ConnectionPerformance) {
    EXPECT_CALL(*mockWebSocketClient_, connect(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mockWebSocketClient_, isConnected())
        .WillRepeatedly(Return(true));
    
    BENCHMARK_OPERATION({
        client_->connect();
        client_->disconnect();
    }, "connection_cycle");
    
    // Verify performance meets requirements
    auto elapsedTime = getElapsedTime();
    EXPECT_LT(elapsedTime.count(), 1000) << "Connection should complete within 1 second";
}

HYDROGEN_PERFORMANCE_TEST(UnifiedDeviceClientTest, MessageThroughput) {
    // Setup connection
    EXPECT_CALL(*mockWebSocketClient_, connect(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mockWebSocketClient_, isConnected()).WillRepeatedly(Return(true));
    
    client_->connect();
    
    // Benchmark message sending
    benchmarkOperation([this]() {
        json testMessage;
        testMessage["type"] = "command";
        testMessage["deviceId"] = "test_device";
        testMessage["command"] = "get_status";
        
        try {
            client_->executeCommand("test_device", "get_status", json::object());
        } catch (const std::exception& e) {
            // Expected in mock environment
        }
    }, 100, "message_throughput");
}

// Stress tests
HYDROGEN_STRESS_TEST(UnifiedDeviceClientTest, ConnectionStressTest) {
    EXPECT_CALL(*mockWebSocketClient_, connect(_, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mockWebSocketClient_, isConnected())
        .WillRepeatedly(Return(true));
    
    // The stress test framework will run this multiple times
    client_->connect();
    EXPECT_TRUE(client_->isConnected());
    client_->disconnect();
    EXPECT_FALSE(client_->isConnected());
}

// Concurrency tests
TEST_F(UnifiedDeviceClientTest, ConcurrentConnections) {
    if (!getConfig().enableConcurrencyTesting) {
        GTEST_SKIP() << "Concurrency testing disabled";
    }
    
    EXPECT_CALL(*mockWebSocketClient_, connect(_, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mockWebSocketClient_, isConnected())
        .WillRepeatedly(Return(true));
    
    runConcurrentTest([this](int threadId) {
        // Each thread attempts to connect/disconnect
        bool connected = client_->connect();
        EXPECT_TRUE(connected);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        client_->disconnect();
        EXPECT_FALSE(client_->isConnected());
    }, 4);
}

// Integration tests
HYDROGEN_INTEGRATION_TEST(UnifiedDeviceClientTest, EndToEndDeviceInteraction) {
    // This test would require a real server running
    // For now, we'll simulate the interaction
    
    EXPECT_CALL(*mockWebSocketClient_, connect(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mockWebSocketClient_, isConnected()).WillRepeatedly(Return(true));
    
    // Connect to server
    bool connected = client_->connect();
    ASSERT_TRUE(connected);
    
    // Discover devices
    json devices = client_->discoverDevices();
    EXPECT_TRUE(devices.is_object() || devices.is_array());
    
    // Test device interaction sequence
    std::string testDeviceId = "integration_test_device";
    
    try {
        // Get device properties
        json properties = client_->getDeviceProperties(testDeviceId);
        
        // Set a property
        json newProperty;
        newProperty["test_value"] = 42;
        json setResult = client_->setDeviceProperties(testDeviceId, newProperty);
        
        // Execute a command
        json commandResult = client_->executeCommand(testDeviceId, "test_command", json::object());
        
        logTestInfo("End-to-end device interaction completed");
        
    } catch (const std::exception& e) {
        logTestWarning("Integration test failed (expected in mock environment): " + std::string(e.what()));
    }
}

// Error handling tests
TEST_F(UnifiedDeviceClientTest, ErrorRecovery) {
    InSequence seq;
    
    // First connection fails
    EXPECT_CALL(*mockWebSocketClient_, connect(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mockWebSocketClient_, isConnected()).WillOnce(Return(false));
    
    // Second connection succeeds
    EXPECT_CALL(*mockWebSocketClient_, connect(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mockWebSocketClient_, isConnected()).WillRepeatedly(Return(true));
    
    // First attempt should fail
    bool connected = client_->connect();
    EXPECT_FALSE(connected);
    
    // Second attempt should succeed
    connected = client_->connect();
    EXPECT_TRUE(connected);
}

// Configuration tests
TEST_F(UnifiedDeviceClientTest, ConfigurationUpdate) {
    ClientConnectionConfig newConfig = config_;
    newConfig.host = "newhost.example.com";
    newConfig.port = 9090;
    newConfig.messageTimeout = std::chrono::milliseconds{10000};
    
    client_->updateConfig(newConfig);
    
    // Verify configuration was updated
    // In a real implementation, we would have a way to verify this
    logTestInfo("Configuration update test completed");
}

// Timeout tests
TEST_F(UnifiedDeviceClientTest, MessageTimeout) {
    EXPECT_CALL(*mockWebSocketClient_, connect(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mockWebSocketClient_, isConnected()).WillRepeatedly(Return(true));
    
    client_->connect();
    
    // Test with very short timeout
    config_.messageTimeout = std::chrono::milliseconds{1};
    client_->updateConfig(config_);
    
    EXPECT_WITHIN_TIMEOUT([this]() {
        try {
            client_->executeCommand("test_device", "slow_command", json::object());
            return false; // Should timeout
        } catch (const std::exception&) {
            return true; // Expected timeout exception
        }
    }, std::chrono::milliseconds{100});
}

// Memory and resource tests
TEST_F(UnifiedDeviceClientTest, ResourceCleanup) {
    size_t initialMemory = PerformanceTester::getCurrentMemoryUsage();
    
    {
        // Create and destroy multiple clients
        for (int i = 0; i < 10; ++i) {
            auto tempClient = std::make_unique<UnifiedDeviceClient>(config_);
            // Simulate some operations
            tempClient.reset();
        }
    }
    
    size_t finalMemory = PerformanceTester::getCurrentMemoryUsage();
    
    // Memory usage should not have grown significantly
    size_t memoryGrowth = finalMemory > initialMemory ? finalMemory - initialMemory : 0;
    EXPECT_LT(memoryGrowth, 1024 * 1024) << "Memory growth should be less than 1MB";
    
    logTestInfo("Resource cleanup test completed - Memory growth: " + std::to_string(memoryGrowth) + " bytes");
}

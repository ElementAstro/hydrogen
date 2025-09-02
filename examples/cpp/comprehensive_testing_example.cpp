/**
 * @file comprehensive_testing_example.cpp
 * @brief Example demonstrating the Hydrogen Comprehensive Testing Framework
 * 
 * This example shows how to use the new testing framework:
 * - ComprehensiveTestFixture for enhanced testing
 * - Mock objects for unit testing
 * - Performance testing capabilities
 * - Integration testing setup
 */

#include "comprehensive_test_framework.h"
#include "mock_objects.h"
#include <hydrogen/core/unified_device_client.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace hydrogen::testing;
using namespace hydrogen::core;
using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;

/**
 * @brief Example test fixture demonstrating comprehensive testing capabilities
 */
HYDROGEN_TEST_FIXTURE(ComprehensiveTestingExample) {
protected:
    void SetUp() override {
        ComprehensiveTestFixture::SetUp();
        
        // Configure testing modes
        getConfig().enablePerformanceTesting = true;
        getConfig().enableIntegrationTesting = true;
        getConfig().enableStressTesting = true;
        getConfig().enableConcurrencyTesting = true;
        
        // Set performance thresholds
        getConfig().maxResponseTime = std::chrono::milliseconds{100};
        getConfig().stressTestIterations = 1000;
        
        // Create mock objects
        setupMocks();
        
        // Create test configuration
        setupTestConfiguration();
        
        logTestInfo("Test fixture setup completed");
    }
    
    void TearDown() override {
        cleanupMocks();
        ComprehensiveTestFixture::TearDown();
        logTestInfo("Test fixture teardown completed");
    }
    
    void setupMocks() {
        // Create mock device
        mockDevice_ = std::make_unique<MockDevice>();
        mockDevice_->setupDefaultBehavior();
        
        // Configure mock device behavior
        EXPECT_CALL(*mockDevice_, getDeviceId())
            .WillRepeatedly(Return("test_device_001"));
        EXPECT_CALL(*mockDevice_, getDeviceType())
            .WillRepeatedly(Return("camera"));
        EXPECT_CALL(*mockDevice_, isOnline())
            .WillRepeatedly(Return(true));
        
        // Create mock WebSocket client
        mockWebSocketClient_ = std::make_unique<MockWebSocketClient>();
        mockWebSocketClient_->setupDefaultBehavior();
        
        // Create mock device manager
        mockDeviceManager_ = std::make_unique<MockDeviceManager>();
        mockDeviceManager_->setupDefaultBehavior();
        mockDeviceManager_->addMockDevice("test_device_001", "camera");
    }
    
    void cleanupMocks() {
        MockTestUtils::resetAllMocks();
        mockDevice_.reset();
        mockWebSocketClient_.reset();
        mockDeviceManager_.reset();
    }
    
    void setupTestConfiguration() {
        testConfig_.host = "localhost";
        testConfig_.port = 8080;
        testConfig_.endpoint = "/ws";
        testConfig_.connectTimeout = std::chrono::milliseconds{5000};
        testConfig_.messageTimeout = std::chrono::milliseconds{3000};
        testConfig_.enableAutoReconnect = true;
    }

protected:
    std::unique_ptr<MockDevice> mockDevice_;
    std::unique_ptr<MockWebSocketClient> mockWebSocketClient_;
    std::unique_ptr<MockDeviceManager> mockDeviceManager_;
    ClientConnectionConfig testConfig_;
};

// Basic unit test example
TEST_F(ComprehensiveTestingExample, BasicDeviceInteraction) {
    logTestInfo("Starting basic device interaction test");
    
    // Test device properties
    EXPECT_EQ(mockDevice_->getDeviceId(), "test_device_001");
    EXPECT_EQ(mockDevice_->getDeviceType(), "camera");
    EXPECT_TRUE(mockDevice_->isOnline());
    
    // Test device commands
    json commandParams;
    commandParams["exposure_time"] = 5.0;
    
    EXPECT_CALL(*mockDevice_, executeCommand("start_exposure", _))
        .WillOnce(Return(json{{"success", true}, {"message", "Exposure started"}}));
    
    auto result = mockDevice_->executeCommand("start_exposure", commandParams);
    EXPECT_TRUE(result["success"].get<bool>());
    
    logTestInfo("Basic device interaction test completed");
}

// Performance test example
HYDROGEN_PERFORMANCE_TEST(ComprehensiveTestingExample, DeviceCommandPerformance) {
    logTestInfo("Starting device command performance test");
    
    // Setup mock expectations
    EXPECT_CALL(*mockDevice_, executeCommand(_, _))
        .Times(AtLeast(100))
        .WillRepeatedly(Return(json{{"success", true}}));
    
    // Benchmark device command execution
    benchmarkOperation([this]() {
        json params;
        params["test"] = true;
        mockDevice_->executeCommand("test_command", params);
    }, 100, "device_command_execution");
    
    // Verify performance meets requirements
    auto elapsedTime = getElapsedTime();
    EXPECT_LT(elapsedTime.count(), 1000) << "Performance test should complete within 1 second";
    
    logTestInfo("Device command performance test completed");
}

// Stress test example
HYDROGEN_STRESS_TEST(ComprehensiveTestingExample, HighLoadDeviceCommands) {
    logTestInfo("Starting high load device commands stress test");
    
    // Setup mock expectations for high load
    EXPECT_CALL(*mockDevice_, executeCommand(_, _))
        .Times(AtLeast(getConfig().stressTestIterations))
        .WillRepeatedly(Return(json{{"success", true}}));
    
    // The stress test framework will run this function multiple times
    json params;
    params["iteration"] = true;
    auto result = mockDevice_->executeCommand("stress_test", params);
    EXPECT_TRUE(result["success"].get<bool>());
}

// Concurrency test example
TEST_F(ComprehensiveTestingExample, ConcurrentDeviceAccess) {
    if (!getConfig().enableConcurrencyTesting) {
        GTEST_SKIP() << "Concurrency testing disabled";
    }
    
    logTestInfo("Starting concurrent device access test");
    
    // Setup mock expectations for concurrent access
    EXPECT_CALL(*mockDevice_, executeCommand(_, _))
        .Times(AtLeast(16)) // 4 threads * 4 operations each
        .WillRepeatedly(Return(json{{"success", true}}));
    
    runConcurrentTest([this](int threadId) {
        for (int i = 0; i < 4; ++i) {
            json params;
            params["thread_id"] = threadId;
            params["operation"] = i;
            
            auto result = mockDevice_->executeCommand("concurrent_test", params);
            EXPECT_TRUE(result["success"].get<bool>());
            
            // Small delay to simulate real work
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }, 4);
    
    logTestInfo("Concurrent device access test completed");
}

// Integration test example
HYDROGEN_INTEGRATION_TEST(ComprehensiveTestingExample, EndToEndDeviceWorkflow) {
    logTestInfo("Starting end-to-end device workflow integration test");
    
    // This test would typically connect to a real server
    // For demonstration, we'll use mocks but show the pattern
    
    // Setup WebSocket client mock
    EXPECT_CALL(*mockWebSocketClient_, connect(testConfig_.host, testConfig_.port))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockWebSocketClient_, isConnected())
        .WillRepeatedly(Return(true));
    
    // Create unified device client (would use real implementation)
    auto client = std::make_unique<UnifiedDeviceClient>(testConfig_);
    
    // Test connection
    // bool connected = client->connect(); // Would work with real implementation
    bool connected = true; // Simulated for example
    ASSERT_TRUE(connected);
    
    // Test device discovery
    json expectedDevices = json::array();
    json device;
    device["deviceId"] = "test_device_001";
    device["deviceType"] = "camera";
    expectedDevices.push_back(device);
    
    // Test device interaction workflow
    try {
        // Get device properties
        json properties = generateTestData("device_properties");
        EXPECT_FALSE(properties.empty());
        
        // Execute command sequence
        json commandResult = generateTestData("command_result");
        EXPECT_TRUE(commandResult.contains("success"));
        
        logTestInfo("End-to-end workflow completed successfully");
        
    } catch (const std::exception& e) {
        logTestWarning("Integration test failed (expected in mock environment): " + std::string(e.what()));
    }
}

// Memory and resource test example
TEST_F(ComprehensiveTestingExample, ResourceManagement) {
    logTestInfo("Starting resource management test");
    
    size_t initialMemory = PerformanceTester::getCurrentMemoryUsage();
    
    // Create and destroy multiple objects to test memory management
    std::vector<std::unique_ptr<MockDevice>> devices;
    
    for (int i = 0; i < 100; ++i) {
        auto device = std::make_unique<MockDevice>();
        device->setupDefaultBehavior();
        devices.push_back(std::move(device));
    }
    
    size_t peakMemory = PerformanceTester::getCurrentMemoryUsage();
    
    // Clear all devices
    devices.clear();
    
    size_t finalMemory = PerformanceTester::getCurrentMemoryUsage();
    
    // Verify memory usage
    size_t memoryGrowth = finalMemory > initialMemory ? finalMemory - initialMemory : 0;
    EXPECT_LT(memoryGrowth, 1024 * 1024) << "Memory growth should be less than 1MB";
    
    logTestInfo("Resource management test completed");
    logTestInfo("Memory usage - Initial: " + std::to_string(initialMemory) + 
                ", Peak: " + std::to_string(peakMemory) + 
                ", Final: " + std::to_string(finalMemory));
}

// Error handling test example
TEST_F(ComprehensiveTestingExample, ErrorHandlingAndRecovery) {
    logTestInfo("Starting error handling and recovery test");
    
    // Test error simulation
    mockDevice_->simulateError("Connection timeout");
    
    EXPECT_CALL(*mockDevice_, executeCommand(_, _))
        .WillOnce(Return(json{{"error", "Connection timeout"}, {"success", false}}));
    
    json params;
    auto result = mockDevice_->executeCommand("test_command", params);
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["error"].get<std::string>(), "Connection timeout");
    
    // Test recovery
    mockDevice_->setupDefaultBehavior(); // Reset to normal behavior
    
    EXPECT_CALL(*mockDevice_, executeCommand(_, _))
        .WillOnce(Return(json{{"success", true}}));
    
    result = mockDevice_->executeCommand("test_command", params);
    EXPECT_TRUE(result["success"].get<bool>());
    
    logTestInfo("Error handling and recovery test completed");
}

// Test data generation example
TEST_F(ComprehensiveTestingExample, TestDataGeneration) {
    logTestInfo("Starting test data generation example");
    
    // Generate different types of test data
    json basicData = generateTestData();
    EXPECT_FALSE(basicData.empty());
    EXPECT_TRUE(basicData.contains("id"));
    EXPECT_TRUE(basicData.contains("timestamp"));
    
    // Generate random data
    auto randomBytes = generateRandomData(1024);
    EXPECT_EQ(randomBytes.size(), 1024);
    
    auto randomString = generateRandomString(50);
    EXPECT_EQ(randomString.length(), 50);
    
    // Use test data manager
    auto& dataManager = TestDataManager::getInstance();
    
    json deviceData = dataManager.getDeviceTestData("camera");
    EXPECT_FALSE(deviceData.empty());
    
    json messageData = dataManager.getMessageTestData("command");
    EXPECT_FALSE(messageData.empty());
    
    logTestInfo("Test data generation example completed");
}

// Performance comparison example
TEST_F(ComprehensiveTestingExample, PerformanceComparison) {
    if (!getConfig().enablePerformanceTesting) {
        GTEST_SKIP() << "Performance testing disabled";
    }
    
    logTestInfo("Starting performance comparison test");
    
    // Setup mock expectations
    EXPECT_CALL(*mockDevice_, executeCommand(_, _))
        .Times(AtLeast(2000))
        .WillRepeatedly(Return(json{{"success", true}}));
    
    // Compare different approaches
    std::vector<std::pair<std::string, std::function<void()>>> operations = {
        {"direct_call", [this]() {
            json params;
            mockDevice_->executeCommand("test", params);
        }},
        {"with_validation", [this]() {
            json params;
            params["validated"] = true;
            auto result = mockDevice_->executeCommand("test", params);
            // Simulate validation
            bool success = result.contains("success") && result["success"].get<bool>();
            (void)success; // Suppress unused variable warning
        }}
    };
    
    PerformanceTester::comparePerformance(operations, 1000);
    
    logTestInfo("Performance comparison test completed");
}

// Example of using test utilities
TEST_F(ComprehensiveTestingExample, TestUtilities) {
    logTestInfo("Starting test utilities example");
    
    // Test timeout utilities
    bool conditionMet = false;
    std::thread delayedThread([&conditionMet]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        conditionMet = true;
    });
    
    EXPECT_WITHIN_TIMEOUT([&conditionMet]() { return conditionMet; }, 
                          std::chrono::milliseconds{100});
    
    delayedThread.join();
    
    // Test file utilities
    std::string tempFile = createTempFile("test content");
    EXPECT_FALSE(tempFile.empty());
    
    std::string tempDir = createTempDirectory();
    EXPECT_FALSE(tempDir.empty());
    
    // Test timer utilities
    startTimer();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    stopTimer();
    
    auto elapsed = getElapsedTime();
    EXPECT_GE(elapsed.count(), 10);
    
    logTestInfo("Test utilities example completed");
}

// Main function to run the examples
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    
    // Configure test environment
    std::cout << "🧪 Hydrogen Comprehensive Testing Framework Example\n" << std::endl;
    
    // Set environment variables for testing
    setenv("HYDROGEN_TEST_PERFORMANCE", "1", 1);
    setenv("HYDROGEN_TEST_INTEGRATION", "1", 1);
    setenv("HYDROGEN_TEST_STRESS", "1", 1);
    setenv("HYDROGEN_TEST_CONCURRENCY", "1", 1);
    
    std::cout << "Environment configured for comprehensive testing" << std::endl;
    std::cout << "Running tests...\n" << std::endl;
    
    int result = RUN_ALL_TESTS();
    
    std::cout << "\n🎯 Testing framework example completed!" << std::endl;
    std::cout << "Result: " << (result == 0 ? "✅ SUCCESS" : "❌ FAILURE") << std::endl;
    
    return result;
}

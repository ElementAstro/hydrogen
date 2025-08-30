#include <gtest/gtest.h>
#include "client/device_client_refactored.h"
#include "client/device_client.h" // Original for comparison
#include <thread>
#include <chrono>
#include <future>

using namespace astrocomm;

/**
 * Integration tests to verify the refactored DeviceClient maintains
 * the same functionality as the original implementation
 */
class ClientIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        refactoredClient = std::make_unique<DeviceClientRefactored>();
        // originalClient = std::make_unique<DeviceClient>(); // Uncomment when comparing
    }

    void TearDown() override {
        if (refactoredClient && refactoredClient->isConnected()) {
            refactoredClient->disconnect();
        }
    }

    std::unique_ptr<DeviceClientRefactored> refactoredClient;
    // std::unique_ptr<DeviceClient> originalClient; // For comparison testing
};

TEST_F(ClientIntegrationTest, ComponentInitialization) {
    // Verify all components are properly initialized
    EXPECT_NE(refactoredClient->getConnectionManager(), nullptr);
    EXPECT_NE(refactoredClient->getMessageProcessor(), nullptr);
    EXPECT_NE(refactoredClient->getDeviceManager(), nullptr);
    EXPECT_NE(refactoredClient->getCommandExecutor(), nullptr);
    EXPECT_NE(refactoredClient->getSubscriptionManager(), nullptr);
    
    // Verify initial state
    EXPECT_FALSE(refactoredClient->isConnected());
    
    json status = refactoredClient->getStatusInfo();
    EXPECT_TRUE(status.contains("connection"));
    EXPECT_TRUE(status.contains("devices"));
    EXPECT_TRUE(status.contains("execution"));
    EXPECT_TRUE(status.contains("subscriptions"));
    EXPECT_TRUE(status.contains("processing"));
}

TEST_F(ClientIntegrationTest, ConnectionLifecycle) {
    // Test connection configuration
    refactoredClient->setAutoReconnect(true, 2000, 3);
    
    json connectionStatus = refactoredClient->getConnectionStatus();
    EXPECT_TRUE(connectionStatus["autoReconnectEnabled"]);
    EXPECT_EQ(connectionStatus["reconnectIntervalMs"], 2000);
    EXPECT_EQ(connectionStatus["maxReconnectAttempts"], 3);
    
    // Note: Actual connection testing would require a test server
    // For now, we test the interface and state management
    EXPECT_FALSE(refactoredClient->isConnected());
}

TEST_F(ClientIntegrationTest, DeviceManagementWorkflow) {
    // Test device management without actual connection
    json devices = refactoredClient->getDevices();
    EXPECT_TRUE(devices.is_object());
    EXPECT_EQ(devices.size(), 0);
    
    // Test device statistics
    json deviceStats = refactoredClient->getDeviceStats();
    EXPECT_EQ(deviceStats["discoveryRequests"], 0);
    EXPECT_EQ(deviceStats["cachedDevices"], 0);
    
    // Test device validation through the facade
    auto* deviceManager = refactoredClient->getDeviceManager();
    EXPECT_FALSE(deviceManager->hasDevice(""));
    EXPECT_FALSE(deviceManager->hasDevice("invalid@device"));
}

TEST_F(ClientIntegrationTest, SubscriptionManagement) {
    bool propertyCallbackCalled = false;
    bool eventCallbackCalled = false;
    
    // Test property subscription
    auto propertyCallback = [&](const std::string& deviceId, 
                                const std::string& property, 
                                const json& value) {
        propertyCallbackCalled = true;
    };
    
    // Test event subscription
    auto eventCallback = [&](const std::string& deviceId, 
                             const std::string& event, 
                             const json& details) {
        eventCallbackCalled = true;
    };
    
    // Subscribe through the facade
    EXPECT_NO_THROW(refactoredClient->subscribeToProperty("test-device", "temperature", propertyCallback));
    EXPECT_NO_THROW(refactoredClient->subscribeToEvent("test-device", "status-change", eventCallback));
    
    // Verify subscriptions through component access
    auto* subscriptionManager = refactoredClient->getSubscriptionManager();
    EXPECT_TRUE(subscriptionManager->isSubscribedToProperty("test-device", "temperature"));
    EXPECT_TRUE(subscriptionManager->isSubscribedToEvent("test-device", "status-change"));
    
    // Test unsubscription
    refactoredClient->unsubscribeFromProperty("test-device", "temperature");
    refactoredClient->unsubscribeFromEvent("test-device", "status-change");
    
    EXPECT_FALSE(subscriptionManager->isSubscribedToProperty("test-device", "temperature"));
    EXPECT_FALSE(subscriptionManager->isSubscribedToEvent("test-device", "status-change"));
}

TEST_F(ClientIntegrationTest, CommandExecutionInterface) {
    // Test command execution interface (without actual connection)
    auto* commandExecutor = refactoredClient->getCommandExecutor();
    
    // Verify initial state
    EXPECT_EQ(commandExecutor->getPendingAsyncCount(), 0);
    
    json executionStats = refactoredClient->getExecutionStats();
    EXPECT_EQ(executionStats["commandsExecuted"], 0);
    EXPECT_EQ(executionStats["asyncCommandsExecuted"], 0);
    
    // Test configuration
    EXPECT_NO_THROW(refactoredClient->setMessageRetryParams(3, 1000));
}

TEST_F(ClientIntegrationTest, MessageProcessingInterface) {
    auto* messageProcessor = refactoredClient->getMessageProcessor();
    
    // Verify initial state
    EXPECT_FALSE(messageProcessor->isRunning());
    
    json processingStats = refactoredClient->getProcessingStats();
    EXPECT_EQ(processingStats["messagesSent"], 0);
    EXPECT_EQ(processingStats["messagesReceived"], 0);
    EXPECT_FALSE(processingStats["running"]);
}

TEST_F(ClientIntegrationTest, ComponentInteraction) {
    // Test that components work together correctly
    
    // 1. Connection state should affect message processing
    EXPECT_FALSE(refactoredClient->isConnected());
    EXPECT_FALSE(refactoredClient->getMessageProcessor()->isRunning());
    
    // 2. Device cache should be accessible through device manager
    auto* deviceManager = refactoredClient->getDeviceManager();
    json devices = deviceManager->getDevices();
    json facadeDevices = refactoredClient->getDevices();
    EXPECT_EQ(devices, facadeDevices);
    
    // 3. Subscription stats should be consistent
    json subscriptionStats = refactoredClient->getSubscriptionStats();
    json managerStats = refactoredClient->getSubscriptionManager()->getSubscriptionStats();
    EXPECT_EQ(subscriptionStats, managerStats);
}

TEST_F(ClientIntegrationTest, ErrorHandling) {
    // Test error handling for invalid operations
    
    // Invalid device operations
    EXPECT_THROW(refactoredClient->getDeviceProperties("", {}), std::invalid_argument);
    EXPECT_THROW(refactoredClient->setDeviceProperties("", json::object()), std::invalid_argument);
    
    // Invalid subscription operations
    EXPECT_THROW(refactoredClient->subscribeToProperty("", "prop", nullptr), std::invalid_argument);
    EXPECT_THROW(refactoredClient->subscribeToEvent("", "event", nullptr), std::invalid_argument);
    
    // Operations requiring connection
    EXPECT_THROW(refactoredClient->executeCommand("device", "command"), std::runtime_error);
    EXPECT_THROW(refactoredClient->authenticate("method", "creds"), std::runtime_error);
}

TEST_F(ClientIntegrationTest, StatisticsConsistency) {
    // Verify that statistics are consistent across components
    json overallStatus = refactoredClient->getStatusInfo();
    
    // Check that all component stats are included
    EXPECT_TRUE(overallStatus["connection"].contains("connected"));
    EXPECT_TRUE(overallStatus["devices"].contains("cachedDevices"));
    EXPECT_TRUE(overallStatus["execution"].contains("commandsExecuted"));
    EXPECT_TRUE(overallStatus["subscriptions"].contains("propertySubscriptionCount"));
    EXPECT_TRUE(overallStatus["processing"].contains("messagesSent"));
}

TEST_F(ClientIntegrationTest, MemoryManagement) {
    // Test that components are properly cleaned up
    
    // Create subscriptions
    auto callback = [](const std::string&, const std::string&, const json&) {};
    refactoredClient->subscribeToProperty("device1", "prop1", callback);
    refactoredClient->subscribeToProperty("device2", "prop2", callback);
    
    // Verify subscriptions exist
    EXPECT_GT(refactoredClient->getSubscriptionStats()["activePropertySubscriptions"], 0);
    
    // Clear subscriptions
    refactoredClient->getSubscriptionManager()->clearAllSubscriptions();
    
    // Verify cleanup
    EXPECT_EQ(refactoredClient->getSubscriptionStats()["activePropertySubscriptions"], 0);
}

// Performance comparison test (when original is available)
TEST_F(ClientIntegrationTest, DISABLED_PerformanceComparison) {
    // This test would compare performance between original and refactored versions
    // Disabled until original DeviceClient is available for comparison
    
    const int iterations = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform operations with refactored client
    for (int i = 0; i < iterations; ++i) {
        refactoredClient->getDevices();
        refactoredClient->getStatusInfo();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto refactoredTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Refactored client time: " << refactoredTime.count() << " microseconds" << std::endl;
    
    // Compare with original implementation
    // EXPECT_LT(refactoredTime.count(), originalTime.count() * 1.1); // Allow 10% overhead
}

// Stress test for component interactions
TEST_F(ClientIntegrationTest, StressTest) {
    const int numSubscriptions = 100;
    const int numOperations = 1000;
    
    // Create many subscriptions
    auto callback = [](const std::string&, const std::string&, const json&) {};
    
    for (int i = 0; i < numSubscriptions; ++i) {
        std::string deviceId = "device" + std::to_string(i);
        std::string property = "property" + std::to_string(i);
        refactoredClient->subscribeToProperty(deviceId, property, callback);
    }
    
    // Perform many operations
    for (int i = 0; i < numOperations; ++i) {
        refactoredClient->getDevices();
        refactoredClient->getStatusInfo();
        
        if (i % 100 == 0) {
            // Periodically check consistency
            json stats = refactoredClient->getSubscriptionStats();
            EXPECT_EQ(stats["activePropertySubscriptions"], numSubscriptions);
        }
    }
    
    // Cleanup
    refactoredClient->getSubscriptionManager()->clearAllSubscriptions();
    EXPECT_EQ(refactoredClient->getSubscriptionStats()["activePropertySubscriptions"], 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

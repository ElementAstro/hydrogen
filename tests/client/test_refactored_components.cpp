#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "client/connection_manager.h"
#include "client/message_processor.h"
#include "client/device_manager.h"
#include "client/command_executor.h"
#include "client/subscription_manager.h"
#include "client/device_client.h"
#include <thread>
#include <chrono>

using namespace hydrogen;
using namespace testing;
using json = nlohmann::json;

// Mock ConnectionManager for testing - removed for now since ConnectionManager is not virtual
// We'll test the actual implementation instead

// Test ConnectionManager basic functionality
class ConnectionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        connectionManager = std::make_unique<ConnectionManager>();
    }

    std::unique_ptr<ConnectionManager> connectionManager;
};

TEST_F(ConnectionManagerTest, InitialState) {
    EXPECT_FALSE(connectionManager->isConnected());
    
    json status = connectionManager->getConnectionStatus();
    EXPECT_FALSE(status["connected"]);
    EXPECT_TRUE(status["autoReconnectEnabled"]);
    EXPECT_EQ(status["reconnectIntervalMs"], 5000);
}

TEST_F(ConnectionManagerTest, AutoReconnectConfiguration) {
    connectionManager->setAutoReconnect(false, 3000, 5);
    
    json status = connectionManager->getConnectionStatus();
    EXPECT_FALSE(status["autoReconnectEnabled"]);
    EXPECT_EQ(status["reconnectIntervalMs"], 3000);
    EXPECT_EQ(status["maxReconnectAttempts"], 5);
}

// Test MessageProcessor functionality
class MessageProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        connectionManager = std::make_unique<hydrogen::ConnectionManager>();
        messageProcessor = std::make_unique<hydrogen::MessageProcessor>(connectionManager.get());
    }

    std::unique_ptr<hydrogen::ConnectionManager> connectionManager;
    std::unique_ptr<hydrogen::MessageProcessor> messageProcessor;
};

TEST_F(MessageProcessorTest, InitialState) {
    EXPECT_FALSE(messageProcessor->isRunning());
    
    json stats = messageProcessor->getProcessingStats();
    EXPECT_EQ(stats["messagesSent"], 0);
    EXPECT_EQ(stats["messagesReceived"], 0);
    EXPECT_EQ(stats["messagesProcessed"], 0);
    EXPECT_FALSE(stats["running"]);
}

TEST_F(MessageProcessorTest, MessageHandlerRegistration) {
    bool handlerCalled = false;

    messageProcessor->registerMessageHandler(MessageType::EVENT,
        [&handlerCalled](const hydrogen::Message& msg) {
            handlerCalled = true;
            // Avoid unused parameter warning
            (void)msg;
        });

    // Unregister handler
    messageProcessor->unregisterMessageHandler(MessageType::EVENT);

    // Test passes if no exceptions are thrown
    SUCCEED();
}

// Test DeviceManager functionality
class DeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        connectionManager = std::make_unique<hydrogen::ConnectionManager>();
        messageProcessor = std::make_unique<hydrogen::MessageProcessor>(connectionManager.get());
        deviceManager = std::make_unique<hydrogen::DeviceManager>(messageProcessor.get());
    }

    std::unique_ptr<hydrogen::ConnectionManager> connectionManager;
    std::unique_ptr<hydrogen::MessageProcessor> messageProcessor;
    std::unique_ptr<hydrogen::DeviceManager> deviceManager;
};

TEST_F(DeviceManagerTest, InitialState) {
    json devices = deviceManager->getDevices();
    EXPECT_TRUE(devices.is_object());
    EXPECT_EQ(devices.size(), 0);
    
    json stats = deviceManager->getDeviceStats();
    EXPECT_EQ(stats["discoveryRequests"], 0);
    EXPECT_EQ(stats["propertyRequests"], 0);
    EXPECT_EQ(stats["cachedDevices"], 0);
}

TEST_F(DeviceManagerTest, DeviceValidation) {
    EXPECT_FALSE(deviceManager->hasDevice(""));
    EXPECT_FALSE(deviceManager->hasDevice("invalid@device"));
    EXPECT_FALSE(deviceManager->hasDevice("device_with_very_long_name_that_exceeds_maximum_length_limit_for_device_identifiers_in_the_system_which_should_be_rejected_by_validation_logic_because_it_is_too_long_and_could_cause_issues_with_storage_or_processing_systems_that_have_length_constraints"));
    
    // Valid device IDs should not throw
    EXPECT_NO_THROW(deviceManager->hasDevice("valid-device.1"));
    EXPECT_NO_THROW(deviceManager->hasDevice("device_123"));
}

TEST_F(DeviceManagerTest, DeviceInfoManagement) {
    json deviceInfo = {
        {"id", "test-device"},
        {"type", "camera"},
        {"name", "Test Camera"},
        {"status", "online"}
    };
    
    deviceManager->updateDeviceInfo("test-device", deviceInfo);
    
    EXPECT_TRUE(deviceManager->hasDevice("test-device"));
    
    json retrievedInfo = deviceManager->getDeviceInfo("test-device");
    EXPECT_EQ(retrievedInfo["type"], "camera");
    EXPECT_EQ(retrievedInfo["name"], "Test Camera");
    
    deviceManager->removeDevice("test-device");
    EXPECT_FALSE(deviceManager->hasDevice("test-device"));
}

// Test CommandExecutor functionality
class CommandExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        connectionManager = std::make_unique<hydrogen::ConnectionManager>();
        messageProcessor = std::make_unique<hydrogen::MessageProcessor>(connectionManager.get());
        commandExecutor = std::make_unique<hydrogen::CommandExecutor>(messageProcessor.get());
    }

    std::unique_ptr<hydrogen::ConnectionManager> connectionManager;
    std::unique_ptr<hydrogen::MessageProcessor> messageProcessor;
    std::unique_ptr<hydrogen::CommandExecutor> commandExecutor;
};

TEST_F(CommandExecutorTest, InitialState) {
    EXPECT_EQ(commandExecutor->getPendingAsyncCount(), 0);
    
    json stats = commandExecutor->getExecutionStats();
    EXPECT_EQ(stats["commandsExecuted"], 0);
    EXPECT_EQ(stats["asyncCommandsExecuted"], 0);
    EXPECT_EQ(stats["commandErrors"], 0);
}

TEST_F(CommandExecutorTest, RetryParameterConfiguration) {
    // Should not throw
    EXPECT_NO_THROW(commandExecutor->setMessageRetryParams(3, 1000));
}

TEST_F(CommandExecutorTest, AsyncCommandCancellation) {
    // Test cancelling non-existent command
    EXPECT_FALSE(commandExecutor->cancelAsyncCommand("non-existent-id"));
}

// Test SubscriptionManager functionality
class SubscriptionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        connectionManager = std::make_unique<hydrogen::ConnectionManager>();
        messageProcessor = std::make_unique<hydrogen::MessageProcessor>(connectionManager.get());
        subscriptionManager = std::make_unique<hydrogen::SubscriptionManager>(messageProcessor.get());
    }

    std::unique_ptr<hydrogen::ConnectionManager> connectionManager;
    std::unique_ptr<hydrogen::MessageProcessor> messageProcessor;
    std::unique_ptr<hydrogen::SubscriptionManager> subscriptionManager;
};

TEST_F(SubscriptionManagerTest, InitialState) {
    json stats = subscriptionManager->getSubscriptionStats();
    EXPECT_EQ(stats["propertySubscriptionCount"], 0);
    EXPECT_EQ(stats["eventSubscriptionCount"], 0);
    EXPECT_EQ(stats["activePropertySubscriptions"], 0);
    EXPECT_EQ(stats["activeEventSubscriptions"], 0);
}

TEST_F(SubscriptionManagerTest, PropertySubscription) {
    bool callbackCalled = false;
    std::string receivedDeviceId, receivedProperty;
    json receivedValue;
    
    auto callback = [&](const std::string& deviceId, const std::string& property, const json& value) {
        callbackCalled = true;
        receivedDeviceId = deviceId;
        receivedProperty = property;
        receivedValue = value;
    };
    
    subscriptionManager->subscribeToProperty("test-device", "temperature", callback);
    
    EXPECT_TRUE(subscriptionManager->isSubscribedToProperty("test-device", "temperature"));
    EXPECT_FALSE(subscriptionManager->isSubscribedToProperty("test-device", "pressure"));
    
    auto properties = subscriptionManager->getPropertySubscriptions("test-device");
    EXPECT_EQ(properties.size(), 1);
    EXPECT_EQ(properties[0], "temperature");
    
    subscriptionManager->unsubscribeFromProperty("test-device", "temperature");
    EXPECT_FALSE(subscriptionManager->isSubscribedToProperty("test-device", "temperature"));
}

TEST_F(SubscriptionManagerTest, EventSubscription) {
    bool callbackCalled = false;
    
    auto callback = [&](const std::string& deviceId, const std::string& event, const json& details) {
        callbackCalled = true;
    };
    
    subscriptionManager->subscribeToEvent("test-device", "status-change", callback);
    
    EXPECT_TRUE(subscriptionManager->isSubscribedToEvent("test-device", "status-change"));
    
    auto events = subscriptionManager->getEventSubscriptions("test-device");
    EXPECT_EQ(events.size(), 1);
    EXPECT_EQ(events[0], "status-change");
    
    subscriptionManager->clearDeviceSubscriptions("test-device");
    EXPECT_FALSE(subscriptionManager->isSubscribedToEvent("test-device", "status-change"));
}

// Test DeviceClient integration
class DeviceClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client = std::make_unique<hydrogen::DeviceClient>();
    }

    std::unique_ptr<hydrogen::DeviceClient> client;
};

TEST_F(DeviceClientTest, InitialState) {
    EXPECT_FALSE(client->isConnected());

    json status = client->getStatusInfo();
    EXPECT_TRUE(status.contains("connection"));
    EXPECT_TRUE(status.contains("devices"));
    EXPECT_TRUE(status.contains("execution"));
    EXPECT_TRUE(status.contains("subscriptions"));
    EXPECT_TRUE(status.contains("processing"));
}

TEST_F(DeviceClientTest, ComponentAccess) {
    EXPECT_NE(client->getConnectionManager(), nullptr);
    EXPECT_NE(client->getMessageProcessor(), nullptr);
    EXPECT_NE(client->getDeviceManager(), nullptr);
    EXPECT_NE(client->getCommandExecutor(), nullptr);
    EXPECT_NE(client->getSubscriptionManager(), nullptr);
}

TEST_F(DeviceClientTest, ConfigurationMethods) {
    // These should not throw
    EXPECT_NO_THROW(client->setAutoReconnect(false, 3000, 5));
    EXPECT_NO_THROW(client->setMessageRetryParams(3, 1000));
}

// Main test runner
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

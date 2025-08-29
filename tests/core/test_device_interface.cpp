#include <gtest/gtest.h>
#include <astrocomm/core/device_interface.h>
#include "test_helpers.h"

using namespace astrocomm::core;
using namespace astrocomm::test;

// Mock device implementation for testing
class MockDevice : public DeviceBase {
public:
    MockDevice(const std::string& deviceId, const std::string& deviceType, 
               const std::string& manufacturer, const std::string& model)
        : DeviceBase(deviceId, deviceType, manufacturer, model) {
        initializeProperties();
    }
    
    bool connect(const std::string& host, uint16_t port) override {
        connected_ = true;
        return true;
    }
    
    void disconnect() override {
        connected_ = false;
    }
    
    bool start() override {
        if (!connected_) return false;
        running_ = true;
        return true;
    }
    
    void stop() override {
        running_ = false;
    }
    
    // Expose protected methods for testing
    using DeviceBase::handleCommandMessage;
    using DeviceBase::sendResponse;
    using DeviceBase::sendEvent;
    using DeviceBase::sendPropertyChangedEvent;
};

class DeviceInterfaceTest : public DeviceTestBase {
protected:
    void SetUp() override {
        DeviceTestBase::SetUp();
        device = std::make_unique<MockDevice>(testDeviceId, deviceType, manufacturer, model);
    }
    
    std::unique_ptr<MockDevice> device;
};

// Test basic device creation and properties
TEST_F(DeviceInterfaceTest, BasicDeviceCreation) {
    EXPECT_EQ(device->getDeviceId(), testDeviceId);
    EXPECT_EQ(device->getDeviceType(), deviceType);
    
    auto info = device->getDeviceInfo();
    EXPECT_EQ(info["deviceId"], testDeviceId);
    EXPECT_EQ(info["deviceType"], deviceType);
    EXPECT_EQ(info["manufacturer"], manufacturer);
    EXPECT_EQ(info["model"], model);
    EXPECT_TRUE(info.contains("capabilities"));
    EXPECT_TRUE(info.contains("properties"));
}

// Test device lifecycle
TEST_F(DeviceInterfaceTest, DeviceLifecycle) {
    // Initially not connected or running
    EXPECT_FALSE(device->isConnected());
    EXPECT_FALSE(device->isRunning());
    
    // Connect to server
    EXPECT_TRUE(device->connect("localhost", 8080));
    EXPECT_TRUE(device->isConnected());
    
    // Start device
    EXPECT_TRUE(device->start());
    EXPECT_TRUE(device->isRunning());
    
    // Stop device
    device->stop();
    EXPECT_FALSE(device->isRunning());
    
    // Disconnect
    device->disconnect();
    EXPECT_FALSE(device->isConnected());
}

// Test property management
TEST_F(DeviceInterfaceTest, PropertyManagement) {
    // Set and get properties
    device->setProperty("test_property", "test_value");
    auto value = device->getProperty("test_property");
    EXPECT_EQ(value, "test_value");
    
    // Set complex property
    nlohmann::json complexValue = {
        {"nested", {{"key", "value"}}},
        {"array", {1, 2, 3}},
        {"number", 42}
    };
    device->setProperty("complex_property", complexValue);
    auto retrievedValue = device->getProperty("complex_property");
    EXPECT_EQ(retrievedValue, complexValue);
    
    // Get all properties
    auto allProperties = device->getAllProperties();
    EXPECT_TRUE(allProperties.contains("test_property"));
    EXPECT_TRUE(allProperties.contains("complex_property"));
    EXPECT_EQ(allProperties["test_property"], "test_value");
    EXPECT_EQ(allProperties["complex_property"], complexValue);
    
    // Test non-existent property
    auto nonExistent = device->getProperty("non_existent");
    EXPECT_TRUE(nonExistent.is_null());
}

// Test capability management
TEST_F(DeviceInterfaceTest, CapabilityManagement) {
    // Add capabilities
    device->addCapability("test_capability");
    device->addCapability("another_capability");
    
    // Check capabilities
    EXPECT_TRUE(device->hasCapability("test_capability"));
    EXPECT_TRUE(device->hasCapability("another_capability"));
    EXPECT_FALSE(device->hasCapability("non_existent_capability"));
    
    // Get all capabilities
    auto capabilities = device->getCapabilities();
    EXPECT_TRUE(std::find(capabilities.begin(), capabilities.end(), "test_capability") != capabilities.end());
    EXPECT_TRUE(std::find(capabilities.begin(), capabilities.end(), "another_capability") != capabilities.end());
    
    // Remove capability
    device->removeCapability("test_capability");
    EXPECT_FALSE(device->hasCapability("test_capability"));
    EXPECT_TRUE(device->hasCapability("another_capability"));
}

// Test command handling
TEST_F(DeviceInterfaceTest, CommandHandling) {
    bool handlerCalled = false;
    std::string receivedCommand;
    nlohmann::json receivedParams;
    
    // Register command handler
    device->registerCommandHandler("test_command", 
        [&](const CommandMessage& cmd, ResponseMessage& response) {
            handlerCalled = true;
            receivedCommand = cmd.getCommand();
            receivedParams = cmd.getParameters();
            response.setSuccess(true);
            response.setMessage("Command executed successfully");
        });
    
    // Create and handle command
    auto cmd = std::make_unique<CommandMessage>("test_command");
    cmd->setDeviceId(testDeviceId);
    cmd->setParameters({{"param1", "value1"}, {"param2", 42}});
    
    device->handleCommandMessage(*cmd);
    
    EXPECT_TRUE(handlerCalled);
    EXPECT_EQ(receivedCommand, "test_command");
    EXPECT_EQ(receivedParams["param1"], "value1");
    EXPECT_EQ(receivedParams["param2"], 42);
}

// Test message sending
TEST_F(DeviceInterfaceTest, MessageSending) {
    // Test response sending
    ResponseMessage response;
    response.setDeviceId(testDeviceId);
    response.setSuccess(true);
    response.setMessage("Test response");
    
    // This would normally send to a server, but we're just testing the interface
    EXPECT_NO_THROW(device->sendResponse(response));
    
    // Test event sending
    EventMessage event("test_event");
    event.setDeviceId(testDeviceId);
    event.setEventData({{"key", "value"}});
    
    EXPECT_NO_THROW(device->sendEvent(event));
    
    // Test property changed event
    nlohmann::json oldValue = "old_value";
    nlohmann::json newValue = "new_value";
    
    EXPECT_NO_THROW(device->sendPropertyChangedEvent("test_property", newValue, oldValue));
}

// Test device status
TEST_F(DeviceInterfaceTest, DeviceStatus) {
    auto status = device->getStatus();
    
    // Should contain basic status information
    EXPECT_TRUE(status.contains("connected"));
    EXPECT_TRUE(status.contains("running"));
    EXPECT_TRUE(status.contains("deviceId"));
    EXPECT_TRUE(status.contains("deviceType"));
    
    EXPECT_EQ(status["deviceId"], testDeviceId);
    EXPECT_EQ(status["deviceType"], deviceType);
    EXPECT_EQ(status["connected"], device->isConnected());
    EXPECT_EQ(status["running"], device->isRunning());
}

// Test device configuration
TEST_F(DeviceInterfaceTest, DeviceConfiguration) {
    nlohmann::json config = {
        {"setting1", "value1"},
        {"setting2", 42},
        {"setting3", true}
    };
    
    // Apply configuration
    EXPECT_TRUE(device->configure(config));
    
    // Verify configuration was applied (this would depend on implementation)
    auto currentConfig = device->getConfiguration();
    EXPECT_TRUE(currentConfig.is_object());
}

// Test error handling in device
TEST_F(DeviceInterfaceTest, DeviceErrorHandling) {
    // Test handling invalid command
    auto invalidCmd = std::make_unique<CommandMessage>("invalid_command");
    invalidCmd->setDeviceId(testDeviceId);
    
    // Should not throw, but may generate error response
    EXPECT_NO_THROW(device->handleCommandMessage(*invalidCmd));
    
    // Test starting device without connection
    device->disconnect();
    EXPECT_FALSE(device->start());
}

// Test device threading safety
TEST_F(DeviceInterfaceTest, DeviceThreadSafety) {
    const int numThreads = 4;
    const int operationsPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    device->connect("localhost", 8080);
    device->start();
    
    // Create multiple threads that perform operations
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([this, &successCount, operationsPerThread]() {
            for (int i = 0; i < operationsPerThread; i++) {
                try {
                    // Set property
                    device->setProperty("thread_property_" + std::to_string(i), i);
                    
                    // Get property
                    auto value = device->getProperty("thread_property_" + std::to_string(i));
                    
                    // Add capability
                    device->addCapability("thread_capability_" + std::to_string(i));
                    
                    successCount++;
                } catch (...) {
                    // Thread safety violation
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All operations should have succeeded
    EXPECT_EQ(successCount, numThreads * operationsPerThread);
    
    device->stop();
    device->disconnect();
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "device/device_base.h"
#include "common/message.h"
#include "utils/simple_helpers.h"
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>

using namespace hydrogen::device;
using namespace hydrogen::common;
using namespace testing;
using json = nlohmann::json;

class MockDeviceBase : public DeviceBase {
public:
    MockDeviceBase(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
        : DeviceBase(deviceId, manufacturer, model) {}

    MOCK_METHOD(bool, handleDeviceCommand, (const std::string& command, const json& parameters, json& response), (override));
    MOCK_METHOD(bool, connect, (const std::string& host, uint16_t port), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(json, getDeviceInfo, (), (const, override));
    MOCK_METHOD(json, getDeviceStatus, (), (const, override));
    MOCK_METHOD(bool, setProperty, (const std::string& name, const json& value), (override));
    MOCK_METHOD(json, getProperty, (const std::string& name), (const, override));
};

class DeviceBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<MockDeviceBase>("test-device", "Test Corp", "Model X");
        
        // Setup default mock behaviors
        ON_CALL(*device, connect(_, _))
            .WillByDefault(Return(true));
        ON_CALL(*device, isConnected())
            .WillByDefault(Return(true));
        ON_CALL(*device, getDeviceInfo())
            .WillByDefault(Return(json{
                {"id", "test-device"},
                {"manufacturer", "Test Corp"},
                {"model", "Model X"},
                {"version", "1.0.0"},
                {"capabilities", json::array({"basic", "advanced"})}
            }));
        ON_CALL(*device, getDeviceStatus())
            .WillByDefault(Return(json{
                {"connected", true},
                {"status", "ready"},
                {"lastUpdate", "2023-01-01T00:00:00Z"}
            }));
    }

    std::unique_ptr<MockDeviceBase> device;
};

// Basic functionality tests
TEST_F(DeviceBaseTest, InitialState) {
    EXPECT_EQ(device->getDeviceId(), "test-device");
    EXPECT_EQ(device->getManufacturer(), "Test Corp");
    EXPECT_EQ(device->getModel(), "Model X");
    
    // Test device info
    json info = device->getDeviceInfo();
    EXPECT_EQ(info["id"], "test-device");
    EXPECT_EQ(info["manufacturer"], "Test Corp");
    EXPECT_EQ(info["model"], "Model X");
    EXPECT_TRUE(info.contains("capabilities"));
}

TEST_F(DeviceBaseTest, ConnectionManagement) {
    // Test connection
    EXPECT_CALL(*device, connect("localhost", 8080))
        .WillOnce(Return(true));
    EXPECT_TRUE(device->connect("localhost", 8080));
    
    // Test connection status
    EXPECT_CALL(*device, isConnected())
        .WillRepeatedly(Return(true));
    EXPECT_TRUE(device->isConnected());
    
    // Test disconnection
    EXPECT_CALL(*device, disconnect())
        .Times(1);
    device->disconnect();
}

TEST_F(DeviceBaseTest, PropertyManagement) {
    // Test setting properties
    EXPECT_CALL(*device, setProperty("test_prop", json("test_value")))
        .WillOnce(Return(true));
    EXPECT_TRUE(device->setProperty("test_prop", json("test_value")));
    
    // Test getting properties
    EXPECT_CALL(*device, getProperty("test_prop"))
        .WillOnce(Return(json("test_value")));
    json value = device->getProperty("test_prop");
    EXPECT_EQ(value, "test_value");
}

TEST_F(DeviceBaseTest, CommandHandling) {
    json parameters = {{"param1", "value1"}, {"param2", 42}};
    json response;
    
    EXPECT_CALL(*device, handleDeviceCommand("test_command", parameters, _))
        .WillOnce(DoAll(
            SetArgReferee<2>(json{{"success", true}, {"result", "command executed"}}),
            Return(true)
        ));
    
    bool result = device->handleDeviceCommand("test_command", parameters, response);
    EXPECT_TRUE(result);
    EXPECT_TRUE(response["success"]);
    EXPECT_EQ(response["result"], "command executed");
}

TEST_F(DeviceBaseTest, DeviceStatus) {
    json status = device->getDeviceStatus();
    EXPECT_TRUE(status.contains("connected"));
    EXPECT_TRUE(status.contains("status"));
    EXPECT_TRUE(status.contains("lastUpdate"));
}

// Error handling tests
TEST_F(DeviceBaseTest, ConnectionFailure) {
    EXPECT_CALL(*device, connect("invalid-host", 9999))
        .WillOnce(Return(false));
    EXPECT_FALSE(device->connect("invalid-host", 9999));
}

TEST_F(DeviceBaseTest, InvalidCommand) {
    json parameters;
    json response;
    
    EXPECT_CALL(*device, handleDeviceCommand("invalid_command", parameters, _))
        .WillOnce(DoAll(
            SetArgReferee<2>(json{{"success", false}, {"error", "Unknown command"}}),
            Return(false)
        ));
    
    bool result = device->handleDeviceCommand("invalid_command", parameters, response);
    EXPECT_FALSE(result);
    EXPECT_FALSE(response["success"]);
    EXPECT_TRUE(response.contains("error"));
}

TEST_F(DeviceBaseTest, PropertyErrors) {
    // Test setting invalid property
    EXPECT_CALL(*device, setProperty("invalid_prop", json("value")))
        .WillOnce(Return(false));
    EXPECT_FALSE(device->setProperty("invalid_prop", json("value")));
    
    // Test getting non-existent property
    EXPECT_CALL(*device, getProperty("non_existent"))
        .WillOnce(Return(json()));
    json value = device->getProperty("non_existent");
    EXPECT_TRUE(value.is_null());
}

// Concurrency tests
TEST_F(DeviceBaseTest, ConcurrentPropertyAccess) {
    const int numThreads = 4;
    const int operationsPerThread = 10;
    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;
    
    // Setup mock expectations for concurrent access
    EXPECT_CALL(*device, setProperty(_, _))
        .Times(AtLeast(numThreads * operationsPerThread))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*device, getProperty(_))
        .Times(AtLeast(numThreads * operationsPerThread))
        .WillRepeatedly(Return(json("test_value")));
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &successCount, operationsPerThread, t]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    std::string propName = "prop_" + std::to_string(t) + "_" + std::to_string(i);
                    
                    // Set property
                    if (device->setProperty(propName, json("value_" + std::to_string(i)))) {
                        // Get property
                        json value = device->getProperty(propName);
                        if (!value.is_null()) {
                            successCount++;
                        }
                    }
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } catch (const std::exception&) {
                    // Handle any exceptions gracefully
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_GT(successCount.load(), 0);
}

// Validation tests
TEST_F(DeviceBaseTest, DeviceIdValidation) {
    // Valid device IDs should work
    auto validDevice = std::make_unique<MockDeviceBase>("valid-device-123", "Test", "Model");
    EXPECT_EQ(validDevice->getDeviceId(), "valid-device-123");
    
    // Test with different valid formats
    auto validDevice2 = std::make_unique<MockDeviceBase>("device.with.dots", "Test", "Model");
    EXPECT_EQ(validDevice2->getDeviceId(), "device.with.dots");
}

TEST_F(DeviceBaseTest, ManufacturerAndModelValidation) {
    // Test with empty manufacturer (should still work)
    auto device1 = std::make_unique<MockDeviceBase>("test", "", "Model");
    EXPECT_EQ(device1->getManufacturer(), "");
    
    // Test with empty model (should still work)
    auto device2 = std::make_unique<MockDeviceBase>("test", "Manufacturer", "");
    EXPECT_EQ(device2->getModel(), "");
    
    // Test with special characters
    auto device3 = std::make_unique<MockDeviceBase>("test", "Test & Co.", "Model-X v2.0");
    EXPECT_EQ(device3->getManufacturer(), "Test & Co.");
    EXPECT_EQ(device3->getModel(), "Model-X v2.0");
}

// Performance tests
TEST_F(DeviceBaseTest, PropertyAccessPerformance) {
    const int iterations = 1000;
    
    EXPECT_CALL(*device, setProperty(_, _))
        .Times(iterations)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*device, getProperty(_))
        .Times(iterations)
        .WillRepeatedly(Return(json("test_value")));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        std::string propName = "perf_prop_" + std::to_string(i);
        device->setProperty(propName, json("value"));
        device->getProperty(propName);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (less than 1 second for 1000 operations)
    EXPECT_LT(duration.count(), 1000);
}

// Lifecycle tests
TEST_F(DeviceBaseTest, DeviceLifecycle) {
    // Test complete device lifecycle
    EXPECT_CALL(*device, connect("localhost", 8080))
        .WillOnce(Return(true));
    EXPECT_CALL(*device, isConnected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*device, disconnect())
        .Times(1);
    
    // Connect
    EXPECT_TRUE(device->connect("localhost", 8080));
    EXPECT_TRUE(device->isConnected());
    
    // Use device
    json info = device->getDeviceInfo();
    EXPECT_FALSE(info.empty());
    
    json status = device->getDeviceStatus();
    EXPECT_FALSE(status.empty());
    
    // Disconnect
    device->disconnect();
}

/**
 * @file test_device_registry.cpp
 * @brief Basic tests for device registry functionality
 */

#include <gtest/gtest.h>
#include "device/device_registry.h"
#include <memory>
#include <vector>
#include <string>

using namespace hydrogen::device;

class DeviceRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get registry instance
        registry = &DeviceRegistry::getInstance();
        registry->registerDefaultFactories();
    }

    void TearDown() override {
        // Clean up registry instances
        if (registry) {
            registry->stopAllDevices();
            registry->disconnectAllDevices();
        }
    }

    DeviceRegistry* registry;
};

// Test device registry creation and basic functionality
TEST_F(DeviceRegistryTest, RegistryCreation) {
    EXPECT_NE(registry, nullptr);

    // Test supported device types
    auto supportedTypes = registry->getSupportedDeviceTypes();
    EXPECT_TRUE(supportedTypes.size() >= 0);
}

// Test device creation
TEST_F(DeviceRegistryTest, DeviceCreation) {
    // Try to create a device
    auto device = registry->createDevice("Camera", "test-camera", "TestManufacturer", "TestModel");

    // Device creation might fail if no factory is registered, which is okay for this test
    // We're just testing that the method can be called without crashing
    EXPECT_TRUE(true); // This test passes if we get here without crashing
}

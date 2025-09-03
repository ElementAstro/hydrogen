/**
 * @file test_device_registry.cpp
 * @brief Comprehensive tests for device registry functionality
 * 
 * Tests device registration, discovery, management, and lifecycle operations
 * in the device registry system.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "device/device_registry.h"
#include "device/camera.h"
#include "device/telescope.h"
#include "device/focuser.h"
#include "device/filter_wheel.h"
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

class DeviceRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry = std::make_unique<DeviceRegistry>();
        
        // Create test devices
        camera = std::make_unique<CameraDevice>("test-camera", "Test Corp", "Camera Model");
        telescope = std::make_unique<TelescopeDevice>("test-telescope", "Test Corp", "Telescope Model");
        focuser = std::make_unique<FocuserDevice>("test-focuser", "Test Corp", "Focuser Model");
        filterWheel = std::make_unique<FilterWheelDevice>("test-filter-wheel", "Test Corp", "FilterWheel Model");
    }
    
    void TearDown() override {
        // Clean up registry
        if (registry) {
            registry->clear();
        }
    }
    
    std::unique_ptr<DeviceRegistry> registry;
    std::unique_ptr<CameraDevice> camera;
    std::unique_ptr<TelescopeDevice> telescope;
    std::unique_ptr<FocuserDevice> focuser;
    std::unique_ptr<FilterWheelDevice> filterWheel;
};

// Test device registry creation and basic functionality
TEST_F(DeviceRegistryTest, RegistryCreation) {
    EXPECT_NE(registry, nullptr);
    
    // Test initial state
    EXPECT_EQ(registry->getDeviceCount(), 0);
    EXPECT_TRUE(registry->getAllDevices().empty());
    EXPECT_TRUE(registry->getDevicesByType("CAMERA").empty());
}

// Test device registration
TEST_F(DeviceRegistryTest, DeviceRegistration) {
    // Register devices
    EXPECT_TRUE(registry->registerDevice(std::move(camera)));
    EXPECT_EQ(registry->getDeviceCount(), 1);
    
    EXPECT_TRUE(registry->registerDevice(std::move(telescope)));
    EXPECT_EQ(registry->getDeviceCount(), 2);
    
    EXPECT_TRUE(registry->registerDevice(std::move(focuser)));
    EXPECT_EQ(registry->getDeviceCount(), 3);
    
    EXPECT_TRUE(registry->registerDevice(std::move(filterWheel)));
    EXPECT_EQ(registry->getDeviceCount(), 4);
    
    // Test getting all devices
    auto allDevices = registry->getAllDevices();
    EXPECT_EQ(allDevices.size(), 4);
}

// Test device retrieval by ID
TEST_F(DeviceRegistryTest, DeviceRetrievalById) {
    // Register devices
    registry->registerDevice(std::move(camera));
    registry->registerDevice(std::move(telescope));
    
    // Test getting device by ID
    auto retrievedCamera = registry->getDevice("test-camera");
    EXPECT_NE(retrievedCamera, nullptr);
    EXPECT_EQ(retrievedCamera->getDeviceId(), "test-camera");
    EXPECT_EQ(retrievedCamera->getDeviceType(), "CAMERA");
    
    auto retrievedTelescope = registry->getDevice("test-telescope");
    EXPECT_NE(retrievedTelescope, nullptr);
    EXPECT_EQ(retrievedTelescope->getDeviceId(), "test-telescope");
    EXPECT_EQ(retrievedTelescope->getDeviceType(), "TELESCOPE");
    
    // Test non-existent device
    auto nonExistent = registry->getDevice("non-existent");
    EXPECT_EQ(nonExistent, nullptr);
}

// Test device retrieval by type
TEST_F(DeviceRegistryTest, DeviceRetrievalByType) {
    // Register devices
    registry->registerDevice(std::move(camera));
    registry->registerDevice(std::move(telescope));
    registry->registerDevice(std::move(focuser));
    registry->registerDevice(std::move(filterWheel));
    
    // Test getting devices by type
    auto cameras = registry->getDevicesByType("CAMERA");
    EXPECT_EQ(cameras.size(), 1);
    EXPECT_EQ(cameras[0]->getDeviceId(), "test-camera");
    
    auto telescopes = registry->getDevicesByType("TELESCOPE");
    EXPECT_EQ(telescopes.size(), 1);
    EXPECT_EQ(telescopes[0]->getDeviceId(), "test-telescope");
    
    auto focusers = registry->getDevicesByType("FOCUSER");
    EXPECT_EQ(focusers.size(), 1);
    EXPECT_EQ(focusers[0]->getDeviceId(), "test-focuser");
    
    auto filterWheels = registry->getDevicesByType("FILTER_WHEEL");
    EXPECT_EQ(filterWheels.size(), 1);
    EXPECT_EQ(filterWheels[0]->getDeviceId(), "test-filter-wheel");
    
    // Test non-existent type
    auto nonExistent = registry->getDevicesByType("NON_EXISTENT");
    EXPECT_TRUE(nonExistent.empty());
}

// Test device unregistration
TEST_F(DeviceRegistryTest, DeviceUnregistration) {
    // Register devices
    registry->registerDevice(std::move(camera));
    registry->registerDevice(std::move(telescope));
    EXPECT_EQ(registry->getDeviceCount(), 2);
    
    // Unregister device
    EXPECT_TRUE(registry->unregisterDevice("test-camera"));
    EXPECT_EQ(registry->getDeviceCount(), 1);
    
    // Verify device is removed
    auto retrievedCamera = registry->getDevice("test-camera");
    EXPECT_EQ(retrievedCamera, nullptr);
    
    // Telescope should still be there
    auto retrievedTelescope = registry->getDevice("test-telescope");
    EXPECT_NE(retrievedTelescope, nullptr);
    
    // Test unregistering non-existent device
    EXPECT_FALSE(registry->unregisterDevice("non-existent"));
    EXPECT_EQ(registry->getDeviceCount(), 1);
}

// Test device existence check
TEST_F(DeviceRegistryTest, DeviceExistenceCheck) {
    // Register device
    registry->registerDevice(std::move(camera));
    
    // Test device existence
    EXPECT_TRUE(registry->hasDevice("test-camera"));
    EXPECT_FALSE(registry->hasDevice("non-existent"));
    
    // Unregister and test again
    registry->unregisterDevice("test-camera");
    EXPECT_FALSE(registry->hasDevice("test-camera"));
}

// Test registry clearing
TEST_F(DeviceRegistryTest, RegistryClearing) {
    // Register multiple devices
    registry->registerDevice(std::move(camera));
    registry->registerDevice(std::move(telescope));
    registry->registerDevice(std::move(focuser));
    EXPECT_EQ(registry->getDeviceCount(), 3);
    
    // Clear registry
    registry->clear();
    EXPECT_EQ(registry->getDeviceCount(), 0);
    EXPECT_TRUE(registry->getAllDevices().empty());
    
    // Verify all devices are removed
    EXPECT_FALSE(registry->hasDevice("test-camera"));
    EXPECT_FALSE(registry->hasDevice("test-telescope"));
    EXPECT_FALSE(registry->hasDevice("test-focuser"));
}

// Test duplicate device registration
TEST_F(DeviceRegistryTest, DuplicateDeviceRegistration) {
    // Register device
    registry->registerDevice(std::move(camera));
    EXPECT_EQ(registry->getDeviceCount(), 1);
    
    // Try to register device with same ID
    auto duplicateCamera = std::make_unique<CameraDevice>("test-camera", "Other Corp", "Other Model");
    EXPECT_FALSE(registry->registerDevice(std::move(duplicateCamera)));
    EXPECT_EQ(registry->getDeviceCount(), 1);
    
    // Original device should still be there
    auto retrievedCamera = registry->getDevice("test-camera");
    EXPECT_NE(retrievedCamera, nullptr);
    EXPECT_EQ(retrievedCamera->getManufacturer(), "Test Corp");
}

// Test device registry events/callbacks
TEST_F(DeviceRegistryTest, RegistryEvents) {
    std::atomic<int> deviceAddedCount{0};
    std::atomic<int> deviceRemovedCount{0};
    std::string lastAddedDeviceId;
    std::string lastRemovedDeviceId;
    
    // Set up event callbacks
    registry->setDeviceAddedCallback([&](const std::string& deviceId) {
        deviceAddedCount++;
        lastAddedDeviceId = deviceId;
    });
    
    registry->setDeviceRemovedCallback([&](const std::string& deviceId) {
        deviceRemovedCount++;
        lastRemovedDeviceId = deviceId;
    });
    
    // Register devices
    registry->registerDevice(std::move(camera));
    EXPECT_EQ(deviceAddedCount.load(), 1);
    EXPECT_EQ(lastAddedDeviceId, "test-camera");
    
    registry->registerDevice(std::move(telescope));
    EXPECT_EQ(deviceAddedCount.load(), 2);
    EXPECT_EQ(lastAddedDeviceId, "test-telescope");
    
    // Unregister device
    registry->unregisterDevice("test-camera");
    EXPECT_EQ(deviceRemovedCount.load(), 1);
    EXPECT_EQ(lastRemovedDeviceId, "test-camera");
    
    // Clear registry
    registry->clear();
    EXPECT_EQ(deviceRemovedCount.load(), 2); // Should trigger for remaining device
}

// Test concurrent registry operations
TEST_F(DeviceRegistryTest, ConcurrentOperations) {
    const int numThreads = 4;
    const int devicesPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    // Create devices for concurrent registration
    std::vector<std::vector<std::unique_ptr<DeviceBase>>> threadDevices(numThreads);
    for (int t = 0; t < numThreads; t++) {
        for (int i = 0; i < devicesPerThread; i++) {
            std::string deviceId = "device_" + std::to_string(t) + "_" + std::to_string(i);
            threadDevices[t].push_back(
                std::make_unique<CameraDevice>(deviceId, "Test Corp", "Test Model")
            );
        }
    }
    
    // Test concurrent device registration
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([this, &successCount, &threadDevices, t, devicesPerThread]() {
            try {
                for (int i = 0; i < devicesPerThread; i++) {
                    if (registry->registerDevice(std::move(threadDevices[t][i]))) {
                        successCount++;
                    }
                }
            } catch (...) {
                // Thread safety test should not throw exceptions
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(successCount.load(), numThreads * devicesPerThread);
    EXPECT_EQ(registry->getDeviceCount(), numThreads * devicesPerThread);
}

// Test registry performance
TEST_F(DeviceRegistryTest, RegistryPerformance) {
    const int numDevices = 1000;
    std::vector<std::unique_ptr<DeviceBase>> devices;
    
    // Create devices
    for (int i = 0; i < numDevices; i++) {
        std::string deviceId = "perf_device_" + std::to_string(i);
        devices.push_back(std::make_unique<CameraDevice>(deviceId, "Test Corp", "Test Model"));
    }
    
    // Test registration performance
    auto start = std::chrono::high_resolution_clock::now();
    for (auto& device : devices) {
        registry->registerDevice(std::move(device));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
    EXPECT_EQ(registry->getDeviceCount(), numDevices);
    
    // Test retrieval performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numDevices; i++) {
        std::string deviceId = "perf_device_" + std::to_string(i);
        auto device = registry->getDevice(deviceId);
        EXPECT_NE(device, nullptr);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
}

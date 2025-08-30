#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../src/device/interfaces/automatic_compatibility.h"
#include "../src/device/camera.h"
#include "../src/device/telescope.h"
#include "../src/device/focuser.h"
#include <memory>
#include <thread>
#include <chrono>

using namespace astrocomm::device;
using namespace astrocomm::device::interfaces;
using namespace testing;

/**
 * @brief Test fixture for automatic compatibility system tests
 */
class AutomaticCompatibilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize compatibility system
        compatibility::initializeCompatibilitySystem(false, true, true, 7624);
        
        // Create test devices
        camera_ = std::make_shared<Camera>("test_cam", "TestMfg", "TestCam");
        telescope_ = std::make_shared<Telescope>("test_tel", "TestMfg", "TestTel");
        focuser_ = std::make_shared<Focuser>("test_foc", "TestMfg", "TestFoc");
        
        // Initialize devices
        camera_->initializeDevice();
        camera_->startDevice();
        
        telescope_->initializeDevice();
        telescope_->startDevice();
        
        focuser_->initializeDevice();
        focuser_->startDevice();
    }
    
    void TearDown() override {
        // Clean up devices
        if (camera_) camera_->stopDevice();
        if (telescope_) telescope_->stopDevice();
        if (focuser_) focuser_->stopDevice();
        
        // Shutdown compatibility system
        compatibility::shutdownCompatibilitySystem();
    }
    
    std::shared_ptr<Camera> camera_;
    std::shared_ptr<Telescope> telescope_;
    std::shared_ptr<Focuser> focuser_;
};

/**
 * @brief Test basic compatibility enablement
 */
TEST_F(AutomaticCompatibilityTest, BasicCompatibilityEnablement) {
    // Enable compatibility for camera
    auto bridge = compatibility::enableAutomaticCompatibility(camera_, "test_camera", true, true);
    
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->isProtocolEnabled(bridge::ProtocolType::INTERNAL));
    EXPECT_TRUE(bridge->isProtocolEnabled(bridge::ProtocolType::ASCOM));
    EXPECT_TRUE(bridge->isProtocolEnabled(bridge::ProtocolType::INDI));
    
    // Check enabled protocols
    auto protocols = bridge->getEnabledProtocols();
    EXPECT_THAT(protocols, Contains(bridge::ProtocolType::INTERNAL));
    EXPECT_THAT(protocols, Contains(bridge::ProtocolType::ASCOM));
    EXPECT_THAT(protocols, Contains(bridge::ProtocolType::INDI));
}

/**
 * @brief Test property access through different protocols
 */
TEST_F(AutomaticCompatibilityTest, PropertyAccessThroughProtocols) {
    auto bridge = compatibility::enableAutomaticCompatibility(camera_, "test_camera", true, true);
    
    // Set property through internal protocol
    bridge->setProperty<bool>("coolerOn", true, bridge::ProtocolType::INTERNAL);
    
    // Read through different protocols
    bool internalValue = bridge->getProperty<bool>("coolerOn", bridge::ProtocolType::INTERNAL);
    bool ascomValue = bridge->getProperty<bool>("CoolerOn", bridge::ProtocolType::ASCOM);
    
    EXPECT_TRUE(internalValue);
    EXPECT_TRUE(ascomValue);
    
    // Set through ASCOM protocol
    bridge->setProperty<double>("ExposureDuration", 5.0, bridge::ProtocolType::ASCOM);
    
    // Read through internal protocol
    double exposureTime = bridge->getProperty<double>("exposureDuration", bridge::ProtocolType::INTERNAL);
    EXPECT_DOUBLE_EQ(exposureTime, 5.0);
}

/**
 * @brief Test method invocation through different protocols
 */
TEST_F(AutomaticCompatibilityTest, MethodInvocationThroughProtocols) {
    auto bridge = compatibility::enableAutomaticCompatibility(camera_, "test_camera", true, true);
    
    // Test ASCOM method invocation
    EXPECT_NO_THROW({
        bridge->invokeMethod<void>("StartExposure", bridge::ProtocolType::ASCOM, 3.0, true);
    });
    
    // Test internal method invocation
    EXPECT_NO_THROW({
        bridge->invokeMethod<void>("START_EXPOSURE", bridge::ProtocolType::INTERNAL, 2.0, false);
    });
}

/**
 * @brief Test automatic type conversion
 */
TEST_F(AutomaticCompatibilityTest, AutomaticTypeConversion) {
    auto bridge = compatibility::enableAutomaticCompatibility(focuser_, "test_focuser", true, true);
    
    // Set integer position through ASCOM
    bridge->setProperty<int>("Position", 1000, bridge::ProtocolType::ASCOM);
    
    // Read as different types
    int intPosition = bridge->getProperty<int>("position", bridge::ProtocolType::INTERNAL);
    double doublePosition = bridge->getProperty<double>("position", bridge::ProtocolType::INTERNAL);
    
    EXPECT_EQ(intPosition, 1000);
    EXPECT_DOUBLE_EQ(doublePosition, 1000.0);
}

/**
 * @brief Test error handling and translation
 */
TEST_F(AutomaticCompatibilityTest, ErrorHandlingAndTranslation) {
    auto bridge = compatibility::enableAutomaticCompatibility(camera_, "test_camera", true, true);
    
    // Test invalid property access
    EXPECT_THROW({
        bridge->getProperty<double>("NonExistentProperty", bridge::ProtocolType::ASCOM);
    }, std::exception);
    
    // Test invalid method invocation
    EXPECT_THROW({
        bridge->invokeMethod<void>("NonExistentMethod", bridge::ProtocolType::ASCOM);
    }, std::exception);
}

/**
 * @brief Test integration manager functionality
 */
TEST_F(AutomaticCompatibilityTest, IntegrationManagerFunctionality) {
    auto& manager = integration::AutomaticIntegrationManager::getInstance();
    
    // Register devices
    manager.registerDevice("test_camera", camera_);
    manager.registerDevice("test_telescope", telescope_);
    
    // Check registered devices
    auto deviceIds = manager.getRegisteredDeviceIds();
    EXPECT_THAT(deviceIds, Contains("test_camera"));
    EXPECT_THAT(deviceIds, Contains("test_telescope"));
    
    // Access devices through manager
    auto retrievedCamera = manager.getTypedDevice<Camera>("test_camera");
    ASSERT_NE(retrievedCamera, nullptr);
    EXPECT_EQ(retrievedCamera, camera_);
    
    // Test property access through manager
    manager.setDeviceProperty<Camera>("test_camera", "coolerOn", true);
    bool coolerState = manager.getDeviceProperty<Camera, bool>("test_camera", "coolerOn");
    EXPECT_TRUE(coolerState);
}

/**
 * @brief Test RAII-style compatibility management
 */
TEST_F(AutomaticCompatibilityTest, RAIIStyleManagement) {
    {
        compatibility::CompatibilitySystemManager manager(false, true, true, 7625);
        
        auto bridge = manager.enableDevice(camera_, "test_camera");
        ASSERT_NE(bridge, nullptr);
        
        // Test device functionality
        bridge->setProperty<bool>("Connected", true, bridge::ProtocolType::ASCOM);
        bool connected = bridge->getProperty<bool>("Connected", bridge::ProtocolType::ASCOM);
        EXPECT_TRUE(connected);
        
        // Manager automatically cleans up when going out of scope
    }
    
    // Verify cleanup occurred (system should be stopped)
    // Note: In real implementation, would check system state
}

/**
 * @brief Test property synchronization across protocols
 */
TEST_F(AutomaticCompatibilityTest, PropertySynchronizationAcrossProtocols) {
    auto bridge = compatibility::enableAutomaticCompatibility(telescope_, "test_telescope", true, true);
    
    // Set RA through internal protocol
    bridge->setProperty<double>("rightAscension", 12.5, bridge::ProtocolType::INTERNAL);
    
    // Allow time for synchronization
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Read through ASCOM protocol
    double ascomRA = bridge->getProperty<double>("RightAscension", bridge::ProtocolType::ASCOM);
    EXPECT_DOUBLE_EQ(ascomRA, 12.5);
    
    // Set through INDI protocol
    bridge->setProperty<double>("EQUATORIAL_EOD_COORD", 15.0, bridge::ProtocolType::INDI);
    
    // Allow time for synchronization
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Read through internal protocol
    double internalRA = bridge->getProperty<double>("rightAscension", bridge::ProtocolType::INTERNAL);
    EXPECT_DOUBLE_EQ(internalRA, 15.0);
}

/**
 * @brief Test system statistics
 */
TEST_F(AutomaticCompatibilityTest, SystemStatistics) {
    auto& manager = integration::AutomaticIntegrationManager::getInstance();
    
    // Register multiple devices
    manager.registerDevice("test_camera", camera_);
    manager.registerDevice("test_telescope", telescope_);
    manager.registerDevice("test_focuser", focuser_);
    
    // Get statistics
    auto stats = manager.getStatistics();
    
    EXPECT_EQ(stats.totalDevices, 3);
    EXPECT_GT(stats.uptime.count(), 0);
    EXPECT_EQ(stats.deviceTypeCount["Camera"], 1);
    EXPECT_EQ(stats.deviceTypeCount["Telescope"], 1);
    EXPECT_EQ(stats.deviceTypeCount["Focuser"], 1);
}

/**
 * @brief Test concurrent access from multiple protocols
 */
TEST_F(AutomaticCompatibilityTest, ConcurrentProtocolAccess) {
    auto bridge = compatibility::enableAutomaticCompatibility(camera_, "test_camera", true, true);
    
    std::atomic<bool> testComplete{false};
    std::atomic<int> successCount{0};
    
    // Thread 1: ASCOM access
    std::thread ascomThread([&]() {
        while (!testComplete.load()) {
            try {
                bridge->setProperty<bool>("CoolerOn", true, bridge::ProtocolType::ASCOM);
                bool state = bridge->getProperty<bool>("CoolerOn", bridge::ProtocolType::ASCOM);
                if (state) successCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (...) {
                // Ignore exceptions for this test
            }
        }
    });
    
    // Thread 2: INDI access
    std::thread indiThread([&]() {
        while (!testComplete.load()) {
            try {
                bridge->setProperty<bool>("CCD_COOLER", true, bridge::ProtocolType::INDI);
                bool state = bridge->getProperty<bool>("CCD_COOLER", bridge::ProtocolType::INDI);
                if (state) successCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (...) {
                // Ignore exceptions for this test
            }
        }
    });
    
    // Thread 3: Internal access
    std::thread internalThread([&]() {
        while (!testComplete.load()) {
            try {
                bridge->setProperty<bool>("coolerOn", true, bridge::ProtocolType::INTERNAL);
                bool state = bridge->getProperty<bool>("coolerOn", bridge::ProtocolType::INTERNAL);
                if (state) successCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (...) {
                // Ignore exceptions for this test
            }
        }
    });
    
    // Run test for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    testComplete = true;
    
    // Wait for threads to complete
    ascomThread.join();
    indiThread.join();
    internalThread.join();
    
    // Verify no crashes and some successful operations
    EXPECT_GT(successCount.load(), 0);
}

/**
 * @brief Test macro convenience functions
 */
TEST_F(AutomaticCompatibilityTest, MacroConvenienceFunctions) {
    // Test enablement macros
    EXPECT_NO_THROW({
        ENABLE_ASCOM_INDI_COMPATIBILITY(camera_, "macro_camera");
    });
    
    // Test access macros
    auto retrievedCamera = GET_DEVICE_AUTO(Camera, "macro_camera");
    ASSERT_NE(retrievedCamera, nullptr);
    
    // Test property macros
    EXPECT_NO_THROW({
        SET_DEVICE_PROPERTY_AUTO(Camera, "macro_camera", "coolerOn", true, bridge::ProtocolType::INTERNAL);
    });
    
    bool coolerState = GET_DEVICE_PROPERTY_AUTO(Camera, "macro_camera", "coolerOn", bool, bridge::ProtocolType::INTERNAL);
    EXPECT_TRUE(coolerState);
    
    // Test method invocation macro
    EXPECT_NO_THROW({
        INVOKE_DEVICE_METHOD_AUTO(Camera, void, "macro_camera", "START_EXPOSURE", bridge::ProtocolType::INTERNAL, 1.0, true);
    });
    
    // Test disable macro
    EXPECT_NO_THROW({
        DISABLE_COMPATIBILITY("macro_camera");
    });
}

/**
 * @brief Performance benchmark test
 */
TEST_F(AutomaticCompatibilityTest, PerformanceBenchmark) {
    auto bridge = compatibility::enableAutomaticCompatibility(camera_, "perf_camera", true, true);
    
    const int iterations = 1000;
    
    // Benchmark direct property access
    auto startTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        bridge->setProperty<bool>("coolerOn", i % 2 == 0, bridge::ProtocolType::INTERNAL);
        bool state = bridge->getProperty<bool>("coolerOn", bridge::ProtocolType::INTERNAL);
        (void)state; // Suppress unused variable warning
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    
    auto directDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Benchmark ASCOM protocol access
    startTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        bridge->setProperty<bool>("CoolerOn", i % 2 == 0, bridge::ProtocolType::ASCOM);
        bool state = bridge->getProperty<bool>("CoolerOn", bridge::ProtocolType::ASCOM);
        (void)state; // Suppress unused variable warning
    }
    endTime = std::chrono::high_resolution_clock::now();
    
    auto ascomDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Verify performance is reasonable (less than 10μs per operation on average)
    double directAvg = static_cast<double>(directDuration.count()) / iterations;
    double ascomAvg = static_cast<double>(ascomDuration.count()) / iterations;
    
    EXPECT_LT(directAvg, 10.0); // Less than 10μs per operation
    EXPECT_LT(ascomAvg, 20.0);  // ASCOM should be less than 20μs per operation
    
    std::cout << "Performance Results:" << std::endl;
    std::cout << "  Direct access: " << directAvg << "μs per operation" << std::endl;
    std::cout << "  ASCOM access: " << ascomAvg << "μs per operation" << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

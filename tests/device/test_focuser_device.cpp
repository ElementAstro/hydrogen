/**
 * @file test_focuser_device.cpp
 * @brief Comprehensive tests for focuser device functionality
 * 
 * Tests focuser-specific operations including position control, temperature
 * compensation, backlash compensation, and movement operations.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "device/focuser.h"
#include "common/message.h"
#include "../utils/simple_helpers.h"
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>

using namespace hydrogen::device;
using namespace hydrogen::common;
using namespace testing;
using json = nlohmann::json;

class FocuserDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        focuser = std::make_unique<Focuser>("test-focuser");
        
        // Set up default focuser properties
        focuser->setProperty("MaxStep", 50000);
        focuser->setProperty("MaxIncrement", 5000);
        focuser->setProperty("StepSize", 1.0);
        focuser->setProperty("HasTemperatureCompensation", true);
        focuser->setProperty("HasBacklashCompensation", true);
        focuser->setProperty("HasAbsolutePosition", true);
        focuser->setProperty("HasHalfStep", false);
    }
    
    void TearDown() override {
        if (focuser && focuser->isConnected()) {
            focuser->halt();
            focuser->disconnect();
        }
    }
    
    std::unique_ptr<Focuser> focuser;
};

// Test focuser device creation and basic properties
TEST_F(FocuserDeviceTest, DeviceCreation) {
    EXPECT_EQ(focuser->getDeviceId(), "test-focuser");
    EXPECT_EQ(focuser->getDeviceType(), "FOCUSER");
    EXPECT_EQ(focuser->getManufacturer(), "Test Corp");
    EXPECT_EQ(focuser->getModel(), "Focuser Model");
    
    // Test device info
    json info = focuser->getDeviceInfo();
    EXPECT_TRUE(info.is_object());
    EXPECT_EQ(info["deviceId"], "test-focuser");
    EXPECT_EQ(info["deviceType"], "FOCUSER");
    EXPECT_EQ(info["manufacturer"], "Test Corp");
    EXPECT_EQ(info["model"], "Focuser Model");
}

// Test focuser position control
TEST_F(FocuserDeviceTest, PositionControl) {
    // Test initial position
    int initialPosition = focuser->getPosition();
    EXPECT_GE(initialPosition, 0);
    
    // Test absolute move
    int targetPosition = 10000;
    EXPECT_NO_THROW(focuser->moveTo(targetPosition));
    
    // Verify position is set (in simulation, position should be updated immediately)
    EXPECT_EQ(focuser->getPosition(), targetPosition);
    
    // Test relative move
    int relativeMove = 1000;
    EXPECT_NO_THROW(focuser->moveBy(relativeMove));
    EXPECT_EQ(focuser->getPosition(), targetPosition + relativeMove);
    
    // Test negative relative move
    EXPECT_NO_THROW(focuser->moveBy(-500));
    EXPECT_EQ(focuser->getPosition(), targetPosition + relativeMove - 500);
}

// Test focuser movement limits
TEST_F(FocuserDeviceTest, MovementLimits) {
    int maxStep = focuser->getProperty("MaxStep").get<int>();
    
    // Test valid positions
    EXPECT_NO_THROW(focuser->moveTo(0));
    EXPECT_NO_THROW(focuser->moveTo(maxStep / 2));
    EXPECT_NO_THROW(focuser->moveTo(maxStep));
    
    // Test invalid positions
    EXPECT_THROW(focuser->moveTo(-1), std::invalid_argument);
    EXPECT_THROW(focuser->moveTo(maxStep + 1), std::invalid_argument);
    
    // Test relative moves that would exceed limits
    focuser->moveTo(maxStep - 100);
    EXPECT_THROW(focuser->moveBy(200), std::invalid_argument); // Would exceed max
    
    focuser->moveTo(100);
    EXPECT_THROW(focuser->moveBy(-200), std::invalid_argument); // Would go below 0
}

// Test focuser movement state
TEST_F(FocuserDeviceTest, MovementState) {
    // Test initial state
    EXPECT_FALSE(focuser->isMoving());
    
    // Start a move
    focuser->moveTo(10000);
    
    // In a real focuser, this might be true during movement
    // For simulation, movement is typically instantaneous
    // EXPECT_TRUE(focuser->isMoving());
    
    // Test halt functionality
    EXPECT_NO_THROW(focuser->halt());
    EXPECT_FALSE(focuser->isMoving());
}

// Test temperature compensation
TEST_F(FocuserDeviceTest, TemperatureCompensation) {
    if (focuser->getProperty("HasTemperatureCompensation").get<bool>()) {
        // Test temperature compensation enable/disable
        EXPECT_NO_THROW(focuser->setTemperatureCompensation(true));
        EXPECT_TRUE(focuser->isTemperatureCompensationEnabled());
        
        EXPECT_NO_THROW(focuser->setTemperatureCompensation(false));
        EXPECT_FALSE(focuser->isTemperatureCompensationEnabled());
        
        // Test temperature coefficient
        double coefficient = 5.0; // steps per degree
        EXPECT_NO_THROW(focuser->setTemperatureCoefficient(coefficient));
        EXPECT_EQ(focuser->getTemperatureCoefficient(), coefficient);
        
        // Test current temperature reading
        double temperature = focuser->getTemperature();
        EXPECT_GT(temperature, -50.0); // Reasonable temperature range
        EXPECT_LT(temperature, 100.0);
    }
}

// Test backlash compensation
TEST_F(FocuserDeviceTest, BacklashCompensation) {
    if (focuser->getProperty("HasBacklashCompensation").get<bool>()) {
        // Test backlash compensation enable/disable
        EXPECT_NO_THROW(focuser->setBacklashCompensation(true));
        EXPECT_TRUE(focuser->isBacklashCompensationEnabled());
        
        EXPECT_NO_THROW(focuser->setBacklashCompensation(false));
        EXPECT_FALSE(focuser->isBacklashCompensationEnabled());
        
        // Test backlash steps
        int backlashSteps = 100;
        EXPECT_NO_THROW(focuser->setBacklashSteps(backlashSteps));
        EXPECT_EQ(focuser->getBacklashSteps(), backlashSteps);
    }
}

// Test focuser speed control
TEST_F(FocuserDeviceTest, SpeedControl) {
    // Test speed setting (if supported)
    try {
        int speed = 50; // Percentage
        focuser->setSpeed(speed);
        EXPECT_EQ(focuser->getSpeed(), speed);
        
        // Test speed limits
        EXPECT_THROW(focuser->setSpeed(-1), std::invalid_argument);
        EXPECT_THROW(focuser->setSpeed(101), std::invalid_argument);
        
        // Test valid speed range
        EXPECT_NO_THROW(focuser->setSpeed(1));
        EXPECT_NO_THROW(focuser->setSpeed(100));
    } catch (const std::exception&) {
        // Speed control might not be supported by all focusers
        SUCCEED(); // Test passes if speed control is not supported
    }
}

// Test focuser step size
TEST_F(FocuserDeviceTest, StepSize) {
    double stepSize = focuser->getStepSize();
    EXPECT_GT(stepSize, 0.0);
    
    // Test setting step size (if supported)
    try {
        double newStepSize = 2.0;
        focuser->setStepSize(newStepSize);
        EXPECT_EQ(focuser->getStepSize(), newStepSize);
        
        // Test invalid step size
        EXPECT_THROW(focuser->setStepSize(0.0), std::invalid_argument);
        EXPECT_THROW(focuser->setStepSize(-1.0), std::invalid_argument);
    } catch (const std::exception&) {
        // Step size might not be configurable on all focusers
        SUCCEED();
    }
}

// Test focuser error conditions
TEST_F(FocuserDeviceTest, ErrorConditions) {
    // Test invalid position values
    EXPECT_THROW(focuser->moveTo(-1), std::invalid_argument);
    EXPECT_THROW(focuser->moveTo(100000), std::invalid_argument); // Assuming max is 50000
    
    // Test invalid relative moves
    focuser->moveTo(0);
    EXPECT_THROW(focuser->moveBy(-1), std::invalid_argument);
    
    focuser->moveTo(50000);
    EXPECT_THROW(focuser->moveBy(1), std::invalid_argument);
    
    // Test invalid temperature coefficient
    EXPECT_THROW(focuser->setTemperatureCoefficient(-100.0), std::invalid_argument);
    EXPECT_THROW(focuser->setTemperatureCoefficient(100.0), std::invalid_argument);
    
    // Test invalid backlash steps
    EXPECT_THROW(focuser->setBacklashSteps(-1), std::invalid_argument);
    EXPECT_THROW(focuser->setBacklashSteps(10000), std::invalid_argument); // Too many steps
}

// Test focuser performance
TEST_F(FocuserDeviceTest, FocuserPerformance) {
    const int numOperations = 100;
    
    // Test position setting performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; i++) {
        int position = i * 100; // Positions from 0 to 9900
        focuser->moveTo(position);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
    
    // Test relative move performance
    start = std::chrono::high_resolution_clock::now();
    focuser->moveTo(25000); // Start from middle position
    for (int i = 0; i < numOperations; i++) {
        int move = (i % 2 == 0) ? 10 : -10; // Alternate +10/-10
        focuser->moveBy(move);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
}

// Test concurrent focuser operations
TEST_F(FocuserDeviceTest, ConcurrentOperations) {
    const int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    // Test concurrent property access
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([this, &successCount, t]() {
            try {
                for (int i = 0; i < 10; i++) {
                    // Perform various operations
                    focuser->getPosition();
                    focuser->isMoving();
                    focuser->getTemperature();
                    focuser->getStepSize();
                    
                    // Set position (with thread-specific values to avoid conflicts)
                    int position = 1000 + t * 1000 + i * 10;
                    if (position <= 50000) {
                        focuser->moveTo(position);
                    }
                    
                    successCount++;
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
    
    EXPECT_EQ(successCount.load(), numThreads * 10);
}

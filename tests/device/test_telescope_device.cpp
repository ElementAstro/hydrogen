#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "device/telescope.h"
#include "common/message.h"
#include "utils/simple_helpers.h"
#include <memory>
#include <thread>
#include <chrono>

using namespace hydrogen::device;
using namespace hydrogen::common;
using namespace testing;
using json = nlohmann::json;

class TelescopeDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        telescope = std::make_unique<Telescope>("telescope-01", "Celestron", "EdgeHD 14");
    }

    std::unique_ptr<Telescope> telescope;
};

// Basic functionality tests
TEST_F(TelescopeDeviceTest, InitialState) {
    EXPECT_EQ(telescope->getDeviceId(), "telescope-01");
    EXPECT_EQ(telescope->getManufacturer(), "Celestron");
    EXPECT_EQ(telescope->getModel(), "EdgeHD 14");
    
    // Test initial telescope state
    EXPECT_FALSE(telescope->isTracking());
    EXPECT_FALSE(telescope->isSlewing());
    EXPECT_FALSE(telescope->isParked());
    EXPECT_FALSE(telescope->isAtHome());
}

TEST_F(TelescopeDeviceTest, DeviceInfo) {
    json info = telescope->getDeviceInfo();
    
    EXPECT_EQ(info["id"], "telescope-01");
    EXPECT_EQ(info["manufacturer"], "Celestron");
    EXPECT_EQ(info["model"], "EdgeHD 14");
    EXPECT_TRUE(info.contains("capabilities"));
    EXPECT_TRUE(info.contains("version"));
    
    // Check telescope-specific capabilities
    auto capabilities = info["capabilities"];
    EXPECT_TRUE(capabilities.is_array());
}

TEST_F(TelescopeDeviceTest, CoordinateSystem) {
    // Test setting coordinates
    double ra = 12.5;  // hours
    double dec = 45.0; // degrees
    
    EXPECT_NO_THROW(telescope->setTargetCoordinates(ra, dec));
    
    // Test getting coordinates
    auto coords = telescope->getTargetCoordinates();
    EXPECT_NEAR(coords.first, ra, 0.001);
    EXPECT_NEAR(coords.second, dec, 0.001);
}

TEST_F(TelescopeDeviceTest, SlewingOperations) {
    // Test slewing to coordinates
    double ra = 10.0;
    double dec = 30.0;
    
    telescope->setTargetCoordinates(ra, dec);
    
    // Start slewing
    EXPECT_NO_THROW(telescope->slewToTarget());
    
    // During slewing, telescope should report as slewing
    // Note: In a real implementation, this would be asynchronous
    
    // Test abort slewing
    EXPECT_NO_THROW(telescope->abortSlew());
}

TEST_F(TelescopeDeviceTest, TrackingControl) {
    // Test starting tracking
    EXPECT_NO_THROW(telescope->startTracking());
    
    // Test stopping tracking
    EXPECT_NO_THROW(telescope->stopTracking());
    
    // Test setting tracking rate
    EXPECT_NO_THROW(telescope->setTrackingRate(1.0)); // Sidereal rate
}

TEST_F(TelescopeDeviceTest, ParkingOperations) {
    // Test parking
    EXPECT_NO_THROW(telescope->park());
    
    // Test unparking
    EXPECT_NO_THROW(telescope->unpark());
    
    // Test finding home
    EXPECT_NO_THROW(telescope->findHome());
}

TEST_F(TelescopeDeviceTest, GuideOperations) {
    // Test guide pulse in different directions
    int duration = 1000; // milliseconds
    
    EXPECT_NO_THROW(telescope->guidePulse(GuideDirection::NORTH, duration));
    EXPECT_NO_THROW(telescope->guidePulse(GuideDirection::SOUTH, duration));
    EXPECT_NO_THROW(telescope->guidePulse(GuideDirection::EAST, duration));
    EXPECT_NO_THROW(telescope->guidePulse(GuideDirection::WEST, duration));
}

TEST_F(TelescopeDeviceTest, SlewRates) {
    // Test setting different slew rates
    EXPECT_NO_THROW(telescope->setSlewRate(SlewRate::GUIDE));
    EXPECT_NO_THROW(telescope->setSlewRate(SlewRate::CENTERING));
    EXPECT_NO_THROW(telescope->setSlewRate(SlewRate::FIND));
    EXPECT_NO_THROW(telescope->setSlewRate(SlewRate::MAX));
    
    // Test getting current slew rate
    SlewRate rate = telescope->getSlewRate();
    EXPECT_GE(static_cast<int>(rate), 0);
    EXPECT_LE(static_cast<int>(rate), 3);
}

TEST_F(TelescopeDeviceTest, CommandHandling) {
    json parameters;
    json response;
    
    // Test slew command
    parameters = {
        {"ra", 12.5},
        {"dec", 45.0}
    };
    
    EXPECT_TRUE(telescope->handleDeviceCommand("slew_to_coordinates", parameters, response));
    EXPECT_TRUE(response.contains("success"));
    
    // Test tracking command
    parameters = {{"enable", true}};
    EXPECT_TRUE(telescope->handleDeviceCommand("set_tracking", parameters, response));
    
    // Test park command
    parameters.clear();
    EXPECT_TRUE(telescope->handleDeviceCommand("park", parameters, response));
    
    // Test invalid command
    EXPECT_FALSE(telescope->handleDeviceCommand("invalid_command", parameters, response));
}

TEST_F(TelescopeDeviceTest, PropertyManagement) {
    // Test setting telescope properties
    EXPECT_TRUE(telescope->setProperty("tracking_rate", json(1.0)));
    EXPECT_TRUE(telescope->setProperty("slew_rate", json(2)));
    EXPECT_TRUE(telescope->setProperty("target_ra", json(12.5)));
    EXPECT_TRUE(telescope->setProperty("target_dec", json(45.0)));
    
    // Test getting telescope properties
    json trackingRate = telescope->getProperty("tracking_rate");
    EXPECT_FALSE(trackingRate.is_null());
    
    json slewRate = telescope->getProperty("slew_rate");
    EXPECT_FALSE(slewRate.is_null());
    
    // Test invalid property
    json invalidProp = telescope->getProperty("non_existent_property");
    EXPECT_TRUE(invalidProp.is_null());
}

TEST_F(TelescopeDeviceTest, StatusReporting) {
    json status = telescope->getDeviceStatus();
    
    EXPECT_TRUE(status.contains("connected"));
    EXPECT_TRUE(status.contains("tracking"));
    EXPECT_TRUE(status.contains("slewing"));
    EXPECT_TRUE(status.contains("parked"));
    EXPECT_TRUE(status.contains("at_home"));
    EXPECT_TRUE(status.contains("current_ra"));
    EXPECT_TRUE(status.contains("current_dec"));
    EXPECT_TRUE(status.contains("target_ra"));
    EXPECT_TRUE(status.contains("target_dec"));
}

// Error handling tests
TEST_F(TelescopeDeviceTest, InvalidCoordinates) {
    // Test invalid RA (should be 0-24 hours)
    EXPECT_NO_THROW(telescope->setTargetCoordinates(-1.0, 45.0));  // Should clamp or handle gracefully
    EXPECT_NO_THROW(telescope->setTargetCoordinates(25.0, 45.0));  // Should clamp or handle gracefully
    
    // Test invalid DEC (should be -90 to +90 degrees)
    EXPECT_NO_THROW(telescope->setTargetCoordinates(12.0, -95.0)); // Should clamp or handle gracefully
    EXPECT_NO_THROW(telescope->setTargetCoordinates(12.0, 95.0));  // Should clamp or handle gracefully
}

TEST_F(TelescopeDeviceTest, InvalidGuideDuration) {
    // Test with invalid guide duration
    EXPECT_NO_THROW(telescope->guidePulse(GuideDirection::NORTH, -100)); // Negative duration
    EXPECT_NO_THROW(telescope->guidePulse(GuideDirection::NORTH, 0));    // Zero duration
    EXPECT_NO_THROW(telescope->guidePulse(GuideDirection::NORTH, 60000)); // Very long duration
}

TEST_F(TelescopeDeviceTest, InvalidTrackingRate) {
    // Test with invalid tracking rates
    EXPECT_NO_THROW(telescope->setTrackingRate(-1.0)); // Negative rate
    EXPECT_NO_THROW(telescope->setTrackingRate(0.0));  // Zero rate
    EXPECT_NO_THROW(telescope->setTrackingRate(10.0)); // Very high rate
}

// Concurrency tests
TEST_F(TelescopeDeviceTest, ConcurrentOperations) {
    const int numThreads = 3;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &successCount, t]() {
            try {
                for (int i = 0; i < 5; ++i) {
                    // Different operations in each thread
                    switch (t) {
                        case 0: // Coordinate operations
                            telescope->setTargetCoordinates(12.0 + i, 45.0 + i);
                            telescope->getTargetCoordinates();
                            break;
                        case 1: // Tracking operations
                            telescope->setTrackingRate(1.0 + i * 0.1);
                            telescope->startTracking();
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            telescope->stopTracking();
                            break;
                        case 2: // Status operations
                            telescope->getDeviceStatus();
                            telescope->isTracking();
                            telescope->isSlewing();
                            break;
                    }
                    successCount++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            } catch (const std::exception&) {
                // Handle exceptions gracefully
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_GT(successCount.load(), 0);
}

// Performance tests
TEST_F(TelescopeDeviceTest, CoordinateUpdatePerformance) {
    const int iterations = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        double ra = i * 0.1;
        double dec = i * 0.5;
        telescope->setTargetCoordinates(ra, dec);
        telescope->getTargetCoordinates();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 1000);
}

// Integration tests
TEST_F(TelescopeDeviceTest, CompleteObservingSequence) {
    // Simulate a complete observing sequence
    
    // 1. Unpark telescope
    EXPECT_NO_THROW(telescope->unpark());
    
    // 2. Set target coordinates
    double ra = 12.5;
    double dec = 45.0;
    telescope->setTargetCoordinates(ra, dec);
    
    // 3. Start slewing
    EXPECT_NO_THROW(telescope->slewToTarget());
    
    // 4. Start tracking
    EXPECT_NO_THROW(telescope->startTracking());
    
    // 5. Perform some guide corrections
    EXPECT_NO_THROW(telescope->guidePulse(GuideDirection::NORTH, 500));
    EXPECT_NO_THROW(telescope->guidePulse(GuideDirection::EAST, 300));
    
    // 6. Stop tracking
    EXPECT_NO_THROW(telescope->stopTracking());
    
    // 7. Park telescope
    EXPECT_NO_THROW(telescope->park());
    
    // Verify final state
    json status = telescope->getDeviceStatus();
    EXPECT_TRUE(status.is_object());
}

TEST_F(TelescopeDeviceTest, EmergencyStop) {
    // Test emergency stop functionality

    // Start some operations
    telescope->setTargetCoordinates(12.0, 45.0);
    telescope->slewToTarget();
    telescope->startTracking();

    // Emergency stop
    EXPECT_NO_THROW(telescope->abortSlew());
    EXPECT_NO_THROW(telescope->stopTracking());

    // Verify telescope is in safe state
    json status = telescope->getDeviceStatus();
    EXPECT_TRUE(status.is_object());
}

// Test telescope error conditions
TEST_F(TelescopeDeviceTest, ErrorConditions) {
    // Test invalid coordinates
    EXPECT_THROW(telescope->setTargetCoordinates(-1.0, 45.0), std::invalid_argument); // Invalid RA
    EXPECT_THROW(telescope->setTargetCoordinates(25.0, 45.0), std::invalid_argument); // RA > 24h
    EXPECT_THROW(telescope->setTargetCoordinates(12.0, -91.0), std::invalid_argument); // Invalid Dec
    EXPECT_THROW(telescope->setTargetCoordinates(12.0, 91.0), std::invalid_argument); // Dec > 90°

    // Test invalid tracking rate
    EXPECT_THROW(telescope->setTrackingRate(-1.0), std::invalid_argument);
    EXPECT_THROW(telescope->setTrackingRate(10.0), std::invalid_argument); // Too high

    // Test invalid guide pulse duration
    EXPECT_THROW(telescope->guidePulse(GuideDirection::NORTH, -1), std::invalid_argument);
    EXPECT_THROW(telescope->guidePulse(GuideDirection::NORTH, 60000), std::invalid_argument); // Too long
}

// Test telescope performance
TEST_F(TelescopeDeviceTest, TelescopePerformance) {
    const int numOperations = 100;

    // Test coordinate setting performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; i++) {
        double ra = 12.0 + i * 0.01;
        double dec = 45.0 + i * 0.001;
        telescope->setTargetCoordinates(ra, dec);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second

    // Test tracking rate setting performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; i++) {
        double rate = 1.0 + i * 0.0001; // Small variations around sidereal rate
        telescope->setTrackingRate(rate);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
}

// Test concurrent telescope operations
TEST_F(TelescopeDeviceTest, ConcurrentOperations) {
    const int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Test concurrent property access
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([this, &successCount, t]() {
            try {
                for (int i = 0; i < 10; i++) {
                    // Perform various operations
                    telescope->getCurrentCoordinates();
                    telescope->getTrackingRate();
                    telescope->isTracking();
                    telescope->isSlewing();
                    telescope->isParked();

                    // Set coordinates (with thread-specific values)
                    double ra = 12.0 + t * 0.1 + i * 0.01;
                    double dec = 45.0 + t * 0.1 + i * 0.001;
                    telescope->setTargetCoordinates(ra, dec);

                    // Set tracking rate
                    double rate = 1.0 + t * 0.0001;
                    telescope->setTrackingRate(rate);

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

// Test telescope state transitions
TEST_F(TelescopeDeviceTest, StateTransitions) {
    // Test initial state
    EXPECT_FALSE(telescope->isSlewing());
    EXPECT_FALSE(telescope->isTracking());
    EXPECT_TRUE(telescope->isParked()); // Assume starts parked

    // Unpark telescope
    telescope->unpark();
    EXPECT_FALSE(telescope->isParked());

    // Start slewing
    telescope->setTargetCoordinates(12.0, 45.0);
    telescope->slewToTarget();
    EXPECT_TRUE(telescope->isSlewing());
    EXPECT_FALSE(telescope->isTracking());

    // Wait for slew to complete (simulated)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Start tracking
    telescope->startTracking();
    EXPECT_FALSE(telescope->isSlewing());
    EXPECT_TRUE(telescope->isTracking());

    // Stop tracking
    telescope->stopTracking();
    EXPECT_FALSE(telescope->isTracking());

    // Park telescope
    telescope->park();
    EXPECT_TRUE(telescope->isParked());
    EXPECT_FALSE(telescope->isTracking());
}

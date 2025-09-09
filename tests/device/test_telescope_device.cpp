#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "device/telescope.h"
#include "device/interfaces/device_interface.h"
#include "../utils/simple_helpers.h"
#include <memory>
#include <thread>
#include <chrono>

using namespace hydrogen::device;
using namespace hydrogen::device::interfaces;
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
    
    // Get device info to check manufacturer and model
    json info = telescope->getDeviceInfo();
    EXPECT_EQ(info["manufacturer"], "Celestron");
    EXPECT_EQ(info["model"], "EdgeHD 14");
    
    // Test initial telescope state using proper API methods
    EXPECT_FALSE(telescope->getTracking());
    EXPECT_FALSE(telescope->getSlewing());
    EXPECT_FALSE(telescope->getAtPark());
    EXPECT_FALSE(telescope->getAtHome());
}

// Simple working tests only
TEST_F(TelescopeDeviceTest, BasicCapabilities) {
    // Test basic capabilities
    EXPECT_TRUE(telescope->getCanSlew());
    EXPECT_TRUE(telescope->getCanSync());
    EXPECT_TRUE(telescope->getCanPark());
    EXPECT_TRUE(telescope->getCanSetTracking());
}

TEST_F(TelescopeDeviceTest, DeviceInfo) {
    json info = telescope->getDeviceInfo();
    
    EXPECT_EQ(info["deviceId"], "telescope-01");
    EXPECT_EQ(info["manufacturer"], "Celestron");
    EXPECT_EQ(info["model"], "EdgeHD 14");
    EXPECT_TRUE(info.contains("deviceType"));
}

TEST_F(TelescopeDeviceTest, CoordinateSystem) {
    // Test coordinate setting and getting using proper API
    double ra = 12.5;  // hours
    double dec = 45.0; // degrees

    EXPECT_NO_THROW(telescope->setTargetRightAscension(ra));
    EXPECT_NO_THROW(telescope->setTargetDeclination(dec));

    // Test getting coordinates
    EXPECT_NEAR(telescope->getTargetRightAscension(), ra, 0.001);
    EXPECT_NEAR(telescope->getTargetDeclination(), dec, 0.001);
}

TEST_F(TelescopeDeviceTest, TrackingControl) {
    // Test tracking control using proper API
    EXPECT_NO_THROW(telescope->setTracking(true));
    EXPECT_TRUE(telescope->getTracking());
    
    EXPECT_NO_THROW(telescope->setTracking(false));
    EXPECT_FALSE(telescope->getTracking());
    
    // Test tracking rate
    EXPECT_NO_THROW(telescope->setTrackingRate(DriveRate::SIDEREAL));
    EXPECT_EQ(telescope->getTrackingRate(), DriveRate::SIDEREAL);
}

TEST_F(TelescopeDeviceTest, SlewingOperations) {
    // Test slewing to coordinates
    double ra = 10.0;
    double dec = 30.0;
    
    telescope->setTargetRightAscension(ra);
    telescope->setTargetDeclination(dec);
    
    // Start slewing
    EXPECT_NO_THROW(telescope->slewToTarget());
    
    // Test abort slewing
    EXPECT_NO_THROW(telescope->abortSlew());
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
    // Test guide pulse operations using proper API
    int duration = 1000; // milliseconds
    
    EXPECT_NO_THROW(telescope->pulseGuide(GuideDirection::NORTH, duration));
    EXPECT_NO_THROW(telescope->pulseGuide(GuideDirection::SOUTH, duration));
    EXPECT_NO_THROW(telescope->pulseGuide(GuideDirection::EAST, duration));
    EXPECT_NO_THROW(telescope->pulseGuide(GuideDirection::WEST, duration));
}

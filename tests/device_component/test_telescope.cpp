#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <hydrogen/device/telescope.h>
#include <hydrogen/device/device_logger.h>
#include <thread>
#include <chrono>
#include <cmath>

using namespace hydrogen::device;
using namespace testing;

class TelescopeTest : public ::testing::Test {
protected:
    void SetUp() override {
        telescope = std::make_unique<Telescope>("test_telescope", "Test", "MockTelescope");
        DeviceLogger::getInstance().setLogLevel(LogLevel::DEBUG);
    }

    void TearDown() override {
        if (telescope) {
            telescope->stop();
        }
    }

    std::unique_ptr<Telescope> telescope;
    
    // Helper function to compare floating point values
    bool isClose(double a, double b, double tolerance = 1e-6) {
        return std::abs(a - b) < tolerance;
    }
};

TEST_F(TelescopeTest, InitialState) {
    EXPECT_EQ(telescope->getDeviceId(), "test_telescope");
    EXPECT_EQ(telescope->getDeviceType(), "telescope");
    EXPECT_TRUE(telescope->isParked());
    EXPECT_FALSE(telescope->isTracking());
    EXPECT_FALSE(telescope->isMoving());
    
    auto position = telescope->getPosition();
    EXPECT_EQ(position.first, 0.0);  // RA
    EXPECT_EQ(position.second, 0.0); // Dec
}

TEST_F(TelescopeTest, StartStop) {
    EXPECT_TRUE(telescope->start());
    EXPECT_TRUE(telescope->isRunning());
    
    telescope->stop();
    EXPECT_FALSE(telescope->isRunning());
}

TEST_F(TelescopeTest, ParkUnpark) {
    // Initially parked
    EXPECT_TRUE(telescope->isParked());
    
    // Unpark
    telescope->unpark();
    EXPECT_FALSE(telescope->isParked());
    
    // Park again
    telescope->park();
    EXPECT_TRUE(telescope->isParked());
}

TEST_F(TelescopeTest, TrackingControl) {
    telescope->unpark();
    
    // Initially not tracking
    EXPECT_FALSE(telescope->isTracking());
    
    // Enable tracking
    telescope->setTracking(true);
    EXPECT_TRUE(telescope->isTracking());
    
    // Disable tracking
    telescope->setTracking(false);
    EXPECT_FALSE(telescope->isTracking());
}

TEST_F(TelescopeTest, SlewRateControl) {
    // Test valid slew rates
    for (int rate = 0; rate <= 9; ++rate) {
        EXPECT_NO_THROW(telescope->setSlewRate(rate));
    }
    
    // Test invalid slew rates
    EXPECT_THROW(telescope->setSlewRate(-1), std::invalid_argument);
    EXPECT_THROW(telescope->setSlewRate(10), std::invalid_argument);
}

TEST_F(TelescopeTest, GotoPosition) {
    telescope->unpark();
    
    // Test valid coordinates
    EXPECT_NO_THROW(telescope->gotoPosition(12.0, 45.0)); // 12h RA, 45° Dec
    EXPECT_TRUE(telescope->isMoving());
    
    // Test that we can't goto when parked
    telescope->park();
    EXPECT_THROW(telescope->gotoPosition(6.0, 30.0), std::runtime_error);
}

TEST_F(TelescopeTest, SyncPosition) {
    telescope->unpark();
    
    // Sync to a position
    telescope->sync(15.5, -20.0);
    
    auto position = telescope->getPosition();
    EXPECT_TRUE(isClose(position.first, 15.5));
    EXPECT_TRUE(isClose(position.second, -20.0));
    
    // Test that we can't sync when parked
    telescope->park();
    EXPECT_THROW(telescope->sync(10.0, 40.0), std::runtime_error);
}

TEST_F(TelescopeTest, ObserverLocation) {
    double latitude = 40.7128;  // New York
    double longitude = -74.0060;
    
    telescope->setObserverLocation(latitude, longitude);
    
    // The observer location affects Alt/Az calculations
    // We can't directly test the internal values, but we can test
    // that the method doesn't throw and that Alt/Az updates work
    telescope->unpark();
    telescope->sync(12.0, 45.0);
    
    auto altaz = telescope->getAltAz();
    // Alt/Az should be calculated based on the observer location
    EXPECT_GE(altaz.first, -90.0);  // Altitude should be >= -90°
    EXPECT_LE(altaz.first, 90.0);   // Altitude should be <= 90°
    EXPECT_GE(altaz.second, 0.0);   // Azimuth should be >= 0°
    EXPECT_LT(altaz.second, 360.0); // Azimuth should be < 360°
}

TEST_F(TelescopeTest, AbortMovement) {
    telescope->unpark();
    telescope->gotoPosition(10.0, 30.0);
    
    EXPECT_TRUE(telescope->isMoving());
    
    telescope->abort();
    EXPECT_FALSE(telescope->isMoving());
}

TEST_F(TelescopeTest, CoordinateValidation) {
    telescope->unpark();
    
    // Test coordinate limits (this depends on the implementation)
    // For now, we'll test that extreme coordinates are handled
    EXPECT_NO_THROW(telescope->gotoPosition(0.0, 90.0));   // North pole
    EXPECT_NO_THROW(telescope->gotoPosition(12.0, -90.0)); // South pole
    EXPECT_NO_THROW(telescope->gotoPosition(23.99, 0.0));  // Near 24h RA
}

TEST_F(TelescopeTest, AngularSeparationCalculation) {
    // Test angular separation calculation
    // From Polaris (RA=2.5h, Dec=89.3°) to Vega (RA=18.6h, Dec=38.8°)
    double separation = telescope->calculateAngularSeparation(2.5, 89.3, 18.6, 38.8);
    
    // The actual separation should be around 51-52 degrees
    EXPECT_GT(separation, 50.0);
    EXPECT_LT(separation, 55.0);
    
    // Test separation between identical coordinates
    separation = telescope->calculateAngularSeparation(12.0, 45.0, 12.0, 45.0);
    EXPECT_TRUE(isClose(separation, 0.0, 1e-3));
}

TEST_F(TelescopeTest, SlewTimeEstimation) {
    telescope->unpark();
    telescope->sync(0.0, 0.0); // Start at origin
    
    // Test slew time estimation
    double slewTime = telescope->calculateSlewTime(6.0, 45.0); // 6h RA, 45° Dec
    
    EXPECT_GT(slewTime, 0.0); // Should take some time
    EXPECT_LT(slewTime, 3600.0); // Should be less than an hour
    
    // Closer targets should take less time
    double closeSlewTime = telescope->calculateSlewTime(1.0, 5.0);
    EXPECT_LT(closeSlewTime, slewTime);
}

TEST_F(TelescopeTest, SimulatedMovement) {
    telescope->start();
    telescope->unpark();
    
    // Start a goto operation
    telescope->gotoPosition(6.0, 30.0);
    EXPECT_TRUE(telescope->isMoving());
    
    // Wait for some movement simulation
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto position = telescope->getPosition();
    // Position should have changed from initial (0,0)
    EXPECT_NE(position.first, 0.0);
    EXPECT_NE(position.second, 0.0);
    
    telescope->stop();
}

TEST_F(TelescopeTest, TrackingSimulation) {
    telescope->start();
    telescope->unpark();
    telescope->sync(12.0, 45.0); // Set initial position
    telescope->setTracking(true);
    
    auto initialPosition = telescope->getPosition();
    
    // Wait for tracking simulation
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto newPosition = telescope->getPosition();
    
    // RA should have increased due to sidereal tracking
    EXPECT_GT(newPosition.first, initialPosition.first);
    // Dec should remain the same
    EXPECT_TRUE(isClose(newPosition.second, initialPosition.second, 1e-3));
    
    telescope->stop();
}

TEST_F(TelescopeTest, ThreadSafety) {
    telescope->start();
    telescope->unpark();
    
    std::vector<std::thread> threads;
    std::atomic<bool> stopFlag{false};
    
    // Thread 1: Continuous position queries
    threads.emplace_back([this, &stopFlag]() {
        while (!stopFlag) {
            auto pos = telescope->getPosition();
            auto altaz = telescope->getAltAz();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Thread 2: Tracking control
    threads.emplace_back([this, &stopFlag]() {
        bool tracking = false;
        while (!stopFlag) {
            telescope->setTracking(tracking);
            tracking = !tracking;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    
    // Thread 3: Slew rate changes
    threads.emplace_back([this, &stopFlag]() {
        int rate = 1;
        while (!stopFlag) {
            telescope->setSlewRate(rate);
            rate = (rate % 9) + 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    
    // Let threads run for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    stopFlag = true;
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    telescope->stop();
}

TEST_F(TelescopeTest, PropertyManagement) {
    telescope->unpark();
    telescope->sync(15.0, 60.0);
    telescope->setTracking(true);
    telescope->setSlewRate(7);
    
    // Test that properties are properly set
    auto ra = telescope->getProperty("ra");
    auto dec = telescope->getProperty("dec");
    auto tracking = telescope->getProperty("tracking");
    auto slewRate = telescope->getProperty("slew_rate");
    
    EXPECT_TRUE(isClose(ra.get<double>(), 15.0, 1e-3));
    EXPECT_TRUE(isClose(dec.get<double>(), 60.0, 1e-3));
    EXPECT_TRUE(tracking.get<bool>());
    EXPECT_EQ(slewRate.get<int>(), 7);
}

// Performance tests
class TelescopePerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        telescope = std::make_unique<Telescope>("perf_telescope", "Performance", "Test");
    }
    
    std::unique_ptr<Telescope> telescope;
};

TEST_F(TelescopePerformanceTest, UpdatePerformance) {
    telescope->start();
    telescope->unpark();
    
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        telescope->getPosition();
        telescope->getAltAz();
        telescope->isTracking();
        telescope->isMoving();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double operationsPerSecond = (iterations * 4 * 1000000.0) / duration.count();
    
    std::cout << "Telescope operations per second: " << operationsPerSecond << std::endl;
    
    // Should be able to handle at least 10,000 operations per second
    EXPECT_GT(operationsPerSecond, 10000.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

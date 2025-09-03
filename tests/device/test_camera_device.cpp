#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "device/camera.h"
#include "common/message.h"
#include "utils/simple_helpers.h"
#include <memory>
#include <thread>
#include <chrono>

using namespace hydrogen::device;
using namespace hydrogen::common;
using namespace testing;
using json = nlohmann::json;

class CameraDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        camera = std::make_unique<Camera>("camera-01", "ZWO", "ASI294MC Pro");
    }

    std::unique_ptr<Camera> camera;
};

// Basic functionality tests
TEST_F(CameraDeviceTest, InitialState) {
    EXPECT_EQ(camera->getDeviceId(), "camera-01");
    EXPECT_EQ(camera->getManufacturer(), "ZWO");
    EXPECT_EQ(camera->getModel(), "ASI294MC Pro");
    
    // Test initial camera state
    EXPECT_FALSE(camera->isExposing());
    EXPECT_FALSE(camera->isImageReady());
    EXPECT_FALSE(camera->isCoolerOn());
}

TEST_F(CameraDeviceTest, DeviceInfo) {
    json info = camera->getDeviceInfo();
    
    EXPECT_EQ(info["id"], "camera-01");
    EXPECT_EQ(info["manufacturer"], "ZWO");
    EXPECT_EQ(info["model"], "ASI294MC Pro");
    EXPECT_TRUE(info.contains("capabilities"));
    EXPECT_TRUE(info.contains("version"));
    
    // Check camera-specific capabilities
    auto capabilities = info["capabilities"];
    EXPECT_TRUE(capabilities.is_array());
}

TEST_F(CameraDeviceTest, ExposureControl) {
    // Test setting exposure time
    double exposureTime = 5.0; // seconds
    EXPECT_NO_THROW(camera->setExposureTime(exposureTime));
    EXPECT_NEAR(camera->getExposureTime(), exposureTime, 0.001);
    
    // Test starting exposure
    EXPECT_NO_THROW(camera->startExposure(exposureTime));
    
    // Test aborting exposure
    EXPECT_NO_THROW(camera->abortExposure());
}

TEST_F(CameraDeviceTest, BinningControl) {
    // Test setting binning
    EXPECT_NO_THROW(camera->setBinning(1, 1)); // 1x1 binning
    EXPECT_NO_THROW(camera->setBinning(2, 2)); // 2x2 binning
    EXPECT_NO_THROW(camera->setBinning(3, 3)); // 3x3 binning
    
    // Test getting binning
    auto binning = camera->getBinning();
    EXPECT_GT(binning.first, 0);
    EXPECT_GT(binning.second, 0);
}

TEST_F(CameraDeviceTest, ROIControl) {
    // Test setting Region of Interest
    int x = 100, y = 100, width = 800, height = 600;
    EXPECT_NO_THROW(camera->setROI(x, y, width, height));
    
    // Test getting ROI
    auto roi = camera->getROI();
    EXPECT_GE(roi[0], 0); // x
    EXPECT_GE(roi[1], 0); // y
    EXPECT_GT(roi[2], 0); // width
    EXPECT_GT(roi[3], 0); // height
}

TEST_F(CameraDeviceTest, GainControl) {
    // Test setting gain
    int gain = 200;
    EXPECT_NO_THROW(camera->setGain(gain));
    EXPECT_EQ(camera->getGain(), gain);
    
    // Test gain limits
    EXPECT_NO_THROW(camera->setGain(0));   // Minimum gain
    EXPECT_NO_THROW(camera->setGain(600)); // High gain
}

TEST_F(CameraDeviceTest, OffsetControl) {
    // Test setting offset
    int offset = 50;
    EXPECT_NO_THROW(camera->setOffset(offset));
    EXPECT_EQ(camera->getOffset(), offset);
}

TEST_F(CameraDeviceTest, TemperatureControl) {
    // Test setting target temperature
    double targetTemp = -10.0; // Celsius
    EXPECT_NO_THROW(camera->setTargetTemperature(targetTemp));
    EXPECT_NEAR(camera->getTargetTemperature(), targetTemp, 0.1);
    
    // Test cooler control
    EXPECT_NO_THROW(camera->setCoolerOn(true));
    EXPECT_NO_THROW(camera->setCoolerOn(false));
    
    // Test getting current temperature
    double currentTemp = camera->getCurrentTemperature();
    EXPECT_GT(currentTemp, -50.0); // Reasonable temperature range
    EXPECT_LT(currentTemp, 50.0);
}

TEST_F(CameraDeviceTest, ImageDownload) {
    // Test image download (simulation)
    EXPECT_NO_THROW(camera->downloadImage());
    
    // Test getting image data
    auto imageData = camera->getImageData();
    EXPECT_TRUE(imageData.is_object() || imageData.is_null());
}

TEST_F(CameraDeviceTest, CommandHandling) {
    json parameters;
    json response;
    
    // Test exposure command
    parameters = {
        {"duration", 5.0},
        {"light", true}
    };
    
    EXPECT_TRUE(camera->handleDeviceCommand("start_exposure", parameters, response));
    EXPECT_TRUE(response.contains("success"));
    
    // Test binning command
    parameters = {
        {"x", 2},
        {"y", 2}
    };
    EXPECT_TRUE(camera->handleDeviceCommand("set_binning", parameters, response));
    
    // Test gain command
    parameters = {{"gain", 200}};
    EXPECT_TRUE(camera->handleDeviceCommand("set_gain", parameters, response));
    
    // Test invalid command
    parameters.clear();
    EXPECT_FALSE(camera->handleDeviceCommand("invalid_command", parameters, response));
}

TEST_F(CameraDeviceTest, PropertyManagement) {
    // Test setting camera properties
    EXPECT_TRUE(camera->setProperty("exposure_time", json(5.0)));
    EXPECT_TRUE(camera->setProperty("gain", json(200)));
    EXPECT_TRUE(camera->setProperty("offset", json(50)));
    EXPECT_TRUE(camera->setProperty("target_temperature", json(-10.0)));
    EXPECT_TRUE(camera->setProperty("cooler_on", json(true)));
    
    // Test getting camera properties
    json exposureTime = camera->getProperty("exposure_time");
    EXPECT_FALSE(exposureTime.is_null());
    
    json gain = camera->getProperty("gain");
    EXPECT_FALSE(gain.is_null());
    
    // Test invalid property
    json invalidProp = camera->getProperty("non_existent_property");
    EXPECT_TRUE(invalidProp.is_null());
}

TEST_F(CameraDeviceTest, StatusReporting) {
    json status = camera->getDeviceStatus();
    
    EXPECT_TRUE(status.contains("connected"));
    EXPECT_TRUE(status.contains("exposing"));
    EXPECT_TRUE(status.contains("image_ready"));
    EXPECT_TRUE(status.contains("cooler_on"));
    EXPECT_TRUE(status.contains("current_temperature"));
    EXPECT_TRUE(status.contains("target_temperature"));
    EXPECT_TRUE(status.contains("exposure_time"));
    EXPECT_TRUE(status.contains("gain"));
    EXPECT_TRUE(status.contains("offset"));
}

// Error handling tests
TEST_F(CameraDeviceTest, InvalidExposureTime) {
    // Test invalid exposure times
    EXPECT_NO_THROW(camera->setExposureTime(-1.0)); // Negative exposure
    EXPECT_NO_THROW(camera->setExposureTime(0.0));  // Zero exposure
    EXPECT_NO_THROW(camera->setExposureTime(3600.0)); // Very long exposure
}

TEST_F(CameraDeviceTest, InvalidBinning) {
    // Test invalid binning values
    EXPECT_NO_THROW(camera->setBinning(0, 1)); // Zero binning
    EXPECT_NO_THROW(camera->setBinning(1, 0)); // Zero binning
    EXPECT_NO_THROW(camera->setBinning(-1, 1)); // Negative binning
    EXPECT_NO_THROW(camera->setBinning(10, 10)); // Very high binning
}

TEST_F(CameraDeviceTest, InvalidROI) {
    // Test invalid ROI values
    EXPECT_NO_THROW(camera->setROI(-100, 100, 800, 600)); // Negative x
    EXPECT_NO_THROW(camera->setROI(100, -100, 800, 600)); // Negative y
    EXPECT_NO_THROW(camera->setROI(100, 100, 0, 600));    // Zero width
    EXPECT_NO_THROW(camera->setROI(100, 100, 800, 0));    // Zero height
}

TEST_F(CameraDeviceTest, InvalidGainOffset) {
    // Test invalid gain values
    EXPECT_NO_THROW(camera->setGain(-100)); // Negative gain
    EXPECT_NO_THROW(camera->setGain(10000)); // Very high gain
    
    // Test invalid offset values
    EXPECT_NO_THROW(camera->setOffset(-100)); // Negative offset
    EXPECT_NO_THROW(camera->setOffset(1000)); // Very high offset
}

TEST_F(CameraDeviceTest, InvalidTemperature) {
    // Test invalid temperature values
    EXPECT_NO_THROW(camera->setTargetTemperature(-100.0)); // Very cold
    EXPECT_NO_THROW(camera->setTargetTemperature(100.0));  // Very hot
}

// Concurrency tests
TEST_F(CameraDeviceTest, ConcurrentOperations) {
    const int numThreads = 3;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &successCount, t]() {
            try {
                for (int i = 0; i < 5; ++i) {
                    // Different operations in each thread
                    switch (t) {
                        case 0: // Exposure operations
                            camera->setExposureTime(1.0 + i);
                            camera->getExposureTime();
                            break;
                        case 1: // Gain/Offset operations
                            camera->setGain(100 + i * 10);
                            camera->setOffset(50 + i * 5);
                            camera->getGain();
                            camera->getOffset();
                            break;
                        case 2: // Temperature operations
                            camera->setTargetTemperature(-10.0 + i);
                            camera->getCurrentTemperature();
                            camera->getTargetTemperature();
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
TEST_F(CameraDeviceTest, ParameterUpdatePerformance) {
    const int iterations = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        camera->setExposureTime(1.0 + i * 0.1);
        camera->setGain(100 + i);
        camera->setOffset(50 + i);
        camera->setBinning((i % 3) + 1, (i % 3) + 1);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 1000);
}

// Integration tests
TEST_F(CameraDeviceTest, CompleteImagingSequence) {
    // Simulate a complete imaging sequence
    
    // 1. Set camera parameters
    camera->setExposureTime(5.0);
    camera->setGain(200);
    camera->setOffset(50);
    camera->setBinning(1, 1);
    camera->setROI(0, 0, 1920, 1080);
    
    // 2. Set target temperature and turn on cooler
    camera->setTargetTemperature(-10.0);
    camera->setCoolerOn(true);
    
    // 3. Start exposure
    EXPECT_NO_THROW(camera->startExposure(5.0));
    
    // 4. Wait for exposure (simulated)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 5. Download image
    EXPECT_NO_THROW(camera->downloadImage());
    
    // 6. Get image data
    auto imageData = camera->getImageData();
    EXPECT_TRUE(imageData.is_object() || imageData.is_null());
    
    // 7. Turn off cooler
    camera->setCoolerOn(false);
    
    // Verify final state
    json status = camera->getDeviceStatus();
    EXPECT_TRUE(status.is_object());
}

TEST_F(CameraDeviceTest, CoolingSequence) {
    // Test complete cooling sequence
    
    // 1. Set target temperature
    double targetTemp = -15.0;
    camera->setTargetTemperature(targetTemp);
    
    // 2. Turn on cooler
    camera->setCoolerOn(true);
    
    // 3. Monitor temperature (simulated)
    for (int i = 0; i < 5; ++i) {
        double currentTemp = camera->getCurrentTemperature();
        EXPECT_GT(currentTemp, -50.0);
        EXPECT_LT(currentTemp, 50.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 4. Turn off cooler
    camera->setCoolerOn(false);

    // Verify cooler is off
    EXPECT_FALSE(camera->isCoolerOn());
}

// Test camera error conditions
TEST_F(CameraDeviceTest, ErrorConditions) {
    // Test invalid exposure time
    EXPECT_THROW(camera->setExposureTime(-1.0), std::invalid_argument);
    EXPECT_THROW(camera->setExposureTime(0.0), std::invalid_argument);

    // Test invalid binning
    EXPECT_THROW(camera->setBinning(0, 1), std::invalid_argument);
    EXPECT_THROW(camera->setBinning(1, 0), std::invalid_argument);
    EXPECT_THROW(camera->setBinning(-1, 1), std::invalid_argument);

    // Test invalid ROI
    EXPECT_THROW(camera->setROI(-1, 0, 100, 100), std::invalid_argument);
    EXPECT_THROW(camera->setROI(0, -1, 100, 100), std::invalid_argument);
    EXPECT_THROW(camera->setROI(0, 0, 0, 100), std::invalid_argument);
    EXPECT_THROW(camera->setROI(0, 0, 100, 0), std::invalid_argument);

    // Test invalid gain/offset
    EXPECT_THROW(camera->setGain(-1), std::invalid_argument);
    EXPECT_THROW(camera->setOffset(-1), std::invalid_argument);
}

// Test camera performance
TEST_F(CameraDeviceTest, CameraPerformance) {
    const int numOperations = 100;

    // Test exposure time setting performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; i++) {
        camera->setExposureTime(1.0 + i * 0.01);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second

    // Test binning setting performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; i++) {
        int bin = (i % 4) + 1; // Cycle through 1-4
        camera->setBinning(bin, bin);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
}

// Test concurrent camera operations
TEST_F(CameraDeviceTest, ConcurrentOperations) {
    const int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Test concurrent property access
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([this, &successCount, t]() {
            try {
                for (int i = 0; i < 10; i++) {
                    // Perform various operations
                    camera->getExposureTime();
                    camera->getBinning();
                    camera->getROI();
                    camera->getGain();
                    camera->getOffset();

                    // Set some properties (with thread-specific values)
                    double exposure = 1.0 + t * 0.1 + i * 0.01;
                    camera->setExposureTime(exposure);

                    int gain = 100 + t * 10 + i;
                    camera->setGain(gain);

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

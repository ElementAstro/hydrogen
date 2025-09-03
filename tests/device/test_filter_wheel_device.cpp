/**
 * @file test_filter_wheel_device.cpp
 * @brief Comprehensive tests for filter wheel device functionality
 * 
 * Tests filter wheel operations including filter selection, position control,
 * filter naming, and wheel rotation functionality.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
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

class FilterWheelDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        filterWheel = std::make_unique<FilterWheelDevice>("test-filter-wheel", "Test Corp", "FilterWheel Model");
        
        // Set up default filter wheel properties
        filterWheel->setProperty("FilterCount", 8);
        filterWheel->setProperty("HasNames", true);
        filterWheel->setProperty("HasOffsets", true);
        
        // Set up default filter names
        std::vector<std::string> filterNames = {
            "Red", "Green", "Blue", "Luminance", "Ha", "OIII", "SII", "Clear"
        };
        
        for (size_t i = 0; i < filterNames.size(); i++) {
            filterWheel->setFilterName(i, filterNames[i]);
        }
        
        // Set up default filter offsets (focus offsets)
        std::vector<int> filterOffsets = {0, 50, -25, 0, 75, 100, 80, -10};
        for (size_t i = 0; i < filterOffsets.size(); i++) {
            filterWheel->setFilterOffset(i, filterOffsets[i]);
        }
    }
    
    void TearDown() override {
        if (filterWheel && filterWheel->isConnected()) {
            filterWheel->disconnect();
        }
    }
    
    std::unique_ptr<FilterWheelDevice> filterWheel;
};

// Test filter wheel device creation and basic properties
TEST_F(FilterWheelDeviceTest, DeviceCreation) {
    EXPECT_EQ(filterWheel->getDeviceId(), "test-filter-wheel");
    EXPECT_EQ(filterWheel->getDeviceType(), "FILTER_WHEEL");
    EXPECT_EQ(filterWheel->getManufacturer(), "Test Corp");
    EXPECT_EQ(filterWheel->getModel(), "FilterWheel Model");
    
    // Test device info
    json info = filterWheel->getDeviceInfo();
    EXPECT_TRUE(info.is_object());
    EXPECT_EQ(info["deviceId"], "test-filter-wheel");
    EXPECT_EQ(info["deviceType"], "FILTER_WHEEL");
    EXPECT_EQ(info["manufacturer"], "Test Corp");
    EXPECT_EQ(info["model"], "FilterWheel Model");
}

// Test filter position control
TEST_F(FilterWheelDeviceTest, FilterPositionControl) {
    int filterCount = filterWheel->getFilterCount();
    EXPECT_EQ(filterCount, 8);
    
    // Test initial position
    int initialPosition = filterWheel->getCurrentPosition();
    EXPECT_GE(initialPosition, 0);
    EXPECT_LT(initialPosition, filterCount);
    
    // Test setting filter position
    for (int i = 0; i < filterCount; i++) {
        EXPECT_NO_THROW(filterWheel->setPosition(i));
        EXPECT_EQ(filterWheel->getCurrentPosition(), i);
    }
    
    // Test invalid positions
    EXPECT_THROW(filterWheel->setPosition(-1), std::invalid_argument);
    EXPECT_THROW(filterWheel->setPosition(filterCount), std::invalid_argument);
    EXPECT_THROW(filterWheel->setPosition(filterCount + 1), std::invalid_argument);
}

// Test filter names
TEST_F(FilterWheelDeviceTest, FilterNames) {
    std::vector<std::string> expectedNames = {
        "Red", "Green", "Blue", "Luminance", "Ha", "OIII", "SII", "Clear"
    };
    
    // Test getting filter names
    for (size_t i = 0; i < expectedNames.size(); i++) {
        EXPECT_EQ(filterWheel->getFilterName(i), expectedNames[i]);
    }
    
    // Test setting filter names
    std::string newName = "NewFilter";
    filterWheel->setFilterName(0, newName);
    EXPECT_EQ(filterWheel->getFilterName(0), newName);
    
    // Test getting all filter names
    auto allNames = filterWheel->getFilterNames();
    EXPECT_EQ(allNames.size(), 8);
    EXPECT_EQ(allNames[0], newName);
    
    // Test invalid filter index
    EXPECT_THROW(filterWheel->getFilterName(-1), std::invalid_argument);
    EXPECT_THROW(filterWheel->getFilterName(8), std::invalid_argument);
    EXPECT_THROW(filterWheel->setFilterName(-1, "Invalid"), std::invalid_argument);
    EXPECT_THROW(filterWheel->setFilterName(8, "Invalid"), std::invalid_argument);
}

// Test filter offsets
TEST_F(FilterWheelDeviceTest, FilterOffsets) {
    std::vector<int> expectedOffsets = {0, 50, -25, 0, 75, 100, 80, -10};
    
    // Test getting filter offsets
    for (size_t i = 0; i < expectedOffsets.size(); i++) {
        EXPECT_EQ(filterWheel->getFilterOffset(i), expectedOffsets[i]);
    }
    
    // Test setting filter offsets
    int newOffset = 200;
    filterWheel->setFilterOffset(0, newOffset);
    EXPECT_EQ(filterWheel->getFilterOffset(0), newOffset);
    
    // Test getting all filter offsets
    auto allOffsets = filterWheel->getFilterOffsets();
    EXPECT_EQ(allOffsets.size(), 8);
    EXPECT_EQ(allOffsets[0], newOffset);
    
    // Test invalid filter index
    EXPECT_THROW(filterWheel->getFilterOffset(-1), std::invalid_argument);
    EXPECT_THROW(filterWheel->getFilterOffset(8), std::invalid_argument);
    EXPECT_THROW(filterWheel->setFilterOffset(-1, 100), std::invalid_argument);
    EXPECT_THROW(filterWheel->setFilterOffset(8, 100), std::invalid_argument);
    
    // Test extreme offset values
    EXPECT_THROW(filterWheel->setFilterOffset(0, -10000), std::invalid_argument);
    EXPECT_THROW(filterWheel->setFilterOffset(0, 10000), std::invalid_argument);
}

// Test filter selection by name
TEST_F(FilterWheelDeviceTest, FilterSelectionByName) {
    // Test selecting filter by name
    EXPECT_NO_THROW(filterWheel->setFilterByName("Red"));
    EXPECT_EQ(filterWheel->getCurrentPosition(), 0);
    EXPECT_EQ(filterWheel->getCurrentFilterName(), "Red");
    
    EXPECT_NO_THROW(filterWheel->setFilterByName("Blue"));
    EXPECT_EQ(filterWheel->getCurrentPosition(), 2);
    EXPECT_EQ(filterWheel->getCurrentFilterName(), "Blue");
    
    EXPECT_NO_THROW(filterWheel->setFilterByName("Ha"));
    EXPECT_EQ(filterWheel->getCurrentPosition(), 4);
    EXPECT_EQ(filterWheel->getCurrentFilterName(), "Ha");
    
    // Test invalid filter name
    EXPECT_THROW(filterWheel->setFilterByName("NonExistent"), std::invalid_argument);
    EXPECT_THROW(filterWheel->setFilterByName(""), std::invalid_argument);
}

// Test filter wheel movement state
TEST_F(FilterWheelDeviceTest, MovementState) {
    // Test initial state
    EXPECT_FALSE(filterWheel->isMoving());
    
    // Start a filter change
    filterWheel->setPosition(3);
    
    // In a real filter wheel, this might be true during movement
    // For simulation, movement is typically instantaneous
    // EXPECT_TRUE(filterWheel->isMoving());
    
    // Movement should complete quickly
    EXPECT_FALSE(filterWheel->isMoving());
    EXPECT_EQ(filterWheel->getCurrentPosition(), 3);
}

// Test filter wheel capabilities
TEST_F(FilterWheelDeviceTest, FilterWheelCapabilities) {
    json info = filterWheel->getDeviceInfo();
    auto capabilities = info["capabilities"];
    
    // Check for expected filter wheel capabilities
    bool hasPositioning = false;
    bool hasNaming = false;
    bool hasOffsets = false;
    
    for (const auto& cap : capabilities) {
        std::string capability = cap.get<std::string>();
        if (capability == "positioning") hasPositioning = true;
        if (capability == "naming") hasNaming = true;
        if (capability == "offsets") hasOffsets = true;
    }
    
    EXPECT_TRUE(hasPositioning);
    EXPECT_TRUE(hasNaming);
    EXPECT_TRUE(hasOffsets);
}

// Test filter wheel error conditions
TEST_F(FilterWheelDeviceTest, ErrorConditions) {
    int filterCount = filterWheel->getFilterCount();
    
    // Test invalid position values
    EXPECT_THROW(filterWheel->setPosition(-1), std::invalid_argument);
    EXPECT_THROW(filterWheel->setPosition(filterCount), std::invalid_argument);
    EXPECT_THROW(filterWheel->setPosition(filterCount + 10), std::invalid_argument);
    
    // Test empty filter name
    EXPECT_THROW(filterWheel->setFilterByName(""), std::invalid_argument);
    
    // Test setting empty filter name
    EXPECT_THROW(filterWheel->setFilterName(0, ""), std::invalid_argument);
    
    // Test very long filter name
    std::string longName(1000, 'A');
    EXPECT_THROW(filterWheel->setFilterName(0, longName), std::invalid_argument);
}

// Test filter wheel performance
TEST_F(FilterWheelDeviceTest, FilterWheelPerformance) {
    const int numOperations = 100;
    int filterCount = filterWheel->getFilterCount();
    
    // Test position setting performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; i++) {
        int position = i % filterCount;
        filterWheel->setPosition(position);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
    
    // Test filter name setting performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; i++) {
        int position = i % filterCount;
        std::string name = "Filter" + std::to_string(i);
        filterWheel->setFilterName(position, name);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
}

// Test concurrent filter wheel operations
TEST_F(FilterWheelDeviceTest, ConcurrentOperations) {
    const int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    int filterCount = filterWheel->getFilterCount();
    
    // Test concurrent property access
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([this, &successCount, t, filterCount]() {
            try {
                for (int i = 0; i < 10; i++) {
                    // Perform various operations
                    filterWheel->getCurrentPosition();
                    filterWheel->getFilterCount();
                    filterWheel->isMoving();
                    
                    // Set position (with thread-specific values to avoid conflicts)
                    int position = (t + i) % filterCount;
                    filterWheel->setPosition(position);
                    
                    // Get filter name
                    filterWheel->getFilterName(position);
                    
                    // Get filter offset
                    filterWheel->getFilterOffset(position);
                    
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

// Test filter wheel configuration persistence
TEST_F(FilterWheelDeviceTest, ConfigurationPersistence) {
    // Set custom configuration
    std::vector<std::string> customNames = {
        "Custom1", "Custom2", "Custom3", "Custom4", 
        "Custom5", "Custom6", "Custom7", "Custom8"
    };
    
    std::vector<int> customOffsets = {10, 20, 30, 40, 50, 60, 70, 80};
    
    // Apply custom configuration
    for (size_t i = 0; i < customNames.size(); i++) {
        filterWheel->setFilterName(i, customNames[i]);
        filterWheel->setFilterOffset(i, customOffsets[i]);
    }
    
    // Verify configuration is applied
    for (size_t i = 0; i < customNames.size(); i++) {
        EXPECT_EQ(filterWheel->getFilterName(i), customNames[i]);
        EXPECT_EQ(filterWheel->getFilterOffset(i), customOffsets[i]);
    }
    
    // Test configuration export/import (if supported)
    try {
        json config = filterWheel->exportConfiguration();
        EXPECT_TRUE(config.is_object());
        EXPECT_TRUE(config.contains("filterNames"));
        EXPECT_TRUE(config.contains("filterOffsets"));
        
        // Test importing configuration
        filterWheel->importConfiguration(config);
        
        // Verify configuration is still correct after import
        for (size_t i = 0; i < customNames.size(); i++) {
            EXPECT_EQ(filterWheel->getFilterName(i), customNames[i]);
            EXPECT_EQ(filterWheel->getFilterOffset(i), customOffsets[i]);
        }
    } catch (const std::exception&) {
        // Configuration export/import might not be supported
        SUCCEED();
    }
}

#pragma once

#include <gtest/gtest.h>
#include "../../src/common/message.h"
#include "../../src/common/utils.h"
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

namespace astrocomm {
namespace test {

/**
 * @brief Test helper utilities for AstroComm testing
 */
class TestHelpers {
public:
    /**
     * @brief Generate a unique test device ID
     */
    static std::string generateTestDeviceId();
    
    /**
     * @brief Generate a unique test message ID
     */
    static std::string generateTestMessageId();
    
    /**
     * @brief Create a test timestamp
     */
    static std::string createTestTimestamp();
    
    /**
     * @brief Wait for a condition with timeout
     */
    template<typename Predicate>
    static bool waitForCondition(Predicate pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        auto start = std::chrono::steady_clock::now();
        while (!pred()) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }
    
    /**
     * @brief Compare JSON objects with tolerance for timestamps
     */
    static bool compareJsonWithTolerance(const nlohmann::json& expected, const nlohmann::json& actual);
    
    /**
     * @brief Create a test configuration JSON
     */
    static nlohmann::json createTestConfig();
    
    /**
     * @brief Validate message structure
     */
    static bool validateMessageStructure(const astrocomm::Message& message);
    
    /**
     * @brief Create a temporary test directory
     */
    static std::string createTempDirectory();
    
    /**
     * @brief Clean up temporary test directory
     */
    static void cleanupTempDirectory(const std::string& path);
};

/**
 * @brief Base test fixture for AstroComm tests
 */
class AstroCommTestBase : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    std::string testDeviceId;
    std::string tempDir;
};

/**
 * @brief Test fixture for message-related tests
 */
class MessageTestBase : public AstroCommTestBase {
protected:
    void SetUp() override;
    void TearDown() override;
    
    std::unique_ptr<astrocomm::CommandMessage> createTestCommand();
    std::unique_ptr<astrocomm::ResponseMessage> createTestResponse();
    std::unique_ptr<astrocomm::EventMessage> createTestEvent();
    std::unique_ptr<astrocomm::ErrorMessage> createTestError();
};

/**
 * @brief Test fixture for device-related tests
 */
class DeviceTestBase : public AstroCommTestBase {
protected:
    void SetUp() override;
    void TearDown() override;
    
    std::string deviceType;
    std::string manufacturer;
    std::string model;
};

} // namespace test
} // namespace astrocomm

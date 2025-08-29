#pragma once

#include <gtest/gtest.h>
#include <string>
#include <functional>
#include <chrono>
#include <thread>

namespace astrocomm {
namespace test {

/**
 * @brief Simple test helpers that don't depend on complex APIs
 */
class SimpleHelpers {
public:
    /**
     * @brief Generate a simple test ID
     */
    static std::string generateTestId();
    
    /**
     * @brief Get current timestamp as ISO string
     */
    static std::string getCurrentTimestamp();
    
    /**
     * @brief Wait for a condition to become true
     */
    static bool waitForCondition(std::function<bool()> condition, 
                                int timeoutMs = 5000, 
                                int intervalMs = 10);
    
    /**
     * @brief Create a temporary directory for testing
     */
    static std::string createTempDirectory();
    
    /**
     * @brief Clean up temporary directory
     */
    static void cleanupTempDirectory(const std::string& path);
};

/**
 * @brief Simple base class for tests
 */
class SimpleTestBase : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    std::string testId;
    std::chrono::steady_clock::time_point startTime;
};

} // namespace test
} // namespace astrocomm

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <hydrogen/core/configuration/stdio_config_manager.h>
#include <hydrogen/core/logging/stdio_logger.h>
#include <hydrogen/core/messaging/message.h>

using namespace hydrogen::core;
using namespace testing;

/**
 * @brief Test fixture for stdio server functionality (simplified)
 */
class StdioServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configure logging for tests
        StdioLogger::LoggerConfig logConfig;
        logConfig.enableConsoleLogging = false;
        logConfig.enableFileLogging = false;
        logConfig.enableDebugMode = false;

        auto& logger = getGlobalStdioLogger();
        logger.updateConfig(logConfig);
        logger.resetMetrics();

        // Setup basic configuration
        auto& configManager = getGlobalStdioConfigManager();
        config_ = configManager.createConfig(StdioConfigManager::ConfigPreset::DEFAULT);
    }

    void TearDown() override {
        // Reset logger
        auto& logger = getGlobalStdioLogger();
        logger.resetMetrics();
    }

    StdioConfig config_;
};

/**
 * @brief Test basic stdio configuration
 */
TEST_F(StdioServerTest, BasicConfiguration) {
    // Test that configuration was set up properly
    EXPECT_GT(config_.bufferSize, 0);
    EXPECT_GT(config_.readTimeout.count(), 0);
    EXPECT_GT(config_.writeTimeout.count(), 0);
}

/**
 * @brief Test stdio logger functionality
 */
TEST_F(StdioServerTest, LoggerFunctionality) {
    auto& logger = getGlobalStdioLogger();

    // Test that logger is working
    auto metrics = logger.getMetrics();
    EXPECT_GE(metrics.totalMessages.load(), 0);
}

/**
 * @brief Test stdio config manager functionality
 */
TEST_F(StdioServerTest, ConfigManagerFunctionality) {
    auto& configManager = getGlobalStdioConfigManager();

    // Test creating different config presets
    auto defaultConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::DEFAULT);
    EXPECT_GT(defaultConfig.bufferSize, 0);

    auto highPerfConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::HIGH_PERFORMANCE);
    EXPECT_GT(highPerfConfig.bufferSize, 0);
}













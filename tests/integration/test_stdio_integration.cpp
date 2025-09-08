#include <gtest/gtest.h>
#include "hydrogen/core/device/device_communicator.h"
#include "hydrogen/core/configuration/stdio_config_manager.h"
#include "hydrogen/core/logging/stdio_logger.h"
#include "hydrogen/core/messaging/message.h"
#include "hydrogen/core/communication/infrastructure/protocol_communicators.h"
#include <thread>
#include <chrono>
#include <future>

// Use core types for stdio communication testing
using namespace hydrogen::core;

/**
 * @brief Integration test fixture for stdio communication functionality
 */
class StdioIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configure logging for integration tests
        StdioLogger::LoggerConfig logConfig;
        logConfig.enableConsoleLogging = false;
        logConfig.enableFileLogging = false;
        logConfig.enableDebugMode = false;

        auto& logger = getGlobalStdioLogger();
        logger.updateConfig(logConfig);
        logger.resetMetrics();

        // Create configurations
        auto& configManager = getGlobalStdioConfigManager();
        config_ = configManager.createConfig(StdioConfigManager::ConfigPreset::DEFAULT);
        config_.enableMessageValidation = true;

        // Reset tracking variables
        receivedMessages_.clear();
        errors_.clear();
    }

    void TearDown() override {
        if (communicator_) {
            communicator_->stop();
        }
    }

    StdioConfig config_;
    std::unique_ptr<StdioCommunicator> communicator_;

    // Message tracking
    std::vector<std::string> receivedMessages_;
    std::vector<std::string> errors_;
    
    void setupCommunicator() {
        communicator_ = ProtocolCommunicatorFactory::createStdioCommunicator(config_);

        communicator_->setMessageHandler([this](const std::string& message) {
            receivedMessages_.push_back(message);
        });

        communicator_->setErrorHandler([this](const std::string& error) {
            errors_.push_back(error);
        });

        ASSERT_TRUE(communicator_->start());
    }
};

/**
 * @brief Test basic stdio communicator setup
 */
TEST_F(StdioIntegrationTest, BasicCommunicatorSetup) {
    setupCommunicator();

    // Verify communicator is active
    EXPECT_TRUE(communicator_->isActive());

    // Test basic functionality
    EXPECT_EQ(communicator_->getLinesSent(), 0);
    EXPECT_EQ(communicator_->getLinesReceived(), 0);
}

/**
 * @brief Test message sending functionality
 */
TEST_F(StdioIntegrationTest, MessageSending) {
    setupCommunicator();

    // Test sending a simple string message
    std::string testMessage = "Hello, stdio!";
    EXPECT_TRUE(communicator_->sendMessage(testMessage));

    // Test sending a JSON message
    nlohmann::json jsonMessage;
    jsonMessage["command"] = "ping";
    jsonMessage["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    EXPECT_TRUE(communicator_->sendMessage(jsonMessage));

    // Verify message count increased
    EXPECT_GT(communicator_->getLinesSent(), 0);
}

/**
 * @brief Test stdio communicator configuration
 */
TEST_F(StdioIntegrationTest, CommunicatorConfiguration) {
    setupCommunicator();

    // Test configuration access
    const auto& currentConfig = communicator_->getConfig();
    EXPECT_GT(currentConfig.bufferSize, 0);
    EXPECT_GT(currentConfig.readTimeout.count(), 0);

    // Test configuration update
    StdioConfig newConfig = currentConfig;
    newConfig.bufferSize = 16384;
    communicator_->updateConfig(newConfig);

    const auto& updatedConfig = communicator_->getConfig();
    EXPECT_EQ(updatedConfig.bufferSize, 16384);
}

/**
 * @brief Test error handling in stdio communication
 */
TEST_F(StdioIntegrationTest, ErrorHandling) {
    setupCommunicator();

    // Clear error tracking
    errors_.clear();

    // Test that communicator handles errors gracefully
    EXPECT_TRUE(communicator_->isActive());

    // Test sending valid message after setup
    std::string validMessage = "valid_message";
    EXPECT_TRUE(communicator_->sendMessage(validMessage));

    // Verify system is still active after operations
    EXPECT_TRUE(communicator_->isActive());
}

/**
 * @brief Test stdio communicator lifecycle
 */
TEST_F(StdioIntegrationTest, CommunicatorLifecycle) {
    setupCommunicator();

    // Test that communicator starts properly
    EXPECT_TRUE(communicator_->isActive());

    // Test stop and restart
    communicator_->stop();
    EXPECT_FALSE(communicator_->isActive());

    // Test restart
    EXPECT_TRUE(communicator_->start());
    EXPECT_TRUE(communicator_->isActive());
}

/**
 * @brief Test stdio communicator input functionality
 */
TEST_F(StdioIntegrationTest, InputFunctionality) {
    setupCommunicator();

    // Test input checking functionality
    EXPECT_FALSE(communicator_->hasInput()); // Should be false initially

    // Test that communicator is ready for input operations
    EXPECT_TRUE(communicator_->isActive());
}



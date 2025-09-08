#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../framework/mock_stdio_communicator.h"
#include <hydrogen/core/configuration/stdio_config_manager.h>
#include <hydrogen/core/logging/stdio_logger.h>
#include <chrono>
#include <thread>

using namespace hydrogen::core;
using namespace hydrogen::testing;
using namespace std::chrono_literals;

/**
 * @brief Test fixture for mock stdio communicator tests
 */
class MockStdioCommunicatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test configuration with short timeouts
        config_.enableLineBuffering = true;
        config_.enableBinaryMode = false;
        config_.lineTerminator = "\n";
        config_.enableFlush = true;
        config_.bufferSize = 4096;
        config_.readTimeout = std::chrono::milliseconds(100);
        config_.writeTimeout = std::chrono::milliseconds(100);
        config_.enableMessageLogging = true;
        config_.enableMessageValidation = true;
        
        // Configure logging for tests
        StdioLogger::LoggerConfig logConfig;
        logConfig.enableConsoleLogging = false;
        logConfig.enableFileLogging = false;
        logConfig.enableDebugMode = true;
        
        auto& logger = getGlobalStdioLogger();
        logger.updateConfig(logConfig);
        logger.resetMetrics();
    }
    
    void TearDown() override {
        if (communicator_) {
            communicator_->stop();
        }
    }
    
    StdioConfig config_;
    std::unique_ptr<MockStdioCommunicator> communicator_;
    std::vector<std::string> receivedMessages_;
    std::vector<std::string> errors_;
    
    void setupCommunicator() {
        communicator_ = MockStdioCommunicatorFactory::create(config_);

        communicator_->setMessageHandler([this](const std::string& message) {
            receivedMessages_.push_back(message);
        });

        communicator_->setErrorHandler([this](const std::string& error) {
            errors_.push_back(error);
        });
    }
};

/**
 * @brief Test basic mock communicator creation and configuration
 */
TEST_F(MockStdioCommunicatorTest, BasicCreationAndConfiguration) {
    setupCommunicator();
    
    ASSERT_NE(communicator_, nullptr);
    EXPECT_FALSE(communicator_->isActive());
    EXPECT_EQ(communicator_->getLinesSent(), 0);
    EXPECT_EQ(communicator_->getLinesReceived(), 0);
    EXPECT_FALSE(communicator_->hasInput());
}

/**
 * @brief Test stdio communicator start and stop (non-blocking)
 */
TEST_F(MockStdioCommunicatorTest, StartAndStop) {
    setupCommunicator();
    
    // Test start
    EXPECT_TRUE(communicator_->start());
    EXPECT_TRUE(communicator_->isActive());
    
    // Test stop
    communicator_->stop();
    EXPECT_FALSE(communicator_->isActive());
    
    // Test multiple start/stop cycles
    EXPECT_TRUE(communicator_->start());
    EXPECT_TRUE(communicator_->isActive());
    communicator_->stop();
    EXPECT_FALSE(communicator_->isActive());
}

/**
 * @brief Test message sending functionality
 */
TEST_F(MockStdioCommunicatorTest, MessageSending) {
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());

    // Test string message
    std::string testMessage = "Hello, World!";
    EXPECT_TRUE(communicator_->sendMessage(testMessage));
    
    auto sentMessages = communicator_->getSentMessages();
    ASSERT_EQ(sentMessages.size(), 1);
    EXPECT_EQ(sentMessages[0], testMessage);

    // Test JSON message
    nlohmann::json jsonMessage = {
        {"command", "test"},
        {"data", "test_data"}
    };
    EXPECT_TRUE(communicator_->sendMessage(jsonMessage));
    
    sentMessages = communicator_->getSentMessages();
    ASSERT_EQ(sentMessages.size(), 2);
    EXPECT_EQ(sentMessages[1], jsonMessage.dump());
    
    EXPECT_EQ(communicator_->getLinesSent(), 2);
}

/**
 * @brief Test message receiving functionality
 */
TEST_F(MockStdioCommunicatorTest, MessageReceiving) {
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());

    // Simulate input
    std::string testInput = "test input message";
    communicator_->simulateInput(testInput);
    
    EXPECT_TRUE(communicator_->hasInput());
    EXPECT_EQ(communicator_->getLinesReceived(), 1);
    
    // Read the input
    std::string receivedMessage = communicator_->readLine();
    EXPECT_EQ(receivedMessage, testInput);
    
    // Verify message handler was called
    ASSERT_EQ(receivedMessages_.size(), 1);
    EXPECT_EQ(receivedMessages_[0], testInput);
}

/**
 * @brief Test multiple message handling
 */
TEST_F(MockStdioCommunicatorTest, MultipleMessages) {
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());

    // Simulate multiple inputs
    std::vector<std::string> testInputs = {
        "message 1",
        "message 2", 
        "message 3"
    };
    
    communicator_->simulateMultipleInputs(testInputs);
    
    EXPECT_EQ(communicator_->getLinesReceived(), 3);
    EXPECT_TRUE(communicator_->hasInput());
    
    // Read all messages
    for (size_t i = 0; i < testInputs.size(); ++i) {
        std::string received = communicator_->readLine();
        EXPECT_EQ(received, testInputs[i]);
    }
    
    EXPECT_FALSE(communicator_->hasInput());
    
    // Verify all messages were handled
    ASSERT_EQ(receivedMessages_.size(), 3);
    for (size_t i = 0; i < testInputs.size(); ++i) {
        EXPECT_EQ(receivedMessages_[i], testInputs[i]);
    }
}

/**
 * @brief Test timeout behavior
 */
TEST_F(MockStdioCommunicatorTest, TimeoutBehavior) {
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());

    // Try to read without any input - should timeout
    auto startTime = std::chrono::steady_clock::now();
    std::string result = communicator_->readLine();
    auto endTime = std::chrono::steady_clock::now();
    
    EXPECT_TRUE(result.empty());
    
    // Should have waited approximately the timeout duration
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    EXPECT_GE(duration.count(), 90); // Allow some tolerance
    EXPECT_LE(duration.count(), 200); // But not too much
}

/**
 * @brief Test error handling
 */
TEST_F(MockStdioCommunicatorTest, ErrorHandling) {
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());

    // Simulate an error
    std::string testError = "Test error message";
    communicator_->simulateError(testError);
    
    // Verify error handler was called
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(errors_[0], testError);
}

/**
 * @brief Test inactive communicator behavior
 */
TEST_F(MockStdioCommunicatorTest, InactiveCommunicator) {
    setupCommunicator();
    
    // Don't start the communicator
    EXPECT_FALSE(communicator_->isActive());
    
    // Operations should fail or return empty results
    EXPECT_FALSE(communicator_->sendMessage(std::string("test")));
    EXPECT_TRUE(communicator_->readLine().empty());
    EXPECT_FALSE(communicator_->hasInput());
}

/**
 * @brief Test cleanup functionality
 */
TEST_F(MockStdioCommunicatorTest, CleanupFunctionality) {
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());

    // Send some messages and simulate input
    communicator_->sendMessage(std::string("test1"));
    communicator_->sendMessage(std::string("test2"));
    communicator_->simulateInput("input1");
    communicator_->simulateInput("input2");
    
    EXPECT_EQ(communicator_->getSentMessages().size(), 2);
    EXPECT_EQ(communicator_->getLinesReceived(), 2);
    
    // Clear sent messages
    communicator_->clearSentMessages();
    EXPECT_EQ(communicator_->getSentMessages().size(), 0);
    
    // Clear input queue
    communicator_->clearInputQueue();
    EXPECT_EQ(communicator_->getLinesReceived(), 0);
    EXPECT_FALSE(communicator_->hasInput());
}

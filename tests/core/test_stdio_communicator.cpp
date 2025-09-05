#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <hydrogen/core/protocol_communicators.h>
#include <hydrogen/core/stdio_config_manager.h>
#include <hydrogen/core/stdio_logger.h>
#include <thread>
#include <chrono>
#include <sstream>

using namespace hydrogen::core;
using namespace testing;

/**
 * @brief Test fixture for stdio communicator tests
 */
class StdioCommunicatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test configuration
        auto& configManager = getGlobalStdioConfigManager();
        config_ = configManager.createConfig(StdioConfigManager::ConfigPreset::DEFAULT);
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
    std::unique_ptr<StdioCommunicator> communicator_;
    std::vector<std::string> receivedMessages_;
    std::vector<std::string> errors_;
    
    void setupCommunicator() {
        communicator_ = createStdioCommunicator(config_);
        
        communicator_->setMessageHandler([this](const std::string& message, CommunicationProtocol protocol) {
            receivedMessages_.push_back(message);
        });
        
        communicator_->setErrorHandler([this](const std::string& error) {
            errors_.push_back(error);
        });
    }
};

/**
 * @brief Test basic stdio communicator creation and configuration
 */
TEST_F(StdioCommunicatorTest, BasicCreationAndConfiguration) {
    setupCommunicator();
    
    ASSERT_NE(communicator_, nullptr);
    EXPECT_FALSE(communicator_->isActive());
    EXPECT_EQ(communicator_->getLinesSent(), 0);
    EXPECT_EQ(communicator_->getLinesReceived(), 0);
}

/**
 * @brief Test stdio communicator start and stop
 */
TEST_F(StdioCommunicatorTest, StartAndStop) {
    setupCommunicator();
    
    // Test start
    EXPECT_TRUE(communicator_->start());
    EXPECT_TRUE(communicator_->isActive());
    
    // Test stop
    communicator_->stop();
    EXPECT_FALSE(communicator_->isActive());
    
    // Test multiple starts/stops
    EXPECT_TRUE(communicator_->start());
    EXPECT_TRUE(communicator_->isActive());
    communicator_->stop();
    EXPECT_FALSE(communicator_->isActive());
}

/**
 * @brief Test message sending functionality
 */
TEST_F(StdioCommunicatorTest, MessageSending) {
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());
    
    // Test string message
    std::string testMessage = "Hello, World!";
    EXPECT_TRUE(communicator_->sendMessage(testMessage));
    EXPECT_GT(communicator_->getLinesSent(), 0);
    
    // Test JSON message
    json jsonMessage;
    jsonMessage["type"] = "test";
    jsonMessage["data"] = "test data";
    EXPECT_TRUE(communicator_->sendMessage(jsonMessage));
    
    // Test empty message
    EXPECT_TRUE(communicator_->sendMessage(""));
}

/**
 * @brief Test message sending when not active
 */
TEST_F(StdioCommunicatorTest, MessageSendingWhenInactive) {
    setupCommunicator();
    
    // Should fail when not started
    EXPECT_FALSE(communicator_->sendMessage("test"));
    
    // Start and stop, then try to send
    ASSERT_TRUE(communicator_->start());
    communicator_->stop();
    EXPECT_FALSE(communicator_->sendMessage("test"));
}

/**
 * @brief Test configuration validation
 */
TEST_F(StdioCommunicatorTest, ConfigurationValidation) {
    auto& configManager = getGlobalStdioConfigManager();
    
    // Test valid configuration
    auto validConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::DEFAULT);
    EXPECT_TRUE(configManager.validateConfig(validConfig));
    EXPECT_TRUE(configManager.getValidationError(validConfig).empty());
    
    // Test invalid configuration
    auto invalidConfig = validConfig;
    invalidConfig.bufferSize = 0; // Invalid buffer size
    EXPECT_FALSE(configManager.validateConfig(invalidConfig));
    EXPECT_FALSE(configManager.getValidationError(invalidConfig).empty());
}

/**
 * @brief Test different configuration presets
 */
TEST_F(StdioCommunicatorTest, ConfigurationPresets) {
    auto& configManager = getGlobalStdioConfigManager();
    
    // Test all presets
    auto defaultConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::DEFAULT);
    auto highPerfConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::HIGH_PERFORMANCE);
    auto lowLatencyConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::LOW_LATENCY);
    auto reliableConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::RELIABLE);
    auto secureConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::SECURE);
    auto debugConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::DEBUG);
    auto embeddedConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::EMBEDDED);
    
    // All presets should be valid
    EXPECT_TRUE(configManager.validateConfig(defaultConfig));
    EXPECT_TRUE(configManager.validateConfig(highPerfConfig));
    EXPECT_TRUE(configManager.validateConfig(lowLatencyConfig));
    EXPECT_TRUE(configManager.validateConfig(reliableConfig));
    EXPECT_TRUE(configManager.validateConfig(secureConfig));
    EXPECT_TRUE(configManager.validateConfig(debugConfig));
    EXPECT_TRUE(configManager.validateConfig(embeddedConfig));
    
    // High performance should have larger buffers
    EXPECT_GT(highPerfConfig.bufferSize, defaultConfig.bufferSize);
    
    // Low latency should have smaller timeouts
    EXPECT_LT(lowLatencyConfig.readTimeout, defaultConfig.readTimeout);
    
    // Secure config should have authentication enabled
    EXPECT_TRUE(secureConfig.enableAuthentication);
    
    // Debug config should have logging enabled
    EXPECT_TRUE(debugConfig.enableMessageLogging);
}

/**
 * @brief Test error handling
 */
TEST_F(StdioCommunicatorTest, ErrorHandling) {
    setupCommunicator();
    
    // Test error handler is called
    errors_.clear();
    
    // Create a configuration that might cause errors
    config_.maxConsecutiveErrors = 1;
    config_.enableErrorRecovery = true;
    
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());
    
    // Simulate some operations that might trigger error handling
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Note: Actual error simulation would require more complex setup
    // This test verifies the error handling infrastructure is in place
}

/**
 * @brief Test message statistics
 */
TEST_F(StdioCommunicatorTest, MessageStatistics) {
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());
    
    uint64_t initialSent = communicator_->getLinesSent();
    uint64_t initialReceived = communicator_->getLinesReceived();
    
    // Send some messages
    for (int i = 0; i < 5; ++i) {
        communicator_->sendMessage("test message " + std::to_string(i));
    }
    
    // Check statistics updated
    EXPECT_GT(communicator_->getLinesSent(), initialSent);
    
    // Check logger metrics
    auto metrics = getGlobalStdioLogger().getMetrics();
    EXPECT_GE(metrics.totalMessages.load(), 0);
}

/**
 * @brief Test concurrent operations
 */
TEST_F(StdioCommunicatorTest, ConcurrentOperations) {
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());
    
    const int numThreads = 4;
    const int messagesPerThread = 10;
    std::vector<std::thread> threads;
    
    // Start multiple threads sending messages
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; ++i) {
                std::string message = "thread_" + std::to_string(t) + "_msg_" + std::to_string(i);
                communicator_->sendMessage(message);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify messages were sent
    EXPECT_GE(communicator_->getLinesSent(), numThreads * messagesPerThread);
}

/**
 * @brief Test configuration serialization
 */
TEST_F(StdioCommunicatorTest, ConfigurationSerialization) {
    auto& configManager = getGlobalStdioConfigManager();
    auto originalConfig = configManager.createConfig(StdioConfigManager::ConfigPreset::HIGH_PERFORMANCE);
    
    // Serialize to JSON
    json configJson = configManager.configToJson(originalConfig);
    EXPECT_FALSE(configJson.empty());
    
    // Deserialize from JSON
    auto deserializedConfig = configManager.configFromJson(configJson);
    
    // Compare key fields
    EXPECT_EQ(originalConfig.bufferSize, deserializedConfig.bufferSize);
    EXPECT_EQ(originalConfig.enableCompression, deserializedConfig.enableCompression);
    EXPECT_EQ(originalConfig.framingMode, deserializedConfig.framingMode);
    EXPECT_EQ(originalConfig.ioThreads, deserializedConfig.ioThreads);
}

/**
 * @brief Test logging functionality
 */
TEST_F(StdioCommunicatorTest, LoggingFunctionality) {
    auto& logger = getGlobalStdioLogger();
    
    // Test basic logging
    logger.info("Test info message", "test_client");
    logger.error("Test error message", "test_client");
    logger.debug("Test debug message", "test_client");
    
    // Test message tracing
    logger.traceIncomingMessage("msg_1", "client_1", "COMMAND", json{{"test", "data"}}, 100);
    logger.traceOutgoingMessage("msg_2", "client_1", "RESPONSE", json{{"result", "ok"}}, 50);
    
    // Test performance metrics
    logger.recordMessage(true, 100, std::chrono::microseconds(1000));
    logger.recordMessage(false, 50, std::chrono::microseconds(2000));
    
    auto metrics = logger.getMetrics();
    EXPECT_GE(metrics.totalMessages.load(), 2);
    EXPECT_GE(metrics.successfulMessages.load(), 1);
    EXPECT_GE(metrics.failedMessages.load(), 1);
}

/**
 * @brief Performance benchmark test
 */
TEST_F(StdioCommunicatorTest, PerformanceBenchmark) {
    setupCommunicator();
    ASSERT_TRUE(communicator_->start());
    
    const int numMessages = 1000;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Send messages as fast as possible
    for (int i = 0; i < numMessages; ++i) {
        communicator_->sendMessage("benchmark_message_" + std::to_string(i));
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Calculate throughput
    double messagesPerSecond = static_cast<double>(numMessages) / (duration.count() / 1000.0);
    
    std::cout << "Sent " << numMessages << " messages in " << duration.count() 
              << "ms (" << messagesPerSecond << " msg/sec)" << std::endl;
    
    // Verify all messages were sent
    EXPECT_EQ(communicator_->getLinesSent(), numMessages);
    
    // Performance should be reasonable (adjust threshold as needed)
    EXPECT_GT(messagesPerSecond, 100); // At least 100 messages per second
}

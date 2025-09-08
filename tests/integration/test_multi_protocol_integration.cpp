#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <hydrogen/core/device/device_communicator.h>
#include <hydrogen/core/configuration/stdio_config_manager.h>
#include <hydrogen/core/configuration/fifo_config_manager.h>
#include <hydrogen/core/messaging/message_transformer.h>
#include <hydrogen/core/messaging/message.h>
#include "../framework/mock_stdio_communicator.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <future>
#include <memory>

using namespace hydrogen::core;
using namespace hydrogen::testing;
using json = nlohmann::json;

/**
 * @brief Integration test fixture for multi-protocol communication
 */
class MultiProtocolIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup configurations
        setupStdioConfig();
        setupFifoConfig();
        setupMessageTransformer();
        
        // Clear tracking variables
        receivedMessages_.clear();
        errors_.clear();
        transformedMessages_.clear();
    }
    
    void TearDown() override {
        if (stdioCommunicator_) {
            stdioCommunicator_->stop();
        }
    }
    
    void setupStdioConfig() {
        auto& configManager = getGlobalStdioConfigManager();
        stdioConfig_ = configManager.createConfig(StdioConfigManager::ConfigPreset::DEFAULT);
        stdioConfig_.enableMessageLogging = true;
        stdioConfig_.enableMessageValidation = true;
        stdioConfig_.readTimeout = std::chrono::milliseconds(100); // Short timeout for tests
        stdioConfig_.writeTimeout = std::chrono::milliseconds(100);
    }
    
    void setupFifoConfig() {
        FifoConfigManager fifoConfigManager;
        fifoConfig_ = fifoConfigManager.createConfig(FifoConfigManager::ConfigPreset::DEFAULT);
        fifoConfig_.enableMessageLogging = true;
        fifoConfig_.enableMessageValidation = true;
        fifoConfig_.readTimeout = std::chrono::milliseconds(100);
        fifoConfig_.writeTimeout = std::chrono::milliseconds(100);
    }
    
    void setupMessageTransformer() {
        messageTransformer_ = std::make_unique<MessageTransformer>();
        stdioTransformer_ = std::make_unique<StdioTransformer>();
    }
    
    void setupMockStdioCommunicator() {
        stdioCommunicator_ = MockStdioCommunicatorFactory::create(stdioConfig_);
        
        stdioCommunicator_->setMessageHandler([this](const std::string& message) {
            receivedMessages_.push_back(message);
            
            // Try to transform the message
            try {
                json messageJson = json::parse(message);
                auto transformedMessage = stdioTransformer_->fromProtocol(messageJson);
                if (transformedMessage) {
                    transformedMessages_.push_back(std::move(transformedMessage));
                }
            } catch (const std::exception& e) {
                errors_.push_back("Message transformation failed: " + std::string(e.what()));
            }
        });
        
        stdioCommunicator_->setErrorHandler([this](const std::string& error) {
            errors_.push_back(error);
        });
    }
    
    // Test data
    StdioConfig stdioConfig_;
    FifoConfig fifoConfig_;
    
    // Components
    std::unique_ptr<MockStdioCommunicator> stdioCommunicator_;
    std::unique_ptr<MessageTransformer> messageTransformer_;
    std::unique_ptr<StdioTransformer> stdioTransformer_;
    
    // Tracking
    std::vector<std::string> receivedMessages_;
    std::vector<std::string> errors_;
    std::vector<std::unique_ptr<Message>> transformedMessages_;
};

/**
 * @brief Test basic multi-protocol setup and configuration
 */
TEST_F(MultiProtocolIntegrationTest, BasicSetupAndConfiguration) {
    setupMockStdioCommunicator();
    
    ASSERT_NE(stdioCommunicator_, nullptr);
    ASSERT_NE(messageTransformer_, nullptr);
    ASSERT_NE(stdioTransformer_, nullptr);
    
    // Test configuration validation
    EXPECT_GT(stdioConfig_.bufferSize, 0);
    EXPECT_GT(stdioConfig_.readTimeout.count(), 0);
    EXPECT_TRUE(fifoConfig_.validate());
    
    // Test communicator startup
    EXPECT_TRUE(stdioCommunicator_->start());
    EXPECT_TRUE(stdioCommunicator_->isActive());
}

/**
 * @brief Test end-to-end message flow: creation -> transformation -> communication
 */
TEST_F(MultiProtocolIntegrationTest, EndToEndMessageFlow) {
    setupMockStdioCommunicator();
    ASSERT_TRUE(stdioCommunicator_->start());
    
    // Create a command message
    auto commandMessage = std::make_unique<CommandMessage>("get_status");
    commandMessage->setDeviceId("test_device_001");
    commandMessage->setMessageId("msg_001");
    commandMessage->setOriginalMessageId("original_001");
    
    json parameters = {{"timeout", 5000}, {"format", "json"}};
    commandMessage->setParameters(parameters);
    
    // Transform message to STDIO protocol
    auto transformResult = stdioTransformer_->toProtocol(*commandMessage);
    ASSERT_TRUE(transformResult.success);
    EXPECT_FALSE(transformResult.transformedData.empty());
    
    // Send the transformed message
    std::string messageStr = transformResult.transformedData.dump();
    EXPECT_TRUE(stdioCommunicator_->sendMessage(messageStr));
    
    // Simulate receiving the same message (echo scenario)
    stdioCommunicator_->simulateInput(messageStr);
    
    // Verify message was received and processed
    ASSERT_EQ(receivedMessages_.size(), 1);
    EXPECT_EQ(receivedMessages_[0], messageStr);
    
    // Verify message was transformed back
    ASSERT_EQ(transformedMessages_.size(), 1);
    EXPECT_EQ(transformedMessages_[0]->getMessageId(), "msg_001");
    EXPECT_EQ(transformedMessages_[0]->getDeviceId(), "test_device_001");
    EXPECT_EQ(transformedMessages_[0]->getOriginalMessageId(), "original_001");
}

/**
 * @brief Test protocol configuration optimization and validation
 */
TEST_F(MultiProtocolIntegrationTest, ConfigurationOptimizationAndValidation) {
    // Test STDIO configuration validation
    auto& stdioConfigManager = getGlobalStdioConfigManager();

    StdioConfig testConfig = stdioConfig_;
    testConfig.bufferSize = 512; // Small buffer
    testConfig.readTimeout = std::chrono::milliseconds(10); // Very short timeout

    // Test that configuration has reasonable values
    EXPECT_GT(testConfig.bufferSize, 0);
    EXPECT_GT(testConfig.readTimeout.count(), 0);
    EXPECT_FALSE(testConfig.lineTerminator.empty());

    // Test FIFO configuration optimization
    FifoConfigManager fifoConfigManager;

    FifoConfig suboptimalFifoConfig = fifoConfig_;
    suboptimalFifoConfig.bufferSize = 256; // Very small buffer
    suboptimalFifoConfig.readTimeout = std::chrono::milliseconds(5); // Very short timeout

    FifoConfig optimizedFifoConfig = fifoConfigManager.optimizeConfig(suboptimalFifoConfig);

    EXPECT_GT(optimizedFifoConfig.bufferSize, suboptimalFifoConfig.bufferSize);
    EXPECT_GE(optimizedFifoConfig.readTimeout, suboptimalFifoConfig.readTimeout);
    EXPECT_TRUE(optimizedFifoConfig.validate());
}

/**
 * @brief Test error handling across protocol boundaries
 */
TEST_F(MultiProtocolIntegrationTest, CrossProtocolErrorHandling) {
    setupMockStdioCommunicator();
    ASSERT_TRUE(stdioCommunicator_->start());
    
    // Test invalid JSON message
    std::string invalidJson = "{invalid json}";
    stdioCommunicator_->simulateInput(invalidJson);
    
    // Should have received the message but transformation should fail
    ASSERT_EQ(receivedMessages_.size(), 1);
    EXPECT_EQ(receivedMessages_[0], invalidJson);
    
    // Should have an error from transformation failure
    EXPECT_GT(errors_.size(), 0);
    EXPECT_THAT(errors_[0], testing::HasSubstr("transformation failed"));
    
    // Test error simulation
    std::string testError = "Simulated communication error";
    stdioCommunicator_->simulateError(testError);
    
    // Should have received the error
    EXPECT_THAT(errors_, testing::Contains(testError));
}

/**
 * @brief Test concurrent message processing
 */
TEST_F(MultiProtocolIntegrationTest, ConcurrentMessageProcessing) {
    setupMockStdioCommunicator();
    ASSERT_TRUE(stdioCommunicator_->start());
    
    const int messageCount = 10;
    std::vector<std::future<void>> futures;
    
    // Send multiple messages concurrently
    for (int i = 0; i < messageCount; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            json message = {
                {"messageId", "msg_" + std::to_string(i)},
                {"command", "test_command"},
                {"deviceId", "device_" + std::to_string(i % 3)},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            
            stdioCommunicator_->simulateInput(message.dump());
        }));
    }
    
    // Wait for all messages to be processed
    for (auto& future : futures) {
        future.wait();
    }
    
    // Give some time for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify all messages were received
    EXPECT_EQ(receivedMessages_.size(), messageCount);
    
    // Verify no errors occurred during concurrent processing
    EXPECT_TRUE(errors_.empty());
}

/**
 * @brief Test protocol statistics and monitoring
 */
TEST_F(MultiProtocolIntegrationTest, ProtocolStatisticsAndMonitoring) {
    setupMockStdioCommunicator();
    ASSERT_TRUE(stdioCommunicator_->start());
    
    // Send some messages
    const int sendCount = 5;
    for (int i = 0; i < sendCount; ++i) {
        std::string message = "test_message_" + std::to_string(i);
        EXPECT_TRUE(stdioCommunicator_->sendMessage(message));
    }
    
    // Simulate receiving some messages
    const int receiveCount = 3;
    for (int i = 0; i < receiveCount; ++i) {
        std::string message = "received_message_" + std::to_string(i);
        stdioCommunicator_->simulateInput(message);
    }
    
    // Verify statistics
    EXPECT_EQ(stdioCommunicator_->getLinesSent(), sendCount);
    EXPECT_EQ(stdioCommunicator_->getLinesReceived(), receiveCount);
    EXPECT_EQ(receivedMessages_.size(), receiveCount);
}

/**
 * @brief Test protocol lifecycle management
 */
TEST_F(MultiProtocolIntegrationTest, ProtocolLifecycleManagement) {
    setupMockStdioCommunicator();
    
    // Test initial state
    EXPECT_FALSE(stdioCommunicator_->isActive());
    
    // Test startup
    EXPECT_TRUE(stdioCommunicator_->start());
    EXPECT_TRUE(stdioCommunicator_->isActive());
    
    // Test operation while active
    EXPECT_TRUE(stdioCommunicator_->sendMessage(std::string("test_message")));
    stdioCommunicator_->simulateInput("input_message");

    EXPECT_EQ(stdioCommunicator_->getLinesSent(), 1);
    EXPECT_EQ(stdioCommunicator_->getLinesReceived(), 1);

    // Test shutdown
    stdioCommunicator_->stop();
    EXPECT_FALSE(stdioCommunicator_->isActive());

    // Test operations after shutdown
    EXPECT_FALSE(stdioCommunicator_->sendMessage(std::string("test_message_after_stop")));
    EXPECT_TRUE(stdioCommunicator_->readLine().empty());
    
    // Test restart
    EXPECT_TRUE(stdioCommunicator_->start());
    EXPECT_TRUE(stdioCommunicator_->isActive());
}

/**
 * @brief Test FIFO configuration management and validation
 */
TEST_F(MultiProtocolIntegrationTest, FifoConfigurationManagement) {
    FifoConfigManager fifoConfigManager;

    // Test default configuration creation
    FifoConfig defaultConfig = fifoConfigManager.createConfig(FifoConfigManager::ConfigPreset::DEFAULT);
    EXPECT_TRUE(defaultConfig.validate());
    EXPECT_GT(defaultConfig.bufferSize, 0);
    EXPECT_GT(defaultConfig.readTimeout.count(), 0);

    // Test high performance configuration
    FifoConfig highPerfConfig = fifoConfigManager.createConfig(FifoConfigManager::ConfigPreset::HIGH_PERFORMANCE);
    EXPECT_TRUE(highPerfConfig.validate());
    EXPECT_GE(highPerfConfig.bufferSize, defaultConfig.bufferSize);

    // Test low latency configuration
    FifoConfig lowLatencyConfig = fifoConfigManager.createConfig(FifoConfigManager::ConfigPreset::LOW_LATENCY);
    EXPECT_TRUE(lowLatencyConfig.validate());
    EXPECT_LE(lowLatencyConfig.readTimeout, defaultConfig.readTimeout);

    // Test configuration optimization
    FifoConfig suboptimalConfig = defaultConfig;
    suboptimalConfig.bufferSize = 128; // Very small
    suboptimalConfig.readTimeout = std::chrono::milliseconds(1); // Very short

    FifoConfig optimizedConfig = fifoConfigManager.optimizeConfig(suboptimalConfig);
    EXPECT_GT(optimizedConfig.bufferSize, suboptimalConfig.bufferSize);
    EXPECT_GE(optimizedConfig.readTimeout, suboptimalConfig.readTimeout);
    EXPECT_TRUE(optimizedConfig.validate());
}

/**
 * @brief Test message transformation basics
 */
TEST_F(MultiProtocolIntegrationTest, MessageTransformationBasics) {
    setupMockStdioCommunicator();
    ASSERT_TRUE(stdioCommunicator_->start());

    // Create a simple command message for testing
    auto commandMessage = std::make_unique<CommandMessage>("test_command");
    commandMessage->setDeviceId("test_device_003");
    commandMessage->setMessageId("cmd_001");

    json parameters = {{"param1", "value1"}, {"param2", 42}};
    commandMessage->setParameters(parameters);

    // Transform to STDIO protocol
    auto transformResult = stdioTransformer_->toProtocol(*commandMessage);
    ASSERT_TRUE(transformResult.success);
    EXPECT_FALSE(transformResult.transformedData.empty());

    // Verify basic transformation worked
    json transformedJson = transformResult.transformedData;
    EXPECT_TRUE(transformedJson.is_object());
    EXPECT_FALSE(transformedJson.empty());

    // Send the message through the communicator
    std::string messageStr = transformedJson.dump();
    EXPECT_TRUE(stdioCommunicator_->sendMessage(messageStr));

    // Verify message was sent
    EXPECT_EQ(stdioCommunicator_->getLinesSent(), 1);
}

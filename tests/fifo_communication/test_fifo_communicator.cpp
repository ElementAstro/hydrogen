#include <gtest/gtest.h>
#include <hydrogen/core/fifo_communicator.h>
#include <hydrogen/core/fifo_config_manager.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>

using namespace hydrogen::core;
using json = nlohmann::json;

class FifoCommunicatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test configuration
        auto& configManager = getGlobalFifoConfigManager();
        config_ = configManager.createConfig();
        
        // Use unique pipe names for each test
        static int testCounter = 0;
        std::string testId = std::to_string(++testCounter);
        
#ifdef _WIN32
        config_.windowsPipePath = "\\\\.\\pipe\\test_fifo_" + testId;
        config_.pipeType = FifoPipeType::WINDOWS_NAMED_PIPE;
#else
        config_.unixPipePath = "/tmp/test_fifo_" + testId;
        config_.pipeType = FifoPipeType::UNIX_FIFO;
#endif
        
        config_.pipeName = "test_fifo_" + testId;
        config_.connectTimeout = std::chrono::milliseconds(1000);
        config_.readTimeout = std::chrono::milliseconds(500);
        config_.writeTimeout = std::chrono::milliseconds(500);
    }
    
    void TearDown() override {
        // Clean up test pipes
#ifndef _WIN32
        if (std::filesystem::exists(config_.unixPipePath)) {
            std::filesystem::remove(config_.unixPipePath);
        }
#endif
    }
    
    FifoConfig config_;
};

// Test basic communicator creation
TEST_F(FifoCommunicatorTest, CreateCommunicator) {
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    EXPECT_FALSE(communicator->isActive());
    EXPECT_FALSE(communicator->isConnected());
    EXPECT_EQ(communicator->getConnectionState(), FifoConnectionState::DISCONNECTED);
}

// Test factory methods
TEST_F(FifoCommunicatorTest, FactoryMethods) {
    // Test default factory
    auto defaultComm = FifoCommunicatorFactory::createDefault();
    ASSERT_NE(defaultComm, nullptr);
    
    // Test preset factories
    auto highPerfComm = FifoCommunicatorFactory::createWithPreset(FifoConfigManager::ConfigPreset::HIGH_PERFORMANCE);
    ASSERT_NE(highPerfComm, nullptr);
    
    auto reliableComm = FifoCommunicatorFactory::createReliable(config_);
    ASSERT_NE(reliableComm, nullptr);
    
    auto bidirectionalComm = FifoCommunicatorFactory::createBidirectional(config_);
    ASSERT_NE(bidirectionalComm, nullptr);
}

// Test platform-specific factories
TEST_F(FifoCommunicatorTest, PlatformSpecificFactories) {
#ifdef _WIN32
    auto windowsComm = FifoCommunicatorFactory::createForWindows(config_);
    ASSERT_NE(windowsComm, nullptr);
    EXPECT_EQ(windowsComm->getConfig().pipeType, FifoPipeType::WINDOWS_NAMED_PIPE);
#else
    auto unixComm = FifoCommunicatorFactory::createForUnix(config_);
    ASSERT_NE(unixComm, nullptr);
    EXPECT_EQ(unixComm->getConfig().pipeType, FifoPipeType::UNIX_FIFO);
#endif
}

// Test communicator lifecycle
TEST_F(FifoCommunicatorTest, CommunicatorLifecycle) {
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    // Initial state
    EXPECT_FALSE(communicator->isActive());
    EXPECT_FALSE(communicator->isConnected());
    
    // Start communicator
    EXPECT_TRUE(communicator->start());
    EXPECT_TRUE(communicator->isActive());
    
    // Stop communicator
    communicator->stop();
    EXPECT_FALSE(communicator->isActive());
    EXPECT_FALSE(communicator->isConnected());
}

// Test event handlers
TEST_F(FifoCommunicatorTest, EventHandlers) {
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    std::atomic<bool> messageReceived{false};
    std::atomic<bool> errorOccurred{false};
    std::atomic<bool> connectionChanged{false};
    
    std::string receivedMessage;
    std::string errorMessage;
    bool connectionState = false;
    
    // Set up event handlers
    communicator->setMessageHandler([&](const std::string& message) {
        receivedMessage = message;
        messageReceived.store(true);
    });
    
    communicator->setErrorHandler([&](const std::string& error) {
        errorMessage = error;
        errorOccurred.store(true);
    });
    
    communicator->setConnectionHandler([&](bool connected) {
        connectionState = connected;
        connectionChanged.store(true);
    });
    
    // Start communicator to trigger connection handler
    communicator->start();
    
    // Wait for connection handler to be called
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Connection handler should have been called
    EXPECT_TRUE(connectionChanged.load());
}

// Test message formatting
TEST_F(FifoCommunicatorTest, MessageFormatting) {
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    // Test different framing modes
    std::vector<FifoFramingMode> framingModes = {
        FifoFramingMode::NEWLINE_DELIMITED,
        FifoFramingMode::JSON_LINES,
        FifoFramingMode::LENGTH_PREFIXED,
        FifoFramingMode::CUSTOM_DELIMITER,
        FifoFramingMode::NULL_TERMINATED
    };
    
    for (auto mode : framingModes) {
        FifoConfig testConfig = config_;
        testConfig.framingMode = mode;
        testConfig.customDelimiter = "|END|";
        
        auto testComm = FifoCommunicatorFactory::create(testConfig);
        ASSERT_NE(testComm, nullptr);
        
        // The actual formatting is tested internally
        // Here we just verify the communicator can be created with different modes
        EXPECT_EQ(testComm->getConfig().framingMode, mode);
    }
}

// Test statistics collection
TEST_F(FifoCommunicatorTest, StatisticsCollection) {
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    auto initialStats = communicator->getStatistics();
    EXPECT_EQ(initialStats.messagesSent.load(), 0);
    EXPECT_EQ(initialStats.messagesReceived.load(), 0);
    EXPECT_EQ(initialStats.bytesTransferred.load(), 0);
    EXPECT_EQ(initialStats.errors.load(), 0);
    
    // Start communicator
    EXPECT_TRUE(communicator->start());
    
    // Statistics should still be zero initially
    auto stats = communicator->getStatistics();
    EXPECT_EQ(stats.messagesSent.load(), 0);
    EXPECT_EQ(stats.messagesReceived.load(), 0);
    
    // Test statistics JSON serialization
    json statsJson = stats.toJson();
    EXPECT_FALSE(statsJson.empty());
    EXPECT_TRUE(statsJson.contains("messagesSent"));
    EXPECT_TRUE(statsJson.contains("messagesReceived"));
    EXPECT_TRUE(statsJson.contains("bytesTransferred"));
    EXPECT_TRUE(statsJson.contains("errors"));
    EXPECT_TRUE(statsJson.contains("messagesPerSecond"));
    EXPECT_TRUE(statsJson.contains("bytesPerSecond"));
    EXPECT_TRUE(statsJson.contains("uptimeMs"));
}

// Test health checking
TEST_F(FifoCommunicatorTest, HealthChecking) {
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    // Initially not healthy (not started)
    EXPECT_FALSE(communicator->isHealthy());
    
    // Start communicator
    EXPECT_TRUE(communicator->start());
    
    // Should be healthy after starting
    EXPECT_TRUE(communicator->isHealthy());
    
    // Get health status
    std::string healthStatus = communicator->getHealthStatus();
    EXPECT_FALSE(healthStatus.empty());
    
    // Stop communicator
    communicator->stop();
    
    // Should not be healthy after stopping
    EXPECT_FALSE(communicator->isHealthy());
}

// Test configuration updates
TEST_F(FifoCommunicatorTest, ConfigurationUpdates) {
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    // Get initial config
    auto initialConfig = communicator->getConfig();
    EXPECT_EQ(initialConfig.pipeName, config_.pipeName);
    
    // Update configuration
    FifoConfig newConfig = config_;
    newConfig.bufferSize = config_.bufferSize * 2;
    newConfig.enableDebugLogging = !config_.enableDebugLogging;
    
    communicator->updateConfig(newConfig);
    
    // Verify configuration was updated
    auto updatedConfig = communicator->getConfig();
    EXPECT_EQ(updatedConfig.bufferSize, newConfig.bufferSize);
    EXPECT_EQ(updatedConfig.enableDebugLogging, newConfig.enableDebugLogging);
}

// Test advanced features
TEST_F(FifoCommunicatorTest, AdvancedFeatures) {
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    // Test bidirectional communication
    bool bidirectionalResult = communicator->enableBidirectional();
    // Result depends on configuration
    
    // Test multiplexing
    bool multiplexingResult = communicator->enableMultiplexing();
    // Result depends on configuration
    
    // Get connected clients (should be empty for basic communicator)
    auto clients = communicator->getConnectedClients();
    // For basic communicator, this might return empty or single default client
}

// Test error conditions
TEST_F(FifoCommunicatorTest, ErrorConditions) {
    // Test with invalid configuration
    FifoConfig invalidConfig = config_;
    invalidConfig.pipeName = ""; // Invalid empty name
    
    auto communicator = FifoCommunicatorFactory::create(invalidConfig);
    ASSERT_NE(communicator, nullptr);
    
    // Starting with invalid config should fail
    EXPECT_FALSE(communicator->start());
    EXPECT_FALSE(communicator->isActive());
}

// Test message size validation
TEST_F(FifoCommunicatorTest, MessageSizeValidation) {
    config_.maxMessageSize = 1024; // Small limit for testing
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    EXPECT_TRUE(communicator->start());
    
    // Test normal size message
    std::string normalMessage(512, 'A');
    EXPECT_TRUE(communicator->sendMessage(normalMessage));
    
    // Test oversized message
    std::string oversizedMessage(2048, 'B');
    EXPECT_FALSE(communicator->sendMessage(oversizedMessage));
}

// Test JSON message sending
TEST_F(FifoCommunicatorTest, JsonMessageSending) {
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    EXPECT_TRUE(communicator->start());
    
    // Test JSON message
    json testMessage;
    testMessage["type"] = "test";
    testMessage["data"] = "Hello World";
    testMessage["timestamp"] = 1234567890;
    
    EXPECT_TRUE(communicator->sendMessage(testMessage));
}

// Test connection state management
TEST_F(FifoCommunicatorTest, ConnectionStateManagement) {
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    // Initial state
    EXPECT_EQ(communicator->getConnectionState(), FifoConnectionState::DISCONNECTED);
    
    // Start communicator
    EXPECT_TRUE(communicator->start());
    
    // Connection state should change
    auto state = communicator->getConnectionState();
    EXPECT_TRUE(state == FifoConnectionState::CONNECTED || 
                state == FifoConnectionState::CONNECTING);
    
    // Stop communicator
    communicator->stop();
    EXPECT_EQ(communicator->getConnectionState(), FifoConnectionState::DISCONNECTED);
}

// Test reconnection functionality
TEST_F(FifoCommunicatorTest, ReconnectionFunctionality) {
    config_.enableAutoReconnect = true;
    config_.maxReconnectAttempts = 3;
    config_.reconnectDelay = std::chrono::milliseconds(100);
    
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    EXPECT_TRUE(communicator->start());
    
    // Test manual reconnection
    bool reconnectResult = communicator->reconnect();
    // Result depends on current connection state
    
    // Disconnect and test auto-reconnection
    communicator->disconnect();
    EXPECT_EQ(communicator->getConnectionState(), FifoConnectionState::DISCONNECTED);
    
    // Auto-reconnection should be attempted (if enabled)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

// Test message queuing
TEST_F(FifoCommunicatorTest, MessageQueuing) {
    config_.maxQueueSize = 10;
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    EXPECT_TRUE(communicator->start());
    
    // Initially no messages
    EXPECT_FALSE(communicator->hasMessage());
    
    // Reading from empty queue should return empty string
    std::string message = communicator->readMessage();
    EXPECT_TRUE(message.empty());
}

// Performance test (basic)
TEST_F(FifoCommunicatorTest, BasicPerformanceTest) {
    config_.enablePerformanceMetrics = true;
    auto communicator = FifoCommunicatorFactory::create(config_);
    ASSERT_NE(communicator, nullptr);
    
    EXPECT_TRUE(communicator->start());
    
    // Send multiple messages quickly
    const int messageCount = 100;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < messageCount; ++i) {
        std::string message = "Test message " + std::to_string(i);
        communicator->sendMessage(message);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Basic performance check - should complete within reasonable time
    EXPECT_LT(duration.count(), 5000); // Less than 5 seconds
    
    // Check statistics
    auto stats = communicator->getStatistics();
    // Note: Actual sent count may be 0 if no real connection is established
}

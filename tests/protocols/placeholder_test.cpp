#include <gtest/gtest.h>
#include <hydrogen/core/device/device_communicator.h>

using namespace hydrogen::core;

// Test Protocol Configuration Structures
class ProtocolConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test fixtures
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

TEST_F(ProtocolConfigTest, StdioConfigTest) {
    // Test StdioConfig structure
    StdioConfig config;
    config.enableLineBuffering = true;
    config.enableBinaryMode = false;
    config.lineTerminator = "\n";
    config.enableFlush = true;
    config.bufferSize = 1024;

    EXPECT_TRUE(config.enableLineBuffering);
    EXPECT_FALSE(config.enableBinaryMode);
    EXPECT_EQ(config.lineTerminator, "\n");
    EXPECT_TRUE(config.enableFlush);
    EXPECT_EQ(config.bufferSize, 1024);
}

TEST_F(ProtocolConfigTest, CommunicationProtocolEnum) {
    // Test that all protocol enums are defined
    CommunicationProtocol websocket = CommunicationProtocol::WEBSOCKET;
    CommunicationProtocol tcp = CommunicationProtocol::TCP;
    CommunicationProtocol stdio = CommunicationProtocol::STDIO;
    CommunicationProtocol grpc = CommunicationProtocol::GRPC;
    CommunicationProtocol mqtt = CommunicationProtocol::MQTT;
    CommunicationProtocol zmq = CommunicationProtocol::ZEROMQ;

    EXPECT_NE(websocket, tcp);
    EXPECT_NE(stdio, grpc);
    EXPECT_NE(mqtt, zmq);
}


// Test Protocol Message Structures
TEST_F(ProtocolConfigTest, CommunicationMessageTest) {
    // Test CommunicationMessage structure
    CommunicationMessage message;
    message.messageId = "test-123";
    message.deviceId = "device-456";
    message.command = "ping";
    message.payload = {{"data", "test"}};

    EXPECT_EQ(message.messageId, "test-123");
    EXPECT_EQ(message.deviceId, "device-456");
    EXPECT_EQ(message.command, "ping");
    EXPECT_TRUE(message.payload.contains("data"));
    EXPECT_EQ(message.payload["data"], "test");
}

// Test Protocol Factory Pattern
TEST_F(ProtocolConfigTest, ProtocolFactoryTest) {
    // Test that we can create different protocol configurations
    StdioConfig stdioConfig;
    stdioConfig.bufferSize = 2048;
    stdioConfig.enableFlush = false;

    EXPECT_EQ(stdioConfig.bufferSize, 2048);
    EXPECT_FALSE(stdioConfig.enableFlush);

    // Test framing modes
    EXPECT_NE(StdioConfig::FramingMode::NONE, StdioConfig::FramingMode::JSON_LINES);
    EXPECT_NE(StdioConfig::FramingMode::DELIMITER, StdioConfig::FramingMode::LENGTH_PREFIX);
}

// Test Protocol Response Structures
TEST_F(ProtocolConfigTest, CommunicationResponseTest) {
    // Test CommunicationResponse structure
    CommunicationResponse response;
    response.messageId = "response-123";
    response.success = true;
    response.errorCode = "0";
    response.errorMessage = "";
    response.payload = {{"result", "success"}};

    EXPECT_EQ(response.messageId, "response-123");
    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.errorCode, "0");
    EXPECT_TRUE(response.errorMessage.empty());
    EXPECT_TRUE(response.payload.contains("result"));
    EXPECT_EQ(response.payload["result"], "success");
}

// Test Protocol Statistics
TEST_F(ProtocolConfigTest, CommunicationStatsTest) {
    // Test CommunicationStats structure
    CommunicationStats stats;
    stats.messagesSent = 100;
    stats.messagesReceived = 95;
    stats.messagesTimeout = 5;
    stats.messagesError = 10;
    stats.averageResponseTime = 50.5;
    stats.minResponseTime = 10.0;
    stats.maxResponseTime = 200.0;
    stats.lastActivity = std::chrono::system_clock::now();

    EXPECT_EQ(stats.messagesSent, 100);
    EXPECT_EQ(stats.messagesReceived, 95);
    EXPECT_EQ(stats.messagesTimeout, 5);
    EXPECT_EQ(stats.messagesError, 10);
    EXPECT_DOUBLE_EQ(stats.averageResponseTime, 50.5);
    EXPECT_DOUBLE_EQ(stats.minResponseTime, 10.0);
    EXPECT_DOUBLE_EQ(stats.maxResponseTime, 200.0);
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <hydrogen/device/websocket_device.h>
#include <hydrogen/device/device_logger.h>
#include <thread>
#include <chrono>

using namespace hydrogen::device;
using namespace testing;

class MockWebSocketDevice : public WebSocketDevice {
public:
    MockWebSocketDevice(const std::string& deviceId)
        : WebSocketDevice(deviceId, "mock", "Test", "MockDevice") {}

    MOCK_METHOD(void, handleMessage, (const std::string& message), (override));
    MOCK_METHOD(void, handleConnectionError, (const std::string& error), (override));
};

class WebSocketDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<MockWebSocketDevice>("test_device");
        DeviceLogger::getInstance().setLogLevel(LogLevel::DEBUG);
    }

    void TearDown() override {
        if (device) {
            device->stop();
            device->disconnect();
        }
    }

    std::unique_ptr<MockWebSocketDevice> device;
};

TEST_F(WebSocketDeviceTest, InitialState) {
    EXPECT_FALSE(device->isRunning());
    EXPECT_FALSE(device->isConnected());
    EXPECT_EQ(device->getDeviceId(), "test_device");
    EXPECT_EQ(device->getDeviceType(), "mock");
}

TEST_F(WebSocketDeviceTest, StartStop) {
    EXPECT_TRUE(device->start());
    EXPECT_TRUE(device->isRunning());
    
    device->stop();
    EXPECT_FALSE(device->isRunning());
}

TEST_F(WebSocketDeviceTest, ConnectionTimeout) {
    device->setConnectionTimeout(1000); // 1 second timeout
    
    auto start = std::chrono::steady_clock::now();
    bool connected = device->connect("nonexistent.host", 12345, 1000);
    auto end = std::chrono::steady_clock::now();
    
    EXPECT_FALSE(connected);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 900); // Should take at least close to the timeout
    EXPECT_LE(duration, 2000); // But not too much longer
}

TEST_F(WebSocketDeviceTest, HeartbeatConfiguration) {
    device->setHeartbeatInterval(5000); // 5 seconds
    
    // Test that heartbeat can be disabled
    device->setHeartbeatInterval(0);
    
    // Test that heartbeat can be re-enabled
    device->setHeartbeatInterval(10000);
}

TEST_F(WebSocketDeviceTest, ConnectionStats) {
    auto stats = device->getConnectionStats();
    
    EXPECT_TRUE(stats.contains("connected"));
    EXPECT_TRUE(stats.contains("messages_sent"));
    EXPECT_TRUE(stats.contains("messages_received"));
    EXPECT_TRUE(stats.contains("connection_errors"));
    EXPECT_TRUE(stats.contains("server_host"));
    EXPECT_TRUE(stats.contains("server_port"));
    
    EXPECT_FALSE(stats["connected"]);
    EXPECT_EQ(stats["messages_sent"], 0);
    EXPECT_EQ(stats["messages_received"], 0);
}

TEST_F(WebSocketDeviceTest, MessageSending) {
    // Test message sending when not connected
    EXPECT_TRUE(device->sendMessage("test message")); // Should queue the message
    
    auto stats = device->getConnectionStats();
    // Message should be queued but not actually sent since not connected
}

TEST_F(WebSocketDeviceTest, ErrorHandling) {
    EXPECT_CALL(*device, handleConnectionError(_))
        .Times(AtLeast(1));
    
    // Try to connect to invalid host to trigger error
    device->connect("invalid.host", 99999, 1000);
}

TEST_F(WebSocketDeviceTest, DeviceRegistration) {
    // Test device registration when not connected
    EXPECT_FALSE(device->registerDevice());
}

TEST_F(WebSocketDeviceTest, MultipleStartStop) {
    // Test multiple start/stop cycles
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(device->start());
        EXPECT_TRUE(device->isRunning());
        
        device->stop();
        EXPECT_FALSE(device->isRunning());
    }
}

TEST_F(WebSocketDeviceTest, ThreadSafety) {
    device->start();
    
    // Test concurrent operations
    std::vector<std::thread> threads;
    
    // Thread 1: Send messages
    threads.emplace_back([this]() {
        for (int i = 0; i < 100; ++i) {
            device->sendMessage("message " + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Thread 2: Get stats
    threads.emplace_back([this]() {
        for (int i = 0; i < 50; ++i) {
            auto stats = device->getConnectionStats();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    
    // Thread 3: Set configuration
    threads.emplace_back([this]() {
        for (int i = 0; i < 25; ++i) {
            device->setHeartbeatInterval(1000 + i * 100);
            device->setConnectionTimeout(5000 + i * 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    });
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    device->stop();
}

class WebSocketDeviceIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // These tests would require a real WebSocket server
        // For now, we'll skip them unless we have a test server
        if (!hasTestServer()) {
            GTEST_SKIP() << "WebSocket test server not available";
        }
    }
    
private:
    bool hasTestServer() {
        // Check if test server is available
        // This could be implemented to check for a local test server
        return false; // Skip integration tests for now
    }
};

TEST_F(WebSocketDeviceIntegrationTest, RealConnection) {
    auto device = std::make_unique<WebSocketDevice>("integration_test", "test", "Test", "Integration");
    
    // This would test against a real WebSocket server
    // EXPECT_TRUE(device->connect("localhost", 8080));
    // EXPECT_TRUE(device->isConnected());
    // EXPECT_TRUE(device->registerDevice());
}

// Performance tests
class WebSocketDevicePerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<WebSocketDevice>("perf_test", "performance", "Test", "Performance");
    }
    
    std::unique_ptr<WebSocketDevice> device;
};

TEST_F(WebSocketDevicePerformanceTest, MessageThroughput) {
    device->start();
    
    const int messageCount = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < messageCount; ++i) {
        device->sendMessage("performance test message " + std::to_string(i));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double messagesPerSecond = (messageCount * 1000000.0) / duration.count();
    
    // Log performance results
    std::cout << "Message throughput: " << messagesPerSecond << " messages/second" << std::endl;
    
    // Should be able to handle at least 1000 messages per second
    EXPECT_GT(messagesPerSecond, 1000.0);
}

TEST_F(WebSocketDevicePerformanceTest, MemoryUsage) {
    device->start();
    
    // Send many messages and check that memory doesn't grow unbounded
    const int messageCount = 10000;
    
    for (int i = 0; i < messageCount; ++i) {
        device->sendMessage("memory test message " + std::to_string(i));
        
        if (i % 1000 == 0) {
            // Force some processing time
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    // In a real test, we would check actual memory usage here
    // For now, just ensure the device is still responsive
    auto stats = device->getConnectionStats();
    EXPECT_TRUE(stats.contains("messages_sent"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

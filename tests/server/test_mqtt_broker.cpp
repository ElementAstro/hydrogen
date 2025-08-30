#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <astrocomm/server/mqtt_broker.h>
#include <astrocomm/core/device_manager.h>
#include <thread>
#include <chrono>

using namespace astrocomm::server;
using namespace astrocomm::core;
using namespace testing;

class MqttBrokerTest : public ::testing::Test {
protected:
    void SetUp() override {
        MqttConfig config;
        config.brokerHost = "localhost";
        config.brokerPort = 1883;
        config.useTls = false;
        config.keepAliveInterval = 60;
        
        broker_ = std::make_unique<MqttBroker>(config);
    }

    void TearDown() override {
        if (broker_ && broker_->isRunning()) {
            broker_->stop();
        }
    }

    std::unique_ptr<MqttBroker> broker_;
};

TEST_F(MqttBrokerTest, BrokerInitialization) {
    EXPECT_FALSE(broker_->isRunning());
    EXPECT_TRUE(broker_->getConnectedClients().empty());
}

TEST_F(MqttBrokerTest, BrokerStartStop) {
    EXPECT_TRUE(broker_->start());
    EXPECT_TRUE(broker_->isRunning());
    
    broker_->stop();
    EXPECT_FALSE(broker_->isRunning());
}

TEST_F(MqttBrokerTest, ClientAuthentication) {
    EXPECT_TRUE(broker_->start());
    
    // Test successful authentication (no auth handler set, should allow all)
    EXPECT_TRUE(broker_->authenticateClient("test_client", "user", "pass"));
    EXPECT_TRUE(broker_->isClientConnected("test_client"));
    
    auto clients = broker_->getConnectedClients();
    EXPECT_EQ(clients.size(), 1);
    EXPECT_EQ(clients[0].clientId, "test_client");
}

TEST_F(MqttBrokerTest, ClientDisconnection) {
    EXPECT_TRUE(broker_->start());
    EXPECT_TRUE(broker_->authenticateClient("test_client", "user", "pass"));
    EXPECT_TRUE(broker_->isClientConnected("test_client"));
    
    broker_->disconnectClient("test_client");
    EXPECT_FALSE(broker_->isClientConnected("test_client"));
    EXPECT_TRUE(broker_->getConnectedClients().empty());
}

TEST_F(MqttBrokerTest, TopicSubscription) {
    EXPECT_TRUE(broker_->start());
    EXPECT_TRUE(broker_->authenticateClient("test_client", "user", "pass"));
    
    // Test subscription
    EXPECT_TRUE(broker_->subscribe("test_client", "test/topic", 1));
    
    auto subscriptions = broker_->getSubscriptions("test_client");
    EXPECT_EQ(subscriptions.size(), 1);
    EXPECT_EQ(subscriptions[0].topic, "test/topic");
    EXPECT_EQ(subscriptions[0].clientId, "test_client");
    EXPECT_EQ(subscriptions[0].qos, 1);
}

TEST_F(MqttBrokerTest, TopicUnsubscription) {
    EXPECT_TRUE(broker_->start());
    EXPECT_TRUE(broker_->authenticateClient("test_client", "user", "pass"));
    EXPECT_TRUE(broker_->subscribe("test_client", "test/topic", 1));
    
    // Test unsubscription
    EXPECT_TRUE(broker_->unsubscribe("test_client", "test/topic"));
    
    auto subscriptions = broker_->getSubscriptions("test_client");
    EXPECT_TRUE(subscriptions.empty());
}

TEST_F(MqttBrokerTest, MessagePublishing) {
    EXPECT_TRUE(broker_->start());
    
    // Test string message publishing
    EXPECT_TRUE(broker_->publish("test/topic", "Hello, World!", 1, false));
    
    // Test JSON message publishing
    json jsonMessage = {{"message", "Hello"}, {"timestamp", 12345}};
    EXPECT_TRUE(broker_->publish("test/topic", jsonMessage, 1, false));
}

TEST_F(MqttBrokerTest, RetainedMessages) {
    EXPECT_TRUE(broker_->start());
    
    // Publish retained message
    EXPECT_TRUE(broker_->publish("test/topic", "Retained message", 1, true));
    
    // Connect client and subscribe - should receive retained message
    EXPECT_TRUE(broker_->authenticateClient("test_client", "user", "pass"));
    EXPECT_TRUE(broker_->subscribe("test_client", "test/topic", 1));
    
    // Clear retained messages
    broker_->clearRetainedMessages();
}

TEST_F(MqttBrokerTest, ClientToClient) {
    EXPECT_TRUE(broker_->start());
    
    // Connect two clients
    EXPECT_TRUE(broker_->authenticateClient("client1", "user1", "pass1"));
    EXPECT_TRUE(broker_->authenticateClient("client2", "user2", "pass2"));
    
    // Client2 subscribes to topic
    EXPECT_TRUE(broker_->subscribe("client2", "test/topic", 1));
    
    // Client1 publishes to topic (would be delivered to client2 in real implementation)
    EXPECT_TRUE(broker_->publish("test/topic", "Message from client1", 1, false));
}

TEST_F(MqttBrokerTest, DeviceIntegration) {
    auto deviceManager = std::make_shared<DeviceManager>();
    broker_->setDeviceManager(deviceManager);
    
    EXPECT_TRUE(broker_->start());
    EXPECT_TRUE(broker_->authenticateClient("device_client", "device", "pass"));
    
    // Register device for client
    EXPECT_TRUE(broker_->registerDeviceClient("device_client", "device_001"));
    EXPECT_EQ(broker_->getDeviceIdForClient("device_client"), "device_001");
}

TEST_F(MqttBrokerTest, Statistics) {
    EXPECT_TRUE(broker_->start());
    
    json stats = broker_->getStatistics();
    EXPECT_TRUE(stats.contains("uptime_seconds"));
    EXPECT_TRUE(stats.contains("messages_received"));
    EXPECT_TRUE(stats.contains("messages_published"));
    EXPECT_TRUE(stats.contains("client_connections"));
    EXPECT_TRUE(stats.contains("connected_clients"));
}

TEST_F(MqttBrokerTest, Status) {
    json status = broker_->getStatus();
    EXPECT_TRUE(status.contains("running"));
    EXPECT_TRUE(status.contains("config"));
    EXPECT_TRUE(status.contains("statistics"));
    
    EXPECT_FALSE(status["running"].get<bool>());
    
    broker_->start();
    status = broker_->getStatus();
    EXPECT_TRUE(status["running"].get<bool>());
}

class MockClientConnectHandler {
public:
    MOCK_METHOD(void, onClientConnect, (const std::string& clientId, bool connected));
};

class MockMessageHandler {
public:
    MOCK_METHOD(void, onMessage, (const MqttMessage& message));
};

class MockAuthHandler {
public:
    MOCK_METHOD(bool, authenticate, (const std::string& clientId, const std::string& username, const std::string& password));
};

TEST_F(MqttBrokerTest, EventHandlers) {
    MockClientConnectHandler connectHandler;
    MockMessageHandler messageHandler;
    MockAuthHandler authHandler;
    
    broker_->setClientConnectHandler([&connectHandler](const std::string& clientId, bool connected) {
        connectHandler.onClientConnect(clientId, connected);
    });
    
    broker_->setMessageHandler([&messageHandler](const MqttMessage& message) {
        messageHandler.onMessage(message);
    });
    
    broker_->setAuthenticationHandler([&authHandler](const std::string& clientId, const std::string& username, const std::string& password) {
        return authHandler.authenticate(clientId, username, password);
    });
    
    EXPECT_TRUE(broker_->start());
    
    // Test authentication handler
    EXPECT_CALL(authHandler, authenticate("test_client", "user", "pass"))
        .WillOnce(Return(true));
    
    EXPECT_TRUE(broker_->authenticateClient("test_client", "user", "pass"));
}

// MqttTopicUtils tests
class MqttTopicUtilsTest : public ::testing::Test {};

TEST_F(MqttTopicUtilsTest, TopicValidation) {
    EXPECT_TRUE(MqttTopicUtils::isValidTopic("test/topic"));
    EXPECT_TRUE(MqttTopicUtils::isValidTopic("device/123/status"));
    EXPECT_FALSE(MqttTopicUtils::isValidTopic("test/topic/+"));  // Wildcards not allowed in publish topics
    EXPECT_FALSE(MqttTopicUtils::isValidTopic("test/topic/#"));  // Wildcards not allowed in publish topics
    EXPECT_FALSE(MqttTopicUtils::isValidTopic(""));              // Empty topic
}

TEST_F(MqttTopicUtilsTest, TopicFilterValidation) {
    EXPECT_TRUE(MqttTopicUtils::isValidTopicFilter("test/topic"));
    EXPECT_TRUE(MqttTopicUtils::isValidTopicFilter("test/+/status"));
    EXPECT_TRUE(MqttTopicUtils::isValidTopicFilter("test/#"));
    EXPECT_TRUE(MqttTopicUtils::isValidTopicFilter("#"));
    EXPECT_FALSE(MqttTopicUtils::isValidTopicFilter("test/+topic"));  // Invalid + placement
    EXPECT_FALSE(MqttTopicUtils::isValidTopicFilter("test/#/more"));  // # must be at end
}

TEST_F(MqttTopicUtilsTest, TopicMatching) {
    EXPECT_TRUE(MqttTopicUtils::matchesFilter("test/topic", "test/topic"));
    EXPECT_TRUE(MqttTopicUtils::matchesFilter("test/+", "test/topic"));
    EXPECT_TRUE(MqttTopicUtils::matchesFilter("test/+/status", "test/device/status"));
    EXPECT_TRUE(MqttTopicUtils::matchesFilter("test/#", "test/device/status"));
    EXPECT_TRUE(MqttTopicUtils::matchesFilter("#", "any/topic/here"));
    
    EXPECT_FALSE(MqttTopicUtils::matchesFilter("test/topic", "test/other"));
    EXPECT_FALSE(MqttTopicUtils::matchesFilter("test/+", "test/topic/extra"));
    EXPECT_FALSE(MqttTopicUtils::matchesFilter("test/+/status", "test/status"));
}

TEST_F(MqttTopicUtilsTest, DeviceTopics) {
    EXPECT_EQ(MqttTopicUtils::getDeviceCommandTopic("device_001"), "astrocomm/device/device_001/command");
    EXPECT_EQ(MqttTopicUtils::getDeviceStatusTopic("device_001"), "astrocomm/device/device_001/status");
    EXPECT_EQ(MqttTopicUtils::getDeviceDataTopic("device_001", "temperature"), "astrocomm/device/device_001/data/temperature");
    EXPECT_EQ(MqttTopicUtils::getDeviceEventTopic("device_001", "alert"), "astrocomm/device/device_001/event/alert");
}

TEST_F(MqttTopicUtilsTest, SystemTopics) {
    EXPECT_EQ(MqttTopicUtils::getSystemTopic("health"), "astrocomm/system/health");
    EXPECT_EQ(MqttTopicUtils::getBroadcastTopic(), "astrocomm/broadcast");
    EXPECT_EQ(MqttTopicUtils::getDiscoveryTopic(), "astrocomm/discovery");
}

TEST_F(MqttTopicUtilsTest, TopicSanitization) {
    EXPECT_EQ(MqttTopicUtils::sanitizeTopic("test/topic"), "test/topic");
    EXPECT_EQ(MqttTopicUtils::sanitizeTopic("/test/topic/"), "test/topic");
    EXPECT_EQ(MqttTopicUtils::sanitizeTopic("test\x00topic"), "testtopic");  // Remove null characters
}

// Factory tests
class MqttBrokerFactoryTest : public ::testing::Test {};

TEST_F(MqttBrokerFactoryTest, CreateBroker) {
    MqttConfig config;
    config.brokerHost = "localhost";
    config.brokerPort = 1883;
    
    auto broker = MqttBrokerFactory::createBroker(config);
    EXPECT_NE(broker, nullptr);
    EXPECT_FALSE(broker->isRunning());
}

TEST_F(MqttBrokerFactoryTest, CreateDeviceBroker) {
    MqttConfig config;
    config.brokerHost = "localhost";
    config.brokerPort = 1883;
    
    auto deviceManager = std::make_shared<DeviceManager>();
    auto broker = MqttBrokerFactory::createDeviceBroker(config, deviceManager);
    
    EXPECT_NE(broker, nullptr);
    EXPECT_FALSE(broker->isRunning());
}

// Performance tests
class MqttBrokerPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        MqttConfig config;
        config.brokerHost = "localhost";
        config.brokerPort = 1883;
        
        broker_ = std::make_unique<MqttBroker>(config);
        EXPECT_TRUE(broker_->start());
    }

    void TearDown() override {
        if (broker_ && broker_->isRunning()) {
            broker_->stop();
        }
    }

    std::unique_ptr<MqttBroker> broker_;
};

TEST_F(MqttBrokerPerformanceTest, DISABLED_MultipleClientConnections) {
    // Test multiple client connections
    const int numClients = 100;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numClients; ++i) {
        std::string clientId = "client_" + std::to_string(i);
        EXPECT_TRUE(broker_->authenticateClient(clientId, "user", "pass"));
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
    EXPECT_EQ(broker_->getConnectedClients().size(), numClients);
}

TEST_F(MqttBrokerPerformanceTest, DISABLED_MessageThroughput) {
    // Test message publishing throughput
    const int numMessages = 1000;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numMessages; ++i) {
        std::string message = "Message " + std::to_string(i);
        EXPECT_TRUE(broker_->publish("test/topic", message, 1, false));
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
    
    double messagesPerSecond = (numMessages * 1000.0) / duration.count();
    EXPECT_GT(messagesPerSecond, 500); // Should handle at least 500 messages per second
}

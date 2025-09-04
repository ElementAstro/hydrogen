#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "hydrogen/server/protocols/mqtt/mqtt_broker.h"
#include <thread>
#include <chrono>
#include <memory>

using namespace hydrogen::server::protocols::mqtt;
using namespace testing;

class MqttBrokerTest : public ::testing::Test {
protected:
    void SetUp() override {
        MqttBrokerConfig config;
        config.host = "localhost";
        config.port = 1883;
        config.enableTLS = false;
        config.keepAliveTimeout = 60;
        config.requireAuthentication = false;

        broker_ = MqttBrokerFactory::createBroker(config);
        ASSERT_NE(broker_, nullptr);
        ASSERT_TRUE(broker_->initialize());
    }

    void TearDown() override {
        if (broker_ && broker_->isRunning()) {
            broker_->stop();
        }
    }

    std::unique_ptr<IMqttBroker> broker_;
};

TEST_F(MqttBrokerTest, BrokerInitialization) {
    EXPECT_TRUE(broker_->isInitialized());
    EXPECT_FALSE(broker_->isRunning());
    EXPECT_TRUE(broker_->getConnectedClients().empty());
}

TEST_F(MqttBrokerTest, BrokerStartStop) {
    EXPECT_TRUE(broker_->start());
    EXPECT_TRUE(broker_->isRunning());

    broker_->stop();
    EXPECT_FALSE(broker_->isRunning());
}

TEST_F(MqttBrokerTest, ClientManagement) {
    EXPECT_TRUE(broker_->start());

    // Create client info
    MqttClientInfo clientInfo;
    clientInfo.clientId = "test_client";
    clientInfo.username = "user";
    clientInfo.remoteAddress = "127.0.0.1";
    clientInfo.remotePort = 12345;
    clientInfo.isConnected = true;

    // Accept client
    EXPECT_TRUE(broker_->acceptClient("test_client", clientInfo));

    auto clients = broker_->getConnectedClients();
    EXPECT_EQ(clients.size(), 1);
    EXPECT_EQ(clients[0], "test_client");

    // Get client info
    auto retrievedInfo = broker_->getClientInfo("test_client");
    EXPECT_TRUE(retrievedInfo.has_value());
    EXPECT_EQ(retrievedInfo->clientId, "test_client");

    // Disconnect client
    broker_->disconnectClient("test_client");
    EXPECT_TRUE(broker_->getConnectedClients().empty());
}

TEST_F(MqttBrokerTest, TopicSubscription) {
    EXPECT_TRUE(broker_->start());

    // Accept a client first
    MqttClientInfo clientInfo;
    clientInfo.clientId = "test_client";
    clientInfo.isConnected = true;
    EXPECT_TRUE(broker_->acceptClient("test_client", clientInfo));

    // Test subscription
    EXPECT_TRUE(broker_->subscribe("test_client", "test/topic", MqttQoS::AT_LEAST_ONCE));

    auto subscriptions = broker_->getSubscriptions("test_client");
    EXPECT_EQ(subscriptions.size(), 1);
    EXPECT_EQ(subscriptions[0].topic, "test/topic");
    EXPECT_EQ(subscriptions[0].clientId, "test_client");
    EXPECT_EQ(subscriptions[0].qos, MqttQoS::AT_LEAST_ONCE);

    // Test unsubscription
    EXPECT_TRUE(broker_->unsubscribe("test_client", "test/topic"));

    subscriptions = broker_->getSubscriptions("test_client");
    EXPECT_TRUE(subscriptions.empty());
}

TEST_F(MqttBrokerTest, MessagePublishing) {
    EXPECT_TRUE(broker_->start());

    // Create a test message
    MqttMessage message;
    message.id = "test_msg_1";
    message.topic = "test/topic";
    message.payload = "Hello, World!";
    message.qos = MqttQoS::AT_LEAST_ONCE;
    message.retain = false;
    message.timestamp = std::chrono::system_clock::now();

    // Test message publishing
    EXPECT_TRUE(broker_->publishMessage(message));
}

TEST_F(MqttBrokerTest, RetainedMessages) {
    EXPECT_TRUE(broker_->start());

    // Create a retained message
    MqttMessage message;
    message.id = "retained_msg_1";
    message.topic = "test/retained";
    message.payload = "Retained message";
    message.qos = MqttQoS::AT_LEAST_ONCE;
    message.retain = true;
    message.timestamp = std::chrono::system_clock::now();

    // Publish retained message
    EXPECT_TRUE(broker_->publishMessage(message));

    // Get retained messages
    auto retainedMessages = broker_->getRetainedMessages();
    EXPECT_FALSE(retainedMessages.empty());

    // Clear retained message
    EXPECT_TRUE(broker_->clearRetainedMessage("test/retained"));
}

TEST_F(MqttBrokerTest, Statistics) {
    EXPECT_TRUE(broker_->start());

    auto stats = broker_->getStatistics();
    EXPECT_EQ(stats.connectedClients, 0);
    EXPECT_EQ(stats.totalMessages, 0);
    EXPECT_EQ(stats.totalSubscriptions, 0);

    // Reset statistics
    broker_->resetStatistics();
    stats = broker_->getStatistics();
    EXPECT_EQ(stats.totalMessages, 0);
}

TEST_F(MqttBrokerTest, Authentication) {
    EXPECT_TRUE(broker_->start());

    // Enable authentication
    EXPECT_TRUE(broker_->enableAuthentication(true));

    // Set credentials
    EXPECT_TRUE(broker_->setCredentials("testuser", "testpass"));

    // Validate credentials
    EXPECT_TRUE(broker_->validateCredentials("testuser", "testpass"));
    EXPECT_FALSE(broker_->validateCredentials("testuser", "wrongpass"));
    EXPECT_FALSE(broker_->validateCredentials("wronguser", "testpass"));

    // Remove credentials
    EXPECT_TRUE(broker_->removeCredentials("testuser"));
    EXPECT_FALSE(broker_->validateCredentials("testuser", "testpass"));
}

TEST_F(MqttBrokerTest, HealthCheck) {
    EXPECT_TRUE(broker_->start());

    EXPECT_TRUE(broker_->isHealthy());

    std::string healthStatus = broker_->getHealthStatus();
    EXPECT_FALSE(healthStatus.empty());
}

// Factory tests
class MqttBrokerFactoryTest : public ::testing::Test {};

TEST_F(MqttBrokerFactoryTest, CreateBroker) {
    MqttBrokerConfig config;
    config.host = "localhost";
    config.port = 1883;

    auto broker = MqttBrokerFactory::createBroker(config);
    EXPECT_NE(broker, nullptr);
    EXPECT_TRUE(broker->initialize());
    EXPECT_FALSE(broker->isRunning());
}

TEST_F(MqttBrokerFactoryTest, CreateBrokerWithHostPort) {
    auto broker = MqttBrokerFactory::createBroker("localhost", 1883);
    EXPECT_NE(broker, nullptr);
    EXPECT_TRUE(broker->initialize());
    EXPECT_FALSE(broker->isRunning());
}

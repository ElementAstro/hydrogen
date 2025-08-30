#include <gtest/gtest.h>
#include "astrocomm/server/protocols/mqtt/mqtt_broker.h"
#include <memory>

using namespace astrocomm::server::protocols::mqtt;

class MqttBrokerTest : public ::testing::Test {
protected:
    void SetUp() override {
        MqttBrokerConfig config;
        config.host = "localhost";
        config.port = 1884; // Use different port for tests
        config.maxClients = 10;
        config.requireAuthentication = false;
        
        broker_ = MqttBrokerFactory::createBroker(config);
        ASSERT_NE(broker_, nullptr);
        ASSERT_TRUE(broker_->initialize());
    }
    
    void TearDown() override {
        if (broker_) {
            broker_->stop();
        }
    }
    
    std::unique_ptr<IMqttBroker> broker_;
};

TEST_F(MqttBrokerTest, BasicOperations) {
    EXPECT_TRUE(broker_->isInitialized());
    EXPECT_FALSE(broker_->isRunning());
    
    // Start broker
    EXPECT_TRUE(broker_->start());
    EXPECT_TRUE(broker_->isRunning());
    
    // Stop broker
    EXPECT_TRUE(broker_->stop());
    EXPECT_FALSE(broker_->isRunning());
}

TEST_F(MqttBrokerTest, ClientManagement) {
    MqttClientInfo clientInfo;
    clientInfo.clientId = "test_client_1";
    clientInfo.username = "testuser";
    clientInfo.remoteAddress = "127.0.0.1";
    clientInfo.remotePort = 12345;
    clientInfo.isConnected = true;
    
    // Accept client
    EXPECT_TRUE(broker_->acceptClient("test_client_1", clientInfo));
    EXPECT_EQ(broker_->getClientCount(), 1);
    
    // Get client info
    auto retrievedInfo = broker_->getClientInfo("test_client_1");
    ASSERT_TRUE(retrievedInfo.has_value());
    EXPECT_EQ(retrievedInfo->clientId, "test_client_1");
    
    // Get connected clients
    auto clients = broker_->getConnectedClients();
    EXPECT_EQ(clients.size(), 1);
    EXPECT_EQ(clients[0], "test_client_1");
    
    // Disconnect client
    EXPECT_TRUE(broker_->disconnectClient("test_client_1"));
    EXPECT_EQ(broker_->getClientCount(), 0);
}

TEST_F(MqttBrokerTest, SubscriptionManagement) {
    // First accept a client
    MqttClientInfo clientInfo;
    clientInfo.clientId = "test_client";
    broker_->acceptClient("test_client", clientInfo);
    
    // Subscribe to topic
    EXPECT_TRUE(broker_->subscribe("test_client", "test/topic", MqttQoS::AT_LEAST_ONCE));
    
    // Get subscriptions
    auto subscriptions = broker_->getSubscriptions("test_client");
    EXPECT_EQ(subscriptions.size(), 1);
    EXPECT_EQ(subscriptions[0].topic, "test/topic");
    EXPECT_EQ(subscriptions[0].qos, MqttQoS::AT_LEAST_ONCE);
    
    // Get topic subscribers
    auto subscribers = broker_->getTopicSubscribers("test/topic");
    EXPECT_EQ(subscribers.size(), 1);
    EXPECT_EQ(subscribers[0], "test_client");
    
    // Unsubscribe
    EXPECT_TRUE(broker_->unsubscribe("test_client", "test/topic"));
    auto emptySubscriptions = broker_->getSubscriptions("test_client");
    EXPECT_EQ(emptySubscriptions.size(), 0);
}

TEST_F(MqttBrokerTest, MessageHandling) {
    // Accept client and subscribe
    MqttClientInfo clientInfo;
    clientInfo.clientId = "test_client";
    broker_->acceptClient("test_client", clientInfo);
    broker_->subscribe("test_client", "test/topic", MqttQoS::AT_MOST_ONCE);
    
    // Publish message
    MqttMessage message;
    message.id = "msg_001";
    message.topic = "test/topic";
    message.payload = "Hello MQTT";
    message.qos = MqttQoS::AT_MOST_ONCE;
    message.retain = true;
    
    EXPECT_TRUE(broker_->publishMessage(message));
    
    // Get retained messages
    auto retainedMessages = broker_->getRetainedMessages("test/topic");
    EXPECT_EQ(retainedMessages.size(), 1);
    EXPECT_EQ(retainedMessages[0].payload, "Hello MQTT");
    
    // Clear retained message
    EXPECT_TRUE(broker_->clearRetainedMessage("test/topic"));
    auto emptyRetained = broker_->getRetainedMessages("test/topic");
    EXPECT_EQ(emptyRetained.size(), 0);
}

TEST_F(MqttBrokerTest, Authentication) {
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
    
    // Disable authentication
    EXPECT_TRUE(broker_->enableAuthentication(false));
    EXPECT_TRUE(broker_->validateCredentials("anyuser", "anypass")); // Should pass when disabled
}

TEST_F(MqttBrokerTest, Statistics) {
    auto stats = broker_->getStatistics();
    EXPECT_EQ(stats.connectedClients, 0);
    EXPECT_EQ(stats.totalMessages, 0);
    
    broker_->resetStatistics();
    auto resetStats = broker_->getStatistics();
    EXPECT_EQ(resetStats.connectedClients, 0);
}

TEST_F(MqttBrokerTest, HealthChecking) {
    EXPECT_TRUE(broker_->isHealthy());
    EXPECT_EQ(broker_->getHealthStatus(), "Healthy");
}

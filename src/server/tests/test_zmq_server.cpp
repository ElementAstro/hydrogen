#include <gtest/gtest.h>
#include "hydrogen/server/protocols/zmq/zmq_server.h"
#include <memory>

using namespace hydrogen::server::protocols::zmq;

class ZmqServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ZmqServerConfig config;
        config.bindAddress = "tcp://*:5556"; // Use different port for tests
        config.socketType = ZmqSocketType::REP;
        config.ioThreads = 1;
        config.sendTimeout = 1000;
        config.receiveTimeout = 1000;
        
        server_ = ZmqServerFactory::createServer(config);
        ASSERT_NE(server_, nullptr);
        ASSERT_TRUE(server_->initialize());
    }
    
    void TearDown() override {
        if (server_) {
            server_->stop();
        }
    }
    
    std::unique_ptr<IZmqServer> server_;
};

TEST_F(ZmqServerTest, BasicOperations) {
    EXPECT_TRUE(server_->isInitialized());
    EXPECT_FALSE(server_->isRunning());
    
    // Start server
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(server_->isRunning());
    
    // Stop server
    EXPECT_TRUE(server_->stop());
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(ZmqServerTest, Configuration) {
    auto config = server_->getConfig();
    EXPECT_EQ(config.bindAddress, "tcp://*:5556");
    EXPECT_EQ(config.socketType, ZmqSocketType::REP);
    
    // Update configuration
    config.sendTimeout = 2000;
    EXPECT_TRUE(server_->updateConfig(config));
    
    auto updatedConfig = server_->getConfig();
    EXPECT_EQ(updatedConfig.sendTimeout, 2000);
}

TEST_F(ZmqServerTest, MessageHandling) {
    // Send message
    EXPECT_TRUE(server_->sendMessage("Hello ZMQ", "client1"));
    
    // Get received messages (initially empty)
    auto messages = server_->getReceivedMessages();
    EXPECT_EQ(messages.size(), 0);
    
    // Clear received messages
    server_->clearReceivedMessages();
    auto clearedMessages = server_->getReceivedMessages();
    EXPECT_EQ(clearedMessages.size(), 0);
}

TEST_F(ZmqServerTest, BroadcastOperations) {
    // Test broadcast (only works with PUB socket type)
    ZmqServerConfig pubConfig;
    pubConfig.bindAddress = "tcp://*:5557";
    pubConfig.socketType = ZmqSocketType::PUB;
    
    auto pubServer = ZmqServerFactory::createServer(pubConfig);
    ASSERT_TRUE(pubServer->initialize());
    ASSERT_TRUE(pubServer->start());
    
    EXPECT_TRUE(pubServer->broadcastMessage("Broadcast message"));
    
    pubServer->stop();
}

TEST_F(ZmqServerTest, ClientManagement) {
    // Get connected clients (initially empty)
    auto clients = server_->getConnectedClients();
    EXPECT_EQ(clients.size(), 0);
    EXPECT_EQ(server_->getClientCount(), 0);
    
    // Disconnect non-existent client
    EXPECT_FALSE(server_->disconnectClient("non_existent"));
}

TEST_F(ZmqServerTest, SocketOptions) {
    // Set socket options
    EXPECT_TRUE(server_->setSocketOption(ZmqSocketOption::SEND_TIMEOUT, 5000));
    EXPECT_EQ(server_->getSocketOption(ZmqSocketOption::SEND_TIMEOUT), 5000);
    
    EXPECT_TRUE(server_->setSocketOption(ZmqSocketOption::RECEIVE_TIMEOUT, 3000));
    EXPECT_EQ(server_->getSocketOption(ZmqSocketOption::RECEIVE_TIMEOUT), 3000);
    
    // Get non-existent option
    EXPECT_EQ(server_->getSocketOption(ZmqSocketOption::LINGER), -1);
}

TEST_F(ZmqServerTest, MessageHandlers) {
    bool messageHandlerCalled = false;
    bool connectionHandlerCalled = false;
    
    // Set message handler
    server_->setMessageHandler([&](const ZmqMessage& message) {
        messageHandlerCalled = true;
    });
    
    // Set connection handler
    server_->setConnectionHandler([&](const std::string& clientId, bool connected) {
        connectionHandlerCalled = true;
    });
    
    // Remove handlers
    server_->removeMessageHandler();
    server_->removeConnectionHandler();
}

TEST_F(ZmqServerTest, Statistics) {
    auto stats = server_->getStatistics();
    EXPECT_EQ(stats.connectedClients, 0);
    EXPECT_EQ(stats.totalMessagesSent, 0);
    EXPECT_EQ(stats.totalMessagesReceived, 0);
    
    server_->resetStatistics();
    auto resetStats = server_->getStatistics();
    EXPECT_EQ(resetStats.totalMessagesSent, 0);
}

TEST_F(ZmqServerTest, HealthChecking) {
    EXPECT_TRUE(server_->isHealthy());
    EXPECT_EQ(server_->getHealthStatus(), "Healthy");
}

TEST_F(ZmqServerTest, DifferentSocketTypes) {
    // Test ROUTER socket type
    ZmqServerConfig routerConfig;
    routerConfig.bindAddress = "tcp://*:5558";
    routerConfig.socketType = ZmqSocketType::ROUTER;
    
    auto routerServer = ZmqServerFactory::createServer(routerConfig);
    ASSERT_TRUE(routerServer->initialize());
    ASSERT_TRUE(routerServer->start());
    EXPECT_TRUE(routerServer->isRunning());
    routerServer->stop();
    
    // Test PUSH socket type
    ZmqServerConfig pushConfig;
    pushConfig.bindAddress = "tcp://*:5559";
    pushConfig.socketType = ZmqSocketType::PUSH;
    
    auto pushServer = ZmqServerFactory::createServer(pushConfig);
    ASSERT_TRUE(pushServer->initialize());
    ASSERT_TRUE(pushServer->start());
    EXPECT_TRUE(pushServer->isRunning());
    pushServer->stop();
}

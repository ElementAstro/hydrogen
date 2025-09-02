#include <gtest/gtest.h>
#include "hydrogen/server/services/communication_service.h"
#include <memory>
#include <chrono>
#include <thread>

using namespace hydrogen::server::services;

class CommunicationServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create communication service instance
        service_ = CommunicationServiceFactory::createService("TestCommunicationService");
        ASSERT_NE(service_, nullptr);
        
        // Initialize the service
        ASSERT_TRUE(service_->initialize());
        ASSERT_TRUE(service_->start());
    }
    
    void TearDown() override {
        if (service_) {
            service_->stop();
        }
    }
    
    std::unique_ptr<ICommunicationService> service_;
};

TEST_F(CommunicationServiceTest, ServiceInitialization) {
    EXPECT_TRUE(service_->isInitialized());
    EXPECT_TRUE(service_->isRunning());
    EXPECT_EQ(service_->getName(), "TestCommunicationService");
}

TEST_F(CommunicationServiceTest, SendMessage) {
    MessageRequest request;
    request.senderId = "sender123";
    request.recipientId = "recipient456";
    request.content = "Test message content";
    request.messageType = "TEXT";
    request.priority = MessagePriority::NORMAL;
    
    std::string messageId = service_->sendMessage(request);
    EXPECT_FALSE(messageId.empty());
    EXPECT_TRUE(messageId.find("msg_") == 0); // Should start with "msg_"
}

TEST_F(CommunicationServiceTest, BroadcastMessage) {
    BroadcastRequest request;
    request.senderId = "broadcaster123";
    request.recipientIds = {"recipient1", "recipient2", "recipient3"};
    request.content = "Broadcast message";
    request.messageType = "BROADCAST";
    request.priority = MessagePriority::HIGH;
    
    bool result = service_->broadcastMessage(request);
    EXPECT_TRUE(result);
}

TEST_F(CommunicationServiceTest, TopicSubscription) {
    std::string clientId = "client123";
    std::string topic = "test/topic";
    core::CommunicationProtocol protocol = core::CommunicationProtocol::HTTP;
    
    // Subscribe to topic
    bool subscribed = service_->subscribeToTopic(clientId, topic, protocol);
    EXPECT_TRUE(subscribed);
    
    // Unsubscribe from topic
    bool unsubscribed = service_->unsubscribeFromTopic(clientId, topic);
    EXPECT_TRUE(unsubscribed);
}

TEST_F(CommunicationServiceTest, GetMessages) {
    // First send a message
    MessageRequest request;
    request.senderId = "sender123";
    request.recipientId = "recipient456";
    request.content = "Test message for retrieval";
    request.messageType = "TEXT";
    request.priority = MessagePriority::NORMAL;
    
    std::string messageId = service_->sendMessage(request);
    EXPECT_FALSE(messageId.empty());
    
    // Wait a bit for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Retrieve messages
    auto messages = service_->getMessages("recipient456", MessageStatus::PENDING);
    EXPECT_GE(messages.size(), 1);
    
    // Check message content
    bool found = false;
    for (const auto& msg : messages) {
        if (msg.id == messageId) {
            EXPECT_EQ(msg.senderId, "sender123");
            EXPECT_EQ(msg.recipientId, "recipient456");
            EXPECT_EQ(msg.content, "Test message for retrieval");
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CommunicationServiceTest, MarkMessageAsRead) {
    // Send a message first
    MessageRequest request;
    request.senderId = "sender123";
    request.recipientId = "recipient456";
    request.content = "Message to mark as read";
    request.messageType = "TEXT";
    request.priority = MessagePriority::NORMAL;
    
    std::string messageId = service_->sendMessage(request);
    EXPECT_FALSE(messageId.empty());
    
    // Mark as read
    bool marked = service_->markMessageAsRead(messageId, "recipient456");
    EXPECT_TRUE(marked);
    
    // Verify delivery status
    auto status = service_->getMessageDeliveryStatus(messageId);
    EXPECT_EQ(status, DeliveryStatus::DELIVERED);
}

TEST_F(CommunicationServiceTest, MessageStatistics) {
    // Get initial statistics
    auto initialStats = service_->getMessageStatistics();
    
    // Send some messages
    for (int i = 0; i < 5; ++i) {
        MessageRequest request;
        request.senderId = "sender" + std::to_string(i);
        request.recipientId = "recipient" + std::to_string(i);
        request.content = "Test message " + std::to_string(i);
        request.messageType = "TEXT";
        request.priority = MessagePriority::NORMAL;
        
        service_->sendMessage(request);
    }
    
    // Get updated statistics
    auto updatedStats = service_->getMessageStatistics();
    EXPECT_GE(updatedStats.totalSent, initialStats.totalSent + 5);
}

TEST_F(CommunicationServiceTest, ServiceRestart) {
    // Stop the service
    EXPECT_TRUE(service_->stop());
    EXPECT_FALSE(service_->isRunning());
    
    // Restart the service
    EXPECT_TRUE(service_->restart());
    EXPECT_TRUE(service_->isRunning());
}

TEST_F(CommunicationServiceTest, InvalidOperations) {
    // Try to mark non-existent message as read
    bool result = service_->markMessageAsRead("invalid_id", "recipient");
    EXPECT_FALSE(result);
    
    // Try to get delivery status for non-existent message
    auto status = service_->getMessageDeliveryStatus("invalid_id");
    EXPECT_EQ(status, DeliveryStatus::UNKNOWN);
    
    // Try to unsubscribe from non-existent subscription
    bool unsubscribed = service_->unsubscribeFromTopic("invalid_client", "invalid_topic");
    EXPECT_FALSE(unsubscribed);
}

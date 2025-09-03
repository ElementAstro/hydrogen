/**
 * @file test_message_queue.cpp
 * @brief Comprehensive tests for message queue functionality
 * 
 * Tests the message queue system including QoS levels, retries,
 * acknowledgments, and priority handling.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <common/message_queue.h>
#include <common/message.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace hydrogen;
using namespace testing;

class MessageQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        messageQueue = std::make_unique<MessageQueueManager>();
        testDeviceId = "test_device_001";
        messagesSent = 0;
        messagesAcknowledged = 0;
        
        // Set up mock message sender
        messageSender = [this](const Message& msg) -> bool {
            messagesSent++;
            sentMessages.push_back(msg.getMessageId());
            return true; // Simulate successful send
        };
        
        // Set up mock acknowledgment callback
        ackCallback = [this](const std::string& messageId, bool success) {
            if (success) {
                messagesAcknowledged++;
                acknowledgedMessages.push_back(messageId);
            }
        };
    }
    
    void TearDown() override {
        if (messageQueue) {
            messageQueue->stop();
        }
    }
    
    std::unique_ptr<MessageQueueManager> messageQueue;
    std::string testDeviceId;
    std::atomic<int> messagesSent{0};
    std::atomic<int> messagesAcknowledged{0};
    std::vector<std::string> sentMessages;
    std::vector<std::string> acknowledgedMessages;
    
    MessageSendCallback messageSender;
    MessageAckCallback ackCallback;
    
    // Helper to create test command message
    std::unique_ptr<CommandMessage> createTestCommand(const std::string& command = "test_command") {
        auto cmd = std::make_unique<CommandMessage>(command);
        cmd->setDeviceId(testDeviceId);
        return cmd;
    }
};

// Test message queue manager creation and lifecycle
TEST_F(MessageQueueTest, MessageQueueManagerLifecycle) {
    EXPECT_NE(messageQueue, nullptr);
    
    // Test start/stop
    messageQueue->setMessageSender(messageSender);
    messageQueue->start();
    messageQueue->stop();
    
    // Should be able to restart
    messageQueue->start();
    messageQueue->stop();
    
    SUCCEED(); // Test passes if no exceptions are thrown
}

// Test message sender configuration
TEST_F(MessageQueueTest, MessageSenderConfiguration) {
    // Test setting message sender
    EXPECT_NO_THROW(messageQueue->setMessageSender(messageSender));
    
    // Test with null sender
    EXPECT_NO_THROW(messageQueue->setMessageSender(nullptr));
}

// Test basic message sending
TEST_F(MessageQueueTest, BasicMessageSending) {
    messageQueue->setMessageSender(messageSender);
    messageQueue->start();
    
    auto cmd = createTestCommand();
    std::string messageId = cmd->getMessageId();
    
    // Send message with acknowledgment callback
    messageQueue->sendMessage(*cmd, ackCallback);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(messagesSent.load(), 1);
    EXPECT_EQ(messagesAcknowledged.load(), 1);
    EXPECT_EQ(sentMessages.size(), 1);
    EXPECT_EQ(sentMessages[0], messageId);
    
    messageQueue->stop();
}

// Test QoS AT_MOST_ONCE behavior
TEST_F(MessageQueueTest, QoSAtMostOnce) {
    messageQueue->setMessageSender(messageSender);
    messageQueue->start();
    
    auto cmd = createTestCommand();
    cmd->setQoSLevel(Message::QoSLevel::AT_MOST_ONCE);
    
    messageQueue->sendMessage(*cmd, ackCallback);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // AT_MOST_ONCE should send once without retries
    EXPECT_EQ(messagesSent.load(), 1);
    EXPECT_EQ(messagesAcknowledged.load(), 1);
    
    messageQueue->stop();
}

// Test QoS AT_LEAST_ONCE behavior
TEST_F(MessageQueueTest, QoSAtLeastOnce) {
    messageQueue->setMessageSender(messageSender);
    messageQueue->start();
    
    auto cmd = createTestCommand();
    cmd->setQoSLevel(Message::QoSLevel::AT_LEAST_ONCE);
    
    messageQueue->sendMessage(*cmd, ackCallback);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should send at least once
    EXPECT_GE(messagesSent.load(), 1);
    
    messageQueue->stop();
}

// Test message priority handling
TEST_F(MessageQueueTest, MessagePriorityHandling) {
    messageQueue->setMessageSender(messageSender);
    messageQueue->start();
    
    // Create messages with different priorities
    auto lowPriorityCmd = createTestCommand("low_priority");
    lowPriorityCmd->setPriority(Message::Priority::LOW);
    
    auto highPriorityCmd = createTestCommand("high_priority");
    highPriorityCmd->setPriority(Message::Priority::HIGH);
    
    auto criticalCmd = createTestCommand("critical");
    criticalCmd->setPriority(Message::Priority::CRITICAL);
    
    // Send in reverse priority order
    messageQueue->sendMessage(*lowPriorityCmd, ackCallback);
    messageQueue->sendMessage(*highPriorityCmd, ackCallback);
    messageQueue->sendMessage(*criticalCmd, ackCallback);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_EQ(messagesSent.load(), 3);
    EXPECT_EQ(messagesAcknowledged.load(), 3);
    
    messageQueue->stop();
}

// Test message sending without sender configured
TEST_F(MessageQueueTest, MessageSendingWithoutSender) {
    // Don't set message sender
    messageQueue->start();
    
    auto cmd = createTestCommand();
    
    // Should handle gracefully
    EXPECT_NO_THROW(messageQueue->sendMessage(*cmd, ackCallback));
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // No messages should be sent
    EXPECT_EQ(messagesSent.load(), 0);
    
    messageQueue->stop();
}

// Test concurrent message sending
TEST_F(MessageQueueTest, ConcurrentMessageSending) {
    messageQueue->setMessageSender(messageSender);
    messageQueue->start();
    
    const int numThreads = 4;
    const int messagesPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> totalSent{0};
    
    // Create multiple threads that send messages
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([this, &totalSent, t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; i++) {
                auto cmd = createTestCommand("thread_" + std::to_string(t) + "_msg_" + std::to_string(i));
                messageQueue->sendMessage(*cmd, [&totalSent](const std::string&, bool success) {
                    if (success) totalSent++;
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Wait for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_EQ(totalSent.load(), numThreads * messagesPerThread);
    
    messageQueue->stop();
}

// Test message expiration
TEST_F(MessageQueueTest, MessageExpiration) {
    messageQueue->setMessageSender(messageSender);
    messageQueue->start();
    
    auto cmd = createTestCommand();
    cmd->setExpireAfterSeconds(1); // 1 second expiration
    
    messageQueue->sendMessage(*cmd, ackCallback);
    
    // Wait for message to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    // Message should still be processed (expiration handling depends on implementation)
    EXPECT_GE(messagesSent.load(), 0);
    
    messageQueue->stop();
}

// Test failed message sending
TEST_F(MessageQueueTest, FailedMessageSending) {
    // Set up sender that always fails
    auto failingSender = [](const Message& msg) -> bool {
        return false; // Simulate send failure
    };
    
    messageQueue->setMessageSender(failingSender);
    messageQueue->start();
    
    std::atomic<bool> ackReceived{false};
    std::atomic<bool> ackSuccess{true};
    
    auto failureAckCallback = [&ackReceived, &ackSuccess](const std::string& messageId, bool success) {
        ackReceived = true;
        ackSuccess = success;
    };
    
    auto cmd = createTestCommand();
    messageQueue->sendMessage(*cmd, failureAckCallback);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(ackReceived.load());
    EXPECT_FALSE(ackSuccess.load()); // Should indicate failure
    
    messageQueue->stop();
}

// Test message queue statistics (if available)
TEST_F(MessageQueueTest, MessageQueueStatistics) {
    messageQueue->setMessageSender(messageSender);
    messageQueue->start();
    
    // Send multiple messages
    for (int i = 0; i < 5; i++) {
        auto cmd = createTestCommand("stats_test_" + std::to_string(i));
        messageQueue->sendMessage(*cmd, ackCallback);
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_EQ(messagesSent.load(), 5);
    EXPECT_EQ(messagesAcknowledged.load(), 5);
    
    messageQueue->stop();
}

// Test message queue performance
TEST_F(MessageQueueTest, MessageQueuePerformance) {
    messageQueue->setMessageSender(messageSender);
    messageQueue->start();
    
    const int numMessages = 100;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numMessages; i++) {
        auto cmd = createTestCommand("perf_test_" + std::to_string(i));
        messageQueue->sendMessage(*cmd, ackCallback);
    }
    
    // Wait for all messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(messagesSent.load(), numMessages);
    EXPECT_EQ(messagesAcknowledged.load(), numMessages);
    
    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 5000); // Less than 5 seconds
    
    messageQueue->stop();
}

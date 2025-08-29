#include <gtest/gtest.h>
#include <astrocomm/core/message.h>
#include "test_helpers.h"

using namespace astrocomm::core;
using namespace astrocomm::test;

class MessageTest : public MessageTestBase {
protected:
    void SetUp() override {
        MessageTestBase::SetUp();
    }
};

// Test basic message creation and properties
TEST_F(MessageTest, BasicMessageCreation) {
    auto cmd = createTestCommand();
    
    EXPECT_FALSE(cmd->getMessageId().empty());
    EXPECT_FALSE(cmd->getTimestamp().empty());
    EXPECT_EQ(cmd->getDeviceId(), testDeviceId);
    EXPECT_EQ(cmd->getMessageType(), MessageType::COMMAND);
    EXPECT_EQ(cmd->getPriority(), Message::Priority::NORMAL);
    EXPECT_EQ(cmd->getQoSLevel(), Message::QoSLevel::AT_MOST_ONCE);
}

// Test message serialization and deserialization
TEST_F(MessageTest, MessageSerialization) {
    auto cmd = createTestCommand();
    cmd->setCommand("test_command");
    cmd->setParameters({{"param1", "value1"}, {"param2", 42}});
    
    // Serialize to JSON
    auto json = cmd->toJson();
    
    // Verify JSON structure
    EXPECT_TRUE(json.contains("messageType"));
    EXPECT_TRUE(json.contains("messageId"));
    EXPECT_TRUE(json.contains("deviceId"));
    EXPECT_TRUE(json.contains("timestamp"));
    EXPECT_TRUE(json.contains("command"));
    EXPECT_TRUE(json.contains("parameters"));
    
    EXPECT_EQ(json["messageType"], "COMMAND");
    EXPECT_EQ(json["deviceId"], testDeviceId);
    EXPECT_EQ(json["command"], "test_command");
    
    // Create new message and deserialize
    auto cmd2 = std::make_unique<CommandMessage>();
    cmd2->fromJson(json);
    
    EXPECT_EQ(cmd2->getMessageId(), cmd->getMessageId());
    EXPECT_EQ(cmd2->getDeviceId(), cmd->getDeviceId());
    EXPECT_EQ(cmd2->getCommand(), cmd->getCommand());
}

// Test message priority levels
TEST_F(MessageTest, MessagePriority) {
    auto cmd = createTestCommand();
    
    // Test all priority levels
    cmd->setPriority(Message::Priority::LOW);
    EXPECT_EQ(cmd->getPriority(), Message::Priority::LOW);
    
    cmd->setPriority(Message::Priority::NORMAL);
    EXPECT_EQ(cmd->getPriority(), Message::Priority::NORMAL);
    
    cmd->setPriority(Message::Priority::HIGH);
    EXPECT_EQ(cmd->getPriority(), Message::Priority::HIGH);
    
    cmd->setPriority(Message::Priority::CRITICAL);
    EXPECT_EQ(cmd->getPriority(), Message::Priority::CRITICAL);
}

// Test QoS levels
TEST_F(MessageTest, MessageQoS) {
    auto cmd = createTestCommand();
    
    // Test all QoS levels
    cmd->setQoSLevel(Message::QoSLevel::AT_MOST_ONCE);
    EXPECT_EQ(cmd->getQoSLevel(), Message::QoSLevel::AT_MOST_ONCE);
    
    cmd->setQoSLevel(Message::QoSLevel::AT_LEAST_ONCE);
    EXPECT_EQ(cmd->getQoSLevel(), Message::QoSLevel::AT_LEAST_ONCE);
    
    cmd->setQoSLevel(Message::QoSLevel::EXACTLY_ONCE);
    EXPECT_EQ(cmd->getQoSLevel(), Message::QoSLevel::EXACTLY_ONCE);
}

// Test message expiration
TEST_F(MessageTest, MessageExpiration) {
    auto cmd = createTestCommand();
    
    // Initially should not expire
    EXPECT_FALSE(cmd->isExpired());
    
    // Set expiration to 1 second
    cmd->setExpireAfter(1);
    EXPECT_EQ(cmd->getExpireAfter(), 1);
    
    // Should not be expired immediately
    EXPECT_FALSE(cmd->isExpired());
    
    // Wait and check expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_TRUE(cmd->isExpired());
}

// Test response message
TEST_F(MessageTest, ResponseMessage) {
    auto resp = createTestResponse();
    
    EXPECT_EQ(resp->getMessageType(), MessageType::RESPONSE);
    EXPECT_TRUE(resp->isSuccess());
    EXPECT_EQ(resp->getMessage(), "Test response");
    
    // Test error response
    resp->setSuccess(false);
    resp->setErrorCode("TEST_ERROR");
    resp->setMessage("Test error message");
    
    EXPECT_FALSE(resp->isSuccess());
    EXPECT_EQ(resp->getErrorCode(), "TEST_ERROR");
    EXPECT_EQ(resp->getMessage(), "Test error message");
}

// Test event message
TEST_F(MessageTest, EventMessage) {
    auto event = createTestEvent();
    
    EXPECT_EQ(event->getMessageType(), MessageType::EVENT);
    EXPECT_EQ(event->getEventType(), "test_event");
    
    // Add event data
    event->setEventData({{"key1", "value1"}, {"key2", 123}});
    auto data = event->getEventData();
    EXPECT_EQ(data["key1"], "value1");
    EXPECT_EQ(data["key2"], 123);
}

// Test error message
TEST_F(MessageTest, ErrorMessage) {
    auto error = createTestError();
    
    EXPECT_EQ(error->getMessageType(), MessageType::ERR);
    EXPECT_EQ(error->getErrorCode(), "TEST_ERROR");
    EXPECT_EQ(error->getErrorMessage(), "Test error message");
    
    // Test severity levels
    error->setSeverity(ErrorMessage::Severity::WARNING);
    EXPECT_EQ(error->getSeverity(), ErrorMessage::Severity::WARNING);
    
    error->setSeverity(ErrorMessage::Severity::ERROR);
    EXPECT_EQ(error->getSeverity(), ErrorMessage::Severity::ERROR);
    
    error->setSeverity(ErrorMessage::Severity::CRITICAL);
    EXPECT_EQ(error->getSeverity(), ErrorMessage::Severity::CRITICAL);
}

// Test message type conversion functions
TEST_F(MessageTest, MessageTypeConversion) {
    EXPECT_EQ(messageTypeToString(MessageType::COMMAND), "COMMAND");
    EXPECT_EQ(messageTypeToString(MessageType::RESPONSE), "RESPONSE");
    EXPECT_EQ(messageTypeToString(MessageType::EVENT), "EVENT");
    EXPECT_EQ(messageTypeToString(MessageType::ERR), "ERROR");
    
    EXPECT_EQ(stringToMessageType("COMMAND"), MessageType::COMMAND);
    EXPECT_EQ(stringToMessageType("RESPONSE"), MessageType::RESPONSE);
    EXPECT_EQ(stringToMessageType("EVENT"), MessageType::EVENT);
    EXPECT_EQ(stringToMessageType("ERROR"), MessageType::ERR);
}

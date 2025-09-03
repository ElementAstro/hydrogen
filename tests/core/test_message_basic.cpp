/**
 * @file test_message_basic.cpp
 * @brief Comprehensive tests for basic message functionality
 *
 * Tests the core message system including Message base class,
 * CommandMessage, ResponseMessage, EventMessage, and ErrorMessage.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <hydrogen/core/message.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

using namespace hydrogen::core;
using json = nlohmann::json;
using namespace testing;

class MessageBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDeviceId = "test_device_001";
        testCommand = "test_command";
        testParameters = {{"param1", "value1"}, {"param2", 42}, {"param3", true}};
    }

    void TearDown() override {
        // Test cleanup
    }

    std::string testDeviceId;
    std::string testCommand;
    json testParameters;
};

// Test Message base class functionality
TEST_F(MessageBasicTest, MessageBaseClassCreation) {
    CommandMessage msg;

    // Test that basic properties are set
    EXPECT_FALSE(msg.getMessageId().empty());
    EXPECT_FALSE(msg.getTimestamp().empty());
    EXPECT_EQ(msg.getMessageType(), MessageType::COMMAND);
    EXPECT_EQ(msg.getPriority(), Message::Priority::NORMAL);
    EXPECT_EQ(msg.getQoSLevel(), Message::QoSLevel::AT_MOST_ONCE);
    EXPECT_EQ(msg.getExpireAfterSeconds(), 0);
}

TEST_F(MessageBasicTest, MessageIdUniqueness) {
    CommandMessage msg1;
    CommandMessage msg2;

    // Each message should have a unique ID
    EXPECT_NE(msg1.getMessageId(), msg2.getMessageId());
}

TEST_F(MessageBasicTest, MessageTimestampFormat) {
    CommandMessage msg;
    std::string timestamp = msg.getTimestamp();

    // Timestamp should be in ISO format (basic validation)
    EXPECT_GT(timestamp.length(), 10); // Should be longer than just a date
    EXPECT_NE(timestamp.find('T'), std::string::npos); // Should contain 'T' separator
}

TEST_F(MessageBasicTest, MessagePropertySettersAndGetters) {
    CommandMessage msg;

    // Test device ID
    msg.setDeviceId(testDeviceId);
    EXPECT_EQ(msg.getDeviceId(), testDeviceId);

    // Test priority
    msg.setPriority(Message::Priority::HIGH);
    EXPECT_EQ(msg.getPriority(), Message::Priority::HIGH);

    // Test QoS level
    msg.setQoSLevel(Message::QoSLevel::EXACTLY_ONCE);
    EXPECT_EQ(msg.getQoSLevel(), Message::QoSLevel::EXACTLY_ONCE);

    // Test expiration
    msg.setExpireAfterSeconds(300);
    EXPECT_EQ(msg.getExpireAfterSeconds(), 300);

    // Test original message ID
    std::string originalId = "original_123";
    msg.setOriginalMessageId(originalId);
    EXPECT_EQ(msg.getOriginalMessageId(), originalId);
}

// Test CommandMessage specific functionality
TEST_F(MessageBasicTest, CommandMessageCreation) {
    CommandMessage cmd(testCommand);

    EXPECT_EQ(cmd.getCommand(), testCommand);
    EXPECT_EQ(cmd.getMessageType(), MessageType::COMMAND);
}

TEST_F(MessageBasicTest, CommandMessageParameters) {
    CommandMessage cmd;
    cmd.setCommand(testCommand);
    cmd.setParameters(testParameters);

    json retrievedParams = cmd.getParameters();
    EXPECT_EQ(retrievedParams["param1"], "value1");
    EXPECT_EQ(retrievedParams["param2"], 42);
    EXPECT_EQ(retrievedParams["param3"], true);
}

TEST_F(MessageBasicTest, CommandMessageProperties) {
    CommandMessage cmd;
    json properties = {{"prop1", "propValue1"}, {"prop2", 123}};

    cmd.setProperties(properties);
    json retrievedProps = cmd.getProperties();

    EXPECT_EQ(retrievedProps["prop1"], "propValue1");
    EXPECT_EQ(retrievedProps["prop2"], 123);
}

// Test ResponseMessage functionality
TEST_F(MessageBasicTest, ResponseMessageCreation) {
    ResponseMessage response;

    EXPECT_EQ(response.getMessageType(), MessageType::RESPONSE);
    EXPECT_FALSE(response.isSuccess()); // Default should be false
    EXPECT_TRUE(response.getMessage().empty()); // Default should be empty
}

TEST_F(MessageBasicTest, ResponseMessageProperties) {
    ResponseMessage response;
    std::string responseMsg = "Operation completed successfully";
    json responseData = {{"result", "success"}, {"value", 42}};

    response.setSuccess(true);
    response.setMessage(responseMsg);
    response.setData(responseData);

    EXPECT_TRUE(response.isSuccess());
    EXPECT_EQ(response.getMessage(), responseMsg);
    EXPECT_EQ(response.getData()["result"], "success");
    EXPECT_EQ(response.getData()["value"], 42);
}

// Test EventMessage functionality
TEST_F(MessageBasicTest, EventMessageCreation) {
    std::string eventType = "device_connected";
    EventMessage event(eventType);

    EXPECT_EQ(event.getMessageType(), MessageType::EVENT);
    EXPECT_EQ(event.getEventType(), eventType);
}

TEST_F(MessageBasicTest, EventMessageData) {
    EventMessage event("status_change");
    json eventData = {{"old_status", "disconnected"}, {"new_status", "connected"}};

    event.setEventData(eventData);
    json retrievedData = event.getEventData();

    EXPECT_EQ(retrievedData["old_status"], "disconnected");
    EXPECT_EQ(retrievedData["new_status"], "connected");
}

// Test ErrorMessage functionality
TEST_F(MessageBasicTest, ErrorMessageCreation) {
    std::string errorCode = "CONNECTION_FAILED";
    std::string errorMsg = "Failed to connect to device";
    ErrorMessage error(errorCode, errorMsg);

    EXPECT_EQ(error.getMessageType(), MessageType::ERR);
    EXPECT_EQ(error.getErrorCode(), errorCode);
    EXPECT_EQ(error.getErrorMessage(), errorMsg);
}

TEST_F(MessageBasicTest, ErrorMessageSeverity) {
    ErrorMessage error("TEST_ERROR", "Test error message");

    // Test default severity
    EXPECT_EQ(error.getSeverity(), ErrorMessage::Severity::ERROR);

    // Test setting severity
    error.setSeverity(ErrorMessage::Severity::CRITICAL);
    EXPECT_EQ(error.getSeverity(), ErrorMessage::Severity::CRITICAL);
}

// Test message serialization and deserialization
TEST_F(MessageBasicTest, MessageJsonSerialization) {
    CommandMessage cmd(testCommand);
    cmd.setDeviceId(testDeviceId);
    cmd.setParameters(testParameters);
    cmd.setPriority(Message::Priority::HIGH);
    cmd.setQoSLevel(Message::QoSLevel::AT_LEAST_ONCE);

    json serialized = cmd.toJson();

    // Verify all required fields are present
    EXPECT_TRUE(serialized.contains("messageType"));
    EXPECT_TRUE(serialized.contains("messageId"));
    EXPECT_TRUE(serialized.contains("timestamp"));
    EXPECT_TRUE(serialized.contains("deviceId"));
    EXPECT_TRUE(serialized.contains("command"));
    EXPECT_TRUE(serialized.contains("parameters"));
    EXPECT_TRUE(serialized.contains("priority"));
    EXPECT_TRUE(serialized.contains("qos"));

    // Verify field values
    EXPECT_EQ(serialized["messageType"], "COMMAND");
    EXPECT_EQ(serialized["deviceId"], testDeviceId);
    EXPECT_EQ(serialized["command"], testCommand);
    EXPECT_EQ(serialized["priority"], static_cast<int>(Message::Priority::HIGH));
    EXPECT_EQ(serialized["qos"], static_cast<int>(Message::QoSLevel::AT_LEAST_ONCE));
}

TEST_F(MessageBasicTest, MessageJsonDeserialization) {
    // Create a JSON representation of a command message
    json messageJson = {
        {"messageType", "COMMAND"},
        {"messageId", "test_msg_123"},
        {"timestamp", "2023-01-01T12:00:00Z"},
        {"deviceId", testDeviceId},
        {"command", testCommand},
        {"parameters", testParameters},
        {"priority", static_cast<int>(Message::Priority::HIGH)},
        {"qos", static_cast<int>(Message::QoSLevel::EXACTLY_ONCE)},
        {"expireAfter", 300}
    };

    CommandMessage cmd;
    cmd.fromJson(messageJson);

    // Verify deserialization
    EXPECT_EQ(cmd.getMessageId(), "test_msg_123");
    EXPECT_EQ(cmd.getDeviceId(), testDeviceId);
    EXPECT_EQ(cmd.getCommand(), testCommand);
    EXPECT_EQ(cmd.getPriority(), Message::Priority::HIGH);
    EXPECT_EQ(cmd.getQoSLevel(), Message::QoSLevel::EXACTLY_ONCE);
    EXPECT_EQ(cmd.getExpireAfterSeconds(), 300);

    json params = cmd.getParameters();
    EXPECT_EQ(params["param1"], "value1");
    EXPECT_EQ(params["param2"], 42);
    EXPECT_EQ(params["param3"], true);
}

TEST_F(MessageBasicTest, MessageRoundTripSerialization) {
    // Create original message
    CommandMessage original(testCommand);
    original.setDeviceId(testDeviceId);
    original.setParameters(testParameters);
    original.setPriority(Message::Priority::CRITICAL);
    original.setQoSLevel(Message::QoSLevel::EXACTLY_ONCE);
    original.setExpireAfterSeconds(600);

    // Serialize and deserialize
    json serialized = original.toJson();
    CommandMessage deserialized;
    deserialized.fromJson(serialized);

    // Verify round-trip integrity
    EXPECT_EQ(deserialized.getMessageId(), original.getMessageId());
    EXPECT_EQ(deserialized.getDeviceId(), original.getDeviceId());
    EXPECT_EQ(deserialized.getCommand(), original.getCommand());
    EXPECT_EQ(deserialized.getPriority(), original.getPriority());
    EXPECT_EQ(deserialized.getQoSLevel(), original.getQoSLevel());
    EXPECT_EQ(deserialized.getExpireAfterSeconds(), original.getExpireAfterSeconds());

    json originalParams = original.getParameters();
    json deserializedParams = deserialized.getParameters();
    EXPECT_EQ(deserializedParams, originalParams);
}

// Test message type conversion utilities
TEST_F(MessageBasicTest, MessageTypeConversion) {
    // Test string to MessageType conversion
    EXPECT_EQ(stringToMessageType("COMMAND"), MessageType::COMMAND);
    EXPECT_EQ(stringToMessageType("RESPONSE"), MessageType::RESPONSE);
    EXPECT_EQ(stringToMessageType("EVENT"), MessageType::EVENT);
    EXPECT_EQ(stringToMessageType("ERR"), MessageType::ERR);

    // Test MessageType to string conversion
    EXPECT_EQ(messageTypeToString(MessageType::COMMAND), "COMMAND");
    EXPECT_EQ(messageTypeToString(MessageType::RESPONSE), "RESPONSE");
    EXPECT_EQ(messageTypeToString(MessageType::EVENT), "EVENT");
    EXPECT_EQ(messageTypeToString(MessageType::ERR), "ERR");
}

// Test message validation
TEST_F(MessageBasicTest, MessageValidation) {
    CommandMessage cmd;

    // Test invalid command (empty)
    EXPECT_FALSE(cmd.isValid()); // Should be invalid without command

    // Set valid command
    cmd.setCommand(testCommand);
    EXPECT_TRUE(cmd.isValid());

    // Test with device ID
    cmd.setDeviceId(testDeviceId);
    EXPECT_TRUE(cmd.isValid());
}

// Test message expiration
TEST_F(MessageBasicTest, MessageExpiration) {
    CommandMessage cmd(testCommand);

    // Test non-expiring message
    EXPECT_FALSE(cmd.isExpired());

    // Test expiring message
    cmd.setExpireAfterSeconds(1); // 1 second expiration
    EXPECT_FALSE(cmd.isExpired()); // Should not be expired immediately

    // Wait and test expiration (this is a simplified test)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_TRUE(cmd.isExpired());
}

// Test message cloning/copying
TEST_F(MessageBasicTest, MessageCloning) {
    CommandMessage original(testCommand);
    original.setDeviceId(testDeviceId);
    original.setParameters(testParameters);
    original.setPriority(Message::Priority::HIGH);

    // Test copy constructor
    CommandMessage copy(original);
    EXPECT_EQ(copy.getCommand(), original.getCommand());
    EXPECT_EQ(copy.getDeviceId(), original.getDeviceId());
    EXPECT_EQ(copy.getPriority(), original.getPriority());

    // Parameters should be equal but message ID should be different (new message)
    EXPECT_EQ(copy.getParameters(), original.getParameters());
    EXPECT_NE(copy.getMessageId(), original.getMessageId());
}

// Test error conditions
TEST_F(MessageBasicTest, ErrorConditions) {
    // Test invalid JSON deserialization
    CommandMessage cmd;
    json invalidJson = {{"invalid", "data"}};

    EXPECT_THROW(cmd.fromJson(invalidJson), std::exception);

    // Test invalid message type
    json invalidTypeJson = {
        {"messageType", "INVALID_TYPE"},
        {"messageId", "test_123"},
        {"timestamp", "2023-01-01T12:00:00Z"}
    };

    EXPECT_THROW(cmd.fromJson(invalidTypeJson), std::exception);
}

// Test message toString functionality
TEST_F(MessageBasicTest, MessageToString) {
    CommandMessage cmd(testCommand);
    cmd.setDeviceId(testDeviceId);

    std::string msgString = cmd.toString();
    EXPECT_FALSE(msgString.empty());

    // Should be valid JSON
    json parsed = json::parse(msgString);
    EXPECT_EQ(parsed["command"], testCommand);
    EXPECT_EQ(parsed["deviceId"], testDeviceId);
}

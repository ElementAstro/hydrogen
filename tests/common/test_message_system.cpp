/**
 * @file test_message_system.cpp
 * @brief Comprehensive tests for the message system
 * 
 * Tests the complete message system including all message types,
 * serialization, validation, and message handling.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <common/message.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>

using namespace hydrogen;
using json = nlohmann::json;
using namespace testing;

class MessageSystemTest : public ::testing::Test {
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

// Test message type enumeration
TEST_F(MessageSystemTest, MessageTypeEnumeration) {
    // Test all message types
    EXPECT_EQ(messageTypeToString(MessageType::COMMAND), "COMMAND");
    EXPECT_EQ(messageTypeToString(MessageType::RESPONSE), "RESPONSE");
    EXPECT_EQ(messageTypeToString(MessageType::EVENT), "EVENT");
    EXPECT_EQ(messageTypeToString(MessageType::ERR), "ERR");
    EXPECT_EQ(messageTypeToString(MessageType::DISCOVERY_REQUEST), "DISCOVERY_REQUEST");
    EXPECT_EQ(messageTypeToString(MessageType::DISCOVERY_RESPONSE), "DISCOVERY_RESPONSE");
    EXPECT_EQ(messageTypeToString(MessageType::REGISTRATION), "REGISTRATION");
    EXPECT_EQ(messageTypeToString(MessageType::AUTHENTICATION), "AUTHENTICATION");
    
    // Test reverse conversion
    EXPECT_EQ(stringToMessageType("COMMAND"), MessageType::COMMAND);
    EXPECT_EQ(stringToMessageType("RESPONSE"), MessageType::RESPONSE);
    EXPECT_EQ(stringToMessageType("EVENT"), MessageType::EVENT);
    EXPECT_EQ(stringToMessageType("ERR"), MessageType::ERR);
    EXPECT_EQ(stringToMessageType("DISCOVERY_REQUEST"), MessageType::DISCOVERY_REQUEST);
    EXPECT_EQ(stringToMessageType("DISCOVERY_RESPONSE"), MessageType::DISCOVERY_RESPONSE);
    EXPECT_EQ(stringToMessageType("REGISTRATION"), MessageType::REGISTRATION);
    EXPECT_EQ(stringToMessageType("AUTHENTICATION"), MessageType::AUTHENTICATION);
}

// Test command message functionality
TEST_F(MessageSystemTest, CommandMessageFunctionality) {
    CommandMessage cmd(testCommand);
    cmd.setDeviceId(testDeviceId);
    cmd.setParameters(testParameters);
    cmd.setPriority(Message::Priority::HIGH);
    cmd.setQoSLevel(Message::QoSLevel::AT_LEAST_ONCE);
    
    // Test getters
    EXPECT_EQ(cmd.getCommand(), testCommand);
    EXPECT_EQ(cmd.getDeviceId(), testDeviceId);
    EXPECT_EQ(cmd.getParameters(), testParameters);
    EXPECT_EQ(cmd.getPriority(), Message::Priority::HIGH);
    EXPECT_EQ(cmd.getQoSLevel(), Message::QoSLevel::AT_LEAST_ONCE);
    EXPECT_EQ(cmd.getMessageType(), MessageType::COMMAND);
    
    // Test properties
    json properties = {{"prop1", "value1"}, {"prop2", 123}};
    cmd.setProperties(properties);
    EXPECT_EQ(cmd.getProperties(), properties);
}

// Test response message functionality
TEST_F(MessageSystemTest, ResponseMessageFunctionality) {
    ResponseMessage response;
    response.setDeviceId(testDeviceId);
    response.setSuccess(true);
    response.setMessage("Operation successful");
    
    json responseData = {{"result", "success"}, {"value", 42}};
    response.setData(responseData);
    
    // Test getters
    EXPECT_EQ(response.getDeviceId(), testDeviceId);
    EXPECT_TRUE(response.isSuccess());
    EXPECT_EQ(response.getMessage(), "Operation successful");
    EXPECT_EQ(response.getData(), responseData);
    EXPECT_EQ(response.getMessageType(), MessageType::RESPONSE);
}

// Test event message functionality
TEST_F(MessageSystemTest, EventMessageFunctionality) {
    std::string eventType = "device_connected";
    EventMessage event(eventType);
    event.setDeviceId(testDeviceId);
    
    json eventData = {{"connection_type", "USB"}, {"port", "/dev/ttyUSB0"}};
    event.setEventData(eventData);
    
    // Test getters
    EXPECT_EQ(event.getEventType(), eventType);
    EXPECT_EQ(event.getDeviceId(), testDeviceId);
    EXPECT_EQ(event.getEventData(), eventData);
    EXPECT_EQ(event.getMessageType(), MessageType::EVENT);
}

// Test error message functionality
TEST_F(MessageSystemTest, ErrorMessageFunctionality) {
    std::string errorCode = "CONNECTION_FAILED";
    std::string errorMessage = "Failed to connect to device";
    ErrorMessage error(errorCode, errorMessage);
    error.setDeviceId(testDeviceId);
    
    json errorDetails = {{"error_code", 500}, {"retry_count", 3}};
    error.setDetails(errorDetails);
    
    // Test getters
    EXPECT_EQ(error.getErrorCode(), errorCode);
    EXPECT_EQ(error.getErrorMessage(), errorMessage);
    EXPECT_EQ(error.getDeviceId(), testDeviceId);
    EXPECT_EQ(error.getDetails(), errorDetails);
    EXPECT_EQ(error.getMessageType(), MessageType::ERR);
}

// Test message serialization consistency
TEST_F(MessageSystemTest, MessageSerializationConsistency) {
    // Test command message serialization
    CommandMessage cmd(testCommand);
    cmd.setDeviceId(testDeviceId);
    cmd.setParameters(testParameters);
    
    json cmdJson = cmd.toJson();
    CommandMessage deserializedCmd;
    deserializedCmd.fromJson(cmdJson);
    
    EXPECT_EQ(deserializedCmd.getCommand(), cmd.getCommand());
    EXPECT_EQ(deserializedCmd.getDeviceId(), cmd.getDeviceId());
    EXPECT_EQ(deserializedCmd.getParameters(), cmd.getParameters());
    
    // Test response message serialization
    ResponseMessage response;
    response.setDeviceId(testDeviceId);
    response.setSuccess(true);
    response.setMessage("Test response");
    
    json responseJson = response.toJson();
    ResponseMessage deserializedResponse;
    deserializedResponse.fromJson(responseJson);
    
    EXPECT_EQ(deserializedResponse.getDeviceId(), response.getDeviceId());
    EXPECT_EQ(deserializedResponse.isSuccess(), response.isSuccess());
    EXPECT_EQ(deserializedResponse.getMessage(), response.getMessage());
}

// Test message validation
TEST_F(MessageSystemTest, MessageValidation) {
    // Test valid command message
    CommandMessage validCmd(testCommand);
    validCmd.setDeviceId(testDeviceId);
    EXPECT_TRUE(validCmd.isValid());
    
    // Test invalid command message (empty command)
    CommandMessage invalidCmd;
    EXPECT_FALSE(invalidCmd.isValid());
    
    // Test command with device ID
    CommandMessage cmdWithDevice(testCommand);
    cmdWithDevice.setDeviceId(testDeviceId);
    EXPECT_TRUE(cmdWithDevice.isValid());
}

// Test message expiration
TEST_F(MessageSystemTest, MessageExpiration) {
    CommandMessage cmd(testCommand);
    
    // Test non-expiring message
    EXPECT_FALSE(cmd.isExpired());
    
    // Test expiring message
    cmd.setExpireAfterSeconds(1);
    EXPECT_FALSE(cmd.isExpired()); // Should not be expired immediately
    
    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_TRUE(cmd.isExpired());
}

// Test message priority and QoS levels
TEST_F(MessageSystemTest, MessagePriorityAndQoS) {
    CommandMessage cmd(testCommand);
    
    // Test all priority levels
    cmd.setPriority(Message::Priority::LOW);
    EXPECT_EQ(cmd.getPriority(), Message::Priority::LOW);
    
    cmd.setPriority(Message::Priority::NORMAL);
    EXPECT_EQ(cmd.getPriority(), Message::Priority::NORMAL);
    
    cmd.setPriority(Message::Priority::HIGH);
    EXPECT_EQ(cmd.getPriority(), Message::Priority::HIGH);
    
    cmd.setPriority(Message::Priority::CRITICAL);
    EXPECT_EQ(cmd.getPriority(), Message::Priority::CRITICAL);
    
    // Test all QoS levels
    cmd.setQoSLevel(Message::QoSLevel::AT_MOST_ONCE);
    EXPECT_EQ(cmd.getQoSLevel(), Message::QoSLevel::AT_MOST_ONCE);
    
    cmd.setQoSLevel(Message::QoSLevel::AT_LEAST_ONCE);
    EXPECT_EQ(cmd.getQoSLevel(), Message::QoSLevel::AT_LEAST_ONCE);
    
    cmd.setQoSLevel(Message::QoSLevel::EXACTLY_ONCE);
    EXPECT_EQ(cmd.getQoSLevel(), Message::QoSLevel::EXACTLY_ONCE);
}

// Test message copying and assignment
TEST_F(MessageSystemTest, MessageCopyingAndAssignment) {
    CommandMessage original(testCommand);
    original.setDeviceId(testDeviceId);
    original.setParameters(testParameters);
    original.setPriority(Message::Priority::HIGH);
    
    // Test copy constructor
    CommandMessage copy(original);
    EXPECT_EQ(copy.getCommand(), original.getCommand());
    EXPECT_EQ(copy.getDeviceId(), original.getDeviceId());
    EXPECT_EQ(copy.getParameters(), original.getParameters());
    EXPECT_EQ(copy.getPriority(), original.getPriority());
    
    // Message ID should be different (new message)
    EXPECT_NE(copy.getMessageId(), original.getMessageId());
}

// Test message polymorphism
TEST_F(MessageSystemTest, MessagePolymorphism) {
    std::vector<std::unique_ptr<Message>> messages;
    
    // Create different types of messages
    auto cmd = std::make_unique<CommandMessage>(testCommand);
    cmd->setDeviceId(testDeviceId);
    
    auto response = std::make_unique<ResponseMessage>();
    response->setDeviceId(testDeviceId);
    response->setSuccess(true);
    
    auto event = std::make_unique<EventMessage>("test_event");
    event->setDeviceId(testDeviceId);
    
    auto error = std::make_unique<ErrorMessage>("TEST_ERROR", "Test error");
    error->setDeviceId(testDeviceId);
    
    // Store in polymorphic container
    messages.push_back(std::move(cmd));
    messages.push_back(std::move(response));
    messages.push_back(std::move(event));
    messages.push_back(std::move(error));
    
    // Test polymorphic behavior
    EXPECT_EQ(messages[0]->getMessageType(), MessageType::COMMAND);
    EXPECT_EQ(messages[1]->getMessageType(), MessageType::RESPONSE);
    EXPECT_EQ(messages[2]->getMessageType(), MessageType::EVENT);
    EXPECT_EQ(messages[3]->getMessageType(), MessageType::ERR);
    
    // All should have the same device ID
    for (const auto& msg : messages) {
        EXPECT_EQ(msg->getDeviceId(), testDeviceId);
    }
}

// Test message toString functionality
TEST_F(MessageSystemTest, MessageToString) {
    CommandMessage cmd(testCommand);
    cmd.setDeviceId(testDeviceId);
    cmd.setParameters(testParameters);
    
    std::string msgString = cmd.toString();
    EXPECT_FALSE(msgString.empty());
    
    // Should be valid JSON
    json parsed = json::parse(msgString);
    EXPECT_EQ(parsed["command"], testCommand);
    EXPECT_EQ(parsed["deviceId"], testDeviceId);
    EXPECT_EQ(parsed["parameters"], testParameters);
}

// Test message error handling
TEST_F(MessageSystemTest, MessageErrorHandling) {
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

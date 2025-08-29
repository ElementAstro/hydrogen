#include <gtest/gtest.h>
#include "../../src/core/include/astrocomm/core/message.h"
#include "../../src/core/include/astrocomm/core/utils.h"
#include "../utils/simple_helpers.h"

using namespace astrocomm::core;
using namespace astrocomm::test;

class MessageBasicTest : public SimpleTestBase {
protected:
    void SetUp() override {
        SimpleTestBase::SetUp();
    }
};

TEST_F(MessageBasicTest, CreateCommandMessage) {
    CommandMessage cmd("test_command");

    // Test basic properties
    EXPECT_EQ(cmd.getCommand(), "test_command");
    EXPECT_EQ(cmd.getMessageType(), MessageType::COMMAND);

    // Test that message ID is generated
    EXPECT_FALSE(cmd.getMessageId().empty());

    // Test that timestamp is set
    EXPECT_FALSE(cmd.getTimestamp().empty());
}

TEST_F(MessageBasicTest, CreateResponseMessage) {
    ResponseMessage resp;
    
    // Test basic properties
    EXPECT_EQ(resp.getMessageType(), MessageType::RESPONSE);
    
    // Test setting status
    resp.setStatus("success");
    EXPECT_EQ(resp.getStatus(), "success");
    
    // Test setting command
    resp.setCommand("test_command");
    EXPECT_EQ(resp.getCommand(), "test_command");
}

TEST_F(MessageBasicTest, CreateEventMessage) {
    EventMessage event("test_event");

    // Test basic properties
    EXPECT_EQ(event.getEvent(), "test_event");
    EXPECT_EQ(event.getMessageType(), MessageType::EVENT);

    // Test that message ID is generated
    EXPECT_FALSE(event.getMessageId().empty());
}

TEST_F(MessageBasicTest, CreateErrorMessage) {
    ErrorMessage error("TEST_ERROR", "Test error message");
    
    // Test basic properties
    EXPECT_EQ(error.getErrorCode(), "TEST_ERROR");
    EXPECT_EQ(error.getErrorMessage(), "Test error message");
    EXPECT_EQ(error.getMessageType(), MessageType::ERR);
}

TEST_F(MessageBasicTest, MessageSerialization) {
    CommandMessage cmd("test_command");
    cmd.setDeviceId("test_device");
    
    // Test JSON serialization
    auto json = cmd.toJson();
    EXPECT_TRUE(json.contains("messageType"));
    EXPECT_TRUE(json.contains("messageId"));
    EXPECT_TRUE(json.contains("timestamp"));
    EXPECT_TRUE(json.contains("deviceId"));
    EXPECT_TRUE(json.contains("command"));
    
    // Test values
    EXPECT_EQ(json["command"], "test_command");
    EXPECT_EQ(json["deviceId"], "test_device");
}

TEST_F(MessageBasicTest, MessageDeserialization) {
    // Create a JSON object
    json j = {
        {"messageType", "COMMAND"},
        {"messageId", "test_123"},
        {"timestamp", "2023-01-01T00:00:00.000Z"},
        {"deviceId", "test_device"},
        {"command", "test_command"},
        {"parameters", json::object()}
    };
    
    CommandMessage cmd;
    cmd.fromJson(j);
    
    // Test that values were set correctly
    EXPECT_EQ(cmd.getMessageId(), "test_123");
    EXPECT_EQ(cmd.getDeviceId(), "test_device");
    EXPECT_EQ(cmd.getCommand(), "test_command");
}

TEST_F(MessageBasicTest, MessageTypeConversion) {
    // Test string to MessageType conversion
    EXPECT_EQ(stringToMessageType("COMMAND"), MessageType::COMMAND);
    EXPECT_EQ(stringToMessageType("RESPONSE"), MessageType::RESPONSE);
    EXPECT_EQ(stringToMessageType("EVENT"), MessageType::EVENT);
    EXPECT_EQ(stringToMessageType("ERROR"), MessageType::ERR);  // Use "ERROR" instead of "ERR"

    // Test MessageType to string conversion
    EXPECT_EQ(messageTypeToString(MessageType::COMMAND), "COMMAND");
    EXPECT_EQ(messageTypeToString(MessageType::RESPONSE), "RESPONSE");
    EXPECT_EQ(messageTypeToString(MessageType::EVENT), "EVENT");
    EXPECT_EQ(messageTypeToString(MessageType::ERR), "ERROR");  // Expect "ERROR" instead of "ERR"
}

TEST_F(MessageBasicTest, UtilityFunctions) {
    // Test UUID generation
    std::string uuid1 = generateUuid();
    std::string uuid2 = generateUuid();

    EXPECT_FALSE(uuid1.empty());
    EXPECT_FALSE(uuid2.empty());
    EXPECT_NE(uuid1, uuid2); // Should be different

    // Test timestamp generation
    std::string timestamp = getIsoTimestamp();
    EXPECT_FALSE(timestamp.empty());
    EXPECT_TRUE(timestamp.find("T") != std::string::npos); // Should contain 'T'
    EXPECT_TRUE(timestamp.find("Z") != std::string::npos); // Should end with 'Z'
}

TEST_F(MessageBasicTest, StringUtilities) {
    // Test trim function
    EXPECT_EQ(string_utils::trim("  hello  "), "hello");
    EXPECT_EQ(string_utils::trim("\t\nhello\t\n"), "hello");
    EXPECT_EQ(string_utils::trim("hello"), "hello");
    EXPECT_EQ(string_utils::trim(""), "");

    // Test case conversion
    EXPECT_EQ(string_utils::toLower("HELLO"), "hello");
    EXPECT_EQ(string_utils::toUpper("hello"), "HELLO");

    // Test split function
    auto parts = string_utils::split("hello,world,test", ',');
    EXPECT_EQ(parts.size(), 3);
    EXPECT_EQ(parts[0], "hello");
    EXPECT_EQ(parts[1], "world");
    EXPECT_EQ(parts[2], "test");
}

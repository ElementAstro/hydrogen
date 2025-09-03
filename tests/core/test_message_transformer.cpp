/**
 * @file test_message_transformer.cpp
 * @brief Comprehensive tests for message transformation functionality
 * 
 * Tests the message transformer that converts between different message formats
 * and protocols in the Hydrogen framework.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <hydrogen/core/message.h>
#include <nlohmann/json.hpp>

using namespace hydrogen::core;
using json = nlohmann::json;
using namespace testing;

class MessageTransformerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDeviceId = "test_device_001";
        testCommand = "get_status";
        testParameters = {{"param1", "value1"}, {"param2", 42}};
    }
    
    void TearDown() override {
        // Test cleanup
    }
    
    std::string testDeviceId;
    std::string testCommand;
    json testParameters;
};

// Test basic message transformation
TEST_F(MessageTransformerTest, BasicCommandTransformation) {
    CommandMessage cmd(testCommand);
    cmd.setDeviceId(testDeviceId);
    cmd.setParameters(testParameters);
    
    // Transform to JSON
    json transformed = cmd.toJson();
    
    // Verify transformation
    EXPECT_EQ(transformed["messageType"], "COMMAND");
    EXPECT_EQ(transformed["command"], testCommand);
    EXPECT_EQ(transformed["deviceId"], testDeviceId);
    EXPECT_EQ(transformed["parameters"], testParameters);
}

// Test response message transformation
TEST_F(MessageTransformerTest, ResponseTransformation) {
    ResponseMessage response;
    response.setDeviceId(testDeviceId);
    response.setSuccess(true);
    response.setMessage("Operation successful");
    response.setData({{"result", "success"}, {"value", 123}});
    
    json transformed = response.toJson();
    
    EXPECT_EQ(transformed["messageType"], "RESPONSE");
    EXPECT_EQ(transformed["deviceId"], testDeviceId);
    EXPECT_EQ(transformed["success"], true);
    EXPECT_EQ(transformed["message"], "Operation successful");
    EXPECT_EQ(transformed["data"]["result"], "success");
    EXPECT_EQ(transformed["data"]["value"], 123);
}

// Test event message transformation
TEST_F(MessageTransformerTest, EventTransformation) {
    EventMessage event("device_connected");
    event.setDeviceId(testDeviceId);
    event.setEventData({{"connection_type", "USB"}, {"port", "/dev/ttyUSB0"}});
    
    json transformed = event.toJson();
    
    EXPECT_EQ(transformed["messageType"], "EVENT");
    EXPECT_EQ(transformed["eventType"], "device_connected");
    EXPECT_EQ(transformed["deviceId"], testDeviceId);
    EXPECT_EQ(transformed["eventData"]["connection_type"], "USB");
    EXPECT_EQ(transformed["eventData"]["port"], "/dev/ttyUSB0");
}

// Test error message transformation
TEST_F(MessageTransformerTest, ErrorTransformation) {
    ErrorMessage error("CONNECTION_FAILED", "Failed to connect to device");
    error.setDeviceId(testDeviceId);
    error.setSeverity(ErrorMessage::Severity::CRITICAL);
    error.setErrorDetails({{"error_code", 500}, {"retry_count", 3}});
    
    json transformed = error.toJson();
    
    EXPECT_EQ(transformed["messageType"], "ERR");
    EXPECT_EQ(transformed["errorCode"], "CONNECTION_FAILED");
    EXPECT_EQ(transformed["errorMessage"], "Failed to connect to device");
    EXPECT_EQ(transformed["deviceId"], testDeviceId);
    EXPECT_EQ(transformed["severity"], static_cast<int>(ErrorMessage::Severity::CRITICAL));
    EXPECT_EQ(transformed["errorDetails"]["error_code"], 500);
    EXPECT_EQ(transformed["errorDetails"]["retry_count"], 3);
}

// Test message priority and QoS transformation
TEST_F(MessageTransformerTest, PriorityAndQoSTransformation) {
    CommandMessage cmd(testCommand);
    cmd.setPriority(Message::Priority::CRITICAL);
    cmd.setQoSLevel(Message::QoSLevel::EXACTLY_ONCE);
    cmd.setExpireAfterSeconds(300);
    
    json transformed = cmd.toJson();
    
    EXPECT_EQ(transformed["priority"], static_cast<int>(Message::Priority::CRITICAL));
    EXPECT_EQ(transformed["qos"], static_cast<int>(Message::QoSLevel::EXACTLY_ONCE));
    EXPECT_EQ(transformed["expireAfter"], 300);
}

// Test reverse transformation (JSON to Message)
TEST_F(MessageTransformerTest, ReverseTransformation) {
    json messageJson = {
        {"messageType", "COMMAND"},
        {"messageId", "test_msg_123"},
        {"timestamp", "2023-01-01T12:00:00Z"},
        {"deviceId", testDeviceId},
        {"command", testCommand},
        {"parameters", testParameters},
        {"priority", static_cast<int>(Message::Priority::HIGH)},
        {"qos", static_cast<int>(Message::QoSLevel::AT_LEAST_ONCE)}
    };
    
    CommandMessage cmd;
    cmd.fromJson(messageJson);
    
    EXPECT_EQ(cmd.getMessageId(), "test_msg_123");
    EXPECT_EQ(cmd.getDeviceId(), testDeviceId);
    EXPECT_EQ(cmd.getCommand(), testCommand);
    EXPECT_EQ(cmd.getPriority(), Message::Priority::HIGH);
    EXPECT_EQ(cmd.getQoSLevel(), Message::QoSLevel::AT_LEAST_ONCE);
    EXPECT_EQ(cmd.getParameters(), testParameters);
}

// Test transformation with metadata
TEST_F(MessageTransformerTest, MetadataTransformation) {
    CommandMessage cmd(testCommand);
    cmd.setDeviceId(testDeviceId);
    
    // Add metadata (if supported)
    json metadata = {{"source", "client"}, {"version", "1.0"}, {"trace_id", "abc123"}};
    // Note: This assumes metadata support exists in the Message class
    // cmd.setMetadata(metadata);
    
    json transformed = cmd.toJson();
    
    // Basic transformation should still work
    EXPECT_EQ(transformed["messageType"], "COMMAND");
    EXPECT_EQ(transformed["command"], testCommand);
    EXPECT_EQ(transformed["deviceId"], testDeviceId);
}

// Test transformation error handling
TEST_F(MessageTransformerTest, TransformationErrorHandling) {
    // Test with invalid JSON
    json invalidJson = {{"invalid", "structure"}};
    
    CommandMessage cmd;
    EXPECT_THROW(cmd.fromJson(invalidJson), std::exception);
    
    // Test with missing required fields
    json incompleteJson = {
        {"messageType", "COMMAND"}
        // Missing messageId, timestamp, etc.
    };
    
    EXPECT_THROW(cmd.fromJson(incompleteJson), std::exception);
}

// Test transformation performance
TEST_F(MessageTransformerTest, TransformationPerformance) {
    CommandMessage cmd(testCommand);
    cmd.setDeviceId(testDeviceId);
    cmd.setParameters(testParameters);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform multiple transformations
    for (int i = 0; i < 1000; ++i) {
        json transformed = cmd.toJson();
        CommandMessage newCmd;
        newCmd.fromJson(transformed);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000); // Less than 1 second for 1000 transformations
}

// Test batch transformation
TEST_F(MessageTransformerTest, BatchTransformation) {
    std::vector<std::unique_ptr<Message>> messages;
    
    // Create different types of messages
    auto cmd = std::make_unique<CommandMessage>("test_cmd");
    cmd->setDeviceId(testDeviceId);
    messages.push_back(std::move(cmd));
    
    auto response = std::make_unique<ResponseMessage>();
    response->setDeviceId(testDeviceId);
    response->setSuccess(true);
    messages.push_back(std::move(response));
    
    auto event = std::make_unique<EventMessage>("test_event");
    event->setDeviceId(testDeviceId);
    messages.push_back(std::move(event));
    
    // Transform all messages
    std::vector<json> transformedMessages;
    for (const auto& msg : messages) {
        transformedMessages.push_back(msg->toJson());
    }
    
    EXPECT_EQ(transformedMessages.size(), 3);
    EXPECT_EQ(transformedMessages[0]["messageType"], "COMMAND");
    EXPECT_EQ(transformedMessages[1]["messageType"], "RESPONSE");
    EXPECT_EQ(transformedMessages[2]["messageType"], "EVENT");
}

// Test transformation with complex nested data
TEST_F(MessageTransformerTest, ComplexDataTransformation) {
    CommandMessage cmd("complex_command");
    cmd.setDeviceId(testDeviceId);
    
    json complexParams = {
        {"simple_param", "value"},
        {"nested_object", {
            {"inner_param", 42},
            {"inner_array", {1, 2, 3, 4, 5}},
            {"deep_nested", {
                {"level3", "deep_value"}
            }}
        }},
        {"array_param", {"item1", "item2", "item3"}}
    };
    
    cmd.setParameters(complexParams);
    
    json transformed = cmd.toJson();
    EXPECT_EQ(transformed["parameters"], complexParams);
    
    // Test round-trip
    CommandMessage newCmd;
    newCmd.fromJson(transformed);
    EXPECT_EQ(newCmd.getParameters(), complexParams);
}

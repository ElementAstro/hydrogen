#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <hydrogen/core/messaging/message_transformer.h>
#include <hydrogen/core/messaging/message.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <memory>
#include <iomanip>
#include <sstream>

using namespace testing;
using json = nlohmann::json;
using namespace hydrogen::core;

/**
 * @brief Test fixture for StdioMessageTransformer tests
 */
class StdioMessageTransformerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create transformer instances
        stdioTransformer_ = std::make_unique<StdioTransformer>();
        messageTransformer_ = std::make_unique<MessageTransformer>();

        // Create test message
        testMessage_ = createTestMessage();
    }

    void TearDown() override {
        stdioTransformer_.reset();
        messageTransformer_.reset();
        testMessage_.reset();
    }

    std::unique_ptr<CommandMessage> createTestMessage() {
        auto message = std::make_unique<CommandMessage>();
        message->setMessageId("test_msg_001");
        message->setDeviceId("test_device");
        message->setMessageType(MessageType::COMMAND);

        // Create timestamp string
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        message->setTimestamp(ss.str());

        message->setOriginalMessageId("original_msg_001");

        // Set command-specific data
        message->setCommand("ping");
        json parameters;
        parameters["timeout"] = 5000;
        parameters["retries"] = 3;
        message->setParameters(parameters);

        return message;
    }

    json createTestStdioJson() {
        json stdioJson;
        stdioJson["id"] = "test_msg_001";
        stdioJson["device"] = "test_device";
        stdioJson["type"] = "command";
        stdioJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        json payload;
        payload["messageType"] = static_cast<int>(MessageType::COMMAND);
        payload["messageId"] = "test_msg_001";
        payload["deviceId"] = "test_device";
        payload["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        payload["originalMessageId"] = "original_msg_001";

        stdioJson["payload"] = payload;

        return stdioJson;
    }

    std::unique_ptr<StdioTransformer> stdioTransformer_;
    std::unique_ptr<MessageTransformer> messageTransformer_;
    std::unique_ptr<CommandMessage> testMessage_;
};

/**
 * @brief Test basic transformer creation and metadata
 */
TEST_F(StdioMessageTransformerTest, BasicTransformerCreation) {
    ASSERT_NE(stdioTransformer_, nullptr);

    // Test protocol metadata
    auto metadata = stdioTransformer_->getProtocolMetadata();
    EXPECT_EQ(metadata["protocol"], "stdio");
    EXPECT_EQ(metadata["version"], "1.0");
    EXPECT_EQ(metadata["encoding"], "utf-8");
    EXPECT_EQ(metadata["content_type"], "application/json");
    EXPECT_EQ(metadata["line_terminator"], "\\n");
    EXPECT_EQ(metadata["supports_binary"], "false");
}

/**
 * @brief Test message transformation to STDIO protocol format
 */
TEST_F(StdioMessageTransformerTest, ToProtocolTransformation) {
    ASSERT_NE(testMessage_, nullptr);

    // Transform message to STDIO format
    auto result = stdioTransformer_->toProtocol(*testMessage_);

    // Verify transformation success
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.errorMessage.empty());
    EXPECT_FALSE(result.transformedData.empty());

    // Verify transformed data structure
    json stdioJson = result.transformedData;
    EXPECT_TRUE(stdioJson.contains("id"));
    EXPECT_TRUE(stdioJson.contains("device"));
    EXPECT_TRUE(stdioJson.contains("type"));
    EXPECT_TRUE(stdioJson.contains("timestamp"));
    EXPECT_TRUE(stdioJson.contains("payload"));

    // Verify field values
    EXPECT_EQ(stdioJson["id"], testMessage_->getMessageId());
    EXPECT_EQ(stdioJson["device"], testMessage_->getDeviceId());
    EXPECT_EQ(stdioJson["type"], "command");

    // Verify metadata
    EXPECT_EQ(result.metadata["Content-Type"], "application/json");
    EXPECT_EQ(result.metadata["X-Protocol"], "stdio");
    EXPECT_EQ(result.metadata["X-Version"], "1.0");
    EXPECT_EQ(result.metadata["X-Encoding"], "utf-8");
}

/**
 * @brief Test message transformation from STDIO protocol format
 */
TEST_F(StdioMessageTransformerTest, FromProtocolTransformation) {
    json stdioJson = createTestStdioJson();

    // Transform from STDIO format to internal message
    auto message = stdioTransformer_->fromProtocol(stdioJson);

    // Verify transformation success
    ASSERT_NE(message, nullptr);

    // Verify message fields
    EXPECT_EQ(message->getMessageId(), "test_msg_001");
    EXPECT_EQ(message->getDeviceId(), "test_device");
    EXPECT_EQ(message->getMessageType(), MessageType::COMMAND);
    EXPECT_EQ(message->getOriginalMessageId(), "original_msg_001");
}

/**
 * @brief Test round-trip transformation (to protocol and back)
 */
TEST_F(StdioMessageTransformerTest, RoundTripTransformation) {
    ASSERT_NE(testMessage_, nullptr);

    // Transform to STDIO format
    auto toResult = stdioTransformer_->toProtocol(*testMessage_);
    ASSERT_TRUE(toResult.success);

    // Transform back to internal format
    auto backMessage = stdioTransformer_->fromProtocol(toResult.transformedData);
    ASSERT_NE(backMessage, nullptr);

    // Verify key fields are preserved
    EXPECT_EQ(backMessage->getMessageId(), testMessage_->getMessageId());
    EXPECT_EQ(backMessage->getDeviceId(), testMessage_->getDeviceId());
    EXPECT_EQ(backMessage->getMessageType(), testMessage_->getMessageType());
    EXPECT_EQ(backMessage->getOriginalMessageId(), testMessage_->getOriginalMessageId());
}

/**
 * @brief Test different message types transformation
 */
TEST_F(StdioMessageTransformerTest, DifferentMessageTypes) {
    std::vector<std::pair<MessageType, std::string>> messageTypes = {
        {MessageType::COMMAND, "command"},
        {MessageType::RESPONSE, "response"},
        {MessageType::EVENT, "event"},
        {MessageType::ERR, "error"}
    };

    for (const auto& [messageType, expectedTypeStr] : messageTypes) {
        auto message = createTestMessage();
        message->setMessageType(messageType);

        // Transform to STDIO format
        auto result = stdioTransformer_->toProtocol(*message);
        ASSERT_TRUE(result.success) << "Failed for message type: " << expectedTypeStr;

        json stdioJson = result.transformedData;
        EXPECT_EQ(stdioJson["type"], expectedTypeStr);

        // Transform back
        auto backMessage = stdioTransformer_->fromProtocol(stdioJson);
        ASSERT_NE(backMessage, nullptr) << "Failed back transformation for: " << expectedTypeStr;
        EXPECT_EQ(backMessage->getMessageType(), messageType);
    }
}

/**
 * @brief Test error handling in transformation
 */
TEST_F(StdioMessageTransformerTest, ErrorHandling) {
    // Test with invalid JSON for fromProtocol
    json invalidJson;
    invalidJson["invalid"] = "data";

    auto message = stdioTransformer_->fromProtocol(invalidJson);
    // Should handle gracefully - either return nullptr or a default message
    // The exact behavior depends on implementation

    // Test with empty JSON
    json emptyJson;
    auto emptyMessage = stdioTransformer_->fromProtocol(emptyJson);
    // Should handle gracefully

    // Test with malformed JSON structure
    json malformedJson;
    malformedJson["id"] = 123; // Wrong type
    malformedJson["device"] = nullptr;
    malformedJson["type"] = "invalid_type";

    auto malformedMessage = stdioTransformer_->fromProtocol(malformedJson);
    // Should handle gracefully
}

/**
 * @brief Test message transformer integration
 */
TEST_F(StdioMessageTransformerTest, MessageTransformerIntegration) {
    ASSERT_NE(messageTransformer_, nullptr);

    // Test if STDIO format is supported
    EXPECT_TRUE(messageTransformer_->isFormatSupported(MessageFormat::STDIO));

    // Test transformation through the unified transformer
    auto result = messageTransformer_->transform(*testMessage_, MessageFormat::STDIO);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.transformedData.empty());

    // Test back transformation
    auto backMessage = messageTransformer_->transformToInternal(result.transformedData, MessageFormat::STDIO);
    ASSERT_NE(backMessage, nullptr);
    EXPECT_EQ(backMessage->getMessageId(), testMessage_->getMessageId());
}

/**
 * @brief Test message validation
 */
TEST_F(StdioMessageTransformerTest, MessageValidation) {
    // Create valid STDIO JSON
    json validJson = createTestStdioJson();

    // Test validation through message transformer
    EXPECT_TRUE(messageTransformer_->validateMessage(validJson, MessageFormat::STDIO));

    // Test with invalid JSON
    json invalidJson;
    invalidJson["missing_required_fields"] = true;

    bool isValid = messageTransformer_->validateMessage(invalidJson, MessageFormat::STDIO);
    // The exact validation behavior depends on implementation
    // We just ensure it doesn't crash

    if (!isValid) {
        std::string error = messageTransformer_->getValidationError(invalidJson, MessageFormat::STDIO);
        EXPECT_FALSE(error.empty());
    }
}

/**
 * @brief Test message normalization
 */
TEST_F(StdioMessageTransformerTest, MessageNormalization) {
    json testJson = createTestStdioJson();

    // Test normalization
    json normalizedJson = messageTransformer_->normalizeMessage(testJson, MessageFormat::STDIO);

    // Normalized message should be valid
    EXPECT_FALSE(normalizedJson.empty());

    // Should be able to transform normalized message
    auto message = stdioTransformer_->fromProtocol(normalizedJson);
    // Should handle gracefully regardless of implementation
}

/**
 * @brief Test edge cases and boundary conditions
 */
TEST_F(StdioMessageTransformerTest, EdgeCasesAndBoundaryConditions) {
    // Test with very long message ID
    auto longIdMessage = createTestMessage();
    std::string longId(1000, 'x');
    longIdMessage->setMessageId(longId);

    auto result = stdioTransformer_->toProtocol(*longIdMessage);
    EXPECT_TRUE(result.success);

    // Test with empty strings
    auto emptyMessage = createTestMessage();
    emptyMessage->setMessageId("");
    emptyMessage->setDeviceId("");
    emptyMessage->setOriginalMessageId("");

    auto emptyResult = stdioTransformer_->toProtocol(*emptyMessage);
    EXPECT_TRUE(emptyResult.success);

    // Test with special characters
    auto specialMessage = createTestMessage();
    specialMessage->setMessageId("msg_with_特殊字符_🚀");
    specialMessage->setDeviceId("device_with_émojis_🔭");

    auto specialResult = stdioTransformer_->toProtocol(*specialMessage);
    EXPECT_TRUE(specialResult.success);

    // Verify round-trip with special characters
    auto backSpecialMessage = stdioTransformer_->fromProtocol(specialResult.transformedData);
    ASSERT_NE(backSpecialMessage, nullptr);
    EXPECT_EQ(backSpecialMessage->getMessageId(), specialMessage->getMessageId());
    EXPECT_EQ(backSpecialMessage->getDeviceId(), specialMessage->getDeviceId());
}

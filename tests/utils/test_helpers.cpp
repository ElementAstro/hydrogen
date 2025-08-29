#include "test_helpers.h"
#include "../../src/common/utils.h"
#include <filesystem>
#include <random>
#include <thread>
#include <chrono>

namespace astrocomm {
namespace test {

std::string TestHelpers::generateTestDeviceId() {
    static int counter = 0;
    return "test_device_" + std::to_string(++counter);
}

std::string TestHelpers::generateTestMessageId() {
    return astrocomm::generateUuid();
}

std::string TestHelpers::createTestTimestamp() {
    return astrocomm::getIsoTimestamp();
}

bool TestHelpers::compareJsonWithTolerance(const nlohmann::json& expected, const nlohmann::json& actual) {
    // Create copies to modify
    auto exp = expected;
    auto act = actual;
    
    // Remove timestamp fields for comparison as they may vary slightly
    if (exp.contains("timestamp")) exp.erase("timestamp");
    if (act.contains("timestamp")) act.erase("timestamp");
    
    return exp == act;
}

nlohmann::json TestHelpers::createTestConfig() {
    return nlohmann::json{
        {"manufacturer", "Test Manufacturer"},
        {"model", "Test Model v1.0"},
        {"version", "1.0.0"},
        {"capabilities", nlohmann::json::array({"basic", "test"})},
        {"properties", nlohmann::json::object()}
    };
}

bool TestHelpers::validateMessageStructure(const astrocomm::Message& message) {
    // Check required fields
    if (message.getMessageId().empty()) return false;
    if (message.getTimestamp().empty()) return false;

    // Validate JSON serialization
    try {
        auto json = message.toJson();
        if (!json.contains("messageType")) return false;
        if (!json.contains("messageId")) return false;
        if (!json.contains("timestamp")) return false;
    } catch (...) {
        return false;
    }

    return true;
}

std::string TestHelpers::createTempDirectory() {
    auto temp_path = std::filesystem::temp_directory_path();
    auto unique_name = "astrocomm_test_" + generateTestMessageId();
    auto test_dir = temp_path / unique_name;
    
    std::filesystem::create_directories(test_dir);
    return test_dir.string();
}

void TestHelpers::cleanupTempDirectory(const std::string& path) {
    if (!path.empty() && std::filesystem::exists(path)) {
        std::filesystem::remove_all(path);
    }
}

// AstroCommTestBase implementation
void AstroCommTestBase::SetUp() {
    testDeviceId = TestHelpers::generateTestDeviceId();
    tempDir = TestHelpers::createTempDirectory();
    
    // Initialize for testing (no specific initialization needed)
}

void AstroCommTestBase::TearDown() {
    TestHelpers::cleanupTempDirectory(tempDir);
}

// MessageTestBase implementation
void MessageTestBase::SetUp() {
    AstroCommTestBase::SetUp();
}

void MessageTestBase::TearDown() {
    AstroCommTestBase::TearDown();
}

std::unique_ptr<astrocomm::CommandMessage> MessageTestBase::createTestCommand() {
    auto cmd = std::make_unique<astrocomm::CommandMessage>("test_command");
    cmd->setDeviceId(testDeviceId);
    cmd->setPriority(astrocomm::Message::Priority::NORMAL);
    return cmd;
}

std::unique_ptr<astrocomm::ResponseMessage> MessageTestBase::createTestResponse() {
    auto resp = std::make_unique<astrocomm::ResponseMessage>();
    resp->setDeviceId(testDeviceId);
    resp->setStatus("success");
    resp->setDetails({{"message", "Test response"}});
    return resp;
}

std::unique_ptr<astrocomm::EventMessage> MessageTestBase::createTestEvent() {
    auto event = std::make_unique<astrocomm::EventMessage>("test_event");
    event->setDeviceId(testDeviceId);
    event->setDetails({{"key", "value"}});
    return event;
}

std::unique_ptr<astrocomm::ErrorMessage> MessageTestBase::createTestError() {
    auto error = std::make_unique<astrocomm::ErrorMessage>("TEST_ERROR", "Test error message");
    error->setDeviceId(testDeviceId);
    return error;
}

// DeviceTestBase implementation
void DeviceTestBase::SetUp() {
    AstroCommTestBase::SetUp();
    deviceType = "test_device";
    manufacturer = "Test Manufacturer";
    model = "Test Model v1.0";
}

void DeviceTestBase::TearDown() {
    AstroCommTestBase::TearDown();
}

} // namespace test
} // namespace astrocomm

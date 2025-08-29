#pragma once

#include "../../src/common/message.h"
#include <memory>
#include <string>
#include <thread>
#include <chrono>

namespace astrocomm {
namespace test {

/**
 * @brief Factory for creating test messages
 */
class TestMessageFactory {
public:
    /**
     * @brief Create a test command message
     */
    static std::unique_ptr<astrocomm::CommandMessage> createCommand(
        const std::string& command = "test_command",
        const std::string& deviceId = "test_device",
        const astrocomm::json& parameters = astrocomm::json::object());

    /**
     * @brief Create a test response message
     */
    static std::unique_ptr<astrocomm::ResponseMessage> createResponse(
        bool success = true,
        const std::string& message = "Test response",
        const std::string& deviceId = "test_device");

    /**
     * @brief Create a test event message
     */
    static std::unique_ptr<astrocomm::EventMessage> createEvent(
        const std::string& eventType = "test_event",
        const std::string& deviceId = "test_device",
        const astrocomm::json& eventData = astrocomm::json::object());

    /**
     * @brief Create a test error message
     */
    static std::unique_ptr<astrocomm::ErrorMessage> createError(
        const std::string& errorCode = "TEST_ERROR",
        const std::string& errorMessage = "Test error message",
        const std::string& deviceId = "test_device");
    
    /**
     * @brief Create a discovery request message
     */
    static std::unique_ptr<astrocomm::DiscoveryRequestMessage> createDiscoveryRequest(
        const std::vector<std::string>& deviceTypes = {"telescope", "camera"});

    /**
     * @brief Create a discovery response message
     */
    static std::unique_ptr<astrocomm::DiscoveryResponseMessage> createDiscoveryResponse(
        const std::vector<astrocomm::json>& devices = {});

    /**
     * @brief Create a registration message
     */
    static std::unique_ptr<astrocomm::RegistrationMessage> createRegistration(
        const std::string& deviceId = "test_device",
        const std::string& deviceType = "telescope");

    /**
     * @brief Create an authentication message
     */
    static std::unique_ptr<astrocomm::AuthenticationMessage> createAuthentication(
        const std::string& username = "test_user",
        const std::string& password = "test_password");
    
    /**
     * @brief Create a message with specific priority
     */
    template<typename T>
    static std::unique_ptr<T> createWithPriority(
        astrocomm::Message::Priority priority) {
        auto msg = std::make_unique<T>();
        msg->setPriority(priority);
        return msg;
    }

    /**
     * @brief Create a message with specific QoS level
     */
    template<typename T>
    static std::unique_ptr<T> createWithQoS(
        astrocomm::Message::QoSLevel qos) {
        auto msg = std::make_unique<T>();
        msg->setQoSLevel(qos);
        return msg;
    }
    
    /**
     * @brief Create an expired message
     */
    template<typename T>
    static std::unique_ptr<T> createExpired() {
        auto msg = std::make_unique<T>();
        msg->setExpireAfter(1); // 1 second
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        return msg;
    }
};

} // namespace test
} // namespace astrocomm

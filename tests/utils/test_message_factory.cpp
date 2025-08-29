#include "test_message_factory.h"
#include "../../src/common/utils.h"

namespace astrocomm {
namespace test {

std::unique_ptr<astrocomm::CommandMessage> TestMessageFactory::createCommand(
    const std::string& command,
    const std::string& deviceId,
    const astrocomm::json& parameters) {

    auto cmd = std::make_unique<astrocomm::CommandMessage>(command);
    cmd->setDeviceId(deviceId);
    cmd->setParameters(parameters);
    cmd->setPriority(astrocomm::Message::Priority::NORMAL);
    cmd->setQoSLevel(astrocomm::Message::QoSLevel::AT_MOST_ONCE);
    return cmd;
}

std::unique_ptr<astrocomm::ResponseMessage> TestMessageFactory::createResponse(
    bool success,
    const std::string& message,
    const std::string& deviceId) {

    auto resp = std::make_unique<astrocomm::ResponseMessage>();
    resp->setDeviceId(deviceId);
    resp->setStatus(success ? "success" : "error");
    resp->setDetails({{"message", message}});

    if (!success) {
        resp->setDetails({{"message", message}, {"errorCode", "TEST_ERROR"}});
    }

    return resp;
}

std::unique_ptr<astrocomm::core::EventMessage> TestMessageFactory::createEvent(
    const std::string& eventType,
    const std::string& deviceId,
    const nlohmann::json& eventData) {
    
    auto event = std::make_unique<astrocomm::core::EventMessage>(eventType);
    event->setDeviceId(deviceId);
    event->setEventData(eventData);
    return event;
}

std::unique_ptr<astrocomm::core::ErrorMessage> TestMessageFactory::createError(
    const std::string& errorCode,
    const std::string& errorMessage,
    const std::string& deviceId) {
    
    auto error = std::make_unique<astrocomm::core::ErrorMessage>(errorCode, errorMessage);
    error->setDeviceId(deviceId);
    error->setSeverity(astrocomm::core::ErrorMessage::Severity::ERROR);
    return error;
}

std::unique_ptr<astrocomm::core::DiscoveryRequestMessage> TestMessageFactory::createDiscoveryRequest(
    const std::vector<std::string>& deviceTypes) {
    
    auto discovery = std::make_unique<astrocomm::core::DiscoveryRequestMessage>();
    discovery->setDeviceTypes(deviceTypes);
    return discovery;
}

std::unique_ptr<astrocomm::core::DiscoveryResponseMessage> TestMessageFactory::createDiscoveryResponse(
    const std::vector<nlohmann::json>& devices) {
    
    auto response = std::make_unique<astrocomm::core::DiscoveryResponseMessage>();
    response->setDevices(devices);
    return response;
}

std::unique_ptr<astrocomm::core::RegistrationMessage> TestMessageFactory::createRegistration(
    const std::string& deviceId,
    const std::string& deviceType) {
    
    auto registration = std::make_unique<astrocomm::core::RegistrationMessage>();
    registration->setDeviceId(deviceId);
    registration->setDeviceType(deviceType);
    
    nlohmann::json deviceInfo = {
        {"manufacturer", "Test Manufacturer"},
        {"model", "Test Model v1.0"},
        {"version", "1.0.0"},
        {"capabilities", nlohmann::json::array({"basic", "test"})}
    };
    registration->setDeviceInfo(deviceInfo);
    
    return registration;
}

std::unique_ptr<astrocomm::core::AuthenticationMessage> TestMessageFactory::createAuthentication(
    const std::string& username,
    const std::string& password) {
    
    auto auth = std::make_unique<astrocomm::core::AuthenticationMessage>();
    auth->setUsername(username);
    auth->setPassword(password);
    auth->setAuthType("basic");
    return auth;
}

} // namespace test
} // namespace astrocomm

#include <hydrogen/client/device_client.h>
#include "client/connection_manager.h"
#include "client/message_processor.h"
#include "client/device_manager.h"
#include "client/command_executor.h"
#include "client/subscription_manager.h"
#include "common/message.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <chrono>
#include <thread>

namespace hydrogen {
namespace client {

DeviceClient::DeviceClient() {
    initializeComponents();
    setupMessageHandlers();
    spdlog::info("Optimized DeviceClient initialized");
}

DeviceClient::~DeviceClient() {
    cleanupComponents();
    spdlog::info("Optimized DeviceClient destroyed");
}

// Connection Management
bool DeviceClient::connect(const std::string& host, uint16_t port) {
    return connectionManager->connect(host, port);
}

void DeviceClient::disconnect() {
    connectionManager->disconnect();
}

bool DeviceClient::isConnected() const {
    return connectionManager->isConnected();
}

// Device Discovery and Management
json DeviceClient::discoverDevices(const std::vector<std::string>& deviceTypes) {
    return deviceManager->discoverDevices(deviceTypes);
}

json DeviceClient::getDevices() const {
    return deviceManager->getDevices();
}

json DeviceClient::getDeviceProperties(const std::string& deviceId,
                                     const std::vector<std::string>& properties) {
    return deviceManager->getDeviceProperties(deviceId, properties);
}

json DeviceClient::setDeviceProperties(const std::string& deviceId, const json& properties) {
    return deviceManager->setDeviceProperties(deviceId, properties);
}

// Command Execution
json DeviceClient::executeCommand(const std::string& deviceId, const std::string& command,
                                const json& parameters) {
    return commandExecutor->executeCommand(deviceId, command, parameters);
}

// Message Operations
json DeviceClient::sendMessage(std::shared_ptr<hydrogen::Message> message, int timeoutMs) {
    return messageProcessor->sendAndWaitForResponse(*message, timeoutMs / 1000);
}

void DeviceClient::sendMessageAsync(std::shared_ptr<hydrogen::Message> message,
                                   std::function<void(const json&)> callback) {
    // For now, implement async as sync call in a separate thread
    // TODO: Implement proper async messaging in MessageProcessor
    std::thread([this, message, callback]() {
        try {
            json response = messageProcessor->sendAndWaitForResponse(*message, 10);
            callback(response);
        } catch (const std::exception& e) {
            callback(json{{"error", e.what()}});
        }
    }).detach();
}

// Subscription Management
void DeviceClient::subscribeToProperty(const std::string& deviceId, const std::string& property,
                                      PropertyCallback callback) {
    // Adapt the callback signature from DeviceClient (2 params) to SubscriptionManager (3 params)
    auto adaptedCallback = [callback](const std::string& deviceId, const std::string& property, const json& value) {
        callback(deviceId, value);
    };
    subscriptionManager->subscribeToProperty(deviceId, property, adaptedCallback);
}

void DeviceClient::unsubscribeFromProperty(const std::string& deviceId, const std::string& property) {
    subscriptionManager->unsubscribeFromProperty(deviceId, property);
}

void DeviceClient::subscribeToEvent(const std::string& deviceId, const std::string& eventType,
                                   EventCallback callback) {
    // Adapt the callback signature from DeviceClient (2 params) to SubscriptionManager (3 params)
    auto adaptedCallback = [callback](const std::string& deviceId, const std::string& event, const json& details) {
        callback(deviceId, details);
    };
    subscriptionManager->subscribeToEvent(deviceId, eventType, adaptedCallback);
}

void DeviceClient::unsubscribeFromEvent(const std::string& deviceId, const std::string& eventType) {
    subscriptionManager->unsubscribeFromEvent(deviceId, eventType);
}

// Connection Configuration
void DeviceClient::setConnectionCallback(std::function<void(bool)> callback) {
    connectionManager->setConnectionCallback(callback);
}

void DeviceClient::setAuthToken(const std::string& token) {
    // Store auth token in message processor for use in message headers
    // This would need to be implemented in MessageProcessor
    spdlog::debug("Auth token set: {}", token.substr(0, 8) + "...");
}

std::string DeviceClient::getAuthToken() const {
    // This would need to be implemented to retrieve from MessageProcessor
    return "";
}

void DeviceClient::setAutoReconnect(bool enabled, int intervalMs) {
    connectionManager->setAutoReconnect(enabled, intervalMs);
}

// Private Methods
void DeviceClient::initializeComponents() {
    // Create components in dependency order
    connectionManager = std::make_unique<hydrogen::ConnectionManager>();
    messageProcessor = std::make_unique<hydrogen::MessageProcessor>(connectionManager.get());
    deviceManager = std::make_unique<hydrogen::DeviceManager>(messageProcessor.get());
    commandExecutor = std::make_unique<hydrogen::CommandExecutor>(messageProcessor.get());
    subscriptionManager = std::make_unique<hydrogen::SubscriptionManager>(messageProcessor.get());
}

void DeviceClient::setupMessageHandlers() {
    // Set up connection state callback
    connectionManager->setConnectionCallback([this](bool connected) {
        if (connected) {
            messageProcessor->startMessageLoop();
        } else {
            messageProcessor->stopMessageLoop();
        }
    });
}

void DeviceClient::cleanupComponents() {
    if (messageProcessor) {
        messageProcessor->stopMessageLoop();
    }
    if (connectionManager) {
        connectionManager->disconnect();
    }
}

} // namespace client
} // namespace hydrogen

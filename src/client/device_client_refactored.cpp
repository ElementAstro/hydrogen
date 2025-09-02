#include "client/device_client_refactored.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>
#include <chrono>

namespace hydrogen {

DeviceClientRefactored::DeviceClientRefactored() {
  initializeComponents();
  setupMessageHandlers();
  spdlog::info("Refactored DeviceClient initialized");
}

DeviceClientRefactored::~DeviceClientRefactored() {
  cleanupComponents();
  spdlog::info("Refactored DeviceClient destroyed");
}

// Connection Management
bool DeviceClientRefactored::connect(const std::string& host, uint16_t port) {
  return connectionManager->connect(host, port);
}

void DeviceClientRefactored::disconnect() {
  connectionManager->disconnect();
}

bool DeviceClientRefactored::isConnected() const {
  return connectionManager->isConnected();
}

void DeviceClientRefactored::setAutoReconnect(bool enable, int intervalMs, int maxAttempts) {
  connectionManager->setAutoReconnect(enable, intervalMs, maxAttempts);
}

// Device Management
json DeviceClientRefactored::discoverDevices(const std::vector<std::string>& deviceTypes) {
  return deviceManager->discoverDevices(deviceTypes);
}

json DeviceClientRefactored::getDevices() const {
  return deviceManager->getDevices();
}

json DeviceClientRefactored::getDeviceProperties(const std::string& deviceId,
                                                const std::vector<std::string>& properties) {
  return deviceManager->getDeviceProperties(deviceId, properties);
}

json DeviceClientRefactored::setDeviceProperties(const std::string& deviceId, const json& properties) {
  return deviceManager->setDeviceProperties(deviceId, properties);
}

// Command Execution
json DeviceClientRefactored::executeCommand(const std::string& deviceId,
                                           const std::string& command,
                                           const json& parameters,
                                           Message::QoSLevel qosLevel) {
  return commandExecutor->executeCommand(deviceId, command, parameters, qosLevel);
}

void DeviceClientRefactored::executeCommandAsync(const std::string& deviceId,
                                                const std::string& command,
                                                const json& parameters,
                                                Message::QoSLevel qosLevel,
                                                std::function<void(const json&)> callback) {
  commandExecutor->executeCommandAsync(deviceId, command, parameters, qosLevel, callback);
}

json DeviceClientRefactored::executeBatchCommands(
    const std::string& deviceId,
    const std::vector<std::pair<std::string, json>>& commands,
    bool sequential,
    Message::QoSLevel qosLevel) {
  return commandExecutor->executeBatchCommands(deviceId, commands, sequential, qosLevel);
}

// Subscription Management
void DeviceClientRefactored::subscribeToProperty(const std::string& deviceId,
                                                const std::string& property,
                                                PropertyCallback callback) {
  subscriptionManager->subscribeToProperty(deviceId, property, callback);
}

void DeviceClientRefactored::unsubscribeFromProperty(const std::string& deviceId,
                                                    const std::string& property) {
  subscriptionManager->unsubscribeFromProperty(deviceId, property);
}

void DeviceClientRefactored::subscribeToEvent(const std::string& deviceId,
                                             const std::string& event,
                                             EventCallback callback) {
  subscriptionManager->subscribeToEvent(deviceId, event, callback);
}

void DeviceClientRefactored::unsubscribeFromEvent(const std::string& deviceId,
                                                 const std::string& event) {
  subscriptionManager->unsubscribeFromEvent(deviceId, event);
}

// Authentication
bool DeviceClientRefactored::authenticate(const std::string& method, const std::string& credentials) {
  if (!connectionManager->isConnected()) {
    throw std::runtime_error("Not connected to server");
  }

  // Create authentication message
  AuthenticationMessage msg;
  msg.setMethod(method);
  msg.setCredentials(credentials);

  // Send request and wait for response
  json response = messageProcessor->sendAndWaitForResponse(msg);

  // Check authentication result
  if (response.contains("payload") && response["payload"].contains("status")) {
    bool success = response["payload"]["status"] == "SUCCESS";
    spdlog::info("Authentication {} using method {}", success ? "successful" : "failed", method);
    return success;
  }

  spdlog::error("Authentication response missing status field");
  return false;
}

// Event Publishing
void DeviceClientRefactored::publishEvent(const std::string& eventName,
                                         const json& details,
                                         Message::Priority priority) {
  if (!connectionManager->isConnected()) {
    spdlog::error("Cannot publish event '{}': Not connected to server", eventName);
    return;
  }

  EventMessage event(eventName);
  event.setPriority(priority);

  if (!details.is_null()) {
    event.setDetails(details);
  }

  if (!messageProcessor->sendMessage(event)) {
    spdlog::error("Failed to publish event '{}'", eventName);
  } else {
    spdlog::debug("Published event '{}'", eventName);
  }
}

// Message Processing Control
void DeviceClientRefactored::run() {
  if (!connectionManager->isConnected()) {
    throw std::runtime_error("Not connected to server");
  }

  spdlog::info("DeviceClientRefactored run() called. Blocking until disconnected or stopped.");

  // Start message processing if not already started
  startMessageProcessing();

  // Wait for the message processing to finish
  while (messageProcessor->isRunning() && connectionManager->isConnected()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  spdlog::info("DeviceClientRefactored run() finished.");
}

void DeviceClientRefactored::startMessageProcessing() {
  messageProcessor->startMessageLoop();
}

void DeviceClientRefactored::stopMessageProcessing() {
  messageProcessor->stopMessageLoop();
}

// Configuration
void DeviceClientRefactored::setMessageRetryParams(int maxRetries, int retryIntervalMs) {
  commandExecutor->setMessageRetryParams(maxRetries, retryIntervalMs);
}

// Status and Statistics
json DeviceClientRefactored::getStatusInfo() const {
  json status;
  
  // Connection status
  status["connection"] = getConnectionStatus();
  
  // Component statistics
  status["devices"] = getDeviceStats();
  status["execution"] = getExecutionStats();
  status["subscriptions"] = getSubscriptionStats();
  status["processing"] = getProcessingStats();
  
  return status;
}

json DeviceClientRefactored::getConnectionStatus() const {
  return connectionManager->getConnectionStatus();
}

json DeviceClientRefactored::getDeviceStats() const {
  return deviceManager->getDeviceStats();
}

json DeviceClientRefactored::getExecutionStats() const {
  return commandExecutor->getExecutionStats();
}

json DeviceClientRefactored::getSubscriptionStats() const {
  return subscriptionManager->getSubscriptionStats();
}

json DeviceClientRefactored::getProcessingStats() const {
  return messageProcessor->getProcessingStats();
}

// Private Methods
void DeviceClientRefactored::initializeComponents() {
  // Create components in dependency order
  connectionManager = std::make_unique<ConnectionManager>();
  messageProcessor = std::make_unique<MessageProcessor>(connectionManager.get());
  deviceManager = std::make_unique<DeviceManager>(messageProcessor.get());
  commandExecutor = std::make_unique<CommandExecutor>(messageProcessor.get());
  subscriptionManager = std::make_unique<SubscriptionManager>(messageProcessor.get());

  // Set up connection state callback
  connectionManager->setConnectionCallback(
      [this](bool connected) { onConnectionStateChanged(connected); });

  spdlog::debug("All components initialized");
}

void DeviceClientRefactored::setupMessageHandlers() {
  // Register message handlers with the message processor
  messageProcessor->registerMessageHandler(MessageType::EVENT,
      [this](const Message& msg) { handleEventMessage(msg); });

  messageProcessor->registerMessageHandler(MessageType::ERR,
      [this](const Message& msg) { handleErrorMessage(msg); });

  spdlog::debug("Message handlers registered");
}

void DeviceClientRefactored::cleanupComponents() {
  // Stop message processing first
  if (messageProcessor) {
    messageProcessor->stopMessageLoop();
  }

  // Clear subscriptions
  if (subscriptionManager) {
    subscriptionManager->clearAllSubscriptions();
  }

  // Clear pending commands
  if (commandExecutor) {
    commandExecutor->clearPendingCommands();
  }

  // Disconnect
  if (connectionManager) {
    connectionManager->disconnect();
  }

  spdlog::debug("All components cleaned up");
}

void DeviceClientRefactored::handleEventMessage(const Message& message) {
  const EventMessage& eventMsg = static_cast<const EventMessage&>(message);
  
  // Delegate to subscription manager
  subscriptionManager->handleEvent(eventMsg);
}

void DeviceClientRefactored::handleErrorMessage(const Message& message) {
  const ErrorMessage& errorMsg = static_cast<const ErrorMessage&>(message);
  
  spdlog::error("Received error message. Original ID: '{}', Code: {}, Message: {}",
                errorMsg.getOriginalMessageId().empty() ? "N/A" : errorMsg.getOriginalMessageId(),
                errorMsg.getErrorCode(), errorMsg.getErrorMessage());
}

void DeviceClientRefactored::onConnectionStateChanged(bool connected) {
  if (connected) {
    spdlog::info("Connection established - starting message processing");
    startMessageProcessing();
  } else {
    spdlog::info("Connection lost - stopping message processing");
    stopMessageProcessing();
    
    // Clear device cache on disconnect
    deviceManager->clearDeviceCache();
  }
}

} // namespace hydrogen

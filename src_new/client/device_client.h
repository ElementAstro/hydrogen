#pragma once

#include "client/connection_manager.h"
#include "client/message_processor.h"
#include "client/device_manager.h"
#include "client/command_executor.h"
#include "client/subscription_manager.h"
#include "common/message.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace hydrogen {

using json = nlohmann::json;

/**
 * @brief DeviceClient class using component-based architecture
 *
 * This class serves as a facade that coordinates specialized components:
 * - ConnectionManager: Handles WebSocket connections and reconnection
 * - MessageProcessor: Handles message sending/receiving and processing loop
 * - DeviceManager: Handles device discovery and property management
 * - CommandExecutor: Handles command execution (sync/async) and batch operations
 * - SubscriptionManager: Handles property and event subscriptions
 *
 * This implementation provides a clean, maintainable architecture with
 * better separation of concerns compared to monolithic approaches.
 */
class DeviceClient {
public:
    DeviceClient();
    ~DeviceClient();

    // Connection Management (delegated to ConnectionManager)
    bool connect(const std::string& host, uint16_t port);
    void disconnect();
    bool isConnected() const;
    void setAutoReconnect(bool enable, int intervalMs = 5000, int maxAttempts = 0);

    // Device Management (delegated to DeviceManager)
    json discoverDevices(const std::vector<std::string>& deviceTypes = {});
    json getDevices() const;
    json getDeviceProperties(const std::string& deviceId,
                            const std::vector<std::string>& properties);
    json setDeviceProperties(const std::string& deviceId, const json& properties);

    // Command Execution (delegated to CommandExecutor)
    json executeCommand(const std::string& deviceId, const std::string& command,
                       const json& parameters = json::object(),
                       Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE);

    void executeCommandAsync(const std::string& deviceId, const std::string& command,
                            const json& parameters = json::object(),
                            Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE,
                            std::function<void(const json&)> callback = nullptr);

    json executeBatchCommands(const std::string& deviceId,
                             const std::vector<std::pair<std::string, json>>& commands,
                             bool sequential = true,
                             Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE);

    // Subscription Management (delegated to SubscriptionManager)
    using PropertyCallback = SubscriptionManager::PropertyCallback;
    using EventCallback = SubscriptionManager::EventCallback;

    void subscribeToProperty(const std::string& deviceId, const std::string& property,
                            PropertyCallback callback);
    void unsubscribeFromProperty(const std::string& deviceId, const std::string& property);

    void subscribeToEvent(const std::string& deviceId, const std::string& event,
                         EventCallback callback);
    void unsubscribeFromEvent(const std::string& deviceId, const std::string& event);

    // Authentication (simplified - could be extracted to AuthenticationManager later)
    bool authenticate(const std::string& method, const std::string& credentials);

    // Event Publishing (simplified - could be extracted to EventPublisher later)
    void publishEvent(const std::string& eventName,
                     const json& details = json::object(),
                     Message::Priority priority = Message::Priority::NORMAL);

    // Message Processing Control
    void run();
    void startMessageProcessing();
    void stopMessageProcessing();

    // Configuration
    void setMessageRetryParams(int maxRetries, int retryIntervalMs);

    // Status and Statistics
    json getStatusInfo() const;
    json getConnectionStatus() const;
    json getDeviceStats() const;
    json getExecutionStats() const;
    json getSubscriptionStats() const;
    json getProcessingStats() const;

    // Component Access (for advanced usage)
    ConnectionManager* getConnectionManager() const { return connectionManager.get(); }
    MessageProcessor* getMessageProcessor() const { return messageProcessor.get(); }
    DeviceManager* getDeviceManager() const { return deviceManager.get(); }
    CommandExecutor* getCommandExecutor() const { return commandExecutor.get(); }
    SubscriptionManager* getSubscriptionManager() const { return subscriptionManager.get(); }

private:
    // Core components
    std::unique_ptr<ConnectionManager> connectionManager;
    std::unique_ptr<MessageProcessor> messageProcessor;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<CommandExecutor> commandExecutor;
    std::unique_ptr<SubscriptionManager> subscriptionManager;

    // Component initialization and setup
    void initializeComponents();
    void setupMessageHandlers();
    void cleanupComponents();

    // Message handlers for different message types
    void handleEventMessage(const Message& message);
    void handleErrorMessage(const Message& message);

    // Connection state callback
    void onConnectionStateChanged(bool connected);
};

} // namespace hydrogen

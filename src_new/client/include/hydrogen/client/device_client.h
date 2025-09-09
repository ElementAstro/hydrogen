#pragma once

#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

// Forward declarations for refactored components
namespace hydrogen {
class ConnectionManager;
class MessageProcessor;
class DeviceManager;
class CommandExecutor;
class SubscriptionManager;

// Message classes forward declarations
class Message;
class CommandMessage;
class ResponseMessage;
class EventMessage;
class DiscoveryRequestMessage;
class DiscoveryResponseMessage;
}

namespace hydrogen {
namespace client {

using json = nlohmann::json;

/**
 * @class DeviceClient
 * @brief Optimized client for communicating with astronomical device servers.
 *
 * This class provides functionality to connect to device servers, discover devices,
 * and control astronomical equipment. It uses a component-based architecture for
 * better performance, maintainability, and error handling.
 */
class DeviceClient {
public:
  /**
   * @brief Default constructor.
   */
  DeviceClient();

  /**
   * @brief Destructor.
   */
  ~DeviceClient();

  /**
   * @brief Connect to a device server.
   *
   * @param host Server hostname or IP address
   * @param port Server port number
   * @return True if connection was successful, false otherwise
   */
  bool connect(const std::string &host, uint16_t port);

  /**
   * @brief Disconnect from the server.
   */
  void disconnect();

  /**
   * @brief Check if connected to server.
   *
   * @return True if connected, false otherwise
   */
  bool isConnected() const;

  /**
   * @brief Discover available devices.
   *
   * @param deviceTypes List of device types to discover (empty for all types)
   * @return JSON object containing discovered devices
   */
  json discoverDevices(const std::vector<std::string> &deviceTypes = {});

  /**
   * @brief Get list of all devices.
   *
   * @return JSON object containing device information
   */
  json getDevices() const;

  /**
   * @brief Get properties of a specific device.
   *
   * @param deviceId Unique identifier for the device
   * @param properties List of property names to retrieve (empty for all)
   * @return JSON object containing device properties
   */
  json getDeviceProperties(const std::string &deviceId,
                           const std::vector<std::string> &properties = {});

  /**
   * @brief Set properties of a specific device.
   *
   * @param deviceId Unique identifier for the device
   * @param properties JSON object containing properties to set
   * @return JSON object containing the result of the operation
   */
  json setDeviceProperties(const std::string &deviceId, const json &properties);

  /**
   * @brief Execute a command on a device.
   *
   * @param deviceId Unique identifier for the device
   * @param command Command name to execute
   * @param parameters Command parameters (optional)
   * @return JSON object containing the command result
   */
  json executeCommand(const std::string &deviceId, const std::string &command,
                     const json &parameters = json::object());

  /**
   * @brief Send a message and wait for response.
   *
   * @param message Message to send
   * @param timeoutMs Timeout in milliseconds (default: 5000)
   * @return JSON response or null if timeout/error
   */
  json sendMessage(std::shared_ptr<hydrogen::Message> message, int timeoutMs = 5000);

  /**
   * @brief Send a message asynchronously.
   *
   * @param message Message to send
   * @param callback Callback function to handle the response
   */
  void sendMessageAsync(std::shared_ptr<hydrogen::Message> message,
                       std::function<void(const json &)> callback);

  // Property and event subscription callbacks
  using PropertyCallback = std::function<void(const std::string &, const json &)>;
  using EventCallback = std::function<void(const std::string &, const json &)>;

  /**
   * @brief Subscribe to property changes for a device.
   *
   * @param deviceId Device identifier
   * @param property Property name
   * @param callback Callback function to handle property changes
   */
  void subscribeToProperty(const std::string &deviceId, const std::string &property,
                          PropertyCallback callback);

  /**
   * @brief Unsubscribe from property changes.
   *
   * @param deviceId Device identifier
   * @param property Property name
   */
  void unsubscribeFromProperty(const std::string &deviceId, const std::string &property);

  /**
   * @brief Subscribe to events from a device.
   *
   * @param deviceId Device identifier
   * @param eventType Event type name
   * @param callback Callback function to handle events
   */
  void subscribeToEvent(const std::string &deviceId, const std::string &eventType,
                       EventCallback callback);

  /**
   * @brief Unsubscribe from events.
   *
   * @param deviceId Device identifier
   * @param eventType Event type name
   */
  void unsubscribeFromEvent(const std::string &deviceId, const std::string &eventType);

  /**
   * @brief Set connection status callback.
   *
   * @param callback Function to call when connection status changes
   */
  void setConnectionCallback(std::function<void(bool)> callback);

  /**
   * @brief Set authentication token.
   *
   * @param token Authentication token from server
   */
  void setAuthToken(const std::string &token);

  /**
   * @brief Get current authentication token.
   *
   * @return Current authentication token
   */
  std::string getAuthToken() const;

  /**
   * @brief Enable automatic reconnection.
   *
   * @param enabled True to enable auto-reconnect, false to disable
   * @param intervalMs Reconnection interval in milliseconds
   */
  void setAutoReconnect(bool enabled, int intervalMs = 5000);

private:
  // Core components
  std::unique_ptr<hydrogen::ConnectionManager> connectionManager;
  std::unique_ptr<hydrogen::MessageProcessor> messageProcessor;
  std::unique_ptr<hydrogen::DeviceManager> deviceManager;
  std::unique_ptr<hydrogen::CommandExecutor> commandExecutor;
  std::unique_ptr<hydrogen::SubscriptionManager> subscriptionManager;

  // Component initialization and setup
  void initializeComponents();
  void setupMessageHandlers();
  void cleanupComponents();
};

} // namespace client
} // namespace hydrogen

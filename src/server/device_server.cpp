#include "device_server.h"
#include <stdexcept>

namespace astrocomm {

DeviceServer::DeviceServer(uint16_t port) : serverPort(port) {

  deviceManager = std::make_unique<DeviceManager>();
  authManager = std::make_unique<AuthManager>();

  // 设置WebSocket路由
  CROW_ROUTE(app, "/ws")
      .websocket()
      .onopen([this](crow::websocket::connection &conn) {
        handleWebSocketOpen(conn);
      })
      .onclose(
          [this](crow::websocket::connection &conn, const std::string &reason) {
            handleWebSocketClose(conn);
          })
      .onmessage([this](crow::websocket::connection &conn,
                        const std::string &data, bool is_binary) {
        handleWebSocketMessage(conn, data, is_binary);
      });

  // 设置默认消息处理器
  setMessageHandler(
      MessageType::REGISTRATION,
      [this](std::shared_ptr<Message> msg, crow::websocket::connection &conn) {
        handleRegistrationMessage(
            std::static_pointer_cast<RegistrationMessage>(msg), conn);
      });

  setMessageHandler(
      MessageType::DISCOVERY_REQUEST,
      [this](std::shared_ptr<Message> msg, crow::websocket::connection &conn) {
        handleDiscoveryRequest(
            std::static_pointer_cast<DiscoveryRequestMessage>(msg), conn);
      });

  setMessageHandler(
      MessageType::AUTHENTICATION,
      [this](std::shared_ptr<Message> msg, crow::websocket::connection &conn) {
        handleAuthenticationMessage(
            std::static_pointer_cast<AuthenticationMessage>(msg), conn);
      });

  setMessageHandler(
      MessageType::COMMAND,
      [this](std::shared_ptr<Message> msg, crow::websocket::connection &conn) {
        handleCommandMessage(std::static_pointer_cast<CommandMessage>(msg),
                             conn);
      });

  setMessageHandler(
      MessageType::RESPONSE,
      [this](std::shared_ptr<Message> msg, crow::websocket::connection &conn) {
        handleResponseMessage(std::static_pointer_cast<ResponseMessage>(msg),
                              conn);
      });

  setMessageHandler(
      MessageType::EVENT,
      [this](std::shared_ptr<Message> msg, crow::websocket::connection &conn) {
        handleEventMessage(std::static_pointer_cast<EventMessage>(msg), conn);
      });

  logInfo("Device server initialized on port " + std::to_string(serverPort),
          "DeviceServer");
}

DeviceServer::~DeviceServer() { stop(); }

void DeviceServer::start() {
  logInfo("Starting device server on port " + std::to_string(serverPort),
          "DeviceServer");
  app.port(serverPort).multithreaded().run();
}

void DeviceServer::stop() {
  logInfo("Stopping device server", "DeviceServer");
  app.stop();
}

void DeviceServer::setMessageHandler(MessageType type, MessageHandler handler) {
  messageHandlers[type] = handler;
}

void DeviceServer::handleWebSocketOpen(crow::websocket::connection &conn) {
  logInfo("New WebSocket connection opened", "DeviceServer");
  // Wait for first message to determine if this is a device or client
}

void DeviceServer::handleWebSocketClose(crow::websocket::connection &conn) {
  std::lock_guard<std::mutex> lock(connectionsMutex);

  // Check if this was a device connection
  auto deviceIt = deviceConnections.find(&conn);
  if (deviceIt != deviceConnections.end()) {
    std::string deviceId = deviceIt->second;
    logInfo("Device disconnected: " + deviceId, "DeviceServer");

    // Remove the device
    deviceManager->removeDevice(deviceId);
    deviceConnections.erase(deviceIt);

    // Notify clients
    EventMessage event("DEVICE_REMOVED");
    event.setDetails({{"deviceId", deviceId}});
    broadcastToClients(event);
  }

  // Check if this was a client connection
  auto clientIt = clientConnections.find(&conn);
  if (clientIt != clientConnections.end()) {
    logInfo("Client disconnected: " + clientIt->second, "DeviceServer");
    clientConnections.erase(clientIt);
  }
}

void DeviceServer::handleWebSocketMessage(crow::websocket::connection &conn,
                                          const std::string &data,
                                          bool is_binary) {
  if (is_binary) {
    sendErrorResponse(conn, "INVALID_FORMAT",
                      "Binary messages are not supported");
    return;
  }

  try {
    // Parse the JSON message
    json j = json::parse(data);
    std::shared_ptr<Message> msg = createMessageFromJson(j);

    // Find and call the appropriate handler
    auto handlerIt = messageHandlers.find(msg->getMessageType());
    if (handlerIt != messageHandlers.end()) {
      handlerIt->second(msg, conn);
    } else {
      logWarning("No handler for message type: " +
                     messageTypeToString(msg->getMessageType()),
                 "DeviceServer");
      sendErrorResponse(conn, "UNSUPPORTED_MESSAGE_TYPE",
                        "Unsupported message type: " +
                            messageTypeToString(msg->getMessageType()));
    }
  } catch (const std::exception &e) {
    logError("Error handling message: " + std::string(e.what()),
             "DeviceServer");
    sendErrorResponse(conn, "INVALID_MESSAGE",
                      std::string("Error parsing message: ") + e.what());
  }
}

void DeviceServer::handleRegistrationMessage(
    std::shared_ptr<RegistrationMessage> msg,
    crow::websocket::connection &conn) {
  try {
    json deviceInfo = msg->getDeviceInfo();
    std::string deviceId = deviceInfo["deviceId"];

    // Register the device
    deviceManager->addDevice(deviceId, deviceInfo);

    // Associate connection with device
    registerDeviceConnection(deviceId, conn);

    // Send registration confirmation
    ResponseMessage response;
    response.setStatus("SUCCESS");
    response.setOriginalMessageId(msg->getMessageId());
    response.setDetails({{"message", "Device registered successfully"}});

    conn.send_text(response.toJson().dump());

    // Notify clients
    EventMessage event("DEVICE_REGISTERED");
    event.setDetails({{"device", deviceInfo}});
    broadcastToClients(event);

    logInfo("Device registered: " + deviceId, "DeviceServer");
  } catch (const std::exception &e) {
    logError("Error registering device: " + std::string(e.what()),
             "DeviceServer");
    sendErrorResponse(conn, "REGISTRATION_ERROR",
                      std::string("Error registering device: ") + e.what(),
                      msg->getMessageId());
  }
}

void DeviceServer::handleDiscoveryRequest(
    std::shared_ptr<DiscoveryRequestMessage> msg,
    crow::websocket::connection &conn) {
  try {
    // Register as client on first discovery request
    registerClientConnection(conn);

    // Get device list (possibly filtered by types)
    std::vector<std::string> deviceTypes = msg->getDeviceTypes();
    json devices = deviceManager->getDevices(deviceTypes);

    // Send discovery response
    DiscoveryResponseMessage response;
    response.setDevices(devices);
    response.setOriginalMessageId(msg->getMessageId());

    conn.send_text(response.toJson().dump());

    logInfo("Sent discovery response with " + std::to_string(devices.size()) +
                " devices",
            "DeviceServer");
  } catch (const std::exception &e) {
    logError("Error handling discovery request: " + std::string(e.what()),
             "DeviceServer");
    sendErrorResponse(conn, "DISCOVERY_ERROR",
                      std::string("Error processing discovery request: ") +
                          e.what(),
                      msg->getMessageId());
  }
}

void DeviceServer::handleAuthenticationMessage(
    std::shared_ptr<AuthenticationMessage> msg,
    crow::websocket::connection &conn) {
  try {
    std::string method = msg->getMethod();
    std::string credentials = msg->getCredentials();

    bool authenticated = authManager->authenticate(method, credentials);

    ResponseMessage response;
    response.setOriginalMessageId(msg->getMessageId());

    if (authenticated) {
      response.setStatus("SUCCESS");
      response.setDetails({{"message", "Authentication successful"}});

      // Register as a client after successful authentication
      registerClientConnection(conn);

      logInfo("Client authenticated successfully", "DeviceServer");
    } else {
      response.setStatus("ERROR");
      response.setDetails({{"error", "AUTHENTICATION_FAILED"},
                           {"message", "Invalid credentials"}});

      logWarning("Authentication failed", "DeviceServer");
    }

    conn.send_text(response.toJson().dump());
  } catch (const std::exception &e) {
    logError("Error during authentication: " + std::string(e.what()),
             "DeviceServer");
    sendErrorResponse(conn, "AUTHENTICATION_ERROR",
                      std::string("Authentication error: ") + e.what(),
                      msg->getMessageId());
  }
}

void DeviceServer::handleCommandMessage(std::shared_ptr<CommandMessage> msg,
                                        crow::websocket::connection &conn) {
  try {
    std::string deviceId = msg->getDeviceId();

    // Verify the device exists
    if (!deviceManager->deviceExists(deviceId)) {
      sendErrorResponse(conn, "DEVICE_NOT_FOUND",
                        "Device not found: " + deviceId, msg->getMessageId());
      return;
    }

    // Forward the command to the device
    forwardToDevice(deviceId, *msg);

    logInfo("Forwarded command to device: " + deviceId +
                ", command: " + msg->getCommand(),
            "DeviceServer");
  } catch (const std::exception &e) {
    logError("Error handling command: " + std::string(e.what()),
             "DeviceServer");
    sendErrorResponse(conn, "COMMAND_ERROR",
                      std::string("Error processing command: ") + e.what(),
                      msg->getMessageId());
  }
}

void DeviceServer::handleResponseMessage(std::shared_ptr<ResponseMessage> msg,
                                         crow::websocket::connection &conn) {
  try {
    // Responses should come from devices and go to clients
    std::string originalMsgId = msg->getOriginalMessageId();

    // Forward to all clients (the clients will filter based on message IDs they
    // care about)
    broadcastToClients(*msg);

    logInfo("Broadcasted response from device: " + msg->getDeviceId(),
            "DeviceServer");
  } catch (const std::exception &e) {
    logError("Error handling response: " + std::string(e.what()),
             "DeviceServer");
  }
}

void DeviceServer::handleEventMessage(std::shared_ptr<EventMessage> msg,
                                      crow::websocket::connection &conn) {
  try {
    // Events should come from devices and go to clients
    broadcastToClients(*msg);

    logInfo("Broadcasted event from device: " + msg->getDeviceId() +
                ", event: " + msg->getEvent(),
            "DeviceServer");
  } catch (const std::exception &e) {
    logError("Error handling event: " + std::string(e.what()), "DeviceServer");
  }
}

void DeviceServer::registerDeviceConnection(const std::string &deviceId,
                                            crow::websocket::connection &conn) {
  std::lock_guard<std::mutex> lock(connectionsMutex);
  deviceConnections[&conn] = deviceId;
}

void DeviceServer::registerClientConnection(crow::websocket::connection &conn) {
  std::lock_guard<std::mutex> lock(connectionsMutex);

  // Check if this connection is already registered
  auto it = clientConnections.find(&conn);
  if (it == clientConnections.end()) {
    // Generate a client ID
    std::string clientId = "client-" + generateUuid();
    clientConnections[&conn] = clientId;

    logInfo("New client registered: " + clientId, "DeviceServer");
  }
}

void DeviceServer::forwardToDevice(const std::string &deviceId,
                                   const Message &msg) {
  std::lock_guard<std::mutex> lock(connectionsMutex);

  // Find the device connection
  for (const auto &entry : deviceConnections) {
    if (entry.second == deviceId) {
      crow::websocket::connection *conn =
          static_cast<crow::websocket::connection *>(entry.first);
      conn->send_text(msg.toJson().dump());
      return;
    }
  }

  throw std::runtime_error("Device connection not found: " + deviceId);
}

void DeviceServer::forwardToClient(const std::string &clientId,
                                   const Message &msg) {
  std::lock_guard<std::mutex> lock(connectionsMutex);

  // Find the client connection
  for (const auto &entry : clientConnections) {
    if (entry.second == clientId) {
      crow::websocket::connection *conn =
          static_cast<crow::websocket::connection *>(entry.first);
      conn->send_text(msg.toJson().dump());
      return;
    }
  }

  logWarning("Client connection not found: " + clientId, "DeviceServer");
}

void DeviceServer::broadcastToClients(const Message &msg) {
  std::lock_guard<std::mutex> lock(connectionsMutex);

  std::string msgJson = msg.toJson().dump();

  for (const auto &entry : clientConnections) {
    crow::websocket::connection *conn =
        static_cast<crow::websocket::connection *>(entry.first);
    conn->send_text(msgJson);
  }
}

void DeviceServer::sendErrorResponse(crow::websocket::connection &conn,
                                     const std::string &code,
                                     const std::string &message,
                                     const std::string &originalMsgId) {
  ErrorMessage errorMsg(code, message);

  if (!originalMsgId.empty()) {
    errorMsg.setOriginalMessageId(originalMsgId);
  }

  conn.send_text(errorMsg.toJson().dump());
}

} // namespace astrocomm
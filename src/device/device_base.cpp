#include "device/device_base.h"
#include "common/logger.h"
#include "common/utils.h"
#include <chrono>
#include <thread>

namespace astrocomm {

DeviceBase::DeviceBase(const std::string &id, const std::string &type,
                       const std::string &mfr, const std::string &mdl)
    : deviceId(id), deviceType(type), manufacturer(mfr), model(mdl),
      firmwareVersion("1.0.0"), connected(false), running(false) {

  logInfo("Device created: " + deviceId + " (" + deviceType + ")",
          "DeviceBase");
}

DeviceBase::~DeviceBase() { disconnect(); }

bool DeviceBase::connect(const std::string &host, uint16_t port) {
  try {
    // Create a WebSocket connection
    ws = std::make_unique<websocket::stream<tcp::socket>>(ioc);

    // Resolve the host
    tcp::resolver resolver{ioc};
    auto const results = resolver.resolve(host, std::to_string(port));

    // Connect to the server
    auto ep = net::connect(ws->next_layer(), results);

    // Set up the websocket handshake
    std::string host_port = host + ":" + std::to_string(port);
    ws->set_option(
        websocket::stream_base::decorator([](websocket::request_type &req) {
          req.set(http::field::user_agent, "DeviceBase/1.0");
        }));

    ws->handshake(host_port, "/ws");
    connected = true;

    logInfo("Connected to server at " + host + ":" + std::to_string(port),
            deviceId);
    return true;
  } catch (const std::exception &e) {
    logError("Connection error: " + std::string(e.what()), deviceId);
    return false;
  }
}

void DeviceBase::disconnect() {
  if (connected && ws) {
    try {
      ws->close(websocket::close_code::normal);
      connected = false;
      logInfo("Disconnected from server", deviceId);
    } catch (const std::exception &e) {
      logError("Error disconnecting: " + std::string(e.what()), deviceId);
    }
  }
}

bool DeviceBase::registerDevice() {
  if (!connected) {
    logError("Cannot register device: not connected to server", deviceId);
    return false;
  }

  try {
    RegistrationMessage regMsg;
    regMsg.setDeviceInfo(getDeviceInfo());

    std::string msgJson = regMsg.toJson().dump();
    ws->write(net::buffer(msgJson));

    // Wait for response
    beast::flat_buffer buffer;
    ws->read(buffer);
    std::string response = beast::buffers_to_string(buffer.data());

    try {
      json respJson = json::parse(response);
      if (respJson["messageType"] == "RESPONSE" &&
          respJson["payload"]["status"] == "SUCCESS") {

        logInfo("Device registered successfully", deviceId);
        return true;
      } else {
        logError("Registration failed: " + response, deviceId);
        return false;
      }
    } catch (const std::exception &e) {
      logError("Error parsing registration response: " + std::string(e.what()),
               deviceId);
      return false;
    }
  } catch (const std::exception &e) {
    logError("Error registering device: " + std::string(e.what()), deviceId);
    return false;
  }
}

bool DeviceBase::start() {
  if (!connected) {
    logError("Cannot start device: not connected to server", deviceId);
    return false;
  }

  running = true;
  logInfo("Device started", deviceId);
  return true;
}

void DeviceBase::stop() {
  running = false;
  logInfo("Device stopped", deviceId);
}

void DeviceBase::run() {
  if (!connected || !running) {
    logError("Cannot run message loop: device not connected or not running",
             deviceId);
    return;
  }

  logInfo("Starting message loop", deviceId);

  while (connected && running) {
    try {
      beast::flat_buffer buffer;
      ws->read(buffer);
      std::string msg = beast::buffers_to_string(buffer.data());

      handleMessage(msg);
    } catch (beast::system_error const &se) {
      if (se.code() == websocket::error::closed) {
        logInfo("WebSocket connection closed", deviceId);
        connected = false;
        break;
      } else {
        logError("WebSocket error: " + se.code().message(), deviceId);
      }
    } catch (const std::exception &e) {
      logError("Error in message loop: " + std::string(e.what()), deviceId);
    }
  }

  logInfo("Message loop ended", deviceId);
}

std::string DeviceBase::getDeviceId() const { return deviceId; }

std::string DeviceBase::getDeviceType() const { return deviceType; }

json DeviceBase::getDeviceInfo() const {
  std::lock_guard<std::mutex> lock(propertiesMutex);

  json deviceInfo = {{"deviceId", deviceId},
                     {"deviceType", deviceType},
                     {"manufacturer", manufacturer},
                     {"model", model},
                     {"firmwareVersion", firmwareVersion}};

  if (!capabilities.empty()) {
    deviceInfo["capabilities"] = capabilities;
  }

  json propertyNames = json::array();
  for (const auto &prop : properties) {
    propertyNames.push_back(prop.first);
  }

  deviceInfo["properties"] = propertyNames;

  return deviceInfo;
}

void DeviceBase::setProperty(const std::string &property, const json &value) {
  json previousValue;

  {
    std::lock_guard<std::mutex> lock(propertiesMutex);

    // Store the previous value for event notification
    if (properties.find(property) != properties.end()) {
      previousValue = properties[property];
    }

    properties[property] = value;
  }

  // Send a property changed event if the value actually changed
  if (previousValue != value) {
    sendPropertyChangedEvent(property, value, previousValue);
  }
}

json DeviceBase::getProperty(const std::string &property) const {
  std::lock_guard<std::mutex> lock(propertiesMutex);

  auto it = properties.find(property);
  if (it != properties.end()) {
    return it->second;
  }

  return json();
}

void DeviceBase::registerCommandHandler(const std::string &command,
                                        CommandHandler handler) {
  commandHandlers[command] = handler;
  logDebug("Registered handler for command: " + command, deviceId);
}

void DeviceBase::handleMessage(const std::string &message) {
  try {
    json j = json::parse(message);
    std::unique_ptr<Message> msg = createMessageFromJson(j);

    if (msg->getMessageType() == MessageType::COMMAND) {
      CommandMessage *cmdMsg = static_cast<CommandMessage *>(msg.get());
      handleCommandMessage(*cmdMsg);
    } else {
      logWarning("Received non-command message: " +
                     messageTypeToString(msg->getMessageType()),
                 deviceId);
    }
  } catch (const std::exception &e) {
    logError("Error handling message: " + std::string(e.what()), deviceId);
  }
}

void DeviceBase::handleCommandMessage(const CommandMessage &cmd) {
  std::string cmdName = cmd.getCommand();
  logInfo("Received command: " + cmdName, deviceId);

  auto it = commandHandlers.find(cmdName);

  ResponseMessage response;
  response.setDeviceId(deviceId);
  response.setOriginalMessageId(cmd.getMessageId());
  response.setCommand(cmdName);

  if (it != commandHandlers.end()) {
    try {
      // Call the registered handler
      it->second(cmd, response);
    } catch (const std::exception &e) {
      logError("Error executing command handler: " + std::string(e.what()),
               deviceId);
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "COMMAND_EXECUTION_FAILED"},
           {"message", std::string("Error executing command: ") + e.what()}});
    }
  } else if (cmdName == "GET_PROPERTY") {
    // Handle standard GET_PROPERTY command
    json properties = cmd.getProperties();
    json responseProps;

    for (auto it = properties.begin(); it != properties.end(); ++it) {
      std::string propName = *it;
      json propValue = getProperty(propName);

      if (!propValue.is_null()) {
        responseProps[propName] = {{"value", propValue},
                                   {"timestamp", getIsoTimestamp()},
                                   {"status", "OK"}};
      } else {
        responseProps[propName] = {{"status", "NOT_FOUND"}};
      }
    }

    response.setStatus("SUCCESS");
    response.setProperties(responseProps);
  } else if (cmdName == "SET_PROPERTY") {
    // Handle standard SET_PROPERTY command
    json properties = cmd.getProperties();
    json updatedProps;

    for (auto it = properties.begin(); it != properties.end(); ++it) {
      std::string propName = it.key();
      json propValue = it.value();
      json previousValue = getProperty(propName);

      setProperty(propName, propValue);

      updatedProps[propName] = {{"value", propValue},
                                {"previousValue", previousValue},
                                {"timestamp", getIsoTimestamp()}};
    }

    response.setStatus("SUCCESS");
    response.setProperties(updatedProps);
  } else {
    // Unknown command
    logWarning("Unknown command: " + cmdName, deviceId);
    response.setStatus("ERROR");
    response.setDetails({{"error", "UNKNOWN_COMMAND"},
                         {"message", "Unknown command: " + cmdName}});
  }

  sendResponse(response);
}

void DeviceBase::sendResponse(const ResponseMessage &response) {
  if (!connected) {
    logWarning("Cannot send response: not connected", deviceId);
    return;
  }

  try {
    std::string msgJson = response.toJson().dump();
    ws->write(net::buffer(msgJson));

    logDebug("Sent response: " + response.getCommand() +
                 ", status: " + response.getStatus(),
             deviceId);
  } catch (const std::exception &e) {
    logError("Error sending response: " + std::string(e.what()), deviceId);
  }
}

void DeviceBase::sendEvent(const EventMessage &event) {
  if (!connected) {
    logWarning("Cannot send event: not connected", deviceId);
    return;
  }

  try {
    // Ensure the device ID is set
    EventMessage eventCopy = event;
    eventCopy.setDeviceId(deviceId);

    std::string msgJson = eventCopy.toJson().dump();
    ws->write(net::buffer(msgJson));

    logDebug("Sent event: " + event.getEvent(), deviceId);
  } catch (const std::exception &e) {
    logError("Error sending event: " + std::string(e.what()), deviceId);
  }
}

void DeviceBase::sendPropertyChangedEvent(const std::string &property,
                                          const json &value,
                                          const json &previousValue) {
  EventMessage event("PROPERTY_CHANGED");

  json properties;
  properties[property] = {{"value", value},
                          {"previousValue", previousValue},
                          {"timestamp", getIsoTimestamp()}};

  event.setProperties(properties);
  sendEvent(event);
}

} // namespace astrocomm
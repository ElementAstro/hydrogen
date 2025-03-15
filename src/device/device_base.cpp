#include "device/device_base.h"
#include "common/utils.h"

#include <spdlog/spdlog.h>

namespace http = boost::beast::http;

namespace astrocomm {

DeviceBase::DeviceBase(const std::string &id, const std::string &type,
                       const std::string &mfr, const std::string &mdl)
    : deviceId(id), deviceType(type), manufacturer(mfr), model(mdl),
      firmwareVersion("1.0.0"), connected(false), running(false) {

  SPDLOG_INFO("Device created: {} ({})", deviceId, deviceType);
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

    SPDLOG_INFO("Connected to server at {}:{}", host, port);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Connection error: {}", e.what());
    return false;
  }
}

void DeviceBase::disconnect() {
  if (connected && ws) {
    try {
      ws->close(websocket::close_code::normal);
      connected = false;
      SPDLOG_INFO("Disconnected from server");
    } catch (const std::exception &e) {
      SPDLOG_ERROR("Error disconnecting: {}", e.what());
    }
  }
}

bool DeviceBase::registerDevice() {
  if (!connected) {
    SPDLOG_ERROR("Cannot register device: not connected to server");
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

        SPDLOG_INFO("Device registered successfully");
        return true;
      } else {
        SPDLOG_ERROR("Registration failed: {}", response);
        return false;
      }
    } catch (const std::exception &e) {
      SPDLOG_ERROR("Error parsing registration response: {}", e.what());
      return false;
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error registering device: {}", e.what());
    return false;
  }
}

bool DeviceBase::start() {
  if (!connected) {
    SPDLOG_ERROR("Cannot start device: not connected to server");
    return false;
  }

  running = true;
  SPDLOG_INFO("Device started");
  return true;
}

void DeviceBase::stop() {
  running = false;
  SPDLOG_INFO("Device stopped");
}

void DeviceBase::run() {
  if (!connected || !running) {
    SPDLOG_ERROR(
        "Cannot run message loop: device not connected or not running");
    return;
  }

  SPDLOG_INFO("Starting message loop");

  while (connected && running) {
    try {
      beast::flat_buffer buffer;
      ws->read(buffer);
      std::string msg = beast::buffers_to_string(buffer.data());

      handleMessage(msg);
    } catch (beast::system_error const &se) {
      if (se.code() == websocket::error::closed) {
        SPDLOG_INFO("WebSocket connection closed");
        connected = false;
        break;
      } else {
        SPDLOG_ERROR("WebSocket error: {}", se.code().message());
      }
    } catch (const std::exception &e) {
      SPDLOG_ERROR("Error in message loop: {}", e.what());
    }
  }

  SPDLOG_INFO("Message loop ended");
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
  SPDLOG_DEBUG("Registered handler for command: {}", command);
}

void DeviceBase::handleMessage(const std::string &message) {
  try {
    json j = json::parse(message);
    std::unique_ptr<Message> msg = createMessageFromJson(j);

    if (msg->getMessageType() == MessageType::COMMAND) {
      CommandMessage *cmdMsg = static_cast<CommandMessage *>(msg.get());
      handleCommandMessage(*cmdMsg);
    } else {
      SPDLOG_WARN("Received non-command message: {}",
                  messageTypeToString(msg->getMessageType()));
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error handling message: {}", e.what());
  }
}

void DeviceBase::handleCommandMessage(const CommandMessage &cmd) {
  std::string cmdName = cmd.getCommand();
  SPDLOG_INFO("Received command: {}", cmdName);

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
      SPDLOG_ERROR("Error executing command handler: {}", e.what());
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
    SPDLOG_WARN("Unknown command: {}", cmdName);
    response.setStatus("ERROR");
    response.setDetails({{"error", "UNKNOWN_COMMAND"},
                         {"message", "Unknown command: " + cmdName}});
  }

  sendResponse(response);
}

void DeviceBase::sendResponse(const ResponseMessage &response) {
  if (!connected) {
    SPDLOG_WARN("Cannot send response: not connected", deviceId);
    return;
  }

  try {
    std::string msgJson = response.toJson().dump();
    ws->write(net::buffer(msgJson));

    SPDLOG_DEBUG("Sent response: {}, status: {}", response.getCommand(),
                 response.getStatus());
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error sending response: {}", e.what());
  }
}

void DeviceBase::sendEvent(const EventMessage &event) {
  if (!connected) {
    SPDLOG_WARN("Cannot send event: not connected", deviceId);
    return;
  }

  try {
    // Ensure the device ID is set
    EventMessage eventCopy = event;
    eventCopy.setDeviceId(deviceId);

    std::string msgJson = eventCopy.toJson().dump();
    ws->write(net::buffer(msgJson));

    SPDLOG_DEBUG("Sent event: {}", event.getEvent());
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error sending event: {}", e.what());
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
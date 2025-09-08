#include <hydrogen/core/communication/infrastructure/protocol_communicators.h>
#include <hydrogen/core/infrastructure/utils.h>
#include <spdlog/spdlog.h>

namespace hydrogen {
namespace core {

MultiProtocolDeviceCommunicator::MultiProtocolDeviceCommunicator(
    const std::string &deviceId)
    : deviceId_(deviceId) {
  SPDLOG_INFO("Multi-protocol device communicator initialized for device: {}",
              deviceId_);
}

MultiProtocolDeviceCommunicator::~MultiProtocolDeviceCommunicator() {
  // Cleanup all protocol communicators
  std::lock_guard<std::mutex> lock(protocolsMutex_);
  mqttCommunicators_.clear();
  grpcCommunicators_.clear();
  zmqCommunicators_.clear();
  tcpCommunicators_.clear();
  stdioCommunicators_.clear();
  SPDLOG_INFO("Multi-protocol device communicator destroyed for device: {}",
              deviceId_);
}

bool MultiProtocolDeviceCommunicator::addProtocol(
    CommunicationProtocol protocol, const json &config) {
  std::lock_guard<std::mutex> lock(protocolsMutex_);

  try {
    switch (protocol) {
    case CommunicationProtocol::MQTT: {
      MqttConfig mqttConfig;
      if (config.contains("brokerHost"))
        mqttConfig.brokerHost = config["brokerHost"];
      if (config.contains("brokerPort"))
        mqttConfig.brokerPort = config["brokerPort"];
      if (config.contains("clientId"))
        mqttConfig.clientId = config["clientId"];
      if (config.contains("username"))
        mqttConfig.username = config["username"];
      if (config.contains("password"))
        mqttConfig.password = config["password"];
      if (config.contains("useTls"))
        mqttConfig.useTls = config["useTls"];
      if (config.contains("qosLevel"))
        mqttConfig.qosLevel = config["qosLevel"];
      if (config.contains("topicPrefix"))
        mqttConfig.topicPrefix = config["topicPrefix"];

      auto communicator =
          ProtocolCommunicatorFactory::createMqttCommunicator(mqttConfig);

      // Set up event handlers
      communicator->setMessageHandler(
          [this](const std::string &topic, const std::string &message) {
            if (messageHandler_) {
              CommunicationMessage msg;
              msg.messageId = generateUuid();
              msg.deviceId = deviceId_;
              msg.command = topic;
              try {
                msg.payload = json::parse(message);
              } catch (...) {
                msg.payload = json{{"raw_message", message}};
              }
              msg.timestamp = std::chrono::system_clock::now();
              messageHandler_(msg, CommunicationProtocol::MQTT);
            }
          });

      communicator->setConnectionHandler([this](bool connected) {
        if (connectionHandler_) {
          connectionHandler_(CommunicationProtocol::MQTT, connected);
        }
      });

      mqttCommunicators_[protocol] = std::move(communicator);
      SPDLOG_INFO("Added MQTT protocol for device: {}", deviceId_);
      return true;
    }

    case CommunicationProtocol::GRPC: {
      GrpcConfig grpcConfig;
      if (config.contains("serverAddress"))
        grpcConfig.serverAddress = config["serverAddress"];
      if (config.contains("useTls"))
        grpcConfig.useTls = config["useTls"];
      if (config.contains("maxReceiveMessageSize"))
        grpcConfig.maxReceiveMessageSize = config["maxReceiveMessageSize"];
      if (config.contains("maxSendMessageSize"))
        grpcConfig.maxSendMessageSize = config["maxSendMessageSize"];
      if (config.contains("enableReflection"))
        grpcConfig.enableReflection = config["enableReflection"];

      auto communicator =
          ProtocolCommunicatorFactory::createGrpcCommunicator(grpcConfig);

      // Set up event handlers
      communicator->setStreamHandler([this](const std::string &data) {
        if (messageHandler_) {
          CommunicationMessage msg;
          msg.messageId = generateUuid();
          msg.deviceId = deviceId_;
          msg.command = "stream_data";
          try {
            msg.payload = json::parse(data);
          } catch (...) {
            msg.payload = json{{"raw_data", data}};
          }
          msg.timestamp = std::chrono::system_clock::now();
          messageHandler_(msg, CommunicationProtocol::GRPC);
        }
      });

      communicator->setErrorHandler([this](const std::string &error) {
        SPDLOG_ERROR("gRPC error for device {}: {}", deviceId_, error);
      });

      grpcCommunicators_[protocol] = std::move(communicator);
      SPDLOG_INFO("Added gRPC protocol for device: {}", deviceId_);
      return true;
    }

    case CommunicationProtocol::ZEROMQ: {
      ZmqConfig zmqConfig;
      if (config.contains("bindAddress"))
        zmqConfig.bindAddress = config["bindAddress"];
      if (config.contains("connectAddress"))
        zmqConfig.connectAddress = config["connectAddress"];
      if (config.contains("socketType"))
        zmqConfig.socketType = config["socketType"];
      if (config.contains("highWaterMark"))
        zmqConfig.highWaterMark = config["highWaterMark"];
      if (config.contains("lingerTime"))
        zmqConfig.lingerTime = config["lingerTime"];

      ZmqCommunicator::SocketType socketType =
          static_cast<ZmqCommunicator::SocketType>(zmqConfig.socketType);
      auto communicator = ProtocolCommunicatorFactory::createZmqCommunicator(
          zmqConfig, socketType);

      // Set up event handlers
      communicator->setMessageHandler(
          [this](const std::vector<std::string> &multipart) {
            if (messageHandler_ && !multipart.empty()) {
              CommunicationMessage msg;
              msg.messageId = generateUuid();
              msg.deviceId = deviceId_;
              msg.command = multipart.size() > 1 ? multipart[0] : "zmq_message";

              if (multipart.size() == 1) {
                try {
                  msg.payload = json::parse(multipart[0]);
                } catch (...) {
                  msg.payload = json{{"raw_message", multipart[0]}};
                }
              } else {
                msg.payload = json{{"multipart", multipart}};
              }

              msg.timestamp = std::chrono::system_clock::now();
              messageHandler_(msg, CommunicationProtocol::ZEROMQ);
            }
          });

      communicator->setErrorHandler([this](const std::string &error) {
        SPDLOG_ERROR("ZeroMQ error for device {}: {}", deviceId_, error);
      });

      zmqCommunicators_[protocol] = std::move(communicator);
      SPDLOG_INFO("Added ZeroMQ protocol for device: {}", deviceId_);
      return true;
    }

    case CommunicationProtocol::TCP: {
      TcpConfig tcpConfig;
      if (config.contains("serverAddress"))
        tcpConfig.serverAddress = config["serverAddress"];
      if (config.contains("serverPort"))
        tcpConfig.serverPort = config["serverPort"];
      if (config.contains("isServer"))
        tcpConfig.isServer = config["isServer"];
      if (config.contains("connectTimeout"))
        tcpConfig.connectTimeout =
            std::chrono::milliseconds(config["connectTimeout"]);
      if (config.contains("readTimeout"))
        tcpConfig.readTimeout =
            std::chrono::milliseconds(config["readTimeout"]);
      if (config.contains("writeTimeout"))
        tcpConfig.writeTimeout =
            std::chrono::milliseconds(config["writeTimeout"]);
      if (config.contains("bufferSize"))
        tcpConfig.bufferSize = config["bufferSize"];
      if (config.contains("enableKeepAlive"))
        tcpConfig.enableKeepAlive = config["enableKeepAlive"];
      if (config.contains("maxConnections"))
        tcpConfig.maxConnections = config["maxConnections"];
      if (config.contains("bindInterface"))
        tcpConfig.bindInterface = config["bindInterface"];

      auto communicator =
          ProtocolCommunicatorFactory::createTcpCommunicator(tcpConfig);

      // Set up event handlers
      communicator->setMessageHandler(
          [this](const std::string &message, const std::string &clientId) {
            if (messageHandler_) {
              CommunicationMessage msg;
              msg.messageId = generateUuid();
              msg.deviceId = deviceId_;
              msg.command = "tcp_message";
              try {
                msg.payload = json::parse(message);
              } catch (...) {
                msg.payload = json{{"raw_message", message}};
              }
              msg.payload["client_id"] = clientId;
              msg.timestamp = std::chrono::system_clock::now();
              messageHandler_(msg, CommunicationProtocol::TCP);
            }
          });

      communicator->setConnectionHandler(
          [this](bool connected, const std::string &clientId) {
            if (connectionHandler_) {
              connectionHandler_(CommunicationProtocol::TCP, connected);
            }
            SPDLOG_INFO("TCP client {} {} for device {}", clientId,
                        connected ? "connected" : "disconnected", deviceId_);
          });

      communicator->setErrorHandler([this](const std::string &error) {
        SPDLOG_ERROR("TCP error for device {}: {}", deviceId_, error);
      });

      tcpCommunicators_[protocol] = std::move(communicator);
      SPDLOG_INFO("Added TCP protocol for device: {}", deviceId_);
      return true;
    }

    case CommunicationProtocol::STDIO: {
      StdioConfig stdioConfig;
      if (config.contains("enableLineBuffering"))
        stdioConfig.enableLineBuffering = config["enableLineBuffering"];
      if (config.contains("enableBinaryMode"))
        stdioConfig.enableBinaryMode = config["enableBinaryMode"];
      if (config.contains("readTimeout"))
        stdioConfig.readTimeout =
            std::chrono::milliseconds(config["readTimeout"]);
      if (config.contains("writeTimeout"))
        stdioConfig.writeTimeout =
            std::chrono::milliseconds(config["writeTimeout"]);
      if (config.contains("bufferSize"))
        stdioConfig.bufferSize = config["bufferSize"];
      if (config.contains("lineTerminator"))
        stdioConfig.lineTerminator = config["lineTerminator"];
      if (config.contains("enableEcho"))
        stdioConfig.enableEcho = config["enableEcho"];
      if (config.contains("enableFlush"))
        stdioConfig.enableFlush = config["enableFlush"];
      if (config.contains("encoding"))
        stdioConfig.encoding = config["encoding"];

      auto communicator =
          ProtocolCommunicatorFactory::createStdioCommunicator(stdioConfig);

      // Set up event handlers
      communicator->setMessageHandler([this](const std::string &message) {
        if (messageHandler_) {
          CommunicationMessage msg;
          msg.messageId = generateUuid();
          msg.deviceId = deviceId_;
          msg.command = "stdio_message";
          try {
            msg.payload = json::parse(message);
          } catch (...) {
            msg.payload = json{{"raw_message", message}};
          }
          msg.timestamp = std::chrono::system_clock::now();
          messageHandler_(msg, CommunicationProtocol::STDIO);
        }
      });

      communicator->setErrorHandler([this](const std::string &error) {
        SPDLOG_ERROR("Stdio error for device {}: {}", deviceId_, error);
      });

      stdioCommunicators_[protocol] = std::move(communicator);
      SPDLOG_INFO("Added stdio protocol for device: {}", deviceId_);
      return true;
    }

    default:
      SPDLOG_WARN("Unsupported protocol type: {}", static_cast<int>(protocol));
      return false;
    }

  } catch (const std::exception &e) {
    SPDLOG_ERROR("Failed to add protocol {} for device {}: {}",
                 static_cast<int>(protocol), deviceId_, e.what());
    return false;
  }
}

bool MultiProtocolDeviceCommunicator::removeProtocol(
    CommunicationProtocol protocol) {
  std::lock_guard<std::mutex> lock(protocolsMutex_);

  switch (protocol) {
  case CommunicationProtocol::MQTT:
    if (mqttCommunicators_.erase(protocol) > 0) {
      SPDLOG_INFO("Removed MQTT protocol for device: {}", deviceId_);
      return true;
    }
    break;

  case CommunicationProtocol::GRPC:
    if (grpcCommunicators_.erase(protocol) > 0) {
      SPDLOG_INFO("Removed gRPC protocol for device: {}", deviceId_);
      return true;
    }
    break;

  case CommunicationProtocol::ZEROMQ:
    if (zmqCommunicators_.erase(protocol) > 0) {
      SPDLOG_INFO("Removed ZeroMQ protocol for device: {}", deviceId_);
      return true;
    }
    break;

  case CommunicationProtocol::TCP:
    if (tcpCommunicators_.erase(protocol) > 0) {
      SPDLOG_INFO("Removed TCP protocol for device: {}", deviceId_);
      return true;
    }
    break;

  case CommunicationProtocol::CUSTOM: // stdio
    if (stdioCommunicators_.erase(protocol) > 0) {
      SPDLOG_INFO("Removed stdio protocol for device: {}", deviceId_);
      return true;
    }
    break;

  default:
    break;
  }

  return false;
}

bool MultiProtocolDeviceCommunicator::hasProtocol(
    CommunicationProtocol protocol) const {
  std::lock_guard<std::mutex> lock(protocolsMutex_);

  switch (protocol) {
  case CommunicationProtocol::MQTT:
    return mqttCommunicators_.find(protocol) != mqttCommunicators_.end();
  case CommunicationProtocol::GRPC:
    return grpcCommunicators_.find(protocol) != grpcCommunicators_.end();
  case CommunicationProtocol::ZEROMQ:
    return zmqCommunicators_.find(protocol) != zmqCommunicators_.end();
  case CommunicationProtocol::TCP:
    return tcpCommunicators_.find(protocol) != tcpCommunicators_.end();
  case CommunicationProtocol::CUSTOM: // stdio
    return stdioCommunicators_.find(protocol) != stdioCommunicators_.end();
  default:
    return false;
  }
}

std::vector<CommunicationProtocol>
MultiProtocolDeviceCommunicator::getActiveProtocols() const {
  std::lock_guard<std::mutex> lock(protocolsMutex_);
  std::vector<CommunicationProtocol> protocols;

  for (const auto &[protocol, _] : mqttCommunicators_) {
    protocols.push_back(protocol);
  }
  for (const auto &[protocol, _] : grpcCommunicators_) {
    protocols.push_back(protocol);
  }
  for (const auto &[protocol, _] : zmqCommunicators_) {
    protocols.push_back(protocol);
  }

  return protocols;
}

bool MultiProtocolDeviceCommunicator::sendMessage(
    const CommunicationMessage &message, CommunicationProtocol protocol) {
  std::lock_guard<std::mutex> lock(protocolsMutex_);

  try {
    switch (protocol) {
    case CommunicationProtocol::MQTT: {
      auto it = mqttCommunicators_.find(protocol);
      if (it != mqttCommunicators_.end()) {
        std::string topic = message.command;
        if (!it->second->getConfig().topicPrefix.empty()) {
          topic = it->second->getConfig().topicPrefix + "/" + topic;
        }
        return it->second->publish(topic, message.payload);
      }
      break;
    }

    case CommunicationProtocol::GRPC: {
      auto it = grpcCommunicators_.find(protocol);
      if (it != grpcCommunicators_.end()) {
        auto response = it->second->sendUnaryRequest(message);
        return response.success;
      }
      break;
    }

    case CommunicationProtocol::ZEROMQ: {
      auto it = zmqCommunicators_.find(protocol);
      if (it != zmqCommunicators_.end()) {
        return it->second->send(message.payload);
      }
      break;
    }

    default:
      SPDLOG_WARN("Unsupported protocol for sending: {}",
                  static_cast<int>(protocol));
      return false;
    }

  } catch (const std::exception &e) {
    SPDLOG_ERROR("Failed to send message via protocol {}: {}",
                 static_cast<int>(protocol), e.what());
    return false;
  }

  return false;
}

bool MultiProtocolDeviceCommunicator::broadcastMessage(
    const CommunicationMessage &message) {
  std::lock_guard<std::mutex> lock(protocolsMutex_);
  bool success = true;

  // Broadcast to all MQTT communicators
  for (const auto &[protocol, communicator] : mqttCommunicators_) {
    if (!sendMessage(message, protocol)) {
      success = false;
    }
  }

  // Broadcast to all gRPC communicators
  for (const auto &[protocol, communicator] : grpcCommunicators_) {
    if (!sendMessage(message, protocol)) {
      success = false;
    }
  }

  // Broadcast to all ZeroMQ communicators
  for (const auto &[protocol, communicator] : zmqCommunicators_) {
    if (!sendMessage(message, protocol)) {
      success = false;
    }
  }

  return success;
}

bool MultiProtocolDeviceCommunicator::isConnected(
    CommunicationProtocol protocol) const {
  std::lock_guard<std::mutex> lock(protocolsMutex_);

  switch (protocol) {
  case CommunicationProtocol::MQTT: {
    auto it = mqttCommunicators_.find(protocol);
    return it != mqttCommunicators_.end() && it->second->isConnected();
  }
  case CommunicationProtocol::GRPC: {
    auto it = grpcCommunicators_.find(protocol);
    return it != grpcCommunicators_.end() && it->second->isConnected();
  }
  case CommunicationProtocol::ZEROMQ: {
    auto it = zmqCommunicators_.find(protocol);
    return it != zmqCommunicators_.end() && it->second->isConnected();
  }
  default:
    return false;
  }
}

json MultiProtocolDeviceCommunicator::getStatus() const {
  std::lock_guard<std::mutex> lock(protocolsMutex_);

  json status = {{"deviceId", deviceId_}, {"protocols", json::object()}};

  for (const auto &[protocol, communicator] : mqttCommunicators_) {
    status["protocols"]["mqtt"] = {
        {"connected", communicator->isConnected()},
        {"config",
         {{"brokerHost", communicator->getConfig().brokerHost},
          {"brokerPort", communicator->getConfig().brokerPort},
          {"clientId", communicator->getConfig().clientId}}}};
  }

  for (const auto &[protocol, communicator] : grpcCommunicators_) {
    status["protocols"]["grpc"] = {
        {"connected", communicator->isConnected()},
        {"config",
         {{"serverAddress", communicator->getConfig().serverAddress},
          {"useTls", communicator->getConfig().useTls}}}};
  }

  for (const auto &[protocol, communicator] : zmqCommunicators_) {
    status["protocols"]["zeromq"] = {
        {"connected", communicator->isConnected()},
        {"config",
         {{"socketType", static_cast<int>(communicator->getSocketType())},
          {"bindAddress", communicator->getConfig().bindAddress}}}};
  }

  return status;
}

} // namespace core
} // namespace hydrogen

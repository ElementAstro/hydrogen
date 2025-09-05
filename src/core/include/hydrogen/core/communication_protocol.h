#pragma once

#include <string>
#include <chrono>
#include <unordered_map>

namespace hydrogen {
namespace core {

/**
 * @brief Communication protocol enumeration
 */
enum class CommunicationProtocol {
    HTTP = 0,
    WEBSOCKET = 1,
    GRPC = 2,
    MQTT = 3,
    ZMQ = 4,
    TCP = 5,
    UDP = 6,
    STDIO = 7,
    FIFO = 8
};

/**
 * @brief Connection information structure
 */
struct ConnectionInfo {
    std::string clientId;
    CommunicationProtocol protocol;
    std::string remoteAddress;
    int remotePort;
    std::chrono::system_clock::time_point connectedAt;
    std::chrono::system_clock::time_point lastActivity;
    bool isActive;
    std::unordered_map<std::string, std::string> metadata;
};

} // namespace core
} // namespace hydrogen

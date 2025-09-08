#pragma once

#include <chrono>
#include <string>
#include "hydrogen/core/device/device_communicator.h"
#include <unordered_map>

namespace hydrogen {
namespace core {

// CommunicationProtocol is defined in device_communicator.h

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

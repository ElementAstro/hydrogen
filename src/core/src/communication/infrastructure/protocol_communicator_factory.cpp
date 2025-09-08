#include <hydrogen/core/communication/infrastructure/protocol_communicators.h>
#include <spdlog/spdlog.h>

namespace hydrogen {
namespace core {

// Forward declarations for implementation classes (defined in separate files)
class TcpCommunicatorImpl;
class StdioCommunicatorImpl;

// External factory functions (implementations are in separate files)
extern std::unique_ptr<StdioCommunicator>
createStdioCommunicatorImpl(const StdioConfig &config);

// TCP communicator implementation (stub for now)
std::unique_ptr<TcpCommunicator>
createTcpCommunicatorImpl(const TcpConfig &config) {
  // TODO: Implement actual TCP communicator
  // For now, return nullptr to indicate TCP is not yet implemented
  (void)config; // Suppress unused parameter warning
  SPDLOG_WARN("TCP communicator not yet implemented");
  return nullptr;
}

// Factory method implementations
std::unique_ptr<TcpCommunicator>
ProtocolCommunicatorFactory::createTcpCommunicator(const TcpConfig &config) {
  try {
    SPDLOG_INFO("Creating TCP communicator with address: {}:{}",
                config.serverAddress, config.serverPort);
    return createTcpCommunicatorImpl(config);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Failed to create TCP communicator: {}", e.what());
    return nullptr;
  }
}

std::unique_ptr<StdioCommunicator>
ProtocolCommunicatorFactory::createStdioCommunicator(
    const StdioConfig &config) {
  try {
    SPDLOG_INFO("Creating stdio communicator with buffer size: {}",
                config.bufferSize);
    return createStdioCommunicatorImpl(config);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Failed to create stdio communicator: {}", e.what());
    return nullptr;
  }
}

} // namespace core
} // namespace hydrogen

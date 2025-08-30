#include <astrocomm/core/protocol_communicators.h>
#include <spdlog/spdlog.h>

namespace astrocomm {
namespace core {

// Forward declarations for implementation classes (defined in separate files)
class TcpCommunicatorImpl;
class StdioCommunicatorImpl;

// External factory functions (implementations are in separate files)
extern std::unique_ptr<TcpCommunicator> createTcpCommunicatorImpl(const TcpConfig& config);
extern std::unique_ptr<StdioCommunicator> createStdioCommunicatorImpl(const StdioConfig& config);

// Factory method implementations
std::unique_ptr<TcpCommunicator> ProtocolCommunicatorFactory::createTcpCommunicator(const TcpConfig& config) {
    try {
        SPDLOG_INFO("Creating TCP communicator with address: {}:{}", config.serverAddress, config.serverPort);
        return createTcpCommunicatorImpl(config);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to create TCP communicator: {}", e.what());
        return nullptr;
    }
}

std::unique_ptr<StdioCommunicator> ProtocolCommunicatorFactory::createStdioCommunicator(const StdioConfig& config) {
    try {
        SPDLOG_INFO("Creating stdio communicator with buffer size: {}", config.bufferSize);
        return createStdioCommunicatorImpl(config);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to create stdio communicator: {}", e.what());
        return nullptr;
    }
}

} // namespace core
} // namespace astrocomm

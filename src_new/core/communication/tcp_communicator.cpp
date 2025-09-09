#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <hydrogen/core/communication/infrastructure/protocol_communicators.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <thread>
#include <unordered_map>

namespace hydrogen {
namespace core {

using boost::asio::ip::tcp;
namespace asio = boost::asio;

/**
 * @brief Concrete implementation of TCP communicator using Boost.Asio
 */
class TcpCommunicatorImpl : public TcpCommunicator {
public:
  TcpCommunicatorImpl(const TcpConfig &config)
      : TcpCommunicator(config), ioContext_(), acceptor_(ioContext_),
        nextClientId_(1), bytesSent_(0), bytesReceived_(0) {}

  ~TcpCommunicatorImpl() override { stop(); }

  bool start() override {
    if (running_.load()) {
      return true;
    }

    try {
      if (config_.isServer) {
        return startServer();
      } else {
        return startClient();
      }
    } catch (const std::exception &e) {
      SPDLOG_ERROR("Failed to start TCP communicator: {}", e.what());
      if (errorHandler_) {
        errorHandler_(e.what());
      }
      return false;
    }
  }

  void stop() override {
    if (!running_.load()) {
      return;
    }

    running_.store(false);
    connected_.store(false);

    try {
      ioContext_.stop();

      // Close all client connections
      std::lock_guard<std::mutex> lock(clientsMutex_);
      for (auto &[clientId, session] : clientSessions_) {
        if (session->socket.is_open()) {
          session->socket.close();
        }
      }
      clientSessions_.clear();

      if (acceptor_.is_open()) {
        acceptor_.close();
      }

      if (ioThread_.joinable()) {
        ioThread_.join();
      }

      SPDLOG_INFO("TCP communicator stopped");
    } catch (const std::exception &e) {
      SPDLOG_ERROR("Error stopping TCP communicator: {}", e.what());
    }
  }

  bool isConnected() const override { return connected_.load(); }

  bool sendMessage(const std::string &message,
                   const std::string &clientId) override {
    if (!running_.load()) {
      return false;
    }

    try {
      std::lock_guard<std::mutex> lock(clientsMutex_);

      if (config_.isServer) {
        if (clientId.empty()) {
          // Send to first available client
          if (clientSessions_.empty()) {
            return false;
          }
          auto &session = clientSessions_.begin()->second;
          return sendToSession(session, message);
        } else {
          auto it = clientSessions_.find(clientId);
          if (it != clientSessions_.end()) {
            return sendToSession(it->second, message);
          }
          return false;
        }
      } else {
        // Client mode - send to server
        if (clientSessions_.empty()) {
          return false;
        }
        auto &session = clientSessions_.begin()->second;
        return sendToSession(session, message);
      }
    } catch (const std::exception &e) {
      SPDLOG_ERROR("Failed to send TCP message: {}", e.what());
      return false;
    }
  }

  bool sendMessage(const json &message, const std::string &clientId) override {
    return sendMessage(message.dump(), clientId);
  }

  bool broadcastMessage(const std::string &message) override {
    if (!config_.isServer || !running_.load()) {
      return false;
    }

    std::lock_guard<std::mutex> lock(clientsMutex_);
    bool success = true;

    for (auto &[clientId, session] : clientSessions_) {
      if (!sendToSession(session, message)) {
        success = false;
      }
    }

    return success;
  }

  std::vector<std::string> getConnectedClients() const override {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::vector<std::string> clients;
    clients.reserve(clientSessions_.size());

    for (const auto &[clientId, session] : clientSessions_) {
      clients.push_back(clientId);
    }

    return clients;
  }

  bool disconnectClient(const std::string &clientId) override {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clientSessions_.find(clientId);
    if (it != clientSessions_.end()) {
      try {
        it->second->socket.close();
        clientSessions_.erase(it);
        return true;
      } catch (const std::exception &e) {
        SPDLOG_ERROR("Error disconnecting client {}: {}", clientId, e.what());
      }
    }
    return false;
  }

  size_t getConnectedClientCount() const override {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return clientSessions_.size();
  }

  uint64_t getBytesSent() const override { return bytesSent_.load(); }

  uint64_t getBytesReceived() const override { return bytesReceived_.load(); }

private:
  struct ClientSession {
    tcp::socket socket;
    std::string clientId;
    std::array<char, 8192> buffer;

    ClientSession(asio::io_context &ioContext, const std::string &id)
        : socket(ioContext), clientId(id) {}
  };

  asio::io_context ioContext_;
  tcp::acceptor acceptor_;
  std::thread ioThread_;

  mutable std::mutex clientsMutex_;
  std::unordered_map<std::string, std::shared_ptr<ClientSession>>
      clientSessions_;
  std::atomic<uint32_t> nextClientId_;

  std::atomic<uint64_t> bytesSent_;
  std::atomic<uint64_t> bytesReceived_;

  bool startServer() {
    tcp::endpoint endpoint(
        asio::ip::address::from_string(config_.bindInterface),
        config_.serverPort);

    acceptor_.open(endpoint.protocol());
    if (config_.reuseAddress) {
      acceptor_.set_option(asio::socket_base::reuse_address(true));
    }
    acceptor_.bind(endpoint);
    acceptor_.listen();

    running_.store(true);
    connected_.store(true);

    // Start IO thread
    ioThread_ = std::thread([this]() { ioContext_.run(); });

    // Start accepting connections
    startAccept();

    SPDLOG_INFO("TCP server started on {}:{}", config_.bindInterface,
                config_.serverPort);
    return true;
  }

  bool startClient() {
    auto session = std::make_shared<ClientSession>(ioContext_, "server");

    tcp::resolver resolver(ioContext_);
    auto endpoints = resolver.resolve(config_.serverAddress,
                                      std::to_string(config_.serverPort));

    asio::async_connect(
        session->socket, endpoints,
        [this, session](std::error_code ec, tcp::endpoint) {
          if (!ec) {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clientSessions_["server"] = session;
            connected_.store(true);

            if (connectionHandler_) {
              connectionHandler_(true, "server");
            }

            startReceive(session);
            SPDLOG_INFO("TCP client connected to {}:{}", config_.serverAddress,
                        config_.serverPort);
          } else {
            SPDLOG_ERROR("TCP client connection failed: {}", ec.message());
            if (errorHandler_) {
              errorHandler_(ec.message());
            }
          }
        });

    running_.store(true);

    // Start IO thread
    ioThread_ = std::thread([this]() { ioContext_.run(); });

    return true;
  }

  void startAccept() {
    if (!running_.load()) {
      return;
    }

    std::string clientId = "client_" + std::to_string(nextClientId_++);
    auto newSession = std::make_shared<ClientSession>(ioContext_, clientId);

    acceptor_.async_accept(
        newSession->socket, [this, newSession](std::error_code ec) {
          if (!ec) {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clientSessions_[newSession->clientId] = newSession;

            if (connectionHandler_) {
              connectionHandler_(true, newSession->clientId);
            }

            startReceive(newSession);
            SPDLOG_DEBUG("TCP client {} connected", newSession->clientId);
          }

          startAccept(); // Continue accepting new connections
        });
  }

  void startReceive(std::shared_ptr<ClientSession> session) {
    session->socket.async_read_some(
        asio::buffer(session->buffer),
        [this, session](std::error_code ec, std::size_t bytesTransferred) {
          if (!ec) {
            bytesReceived_.fetch_add(bytesTransferred);

            std::string message(session->buffer.data(), bytesTransferred);
            if (messageHandler_) {
              messageHandler_(message, session->clientId);
            }

            startReceive(session); // Continue receiving
          } else {
            handleDisconnection(session, ec);
          }
        });
  }

  bool sendToSession(std::shared_ptr<ClientSession> session,
                     const std::string &message) {
    try {
      std::size_t bytesWritten =
          asio::write(session->socket, asio::buffer(message));
      bytesSent_.fetch_add(bytesWritten);
      return true;
    } catch (const std::exception &e) {
      SPDLOG_ERROR("Failed to send to session {}: {}", session->clientId,
                   e.what());
      return false;
    }
  }

  void handleDisconnection(std::shared_ptr<ClientSession> session,
                           std::error_code ec) {
    SPDLOG_DEBUG("TCP client {} disconnected: {}", session->clientId,
                 ec.message());

    {
      std::lock_guard<std::mutex> lock(clientsMutex_);
      clientSessions_.erase(session->clientId);

      if (!config_.isServer && clientSessions_.empty()) {
        connected_.store(false);
      }
    }

    if (connectionHandler_) {
      connectionHandler_(false, session->clientId);
    }
  }
};

// Factory method implementation
TcpCommunicator::TcpCommunicator(const TcpConfig &config) : config_(config) {}
TcpCommunicator::~TcpCommunicator() = default;

// External factory function
std::unique_ptr<TcpCommunicator>
createTcpCommunicatorImpl(const TcpConfig &config) {
  return std::make_unique<TcpCommunicatorImpl>(config);
}

} // namespace core
} // namespace hydrogen

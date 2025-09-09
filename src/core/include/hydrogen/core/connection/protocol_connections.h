#pragma once

#include "unified_connection_architecture.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <queue>

namespace hydrogen {
namespace core {
namespace connection {

/**
 * @brief Base implementation for protocol connections
 */
class BaseProtocolConnection : public IProtocolConnection {
public:
    BaseProtocolConnection();
    virtual ~BaseProtocolConnection() = default;
    
    // Common interface implementation
    ConnectionState getState() const override;
    ConnectionStatistics getStatistics() const override;
    
    void setStateCallback(ConnectionStateCallback callback) override;
    void setMessageCallback(MessageReceivedCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;

protected:
    void updateState(ConnectionState newState, const std::string& error = "");
    void updateStatistics(bool messageReceived, bool messageSent, size_t bytes, bool error = false);
    void notifyMessage(const std::string& message);
    void notifyError(const std::string& error, int code = 0);
    
    std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
    ConnectionStatistics statistics_;
    mutable std::mutex statisticsMutex_;
    
    ConnectionStateCallback stateCallback_;
    MessageReceivedCallback messageCallback_;
    ErrorCallback errorCallback_;
    mutable std::mutex callbackMutex_;
};

/**
 * @brief WebSocket connection implementation
 */
class WebSocketConnection : public BaseProtocolConnection {
public:
    WebSocketConnection();
    ~WebSocketConnection() override;
    
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    
    bool sendMessage(const std::string& message) override;
    std::string receiveMessage() override;
    bool hasMessage() const override;

private:
    void startReceiveLoop();
    void handleReceive();
    void handleError(const boost::system::error_code& ec);
    
    boost::asio::io_context ioContext_;
    std::unique_ptr<boost::beast::websocket::stream<boost::asio::ip::tcp::socket>> websocket_;
    std::thread ioThread_;
    
    std::queue<std::string> incomingMessages_;
    mutable std::mutex messagesMutex_;
    
    ConnectionConfig config_;
    std::atomic<bool> running_{false};
};

/**
 * @brief HTTP connection implementation
 */
class HttpConnection : public BaseProtocolConnection {
public:
    HttpConnection();
    ~HttpConnection() override;
    
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    
    bool sendMessage(const std::string& message) override;
    std::string receiveMessage() override;
    bool hasMessage() const override;

private:
    void performHttpRequest(const std::string& message);
    
    boost::asio::io_context ioContext_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    
    std::queue<std::string> incomingMessages_;
    mutable std::mutex messagesMutex_;
    
    ConnectionConfig config_;
    std::atomic<bool> connected_{false};
};

/**
 * @brief gRPC connection implementation
 */
class GrpcConnection : public BaseProtocolConnection {
public:
    GrpcConnection();
    ~GrpcConnection() override;
    
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    
    bool sendMessage(const std::string& message) override;
    std::string receiveMessage() override;
    bool hasMessage() const override;

private:
    void initializeChannel();
    void startStreamingLoop();
    
    std::queue<std::string> incomingMessages_;
    mutable std::mutex messagesMutex_;
    
    ConnectionConfig config_;
    std::atomic<bool> connected_{false};
    std::thread streamingThread_;
};

/**
 * @brief STDIO connection implementation
 */
class StdioConnection : public BaseProtocolConnection {
public:
    StdioConnection();
    ~StdioConnection() override;
    
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    
    bool sendMessage(const std::string& message) override;
    std::string receiveMessage() override;
    bool hasMessage() const override;

private:
    void startInputLoop();
    void handleInput();
    
    std::queue<std::string> incomingMessages_;
    mutable std::mutex messagesMutex_;
    
    std::atomic<bool> running_{false};
    std::thread inputThread_;
};

/**
 * @brief FIFO connection implementation
 */
class FifoConnection : public BaseProtocolConnection {
public:
    FifoConnection();
    ~FifoConnection() override;
    
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    
    bool sendMessage(const std::string& message) override;
    std::string receiveMessage() override;
    bool hasMessage() const override;

private:
    bool createFifoPipes();
    void startReadLoop();
    void handleRead();
    
    std::string readPipePath_;
    std::string writePipePath_;
    
#ifdef _WIN32
    void* readHandle_ = nullptr;
    void* writeHandle_ = nullptr;
#else
    int readFd_ = -1;
    int writeFd_ = -1;
#endif
    
    std::queue<std::string> incomingMessages_;
    mutable std::mutex messagesMutex_;
    
    std::atomic<bool> running_{false};
    std::thread readThread_;
    ConnectionConfig config_;
};

/**
 * @brief TCP connection implementation
 */
class TcpConnection : public BaseProtocolConnection {
public:
    TcpConnection();
    ~TcpConnection() override;
    
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    
    bool sendMessage(const std::string& message) override;
    std::string receiveMessage() override;
    bool hasMessage() const override;

private:
    void startReceiveLoop();
    void handleReceive();
    
    boost::asio::io_context ioContext_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::thread ioThread_;
    
    std::queue<std::string> incomingMessages_;
    mutable std::mutex messagesMutex_;
    
    ConnectionConfig config_;
    std::atomic<bool> running_{false};
};

/**
 * @brief UDP connection implementation
 */
class UdpConnection : public BaseProtocolConnection {
public:
    UdpConnection();
    ~UdpConnection() override;
    
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    
    bool sendMessage(const std::string& message) override;
    std::string receiveMessage() override;
    bool hasMessage() const override;

private:
    void startReceiveLoop();
    void handleReceive();
    
    boost::asio::io_context ioContext_;
    std::unique_ptr<boost::asio::ip::udp::socket> socket_;
    boost::asio::ip::udp::endpoint remoteEndpoint_;
    std::thread ioThread_;
    
    std::queue<std::string> incomingMessages_;
    mutable std::mutex messagesMutex_;
    
    ConnectionConfig config_;
    std::atomic<bool> running_{false};
};

} // namespace connection
} // namespace core
} // namespace hydrogen

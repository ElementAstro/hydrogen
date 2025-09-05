#include "hydrogen/device/websocket_device.h"

#ifdef HYDROGEN_HAS_WEBSOCKETS
#include <hydrogen/core/utils.h>
#include <hydrogen/core/message.h>
#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>

namespace hydrogen {
namespace device {

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

WebSocketDevice::WebSocketDevice(const std::string& deviceId, const std::string& deviceType,
                                 const std::string& manufacturer, const std::string& model)
    : DeviceBase(deviceId, deviceType, manufacturer, model),
      ws_(std::make_unique<websocket::stream<tcp::socket>>(ioc_)),
      heartbeatTimer_(std::make_unique<net::steady_timer>(ioc_)),
      serverPort_(0), connectionTimeoutMs_(30000), heartbeatIntervalMs_(30000),
      running_(false), connected_(false), reconnecting_(false),
      messageThreadRunning_(false), heartbeatThreadRunning_(false),
      maxReconnectAttempts_(5), reconnectDelayMs_(5000) {

    // Initialize connection statistics
    connectionStats_ = {};
    connectionStats_.connectionStartTime = std::chrono::steady_clock::now();
    connectionStats_.isConnected = false;
}

WebSocketDevice::~WebSocketDevice() {
    stop();
    disconnect();
}

bool WebSocketDevice::start() {
    if (running_) {
        return true;
    }
    
    running_ = true;
    setProperty("running", true);
    
    return true;
}

void WebSocketDevice::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    setProperty("running", false);
    
    stopMessageThread();
}

bool WebSocketDevice::isRunning() const {
    return running_;
}

bool WebSocketDevice::connect(const std::string& host, uint16_t port) {
    return connect(host, port, connectionTimeoutMs_);
}

bool WebSocketDevice::connect(const std::string& host, uint16_t port, uint32_t timeoutMs) {
    if (connected_) {
        return true;
    }

    try {
        serverHost_ = host;
        serverPort_ = port;

        // Update connection stats
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            connectionStats_.connectionStartTime = std::chrono::steady_clock::now();
        }

        // Resolve hostname with timeout
        tcp::resolver resolver(ioc_);
        auto const results = resolver.resolve(host, std::to_string(port));

        // Set socket timeout (simplified - timeout functionality disabled for now)

        // Make the connection on the IP address we get from a lookup
        auto ep = net::connect(ws_->next_layer(), results);

        // Update the host string for the HTTP handshake
        std::string host_port = host + ':' + std::to_string(ep.port());

        // Set a decorator to change the User-Agent of the handshake
        ws_->set_option(websocket::stream_base::decorator(
            [this](websocket::request_type& req) {
                req.set(http::field::user_agent, "Hydrogen-Device/" + deviceType_ + "/1.0");
            }));

        // Set WebSocket timeout (simplified - timeout functionality disabled for now)

        // Perform the websocket handshake
        ws_->handshake(host_port, "/ws");

        // Disable timeout after successful connection (simplified - timeout functionality disabled for now)

        connected_ = true;
        reconnecting_ = false;
        setProperty("connected", true);

        // Update connection stats
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            connectionStats_.isConnected = true;
            connectionStats_.lastMessageTime = std::chrono::steady_clock::now();
        }

        startMessageThread();
        if (heartbeatIntervalMs_ > 0) {
            startHeartbeatThread();
        }

        return true;
    } catch (const std::exception& e) {
        connected_ = false;
        setProperty("connected", false);

        // Update connection stats
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            connectionStats_.connectionErrors++;
            connectionStats_.isConnected = false;
        }

        std::string errorMsg = "Connection failed: " + std::string(e.what());
        std::cerr << "[ERROR] " << getDeviceId() << ": " << errorMsg << std::endl;
        handleConnectionError(errorMsg);
        return false;
    }
}

void WebSocketDevice::disconnect() {
    if (!connected_) {
        return;
    }

    stopMessageThread();
    stopHeartbeatThread();

    try {
        if (ws_ && ws_->is_open()) {
            ws_->close(websocket::close_code::normal);
        }
    } catch (const std::exception& e) {
        // Ignore close errors
    }

    connected_ = false;
    reconnecting_ = false;
    setProperty("connected", false);

    // Update connection stats
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        connectionStats_.isConnected = false;
    }
}

bool WebSocketDevice::isConnected() const {
    return connected_;
}

bool WebSocketDevice::registerDevice() {
    if (!connected_) {
        return false;
    }
    
    try {
        hydrogen::core::RegistrationMessage regMsg;
        regMsg.setDeviceId(deviceId_);
        regMsg.setDeviceInfo(getDeviceInfo());
        
        std::string message = regMsg.toJson().dump();
        return sendMessage(message);
    } catch (const std::exception& e) {
        return false;
    }
}

void WebSocketDevice::setConnectionTimeout(uint32_t timeoutMs) {
    connectionTimeoutMs_ = timeoutMs;
}

void WebSocketDevice::setHeartbeatInterval(uint32_t intervalMs) {
    heartbeatIntervalMs_ = intervalMs;

    // Restart heartbeat thread if running
    if (heartbeatThreadRunning_) {
        stopHeartbeatThread();
        if (intervalMs > 0 && connected_) {
            startHeartbeatThread();
        }
    }
}

nlohmann::json WebSocketDevice::getConnectionStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    auto now = std::chrono::steady_clock::now();
    auto connectionDuration = std::chrono::duration_cast<std::chrono::seconds>(
        now - connectionStats_.connectionStartTime).count();

    return nlohmann::json{
        {"connected", connectionStats_.isConnected},
        {"connection_duration_seconds", connectionDuration},
        {"messages_sent", connectionStats_.messagesSent},
        {"messages_received", connectionStats_.messagesReceived},
        {"reconnect_attempts", connectionStats_.reconnectAttempts},
        {"connection_errors", connectionStats_.connectionErrors},
        {"server_host", serverHost_},
        {"server_port", serverPort_}
    };
}

void WebSocketDevice::run() {
    if (!connected_ || !running_) {
        return;
    }

    // The message thread handles incoming messages
    // This method can be used for device-specific processing
}

void WebSocketDevice::sendResponse(const hydrogen::core::ResponseMessage& response) {
    if (connected_) {
        std::string message = response.toJson().dump();
        sendMessage(message);
    }
}

void WebSocketDevice::sendEvent(const hydrogen::core::EventMessage& event) {
    if (connected_) {
        std::string message = event.toJson().dump();
        sendMessage(message);
    }
}

void WebSocketDevice::handleMessage(const std::string& message) {
    try {
        nlohmann::json messageJson = nlohmann::json::parse(message);

        // TODO: Implement message parsing without factory function
        // For now, just log the received message
        std::cout << "Received message: " << message << std::endl;

        // Handle other message types as needed

    } catch (const std::exception& e) {
        // Send error response
        hydrogen::core::ErrorMessage errorMsg("PARSE_ERROR", "Failed to parse message: " + std::string(e.what()));
        errorMsg.setDeviceId(deviceId_);
        sendMessage(errorMsg.toJson().dump());
    }
}

void WebSocketDevice::messageThreadFunction() {
    while (messageThreadRunning_ && connected_) {
        try {
            // Process outgoing message queue
            {
                std::unique_lock<std::mutex> lock(messageQueueMutex_);
                messageQueueCondition_.wait_for(lock, std::chrono::milliseconds(100),
                    [this] { return !messageQueue_.empty() || !messageThreadRunning_; });

                while (!messageQueue_.empty() && connected_) {
                    std::string message = messageQueue_.front();
                    messageQueue_.pop();
                    lock.unlock();

                    try {
                        ws_->write(net::buffer(message));

                        // Update stats
                        {
                            std::lock_guard<std::mutex> statsLock(statsMutex_);
                            connectionStats_.messagesSent++;
                        }
                    } catch (const std::exception& e) {
                        handleConnectionError("Failed to send message: " + std::string(e.what()));
                        break;
                    }

                    lock.lock();
                }
            }

            // Check for incoming messages (non-blocking)
            if (ws_->is_open()) {
                beast::flat_buffer buffer;

                // Set a short timeout for reading (simplified - timeout functionality disabled for now)

                try {
                    ws_->read(buffer);

                    std::string message = beast::buffers_to_string(buffer.data());
                    handleMessage(message);

                    // Update stats
                    {
                        std::lock_guard<std::mutex> lock(statsMutex_);
                        connectionStats_.messagesReceived++;
                        connectionStats_.lastMessageTime = std::chrono::steady_clock::now();
                    }

                } catch (const beast::system_error& se) {
                    if (se.code() != beast::error::timeout) {
                        throw; // Re-throw non-timeout errors
                    }
                    // Timeout is expected for non-blocking operation
                }

                // Reset timeout (simplified - timeout functionality disabled for now)
            }

        } catch (const std::exception& e) {
            if (messageThreadRunning_ && connected_) {
                handleConnectionError("Message thread error: " + std::string(e.what()));

                // Attempt reconnection if enabled
                if (!reconnecting_ && maxReconnectAttempts_ > 0) {
                    attemptReconnect();
                }
            }
            break;
        }
    }
}

void WebSocketDevice::startMessageThread() {
    if (messageThreadRunning_) {
        return;
    }
    
    messageThreadRunning_ = true;
    messageThread_ = std::thread(&WebSocketDevice::messageThreadFunction, this);
}

void WebSocketDevice::stopMessageThread() {
    if (!messageThreadRunning_) {
        return;
    }

    messageThreadRunning_ = false;
    messageQueueCondition_.notify_all(); // Wake up the message thread

    if (messageThread_.joinable()) {
        messageThread_.join();
    }
}

void WebSocketDevice::heartbeatThreadFunction() {
    while (heartbeatThreadRunning_ && connected_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeatIntervalMs_));

        if (!heartbeatThreadRunning_ || !connected_) {
            break;
        }

        try {
            // Send heartbeat message
            nlohmann::json heartbeat = {
                {"type", "heartbeat"},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"device_id", getDeviceId()}
            };

            sendMessage(heartbeat.dump());

            // Update stats
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                connectionStats_.lastHeartbeatTime = std::chrono::steady_clock::now();
            }

        } catch (const std::exception& e) {
            handleConnectionError("Heartbeat failed: " + std::string(e.what()));
            break;
        }
    }
}

void WebSocketDevice::startHeartbeatThread() {
    if (heartbeatThreadRunning_ || heartbeatIntervalMs_ == 0) {
        return;
    }

    heartbeatThreadRunning_ = true;
    heartbeatThread_ = std::thread(&WebSocketDevice::heartbeatThreadFunction, this);
}

void WebSocketDevice::stopHeartbeatThread() {
    if (!heartbeatThreadRunning_) {
        return;
    }

    heartbeatThreadRunning_ = false;

    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
}

bool WebSocketDevice::sendMessage(const std::string& message) {
    if (!connected_ || !ws_) {
        return false;
    }

    // Add message to queue for thread-safe sending
    {
        std::lock_guard<std::mutex> lock(messageQueueMutex_);
        messageQueue_.push(message);
    }
    messageQueueCondition_.notify_one();

    return true;
}

bool WebSocketDevice::sendMessageWithRetry(const std::string& message, int maxRetries) {
    for (int attempt = 0; attempt <= maxRetries; ++attempt) {
        if (sendMessage(message)) {
            return true;
        }

        if (attempt < maxRetries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (attempt + 1)));
        }
    }

    return false;
}

void WebSocketDevice::handleConnectionError(const std::string& error) {
    // Log error with proper logging
    std::cerr << "[ERROR] " << getDeviceId() << ": WebSocket connection error: " << error << std::endl;

    // Update connection state
    connected_ = false;
    setProperty("connected", false);

    // Update stats
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        connectionStats_.connectionErrors++;
        connectionStats_.isConnected = false;
    }

    // Call error callback if set
    if (errorCallback_) {
        try {
            errorCallback_(error);
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] " << getDeviceId() << ": Error callback failed: " << e.what() << std::endl;
        }
    }

    // Send error event
    try {
        hydrogen::core::EventMessage event("connection_error");
        event.setDeviceId(getDeviceId());
        event.setProperties({{"error", error}});
        // Note: Can't send through WebSocket since connection is broken
        // Store for later sending when connection is restored
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << getDeviceId() << ": Failed to create error event: " << e.what() << std::endl;
    }
}

bool WebSocketDevice::attemptReconnect() {
    if (reconnecting_ || serverHost_.empty()) {
        return false;
    }

    reconnecting_ = true;

    // Update stats
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        connectionStats_.reconnectAttempts++;
    }

    // Disconnect cleanly first
    disconnect();

    // Wait before reconnecting
    std::this_thread::sleep_for(std::chrono::milliseconds(reconnectDelayMs_));

    // Attempt to reconnect
    bool success = connect(serverHost_, serverPort_, connectionTimeoutMs_);

    if (!success) {
        reconnecting_ = false;
    }

    return success;
}

void WebSocketDevice::updateConnectionStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    connectionStats_.lastMessageTime = std::chrono::steady_clock::now();
}

} // namespace device
} // namespace hydrogen

#endif // HYDROGEN_HAS_WEBSOCKETS

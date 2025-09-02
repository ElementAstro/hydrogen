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
      serverPort_(0), messageThreadRunning_(false) {
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
    if (connected_) {
        return true;
    }
    
    try {
        serverHost_ = host;
        serverPort_ = port;
        
        // Resolve hostname
        tcp::resolver resolver(ioc_);
        auto const results = resolver.resolve(host, std::to_string(port));
        
        // Make the connection on the IP address we get from a lookup
        auto ep = net::connect(ws_->next_layer(), results);
        
        // Update the host string for the HTTP handshake
        std::string host_port = host + ':' + std::to_string(ep.port());
        
        // Set a decorator to change the User-Agent of the handshake
        ws_->set_option(websocket::stream_base::decorator(
            [this](websocket::request_type& req) {
                req.set(http::field::user_agent, "Hydrogen-Device/" + deviceType_ + "/1.0");
            }));
        
        // Perform the websocket handshake
        ws_->handshake(host_port, "/ws");
        
        connected_ = true;
        setProperty("connected", true);
        
        startMessageThread();
        
        return true;
    } catch (const std::exception& e) {
        connected_ = false;
        setProperty("connected", false);
        return false;
    }
}

void WebSocketDevice::disconnect() {
    if (!connected_) {
        return;
    }
    
    stopMessageThread();
    
    try {
        if (ws_ && ws_->is_open()) {
            ws_->close(websocket::close_code::normal);
        }
    } catch (const std::exception& e) {
        // Ignore close errors
    }
    
    connected_ = false;
    setProperty("connected", false);
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
            beast::flat_buffer buffer;
            ws_->read(buffer);
            
            std::string message = beast::buffers_to_string(buffer.data());
            handleMessage(message);
            
        } catch (const std::exception& e) {
            if (messageThreadRunning_ && connected_) {
                // Connection error
                connected_ = false;
                setProperty("connected", false);
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
    
    if (messageThread_.joinable()) {
        messageThread_.join();
    }
}

bool WebSocketDevice::sendMessage(const std::string& message) {
    if (!connected_ || !ws_) {
        return false;
    }
    
    try {
        ws_->write(net::buffer(message));
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace device
} // namespace hydrogen

#endif // HYDROGEN_HAS_WEBSOCKETS

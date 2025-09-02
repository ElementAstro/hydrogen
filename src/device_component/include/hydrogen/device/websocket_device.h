#pragma once

#include <hydrogen/core/device_interface.h>
#ifdef HYDROGEN_HAS_WEBSOCKETS
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#endif
#include <atomic>
#include <memory>
#include <thread>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace hydrogen {
namespace device {

#ifdef HYDROGEN_HAS_WEBSOCKETS
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace http = beast::http;
using tcp = net::ip::tcp;
#endif

/**
 * @class WebSocketDevice
 * @brief Base class for devices that communicate via WebSocket
 * 
 * This class extends DeviceBase to provide WebSocket communication
 * capabilities for connecting to device servers.
 */
class WebSocketDevice : public hydrogen::core::DeviceBase {
public:
    /**
     * @brief Constructor
     * @param deviceId Unique device identifier
     * @param deviceType Device type string
     * @param manufacturer Device manufacturer
     * @param model Device model
     */
    WebSocketDevice(const std::string& deviceId, const std::string& deviceType,
                    const std::string& manufacturer, const std::string& model);

    virtual ~WebSocketDevice();

    // IDevice interface implementation
    bool start() override;
    void stop() override;
    bool isRunning() const override;
    bool connect(const std::string& host, uint16_t port) override;
    void disconnect() override;
    bool isConnected() const override;
    bool registerDevice() override;

    /**
     * @brief Run the device message loop
     * 
     * This method starts the message processing loop and should be called
     * after connecting to the server.
     */
    virtual void run();

protected:
    // Override DeviceBase methods to use WebSocket communication
    void sendResponse(const hydrogen::core::ResponseMessage& response) override;
    void sendEvent(const hydrogen::core::EventMessage& event) override;

    /**
     * @brief Handle incoming WebSocket message
     * @param message Raw message string
     */
    virtual void handleMessage(const std::string& message);

    /**
     * @brief Message processing thread function
     */
    virtual void messageThreadFunction();

    /**
     * @brief Start the message processing thread
     */
    void startMessageThread();

    /**
     * @brief Stop the message processing thread
     */
    void stopMessageThread();

    /**
     * @brief Send a message through WebSocket
     * @param message Message to send
     * @return True if sent successfully
     */
    bool sendMessage(const std::string& message);

#ifdef HYDROGEN_HAS_WEBSOCKETS
    // WebSocket connection
    net::io_context ioc_;
    std::unique_ptr<websocket::stream<tcp::socket>> ws_;
#endif

    // Connection details
    std::string serverHost_;
    uint16_t serverPort_;

    // State management
    std::atomic<bool> running_;
    std::atomic<bool> connected_;

    // Threading
    std::thread messageThread_;
    std::atomic<bool> messageThreadRunning_;
};



} // namespace device
} // namespace hydrogen

#pragma once

#include <astrocomm/core/device_interface.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <atomic>
#include <memory>
#include <thread>

namespace astrocomm {
namespace device {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace astrocomm::core;

/**
 * @class WebSocketDevice
 * @brief Base class for devices that communicate via WebSocket
 * 
 * This class extends DeviceBase to provide WebSocket communication
 * capabilities for connecting to device servers.
 */
class WebSocketDevice : public DeviceBase {
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
    void sendResponse(const ResponseMessage& response) override;
    void sendEvent(const EventMessage& event) override;

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

    // WebSocket connection
    net::io_context ioc_;
    std::unique_ptr<websocket::stream<tcp::socket>> ws_;
    
    // Connection details
    std::string serverHost_;
    uint16_t serverPort_;
    
    // Threading
    std::thread messageThread_;
    std::atomic<bool> messageThreadRunning_;
};

} // namespace device
} // namespace astrocomm

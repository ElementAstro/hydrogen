#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

namespace astrocomm {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

/**
 * @brief Manages WebSocket connections and reconnection logic
 * 
 * This class is responsible for:
 * - Establishing and maintaining WebSocket connections
 * - Automatic reconnection with configurable parameters
 * - Connection state management
 * - Connection status reporting
 */
class ConnectionManager {
public:
    /**
     * @brief Callback function type for connection state changes
     * @param connected True if connected, false if disconnected
     */
    using ConnectionCallback = std::function<void(bool connected)>;

    ConnectionManager();
    ~ConnectionManager();

    /**
     * @brief Connect to a WebSocket server
     * @param host Server hostname or IP address
     * @param port Server port number
     * @return true if connection successful, false otherwise
     */
    bool connect(const std::string& host, uint16_t port);

    /**
     * @brief Disconnect from the server
     */
    void disconnect();

    /**
     * @brief Check if currently connected
     * @return true if connected, false otherwise
     */
    bool isConnected() const { return connected; }

    /**
     * @brief Configure automatic reconnection
     * @param enable Whether to enable auto-reconnect
     * @param intervalMs Interval between reconnection attempts in milliseconds
     * @param maxAttempts Maximum number of reconnection attempts (0 = infinite)
     */
    void setAutoReconnect(bool enable, int intervalMs = 5000, int maxAttempts = 0);

    /**
     * @brief Set callback for connection state changes
     * @param callback Function to call when connection state changes
     */
    void setConnectionCallback(ConnectionCallback callback);

    /**
     * @brief Get detailed connection status information
     * @return JSON object with connection status details
     */
    json getConnectionStatus() const;

    /**
     * @brief Get the WebSocket stream for message operations
     * @return Pointer to WebSocket stream, or nullptr if not connected
     */
    websocket::stream<tcp::socket>* getWebSocket() const;

    /**
     * @brief Get the IO context for async operations
     * @return Reference to the IO context
     */
    net::io_context& getIOContext() { return ioc; }

private:
    // WebSocket connection
    net::io_context ioc;
    std::unique_ptr<websocket::stream<tcp::socket>> ws;
    std::atomic<bool> connected;

    // Connection details for reconnection
    std::string lastHost;
    uint16_t lastPort;

    // Auto-reconnect configuration
    bool enableAutoReconnect;
    int reconnectIntervalMs;
    int maxReconnectAttempts;
    int reconnectCount;
    std::atomic<bool> reconnecting;
    std::thread reconnectThread;

    // Connection state callback
    ConnectionCallback connectionCallback;
    mutable std::mutex callbackMutex;

    // Synchronization
    mutable std::mutex connectionMutex;
    std::condition_variable reconnectCV;

    /**
     * @brief Reconnection loop thread function
     */
    void reconnectLoop();

    /**
     * @brief Attempt to reconnect to the last known server
     * @return true if reconnection successful, false otherwise
     */
    bool tryReconnect();

    /**
     * @brief Handle connection state changes
     * @param isConnected New connection state
     */
    void handleConnectionStateChange(bool isConnected);

    /**
     * @brief Reset internal state for reconnection
     */
    void resetState();

    /**
     * @brief Stop the reconnection thread
     */
    void stopReconnectThread();
};

} // namespace astrocomm

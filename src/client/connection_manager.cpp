#include "client/connection_manager.h"
#include <hydrogen/core/unified_websocket_error_handler.h>
#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

#ifdef _WIN32
#undef ERROR
#endif

namespace hydrogen {

ConnectionManager::ConnectionManager()
    : ioc(), connected(false), lastPort(0),
      enableAutoReconnect(true), reconnectIntervalMs(5000),
      maxReconnectAttempts(0), reconnectCount(0), reconnecting(false) {
  spdlog::debug("ConnectionManager initialized");
}

ConnectionManager::~ConnectionManager() {
  // Stop auto-reconnect
  enableAutoReconnect = false;
  reconnecting.store(false);
  reconnectCV.notify_all();
  
  // Stop reconnection thread
  stopReconnectThread();
  
  // Disconnect
  disconnect();
  
  spdlog::debug("ConnectionManager destroyed");
}

bool ConnectionManager::connect(const std::string& host, uint16_t port) {
  std::lock_guard<std::mutex> lock(connectionMutex);
  
  if (connected) {
    spdlog::warn("Already connected to {}:{}", lastHost, lastPort);
    return true;
  }

  try {
    // Save connection info for reconnection
    lastHost = host;
    lastPort = port;

    // Restart IO context if stopped
    if (ioc.stopped()) {
      ioc.restart();
    }

    // Create new WebSocket connection
    ws = std::make_unique<websocket::stream<tcp::socket>>(ioc);

    // Resolve server address
    tcp::resolver resolver{ioc};
    auto const results = resolver.resolve(host, std::to_string(port));

    // Connect to server
    auto ep = boost::asio::connect(ws->next_layer(), results);

    // Set WebSocket handshake options
    std::string host_port = host + ":" + std::to_string(port);
    ws->set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
          req.set(http::field::user_agent, "Hydrogen-ConnectionManager/1.0");
        }));

    // Perform WebSocket handshake
    ws->handshake(host_port, "/ws");

    // Update connection state
    bool wasConnected = connected.load();
    connected.store(true);

    // Reset reconnection state
    reconnectCount = 0;
    reconnecting.store(false);

    // Notify connection state change
    if (!wasConnected) {
      handleConnectionStateChange(true);
    }

    spdlog::info("Connected to server at {}:{}", host, port);
    return true;

  } catch (const beast::system_error& se) {
    spdlog::error("Connection error: {}", se.what());

    // Use unified error handling
    auto errorHandler = hydrogen::core::UnifiedWebSocketErrorRegistry::getInstance().getGlobalHandler();
    if (errorHandler) {
      hydrogen::core::WebSocketError error = hydrogen::core::WebSocketErrorFactory::createFromBoostError(
        se.code(), "ConnectionManager", "connect");
      errorHandler->handleError(error);
    }

    // Update connection state
    bool wasConnected = connected.load();
    connected.store(false);

    // Notify connection state change
    if (wasConnected) {
      handleConnectionStateChange(false);
    }

    // Clean up WebSocket resources
    ws.reset();

    return false;
  } catch (const std::exception& e) {
    spdlog::error("Connection error: {}", e.what());

    // Use unified error handling for generic exceptions
    auto errorHandler = hydrogen::core::UnifiedWebSocketErrorRegistry::getInstance().getGlobalHandler();
    if (errorHandler) {
      hydrogen::core::WebSocketError error = hydrogen::core::WebSocketErrorFactory::createFromException(
        e, "ConnectionManager", "connect");
      errorHandler->handleError(error);
    }

    // Update connection state
    bool wasConnected = connected.load();
    connected.store(false);

    // Notify connection state change
    if (wasConnected) {
      handleConnectionStateChange(false);
    }

    // Clean up WebSocket resources
    ws.reset();

    return false;
  }
}

void ConnectionManager::disconnect() {
  std::lock_guard<std::mutex> lock(connectionMutex);
  
  if (connected.load()) {
    try {
      if (ws) {
        // Close WebSocket connection gracefully
        ws->close(websocket::close_code::normal);
      }
      connected.store(false);
      spdlog::info("Disconnected from server");
    } catch (const beast::system_error& e) {
      if (e.code() != boost::asio::error::eof &&
          e.code() != boost::asio::error::connection_reset) {
        spdlog::error("Error disconnecting: {}", e.what());
      }
      connected.store(false);
    } catch (const std::exception& e) {
      spdlog::error("Error disconnecting: {}", e.what());
      connected.store(false);
    }

    // Clean up WebSocket resources
    ws.reset();

    // Stop IO context
    if (!ioc.stopped()) {
      ioc.stop();
    }

    // Notify connection state change
    handleConnectionStateChange(false);
  }
}

void ConnectionManager::setAutoReconnect(bool enable, int intervalMs, int maxAttempts) {
  std::lock_guard<std::mutex> lock(connectionMutex);
  
  enableAutoReconnect = enable;
  reconnectIntervalMs = intervalMs;
  maxReconnectAttempts = maxAttempts;
  
  spdlog::info("Auto-reconnect settings updated: enabled={}, interval={}ms, maxAttempts={}",
               enable, intervalMs, maxAttempts);

  if (!enable && reconnecting.load()) {
    reconnecting.store(false);
    reconnectCV.notify_all();
  }
}

void ConnectionManager::setConnectionCallback(ConnectionCallback callback) {
  std::lock_guard<std::mutex> lock(callbackMutex);
  connectionCallback = std::move(callback);
}

json ConnectionManager::getConnectionStatus() const {
  std::lock_guard<std::mutex> lock(connectionMutex);
  
  json status;
  status["connected"] = connected.load();
  status["host"] = lastHost;
  status["port"] = lastPort;
  status["autoReconnectEnabled"] = enableAutoReconnect;
  status["reconnecting"] = reconnecting.load();
  status["reconnectCount"] = reconnectCount;
  status["maxReconnectAttempts"] = maxReconnectAttempts;
  status["reconnectIntervalMs"] = reconnectIntervalMs;
  
  return status;
}

websocket::stream<tcp::socket>* ConnectionManager::getWebSocket() const {
  std::lock_guard<std::mutex> lock(connectionMutex);
  return connected.load() ? ws.get() : nullptr;
}

void ConnectionManager::handleConnectionStateChange(bool isConnected) {
  spdlog::info("Connection state changed: {}", isConnected ? "Connected" : "Disconnected");

  // Call user callback
  {
    std::lock_guard<std::mutex> lock(callbackMutex);
    if (connectionCallback) {
      try {
        connectionCallback(isConnected);
      } catch (const std::exception& e) {
        spdlog::error("Error in connection callback: {}", e.what());
      }
    }
  }

  // Start auto-reconnect if disconnected
  if (!isConnected && enableAutoReconnect) {
    bool alreadyReconnecting = reconnecting.exchange(true);
    if (!alreadyReconnecting) {
      spdlog::info("Auto-reconnect enabled. Starting reconnection process.");
      stopReconnectThread(); // Stop any existing thread
      reconnectThread = std::thread(&ConnectionManager::reconnectLoop, this);
    } else {
      spdlog::debug("Reconnection process already in progress.");
    }
  }

  if (isConnected) {
    reconnectCount = 0;
  }
}

void ConnectionManager::reconnectLoop() {
  spdlog::info("Reconnection loop started.");

  while (enableAutoReconnect && !connected.load() && reconnecting.load()) {
    reconnectCount++;

    std::string attemptLimitStr = (maxReconnectAttempts <= 0)
                                      ? "infinite"
                                      : std::to_string(maxReconnectAttempts);
    spdlog::info("Reconnection attempt {} of {}", reconnectCount, attemptLimitStr);

    if (tryReconnect()) {
      spdlog::info("Reconnection successful to {}:{}", lastHost, lastPort);
      return;
    }

    if (maxReconnectAttempts > 0 && reconnectCount >= maxReconnectAttempts) {
      spdlog::error("Maximum reconnection attempts ({}) reached. Stopping reconnection.",
                    maxReconnectAttempts);
      break;
    }

    spdlog::info("Reconnection attempt failed. Waiting {}ms before next attempt.",
                 reconnectIntervalMs);
    
    std::unique_lock<std::mutex> lock(connectionMutex);
    reconnectCV.wait_for(lock, std::chrono::milliseconds(reconnectIntervalMs),
                        [this] { return !reconnecting.load(); });

    if (!enableAutoReconnect || connected.load() || !reconnecting.load()) {
      break;
    }
  }

  if (!connected.load()) {
    spdlog::error("Reconnection failed or stopped after {} attempts.", reconnectCount);
  }

  reconnecting.store(false);
  spdlog::info("Reconnection loop finished.");
}

bool ConnectionManager::tryReconnect() {
  if (lastHost.empty() || lastPort == 0) {
    spdlog::error("Cannot reconnect: No previous connection information available.");
    return false;
  }

  spdlog::info("Attempting to reconnect to {}:{}", lastHost, lastPort);

  // Don't call disconnect() here as it would acquire the same lock
  // Just reset the connection state
  resetState();

  return connect(lastHost, lastPort);
}

void ConnectionManager::resetState() {
  // Reset WebSocket
  ws.reset();
  
  // Stop IO context
  if (!ioc.stopped()) {
    ioc.stop();
  }
  
  spdlog::debug("Connection state reset for reconnection.");
}

void ConnectionManager::stopReconnectThread() {
  if (reconnectThread.joinable()) {
    reconnectThread.join();
  }
}

} // namespace hydrogen

#include <hydrogen/client/device_client.h>
#include <hydrogen/core/utils.h>
#ifdef HYDROGEN_HAS_WEBSOCKETS
#include <hydrogen/core/unified_websocket_error_handler.h>
#endif
#include <atomic>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

#ifdef _WIN32
#undef ERROR
#endif

namespace hydrogen {
namespace client {

DeviceClient::DeviceClient()
    : ioc(), connected(false), running(false),
      ws(std::make_unique<beast::websocket::stream<tcp::socket>>(ioc)),
      autoReconnectEnabled(false), reconnectIntervalMs(5000), lastPort(0) {
}

DeviceClient::~DeviceClient() {
    disconnect();
}

bool DeviceClient::connect(const std::string &host, uint16_t port) {
    if (connected) {
        return true;
    }

    try {
        // Store connection details for auto-reconnect
        lastHost = host;
        lastPort = port;

        // Resolve hostname
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host, std::to_string(port));

        // Make the connection on the IP address we get from a lookup
        auto ep = net::connect(ws->next_layer(), results);

        // Update the host string for the HTTP handshake
        std::string host_port = host + ':' + std::to_string(ep.port());

        // Set a decorator to change the User-Agent of the handshake
        ws->set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(http::field::user_agent, "Hydrogen-Client/1.0");
            }));

        // Perform the websocket handshake
        ws->handshake(host_port, "/ws");

        connected = true;
        startMessageThread();

        if (connectionCallback) {
            connectionCallback(true);
        }

        return true;
    } catch (const std::exception& e) {
        connected = false;
        if (connectionCallback) {
            connectionCallback(false);
        }
        return false;
    }
}

void DeviceClient::disconnect() {
    if (!connected) {
        return;
    }

    connected = false;
    stopMessageThread();

    try {
        if (ws && ws->is_open()) {
            ws->close(websocket::close_code::normal);
        }
    } catch (const std::exception& e) {
        // Ignore close errors
    }

    if (connectionCallback) {
        connectionCallback(false);
    }
}

bool DeviceClient::isConnected() const {
    return connected;
}

json DeviceClient::discoverDevices(const std::vector<std::string> &deviceTypes) {
    auto request = std::make_shared<hydrogen::core::DiscoveryRequestMessage>();
    request->setDeviceTypes(deviceTypes);
    
    return sendMessage(request);
}

json DeviceClient::getDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    return devices;
}

json DeviceClient::getDeviceProperties(const std::string &deviceId,
                                     const std::vector<std::string> &properties) {
    auto command = std::make_shared<hydrogen::core::CommandMessage>("get_properties");
    command->setDeviceId(deviceId);
    
    if (!properties.empty()) {
        command->setParameters({{"properties", properties}});
    }
    
    return sendMessage(command);
}

json DeviceClient::setDeviceProperties(const std::string &deviceId, const json &properties) {
    auto command = std::make_shared<hydrogen::core::CommandMessage>("set_properties");
    command->setDeviceId(deviceId);
    command->setParameters({{"properties", properties}});
    
    return sendMessage(command);
}

json DeviceClient::executeCommand(const std::string &deviceId, const std::string &command,
                                const json &parameters) {
    auto commandMsg = std::make_shared<hydrogen::core::CommandMessage>(command);
    commandMsg->setDeviceId(deviceId);
    commandMsg->setParameters(parameters);
    
    return sendMessage(commandMsg);
}

json DeviceClient::sendMessage(std::shared_ptr<hydrogen::core::Message> message, int timeoutMs) {
    if (!connected) {
        return json{{"error", "Not connected"}};
    }

    std::string messageId = message->getMessageId();
    
    // Set up response waiting
    std::unique_lock<std::mutex> lock(responseMutex);
    
    try {
        // Send the message
        std::string messageStr = message->toJson().dump();
        ws->write(net::buffer(messageStr));
        
        // Wait for response
        bool received = responseCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                          [this, &messageId] {
                                              return responses.find(messageId) != responses.end();
                                          });
        
        if (received) {
            json response = responses[messageId];
            responses.erase(messageId);
            return response;
        } else {
            return json{{"error", "Timeout"}};
        }
    } catch (const std::exception& e) {
        return json{{"error", e.what()}};
    }
}

void DeviceClient::sendMessageAsync(std::shared_ptr<hydrogen::core::Message> message,
                                   std::function<void(const json &)> callback) {
    if (!connected) {
        callback(json{{"error", "Not connected"}});
        return;
    }

    std::string messageId = message->getMessageId();
    
    {
        std::lock_guard<std::mutex> lock(callbacksMutex);
        asyncCallbacks[messageId] = callback;
    }
    
    try {
        std::string messageStr = message->toJson().dump();
        ws->write(net::buffer(messageStr));
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(callbacksMutex);
        asyncCallbacks.erase(messageId);
        callback(json{{"error", e.what()}});
    }
}

void DeviceClient::subscribeToProperty(const std::string &deviceId, const std::string &property,
                                      PropertyCallback callback) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex);
    propertySubscriptions[deviceId][property] = callback;
}

void DeviceClient::unsubscribeFromProperty(const std::string &deviceId, const std::string &property) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex);
    auto deviceIt = propertySubscriptions.find(deviceId);
    if (deviceIt != propertySubscriptions.end()) {
        deviceIt->second.erase(property);
        if (deviceIt->second.empty()) {
            propertySubscriptions.erase(deviceIt);
        }
    }
}

void DeviceClient::subscribeToEvent(const std::string &deviceId, const std::string &eventType,
                                   EventCallback callback) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex);
    eventSubscriptions[deviceId][eventType] = callback;
}

void DeviceClient::unsubscribeFromEvent(const std::string &deviceId, const std::string &eventType) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex);
    auto deviceIt = eventSubscriptions.find(deviceId);
    if (deviceIt != eventSubscriptions.end()) {
        deviceIt->second.erase(eventType);
        if (deviceIt->second.empty()) {
            eventSubscriptions.erase(deviceIt);
        }
    }
}

void DeviceClient::setConnectionCallback(std::function<void(bool)> callback) {
    connectionCallback = callback;
}

void DeviceClient::setAuthToken(const std::string &token) {
    authToken = token;
}

std::string DeviceClient::getAuthToken() const {
    return authToken;
}

void DeviceClient::setAutoReconnect(bool enabled, int intervalMs) {
    autoReconnectEnabled = enabled;
    reconnectIntervalMs = intervalMs;
}

// Private methods
void DeviceClient::startMessageThread() {
    if (running) {
        return;
    }
    
    running = true;
    message_thread = std::thread(&DeviceClient::messageThreadFunction, this);
}

void DeviceClient::stopMessageThread() {
    if (!running) {
        return;
    }
    
    running = false;
    
    if (message_thread.joinable()) {
        message_thread.join();
    }
}

void DeviceClient::messageThreadFunction() {
    while (running && connected) {
        try {
            beast::flat_buffer buffer;
            ws->read(buffer);
            
            std::string message = beast::buffers_to_string(buffer.data());
            json messageJson = json::parse(message);
            
            handleMessage(messageJson);
        } catch (const beast::system_error& se) {
            if (running && connected) {
#ifdef HYDROGEN_HAS_WEBSOCKETS
                // Use unified error handling
                auto errorHandler = hydrogen::core::UnifiedWebSocketErrorRegistry::getInstance().getGlobalHandler();
                if (errorHandler) {
                    hydrogen::core::WebSocketError error = hydrogen::core::WebSocketErrorFactory::createFromBoostError(se.code(), "DeviceClient", "messageRead");
                    errorHandler->handleError(error);

                    // Determine if we should reconnect based on error handler recommendation
                    hydrogen::core::WebSocketRecoveryAction action = errorHandler->determineRecoveryAction(error);
                    if (action == hydrogen::core::WebSocketRecoveryAction::RECONNECT && autoReconnectEnabled) {
                        connected = false;
                        if (connectionCallback) {
                            connectionCallback(false);
                        }

                        auto retryDelay = errorHandler->getRetryDelay(error, 0);
                        std::this_thread::sleep_for(retryDelay);
                        connect(lastHost, lastPort);
                    }
                } else
#endif
                {
                    // Fallback to original behavior
                    connected = false;
                    if (connectionCallback) {
                        connectionCallback(false);
                    }

                    if (autoReconnectEnabled) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(reconnectIntervalMs));
                        connect(lastHost, lastPort);
                    }
                }
            }
            break;
        } catch (const std::exception& e) {
            if (running && connected) {
#ifdef HYDROGEN_HAS_WEBSOCKETS
                // Use unified error handling for generic exceptions
                auto errorHandler = hydrogen::core::UnifiedWebSocketErrorRegistry::getInstance().getGlobalHandler();
                if (errorHandler) {
                    hydrogen::core::WebSocketError error = hydrogen::core::WebSocketErrorFactory::createFromException(e, "DeviceClient", "messageRead");
                    errorHandler->handleError(error);

                    hydrogen::core::WebSocketRecoveryAction action = errorHandler->determineRecoveryAction(error);
                    if (action == hydrogen::core::WebSocketRecoveryAction::RECONNECT && autoReconnectEnabled) {
                        connected = false;
                        if (connectionCallback) {
                            connectionCallback(false);
                        }

                        auto retryDelay = errorHandler->getRetryDelay(error, 0);
                        std::this_thread::sleep_for(retryDelay);
                        connect(lastHost, lastPort);
                    }
                } else
#endif
                {
                    // Fallback to original behavior
                    connected = false;
                    if (connectionCallback) {
                        connectionCallback(false);
                    }

                    if (autoReconnectEnabled) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(reconnectIntervalMs));
                        connect(lastHost, lastPort);
                    }
                }
            }
            break;
        }
    }
}

void DeviceClient::handleMessage(const json &message) {
    try {
        std::string messageType = message.value("messageType", "");
        
        if (messageType == "RESPONSE") {
            handleResponse(message);
        } else if (messageType == "EVENT") {
            handleEvent(message);
        } else if (messageType == "DISCOVERY_RESPONSE") {
            handleResponse(message);
        }
    } catch (const std::exception& e) {
        // Log error handling message
    }
}

void DeviceClient::handleResponse(const json &response) {
    std::string originalMessageId = response.value("originalMessageId", "");
    
    if (!originalMessageId.empty()) {
        // Check for async callback
        {
            std::lock_guard<std::mutex> lock(callbacksMutex);
            auto it = asyncCallbacks.find(originalMessageId);
            if (it != asyncCallbacks.end()) {
                it->second(response);
                asyncCallbacks.erase(it);
                return;
            }
        }
        
        // Store response for synchronous waiting
        {
            std::lock_guard<std::mutex> lock(responseMutex);
            responses[originalMessageId] = response;
        }
        responseCV.notify_all();
    }
}

void DeviceClient::handleEvent(const json &event) {
    std::string deviceId = event.value("deviceId", "");
    std::string eventType = event.value("event", "");
    
    if (!deviceId.empty() && !eventType.empty()) {
        notifyEvent(deviceId, eventType, event);
    }
}

void DeviceClient::notifyPropertyChange(const std::string &deviceId, const std::string &property, const json &value) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex);
    auto deviceIt = propertySubscriptions.find(deviceId);
    if (deviceIt != propertySubscriptions.end()) {
        auto propertyIt = deviceIt->second.find(property);
        if (propertyIt != deviceIt->second.end()) {
            propertyIt->second(deviceId, value);
        }
    }
}

void DeviceClient::notifyEvent(const std::string &deviceId, const std::string &eventType, const json &data) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex);
    auto deviceIt = eventSubscriptions.find(deviceId);
    if (deviceIt != eventSubscriptions.end()) {
        auto eventIt = deviceIt->second.find(eventType);
        if (eventIt != deviceIt->second.end()) {
            eventIt->second(deviceId, data);
        }
    }
}

std::string DeviceClient::generateMessageId() {
    return hydrogen::core::generateUuid();
}

} // namespace client
} // namespace hydrogen

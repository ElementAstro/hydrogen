#include "astrocomm/server/device_server.h"
#include <astrocomm/core/utils.h>
#include <crow/app.h>
#include <crow/http_request.h>
#include <crow/http_response.h>
#include <filesystem>
#include <fstream>

namespace astrocomm {
namespace server {

namespace fs = std::filesystem;

/**
 * @brief Default constructor.
 */
DeviceServer::DeviceServer()
    : serverPort(8000), 
      configPath("./data/server_config.json"),
      port(8000), 
      configDirectory("./data/devices"), 
      autosaveInterval(300), 
      running(false) {
    
    deviceManager = std::make_unique<DeviceManager>(configDirectory, autosaveInterval);
    authManager = std::make_unique<AuthManager>();
    errorManager = std::make_unique<ErrorRecoveryManager>();
}

/**
 * @brief Constructor with configuration options.
 */
DeviceServer::DeviceServer(int port, const std::string& persistenceDir, int autoSaveInterval)
    : serverPort(port), 
      port(port), 
      configDirectory(persistenceDir), 
      autosaveInterval(autoSaveInterval), 
      running(false) {
    
    configPath = persistenceDir + "/server_config.json";
    deviceManager = std::make_unique<DeviceManager>(persistenceDir, autoSaveInterval);
    authManager = std::make_unique<AuthManager>();
    errorManager = std::make_unique<ErrorRecoveryManager>();
}

void DeviceServer::start(bool loadPreviousConfig) {
    if (running) {
        return;
    }

    running = true;

    // Load previous configuration if requested
    if (loadPreviousConfig) {
        loadConfiguration();
        deviceManager->loadConfiguration();
    }

    // Setup WebSocket and REST API handlers
    setupWebSocketHandlers();
    setupRestApi();

    // Start heartbeat monitoring if enabled
    if (heartbeatInterval > 0) {
        startHeartbeatCheck();
    }

    // Start the Crow server
    app.port(serverPort).multithreaded().run_async();
}

void DeviceServer::stop() {
    if (!running) {
        return;
    }

    running = false;

    // Stop heartbeat monitoring
    stopHeartbeatCheck();

    // Save configuration
    saveConfiguration();
    deviceManager->saveConfiguration();

    // Stop the Crow server
    app.stop();
}

void DeviceServer::setupWebSocketHandlers() {
    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([this](crow::websocket::connection& conn) {
            handleWebSocketOpen(conn);
        })
        .onclose([this](crow::websocket::connection& conn, const std::string& reason, uint16_t status_code) {
            handleWebSocketClose(conn);
        })
        .onmessage([this](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
            handleWebSocketMessage(conn, data, is_binary);
        });
}

void DeviceServer::setupRestApi() {
    // Device management endpoints
    CROW_ROUTE(app, "/api/devices").methods("GET"_method)
    ([this](const crow::request& req) {
        auto devices = deviceManager->getAllDevices();
        return crow::response(200, devices.dump());
    });

    CROW_ROUTE(app, "/api/devices/<string>").methods("GET"_method)
    ([this](const crow::request& req, const std::string& deviceId) {
        auto device = deviceManager->getDeviceInfo(deviceId);
        if (device.is_null()) {
            return crow::response(404, "Device not found");
        }
        return crow::response(200, device.dump());
    });

    // Authentication endpoints
    CROW_ROUTE(app, "/api/auth/login").methods("POST"_method)
    ([this](const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string username = body["username"];
            std::string password = body["password"];
            
            std::string token = authManager->authenticate(username, password, req.get_header_value("X-Real-IP"));
            if (token.empty()) {
                return crow::response(401, "Authentication failed");
            }
            
            nlohmann::json response = {{"token", token}};
            return crow::response(200, response.dump());
        } catch (const std::exception& e) {
            return crow::response(400, "Invalid request");
        }
    });

    // Server status endpoint
    CROW_ROUTE(app, "/api/status").methods("GET"_method)
    ([this](const crow::request& req) {
        nlohmann::json status = {
            {"running", running},
            {"port", serverPort},
            {"devices", deviceManager->getAllDevices().size()},
            {"uptime", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - std::chrono::steady_clock::now()).count()}
        };
        return crow::response(200, status.dump());
    });
}

void DeviceServer::handleWebSocketOpen(crow::websocket::connection& conn) {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    // Generate a unique client ID for this connection
    std::string clientId = astrocomm::core::generateUuid();
    clientConnections[&conn] = clientId;
}

void DeviceServer::handleWebSocketClose(crow::websocket::connection& conn) {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    auto it = clientConnections.find(&conn);
    if (it != clientConnections.end()) {
        clientConnections.erase(it);
    }
    
    auto deviceIt = deviceConnections.find(&conn);
    if (deviceIt != deviceConnections.end()) {
        deviceManager->setDeviceConnectionStatus(deviceIt->second, false);
        deviceConnections.erase(deviceIt);
    }
}

void DeviceServer::handleWebSocketMessage(crow::websocket::connection& conn, 
                                        const std::string& data, bool is_binary) {
    try {
        auto json_data = nlohmann::json::parse(data);
        auto message = createMessageFromJson(json_data);
        
        // Handle the message based on its type
        auto messageType = message->getMessageType();
        auto handlerIt = messageHandlers.find(messageType);
        if (handlerIt != messageHandlers.end()) {
            handlerIt->second(std::shared_ptr<astrocomm::core::Message>(message.release()), conn);
        }
    } catch (const std::exception& e) {
        // Send error response
        ErrorMessage errorMsg("PARSE_ERROR", "Failed to parse message");
        conn.send_text(errorMsg.toJson().dump());
    }
}

void DeviceServer::setMessageHandler(MessageType type, MessageHandler handler) {
    messageHandlers[type] = handler;
}

bool DeviceServer::loadConfiguration() {
    try {
        if (!fs::exists(configPath)) {
            return false;
        }
        
        std::ifstream file(configPath);
        nlohmann::json config;
        file >> config;
        
        if (config.contains("port")) {
            serverPort = config["port"];
        }
        if (config.contains("heartbeatInterval")) {
            heartbeatInterval = config["heartbeatInterval"];
        }
        if (config.contains("accessControlEnabled")) {
            accessControlEnabled = config["accessControlEnabled"];
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool DeviceServer::saveConfiguration() {
    try {
        // Ensure directory exists
        fs::create_directories(fs::path(configPath).parent_path());
        
        nlohmann::json config = {
            {"port", serverPort},
            {"heartbeatInterval", heartbeatInterval},
            {"accessControlEnabled", accessControlEnabled},
            {"commandQueueEnabled", commandQueueEnabled}
        };
        
        std::ofstream file(configPath);
        file << config.dump(4);
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void DeviceServer::startHeartbeatCheck() {
    if (heartbeatRunning) {
        return;
    }
    
    heartbeatRunning = true;
    heartbeatThread = std::thread(&DeviceServer::heartbeatThreadFunction, this);
}

void DeviceServer::stopHeartbeatCheck() {
    if (!heartbeatRunning) {
        return;
    }
    
    heartbeatRunning = false;
    if (heartbeatThread.joinable()) {
        heartbeatThread.join();
    }
}

void DeviceServer::heartbeatThreadFunction() {
    while (heartbeatRunning && running) {
        // Send heartbeat to all connected devices
        std::this_thread::sleep_for(std::chrono::seconds(heartbeatInterval));
        
        // Implementation would send heartbeat messages to connected devices
        // and check for responses
    }
}

void DeviceServer::setServerId(const std::string& serverId) {
    this->serverId = serverId;
}

nlohmann::json DeviceServer::getServerStats() const {
    return nlohmann::json{
        {"running", running},
        {"port", serverPort},
        {"connectedDevices", deviceManager->getConnectedDevices().size()},
        {"totalDevices", deviceManager->getAllDevices().size()},
        {"activeConnections", clientConnections.size()}
    };
}

void DeviceServer::addUserDevicePermission(const std::string& clientId, const std::string& deviceId) {
    userDevicePermissions[clientId].push_back(deviceId);
}

void DeviceServer::removeUserDevicePermission(const std::string& clientId, const std::string& deviceId) {
    auto& permissions = userDevicePermissions[clientId];
    permissions.erase(std::remove(permissions.begin(), permissions.end(), deviceId), permissions.end());
}

bool DeviceServer::hasDeviceAccess(const std::string& clientId, const std::string& deviceId) {
    if (!accessControlEnabled) {
        return true;
    }
    
    auto it = userDevicePermissions.find(clientId);
    if (it == userDevicePermissions.end()) {
        return false;
    }
    
    const auto& permissions = it->second;
    return std::find(permissions.begin(), permissions.end(), deviceId) != permissions.end();
}

bool DeviceServer::checkRateLimit(const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(rateLimitMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto it = lastRequestTimes.find(ipAddress);
    
    if (it == lastRequestTimes.end()) {
        lastRequestTimes[ipAddress] = now;
        return true;
    }
    
    auto timeDiff = std::chrono::duration_cast<std::chrono::minutes>(now - it->second);
    if (timeDiff.count() >= 1) {
        it->second = now;
        return true;
    }
    
    return false;
}

} // namespace server
} // namespace astrocomm

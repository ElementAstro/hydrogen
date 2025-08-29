#include "server/device_server.h"
#include <crow/app.h>
#include <crow/http_request.h>
#include <crow/http_response.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

namespace astrocomm {

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
    errorManager = std::make_unique<ErrorRecoveryManager>(); // 初始化错误恢复管理器
    spdlog::info("[DeviceServer] Server initialized with default settings");
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
    errorManager = std::make_unique<ErrorRecoveryManager>(); // 初始化错误恢复管理器
    spdlog::info("[DeviceServer] Server initialized with custom settings (port: {}, persistence: {})",
                 port, persistenceDir);
}

/**
 * @brief Start the server.
 */
void DeviceServer::start(bool loadPreviousConfig) {
    if (running) {
        spdlog::warn("[DeviceServer] Server already running");
        return;
    }
    
    spdlog::info("[DeviceServer] Starting server on port {}", port);
    
    // Ensure configuration directory exists
    if (!fs::exists(configDirectory)) {
        spdlog::info("[DeviceServer] Creating persistence directory: {}", configDirectory);
        fs::create_directories(configDirectory);
    }
    
    // Load previous device configuration if requested
    if (loadPreviousConfig) {
        std::string configFile = configDirectory + "/devices.json";
        if (fs::exists(configFile)) {
            spdlog::info("[DeviceServer] Loading previous device configuration");
            deviceManager->loadDeviceConfiguration(configFile);
        } else {
            spdlog::info("[DeviceServer] No previous configuration found at {}", configFile);
        }
        
        // Also load server configuration if available
        if (fs::exists(configPath)) {
            loadConfiguration();
        }
    }
    
    // Setup API routes
    setupRoutes();
    
    // Start heartbeat check
    startHeartbeatCheck();
    
    // Mark as running
    running = true;
    
    // Start the server
    app.port(port).run();
}

/**
 * @brief Stop the server.
 */
void DeviceServer::stop() {
    if (!running) {
        spdlog::warn("[DeviceServer] Server not running");
        return;
    }
    
    spdlog::info("[DeviceServer] Stopping server");
    
    // Stop heartbeat check
    stopHeartbeatCheck();
    
    // Save current device configuration
    std::string configFile = configDirectory + "/devices.json";
    deviceManager->saveDeviceConfiguration(configFile);
    
    // Save server configuration
    saveConfiguration();
    
    // Stop the server
    app.stop();
    
    // Mark as not running
    running = false;
    
    spdlog::info("[DeviceServer] Server stopped");
}

/**
 * @brief Setup API routes for the server.
 */
void DeviceServer::setupRoutes() {
    // Setup REST API routes
    setupRestApi();
    
    // Handle WebSocket connections
    CROW_ROUTE(app, "/api/v1/ws")
        .websocket()
        .onopen([this](crow::websocket::connection& conn) {
            handleWebSocketOpen(conn);
        })
        .onclose([this](crow::websocket::connection& conn) {
            handleWebSocketClose(conn);
        })
        .onmessage([this](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
            handleWebSocketMessage(conn, data, is_binary);
        });
        
    spdlog::info("[DeviceServer] API routes configured");
}

/**
 * @brief Setup REST API routes.
 */
void DeviceServer::setupRestApi() {
    // API Root - Server info
    CROW_ROUTE(app, "/api/v1")
    ([this]() {
        json serverInfo = {
            {"name", "Astro Device Server"},
            {"version", "1.0.0"},
            {"persistence", {
                {"enabled", true},
                {"directory", configDirectory},
                {"autosaveInterval", autosaveInterval}
            }}
        };
        return crow::response(200, serverInfo.dump(2));
    });
    
    // Get all devices
    CROW_ROUTE(app, "/api/v1/devices")
    ([this](const crow::request& req) {
        if (!authenticate(req)) {
            return crow::response(401, "{\"error\": \"Unauthorized\"}");
        }
        
        std::vector<std::string> deviceTypes;
        if (req.url_params.get("type")) {
            deviceTypes.push_back(req.url_params.get("type"));
        }
        
        json devices = deviceManager->getDevices(deviceTypes);
        return crow::response(200, devices.dump(2));
    });
    
    // Get device info
    CROW_ROUTE(app, "/api/v1/devices/<string>")
    ([this](const crow::request& req, const std::string& deviceId) {
        if (!authenticate(req)) {
            return crow::response(401, "{\"error\": \"Unauthorized\"}");
        }
        
        try {
            json deviceInfo = deviceManager->getDeviceInfo(deviceId);
            return crow::response(200, deviceInfo.dump(2));
        } catch (const std::runtime_error& e) {
            return crow::response(404, "{\"error\": \"Device not found\"}");
        }
    });
    
    // Get device property
    CROW_ROUTE(app, "/api/v1/devices/<string>/properties/<string>")
    ([this](const crow::request& req, const std::string& deviceId, const std::string& property) {
        if (!authenticate(req)) {
            return crow::response(401, "{\"error\": \"Unauthorized\"}");
        }
        
        try {
            json propertyValue = deviceManager->getDeviceProperty(deviceId, property);
            return crow::response(200, propertyValue.dump(2));
        } catch (const std::runtime_error& e) {
            return crow::response(404, "{\"error\": \"Device or property not found\"}");
        }
    });
    
    // Update device property
    CROW_ROUTE(app, "/api/v1/devices/<string>/properties/<string>")
    .methods("PUT"_method)
    ([this](const crow::request& req, const std::string& deviceId, const std::string& property) {
        if (!authenticate(req)) {
            return crow::response(401, "{\"error\": \"Unauthorized\"}");
        }
        
        try {
            json value = json::parse(req.body);
            deviceManager->updateDeviceProperty(deviceId, property, value);
            return crow::response(200, "{\"status\": \"success\"}");
        } catch (const std::runtime_error& e) {
            return crow::response(404, "{\"error\": \"Device not found\"}");
        } catch (const json::exception& e) {
            return crow::response(400, "{\"error\": \"Invalid JSON\"}");
        }
    });
    
    // Get device status
    CROW_ROUTE(app, "/api/v1/status")
    ([this](const crow::request& req) {
        if (!authenticate(req)) {
            return crow::response(401, "{\"error\": \"Unauthorized\"}");
        }
        
        json status = deviceManager->getDeviceStatus();
        return crow::response(200, status.dump(2));
    });
    
    // Create a configuration backup
    CROW_ROUTE(app, "/api/v1/backup")
    .methods("POST"_method)
    ([this](const crow::request& req) {
        if (!authenticate(req)) {
            return crow::response(401, "{\"error\": \"Unauthorized\"}");
        }
        
        std::string backupDir = configDirectory + "/backups";
        bool success = deviceManager->backupConfiguration(backupDir);
        
        if (success) {
            return crow::response(200, "{\"status\": \"success\", \"backupDir\": \"" + backupDir + "\"}");
        } else {
            return crow::response(500, "{\"error\": \"Failed to create backup\"}");
        }
    });
    
    // Restore from backup
    CROW_ROUTE(app, "/api/v1/restore")
    .methods("POST"_method)
    ([this](const crow::request& req) {
        if (!authenticate(req)) {
            return crow::response(401, "{\"error\": \"Unauthorized\"}");
        }
        
        try {
            json body = json::parse(req.body);
            if (!body.contains("backupFile")) {
                return crow::response(400, "{\"error\": \"Missing backupFile parameter\"}");
            }
            
            std::string backupFile = body["backupFile"];
            bool success = deviceManager->restoreFromBackup(backupFile);
            
            if (success) {
                return crow::response(200, "{\"status\": \"success\"}");
            } else {
                return crow::response(500, "{\"error\": \"Failed to restore from backup\"}");
            }
        } catch (const json::exception& e) {
            return crow::response(400, "{\"error\": \"Invalid JSON\"}");
        }
    });
    
    // Configure autosave settings
    CROW_ROUTE(app, "/api/v1/config/autosave")
    .methods("POST"_method)
    ([this](const crow::request& req) {
        if (!authenticate(req)) {
            return crow::response(401, "{\"error\": \"Unauthorized\"}");
        }
        
        try {
            json body = json::parse(req.body);
            bool enabled = body.contains("enabled") ? body["enabled"].get<bool>() : true;
            int interval = body.contains("intervalSeconds") ? body["intervalSeconds"].get<int>() : 300;
            std::string directory = body.contains("directory") ? body["directory"].get<std::string>() : configDirectory;
            
            deviceManager->configureAutosave(enabled, interval, directory);
            autosaveInterval = interval;
            configDirectory = directory;
            
            return crow::response(200, "{\"status\": \"success\"}");
        } catch (const json::exception& e) {
            return crow::response(400, "{\"error\": \"Invalid JSON\"}");
        }
    });
}

/**
 * @brief Save server configuration to file.
 */
bool DeviceServer::saveConfiguration() {
    try {
        json config = {
            {"serverPort", serverPort},
            {"heartbeatInterval", heartbeatInterval},
            {"accessControlEnabled", accessControlEnabled},
            {"commandQueueEnabled", commandQueueEnabled},
            {"requestsPerMinute", requestsPerMinute},
            {"persistenceConfig", {
                {"directory", configDirectory},
                {"autosaveInterval", autosaveInterval}
            }}
        };
        
        // Add user permissions if access control is enabled
        if (accessControlEnabled) {
            config["userPermissions"] = userDevicePermissions;
        }
        
        // Ensure directory exists
        fs::path configFilePath(configPath);
        fs::create_directories(configFilePath.parent_path());
        
        // Write config to file
        std::ofstream file(configPath);
        if (!file.is_open()) {
            spdlog::error("[DeviceServer] Failed to open config file for writing: {}", configPath);
            return false;
        }
        
        file << config.dump(2);
        file.close();
        
        spdlog::info("[DeviceServer] Server configuration saved to {}", configPath);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[DeviceServer] Error saving server configuration: {}", e.what());
        return false;
    }
}

/**
 * @brief Load server configuration from file.
 */
bool DeviceServer::loadConfiguration() {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            spdlog::warn("[DeviceServer] Failed to open config file: {}", configPath);
            return false;
        }
        
        json config;
        file >> config;
        file.close();
        
        // Load server settings
        serverPort = config.contains("serverPort") ? config["serverPort"].get<uint16_t>() : serverPort;
        heartbeatInterval = config.contains("heartbeatInterval") ? config["heartbeatInterval"].get<int>() : heartbeatInterval;
        accessControlEnabled = config.contains("accessControlEnabled") ? config["accessControlEnabled"].get<bool>() : accessControlEnabled;
        commandQueueEnabled = config.contains("commandQueueEnabled") ? config["commandQueueEnabled"].get<bool>() : commandQueueEnabled;
        requestsPerMinute = config.contains("requestsPerMinute") ? config["requestsPerMinute"].get<int>() : requestsPerMinute;
        
        // Load persistence configuration
        if (config.contains("persistenceConfig")) {
            if (config["persistenceConfig"].contains("directory")) {
                configDirectory = config["persistenceConfig"]["directory"].get<std::string>();
            }
            if (config["persistenceConfig"].contains("autosaveInterval")) {
                autosaveInterval = config["persistenceConfig"]["autosaveInterval"].get<int>();
            }
        }
        
        // Load user permissions if access control is enabled
        if (accessControlEnabled && config.contains("userPermissions")) {
            userDevicePermissions = config["userPermissions"].get<std::unordered_map<std::string, std::vector<std::string>>>();
        }
        
        spdlog::info("[DeviceServer] Server configuration loaded from {}", configPath);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[DeviceServer] Error loading server configuration: {}", e.what());
        return false;
    }
}

/**
 * @brief Start heartbeat check.
 */
void DeviceServer::startHeartbeatCheck() {
    if (heartbeatRunning) {
        return;
    }
    
    heartbeatRunning = true;
    heartbeatThread = std::thread(&DeviceServer::heartbeatThreadFunction, this);
    spdlog::info("[DeviceServer] Heartbeat check started (interval: {}s)", heartbeatInterval);
}

/**
 * @brief Stop heartbeat check.
 */
void DeviceServer::stopHeartbeatCheck() {
    if (!heartbeatRunning) {
        return;
    }
    
    heartbeatRunning = false;
    if (heartbeatThread.joinable()) {
        heartbeatThread.join();
    }
    
    spdlog::info("[DeviceServer] Heartbeat check stopped");
}

/**
 * @brief Heartbeat thread function.
 */
void DeviceServer::heartbeatThreadFunction() {
    while (heartbeatRunning) {
        // Sleep for the specified interval
        std::this_thread::sleep_for(std::chrono::seconds(heartbeatInterval));
        
        if (!heartbeatRunning) {
            break;
        }
        
        // Check device status
        deviceManager->checkDeviceStatus(heartbeatInterval * 2);
        
        // Send heartbeat to all connected devices
        HeartbeatMessage heartbeat;
        broadcastToDevices(heartbeat);
        
        // Process command queue if enabled
        if (commandQueueEnabled) {
            processCommandQueue();
        }
    }
}

/**
 * @brief Process command queue.
 */
void DeviceServer::processCommandQueue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    
    if (commandQueue.empty()) {
        return;
    }
    
    spdlog::debug("[DeviceServer] Processing command queue ({} items)", commandQueue.size());
    
    // Sort by priority (higher values first)
    std::sort(commandQueue.begin(), commandQueue.end(), 
              [](const CommandMessage& a, const CommandMessage& b) {
                  return a.getPriority() > b.getPriority();
              });
    
    // Process commands
    for (auto& cmd : commandQueue) {
        try {
            // Try to forward the command to the target device
            forwardToDevice(cmd.getDeviceId(), cmd);
        } catch (const std::exception& e) {
            spdlog::error("[DeviceServer] Error processing queued command: {}", e.what());
        }
    }
    
    // Clear the queue
    commandQueue.clear();
}

/**
 * @brief Add command to queue.
 */
void DeviceServer::addCommandToQueue(const CommandMessage& cmd) {
    if (!commandQueueEnabled) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(queueMutex);
    commandQueue.push_back(cmd);
    
    spdlog::debug("[DeviceServer] Command added to queue for device {}", cmd.getDeviceId());
}

/**
 * @brief Middleware for authentication.
 */
bool DeviceServer::authenticate(const crow::request& req) {
    // Skip authentication if no auth manager is available
    if (!authManager) {
        return true;
    }
    
    // Check rate limit
    if (!checkRateLimit(req.remote_ip)) {
        spdlog::warn("[DeviceServer] Rate limit exceeded for IP: {}", req.remote_ip);
        return false;
    }
    
    // Get authentication token from header or query param
    std::string token;
    
    // Check Authorization header
    auto authHeader = req.headers.find("Authorization");
    if (authHeader != req.headers.end()) {
        std::string authValue = authHeader->second;
        if (authValue.substr(0, 7) == "Bearer ") {
            token = authValue.substr(7);
        }
    }
    
    // If no token in header, check query params
    if (token.empty()) {
        token = req.url_params.get("token") ? req.url_params.get("token") : "";
    }
    
    // Verify token
    if (!token.empty()) {
        return authManager->verifyToken(token);
    }
    
    return false;
}

/**
 * @brief Check rate limit for an IP address.
 */
bool DeviceServer::checkRateLimit(const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(rateLimitMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto it = lastRequestTimes.find(ipAddress);
    
    if (it != lastRequestTimes.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        double rate = 60000.0 / elapsed; // Requests per minute
        
        // Update timestamp
        it->second = now;
        
        // Check if rate exceeds limit
        if (rate > requestsPerMinute) {
            return false;
        }
    } else {
        // First request from this IP
        lastRequestTimes[ipAddress] = now;
    }
    
    return true;
}

/**
 * @brief Check if user has access to a device.
 */
bool DeviceServer::hasDeviceAccess(const std::string& clientId, const std::string& deviceId) {
    // Skip check if access control is disabled
    if (!accessControlEnabled) {
        return true;
    }
    
    auto it = userDevicePermissions.find(clientId);
    if (it != userDevicePermissions.end()) {
        // Check if the device is in the allowed list
        for (const auto& allowedDevice : it->second) {
            if (allowedDevice == deviceId || allowedDevice == "*") {
                return true;
            }
        }
    }
    
    return false;
}

/**
 * @brief Enable or disable distributed mode.
 */
void DeviceServer::enableDistributedMode(bool enabled, uint16_t discoveryPort,
                                   const std::string &multicastGroup) {
  if (deviceManager) {
    deviceManager->enableDistributedMode(enabled, discoveryPort, multicastGroup);
  }
}

/**
 * @brief Set the server ID.
 */
void DeviceServer::setServerId(const std::string &serverId) {
  if (deviceManager) {
    deviceManager->setServerId(serverId);
  }
}

/**
 * @brief Get the device topology.
 */
json DeviceServer::getDeviceTopology() const {
  if (deviceManager) {
    return deviceManager->getDeviceTopology();
  }
  return json::object();
}

/**
 * @brief Set device dependency.
 */
void DeviceServer::setDeviceDependency(const std::string &dependentDeviceId,
                                     const std::string &dependencyDeviceId,
                                     const std::string &dependencyType) {
  if (deviceManager) {
    deviceManager->setDeviceDependency(dependentDeviceId, dependencyDeviceId, dependencyType);
  }
}

/**
 * @brief Set error handling strategy.
 */
void DeviceServer::setErrorStrategy(const std::string &errorCode, ErrorHandlingStrategy strategy) {
  if (errorManager) {
    errorManager->setErrorStrategy(errorCode, strategy);
  }
}

/**
 * @brief Get pending errors.
 */
json DeviceServer::getPendingErrors() const {
  if (errorManager) {
    return errorManager->getPendingErrors();
  }
  return json::array();
}

// 处理错误消息
void DeviceServer::handleErrorMessage(std::shared_ptr<ErrorMessage> msg,
                                    crow::websocket::connection &conn) {
  spdlog::info("[DeviceServer] 收到错误消息: {} ({}) 来自设备: {}", 
              msg->getErrorCode(), msg->getErrorMessage(), msg->getDeviceId());
  
  // 将错误消息转发给所有客户端
  broadcastToClients(*msg);
  
  // 使用错误恢复管理器处理错误
  if (errorManager) {
    bool resolved = errorManager->handleError(*msg);
    
    if (resolved) {
      spdlog::info("[DeviceServer] 错误已自动解决: {} (设备: {})", 
                 msg->getErrorCode(), msg->getDeviceId());
    } else {
      spdlog::warn("[DeviceServer] 错误未能自动解决: {} (设备: {})", 
                 msg->getErrorCode(), msg->getDeviceId());
    }
  }
}

} // namespace astrocomm
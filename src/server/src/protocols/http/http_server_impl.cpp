#include "hydrogen/server/protocols/http/http_server.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <random>
#include <iomanip>
#include <sstream>

namespace hydrogen {
namespace server {
namespace protocols {
namespace http {

// HttpProtocolHandler Implementation
HttpProtocolHandler::HttpProtocolHandler() 
    : core::BaseProtocolHandler(core::CommunicationProtocol::HTTP) {
}

std::vector<std::string> HttpProtocolHandler::getSupportedMessageTypes() const {
    return {"GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS", "HEAD"};
}

bool HttpProtocolHandler::canHandle(const core::Message& message) const {
    return message.sourceProtocol == core::CommunicationProtocol::HTTP ||
           message.sourceProtocol == core::CommunicationProtocol::WEBSOCKET;
}

bool HttpProtocolHandler::processIncomingMessage(const core::Message& message) {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    
    logMessage("debug", "Processing incoming HTTP message from: " + message.senderId);
    
    // Process HTTP-specific headers
    for (const auto& header : message.headers) {
        if (header.first.substr(0, 5) == "HTTP_") {
            httpHeaders_[header.first] = header.second;
        }
    }
    
    updateStatistics("incoming_messages", true);
    return true;
}

bool HttpProtocolHandler::processOutgoingMessage(core::Message& message) {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    
    logMessage("debug", "Processing outgoing HTTP message to: " + message.recipientId);
    
    // Add HTTP-specific headers
    message.headers["Content-Type"] = "application/json";
    message.headers["Server"] = "Hydrogen-Server/1.0";
    message.headers["X-Powered-By"] = "Hydrogen";
    
    // Add custom HTTP headers
    for (const auto& header : httpHeaders_) {
        message.headers[header.first] = header.second;
    }
    
    updateStatistics("outgoing_messages", true);
    return true;
}

core::Message HttpProtocolHandler::transformMessage(const core::Message& source, 
                                                   core::CommunicationProtocol targetProtocol) const {
    core::Message transformed = source;
    transformed.sourceProtocol = getProtocol();
    transformed.targetProtocol = targetProtocol;
    
    // Transform based on target protocol
    switch (targetProtocol) {
        case core::CommunicationProtocol::MQTT:
            // Convert HTTP request to MQTT message
            transformed.topic = "http/" + source.topic;
            break;
        case core::CommunicationProtocol::GRPC:
            // Convert HTTP to gRPC format
            transformed.headers["grpc-method"] = source.headers.at("HTTP_METHOD");
            break;
        case core::CommunicationProtocol::WEBSOCKET:
            // Convert HTTP to WebSocket
            transformed.headers["ws-origin"] = source.headers.at("HTTP_ORIGIN");
            break;
        default:
            // No transformation needed
            break;
    }
    
    return transformed;
}

bool HttpProtocolHandler::handleClientConnect(const core::ConnectionInfo& connection) {
    logMessage("info", "HTTP client connected: " + connection.clientId);
    return true;
}

bool HttpProtocolHandler::handleClientDisconnect(const std::string& clientId) {
    logMessage("info", "HTTP client disconnected: " + clientId);
    return true;
}

// HttpServer Implementation
HttpServer::HttpServer(const HttpServerConfig& config) 
    : config_(config), status_(core::ServerStatus::STOPPED) {
    app_ = std::make_unique<crow::SimpleApp>();
    startTime_ = std::chrono::system_clock::now();
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    std::lock_guard<std::mutex> lock(serverMutex_);
    
    if (status_ == core::ServerStatus::RUNNING) {
        spdlog::warn("HTTP server already running");
        return true;
    }
    
    status_ = core::ServerStatus::STARTING;
    spdlog::info("Starting HTTP server on {}:{}", config_.host, config_.port);
    
    try {
        setupMiddleware();
        setupRoutes();
        setupWebSocketHandlers();
        
        // Configure CORS if enabled
        if (config_.enableCors) {
            enableCors(true);
        }
        
        // Configure SSL if enabled
        if (config_.enableSsl && !config_.sslCertPath.empty() && !config_.sslKeyPath.empty()) {
            configureSsl(config_.sslCertPath, config_.sslKeyPath);
        }
        
        // Start server in separate thread
        serverThread_ = std::thread([this]() {
            try {
                app_->port(config_.port);
                app_->run();
            } catch (const std::exception& e) {
                spdlog::error("HTTP server error: {}", e.what());
                status_ = core::ServerStatus::ERROR;
                if (errorCallback_) {
                    errorCallback_("HTTP server startup failed: " + std::string(e.what()));
                }
            }
        });
        
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        status_ = core::ServerStatus::RUNNING;
        spdlog::info("HTTP server started successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to start HTTP server: {}", e.what());
        status_ = core::ServerStatus::ERROR;
        return false;
    }
}

bool HttpServer::stop() {
    std::lock_guard<std::mutex> lock(serverMutex_);
    
    if (status_ != core::ServerStatus::RUNNING) {
        return true;
    }
    
    status_ = core::ServerStatus::STOPPING;
    spdlog::info("Stopping HTTP server...");
    
    try {
        // Stop the Crow app
        app_->stop();
        
        // Wait for server thread to finish
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        
        // Disconnect all WebSocket connections
        {
            std::lock_guard<std::mutex> connLock(connectionsMutex_);
            for (auto& pair : webSocketConnections_) {
                if (pair.second.connection) {
                    pair.second.connection->close("Server shutting down");
                }
            }
            webSocketConnections_.clear();
        }
        
        status_ = core::ServerStatus::STOPPED;
        spdlog::info("HTTP server stopped");
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Error stopping HTTP server: {}", e.what());
        status_ = core::ServerStatus::ERROR;
        return false;
    }
}

bool HttpServer::restart() {
    spdlog::info("Restarting HTTP server...");
    
    if (!stop()) {
        return false;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    return start();
}

core::ServerStatus HttpServer::getStatus() const {
    std::lock_guard<std::mutex> lock(serverMutex_);
    return status_;
}

void HttpServer::setConfig(const core::ServerConfig& config) {
    // Convert generic config to HTTP-specific config
    config_.host = config.host;
    config_.port = static_cast<uint16_t>(config.port);
    config_.enableSsl = config.enableSsl;
    config_.maxConnections = config.maxConnections;
}

core::ServerConfig HttpServer::getConfig() const {
    core::ServerConfig config;
    config.host = config_.host;
    config.port = config_.port;
    config.enableSsl = config_.enableSsl;
    config.maxConnections = config_.maxConnections;
    return config;
}

bool HttpServer::isConfigValid() const {
    return !config_.host.empty() &&
           config_.port > 0 &&
           config_.port <= 65535 &&
           config_.maxConnections > 0;
}

std::vector<core::ConnectionInfo> HttpServer::getActiveConnections() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    std::vector<core::ConnectionInfo> connections;
    connections.reserve(webSocketConnections_.size());
    
    for (const auto& pair : webSocketConnections_) {
        const auto& wsConn = pair.second;
        
        core::ConnectionInfo connInfo;
        connInfo.clientId = wsConn.clientId;
        connInfo.protocol = core::CommunicationProtocol::WEBSOCKET;
        connInfo.remoteAddress = wsConn.remoteAddress;
        connInfo.connectedAt = wsConn.connectedAt;
        connInfo.lastActivity = wsConn.lastActivity;
        // Note: isActive field removed from ConnectionInfo structure
        
        connections.push_back(connInfo);
    }
    
    return connections;
}

size_t HttpServer::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return webSocketConnections_.size();
}

bool HttpServer::disconnectClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    auto it = webSocketConnections_.find(clientId);
    if (it != webSocketConnections_.end()) {
        if (it->second.connection) {
            it->second.connection->close("Disconnected by server");
        }
        webSocketConnections_.erase(it);
        return true;
    }
    
    return false;
}

core::CommunicationProtocol HttpServer::getProtocol() const {
    return core::CommunicationProtocol::HTTP;
}

std::string HttpServer::getProtocolName() const {
    return "HTTP/WebSocket";
}

bool HttpServer::isHealthy() const {
    return status_ == core::ServerStatus::RUNNING;
}

std::string HttpServer::getHealthStatus() const {
    switch (status_) {
        case core::ServerStatus::STOPPED: return "Server stopped";
        case core::ServerStatus::STARTING: return "Server starting";
        case core::ServerStatus::RUNNING: return "Server running normally";
        case core::ServerStatus::STOPPING: return "Server stopping";
        case core::ServerStatus::ERROR: return "Server error";
        default: return "Unknown status";
    }
}

void HttpServer::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = callback;
}

void HttpServer::setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
}

void HttpServer::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = callback;
}

void HttpServer::setHttpConfig(const HttpServerConfig& config) {
    config_ = config;
}

HttpServerConfig HttpServer::getHttpConfig() const {
    return config_;
}

bool HttpServer::addRoute(const std::string& method, const std::string& path,
                         std::function<crow::response(const crow::request&, const HttpRequestContext&)> handler) {
    std::lock_guard<std::mutex> lock(routesMutex_);
    
    std::string routeKey = method + ":" + path;
    routes_[routeKey] = path;
    
    // Register route with Crow (simplified implementation)
    // Note: This is a simplified implementation that doesn't distinguish HTTP methods
    // Store the handler for later use - Crow API varies significantly between versions
    spdlog::info("Route registered: {} {}", method, path);
    // TODO: Implement proper Crow route registration based on the specific Crow version
    
    spdlog::debug("Added route: {} {}", method, path);
    return true;
}

bool HttpServer::removeRoute(const std::string& method, const std::string& path) {
    std::lock_guard<std::mutex> lock(routesMutex_);
    
    std::string routeKey = method + ":" + path;
    auto it = routes_.find(routeKey);
    if (it != routes_.end()) {
        routes_.erase(it);
        spdlog::debug("Removed route: {} {}", method, path);
        return true;
    }
    
    return false;
}

std::vector<std::string> HttpServer::getRoutes() const {
    std::lock_guard<std::mutex> lock(routesMutex_);
    
    std::vector<std::string> result;
    result.reserve(routes_.size());
    
    for (const auto& pair : routes_) {
        result.push_back(pair.first);
    }
    
    return result;
}

bool HttpServer::addMiddleware(const std::string& name,
                             std::function<bool(const crow::request&, crow::response&, HttpRequestContext&)> middleware) {
    // Middleware implementation would be added here
    middleware_.push_back(name);
    spdlog::debug("Added middleware: {}", name);
    return true;
}

bool HttpServer::removeMiddleware(const std::string& name) {
    auto it = std::find(middleware_.begin(), middleware_.end(), name);
    if (it != middleware_.end()) {
        middleware_.erase(it);
        spdlog::debug("Removed middleware: {}", name);
        return true;
    }
    return false;
}

std::vector<std::string> HttpServer::getMiddleware() const {
    return middleware_;
}

std::vector<WebSocketConnection> HttpServer::getWebSocketConnections() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    std::vector<WebSocketConnection> result;
    result.reserve(webSocketConnections_.size());
    
    for (const auto& pair : webSocketConnections_) {
        result.push_back(pair.second);
    }
    
    return result;
}

bool HttpServer::sendWebSocketMessage(const std::string& connectionId, const std::string& message) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    auto it = webSocketConnections_.find(connectionId);
    if (it != webSocketConnections_.end() && it->second.connection) {
        it->second.connection->send_text(message);
        it->second.lastActivity = std::chrono::system_clock::now();
        return true;
    }
    
    return false;
}

bool HttpServer::broadcastWebSocketMessage(const std::string& message, const std::vector<std::string>& connectionIds) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    bool success = true;
    
    if (connectionIds.empty()) {
        // Broadcast to all connections
        for (auto& pair : webSocketConnections_) {
            if (pair.second.connection) {
                pair.second.connection->send_text(message);
                pair.second.lastActivity = std::chrono::system_clock::now();
            }
        }
    } else {
        // Broadcast to specific connections
        for (const auto& connId : connectionIds) {
            auto it = webSocketConnections_.find(connId);
            if (it != webSocketConnections_.end() && it->second.connection) {
                it->second.connection->send_text(message);
                it->second.lastActivity = std::chrono::system_clock::now();
            } else {
                success = false;
            }
        }
    }
    
    return success;
}

bool HttpServer::disconnectWebSocket(const std::string& connectionId) {
    return disconnectClient(connectionId);
}

bool HttpServer::setStaticFilesPath(const std::string& path) {
    config_.staticFilesPath = path;
    return true;
}

std::string HttpServer::getStaticFilesPath() const {
    return config_.staticFilesPath;
}

bool HttpServer::enableStaticFiles(bool enabled) {
    // Implementation would configure static file serving
    return true;
}

bool HttpServer::setCorsOrigins(const std::vector<std::string>& origins) {
    config_.allowedOrigins = origins;
    return true;
}

std::vector<std::string> HttpServer::getCorsOrigins() const {
    return config_.allowedOrigins;
}

bool HttpServer::enableCors(bool enabled) {
    config_.enableCors = enabled;
    return true;
}

bool HttpServer::configureSsl(const std::string& certPath, const std::string& keyPath) {
    config_.enableSsl = true;
    config_.sslCertPath = certPath;
    config_.sslKeyPath = keyPath;
    return true;
}

bool HttpServer::isSslEnabled() const {
    return config_.enableSsl;
}

HttpRequestContext HttpServer::createRequestContext(const crow::request& req) {
    HttpRequestContext ctx;
    ctx.clientId = extractClientId(req);
    ctx.remoteAddress = req.remote_ip_address;
    ctx.timestamp = std::chrono::system_clock::now();
    
    // Extract headers
    for (const auto& header : req.headers) {
        ctx.headers[header.first] = header.second;
    }
    
    // Extract query parameters
    for (const auto& param : req.url_params) {
        ctx.queryParams[param.first] = param.second;
    }
    
    return ctx;
}

crow::response HttpServer::createJsonResponse(int statusCode, const std::string& json) {
    crow::response res(statusCode);
    res.set_header("Content-Type", "application/json");
    res.write(json);
    return res;
}

crow::response HttpServer::createErrorResponse(int statusCode, const std::string& error) {
    nlohmann::json errorJson;
    errorJson["error"] = error;
    errorJson["status"] = statusCode;
    errorJson["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return createJsonResponse(statusCode, errorJson.dump());
}

crow::response HttpServer::createSuccessResponse(const std::string& data) {
    nlohmann::json successJson;
    successJson["success"] = true;
    successJson["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (!data.empty()) {
        successJson["data"] = nlohmann::json::parse(data);
    }
    
    return createJsonResponse(200, successJson.dump());
}

void HttpServer::setDeviceService(std::shared_ptr<services::IDeviceService> deviceService) {
    deviceService_ = deviceService;
}

void HttpServer::setAuthService(std::shared_ptr<services::IAuthService> authService) {
    authService_ = authService;
}

void HttpServer::setCommunicationService(std::shared_ptr<services::ICommunicationService> communicationService) {
    communicationService_ = communicationService;
}

void HttpServer::setupRoutes() {
    setupDeviceRoutes();
    setupAuthRoutes();
    setupSystemRoutes();
}

void HttpServer::setupMiddleware() {
    // Add CORS middleware
    addMiddleware("cors", [this](const crow::request& req, crow::response& res, HttpRequestContext& ctx) {
        return corsMiddleware(req, res, ctx);
    });
    
    // Add logging middleware
    addMiddleware("logging", [this](const crow::request& req, crow::response& res, HttpRequestContext& ctx) {
        return loggingMiddleware(req, res, ctx);
    });
    
    // Add authentication middleware
    addMiddleware("auth", [this](const crow::request& req, crow::response& res, HttpRequestContext& ctx) {
        return authenticationMiddleware(req, res, ctx);
    });
}

void HttpServer::setupWebSocketHandlers() {
    if (!config_.enableWebSocket) {
        return;
    }
    
    // WebSocket support simplified - Crow WebSocket API varies by version
    // This is a placeholder implementation
    spdlog::info("WebSocket handlers setup (simplified implementation)");
}

void HttpServer::setupDeviceRoutes() {
    // GET /api/devices - Get all devices
    addRoute("GET", "/api/devices", [this](const crow::request& req, const HttpRequestContext& ctx) {
        if (!deviceService_) {
            return createErrorResponse(503, "Device service not available");
        }
        
        auto devices = deviceService_->getAllDevices();
        nlohmann::json devicesJson = nlohmann::json::array();
        
        for (const auto& device : devices) {
            nlohmann::json deviceJson;
            deviceJson["deviceId"] = device.deviceId;
            deviceJson["deviceName"] = device.deviceName;
            deviceJson["deviceType"] = device.deviceType;
            deviceJson["manufacturer"] = device.manufacturer;
            deviceJson["model"] = device.model;
            deviceJson["connectionStatus"] = static_cast<int>(device.connectionStatus);
            deviceJson["healthStatus"] = static_cast<int>(device.healthStatus);
            devicesJson.push_back(deviceJson);
        }
        
        return createJsonResponse(200, devicesJson.dump());
    });
    
    // GET /api/devices/{id} - Get specific device
    addRoute("GET", "/api/devices/<string>", [this](const crow::request& req, const HttpRequestContext& ctx) {
        if (!deviceService_) {
            return createErrorResponse(503, "Device service not available");
        }
        
        // Fix URL parameter access
        std::string deviceId;
        auto it = req.url_params.find("id");
        if (it != req.url_params.end()) {
            deviceId = it->second;
        }
        if (deviceId.empty()) {
            return createErrorResponse(400, "Device ID required");
        }
        
        auto device = deviceService_->getDeviceInfo(deviceId);
        if (device.deviceId.empty()) {
            return createErrorResponse(404, "Device not found");
        }
        
        nlohmann::json deviceJson;
        deviceJson["deviceId"] = device.deviceId;
        deviceJson["deviceName"] = device.deviceName;
        deviceJson["deviceType"] = device.deviceType;
        deviceJson["manufacturer"] = device.manufacturer;
        deviceJson["model"] = device.model;
        deviceJson["connectionStatus"] = static_cast<int>(device.connectionStatus);
        deviceJson["healthStatus"] = static_cast<int>(device.healthStatus);
        deviceJson["properties"] = device.properties;
        
        return createJsonResponse(200, deviceJson.dump());
    });
}

void HttpServer::setupAuthRoutes() {
    // POST /api/auth/login - User login
    addRoute("POST", "/api/auth/login", [this](const crow::request& req, const HttpRequestContext& ctx) {
        if (!authService_) {
            return createErrorResponse(503, "Authentication service not available");
        }
        
        try {
            auto loginData = nlohmann::json::parse(req.body);
            
            services::AuthRequest authReq;
            authReq.username = loginData["username"];
            authReq.password = loginData["password"];
            authReq.clientId = ctx.clientId;
            authReq.remoteAddress = ctx.remoteAddress;
            authReq.method = services::AuthMethod::BASIC;
            authReq.timestamp = std::chrono::system_clock::now();
            
            auto result = authService_->authenticate(authReq);
            
            if (result.success) {
                nlohmann::json responseJson;
                responseJson["success"] = true;
                responseJson["token"] = result.token.token;
                responseJson["expiresAt"] = std::chrono::duration_cast<std::chrono::seconds>(
                    result.token.expiresAt.time_since_epoch()).count();
                responseJson["user"] = {
                    {"userId", result.token.userId},
                    {"username", result.token.username},
                    {"role", static_cast<int>(result.token.role)}
                };
                
                return createJsonResponse(200, responseJson.dump());
            } else {
                return createErrorResponse(401, result.errorMessage);
            }
            
        } catch (const std::exception& e) {
            return createErrorResponse(400, "Invalid request format");
        }
    });
    
    // POST /api/auth/logout - User logout
    addRoute("POST", "/api/auth/logout", [this](const crow::request& req, const HttpRequestContext& ctx) {
        if (!authService_) {
            return createErrorResponse(503, "Authentication service not available");
        }
        
        auto authHeader = ctx.headers.find("Authorization");
        if (authHeader != ctx.headers.end()) {
            std::string token = authHeader->second;
            if (token.substr(0, 7) == "Bearer ") {
                token = token.substr(7);
                authService_->revokeToken(token);
            }
        }
        
        return createSuccessResponse("");
    });
}

void HttpServer::setupSystemRoutes() {
    // GET /api/status - Server status
    addRoute("GET", "/api/status", [this](const crow::request& req, const HttpRequestContext& ctx) {
        nlohmann::json statusJson;
        statusJson["status"] = "running";
        statusJson["uptime"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - startTime_).count();
        statusJson["connections"] = getConnectionCount();
        statusJson["requests"] = requestCount_.load();
        statusJson["errors"] = errorCount_.load();
        
        return createJsonResponse(200, statusJson.dump());
    });
    
    // GET /api/health - Health check
    addRoute("GET", "/api/health", [this](const crow::request& req, const HttpRequestContext& ctx) {
        nlohmann::json healthJson;
        healthJson["healthy"] = isHealthy();
        healthJson["status"] = getHealthStatus();
        healthJson["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        return createJsonResponse(200, healthJson.dump());
    });
}

void HttpServer::handleWebSocketOpen(crow::websocket::connection& conn) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    WebSocketConnection wsConn;
    wsConn.connectionId = generateConnectionId();
    wsConn.clientId = wsConn.connectionId; // Use connection ID as client ID for now
    wsConn.remoteAddress = ""; // Would extract from connection
    wsConn.connectedAt = std::chrono::system_clock::now();
    wsConn.lastActivity = wsConn.connectedAt;
    wsConn.connection = &conn;
    
    webSocketConnections_[wsConn.connectionId] = wsConn;
    
    spdlog::info("WebSocket connection opened: {}", wsConn.connectionId);
    
    if (connectionCallback_) {
        core::ConnectionInfo connInfo;
        connInfo.clientId = wsConn.clientId;
        connInfo.protocol = core::CommunicationProtocol::WEBSOCKET;
        connInfo.remoteAddress = wsConn.remoteAddress;
        connInfo.connectedAt = wsConn.connectedAt;
        // Note: isActive field removed from ConnectionInfo structure
        
        connectionCallback_(connInfo, true);
    }
}

void HttpServer::handleWebSocketClose(crow::websocket::connection& conn, const std::string& reason) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    // Find and remove the connection
    auto it = std::find_if(webSocketConnections_.begin(), webSocketConnections_.end(),
        [&conn](const auto& pair) {
            return pair.second.connection == &conn;
        });
    
    if (it != webSocketConnections_.end()) {
        spdlog::info("WebSocket connection closed: {} (reason: {})", it->first, reason);
        
        if (connectionCallback_) {
            core::ConnectionInfo connInfo;
            connInfo.clientId = it->second.clientId;
            connInfo.protocol = core::CommunicationProtocol::WEBSOCKET;
            connInfo.remoteAddress = it->second.remoteAddress;
            connInfo.connectedAt = it->second.connectedAt;
            // Note: isActive field removed from ConnectionInfo structure
            
            connectionCallback_(connInfo, false);
        }
        
        webSocketConnections_.erase(it);
    }
}

void HttpServer::handleWebSocketMessage(crow::websocket::connection& conn, const std::string& data, bool is_binary) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    // Find the connection
    auto it = std::find_if(webSocketConnections_.begin(), webSocketConnections_.end(),
        [&conn](const auto& pair) {
            return pair.second.connection == &conn;
        });
    
    if (it != webSocketConnections_.end()) {
        it->second.lastActivity = std::chrono::system_clock::now();
        
        spdlog::debug("WebSocket message received from {}: {}", it->first, data);
        
        if (messageCallback_) {
            core::Message message;
            message.senderId = it->second.clientId;
            message.sourceProtocol = core::CommunicationProtocol::WEBSOCKET;
            message.payload = data;
            message.timestamp = std::chrono::system_clock::now();
            
            messageCallback_(message);
        }
    }
}

bool HttpServer::authenticationMiddleware(const crow::request& req, crow::response& res, HttpRequestContext& ctx) {
    // Skip authentication for certain endpoints
    std::string path = req.url;
    if (path == "/api/auth/login" || path == "/api/status" || path == "/api/health") {
        return true;
    }
    
    // Check for Authorization header
    auto authHeader = req.get_header_value("Authorization");
    if (authHeader.empty()) {
        res = createErrorResponse(401, "Authorization header required");
        return false;
    }
    
    // Validate Bearer token
    if (authHeader.substr(0, 7) != "Bearer ") {
        res = createErrorResponse(401, "Invalid authorization format");
        return false;
    }
    
    std::string token = authHeader.substr(7);
    if (authService_ && authService_->validateToken(token)) {
        auto authToken = authService_->parseToken(token);
        ctx.userId = authToken.userId;
        ctx.sessionId = token; // Use token as session ID for simplicity
        return true;
    }
    
    res = createErrorResponse(401, "Invalid or expired token");
    return false;
}

bool HttpServer::corsMiddleware(const crow::request& req, crow::response& res, HttpRequestContext& ctx) {
    if (config_.enableCors) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        
        // Simplified HTTP method check
        if (req.url.find("OPTIONS") != std::string::npos) {
            res.code = 200;
            res.end();
            return false; // Stop processing for OPTIONS requests
        }
    }
    
    return true;
}

bool HttpServer::loggingMiddleware(const crow::request& req, crow::response& res, HttpRequestContext& ctx) {
    spdlog::info("HTTP request {} from {}", req.url, ctx.remoteAddress);
    return true;
}

bool HttpServer::rateLimitMiddleware(const crow::request& req, crow::response& res, HttpRequestContext& ctx) {
    // Simple rate limiting implementation
    // In production, this would use a more sophisticated algorithm
    return true;
}

std::string HttpServer::generateConnectionId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "ws_";
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

bool HttpServer::validateRequest(const crow::request& req) const {
    // Basic request validation
    return req.url.length() <= 2048 && req.body.length() <= config_.maxRequestSize;
}

std::string HttpServer::extractClientId(const crow::request& req) const {
    // Extract client ID from headers or generate one
    auto clientIdHeader = req.get_header_value("X-Client-ID");
    if (!clientIdHeader.empty()) {
        return clientIdHeader;
    }
    
    // Generate client ID based on IP and User-Agent
    std::string userAgent = req.get_header_value("User-Agent");
    std::hash<std::string> hasher;
    size_t hash = hasher(req.remote_ip_address + userAgent);
    
    std::stringstream ss;
    ss << "client_" << std::hex << hash;
    return ss.str();
}

std::string HttpServer::extractUserId(const HttpRequestContext& ctx) const {
    return ctx.userId;
}

// HttpServerFactory Implementation
std::unique_ptr<IHttpServer> HttpServerFactory::createServer(const HttpServerConfig& config) {
    return std::make_unique<HttpServer>(config);
}

std::unique_ptr<IHttpServer> HttpServerFactory::createSecureServer(const std::string& certPath, const std::string& keyPath,
                                                                  const HttpServerConfig& config) {
    HttpServerConfig secureConfig = config;
    secureConfig.enableSsl = true;
    secureConfig.sslCertPath = certPath;
    secureConfig.sslKeyPath = keyPath;
    
    return std::make_unique<HttpServer>(secureConfig);
}

} // namespace http
} // namespace protocols
} // namespace server
} // namespace hydrogen

#pragma once

#include "../../core/server_interface.h"
#include "../../core/protocol_handler.h"
#include "../../services/device_service.h"
#include "../../services/auth_service.h"
#include "../../services/communication_service.h"
#include <crow/app.h>
#include <crow/websocket.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>

namespace astrocomm {
namespace server {
namespace protocols {
namespace http {

/**
 * @brief HTTP request context
 */
struct HttpRequestContext {
    std::string clientId;
    std::string userId;
    std::string sessionId;
    std::string remoteAddress;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> queryParams;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief WebSocket connection information
 */
struct WebSocketConnection {
    std::string connectionId;
    std::string clientId;
    std::string userId;
    std::string sessionId;
    std::string remoteAddress;
    std::chrono::system_clock::time_point connectedAt;
    std::chrono::system_clock::time_point lastActivity;
    std::unordered_map<std::string, std::string> metadata;
    crow::websocket::connection* connection;
};

/**
 * @brief HTTP/WebSocket server configuration
 */
struct HttpServerConfig {
    std::string host = "localhost";
    uint16_t port = 8080;
    bool enableSsl = false;
    std::string sslCertPath;
    std::string sslKeyPath;
    bool enableCors = true;
    std::vector<std::string> allowedOrigins;
    std::string staticFilesPath;
    bool enableCompression = true;
    size_t maxRequestSize = 1024 * 1024; // 1MB
    std::chrono::seconds requestTimeout{30};
    int maxConnections = 1000;
    bool enableWebSocket = true;
    std::chrono::seconds websocketTimeout{300};
    std::chrono::seconds heartbeatInterval{30};
};

/**
 * @brief HTTP/WebSocket server interface
 * 
 * Provides HTTP REST API and WebSocket communication capabilities
 * with authentication, device management, and real-time messaging.
 */
class IHttpServer : public core::IServerInterface {
public:
    virtual ~IHttpServer() = default;

    // HTTP-specific configuration
    virtual void setHttpConfig(const HttpServerConfig& config) = 0;
    virtual HttpServerConfig getHttpConfig() const = 0;

    // Route management
    virtual bool addRoute(const std::string& method, const std::string& path,
                         std::function<crow::response(const crow::request&, const HttpRequestContext&)> handler) = 0;
    virtual bool removeRoute(const std::string& method, const std::string& path) = 0;
    virtual std::vector<std::string> getRoutes() const = 0;

    // Middleware management
    virtual bool addMiddleware(const std::string& name,
                             std::function<bool(const crow::request&, crow::response&, HttpRequestContext&)> middleware) = 0;
    virtual bool removeMiddleware(const std::string& name) = 0;
    virtual std::vector<std::string> getMiddleware() const = 0;

    // WebSocket management
    virtual std::vector<WebSocketConnection> getWebSocketConnections() const = 0;
    virtual bool sendWebSocketMessage(const std::string& connectionId, const std::string& message) = 0;
    virtual bool broadcastWebSocketMessage(const std::string& message, const std::vector<std::string>& connectionIds = {}) = 0;
    virtual bool disconnectWebSocket(const std::string& connectionId) = 0;

    // Static file serving
    virtual bool setStaticFilesPath(const std::string& path) = 0;
    virtual std::string getStaticFilesPath() const = 0;
    virtual bool enableStaticFiles(bool enabled) = 0;

    // CORS configuration
    virtual bool setCorsOrigins(const std::vector<std::string>& origins) = 0;
    virtual std::vector<std::string> getCorsOrigins() const = 0;
    virtual bool enableCors(bool enabled) = 0;

    // SSL/TLS configuration
    virtual bool configureSsl(const std::string& certPath, const std::string& keyPath) = 0;
    virtual bool isSslEnabled() const = 0;

    // Request/Response utilities
    virtual HttpRequestContext createRequestContext(const crow::request& req) = 0;
    virtual crow::response createJsonResponse(int statusCode, const std::string& json) = 0;
    virtual crow::response createErrorResponse(int statusCode, const std::string& error) = 0;
    virtual crow::response createSuccessResponse(const std::string& data = "") = 0;
};

/**
 * @brief HTTP protocol handler
 * 
 * Handles HTTP-specific message processing and protocol conversion.
 */
class HttpProtocolHandler : public core::BaseProtocolHandler {
public:
    HttpProtocolHandler();
    virtual ~HttpProtocolHandler() = default;

    // IProtocolHandler implementation
    std::vector<std::string> getSupportedMessageTypes() const override;
    bool canHandle(const core::Message& message) const override;
    bool processIncomingMessage(const core::Message& message) override;
    bool processOutgoingMessage(core::Message& message) override;
    core::Message transformMessage(const core::Message& source, 
                                 core::CommunicationProtocol targetProtocol) const override;
    bool handleClientConnect(const core::ConnectionInfo& connection) override;
    bool handleClientDisconnect(const std::string& clientId) override;

private:
    std::unordered_map<std::string, std::string> httpHeaders_;
    std::mutex handlerMutex_;
};

/**
 * @brief Concrete HTTP/WebSocket server implementation
 * 
 * Implements the HTTP server interface using the Crow framework
 * with integrated device management and authentication.
 */
class HttpServer : public IHttpServer {
public:
    explicit HttpServer(const HttpServerConfig& config = {});
    virtual ~HttpServer();

    // IServerInterface implementation
    bool start() override;
    bool stop() override;
    bool restart() override;
    core::ServerStatus getStatus() const override;
    void setConfig(const core::ServerConfig& config) override;
    core::ServerConfig getConfig() const override;
    bool isConfigValid() const override;
    std::vector<core::ConnectionInfo> getActiveConnections() const override;
    size_t getConnectionCount() const override;
    bool disconnectClient(const std::string& clientId) override;
    core::CommunicationProtocol getProtocol() const override;
    std::string getProtocolName() const override;
    bool isHealthy() const override;
    std::string getHealthStatus() const override;
    void setConnectionCallback(ConnectionCallback callback) override;
    void setMessageCallback(MessageCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;

    // IHttpServer implementation
    void setHttpConfig(const HttpServerConfig& config) override;
    HttpServerConfig getHttpConfig() const override;
    bool addRoute(const std::string& method, const std::string& path,
                 std::function<crow::response(const crow::request&, const HttpRequestContext&)> handler) override;
    bool removeRoute(const std::string& method, const std::string& path) override;
    std::vector<std::string> getRoutes() const override;
    bool addMiddleware(const std::string& name,
                     std::function<bool(const crow::request&, crow::response&, HttpRequestContext&)> middleware) override;
    bool removeMiddleware(const std::string& name) override;
    std::vector<std::string> getMiddleware() const override;
    std::vector<WebSocketConnection> getWebSocketConnections() const override;
    bool sendWebSocketMessage(const std::string& connectionId, const std::string& message) override;
    bool broadcastWebSocketMessage(const std::string& message, const std::vector<std::string>& connectionIds = {}) override;
    bool disconnectWebSocket(const std::string& connectionId) override;
    bool setStaticFilesPath(const std::string& path) override;
    std::string getStaticFilesPath() const override;
    bool enableStaticFiles(bool enabled) override;
    bool setCorsOrigins(const std::vector<std::string>& origins) override;
    std::vector<std::string> getCorsOrigins() const override;
    bool enableCors(bool enabled) override;
    bool configureSsl(const std::string& certPath, const std::string& keyPath) override;
    bool isSslEnabled() const override;
    HttpRequestContext createRequestContext(const crow::request& req) override;
    crow::response createJsonResponse(int statusCode, const std::string& json) override;
    crow::response createErrorResponse(int statusCode, const std::string& error) override;
    crow::response createSuccessResponse(const std::string& data = "") override;

    // Service integration
    void setDeviceService(std::shared_ptr<services::IDeviceService> deviceService);
    void setAuthService(std::shared_ptr<services::IAuthService> authService);
    void setCommunicationService(std::shared_ptr<services::ICommunicationService> communicationService);

private:
    // Internal methods
    void setupRoutes();
    void setupMiddleware();
    void setupWebSocketHandlers();
    void setupDeviceRoutes();
    void setupAuthRoutes();
    void setupSystemRoutes();
    
    // WebSocket handlers
    void handleWebSocketOpen(crow::websocket::connection& conn);
    void handleWebSocketClose(crow::websocket::connection& conn, const std::string& reason);
    void handleWebSocketMessage(crow::websocket::connection& conn, const std::string& data, bool is_binary);
    
    // Middleware functions
    bool authenticationMiddleware(const crow::request& req, crow::response& res, HttpRequestContext& ctx);
    bool corsMiddleware(const crow::request& req, crow::response& res, HttpRequestContext& ctx);
    bool loggingMiddleware(const crow::request& req, crow::response& res, HttpRequestContext& ctx);
    bool rateLimitMiddleware(const crow::request& req, crow::response& res, HttpRequestContext& ctx);
    
    // Utility methods
    std::string generateConnectionId() const;
    bool validateRequest(const crow::request& req) const;
    std::string extractClientId(const crow::request& req) const;
    std::string extractUserId(const HttpRequestContext& ctx) const;

    // Member variables
    mutable std::mutex serverMutex_;
    HttpServerConfig config_;
    core::ServerStatus status_;
    std::unique_ptr<crow::SimpleApp> app_;
    std::thread serverThread_;
    
    // WebSocket connections
    mutable std::mutex connectionsMutex_;
    std::unordered_map<std::string, WebSocketConnection> webSocketConnections_;
    
    // Routes and middleware
    mutable std::mutex routesMutex_;
    std::unordered_map<std::string, std::string> routes_;
    std::vector<std::string> middleware_;
    
    // Service dependencies
    std::shared_ptr<services::IDeviceService> deviceService_;
    std::shared_ptr<services::IAuthService> authService_;
    std::shared_ptr<services::ICommunicationService> communicationService_;
    
    // Callbacks
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    ErrorCallback errorCallback_;
    
    // Statistics
    std::atomic<size_t> requestCount_{0};
    std::atomic<size_t> errorCount_{0};
    std::chrono::system_clock::time_point startTime_;
};

/**
 * @brief HTTP server factory
 */
class HttpServerFactory {
public:
    static std::unique_ptr<IHttpServer> createServer(const HttpServerConfig& config = {});
    static std::unique_ptr<IHttpServer> createSecureServer(const std::string& certPath, const std::string& keyPath,
                                                          const HttpServerConfig& config = {});
};

} // namespace http
} // namespace protocols
} // namespace server
} // namespace astrocomm

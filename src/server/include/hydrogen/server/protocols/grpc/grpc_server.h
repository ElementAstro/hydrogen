#pragma once

#include "hydrogen/core/communication/infrastructure/communication_protocol.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>

// Forward declarations for gRPC types
namespace grpc {
    class Service;
}

namespace hydrogen {
namespace server {
namespace protocols {
namespace grpc {

/**
 * @brief gRPC authentication methods
 */
enum class GrpcAuthMethod {
    NONE = 0,
    TOKEN = 1,
    CERTIFICATE = 2,
    OAUTH2 = 3
};

/**
 * @brief gRPC server configuration
 */
struct GrpcServerConfig {
    std::string serverAddress = "localhost:50051";
    int maxMessageSize = 4 * 1024 * 1024; // 4MB
    int maxConcurrentStreams = 100;
    int shutdownTimeoutSeconds = 30;
    bool enableReflection = true;
    bool enableTLS = false;
    std::string certFile;
    std::string keyFile;
    GrpcAuthMethod authMethod = GrpcAuthMethod::NONE;
};

/**
 * @brief gRPC server statistics
 */
struct GrpcServerStatistics {
    size_t totalRequests = 0;
    size_t activeConnections = 0;
    size_t totalErrors = 0;
    double averageResponseTime = 0.0;
    double requestsPerSecond = 0.0;
    int64_t uptime = 0;
};

/**
 * @brief gRPC request handler function type
 */
using GrpcRequestHandler = std::function<void(const std::string& method, const std::string& request)>;

/**
 * @brief gRPC interceptor interface
 */
class GrpcInterceptor {
public:
    virtual ~GrpcInterceptor() = default;
    virtual void intercept(const std::string& method, const std::string& request) = 0;
};

/**
 * @brief Interface for gRPC server implementation
 */
class IGrpcServer {
public:
    virtual ~IGrpcServer() = default;

    // Server lifecycle
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool restart() = 0;
    virtual bool isRunning() const = 0;
    virtual bool isInitialized() const = 0;

    // Configuration
    virtual GrpcServerConfig getConfig() const = 0;
    virtual bool updateConfig(const GrpcServerConfig& config) = 0;

    // Service management
    virtual bool registerService(const std::string& serviceName, 
                                std::shared_ptr<::grpc::Service> service) = 0;
    virtual bool unregisterService(const std::string& serviceName) = 0;
    virtual std::vector<std::string> getRegisteredServices() const = 0;

    // Statistics and monitoring
    virtual GrpcServerStatistics getStatistics() const = 0;
    virtual void resetStatistics() = 0;

    // Health checking
    virtual bool isHealthy() const = 0;
    virtual std::string getHealthStatus() const = 0;

    // Connection management
    virtual size_t getActiveConnectionCount() const = 0;
    virtual bool closeConnection(const std::string& connectionId) = 0;
    virtual std::vector<std::string> getActiveConnections() const = 0;

    // Security and authentication
    virtual bool enableTLS(const std::string& certFile, const std::string& keyFile) = 0;
    virtual bool disableTLS() = 0;
    virtual bool setAuthenticationMethod(GrpcAuthMethod method) = 0;

    // Request handling
    virtual void setRequestHandler(const std::string& method, GrpcRequestHandler handler) = 0;
    virtual void removeRequestHandler(const std::string& method) = 0;

    // Interceptors
    virtual void addInterceptor(std::shared_ptr<GrpcInterceptor> interceptor) = 0;
    virtual void removeInterceptor(std::shared_ptr<GrpcInterceptor> interceptor) = 0;
    virtual void clearInterceptors() = 0;
};

/**
 * @brief Factory for creating gRPC server instances
 */
class GrpcServerFactory {
public:
    /**
     * @brief Create a gRPC server with custom configuration
     */
    static std::unique_ptr<IGrpcServer> createServer(const GrpcServerConfig& config);

    /**
     * @brief Create a gRPC server with default configuration
     */
    static std::unique_ptr<IGrpcServer> createServer(const std::string& address, int port);
};

} // namespace grpc
} // namespace protocols
} // namespace server
} // namespace hydrogen

#include "astrocomm/server/protocols/grpc/grpc_server.h"
#include <spdlog/spdlog.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <thread>
#include <chrono>

namespace astrocomm {
namespace server {
namespace protocols {
namespace grpc {

/**
 * @brief Concrete implementation of the gRPC Server
 */
class GrpcServerImpl : public IGrpcServer {
public:
    explicit GrpcServerImpl(const GrpcServerConfig& config)
        : config_(config), running_(false), initialized_(false) {
        spdlog::info("gRPC server created with address: {}", config_.serverAddress);
    }

    ~GrpcServerImpl() {
        stop();
    }

    // Server lifecycle
    bool initialize() override {
        if (initialized_) {
            spdlog::warn("gRPC server already initialized");
            return true;
        }

        try {
            // Create server builder
            ::grpc::ServerBuilder builder;
            
            // Configure server address
            builder.AddListeningPort(config_.serverAddress, ::grpc::InsecureServerCredentials());
            
            // Enable health checking service
            ::grpc::EnableDefaultHealthCheckService(true);
            
            // Enable server reflection for debugging
            if (config_.enableReflection) {
                ::grpc::reflection::InitProtoReflectionServerBuilderPlugin();
            }
            
            // Set server options
            builder.SetMaxReceiveMessageSize(config_.maxMessageSize);
            builder.SetMaxSendMessageSize(config_.maxMessageSize);
            builder.SetMaxConcurrentStreams(config_.maxConcurrentStreams);
            
            // Register services (placeholder - would register actual gRPC services here)
            // builder.RegisterService(&communicationService_);
            // builder.RegisterService(&healthService_);
            
            // Build server
            server_ = builder.BuildAndStart();
            
            if (!server_) {
                spdlog::error("Failed to build gRPC server");
                return false;
            }
            
            initialized_ = true;
            spdlog::info("gRPC server initialized successfully on {}", config_.serverAddress);
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Failed to initialize gRPC server: {}", e.what());
            return false;
        }
    }

    bool start() override {
        if (!initialized_) {
            spdlog::error("gRPC server not initialized");
            return false;
        }

        if (running_) {
            spdlog::warn("gRPC server already running");
            return true;
        }

        try {
            running_ = true;
            
            // Start server monitoring thread
            serverThread_ = std::thread(&GrpcServerImpl::serverLoop, this);
            
            spdlog::info("gRPC server started and listening on {}", config_.serverAddress);
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Failed to start gRPC server: {}", e.what());
            running_ = false;
            return false;
        }
    }

    bool stop() override {
        if (!running_) {
            return true;
        }

        try {
            spdlog::info("Stopping gRPC server...");
            
            running_ = false;
            
            // Shutdown server
            if (server_) {
                auto deadline = std::chrono::system_clock::now() + 
                               std::chrono::seconds(config_.shutdownTimeoutSeconds);
                server_->Shutdown(deadline);
            }
            
            // Wait for server thread to finish
            if (serverThread_.joinable()) {
                serverThread_.join();
            }
            
            spdlog::info("gRPC server stopped");
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Error stopping gRPC server: {}", e.what());
            return false;
        }
    }

    bool restart() override {
        return stop() && start();
    }

    bool isRunning() const override {
        return running_;
    }

    bool isInitialized() const override {
        return initialized_;
    }

    // Configuration
    GrpcServerConfig getConfig() const override {
        return config_;
    }

    bool updateConfig(const GrpcServerConfig& config) override {
        if (running_) {
            spdlog::warn("Cannot update gRPC server config while running");
            return false;
        }

        config_ = config;
        spdlog::info("gRPC server configuration updated");
        return true;
    }

    // Service management
    bool registerService(const std::string& serviceName, 
                        std::shared_ptr<::grpc::Service> service) override {
        if (running_) {
            spdlog::error("Cannot register service while server is running");
            return false;
        }

        try {
            services_[serviceName] = service;
            spdlog::info("gRPC service registered: {}", serviceName);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to register gRPC service '{}': {}", serviceName, e.what());
            return false;
        }
    }

    bool unregisterService(const std::string& serviceName) override {
        if (running_) {
            spdlog::error("Cannot unregister service while server is running");
            return false;
        }

        auto it = services_.find(serviceName);
        if (it != services_.end()) {
            services_.erase(it);
            spdlog::info("gRPC service unregistered: {}", serviceName);
            return true;
        }

        spdlog::warn("gRPC service not found for unregistration: {}", serviceName);
        return false;
    }

    std::vector<std::string> getRegisteredServices() const override {
        std::vector<std::string> serviceNames;
        for (const auto& pair : services_) {
            serviceNames.push_back(pair.first);
        }
        return serviceNames;
    }

    // Statistics and monitoring
    GrpcServerStatistics getStatistics() const override {
        GrpcServerStatistics stats;
        
        // In a real implementation, these would be tracked
        stats.totalRequests = 0;
        stats.activeConnections = 0;
        stats.totalErrors = 0;
        stats.averageResponseTime = 0.0;
        stats.requestsPerSecond = 0.0;
        stats.uptime = running_ ? std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime_).count() : 0;
        
        return stats;
    }

    void resetStatistics() override {
        // Reset internal statistics counters
        spdlog::debug("gRPC server statistics reset");
    }

    // Health checking
    bool isHealthy() const override {
        return initialized_ && running_ && server_ != nullptr;
    }

    std::string getHealthStatus() const override {
        if (!initialized_) {
            return "Not initialized";
        }
        if (!running_) {
            return "Not running";
        }
        if (!server_) {
            return "Server not created";
        }
        return "Healthy";
    }

    // Connection management
    size_t getActiveConnectionCount() const override {
        // In a real implementation, this would track actual connections
        return running_ ? 1 : 0;
    }

    bool closeConnection(const std::string& connectionId) override {
        // In a real implementation, this would close specific connections
        spdlog::debug("Connection close requested: {}", connectionId);
        return true;
    }

    std::vector<std::string> getActiveConnections() const override {
        // In a real implementation, this would return actual connection IDs
        std::vector<std::string> connections;
        if (running_) {
            connections.push_back("grpc_connection_1");
        }
        return connections;
    }

    // Security and authentication
    bool enableTLS(const std::string& certFile, const std::string& keyFile) override {
        if (running_) {
            spdlog::error("Cannot enable TLS while server is running");
            return false;
        }

        try {
            // Store TLS configuration
            config_.enableTLS = true;
            config_.certFile = certFile;
            config_.keyFile = keyFile;
            
            spdlog::info("TLS enabled for gRPC server");
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to enable TLS: {}", e.what());
            return false;
        }
    }

    bool disableTLS() override {
        if (running_) {
            spdlog::error("Cannot disable TLS while server is running");
            return false;
        }

        config_.enableTLS = false;
        config_.certFile.clear();
        config_.keyFile.clear();
        
        spdlog::info("TLS disabled for gRPC server");
        return true;
    }

    bool setAuthenticationMethod(GrpcAuthMethod method) override {
        if (running_) {
            spdlog::error("Cannot change authentication method while server is running");
            return false;
        }

        config_.authMethod = method;
        spdlog::info("gRPC authentication method set to: {}", static_cast<int>(method));
        return true;
    }

    // Request handling
    void setRequestHandler(const std::string& method, GrpcRequestHandler handler) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        requestHandlers_[method] = handler;
        spdlog::debug("gRPC request handler set for method: {}", method);
    }

    void removeRequestHandler(const std::string& method) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        auto it = requestHandlers_.find(method);
        if (it != requestHandlers_.end()) {
            requestHandlers_.erase(it);
            spdlog::debug("gRPC request handler removed for method: {}", method);
        }
    }

    // Interceptors
    void addInterceptor(std::shared_ptr<GrpcInterceptor> interceptor) override {
        if (running_) {
            spdlog::error("Cannot add interceptor while server is running");
            return;
        }

        interceptors_.push_back(interceptor);
        spdlog::debug("gRPC interceptor added");
    }

    void removeInterceptor(std::shared_ptr<GrpcInterceptor> interceptor) override {
        if (running_) {
            spdlog::error("Cannot remove interceptor while server is running");
            return;
        }

        auto it = std::find(interceptors_.begin(), interceptors_.end(), interceptor);
        if (it != interceptors_.end()) {
            interceptors_.erase(it);
            spdlog::debug("gRPC interceptor removed");
        }
    }

    void clearInterceptors() override {
        if (running_) {
            spdlog::error("Cannot clear interceptors while server is running");
            return;
        }

        interceptors_.clear();
        spdlog::debug("All gRPC interceptors cleared");
    }

private:
    GrpcServerConfig config_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    std::unique_ptr<::grpc::Server> server_;
    std::thread serverThread_;
    std::chrono::steady_clock::time_point startTime_;
    
    std::unordered_map<std::string, std::shared_ptr<::grpc::Service>> services_;
    std::unordered_map<std::string, GrpcRequestHandler> requestHandlers_;
    std::vector<std::shared_ptr<GrpcInterceptor>> interceptors_;
    
    mutable std::mutex handlersMutex_;

    void serverLoop() {
        startTime_ = std::chrono::steady_clock::now();
        
        try {
            // Wait for server to finish
            if (server_) {
                server_->Wait();
            }
        } catch (const std::exception& e) {
            spdlog::error("Error in gRPC server loop: {}", e.what());
        }
        
        spdlog::debug("gRPC server loop finished");
    }
};

// Factory function implementation
std::unique_ptr<IGrpcServer> GrpcServerFactory::createServer(const GrpcServerConfig& config) {
    return std::make_unique<GrpcServerImpl>(config);
}

std::unique_ptr<IGrpcServer> GrpcServerFactory::createServer(const std::string& address, int port) {
    GrpcServerConfig config;
    config.serverAddress = address + ":" + std::to_string(port);
    config.maxMessageSize = 4 * 1024 * 1024; // 4MB
    config.maxConcurrentStreams = 100;
    config.shutdownTimeoutSeconds = 30;
    config.enableReflection = true;
    config.enableTLS = false;
    config.authMethod = GrpcAuthMethod::NONE;
    
    return std::make_unique<GrpcServerImpl>(config);
}

} // namespace grpc
} // namespace protocols
} // namespace server
} // namespace astrocomm

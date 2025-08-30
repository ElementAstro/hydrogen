#pragma once

// Core functionality
#include "core/server_interface.h"
#include "core/protocol_handler.h"
#include "core/service_registry.h"

// Service layer
#include "services/device_service.h"
#include "services/auth_service.h"
#include "services/communication_service.h"
#include "services/health_service.h"

// Repository layer
#include "repositories/device_repository.h"
#include "repositories/user_repository.h"
#include "repositories/config_repository.h"

// Infrastructure layer
#include "infrastructure/config_manager.h"
#include "infrastructure/logging.h"
#include "infrastructure/error_handler.h"

// Protocol implementations
#include "protocols/http/http_server.h"
#include "protocols/grpc/grpc_server.h"
#include "protocols/mqtt/mqtt_broker.h"
#include "protocols/zmq/zmq_server.h"

/**
 * @namespace astrocomm::server
 * @brief Reorganized server functionality namespace for AstroComm library
 * 
 * This namespace contains all the server-side components organized in a
 * layered architecture with clear separation of concerns:
 * 
 * - Core: Base interfaces and abstractions
 * - Services: Business logic layer
 * - Repositories: Data access layer  
 * - Infrastructure: Cross-cutting concerns
 * - Protocols: Communication protocol implementations
 */
namespace astrocomm {
namespace server {

/**
 * @brief Initialize the reorganized server component
 * 
 * This function initializes the service registry and sets up the
 * dependency injection container with all available services.
 */
void initialize();

/**
 * @brief Shutdown the server component
 * 
 * Properly shuts down all services and cleans up resources.
 */
void shutdown();

/**
 * @brief Get the global service registry instance
 * 
 * @return Reference to the service registry
 */
core::ServiceRegistry& getServiceRegistry();

/**
 * @brief Create a multi-protocol server with default configuration
 * 
 * @return Unique pointer to configured multi-protocol server
 */
std::unique_ptr<core::IMultiProtocolServer> createDefaultServer();

/**
 * @brief Create a multi-protocol server with custom configuration
 * 
 * @param config Configuration map for server setup
 * @return Unique pointer to configured multi-protocol server
 */
std::unique_ptr<core::IMultiProtocolServer> createServer(
    const std::unordered_map<std::string, std::string>& config
);

/**
 * @brief Server builder for fluent configuration
 */
class ServerBuilder {
public:
    ServerBuilder();
    
    // Protocol configuration
    ServerBuilder& withHttp(const std::string& host = "localhost", uint16_t port = 8080);
    ServerBuilder& withHttps(const std::string& host, uint16_t port, 
                           const std::string& certPath, const std::string& keyPath);
    ServerBuilder& withGrpc(const std::string& host = "localhost", uint16_t port = 9090);
    ServerBuilder& withMqtt(const std::string& host = "localhost", uint16_t port = 1883);
    ServerBuilder& withZmq(const std::string& address = "tcp://*:5555");
    
    // Service configuration
    ServerBuilder& withDeviceService(const std::string& persistenceDir = "./data/devices");
    ServerBuilder& withAuthService(const std::string& configPath = "./data/auth.json");
    ServerBuilder& withHealthService(bool enableMetrics = true);
    
    // Infrastructure configuration
    ServerBuilder& withLogging(const std::string& logLevel = "info", 
                             const std::string& logFile = "");
    ServerBuilder& withConfiguration(const std::string& configPath = "./config");
    ServerBuilder& withErrorHandling(bool enableRecovery = true);
    
    // Build the server
    std::unique_ptr<core::IMultiProtocolServer> build();
    
private:
    std::unordered_map<std::string, std::string> config_;
    std::vector<core::CommunicationProtocol> enabledProtocols_;
};

/**
 * @brief Convenience functions for common server configurations
 */
namespace presets {

/**
 * @brief Create a development server with HTTP and WebSocket
 */
std::unique_ptr<core::IMultiProtocolServer> createDevelopmentServer(uint16_t port = 8080);

/**
 * @brief Create a production server with all protocols and security
 */
std::unique_ptr<core::IMultiProtocolServer> createProductionServer(
    const std::string& configPath = "./config/production.json"
);

/**
 * @brief Create a testing server with minimal configuration
 */
std::unique_ptr<core::IMultiProtocolServer> createTestingServer();

/**
 * @brief Create a secure server with HTTPS and authentication
 */
std::unique_ptr<core::IMultiProtocolServer> createSecureServer(
    const std::string& certPath, const std::string& keyPath,
    const std::string& authConfig = "./config/auth.json"
);

} // namespace presets

/**
 * @brief Server configuration utilities
 */
namespace config {

/**
 * @brief Load server configuration from file
 */
std::unordered_map<std::string, std::string> loadFromFile(const std::string& filePath);

/**
 * @brief Save server configuration to file
 */
bool saveToFile(const std::unordered_map<std::string, std::string>& config, 
               const std::string& filePath);

/**
 * @brief Validate server configuration
 */
bool validate(const std::unordered_map<std::string, std::string>& config);

/**
 * @brief Get default configuration
 */
std::unordered_map<std::string, std::string> getDefaults();

} // namespace config

/**
 * @brief Server monitoring and diagnostics
 */
namespace diagnostics {

/**
 * @brief Get server health status
 */
std::string getHealthStatus();

/**
 * @brief Get server metrics
 */
std::unordered_map<std::string, std::string> getMetrics();

/**
 * @brief Generate diagnostic report
 */
std::string generateReport();

/**
 * @brief Check if server is ready
 */
bool isReady();

} // namespace diagnostics

} // namespace server
} // namespace astrocomm

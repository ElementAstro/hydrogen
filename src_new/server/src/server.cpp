#include "hydrogen/server/server.h"
#include "hydrogen/server/core/service_registry.h"
#include "hydrogen/server/services/device_service.h"
#include "hydrogen/server/services/auth_service.h"
#include "hydrogen/server/services/communication_service.h"
#include "hydrogen/server/services/health_service.h"
#include "hydrogen/server/protocols/http/http_server.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <memory>

namespace hydrogen {
namespace server {

// Global service registry instance
static std::unique_ptr<core::ServiceRegistry> g_serviceRegistry;

/**
 * @brief Multi-protocol server implementation
 */
class MultiProtocolServerImpl : public core::IMultiProtocolServer {
public:
    MultiProtocolServerImpl() = default;
    virtual ~MultiProtocolServerImpl() = default;

    // IMultiProtocolServer implementation
    bool addProtocol(core::CommunicationProtocol protocol, std::shared_ptr<core::IServerInterface> server) override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        if (servers_.find(protocol) != servers_.end()) {
            spdlog::warn("Protocol {} already registered, replacing", static_cast<int>(protocol));
        }
        
        servers_[protocol] = server;
        spdlog::info("Added protocol server: {}", server->getProtocolName());
        return true;
    }

    bool removeProtocol(core::CommunicationProtocol protocol) override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        auto it = servers_.find(protocol);
        if (it != servers_.end()) {
            if (it->second->getStatus() == core::ServerStatus::RUNNING) {
                it->second->stop();
            }
            servers_.erase(it);
            spdlog::info("Removed protocol server: {}", static_cast<int>(protocol));
            return true;
        }
        
        return false;
    }

    std::shared_ptr<core::IServerInterface> getProtocolServer(core::CommunicationProtocol protocol) const override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        auto it = servers_.find(protocol);
        return (it != servers_.end()) ? it->second : nullptr;
    }

    std::vector<core::CommunicationProtocol> getActiveProtocols() const override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        std::vector<core::CommunicationProtocol> protocols;
        protocols.reserve(servers_.size());
        
        for (const auto& pair : servers_) {
            if (pair.second->getStatus() == core::ServerStatus::RUNNING) {
                protocols.push_back(pair.first);
            }
        }
        
        return protocols;
    }

    std::vector<core::CommunicationProtocol> getRegisteredProtocols() const override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        std::vector<core::CommunicationProtocol> protocols;
        protocols.reserve(servers_.size());
        
        for (const auto& pair : servers_) {
            protocols.push_back(pair.first);
        }
        
        return protocols;
    }

    bool isProtocolRegistered(core::CommunicationProtocol protocol) const override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        return servers_.find(protocol) != servers_.end();
    }

    bool isProtocolActive(core::CommunicationProtocol protocol) const override {
        auto server = getProtocolServer(protocol);
        return server && server->getStatus() == core::ServerStatus::RUNNING;
    }

    bool startProtocol(core::CommunicationProtocol protocol) override {
        auto server = getProtocolServer(protocol);
        if (!server) {
            spdlog::error("Protocol not registered: {}", static_cast<int>(protocol));
            return false;
        }
        
        spdlog::info("Starting protocol: {}", server->getProtocolName());
        return server->start();
    }

    bool stopProtocol(core::CommunicationProtocol protocol) override {
        auto server = getProtocolServer(protocol);
        if (!server) {
            spdlog::error("Protocol not registered: {}", static_cast<int>(protocol));
            return false;
        }
        
        spdlog::info("Stopping protocol: {}", server->getProtocolName());
        return server->stop();
    }

    bool restartProtocol(core::CommunicationProtocol protocol) override {
        auto server = getProtocolServer(protocol);
        if (!server) {
            spdlog::error("Protocol not registered: {}", static_cast<int>(protocol));
            return false;
        }
        
        spdlog::info("Restarting protocol: {}", server->getProtocolName());
        return server->restart();
    }

    bool startAll() override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        bool allStarted = true;
        for (auto& pair : servers_) {
            spdlog::info("Starting protocol: {}", pair.second->getProtocolName());
            if (!pair.second->start()) {
                spdlog::error("Failed to start protocol: {}", pair.second->getProtocolName());
                allStarted = false;
            }
        }
        
        if (allStarted) {
            spdlog::info("All protocols started successfully");
        } else {
            spdlog::warn("Some protocols failed to start");
        }
        
        return allStarted;
    }

    bool stopAll() override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        bool allStopped = true;
        for (auto& pair : servers_) {
            spdlog::info("Stopping protocol: {}", pair.second->getProtocolName());
            if (!pair.second->stop()) {
                spdlog::error("Failed to stop protocol: {}", pair.second->getProtocolName());
                allStopped = false;
            }
        }
        
        if (allStopped) {
            spdlog::info("All protocols stopped successfully");
        } else {
            spdlog::warn("Some protocols failed to stop");
        }
        
        return allStopped;
    }

    bool restartAll() override {
        spdlog::info("Restarting all protocols...");
        
        if (!stopAll()) {
            spdlog::error("Failed to stop all protocols during restart");
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        return startAll();
    }

    core::ServerStatus getOverallStatus() const override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        if (servers_.empty()) {
            return core::ServerStatus::STOPPED;
        }
        
        bool anyRunning = false;
        bool anyError = false;
        bool anyStarting = false;
        bool anyStopping = false;
        
        for (const auto& pair : servers_) {
            auto status = pair.second->getStatus();
            switch (status) {
                case core::ServerStatus::RUNNING:
                    anyRunning = true;
                    break;
                case core::ServerStatus::ERROR:
                    anyError = true;
                    break;
                case core::ServerStatus::STARTING:
                    anyStarting = true;
                    break;
                case core::ServerStatus::STOPPING:
                    anyStopping = true;
                    break;
                default:
                    break;
            }
        }
        
        if (anyError) return core::ServerStatus::ERROR;
        if (anyStarting) return core::ServerStatus::STARTING;
        if (anyStopping) return core::ServerStatus::STOPPING;
        if (anyRunning) return core::ServerStatus::RUNNING;
        
        return core::ServerStatus::STOPPED;
    }

    std::unordered_map<core::CommunicationProtocol, core::ServerStatus> getProtocolStatuses() const override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        std::unordered_map<core::CommunicationProtocol, core::ServerStatus> statuses;
        
        for (const auto& pair : servers_) {
            statuses[pair.first] = pair.second->getStatus();
        }
        
        return statuses;
    }

    bool isProtocolHealthy(core::CommunicationProtocol protocol) const override {
        auto server = getProtocolServer(protocol);
        return server && server->isHealthy();
    }

    std::string getProtocolHealthStatus(core::CommunicationProtocol protocol) const override {
        auto server = getProtocolServer(protocol);
        return server ? server->getHealthStatus() : "Protocol not registered";
    }

    std::unordered_map<core::CommunicationProtocol, std::string> getAllHealthStatuses() const override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        std::unordered_map<core::CommunicationProtocol, std::string> healthStatuses;
        
        for (const auto& pair : servers_) {
            healthStatuses[pair.first] = pair.second->getHealthStatus();
        }
        
        return healthStatuses;
    }

    size_t getTotalConnectionCount() const override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        size_t totalConnections = 0;
        for (const auto& pair : servers_) {
            totalConnections += pair.second->getConnectionCount();
        }
        
        return totalConnections;
    }

    size_t getProtocolConnectionCount(core::CommunicationProtocol protocol) const override {
        auto server = getProtocolServer(protocol);
        return server ? server->getConnectionCount() : 0;
    }

    std::vector<core::ConnectionInfo> getAllConnections() const override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        std::vector<core::ConnectionInfo> allConnections;
        
        for (const auto& pair : servers_) {
            auto connections = pair.second->getActiveConnections();
            allConnections.insert(allConnections.end(), connections.begin(), connections.end());
        }
        
        return allConnections;
    }

    std::vector<core::ConnectionInfo> getProtocolConnections(core::CommunicationProtocol protocol) const override {
        auto server = getProtocolServer(protocol);
        return server ? server->getActiveConnections() : std::vector<core::ConnectionInfo>{};
    }

    bool disconnectClient(const std::string& clientId, core::CommunicationProtocol protocol) override {
        auto server = getProtocolServer(protocol);
        return server && server->disconnectClient(clientId);
    }

    bool sendMessage(const core::Message& message) override {
        auto server = getProtocolServer(message.sourceProtocol);
        if (!server) {
            spdlog::error("No server for protocol: {}", static_cast<int>(message.sourceProtocol));
            return false;
        }
        
        // In a full implementation, this would route the message appropriately
        spdlog::debug("Sending message via protocol: {}", server->getProtocolName());
        return true;
    }

    bool broadcastMessage(const core::Message& message, const std::vector<core::CommunicationProtocol>& protocols) override {
        bool success = true;
        
        auto targetProtocols = protocols.empty() ? getActiveProtocols() : protocols;
        
        for (auto protocol : targetProtocols) {
            auto server = getProtocolServer(protocol);
            if (server) {
                // In a full implementation, this would broadcast the message
                spdlog::debug("Broadcasting message via protocol: {}", server->getProtocolName());
            } else {
                success = false;
            }
        }
        
        return success;
    }

    void setGlobalConnectionCallback(core::IServerInterface::ConnectionCallback callback) override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        for (auto& pair : servers_) {
            pair.second->setConnectionCallback(callback);
        }
    }

    void setGlobalMessageCallback(core::IServerInterface::MessageCallback callback) override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        for (auto& pair : servers_) {
            pair.second->setMessageCallback(callback);
        }
    }

    void setGlobalErrorCallback(core::IServerInterface::ErrorCallback callback) override {
        std::lock_guard<std::mutex> lock(serversMutex_);
        
        for (auto& pair : servers_) {
            pair.second->setErrorCallback(callback);
        }
    }

    void setConfiguration(const std::unordered_map<std::string, std::string>& config) override {
        config_ = config;
        
        // Apply configuration to all servers
        std::lock_guard<std::mutex> lock(serversMutex_);
        for (auto& pair : servers_) {
            core::ServerConfig serverConfig;
            serverConfig.host = getConfigValue("host", "localhost");
            serverConfig.port = std::stoi(getConfigValue("port", "8080"));
            serverConfig.enableSsl = getConfigValue("enable_ssl", "false") == "true";
            serverConfig.maxConnections = std::stoi(getConfigValue("max_connections", "1000"));
            
            pair.second->setConfig(serverConfig);
        }
    }

    std::unordered_map<std::string, std::string> getConfiguration() const override {
        return config_;
    }

private:
    mutable std::mutex serversMutex_;
    std::unordered_map<core::CommunicationProtocol, std::shared_ptr<core::IServerInterface>> servers_;
    std::unordered_map<std::string, std::string> config_;

    std::string getConfigValue(const std::string& key, const std::string& defaultValue = "") const {
        auto it = config_.find(key);
        return (it != config_.end()) ? it->second : defaultValue;
    }
};

// Global functions implementation
void initialize() {
    if (!g_serviceRegistry) {
        g_serviceRegistry = std::make_unique<core::ServiceRegistry>();
        
        // Register service factories
        g_serviceRegistry->registerFactory(std::make_unique<services::DeviceServiceFactory>());
        g_serviceRegistry->registerFactory(std::make_unique<services::AuthServiceFactory>());
        
        spdlog::info("Hydrogen Server component initialized");
    }
}

void shutdown() {
    if (g_serviceRegistry) {
        g_serviceRegistry->shutdownAllServices();
        g_serviceRegistry.reset();
        spdlog::info("Hydrogen Server component shutdown");
    }
}

core::ServiceRegistry& getServiceRegistry() {
    if (!g_serviceRegistry) {
        initialize();
    }
    return *g_serviceRegistry;
}

std::unique_ptr<core::IMultiProtocolServer> createDefaultServer() {
    std::unordered_map<std::string, std::string> defaultConfig = {
        {"host", "localhost"},
        {"http_port", "8080"},
        {"grpc_port", "9090"},
        {"mqtt_port", "1883"},
        {"zmq_address", "tcp://*:5555"},
        {"enable_ssl", "false"},
        {"max_connections", "1000"}
    };
    
    return createServer(defaultConfig);
}

std::unique_ptr<core::IMultiProtocolServer> createServer(
    const std::unordered_map<std::string, std::string>& config) {
    
    auto server = std::make_unique<MultiProtocolServerImpl>();
    server->setConfiguration(config);
    
    // Create and add HTTP server by default
    protocols::http::HttpServerConfig httpConfig;
    httpConfig.host = config.count("host") ? config.at("host") : "localhost";
    httpConfig.port = config.count("http_port") ? static_cast<uint16_t>(std::stoi(config.at("http_port"))) : 8080;
    httpConfig.enableSsl = config.count("enable_ssl") ? (config.at("enable_ssl") == "true") : false;
    httpConfig.maxConnections = config.count("max_connections") ? std::stoi(config.at("max_connections")) : 1000;
    
    auto httpServer = std::make_shared<protocols::http::HttpServer>(httpConfig);
    
    // Integrate services with HTTP server
    auto& registry = getServiceRegistry();
    
    // Create and register services if they don't exist
    if (!registry.isServiceRegistered("DeviceService")) {
        auto deviceService = registry.createService("DeviceService", config);
        if (deviceService) {
            registry.registerService("DeviceService", deviceService);
        }
    }
    
    if (!registry.isServiceRegistered("AuthService")) {
        auto authService = registry.createService("AuthService", config);
        if (authService) {
            registry.registerService("AuthService", authService);
        }
    }
    
    // Set services on HTTP server
    auto deviceService = std::dynamic_pointer_cast<services::IDeviceService>(
        registry.getService("DeviceService"));
    auto authService = std::dynamic_pointer_cast<services::IAuthService>(
        registry.getService("AuthService"));
    
    if (deviceService) {
        httpServer->setDeviceService(deviceService);
    }
    
    if (authService) {
        httpServer->setAuthService(authService);
    }
    
    server->addProtocol(core::CommunicationProtocol::HTTP, httpServer);
    
    return server;
}

// ServerBuilder Implementation
ServerBuilder::ServerBuilder() {
    config_["host"] = "localhost";
    config_["enable_ssl"] = "false";
    config_["max_connections"] = "1000";
}

ServerBuilder& ServerBuilder::withHttp(const std::string& host, uint16_t port) {
    config_["host"] = host;
    config_["http_port"] = std::to_string(port);
    enabledProtocols_.push_back(core::CommunicationProtocol::HTTP);
    return *this;
}

ServerBuilder& ServerBuilder::withHttps(const std::string& host, uint16_t port, 
                                       const std::string& certPath, const std::string& keyPath) {
    config_["host"] = host;
    config_["http_port"] = std::to_string(port);
    config_["enable_ssl"] = "true";
    config_["ssl_cert_path"] = certPath;
    config_["ssl_key_path"] = keyPath;
    enabledProtocols_.push_back(core::CommunicationProtocol::HTTP);
    return *this;
}

ServerBuilder& ServerBuilder::withGrpc(const std::string& host, uint16_t port) {
    config_["host"] = host;
    config_["grpc_port"] = std::to_string(port);
    enabledProtocols_.push_back(core::CommunicationProtocol::GRPC);
    return *this;
}

ServerBuilder& ServerBuilder::withMqtt(const std::string& host, uint16_t port) {
    config_["host"] = host;
    config_["mqtt_port"] = std::to_string(port);
    enabledProtocols_.push_back(core::CommunicationProtocol::MQTT);
    return *this;
}

ServerBuilder& ServerBuilder::withZmq(const std::string& address) {
    config_["zmq_address"] = address;
    enabledProtocols_.push_back(core::CommunicationProtocol::ZMQ_REQ_REP);
    return *this;
}

ServerBuilder& ServerBuilder::withDeviceService(const std::string& persistenceDir) {
    config_["device_persistence_dir"] = persistenceDir;
    return *this;
}

ServerBuilder& ServerBuilder::withAuthService(const std::string& configPath) {
    config_["auth_config_path"] = configPath;
    return *this;
}

ServerBuilder& ServerBuilder::withHealthService(bool enableMetrics) {
    config_["health_enable_metrics"] = enableMetrics ? "true" : "false";
    return *this;
}

ServerBuilder& ServerBuilder::withLogging(const std::string& logLevel, const std::string& logFile) {
    config_["log_level"] = logLevel;
    if (!logFile.empty()) {
        config_["log_file"] = logFile;
    }
    return *this;
}

ServerBuilder& ServerBuilder::withConfiguration(const std::string& configPath) {
    config_["config_path"] = configPath;
    return *this;
}

ServerBuilder& ServerBuilder::withErrorHandling(bool enableRecovery) {
    config_["error_recovery_enabled"] = enableRecovery ? "true" : "false";
    return *this;
}

std::unique_ptr<core::IMultiProtocolServer> ServerBuilder::build() {
    return createServer(config_);
}

// Preset functions implementation
namespace presets {

std::unique_ptr<core::IMultiProtocolServer> createDevelopmentServer(uint16_t port) {
    return ServerBuilder()
        .withHttp("localhost", port)
        .withDeviceService("./data/devices")
        .withAuthService("./data/auth.json")
        .withLogging("debug")
        .build();
}

std::unique_ptr<core::IMultiProtocolServer> createProductionServer(const std::string& configPath) {
    // In a real implementation, this would load configuration from file
    return ServerBuilder()
        .withHttp("0.0.0.0", 8080)
        .withGrpc("0.0.0.0", 9090)
        .withMqtt("0.0.0.0", 1883)
        .withDeviceService("/var/lib/Hydrogen/devices")
        .withAuthService("/etc/Hydrogen/auth.json")
        .withHealthService(true)
        .withLogging("info", "/var/log/Hydrogen/server.log")
        .withConfiguration("/etc/Hydrogen")
        .withErrorHandling(true)
        .build();
}

std::unique_ptr<core::IMultiProtocolServer> createTestingServer() {
    return ServerBuilder()
        .withHttp("127.0.0.1", 0) // Use random port
        .withLogging("error")
        .build();
}

std::unique_ptr<core::IMultiProtocolServer> createSecureServer(
    const std::string& certPath, const std::string& keyPath,
    const std::string& authConfig) {
    
    return ServerBuilder()
        .withHttps("0.0.0.0", 8443, certPath, keyPath)
        .withDeviceService("./data/devices")
        .withAuthService(authConfig)
        .withHealthService(true)
        .withLogging("info")
        .withErrorHandling(true)
        .build();
}

} // namespace presets

// Config utility functions implementation
namespace config {

std::unordered_map<std::string, std::string> loadFromFile(const std::string& filePath) {
    // Placeholder implementation - would load from JSON/YAML file
    spdlog::debug("Loading configuration from: {}", filePath);
    return getDefaults();
}

bool saveToFile(const std::unordered_map<std::string, std::string>& config, const std::string& filePath) {
    // Placeholder implementation - would save to JSON/YAML file
    spdlog::debug("Saving configuration to: {}", filePath);
    return true;
}

bool validate(const std::unordered_map<std::string, std::string>& config) {
    // Basic validation
    if (config.count("host") && config.at("host").empty()) {
        return false;
    }
    
    if (config.count("http_port")) {
        try {
            int port = std::stoi(config.at("http_port"));
            if (port <= 0 || port > 65535) {
                return false;
            }
        } catch (const std::exception&) {
            return false;
        }
    }
    
    return true;
}

std::unordered_map<std::string, std::string> getDefaults() {
    return {
        {"host", "localhost"},
        {"http_port", "8080"},
        {"grpc_port", "9090"},
        {"mqtt_port", "1883"},
        {"zmq_address", "tcp://*:5555"},
        {"enable_ssl", "false"},
        {"max_connections", "1000"},
        {"log_level", "info"},
        {"device_persistence_dir", "./data/devices"},
        {"auth_config_path", "./data/auth.json"},
        {"health_enable_metrics", "true"},
        {"error_recovery_enabled", "true"}
    };
}

} // namespace config

// Diagnostics utility functions implementation
namespace diagnostics {

std::string getHealthStatus() {
    auto& registry = getServiceRegistry();
    auto healthStatus = registry.getServiceHealthStatus();
    
    bool allHealthy = true;
    for (const auto& pair : healthStatus) {
        if (!pair.second) {
            allHealthy = false;
            break;
        }
    }
    
    return allHealthy ? "healthy" : "unhealthy";
}

std::unordered_map<std::string, std::string> getMetrics() {
    std::unordered_map<std::string, std::string> metrics;
    
    auto& registry = getServiceRegistry();
    auto services = registry.getRegisteredServices();
    
    metrics["total_services"] = std::to_string(services.size());
    metrics["health_status"] = getHealthStatus();
    
    return metrics;
}

std::string generateReport() {
    std::stringstream report;
    report << "=== Hydrogen Server Diagnostic Report ===\n";
    report << "Health Status: " << getHealthStatus() << "\n";
    
    auto metrics = getMetrics();
    report << "Metrics:\n";
    for (const auto& pair : metrics) {
        report << "  " << pair.first << ": " << pair.second << "\n";
    }
    
    return report.str();
}

bool isReady() {
    return getHealthStatus() == "healthy";
}

} // namespace diagnostics

} // namespace server
} // namespace hydrogen

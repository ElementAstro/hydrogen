/**
 * @file unified_client_example.cpp
 * @brief Comprehensive example demonstrating the Hydrogen Unified Architecture
 * 
 * This example shows how to use the new unified components:
 * - UnifiedDeviceClient for device interactions
 * - UnifiedConnectionManager for connection management
 * - ClientConfiguration for configuration management
 * - UnifiedWebSocketErrorHandler for error handling
 */

#include <hydrogen/core/unified_device_client.h>
#include <hydrogen/core/unified_connection_manager.h>
#include <hydrogen/core/client_configuration.h>
#include <hydrogen/core/unified_websocket_error_handler.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace hydrogen::core;

class UnifiedClientExample {
public:
    UnifiedClientExample() {
        setupLogging();
        setupConfiguration();
        setupErrorHandling();
    }
    
    ~UnifiedClientExample() {
        cleanup();
    }
    
    void run() {
        std::cout << "🚀 Hydrogen Unified Architecture Example\n" << std::endl;
        
        try {
            // Demonstrate configuration management
            demonstrateConfiguration();
            
            // Demonstrate connection management
            demonstrateConnectionManagement();
            
            // Demonstrate unified device client
            demonstrateUnifiedDeviceClient();
            
            // Demonstrate error handling
            demonstrateErrorHandling();
            
            // Show performance monitoring
            demonstratePerformanceMonitoring();
            
            std::cout << "\n✅ Example completed successfully!" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Example failed: " << e.what() << std::endl;
        }
    }

private:
    std::unique_ptr<ClientConfiguration> config_;
    std::unique_ptr<UnifiedConnectionManager> connectionManager_;
    std::unique_ptr<UnifiedDeviceClient> deviceClient_;
    std::shared_ptr<UnifiedWebSocketErrorHandler> errorHandler_;
    
    void setupLogging() {
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        std::cout << "📝 Logging configured" << std::endl;
    }
    
    void setupConfiguration() {
        std::cout << "\n🔧 Setting up configuration..." << std::endl;
        
        // Create configuration using builder pattern
        config_ = ConfigurationBuilder()
            .withHost("localhost")
            .withPort(8080)
            .withEndpoint("/ws")
            .withTimeout(std::chrono::milliseconds{5000})
            .withLogLevel(LoggingConfig::LogLevel::INFO)
            .withFeature("auto_reconnect", true)
            .withFeature("device_discovery", true)
            .withFeature("heartbeat", true)
            .withWorkerThreads(4)
            .withMaxQueueSize(1000)
            .build();
        
        // Validate configuration
        auto validation = config_->validate();
        if (!validation.isValid) {
            std::cerr << "Configuration validation failed:" << std::endl;
            for (const auto& error : validation.errors) {
                std::cerr << "  - " << error << std::endl;
            }
            throw std::runtime_error("Invalid configuration");
        }
        
        std::cout << "✅ Configuration validated successfully" << std::endl;
        
        // Show configuration details
        std::cout << "Configuration details:" << std::endl;
        std::cout << "  Host: " << config_->network.host << std::endl;
        std::cout << "  Port: " << config_->network.port << std::endl;
        std::cout << "  Protocol: " << static_cast<int>(config_->defaultProtocol) << std::endl;
        std::cout << "  Features: " << config_->getEnabledFeatures().size() << " enabled" << std::endl;
    }
    
    void setupErrorHandling() {
        std::cout << "\n🛡️ Setting up unified error handling..." << std::endl;
        
        // Create error handler
        errorHandler_ = UnifiedWebSocketErrorHandlerFactory::createClientHandler();
        
        // Set up error event callback
        errorHandler_->setErrorEventCallback([](const WebSocketErrorEvent& event) {
            std::cout << "🚨 Error Event: " << event.error.message 
                      << " (Action: " << static_cast<int>(event.actionTaken) << ")" << std::endl;
        });
        
        // Set up custom recovery strategy
        errorHandler_->setRecoveryStrategyCallback([](const EnhancedWebSocketError& error) -> WebSocketRecoveryAction {
            if (error.category == WebSocketErrorCategory::CONNECTION) {
                return WebSocketRecoveryAction::RECONNECT;
            } else if (error.category == WebSocketErrorCategory::TIMEOUT) {
                return WebSocketRecoveryAction::RETRY;
            }
            return WebSocketRecoveryAction::NONE;
        });
        
        // Register with global registry
        UnifiedWebSocketErrorRegistry::getInstance().setGlobalHandler(errorHandler_);
        
        std::cout << "✅ Error handling configured" << std::endl;
    }
    
    void demonstrateConfiguration() {
        std::cout << "\n📋 Demonstrating Configuration Management..." << std::endl;
        
        // Show different configuration creation methods
        auto defaultConfig = ClientConfiguration::createDefault();
        auto secureConfig = ClientConfiguration::createSecure();
        auto performanceConfig = ClientConfiguration::createHighPerformance();
        
        std::cout << "Created configuration templates:" << std::endl;
        std::cout << "  Default: " << (defaultConfig.network.useTls ? "TLS" : "Plain") << std::endl;
        std::cout << "  Secure: " << (secureConfig.network.useTls ? "TLS" : "Plain") << std::endl;
        std::cout << "  Performance: " << performanceConfig.performance.workerThreads << " threads" << std::endl;
        
        // Demonstrate configuration merging
        ClientConfiguration merged = defaultConfig;
        merged.merge(secureConfig);
        
        std::cout << "Merged configuration uses TLS: " << (merged.network.useTls ? "Yes" : "No") << std::endl;
        
        // Show environment variable support
        config_->loadFromEnvironment("HYDROGEN_");
        std::cout << "Environment variables loaded" << std::endl;
    }
    
    void demonstrateConnectionManagement() {
        std::cout << "\n🔗 Demonstrating Connection Management..." << std::endl;
        
        // Create connection manager
        connectionManager_ = ConnectionManagerFactory::createManagerWithDefaults();
        
        // Create different types of connections
        auto wsConfig = ConnectionManagerFactory::createWebSocketConfig("localhost", 8080);
        auto httpConfig = ConnectionManagerFactory::createHttpConfig("localhost", 8080);
        
        std::string wsConnId = connectionManager_->createConnection(wsConfig);
        std::string httpConnId = connectionManager_->createConnection(httpConfig);
        
        std::cout << "Created connections:" << std::endl;
        std::cout << "  WebSocket: " << wsConnId << std::endl;
        std::cout << "  HTTP: " << httpConnId << std::endl;
        
        // Show connection statistics
        auto stats = connectionManager_->getAllStatistics();
        std::cout << "Connection statistics:" << std::endl;
        std::cout << "  Total connections: " << stats.totalConnections << std::endl;
        std::cout << "  Active connections: " << stats.activeConnections << std::endl;
        std::cout << "  Messages sent: " << stats.messagesSent << std::endl;
        std::cout << "  Messages received: " << stats.messagesReceived << std::endl;
    }
    
    void demonstrateUnifiedDeviceClient() {
        std::cout << "\n🎛️ Demonstrating Unified Device Client..." << std::endl;
        
        // Create unified device client
        ClientConnectionConfig clientConfig;
        clientConfig.host = config_->network.host;
        clientConfig.port = config_->network.port;
        clientConfig.endpoint = config_->network.endpoint;
        clientConfig.connectTimeout = config_->network.connectTimeout;
        clientConfig.messageTimeout = config_->network.readTimeout;
        clientConfig.enableAutoReconnect = config_->isFeatureEnabled("auto_reconnect");
        
        deviceClient_ = std::make_unique<UnifiedDeviceClient>(clientConfig);
        
        // Attempt to connect (will likely fail in example, but shows the API)
        std::cout << "Attempting to connect to device server..." << std::endl;
        bool connected = deviceClient_->connect();
        
        if (connected) {
            std::cout << "✅ Connected to device server" << std::endl;
            
            // Discover devices
            std::cout << "Discovering devices..." << std::endl;
            json devices = deviceClient_->discoverDevices();
            std::cout << "Found " << devices.size() << " devices" << std::endl;
            
            // Example device interactions (would work with real server)
            try {
                auto properties = deviceClient_->getDeviceProperties("camera_001");
                std::cout << "Camera properties retrieved" << std::endl;
                
                json commandResult = deviceClient_->executeCommand("camera_001", "get_status", json::object());
                std::cout << "Command executed successfully" << std::endl;
                
            } catch (const std::exception& e) {
                std::cout << "Device interaction failed (expected in example): " << e.what() << std::endl;
            }
            
        } else {
            std::cout << "⚠️ Connection failed (expected in example without server)" << std::endl;
        }
        
        // Show client statistics
        auto clientStats = deviceClient_->getStatistics();
        std::cout << "Client statistics:" << std::endl;
        std::cout << "  Connection attempts: " << clientStats.connectionAttempts << std::endl;
        std::cout << "  Messages sent: " << clientStats.messagesSent << std::endl;
        std::cout << "  Messages received: " << clientStats.messagesReceived << std::endl;
    }
    
    void demonstrateErrorHandling() {
        std::cout << "\n🚨 Demonstrating Error Handling..." << std::endl;
        
        // Create a sample error
        WebSocketError sampleError = WebSocketErrorFactory::createConnectionError(
            "Sample connection timeout", "UnifiedClientExample");
        
        // Handle the error
        errorHandler_->handleError(sampleError);
        
        // Show error statistics
        auto errorStats = errorHandler_->getStatistics();
        std::cout << "Error statistics:" << std::endl;
        std::cout << "  Total errors: " << errorStats.totalErrors << std::endl;
        std::cout << "  Connection errors: " << errorStats.connectionErrors << std::endl;
        std::cout << "  Successful recoveries: " << errorStats.successfulRecoveries << std::endl;
        
        // Generate error report
        auto errorReport = errorHandler_->generateErrorReport();
        std::cout << "Error report generated with " << errorReport.size() << " entries" << std::endl;
        
        // Show top error patterns
        auto topPatterns = errorHandler_->getTopErrorPatterns(3);
        std::cout << "Top error patterns:" << std::endl;
        for (size_t i = 0; i < topPatterns.size(); ++i) {
            std::cout << "  " << (i + 1) << ". " << topPatterns[i] << std::endl;
        }
    }
    
    void demonstratePerformanceMonitoring() {
        std::cout << "\n📊 Demonstrating Performance Monitoring..." << std::endl;
        
        // Simulate some operations for metrics
        auto start = std::chrono::high_resolution_clock::now();
        
        // Simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Simulated operation took: " << duration.count() << "ms" << std::endl;
        
        // Show connection manager performance
        if (connectionManager_) {
            auto perfStats = connectionManager_->getPerformanceStatistics();
            std::cout << "Connection performance:" << std::endl;
            std::cout << "  Average response time: " << perfStats.averageResponseTime.count() << "ms" << std::endl;
            std::cout << "  Messages per second: " << perfStats.messagesPerSecond << std::endl;
            std::cout << "  Error rate: " << (perfStats.errorRate * 100) << "%" << std::endl;
        }
        
        // Show memory usage (simplified)
        std::cout << "Memory usage monitoring enabled" << std::endl;
    }
    
    void cleanup() {
        std::cout << "\n🧹 Cleaning up..." << std::endl;
        
        if (deviceClient_) {
            deviceClient_->disconnect();
            deviceClient_.reset();
        }
        
        if (connectionManager_) {
            connectionManager_->shutdown();
            connectionManager_.reset();
        }
        
        if (errorHandler_) {
            errorHandler_->resetStatistics();
        }
        
        std::cout << "✅ Cleanup completed" << std::endl;
    }
};

int main() {
    try {
        UnifiedClientExample example;
        example.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

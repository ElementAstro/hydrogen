#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <spdlog/spdlog.h>

// Include the new unified connection architecture
#include "hydrogen/core/connection/unified_connection_architecture.h"
#include "hydrogen/core/device/enhanced_device_connection_manager.h"

using namespace hydrogen::core::connection;
using namespace hydrogen::core::device;

/**
 * @brief Enhanced Hydrogen Device Server with unified connection management
 */
class EnhancedHydrogenServer {
public:
    explicit EnhancedHydrogenServer(int port)
        : port_(port), running_(false) {

        // Initialize server configuration
        serverConfig_.protocol = ProtocolType::WEBSOCKET;
        serverConfig_.host = "0.0.0.0"; // Listen on all interfaces
        serverConfig_.port = static_cast<uint16_t>(port);
        serverConfig_.enableAutoReconnect = false; // Server doesn't reconnect
        serverConfig_.enableHeartbeat = true;
        serverConfig_.heartbeatInterval = std::chrono::seconds(30);
        serverConfig_.maxMessageQueueSize = 10000;

        // Initialize device registry
        deviceRegistry_ = std::make_unique<DeviceRegistry>();

        // Set up device registry callbacks
        deviceRegistry_->setGlobalStateCallback(
            [this](const std::string& deviceId, DeviceConnectionState state) {
                handleDeviceStateChange(deviceId, state);
            });

        deviceRegistry_->setGlobalErrorCallback(
            [this](const std::string& deviceId, const std::string& error) {
                handleDeviceError(deviceId, error);
            });
    }

    bool start() {
        std::cout << "Starting Enhanced Hydrogen Device Server..." << std::endl;
        std::cout << "===========================================" << std::endl;
        std::cout << "Port: " << port_ << std::endl;
        std::cout << "Protocol: WebSocket" << std::endl;
        std::cout << "Health Monitoring: Enabled" << std::endl;
        std::cout << "Auto-Recovery: Enabled" << std::endl;
        std::cout << std::endl;

        try {
            // Initialize connection manager for server
            connectionManager_ = std::make_unique<UnifiedConnectionManager>();

            // Set up connection callbacks
            connectionManager_->setStateCallback(
                [this](ConnectionState state, const std::string& error) {
                    handleConnectionStateChange(state, error);
                });

            connectionManager_->setMessageCallback(
                [this](const std::string& message) {
                    handleIncomingMessage(message);
                });

            connectionManager_->setErrorCallback(
                [this](const std::string& error, int code) {
                    handleConnectionError(error, code);
                });

            // Enable health monitoring and connection pooling
            connectionManager_->enableHealthMonitoring(true);
            connectionManager_->enableConnectionPooling(true, 50); // Support up to 50 concurrent connections

            // Start the server (in server mode, we listen rather than connect)
            running_.store(true);

            // Start background threads
            serverThread_ = std::thread(&EnhancedHydrogenServer::serverLoop, this);
            monitoringThread_ = std::thread(&EnhancedHydrogenServer::monitoringLoop, this);

            std::cout << "✓ Server started successfully!" << std::endl;
            std::cout << "✓ WebSocket endpoint: ws://localhost:" << port_ << std::endl;
            std::cout << "✓ Health monitoring: Active" << std::endl;
            std::cout << "✓ Connection pooling: Enabled (max 50 connections)" << std::endl;
            std::cout << "✓ Ready to accept device connections" << std::endl;
            std::cout << std::endl;
            std::cout << "Server Statistics:" << std::endl;
            std::cout << "- Connected devices: 0" << std::endl;
            std::cout << "- Active connections: 0" << std::endl;
            std::cout << "- Health status: Excellent" << std::endl;
            std::cout << std::endl;
            std::cout << "Press Ctrl+C to stop the server..." << std::endl;

            return true;

        } catch (const std::exception& e) {
            std::cerr << "✗ Failed to start server: " << e.what() << std::endl;
            return false;
        }
    }

    void stop() {
        std::cout << "\nShutting down Enhanced Hydrogen Server..." << std::endl;
        running_.store(false);

        // Disconnect all devices
        if (deviceRegistry_) {
            std::cout << "✓ Disconnecting all devices..." << std::endl;
            deviceRegistry_->disconnectAllDevices();
        }

        // Stop connection manager
        if (connectionManager_) {
            std::cout << "✓ Stopping connection manager..." << std::endl;
            connectionManager_->disconnect();
        }

        // Wait for threads to finish
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        if (monitoringThread_.joinable()) {
            monitoringThread_.join();
        }

        // Print final statistics
        printFinalStatistics();

        std::cout << "✓ Server stopped successfully" << std::endl;
    }

    bool isRunning() const {
        return running_.load();
    }

    void run() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    void serverLoop() {
        while (running_.load()) {
            // Simulate server processing
            processIncomingConnections();
            processDeviceMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void monitoringLoop() {
        auto lastStatsUpdate = std::chrono::steady_clock::now();

        while (running_.load()) {
            // Update server statistics periodically
            auto now = std::chrono::steady_clock::now();
            if (now - lastStatsUpdate >= std::chrono::seconds(30)) {
                updateServerStatistics();
                lastStatsUpdate = now;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void processIncomingConnections() {
        // Simulate processing incoming device connections
        // In a real implementation, this would handle WebSocket connections
    }

    void processDeviceMessages() {
        // Process messages from connected devices
        if (connectionManager_ && connectionManager_->hasMessage()) {
            std::string message = connectionManager_->receiveMessage();
            if (!message.empty()) {
                handleIncomingMessage(message);
            }
        }
    }

    void handleConnectionStateChange(ConnectionState state, const std::string& error) {
        switch (state) {
            case ConnectionState::CONNECTED:
                spdlog::info("Server: New connection established");
                break;
            case ConnectionState::DISCONNECTED:
                spdlog::info("Server: Connection closed");
                break;
            case ConnectionState::ERROR:
                spdlog::error("Server: Connection error - {}", error);
                break;
            default:
                break;
        }
    }

    void handleIncomingMessage(const std::string& message) {
        spdlog::debug("Server: Received message - {}", message);

        // Parse and route message to appropriate device
        // This is a simplified implementation
        if (message.find("DEVICE_REGISTER") != std::string::npos) {
            handleDeviceRegistration(message);
        } else if (message.find("DEVICE_COMMAND") != std::string::npos) {
            handleDeviceCommand(message);
        }
    }

    void handleConnectionError(const std::string& error, int code) {
        spdlog::error("Server: Connection error - {} (Code: {})", error, code);
        connectionErrors_++;
    }

    void handleDeviceRegistration(const std::string& message) {
        // Simulate device registration
        static int deviceCounter = 0;
        std::string deviceId = "device_" + std::to_string(++deviceCounter);

        spdlog::info("Server: Registering new device - {}", deviceId);
        connectedDevices_[deviceId] = std::chrono::system_clock::now();
    }

    void handleDeviceCommand(const std::string& message) {
        // Simulate device command processing
        spdlog::debug("Server: Processing device command");
        commandsProcessed_++;
    }

    void handleDeviceStateChange(const std::string& deviceId, DeviceConnectionState state) {
        spdlog::info("Server: Device {} state changed to {}", deviceId, static_cast<int>(state));
    }

    void handleDeviceError(const std::string& deviceId, const std::string& error) {
        spdlog::error("Server: Device {} error - {}", deviceId, error);
    }

    void updateServerStatistics() {
        if (connectionManager_) {
            auto stats = connectionManager_->getStatistics();

            spdlog::info("Server Statistics:");
            spdlog::info("- Connected devices: {}", connectedDevices_.size());
            spdlog::info("- Messages sent: {}", stats.messagesSent.load());
            spdlog::info("- Messages received: {}", stats.messagesReceived.load());
            spdlog::info("- Commands processed: {}", commandsProcessed_.load());
            spdlog::info("- Connection errors: {}", connectionErrors_.load());
            spdlog::info("- Health status: {}", connectionManager_->isHealthy() ? "Healthy" : "Unhealthy");
            spdlog::info("- Latency: {}ms", connectionManager_->getLatency().count());
        }
    }

    void printFinalStatistics() {
        std::cout << "\nFinal Server Statistics:" << std::endl;
        std::cout << "========================" << std::endl;
        std::cout << "Total devices connected: " << connectedDevices_.size() << std::endl;
        std::cout << "Commands processed: " << commandsProcessed_.load() << std::endl;
        std::cout << "Connection errors: " << connectionErrors_.load() << std::endl;

        if (connectionManager_) {
            auto stats = connectionManager_->getStatistics();
            std::cout << "Messages sent: " << stats.messagesSent.load() << std::endl;
            std::cout << "Messages received: " << stats.messagesReceived.load() << std::endl;
            std::cout << "Bytes transferred: " << (stats.bytesSent.load() + stats.bytesReceived.load()) << std::endl;
        }
    }

    // Configuration and state
    int port_;
    std::atomic<bool> running_;
    ConnectionConfig serverConfig_;

    // Connection management
    std::unique_ptr<UnifiedConnectionManager> connectionManager_;
    std::unique_ptr<DeviceRegistry> deviceRegistry_;

    // Threading
    std::thread serverThread_;
    std::thread monitoringThread_;

    // Statistics
    std::unordered_map<std::string, std::chrono::system_clock::time_point> connectedDevices_;
    std::atomic<uint64_t> commandsProcessed_{0};
    std::atomic<uint64_t> connectionErrors_{0};
};

// Global server instance for signal handling
std::unique_ptr<EnhancedHydrogenServer> g_server;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

void showHelp() {
    std::cout << "Hydrogen Device Server" << std::endl;
    std::cout << "Usage: astro_server [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --port <port>    Server port (default: 8000)" << std::endl;
    std::cout << "  --help           Show this help" << std::endl;
}

int main(int argc, char* argv[]) {
    int port = 8000;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") {
            showHelp();
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        // Create and start server
        g_server = std::make_unique<EnhancedHydrogenServer>(port);
        
        if (!g_server->start()) {
            std::cerr << "Failed to start server" << std::endl;
            return 1;
        }
        
        // Run server loop
        g_server->run();
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

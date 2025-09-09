#include <iomanip>
#include <iostream>
#include <memory>
#include <atomic>
#include <csignal>
#include <spdlog/spdlog.h>

// Include the new unified connection architecture
#include "hydrogen/core/connection/unified_connection_architecture.h"
#include "hydrogen/core/device/enhanced_device_connection_manager.h"

using namespace hydrogen::core::connection;
using namespace hydrogen::core::device;

// 命令行界面颜色
namespace Color {
const std::string RESET = "\033[0m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN = "\033[36m";
const std::string WHITE = "\033[37m";
const std::string BOLD = "\033[1m";
} // namespace Color

/**
 * @brief Enhanced Hydrogen Device Client Application
 */
class EnhancedHydrogenClient {
public:
    EnhancedHydrogenClient(const std::string& serverHost, uint16_t serverPort)
        : serverHost_(serverHost), serverPort_(serverPort), running_(false) {
        
        // Configure connection settings
        connectionConfig_.protocol = ProtocolType::WEBSOCKET;
        connectionConfig_.host = serverHost;
        connectionConfig_.port = serverPort;
        connectionConfig_.enableAutoReconnect = true;
        connectionConfig_.maxRetries = 3;
        connectionConfig_.retryInterval = std::chrono::seconds(5);
        connectionConfig_.enableHeartbeat = true;
        connectionConfig_.heartbeatInterval = std::chrono::seconds(30);
        
        // Create connection manager
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
    }
    
    ~EnhancedHydrogenClient() {
        disconnect();
    }
    
    bool connect() {
        std::cout << "Connecting to Hydrogen server at " << serverHost_ << ":" << serverPort_ << "..." << std::endl;
        
        if (!connectionManager_->connect(connectionConfig_)) {
            std::cerr << "✗ Failed to connect to server" << std::endl;
            return false;
        }
        
        running_.store(true);
        std::cout << "✓ Connected to server successfully" << std::endl;
        return true;
    }
    
    void disconnect() {
        if (!running_.load()) {
            return;
        }
        
        running_.store(false);
        
        if (connectionManager_) {
            connectionManager_->disconnect();
        }
        
        std::cout << "✓ Disconnected from server" << std::endl;
    }
    
    bool isConnected() const {
        return running_.load() && connectionManager_ && connectionManager_->isConnected();
    }
    
    void runInteractiveMode() {
        if (!isConnected()) {
            std::cerr << "Not connected to server" << std::endl;
            return;
        }
        
        std::cout << "\nEntering interactive mode..." << std::endl;
        std::cout << "Type 'help' for available commands, 'quit' to exit" << std::endl;
        std::cout << "> ";
        
        std::string input;
        while (running_.load() && std::getline(std::cin, input)) {
            if (input == "quit" || input == "exit") {
                break;
            }
            
            if (input == "help") {
                printHelp();
            } else if (input == "status") {
                printConnectionStatus();
            } else if (input == "devices") {
                listDevices();
            } else if (!input.empty()) {
                sendCommand(input);
            }
            
            std::cout << "> ";
        }
    }

private:
    void handleConnectionStateChange(ConnectionState state, const std::string& error) {
        switch (state) {
            case ConnectionState::CONNECTING:
                std::cout << "Connecting to server..." << std::endl;
                break;
            case ConnectionState::CONNECTED:
                std::cout << "✓ Connected to server" << std::endl;
                break;
            case ConnectionState::DISCONNECTED:
                std::cout << "Disconnected from server" << std::endl;
                running_.store(false);
                break;
            case ConnectionState::RECONNECTING:
                std::cout << "Reconnecting to server..." << std::endl;
                break;
            case ConnectionState::ERROR:
                std::cerr << "Connection error: " << error << std::endl;
                break;
            default:
                break;
        }
    }
    
    void handleIncomingMessage(const std::string& message) {
        std::cout << Color::GREEN << "Received: " << message << Color::RESET << std::endl;
    }
    
    void handleConnectionError(const std::string& error, int code) {
        std::cerr << Color::RED << "Connection error: " << error << " (Code: " << code << ")" << Color::RESET << std::endl;
    }
    
    void sendCommand(const std::string& command) {
        if (!isConnected()) {
            std::cerr << Color::RED << "Not connected to server" << Color::RESET << std::endl;
            return;
        }
        
        if (connectionManager_->sendMessage(command)) {
            std::cout << Color::BLUE << "Sent: " << command << Color::RESET << std::endl;
        } else {
            std::cerr << Color::RED << "Failed to send command" << Color::RESET << std::endl;
        }
    }
    
    void printHelp() {
        std::cout << Color::BOLD << Color::CYAN << "\nAvailable commands:" << Color::RESET << std::endl;
        std::cout << "==================" << std::endl;
        std::cout << Color::YELLOW << "help" << Color::RESET << "     - Show this help message" << std::endl;
        std::cout << Color::YELLOW << "status" << Color::RESET << "   - Show connection status" << std::endl;
        std::cout << Color::YELLOW << "devices" << Color::RESET << "  - List connected devices" << std::endl;
        std::cout << Color::YELLOW << "quit" << Color::RESET << "     - Exit the client" << std::endl;
        std::cout << "\n" << Color::BOLD << "Device commands:" << Color::RESET << std::endl;
        std::cout << Color::MAGENTA << "TELESCOPE_SLEW <ra> <dec>" << Color::RESET << "  - Slew telescope to coordinates" << std::endl;
        std::cout << Color::MAGENTA << "TELESCOPE_PARK" << Color::RESET << "             - Park telescope" << std::endl;
        std::cout << Color::MAGENTA << "TELESCOPE_UNPARK" << Color::RESET << "           - Unpark telescope" << std::endl;
        std::cout << Color::MAGENTA << "CAMERA_EXPOSE <duration>" << Color::RESET << "   - Start camera exposure" << std::endl;
        std::cout << Color::MAGENTA << "FOCUSER_MOVE <position>" << Color::RESET << "    - Move focuser to position" << std::endl;
        std::cout << std::endl;
    }
    
    void printConnectionStatus() {
        if (!connectionManager_) {
            std::cout << Color::RED << "Connection manager not initialized" << Color::RESET << std::endl;
            return;
        }
        
        auto stats = connectionManager_->getStatistics();
        
        std::cout << Color::BOLD << Color::CYAN << "\nConnection Status:" << Color::RESET << std::endl;
        std::cout << "==================" << std::endl;
        std::cout << "Server: " << Color::WHITE << serverHost_ << ":" << serverPort_ << Color::RESET << std::endl;
        std::cout << "Connected: " << (isConnected() ? Color::GREEN + "Yes" : Color::RED + "No") << Color::RESET << std::endl;
        std::cout << "Healthy: " << (connectionManager_->isHealthy() ? Color::GREEN + "Yes" : Color::RED + "No") << Color::RESET << std::endl;
        std::cout << "Latency: " << Color::YELLOW << connectionManager_->getLatency().count() << "ms" << Color::RESET << std::endl;
        std::cout << "Messages sent: " << Color::BLUE << stats.messagesSent.load() << Color::RESET << std::endl;
        std::cout << "Messages received: " << Color::BLUE << stats.messagesReceived.load() << Color::RESET << std::endl;
        std::cout << "Errors: " << Color::RED << stats.errorCount.load() << Color::RESET << std::endl;
        std::cout << std::endl;
    }
    
    void listDevices() {
        std::cout << Color::CYAN << "\nRequesting device list..." << Color::RESET << std::endl;
        sendCommand("LIST_DEVICES");
    }
    
    // Configuration and state
    std::string serverHost_;
    uint16_t serverPort_;
    std::atomic<bool> running_;
    ConnectionConfig connectionConfig_;
    
    // Connection management
    std::unique_ptr<UnifiedConnectionManager> connectionManager_;
};

// Global client instance
std::unique_ptr<EnhancedHydrogenClient> g_client;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down client gracefully..." << std::endl;
    
    if (g_client) {
        g_client->disconnect();
    }
    
    exit(0);
}

void printBanner() {
    std::cout << Color::BOLD << Color::CYAN;
    std::cout << "\n";
    std::cout << "  +======================================================+\n";
    std::cout << "  |                                                      |\n";
    std::cout << "  |       Enhanced Hydrogen Device Control Client       |\n";
    std::cout << "  |                                                      |\n";
    std::cout << "  +======================================================+\n\n";
    std::cout << Color::RESET;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string serverHost = "localhost";
    uint16_t serverPort = 8000;
    
    if (argc >= 2) {
        serverHost = argv[1];
    }
    if (argc >= 3) {
        serverPort = static_cast<uint16_t>(std::stoi(argv[2]));
    }
    
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Configure logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    
    // Print banner
    printBanner();
    
    std::cout << "Starting Enhanced Hydrogen Device Client..." << std::endl;
    std::cout << "Server: " << serverHost << ":" << serverPort << std::endl;
    std::cout << std::endl;
    
    try {
        // Create and connect client
        g_client = std::make_unique<EnhancedHydrogenClient>(serverHost, serverPort);
        
        if (!g_client->connect()) {
            std::cerr << "Failed to connect to server" << std::endl;
            return 1;
        }
        
        std::cout << "✓ Client connected successfully" << std::endl;
        std::cout << "✓ Health monitoring: Active" << std::endl;
        std::cout << "✓ Auto-reconnection: Enabled" << std::endl;
        std::cout << std::endl;
        
        // Run interactive mode
        g_client->runInteractiveMode();
        
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

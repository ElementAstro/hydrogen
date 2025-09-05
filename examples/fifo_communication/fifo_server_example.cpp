#include <hydrogen/server/protocols/fifo/fifo_server.h>
#include <hydrogen/core/fifo_config_manager.h>
#include <hydrogen/core/fifo_logger.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <atomic>
#include <signal.h>

using namespace hydrogen::server::protocols::fifo;
using namespace hydrogen::core;

class FifoServerExample {
public:
    FifoServerExample() : running_(true) {
        setupSignalHandlers();
        initializeLogger();
    }
    
    ~FifoServerExample() {
        stop();
    }
    
    bool start(const std::string& configFile = "") {
        std::cout << "Starting FIFO server example..." << std::endl;
        
        // Load configuration
        FifoServerConfig config;
        if (!configFile.empty()) {
            try {
                std::ifstream file(configFile);
                if (file.is_open()) {
                    json configJson;
                    file >> configJson;
                    config.fromJson(configJson);
                    std::cout << "Loaded configuration from: " << configFile << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to load config: " << e.what() << std::endl;
                config = FifoServerFactory::createDefaultConfig();
            }
        } else {
            // Use default configuration
            config = FifoServerFactory::createDefaultConfig();
        }
        
        // Create FIFO server
        server_ = FifoServerFactory::createWithConfig(config);
        if (!server_) {
            std::cerr << "Failed to create FIFO server" << std::endl;
            return false;
        }
        
        // Set up event handlers
        setupEventHandlers();
        
        // Start the server
        if (!server_->start()) {
            std::cerr << "Failed to start FIFO server" << std::endl;
            return false;
        }
        
        std::cout << "FIFO server started successfully" << std::endl;
        std::cout << "Server ID: " << config.serverId << std::endl;
        std::cout << "Max clients: " << config.maxConcurrentClients << std::endl;
        
        // Start management thread
        startManagementThread();
        
        return true;
    }
    
    void stop() {
        running_.store(false);
        
        if (managementThread_ && managementThread_->joinable()) {
            managementThread_->join();
            managementThread_.reset();
        }
        
        if (server_) {
            server_->stop();
            server_.reset();
        }
        
        std::cout << "FIFO server stopped" << std::endl;
    }
    
    void waitForShutdown() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
private:
    std::unique_ptr<FifoServer> server_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> managementThread_;
    
    void setupSignalHandlers() {
        signal(SIGINT, [](int) {
            std::cout << "\nReceived interrupt signal, shutting down..." << std::endl;
            // Note: In a real application, you'd want a more robust signal handling mechanism
        });
    }
    
    void initializeLogger() {
        FifoLoggerConfig logConfig;
        logConfig.enableConsoleLogging = true;
        logConfig.enableFileLogging = true;
        logConfig.logFilePath = "fifo_server.log";
        logConfig.enableMessageTracing = true;
        logConfig.enablePerformanceMetrics = true;
        logConfig.logLevel = FifoLogLevel::INFO;
        
        getGlobalFifoLogger().updateConfig(logConfig);
        
        FIFO_LOG_INFO("SERVER", "FIFO server logger initialized", "server");
    }
    
    void setupEventHandlers() {
        // Client connected callback
        server_->setClientConnectedCallback([this](const std::string& clientId) {
            onClientConnected(clientId);
        });
        
        // Client disconnected callback
        server_->setClientDisconnectedCallback([this](const std::string& clientId) {
            onClientDisconnected(clientId);
        });
        
        // Message received callback
        server_->setMessageReceivedCallback([this](const std::string& clientId, const Message& message) {
            onMessageReceived(clientId, message);
        });
        
        // Error callback
        server_->setErrorCallback([this](const std::string& error, const std::string& clientId) {
            onError(error, clientId);
        });
    }
    
    void onClientConnected(const std::string& clientId) {
        FIFO_LOG_INFO("SERVER", "Client connected: " + clientId, clientId);
        std::cout << "Client connected: " << clientId << std::endl;
        
        // Send welcome message
        Message welcomeMsg;
        welcomeMsg.senderId = "server";
        welcomeMsg.recipientId = clientId;
        welcomeMsg.topic = "welcome";
        welcomeMsg.sourceProtocol = CommunicationProtocol::FIFO;
        welcomeMsg.targetProtocol = CommunicationProtocol::FIFO;
        welcomeMsg.timestamp = getCurrentTimestamp();
        
        json welcomeData;
        welcomeData["type"] = "notification";
        welcomeData["message"] = "Welcome to Hydrogen FIFO Server!";
        welcomeData["server"] = "HydrogenFifoServer";
        welcomeData["version"] = "1.0.0";
        welcomeData["timestamp"] = getCurrentTimestamp();
        
        welcomeMsg.payload = welcomeData.dump();
        
        server_->sendMessageToClient(clientId, welcomeMsg);
    }
    
    void onClientDisconnected(const std::string& clientId) {
        FIFO_LOG_INFO("SERVER", "Client disconnected: " + clientId, clientId);
        std::cout << "Client disconnected: " << clientId << std::endl;
    }
    
    void onMessageReceived(const std::string& clientId, const Message& message) {
        FIFO_LOG_DEBUG("SERVER", "Message received from client: " + clientId, clientId);
        
        try {
            // Parse message payload as JSON
            json messageJson;
            try {
                messageJson = json::parse(message.payload);
            } catch (const std::exception&) {
                // If payload is not JSON, treat it as a simple command
                messageJson["command"] = message.topic;
                messageJson["payload"] = message.payload;
            }
            
            if (messageJson.contains("command")) {
                std::string command = messageJson["command"];
                std::string payload = messageJson.contains("data") ? messageJson["data"] : "";
                
                processCommand(clientId, command, payload, message.senderId + "_" + message.timestamp);
            } else {
                sendErrorResponse(clientId, "Invalid message format", message.senderId + "_" + message.timestamp);
            }
            
        } catch (const std::exception& e) {
            FIFO_LOG_ERROR("SERVER", "Error processing message: " + std::string(e.what()), clientId);
            sendErrorResponse(clientId, "Message processing error", message.senderId + "_" + message.timestamp);
        }
    }
    
    void onError(const std::string& error, const std::string& clientId) {
        FIFO_LOG_ERROR("SERVER", "Server error: " + error, clientId);
        std::cerr << "Server error";
        if (!clientId.empty()) {
            std::cerr << " (client " << clientId << ")";
        }
        std::cerr << ": " << error << std::endl;
    }
    
    void processCommand(const std::string& clientId, const std::string& command, 
                       const std::string& payload, const std::string& originalMessageId) {
        FIFO_LOG_DEBUG("SERVER", "Processing command: " + command + " from client: " + clientId, clientId);
        
        if (command == "ping") {
            sendResponse(clientId, "pong", originalMessageId);
        } else if (command == "echo") {
            std::string echoData = payload.empty() ? "echo" : payload;
            sendResponse(clientId, echoData, originalMessageId);
        } else if (command == "status") {
            sendStatusResponse(clientId, originalMessageId);
        } else if (command == "help") {
            sendHelpResponse(clientId, originalMessageId);
        } else if (command == "stats") {
            sendStatsResponse(clientId, originalMessageId);
        } else if (command == "clients") {
            sendClientsResponse(clientId, originalMessageId);
        } else {
            sendErrorResponse(clientId, "Unknown command: " + command, originalMessageId);
        }
    }
    
    void sendResponse(const std::string& clientId, const std::string& responseType, 
                     const json& data, const std::string& originalMessageId) {
        Message response;
        response.senderId = "server";
        response.recipientId = clientId;
        response.topic = responseType;
        response.sourceProtocol = CommunicationProtocol::FIFO;
        response.targetProtocol = CommunicationProtocol::FIFO;
        response.timestamp = getCurrentTimestamp();
        
        json responseData;
        responseData["type"] = "response";
        responseData["command"] = responseType;
        responseData["data"] = data;
        responseData["originalMessageId"] = originalMessageId;
        responseData["timestamp"] = getCurrentTimestamp();
        
        response.payload = responseData.dump();
        
        if (server_->sendMessageToClient(clientId, response)) {
            FIFO_LOG_DEBUG("SERVER", "Sent response: " + responseType + " to client: " + clientId, clientId);
        } else {
            FIFO_LOG_ERROR("SERVER", "Failed to send response to client: " + clientId, clientId);
        }
    }
    
    void sendResponse(const std::string& clientId, const std::string& data, const std::string& originalMessageId) {
        sendResponse(clientId, "response", data, originalMessageId);
    }
    
    void sendErrorResponse(const std::string& clientId, const std::string& error, 
                          const std::string& originalMessageId) {
        Message errorMsg;
        errorMsg.senderId = "server";
        errorMsg.recipientId = clientId;
        errorMsg.topic = "error";
        errorMsg.sourceProtocol = CommunicationProtocol::FIFO;
        errorMsg.targetProtocol = CommunicationProtocol::FIFO;
        errorMsg.timestamp = getCurrentTimestamp();
        
        json errorData;
        errorData["type"] = "error";
        errorData["message"] = error;
        errorData["originalMessageId"] = originalMessageId;
        errorData["timestamp"] = getCurrentTimestamp();
        
        errorMsg.payload = errorData.dump();
        
        server_->sendMessageToClient(clientId, errorMsg);
        FIFO_LOG_WARN("SERVER", "Sent error response: " + error + " to client: " + clientId, clientId);
    }
    
    void sendStatusResponse(const std::string& clientId, const std::string& originalMessageId) {
        json statusData;
        statusData["server"] = "HydrogenFifoServer";
        statusData["version"] = "1.0.0";
        statusData["status"] = static_cast<int>(server_->getStatus());
        statusData["isRunning"] = server_->isRunning();
        statusData["isHealthy"] = server_->isHealthy();
        statusData["connectedClients"] = server_->getConnectedClients().size();
        statusData["statistics"] = server_->getStatistics().toJson();
        
        sendResponse(clientId, "status", statusData, originalMessageId);
    }
    
    void sendHelpResponse(const std::string& clientId, const std::string& originalMessageId) {
        json helpData;
        helpData["commands"] = {
            {"ping", "Test server connectivity"},
            {"echo", "Echo back the provided message"},
            {"status", "Get server status and statistics"},
            {"help", "Show available commands"},
            {"stats", "Get detailed server statistics"},
            {"clients", "List connected clients"}
        };
        helpData["usage"] = "Send JSON messages with 'command' field";
        helpData["example"] = R"({"command": "ping"})";
        
        sendResponse(clientId, "help", helpData, originalMessageId);
    }
    
    void sendStatsResponse(const std::string& clientId, const std::string& originalMessageId) {
        json statsData = server_->getStatistics().toJson();
        statsData["serverInfo"] = server_->getServerInfo();
        
        sendResponse(clientId, "stats", statsData, originalMessageId);
    }
    
    void sendClientsResponse(const std::string& clientId, const std::string& originalMessageId) {
        auto clients = server_->getConnectedClients();
        json clientsData;
        clientsData["connectedClients"] = clients;
        clientsData["totalClients"] = clients.size();
        clientsData["maxClients"] = server_->getServerConfig().maxConcurrentClients;
        
        sendResponse(clientId, "clients", clientsData, originalMessageId);
    }
    
    void startManagementThread() {
        managementThread_ = std::make_unique<std::thread>([this]() {
            managementThreadFunction();
        });
    }
    
    void managementThreadFunction() {
        FIFO_LOG_INFO("SERVER", "Management thread started", "server");
        
        auto lastStatsTime = std::chrono::steady_clock::now();
        const auto statsInterval = std::chrono::seconds(30);
        
        while (running_.load()) {
            auto now = std::chrono::steady_clock::now();
            
            // Print periodic statistics
            if (now - lastStatsTime >= statsInterval) {
                printServerStats();
                lastStatsTime = now;
            }
            
            // Check server health
            if (!server_->isHealthy()) {
                FIFO_LOG_WARN("SERVER", "Server health check failed", "server");
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        FIFO_LOG_INFO("SERVER", "Management thread stopped", "server");
    }
    
    void printServerStats() {
        auto stats = server_->getStatistics();
        auto clients = server_->getConnectedClients();
        
        std::cout << "\n=== Server Statistics ===" << std::endl;
        std::cout << "Status: " << (server_->isRunning() ? "Running" : "Stopped") << std::endl;
        std::cout << "Health: " << server_->getHealthStatus() << std::endl;
        std::cout << "Connected clients: " << clients.size() << std::endl;
        std::cout << "Total clients connected: " << stats.totalClientsConnected.load() << std::endl;
        std::cout << "Messages processed: " << stats.totalMessagesProcessed.load() << std::endl;
        std::cout << "Bytes transferred: " << stats.totalBytesTransferred.load() << std::endl;
        std::cout << "Errors: " << stats.totalErrors.load() << std::endl;
        std::cout << "Uptime: " << stats.getUptime().count() << " ms" << std::endl;
        
        if (!clients.empty()) {
            std::cout << "Active clients: ";
            for (const auto& client : clients) {
                std::cout << client << " ";
            }
            std::cout << std::endl;
        }
        std::cout << "=========================" << std::endl;
    }
    
    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

int main(int argc, char* argv[]) {
    std::cout << "Hydrogen FIFO Server Example" << std::endl;
    std::cout << "=============================" << std::endl;
    
    std::string configFile;
    if (argc > 1) {
        configFile = argv[1];
        std::cout << "Using configuration file: " << configFile << std::endl;
    } else {
        std::cout << "Using default configuration" << std::endl;
        std::cout << "Usage: " << argv[0] << " [config_file.json]" << std::endl;
    }
    
    try {
        FifoServerExample server;
        
        if (!server.start(configFile)) {
            std::cerr << "Failed to start FIFO server" << std::endl;
            return 1;
        }
        
        std::cout << "\nServer is running. Press Ctrl+C to stop." << std::endl;
        
        // Wait for shutdown signal
        server.waitForShutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "FIFO server example completed" << std::endl;
    return 0;
}

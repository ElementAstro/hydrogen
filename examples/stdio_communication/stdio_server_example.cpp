#include <hydrogen/server/protocols/stdio/stdio_server.h>
#include <hydrogen/core/stdio_config_manager.h>
#include <hydrogen/core/stdio_logger.h>
#include <hydrogen/core/message.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <string>
#include <sstream>

using namespace hydrogen::server::protocols::stdio;
using namespace hydrogen::core;

/**
 * @brief Example stdio server implementation
 * 
 * This example demonstrates how to create a stdio-based server that
 * can accept connections from stdio-based clients and process commands.
 */
class StdioServerExample {
public:
    StdioServerExample() {
        // Configure logging
        StdioLogger::LoggerConfig logConfig;
        logConfig.enableDebugMode = true;
        logConfig.enableMessageTracing = true;
        logConfig.enableConsoleLogging = true;
        logConfig.enableFileLogging = true;
        logConfig.logFileName = "stdio_server_example.log";
        logConfig.logLevel = StdioLogLevel::INFO;
        
        auto& logger = getGlobalStdioLogger();
        logger.updateConfig(logConfig);
        
        // Create server configuration
        serverConfig_ = StdioServerFactory::createDefaultConfig();
        serverConfig_.serverName = "StdioServerExample";
        serverConfig_.maxConcurrentClients = 10;
        serverConfig_.enableCommandFiltering = true;
        serverConfig_.allowedCommands = {"ping", "status", "help", "echo", "custom"};
        
        // Configure protocol settings
        serverConfig_.protocolConfig.enableMessageValidation = true;
        serverConfig_.protocolConfig.enableMessageLogging = true;
        serverConfig_.protocolConfig.framingMode = StdioConfig::FramingMode::JSON_LINES;
        
        STDIO_LOG_INFO("StdioServerExample initialized", "server");
    }
    
    bool start() {
        try {
            // Create server
            server_ = StdioServerFactory::createWithConfig(serverConfig_);
            
            // Set up callbacks
            server_->setClientConnectedCallback([this](const std::string& clientId) {
                handleClientConnected(clientId);
            });
            
            server_->setClientDisconnectedCallback([this](const std::string& clientId) {
                handleClientDisconnected(clientId);
            });
            
            server_->setMessageReceivedCallback([this](const std::string& clientId, const Message& message) {
                handleMessageReceived(clientId, message);
            });
            
            server_->setErrorCallback([this](const std::string& error, const std::string& clientId) {
                handleError(error, clientId);
            });
            
            // Start server
            if (!server_->start()) {
                STDIO_LOG_ERROR("Failed to start stdio server", "server");
                return false;
            }
            
            STDIO_LOG_INFO("Stdio server started successfully", "server");
            return true;
            
        } catch (const std::exception& e) {
            STDIO_LOG_ERROR("Server startup failed: " + std::string(e.what()), "server");
            return false;
        }
    }
    
    void stop() {
        if (server_) {
            server_->stop();
            STDIO_LOG_INFO("Stdio server stopped", "server");
        }
    }
    
    void run() {
        std::cout << "Stdio Server Example Running\n";
        std::cout << "Press Ctrl+C to stop the server\n\n";
        
        // Accept a default client connection (for demonstration)
        server_->acceptClient("stdio_client_default", "interactive");
        
        // Keep server running
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Periodic status update
            static auto lastStatusTime = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            
            if (now - lastStatusTime > std::chrono::seconds(30)) {
                showStatus();
                lastStatusTime = now;
            }
        }
    }
    
    void shutdown() {
        running_ = false;
        stop();
    }
    
private:
    StdioServer::ServerConfig serverConfig_;
    std::unique_ptr<StdioServer> server_;
    std::atomic<bool> running_{true};
    
    void handleClientConnected(const std::string& clientId) {
        STDIO_LOG_INFO("Client connected: " + clientId, clientId);
        std::cout << "Client connected: " << clientId << std::endl;
        
        // Send welcome message
        Message welcomeMsg;
        welcomeMsg.senderId = "server";
        welcomeMsg.recipientId = clientId;
        welcomeMsg.topic = "welcome";
        welcomeMsg.sourceProtocol = CommunicationProtocol::STDIO;
        welcomeMsg.targetProtocol = CommunicationProtocol::STDIO;
        welcomeMsg.timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        json welcomePayload;
        welcomePayload["type"] = "event";
        welcomePayload["event"] = "welcome";
        welcomePayload["message"] = "Welcome to Stdio Server Example";
        welcomePayload["server"] = serverConfig_.serverName;
        welcomePayload["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        server_->sendMessageToClient(clientId, welcomeMsg);
    }
    
    void handleClientDisconnected(const std::string& clientId) {
        STDIO_LOG_INFO("Client disconnected: " + clientId, clientId);
        std::cout << "Client disconnected: " << clientId << std::endl;
    }
    
    void handleMessageReceived(const std::string& clientId, const Message& message) {
        try {
            STDIO_LOG_DEBUG("Message received from client: " + clientId, clientId);

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
                std::string payload = messageJson.contains("payload") ? messageJson["payload"] : "";

                processCommand(clientId, command, payload, message.senderId + "_" + message.timestamp);
            } else {
                sendErrorResponse(clientId, "Invalid message format", message.senderId + "_" + message.timestamp);
            }

        } catch (const std::exception& e) {
            STDIO_LOG_ERROR("Error processing message: " + std::string(e.what()), clientId);
            sendErrorResponse(clientId, "Message processing error", message.senderId + "_" + message.timestamp);
        }
    }
    
    void processCommand(const std::string& clientId, const std::string& command, 
                       const std::string& payload, const std::string& messageId) {
        
        STDIO_LOG_INFO("Processing command: " + command + " from client: " + clientId, clientId);
        
        if (command == "ping") {
            sendResponse(clientId, "pong", "Server is alive", messageId);
            
        } else if (command == "status") {
            json statusData;
            statusData["server"] = serverConfig_.serverName;
            statusData["uptime"] = server_->getStatistics().uptime.count();
            statusData["connectedClients"] = server_->getConnectedClients().size();
            statusData["totalMessages"] = server_->getStatistics().totalMessagesProcessed;
            
            sendResponse(clientId, "status", statusData, messageId);
            
        } else if (command == "help") {
            json helpData;
            helpData["commands"] = serverConfig_.allowedCommands;
            helpData["description"] = "Available commands for this server";
            
            sendResponse(clientId, "help", helpData, messageId);
            
        } else if (command == "echo") {
            sendResponse(clientId, "echo", payload, messageId);
            
        } else if (command == "custom") {
            json customResponse;
            customResponse["received"] = payload;
            customResponse["processed_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            sendResponse(clientId, "custom_response", customResponse, messageId);
            
        } else {
            sendErrorResponse(clientId, "Unknown command: " + command, messageId);
        }
    }
    
    void sendResponse(const std::string& clientId, const std::string& responseType,
                     const json& data, const std::string& originalMessageId) {
        Message response;
        response.senderId = "server";
        response.recipientId = clientId;
        response.topic = responseType;
        response.sourceProtocol = CommunicationProtocol::STDIO;
        response.targetProtocol = CommunicationProtocol::STDIO;
        response.timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        json responsePayload;
        responsePayload["type"] = "response";
        responsePayload["response_type"] = responseType;
        responsePayload["data"] = data;
        responsePayload["original_message_id"] = originalMessageId;
        responsePayload["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        server_->sendMessageToClient(clientId, response);
    }
    
    void sendErrorResponse(const std::string& clientId, const std::string& error,
                          const std::string& originalMessageId) {
        Message errorMsg;
        errorMsg.senderId = "server";
        errorMsg.recipientId = clientId;
        errorMsg.topic = "error";
        errorMsg.sourceProtocol = CommunicationProtocol::STDIO;
        errorMsg.targetProtocol = CommunicationProtocol::STDIO;
        errorMsg.timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        json errorPayload;
        errorPayload["type"] = "error";
        errorPayload["error"] = error;
        errorPayload["original_message_id"] = originalMessageId;
        errorPayload["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        server_->sendMessageToClient(clientId, errorMsg);
    }
    
    void handleError(const std::string& error, const std::string& clientId) {
        STDIO_LOG_ERROR("Server error: " + error, clientId);
        std::cerr << "Server Error [" << clientId << "]: " << error << std::endl;
    }
    
    void showStatus() {
        if (!server_) return;
        
        auto stats = server_->getStatistics();
        std::cout << "\n=== Server Status ===" << std::endl;
        std::cout << "Server: " << serverConfig_.serverName << std::endl;
        std::cout << "Status: " << (server_->isHealthy() ? "HEALTHY" : "UNHEALTHY") << std::endl;
        std::cout << "Connected Clients: " << stats.currentActiveClients << std::endl;
        std::cout << "Total Messages: " << stats.totalMessagesProcessed << std::endl;
        std::cout << "Uptime: " << stats.uptime.count() << "ms" << std::endl;
        std::cout << "===================" << std::endl;
    }
    
    std::string generateMessageId() {
        static int counter = 0;
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return "srv_msg_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
    }
};

// Global server instance for signal handling
StdioServerExample* g_server = nullptr;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_server) {
        g_server->shutdown();
    }
}

int main(int argc, char* argv[]) {
    try {
        // Set up signal handling
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        StdioServerExample server;
        g_server = &server;
        
        if (!server.start()) {
            std::cerr << "Failed to start server" << std::endl;
            return 1;
        }
        
        server.run();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

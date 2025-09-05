#include <hydrogen/core/device_communicator.h>
#include <hydrogen/core/stdio_config_manager.h>
#include <hydrogen/core/stdio_logger.h>
#include <hydrogen/core/message.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>

using namespace hydrogen::core;

/**
 * @brief Example stdio client implementation
 * 
 * This example demonstrates how to create a stdio-based client that
 * communicates with a Hydrogen server using standard input/output.
 */
class StdioClientExample {
public:
    StdioClientExample() {
        // Configure logging for debugging
        StdioLogger::LoggerConfig logConfig;
        logConfig.enableDebugMode = true;
        logConfig.enableMessageTracing = true;
        logConfig.enableConsoleLogging = true;
        logConfig.logLevel = StdioLogLevel::DEBUG;
        
        auto& logger = getGlobalStdioLogger();
        logger.updateConfig(logConfig);
        
        // Create stdio configuration
        auto& configManager = getGlobalStdioConfigManager();
        config_ = configManager.createConfig(StdioConfigManager::ConfigPreset::DEFAULT);
        
        // Enable message validation and logging
        config_.enableMessageValidation = true;
        config_.enableMessageLogging = true;
        config_.framingMode = StdioConfig::FramingMode::JSON_LINES;
        
        STDIO_LOG_INFO("StdioClientExample initialized", "client");
    }
    
    bool connect() {
        try {
            // Create stdio communicator
            communicator_ = createStdioCommunicator(config_);
            
            // Set up message handler
            communicator_->setMessageHandler([this](const std::string& message, CommunicationProtocol protocol) {
                handleIncomingMessage(message, protocol);
            });
            
            // Set up error handler
            communicator_->setErrorHandler([this](const std::string& error) {
                handleError(error);
            });
            
            // Start the communicator
            if (!communicator_->start()) {
                STDIO_LOG_ERROR("Failed to start stdio communicator", "client");
                return false;
            }
            
            STDIO_LOG_INFO("Connected to stdio server", "client");
            return true;
            
        } catch (const std::exception& e) {
            STDIO_LOG_ERROR("Connection failed: " + std::string(e.what()), "client");
            return false;
        }
    }
    
    void disconnect() {
        if (communicator_) {
            communicator_->stop();
            STDIO_LOG_INFO("Disconnected from stdio server", "client");
        }
    }
    
    bool sendCommand(const std::string& command, const std::string& payload = "") {
        if (!communicator_ || !communicator_->isActive()) {
            STDIO_LOG_ERROR("Communicator not active", "client");
            return false;
        }
        
        try {
            // Create message
            Message message;
            message.setMessageType(MessageType::COMMAND);
            message.setDeviceId("stdio_client");
            message.setMessageId(generateMessageId());
            
            // Create command payload
            json commandJson;
            commandJson["command"] = command;
            commandJson["payload"] = payload;
            commandJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // Send message
            bool success = communicator_->sendMessage(commandJson);
            
            if (success) {
                STDIO_LOG_INFO("Command sent: " + command, "client");
            } else {
                STDIO_LOG_ERROR("Failed to send command: " + command, "client");
            }
            
            return success;
            
        } catch (const std::exception& e) {
            STDIO_LOG_ERROR("Error sending command: " + std::string(e.what()), "client");
            return false;
        }
    }
    
    void runInteractiveMode() {
        std::cout << "Stdio Client Interactive Mode\n";
        std::cout << "Commands: help, status, ping, quit\n";
        std::cout << "Type 'quit' to exit\n\n";
        
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "quit" || input == "exit") {
                break;
            }
            
            if (input == "help") {
                showHelp();
            } else if (input == "status") {
                showStatus();
            } else if (input == "ping") {
                sendCommand("ping");
            } else if (input.empty()) {
                continue;
            } else {
                // Send as custom command
                sendCommand("custom", input);
            }
        }
    }
    
private:
    StdioConfig config_;
    std::unique_ptr<StdioCommunicator> communicator_;
    
    void handleIncomingMessage(const std::string& message, CommunicationProtocol protocol) {
        try {
            json messageJson = json::parse(message);
            
            STDIO_LOG_DEBUG("Received message: " + message, "client");
            
            // Process different message types
            if (messageJson.contains("type")) {
                std::string type = messageJson["type"];
                
                if (type == "response") {
                    handleResponse(messageJson);
                } else if (type == "event") {
                    handleEvent(messageJson);
                } else if (type == "error") {
                    handleServerError(messageJson);
                }
            }
            
        } catch (const std::exception& e) {
            STDIO_LOG_ERROR("Error processing message: " + std::string(e.what()), "client");
        }
    }
    
    void handleResponse(const json& response) {
        std::cout << "Server Response: ";
        if (response.contains("payload")) {
            std::cout << response["payload"].dump(2) << std::endl;
        } else {
            std::cout << response.dump(2) << std::endl;
        }
    }
    
    void handleEvent(const json& event) {
        std::cout << "Server Event: ";
        if (event.contains("payload")) {
            std::cout << event["payload"].dump(2) << std::endl;
        } else {
            std::cout << event.dump(2) << std::endl;
        }
    }
    
    void handleServerError(const json& error) {
        std::cout << "Server Error: ";
        if (error.contains("payload")) {
            std::cout << error["payload"].dump(2) << std::endl;
        } else {
            std::cout << error.dump(2) << std::endl;
        }
    }
    
    void handleError(const std::string& error) {
        STDIO_LOG_ERROR("Communication error: " + error, "client");
        std::cerr << "Error: " << error << std::endl;
    }
    
    void showHelp() {
        std::cout << "Available commands:\n";
        std::cout << "  help   - Show this help message\n";
        std::cout << "  status - Show client status\n";
        std::cout << "  ping   - Send ping to server\n";
        std::cout << "  quit   - Exit the client\n";
        std::cout << "  <text> - Send custom command with text as payload\n";
    }
    
    void showStatus() {
        std::cout << "Client Status:\n";
        std::cout << "  Active: " << (communicator_ && communicator_->isActive() ? "Yes" : "No") << "\n";
        
        if (communicator_) {
            std::cout << "  Lines Sent: " << communicator_->getLinesSent() << "\n";
            std::cout << "  Lines Received: " << communicator_->getLinesReceived() << "\n";
        }
        
        auto metrics = getGlobalStdioLogger().getMetrics();
        std::cout << "  Total Messages: " << metrics.totalMessages.load() << "\n";
        std::cout << "  Success Rate: " << metrics.getSuccessRate() << "%\n";
    }
    
    std::string generateMessageId() {
        static int counter = 0;
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return "msg_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
    }
};

int main(int argc, char* argv[]) {
    try {
        StdioClientExample client;
        
        if (!client.connect()) {
            std::cerr << "Failed to connect to server" << std::endl;
            return 1;
        }
        
        // Run interactive mode
        client.runInteractiveMode();
        
        client.disconnect();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

#include <hydrogen/core/fifo_communicator.h>
#include <hydrogen/core/fifo_config_manager.h>
#include <hydrogen/core/fifo_logger.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <atomic>
#include <signal.h>

using namespace hydrogen::core;

class FifoClientExample {
public:
    FifoClientExample() : running_(true) {
        setupSignalHandlers();
        initializeLogger();
    }
    
    ~FifoClientExample() {
        stop();
    }
    
    bool start(const std::string& configFile = "") {
        std::cout << "Starting FIFO client example..." << std::endl;
        
        // Load configuration
        FifoConfig config;
        if (!configFile.empty()) {
            try {
                auto& configManager = getGlobalFifoConfigManager();
                config = configManager.loadConfig(configFile);
                std::cout << "Loaded configuration from: " << configFile << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to load config: " << e.what() << std::endl;
                config = getGlobalFifoConfigManager().createConfig();
            }
        } else {
            // Use default configuration
            config = getGlobalFifoConfigManager().createConfig();
        }
        
        // Create FIFO communicator
        communicator_ = FifoCommunicatorFactory::create(config);
        if (!communicator_) {
            std::cerr << "Failed to create FIFO communicator" << std::endl;
            return false;
        }
        
        // Set up event handlers
        setupEventHandlers();
        
        // Start the communicator
        if (!communicator_->start()) {
            std::cerr << "Failed to start FIFO communicator" << std::endl;
            return false;
        }
        
        std::cout << "FIFO client started successfully" << std::endl;
        std::cout << "Pipe path: " << (config.pipeType == FifoPipeType::WINDOWS_NAMED_PIPE ? 
                                       config.windowsPipePath : config.unixPipePath) << std::endl;
        
        // Start interactive session
        startInteractiveSession();
        
        return true;
    }
    
    void stop() {
        running_.store(false);
        
        if (communicator_) {
            communicator_->stop();
            communicator_.reset();
        }
        
        std::cout << "FIFO client stopped" << std::endl;
    }
    
private:
    std::unique_ptr<FifoCommunicator> communicator_;
    std::atomic<bool> running_;
    
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
        logConfig.logFilePath = "fifo_client.log";
        logConfig.enableMessageTracing = true;
        logConfig.logLevel = FifoLogLevel::INFO;
        
        getGlobalFifoLogger().updateConfig(logConfig);
        
        FIFO_LOG_INFO("CLIENT", "FIFO client logger initialized", "client");
    }
    
    void setupEventHandlers() {
        // Message handler
        communicator_->setMessageHandler([this](const std::string& message) {
            handleIncomingMessage(message);
        });
        
        // Error handler
        communicator_->setErrorHandler([this](const std::string& error) {
            handleError(error);
        });
        
        // Connection handler
        communicator_->setConnectionHandler([this](bool connected) {
            handleConnectionChange(connected);
        });
    }
    
    void handleIncomingMessage(const std::string& message) {
        FIFO_LOG_INFO("CLIENT", "Received message: " + message, "client");
        
        try {
            json messageJson = json::parse(message);
            
            std::cout << "\n=== Received Message ===" << std::endl;
            std::cout << "Raw: " << message << std::endl;
            std::cout << "Formatted: " << messageJson.dump(2) << std::endl;
            
            // Handle specific message types
            if (messageJson.contains("type")) {
                std::string type = messageJson["type"];
                
                if (type == "response") {
                    handleResponse(messageJson);
                } else if (type == "notification") {
                    handleNotification(messageJson);
                } else if (type == "error") {
                    handleServerError(messageJson);
                }
            }
            
        } catch (const std::exception& e) {
            std::cout << "\n=== Received Raw Message ===" << std::endl;
            std::cout << message << std::endl;
        }
        
        std::cout << "fifo_client> " << std::flush;
    }
    
    void handleError(const std::string& error) {
        FIFO_LOG_ERROR("CLIENT", "Error: " + error, "client");
        std::cerr << "FIFO Error: " << error << std::endl;
    }
    
    void handleConnectionChange(bool connected) {
        if (connected) {
            FIFO_LOG_INFO("CLIENT", "Connected to FIFO server", "client");
            std::cout << "Connected to FIFO server" << std::endl;
        } else {
            FIFO_LOG_WARN("CLIENT", "Disconnected from FIFO server", "client");
            std::cout << "Disconnected from FIFO server" << std::endl;
        }
    }
    
    void handleResponse(const json& response) {
        std::cout << "Server Response: ";
        if (response.contains("data")) {
            std::cout << response["data"] << std::endl;
        } else {
            std::cout << response.dump() << std::endl;
        }
    }
    
    void handleNotification(const json& notification) {
        std::cout << "Server Notification: ";
        if (notification.contains("message")) {
            std::cout << notification["message"] << std::endl;
        } else {
            std::cout << notification.dump() << std::endl;
        }
    }
    
    void handleServerError(const json& error) {
        std::cout << "Server Error: ";
        if (error.contains("message")) {
            std::cout << error["message"] << std::endl;
        } else {
            std::cout << error.dump() << std::endl;
        }
    }
    
    void startInteractiveSession() {
        std::cout << "\n=== FIFO Client Interactive Session ===" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  ping                 - Send ping to server" << std::endl;
        std::cout << "  echo <message>       - Echo message through server" << std::endl;
        std::cout << "  status               - Get server status" << std::endl;
        std::cout << "  stats                - Get client statistics" << std::endl;
        std::cout << "  send <json>          - Send custom JSON message" << std::endl;
        std::cout << "  help                 - Show this help" << std::endl;
        std::cout << "  quit                 - Exit client" << std::endl;
        std::cout << "=========================" << std::endl;
        
        std::string input;
        while (running_.load()) {
            std::cout << "fifo_client> ";
            std::getline(std::cin, input);
            
            if (input.empty()) {
                continue;
            }
            
            if (!processCommand(input)) {
                break;
            }
        }
    }
    
    bool processCommand(const std::string& input) {
        std::istringstream iss(input);
        std::string command;
        iss >> command;
        
        if (command == "quit" || command == "exit") {
            return false;
        } else if (command == "ping") {
            sendPing();
        } else if (command == "echo") {
            std::string message;
            std::getline(iss, message);
            if (!message.empty() && message[0] == ' ') {
                message = message.substr(1); // Remove leading space
            }
            sendEcho(message);
        } else if (command == "status") {
            sendStatusRequest();
        } else if (command == "stats") {
            showClientStats();
        } else if (command == "send") {
            std::string jsonStr;
            std::getline(iss, jsonStr);
            if (!jsonStr.empty() && jsonStr[0] == ' ') {
                jsonStr = jsonStr.substr(1); // Remove leading space
            }
            sendCustomMessage(jsonStr);
        } else if (command == "help") {
            showHelp();
        } else {
            std::cout << "Unknown command: " << command << ". Type 'help' for available commands." << std::endl;
        }
        
        return true;
    }
    
    void sendPing() {
        json message;
        message["type"] = "command";
        message["command"] = "ping";
        message["id"] = generateMessageId();
        message["timestamp"] = getCurrentTimestamp();
        
        if (communicator_->sendMessage(message)) {
            FIFO_LOG_INFO("CLIENT", "Sent ping command", "client");
            std::cout << "Ping sent" << std::endl;
        } else {
            std::cout << "Failed to send ping" << std::endl;
        }
    }
    
    void sendEcho(const std::string& message) {
        json msg;
        msg["type"] = "command";
        msg["command"] = "echo";
        msg["data"] = message.empty() ? "Hello from FIFO client!" : message;
        msg["id"] = generateMessageId();
        msg["timestamp"] = getCurrentTimestamp();
        
        if (communicator_->sendMessage(msg)) {
            FIFO_LOG_INFO("CLIENT", "Sent echo command: " + msg["data"].get<std::string>(), "client");
            std::cout << "Echo sent: " << msg["data"] << std::endl;
        } else {
            std::cout << "Failed to send echo" << std::endl;
        }
    }
    
    void sendStatusRequest() {
        json message;
        message["type"] = "command";
        message["command"] = "status";
        message["id"] = generateMessageId();
        message["timestamp"] = getCurrentTimestamp();
        
        if (communicator_->sendMessage(message)) {
            FIFO_LOG_INFO("CLIENT", "Sent status request", "client");
            std::cout << "Status request sent" << std::endl;
        } else {
            std::cout << "Failed to send status request" << std::endl;
        }
    }
    
    void showClientStats() {
        if (!communicator_) {
            std::cout << "No communicator available" << std::endl;
            return;
        }
        
        auto stats = communicator_->getStatistics();
        std::cout << "\n=== Client Statistics ===" << std::endl;
        std::cout << stats.toJson().dump(2) << std::endl;
        
        std::cout << "\nHealth Status: " << communicator_->getHealthStatus() << std::endl;
        std::cout << "Connection State: " << static_cast<int>(communicator_->getConnectionState()) << std::endl;
        std::cout << "Active: " << (communicator_->isActive() ? "Yes" : "No") << std::endl;
        std::cout << "Connected: " << (communicator_->isConnected() ? "Yes" : "No") << std::endl;
    }
    
    void sendCustomMessage(const std::string& jsonStr) {
        if (jsonStr.empty()) {
            std::cout << "Please provide a JSON message" << std::endl;
            return;
        }
        
        try {
            json message = json::parse(jsonStr);
            
            // Add standard fields if not present
            if (!message.contains("id")) {
                message["id"] = generateMessageId();
            }
            if (!message.contains("timestamp")) {
                message["timestamp"] = getCurrentTimestamp();
            }
            
            if (communicator_->sendMessage(message)) {
                FIFO_LOG_INFO("CLIENT", "Sent custom message", "client");
                std::cout << "Custom message sent" << std::endl;
            } else {
                std::cout << "Failed to send custom message" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "Invalid JSON: " << e.what() << std::endl;
        }
    }
    
    void showHelp() {
        std::cout << "\nAvailable Commands:" << std::endl;
        std::cout << "  ping                 - Send ping to server" << std::endl;
        std::cout << "  echo <message>       - Echo message through server" << std::endl;
        std::cout << "  status               - Get server status" << std::endl;
        std::cout << "  stats                - Get client statistics" << std::endl;
        std::cout << "  send <json>          - Send custom JSON message" << std::endl;
        std::cout << "  help                 - Show this help" << std::endl;
        std::cout << "  quit                 - Exit client" << std::endl;
        std::cout << "\nExample custom message:" << std::endl;
        std::cout << "  send {\"type\":\"command\",\"command\":\"custom\",\"data\":\"test\"}" << std::endl;
    }
    
    std::string generateMessageId() const {
        static int counter = 0;
        return "client_msg_" + std::to_string(++counter);
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
    std::cout << "Hydrogen FIFO Client Example" << std::endl;
    std::cout << "============================" << std::endl;
    
    std::string configFile;
    if (argc > 1) {
        configFile = argv[1];
        std::cout << "Using configuration file: " << configFile << std::endl;
    } else {
        std::cout << "Using default configuration" << std::endl;
        std::cout << "Usage: " << argv[0] << " [config_file.json]" << std::endl;
    }
    
    try {
        FifoClientExample client;
        
        if (!client.start(configFile)) {
            std::cerr << "Failed to start FIFO client" << std::endl;
            return 1;
        }
        
        // Client runs interactively until user quits
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "FIFO client example completed" << std::endl;
    return 0;
}

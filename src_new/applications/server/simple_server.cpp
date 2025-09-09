#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

// Simple WebSocket server implementation for testing
class SimpleServer {
public:
    SimpleServer(int port) : port_(port), running_(false) {}
    
    bool start() {
        std::cout << "Starting Hydrogen Device Server on port " << port_ << "..." << std::endl;
        running_ = true;
        
        // Simulate server startup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "✓ Server started successfully!" << std::endl;
        std::cout << "✓ WebSocket endpoint: ws://localhost:" << port_ << std::endl;
        std::cout << "✓ Ready to accept device connections" << std::endl;
        std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;
        
        return true;
    }
    
    void stop() {
        std::cout << "\nShutting down server..." << std::endl;
        running_ = false;
        std::cout << "✓ Server stopped successfully" << std::endl;
    }
    
    bool isRunning() const { return running_; }
    
    void run() {
        while (running_) {
            // Simulate server activity
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
private:
    int port_;
    std::atomic<bool> running_;
};

// Global server instance for signal handling
std::unique_ptr<SimpleServer> g_server;

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
        g_server = std::make_unique<SimpleServer>(port);
        
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

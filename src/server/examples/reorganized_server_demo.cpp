#include <astrocomm/server/server.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

using namespace astrocomm::server;

std::atomic<bool> running{true};

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

void demonstrateDeviceService() {
    std::cout << "\n🔧 Device Service Demonstration:" << std::endl;
    
    auto& registry = getServiceRegistry();
    auto deviceService = registry.getService<services::IDeviceService>();
    
    if (!deviceService) {
        std::cout << "  ❌ Device service not available" << std::endl;
        return;
    }
    
    // Register a sample device
    services::DeviceInfo telescope;
    telescope.deviceId = "telescope-001";
    telescope.deviceType = "telescope";
    telescope.deviceName = "Main Observatory Telescope";
    telescope.manufacturer = "AstroComm";
    telescope.model = "AC-2000";
    telescope.capabilities = {"goto", "tracking", "imaging", "guiding"};
    telescope.properties["focal_length"] = "2000mm";
    telescope.properties["aperture"] = "200mm";
    telescope.properties["mount_type"] = "equatorial";
    
    if (deviceService->registerDevice(telescope)) {
        std::cout << "  ✅ Registered telescope: " << telescope.deviceName << std::endl;
        
        // Connect the device
        if (deviceService->connectDevice(telescope.deviceId)) {
            std::cout << "  ✅ Connected telescope" << std::endl;
            
            // Execute a command
            services::DeviceCommand gotoCommand;
            gotoCommand.deviceId = telescope.deviceId;
            gotoCommand.command = "goto";
            gotoCommand.parameters["ra"] = "12h30m45s";
            gotoCommand.parameters["dec"] = "+45d30m15s";
            gotoCommand.clientId = "demo_client";
            
            std::string commandId = deviceService->executeCommand(gotoCommand);
            std::cout << "  ✅ Executed goto command: " << commandId << std::endl;
            
            // Wait a moment and check result
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            auto result = deviceService->getCommandResult(commandId);
            if (!result.commandId.empty()) {
                std::cout << "  ✅ Command result: " << (result.success ? "SUCCESS" : "FAILED") 
                         << " - " << result.result << std::endl;
            }
        }
    }
    
    // Show device statistics
    std::cout << "  📊 Device Statistics:" << std::endl;
    std::cout << "    Total devices: " << deviceService->getDeviceCount() << std::endl;
    std::cout << "    Connected devices: " << deviceService->getConnectedDeviceCount() << std::endl;
    
    auto devicesByType = deviceService->getDeviceCountByType();
    for (const auto& pair : devicesByType) {
        std::cout << "    " << pair.first << " devices: " << pair.second << std::endl;
    }
}

void demonstrateAuthService() {
    std::cout << "\n🔐 Authentication Service Demonstration:" << std::endl;
    
    auto& registry = getServiceRegistry();
    auto authService = registry.getService<services::IAuthService>();
    
    if (!authService) {
        std::cout << "  ❌ Authentication service not available" << std::endl;
        return;
    }
    
    // Try to authenticate with default admin user
    services::AuthRequest authReq;
    authReq.username = "admin";
    authReq.password = "admin123!";
    authReq.clientId = "demo_client";
    authReq.remoteAddress = "127.0.0.1";
    authReq.method = services::AuthMethod::BASIC;
    authReq.timestamp = std::chrono::system_clock::now();
    
    auto authResult = authService->authenticate(authReq);
    
    if (authResult.success) {
        std::cout << "  ✅ Authentication successful!" << std::endl;
        std::cout << "    User: " << authResult.token.username << std::endl;
        std::cout << "    Role: " << static_cast<int>(authResult.token.role) << std::endl;
        std::cout << "    Token expires: " << std::chrono::duration_cast<std::chrono::seconds>(
            authResult.token.expiresAt.time_since_epoch()).count() << std::endl;
        
        // Validate the token
        if (authService->validateToken(authResult.token.token)) {
            std::cout << "  ✅ Token validation successful" << std::endl;
        }
        
        // Create a new user
        services::UserInfo newUser;
        newUser.username = "operator1";
        newUser.email = "operator1@astrocomm.local";
        newUser.fullName = "Telescope Operator";
        newUser.role = services::UserRole::OPERATOR;
        
        if (authService->createUser(newUser, "operator123!")) {
            std::cout << "  ✅ Created new user: " << newUser.username << std::endl;
        }
        
        // Show user statistics
        auto allUsers = authService->getAllUsers();
        std::cout << "  📊 User Statistics:" << std::endl;
        std::cout << "    Total users: " << allUsers.size() << std::endl;
        
        for (const auto& user : allUsers) {
            std::cout << "    - " << user.username << " (" 
                     << static_cast<int>(user.role) << ")" << std::endl;
        }
        
    } else {
        std::cout << "  ❌ Authentication failed: " << authResult.errorMessage << std::endl;
    }
}

int main() {
    std::cout << "🚀 AstroComm Server - Reorganized Architecture Demo" << std::endl;
    std::cout << "===================================================" << std::endl;
    
    // Set up signal handling
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        // Initialize the server component
        std::cout << "\n📦 Initializing server component..." << std::endl;
        initialize();
        
        // Create a development server
        std::cout << "🏗️  Creating development server..." << std::endl;
        auto server = presets::createDevelopmentServer(8080);
        
        if (!server) {
            std::cerr << "❌ Failed to create server" << std::endl;
            return 1;
        }
        
        // Display server configuration
        std::cout << "\n⚙️  Server Configuration:" << std::endl;
        auto config = server->getConfiguration();
        for (const auto& pair : config) {
            std::cout << "  " << pair.first << " = " << pair.second << std::endl;
        }
        
        // Start the server
        std::cout << "\n🚀 Starting server..." << std::endl;
        if (!server->startAll()) {
            std::cerr << "❌ Failed to start server" << std::endl;
            return 1;
        }
        
        std::cout << "\n✅ Server started successfully!" << std::endl;
        
        // Show active protocols
        std::cout << "\n🌐 Active Protocols:" << std::endl;
        auto protocols = server->getActiveProtocols();
        for (const auto& protocol : protocols) {
            auto protocolServer = server->getProtocolServer(protocol);
            if (protocolServer) {
                std::cout << "  ✓ " << protocolServer->getProtocolName() 
                         << " (Status: " << protocolServer->getHealthStatus() << ")" << std::endl;
            }
        }
        
        // Show available endpoints
        std::cout << "\n🔗 Available Endpoints:" << std::endl;
        std::cout << "  📡 HTTP API: http://localhost:8080/api/" << std::endl;
        std::cout << "  ❤️  Health Check: http://localhost:8080/api/health" << std::endl;
        std::cout << "  📊 Status: http://localhost:8080/api/status" << std::endl;
        std::cout << "  🔌 WebSocket: ws://localhost:8080/ws" << std::endl;
        std::cout << "  🔐 Login: POST http://localhost:8080/api/auth/login" << std::endl;
        std::cout << "  🔧 Devices: GET http://localhost:8080/api/devices" << std::endl;
        
        // Demonstrate services
        demonstrateDeviceService();
        demonstrateAuthService();
        
        // Show diagnostics
        std::cout << "\n🩺 System Diagnostics:" << std::endl;
        std::cout << "  Health Status: " << diagnostics::getHealthStatus() << std::endl;
        std::cout << "  Ready: " << (diagnostics::isReady() ? "✅ Yes" : "❌ No") << std::endl;
        
        auto metrics = diagnostics::getMetrics();
        std::cout << "  Metrics:" << std::endl;
        for (const auto& pair : metrics) {
            std::cout << "    " << pair.first << ": " << pair.second << std::endl;
        }
        
        std::cout << "\n⏳ Server is running. Press Ctrl+C to stop..." << std::endl;
        std::cout << "\n💡 Try these commands in another terminal:" << std::endl;
        std::cout << "  curl http://localhost:8080/api/health" << std::endl;
        std::cout << "  curl http://localhost:8080/api/status" << std::endl;
        std::cout << "  curl http://localhost:8080/api/devices" << std::endl;
        std::cout << "  curl -X POST http://localhost:8080/api/auth/login \\" << std::endl;
        std::cout << "       -H \"Content-Type: application/json\" \\" << std::endl;
        std::cout << "       -d '{\"username\":\"admin\",\"password\":\"admin123!\"}'" << std::endl;
        
        // Main monitoring loop
        auto lastStatusTime = std::chrono::steady_clock::now();
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Display status every 60 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStatusTime).count() >= 60) {
                std::cout << "\n📊 Periodic Status Update:" << std::endl;
                
                auto overallStatus = server->getOverallStatus();
                std::cout << "  Overall Status: ";
                switch (overallStatus) {
                    case core::ServerStatus::RUNNING:
                        std::cout << "🟢 RUNNING" << std::endl;
                        break;
                    case core::ServerStatus::ERROR:
                        std::cout << "🔴 ERROR" << std::endl;
                        break;
                    default:
                        std::cout << "🟡 " << static_cast<int>(overallStatus) << std::endl;
                        break;
                }
                
                std::cout << "  Total Connections: " << server->getTotalConnectionCount() << std::endl;
                std::cout << "  Health: " << diagnostics::getHealthStatus() << std::endl;
                
                lastStatusTime = now;
            }
        }
        
        // Graceful shutdown
        std::cout << "\n🛑 Shutting down server..." << std::endl;
        server->stopAll();
        
        // Shutdown the server component
        shutdown();
        
        std::cout << "✅ Server stopped successfully. Goodbye!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

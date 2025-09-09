#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <iomanip>
#include <spdlog/spdlog.h>

// Include the new unified connection architecture
#include "hydrogen/core/connection/unified_connection_architecture.h"
#include "hydrogen/core/device/enhanced_device_connection_manager.h"

using namespace hydrogen::core::connection;
using namespace hydrogen::core::device;

/**
 * @brief Enhanced Telescope Device Application
 */
class EnhancedTelescopeDevice {
public:
    EnhancedTelescopeDevice(const std::string& deviceId, const std::string& serverHost, uint16_t serverPort)
        : deviceId_(deviceId), running_(false) {

        // Configure device initialization
        DeviceInitConfig config;
        config.deviceType = DeviceType::TELESCOPE;
        config.deviceId = deviceId;
        config.manufacturer = "Hydrogen";
        config.model = "Virtual Telescope v2.0";
        config.serialNumber = "HT-2024-001";

        // Configure connection settings
        config.connectionConfig.protocol = ProtocolType::WEBSOCKET;
        config.connectionConfig.host = serverHost;
        config.connectionConfig.port = serverPort;
        config.connectionConfig.enableAutoReconnect = true;
        config.connectionConfig.maxRetries = 5;
        config.connectionConfig.retryInterval = std::chrono::seconds(5);
        config.connectionConfig.enableHeartbeat = true;
        config.connectionConfig.heartbeatInterval = std::chrono::seconds(30);

        // Configure device-specific settings
        config.initializationTimeout = std::chrono::milliseconds(10000);
        config.commandTimeout = std::chrono::milliseconds(15000); // Longer timeout for slew commands
        config.enableStatusMonitoring = true;
        config.statusUpdateInterval = std::chrono::seconds(2); // Frequent updates during operations
        config.validateOnConnect = true;
        config.performSelfTest = true;
        config.selfTestTimeout = std::chrono::milliseconds(5000);

        // Create device connection manager
        deviceManager_ = std::make_unique<EnhancedDeviceConnectionManager>(config);

        // Set up device callbacks
        setupDeviceCallbacks();

        // Initialize telescope state
        initializeTelescopeState();
    }

    ~EnhancedTelescopeDevice() {
        stop();
    }

    bool start() {
        std::cout << "Enhanced Hydrogen Telescope Device" << std::endl;
        std::cout << "==================================" << std::endl;
        std::cout << "Device ID: " << deviceId_ << std::endl;
        std::cout << "Device Type: Telescope" << std::endl;
        std::cout << "Manufacturer: Hydrogen" << std::endl;
        std::cout << "Model: Virtual Telescope v2.0" << std::endl;
        std::cout << "Serial Number: HT-2024-001" << std::endl;
        std::cout << std::endl;

        try {
            // Initialize device manager
            if (!deviceManager_->initialize()) {
                std::cerr << "✗ Failed to initialize device manager" << std::endl;
                return false;
            }
            std::cout << "✓ Device manager initialized" << std::endl;

            // Connect to server
            std::cout << "Connecting to server..." << std::endl;
            if (!deviceManager_->connect()) {
                std::cerr << "✗ Failed to connect to server" << std::endl;
                return false;
            }
            std::cout << "✓ Connected to server" << std::endl;

            // Register device
            if (!registerDevice()) {
                std::cerr << "✗ Failed to register device" << std::endl;
                return false;
            }
            std::cout << "✓ Device registered successfully" << std::endl;

            // Start device operations
            running_.store(true);

            // Start background threads
            statusThread_ = std::thread(&EnhancedTelescopeDevice::statusUpdateLoop, this);
            commandThread_ = std::thread(&EnhancedTelescopeDevice::commandProcessingLoop, this);

            std::cout << "✓ Telescope device is now running" << std::endl;
            std::cout << "✓ Health monitoring: Active" << std::endl;
            std::cout << "✓ Auto-reconnection: Enabled" << std::endl;
            std::cout << "✓ Command processing: Ready" << std::endl;
            std::cout << std::endl;

            printTelescopeStatus();
            printSupportedCommands();

            std::cout << "\nPress Ctrl+C to stop the device..." << std::endl;

            return true;

        } catch (const std::exception& e) {
            std::cerr << "✗ Failed to start telescope device: " << e.what() << std::endl;
            return false;
        }
    }

    void stop() {
        if (!running_.load()) {
            return;
        }

        std::cout << "\nShutting down telescope device..." << std::endl;
        running_.store(false);

        // Stop background threads
        if (statusThread_.joinable()) {
            statusThread_.join();
        }
        if (commandThread_.joinable()) {
            commandThread_.join();
        }

        // Disconnect device
        if (deviceManager_) {
            std::cout << "✓ Disconnecting from server..." << std::endl;
            deviceManager_->disconnect();
        }

        // Print final statistics
        printFinalStatistics();

        std::cout << "✓ Telescope device stopped successfully" << std::endl;
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
    void setupDeviceCallbacks() {
        // Set up state change callback
        deviceManager_->setStateCallback(
            [this](DeviceConnectionState state, const std::string& error) {
                handleStateChange(state, error);
            });

        // Set up status update callback
        deviceManager_->setStatusCallback(
            [this](const DeviceStatus& status) {
                handleStatusUpdate(status);
            });

        // Set up command callback
        deviceManager_->setCommandCallback(
            [this](const DeviceResponse& response) {
                handleCommandResponse(response);
            });

        // Set up error callback
        deviceManager_->setErrorCallback(
            [this](const std::string& error, int code) {
                handleError(error, code);
            });
    }

    void initializeTelescopeState() {
        // Initialize telescope-specific state
        currentRA_ = 0.0;
        currentDec_ = 0.0;
        currentAlt_ = 45.0;
        currentAz_ = 180.0;
        isParked_ = true;
        isTracking_ = false;
        isSlewing_ = false;
        slewProgress_ = 0.0;
    }

    bool registerDevice() {
        DeviceCommand registerCommand;
        registerCommand.commandId = "register_" + std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        registerCommand.command = "DEVICE_REGISTER";
        registerCommand.parameters = "type=telescope,id=" + deviceId_;
        registerCommand.timeout = std::chrono::milliseconds(5000);

        auto response = deviceManager_->sendCommandSync(registerCommand);
        return response.success;
    }

    void statusUpdateLoop() {
        while (running_.load()) {
            updateTelescopeStatus();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    void commandProcessingLoop() {
        while (running_.load()) {
            // Simulate telescope operations
            if (isSlewing_) {
                updateSlewProgress();
            }

            if (isTracking_) {
                updateTracking();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void handleStateChange(DeviceConnectionState state, const std::string& error) {
        switch (state) {
            case DeviceConnectionState::CONNECTED:
                spdlog::info("Telescope: Connected to server");
                break;
            case DeviceConnectionState::READY:
                spdlog::info("Telescope: Device ready for commands");
                break;
            case DeviceConnectionState::BUSY:
                spdlog::info("Telescope: Processing command");
                break;
            case DeviceConnectionState::ERROR:
                spdlog::error("Telescope: Connection error - {}", error);
                break;
            case DeviceConnectionState::RECONNECTING:
                spdlog::warn("Telescope: Attempting to reconnect");
                break;
            default:
                break;
        }
    }

    void handleStatusUpdate(const DeviceStatus& status) {
        lastDeviceStatus_ = status;

        if (!status.isHealthy) {
            spdlog::warn("Telescope: Device health warning - {}", status.lastError);
        }
    }

    void handleCommandResponse(const DeviceResponse& response) {
        if (response.success) {
            spdlog::info("Telescope: Command {} completed successfully", response.commandId);
            processSuccessfulCommand(response);
        } else {
            spdlog::error("Telescope: Command {} failed - {}", response.commandId, response.errorMessage);
        }
    }

    void handleError(const std::string& error, int code) {
        spdlog::error("Telescope: Error - {} (Code: {})", error, code);
    }

    void processSuccessfulCommand(const DeviceResponse& response) {
        // Process telescope-specific command responses
        if (response.response.find("SLEW") != std::string::npos) {
            isSlewing_ = true;
            slewProgress_ = 0.0;
        } else if (response.response.find("PARK") != std::string::npos) {
            isParked_ = true;
            isTracking_ = false;
        } else if (response.response.find("UNPARK") != std::string::npos) {
            isParked_ = false;
        } else if (response.response.find("TRACK") != std::string::npos) {
            isTracking_ = true;
        }
    }

    void updateTelescopeStatus() {
        // Update telescope position and status
        if (isSlewing_) {
            // Simulate slew progress
            slewProgress_ += 0.1;
            if (slewProgress_ >= 1.0) {
                isSlewing_ = false;
                slewProgress_ = 1.0;
                spdlog::info("Telescope: Slew completed");
            }
        }

        if (isTracking_ && !isParked_) {
            // Simulate tracking updates
            currentRA_ += 0.0001; // Small tracking adjustment
        }
    }

    void updateSlewProgress() {
        // Simulate slew movement
        if (slewProgress_ < 1.0) {
            slewProgress_ += 0.05; // 5% progress per update
            if (slewProgress_ >= 1.0) {
                slewProgress_ = 1.0;
                isSlewing_ = false;
                spdlog::info("Telescope: Slew completed to target position");
            }
        }
    }

    void updateTracking() {
        // Simulate sidereal tracking
        currentRA_ += 0.00004166; // Sidereal rate: 15 arcsec/sec
        if (currentRA_ >= 24.0) {
            currentRA_ -= 24.0;
        }
    }

    void printTelescopeStatus() {
        std::cout << "Current Telescope Status:" << std::endl;
        std::cout << "========================" << std::endl;
        std::cout << "Position: RA " << std::fixed << std::setprecision(3) << currentRA_
                  << "h, Dec " << currentDec_ << "°" << std::endl;
        std::cout << "Alt/Az: " << currentAlt_ << "°, " << currentAz_ << "°" << std::endl;
        std::cout << "Parked: " << (isParked_ ? "Yes" : "No") << std::endl;
        std::cout << "Tracking: " << (isTracking_ ? "Yes" : "No") << std::endl;
        std::cout << "Slewing: " << (isSlewing_ ? "Yes" : "No") << std::endl;
        if (isSlewing_) {
            std::cout << "Slew Progress: " << std::fixed << std::setprecision(1)
                      << (slewProgress_ * 100.0) << "%" << std::endl;
        }
        std::cout << std::endl;
    }

    void printSupportedCommands() {
        std::cout << "Supported Commands:" << std::endl;
        std::cout << "==================" << std::endl;
        auto commands = deviceManager_->getSupportedCommands();
        for (const auto& command : commands) {
            std::cout << "- " << command << std::endl;
        }
        std::cout << std::endl;
    }

    void printFinalStatistics() {
        std::cout << "\nFinal Telescope Statistics:" << std::endl;
        std::cout << "===========================" << std::endl;

        if (deviceManager_) {
            auto stats = deviceManager_->getConnectionStatistics();
            std::cout << "Commands executed: " << lastDeviceStatus_.commandsExecuted << std::endl;
            std::cout << "Errors encountered: " << lastDeviceStatus_.errorsEncountered << std::endl;
            std::cout << "Messages sent: " << stats.messagesSent.load() << std::endl;
            std::cout << "Messages received: " << stats.messagesReceived.load() << std::endl;
            std::cout << "Average response time: " << lastDeviceStatus_.averageResponseTime.count() << "ms" << std::endl;
            std::cout << "Final health status: " << (lastDeviceStatus_.isHealthy ? "Healthy" : "Unhealthy") << std::endl;
        }

        std::cout << "Final position: RA " << std::fixed << std::setprecision(3) << currentRA_
                  << "h, Dec " << currentDec_ << "°" << std::endl;
    }

    // Device configuration and state
    std::string deviceId_;
    std::atomic<bool> running_;

    // Device manager
    std::unique_ptr<EnhancedDeviceConnectionManager> deviceManager_;

    // Threading
    std::thread statusThread_;
    std::thread commandThread_;

    // Telescope state
    double currentRA_ = 0.0;
    double currentDec_ = 0.0;
    double currentAlt_ = 45.0;
    double currentAz_ = 180.0;
    std::atomic<bool> isParked_{true};
    std::atomic<bool> isTracking_{false};
    std::atomic<bool> isSlewing_{false};
    std::atomic<double> slewProgress_{0.0};

    // Status tracking
    DeviceStatus lastDeviceStatus_;
};

// Global telescope device instance for signal handling
std::unique_ptr<EnhancedTelescopeDevice> g_telescope;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down telescope gracefully..." << std::endl;

    if (g_telescope) {
        g_telescope->stop();
    }

    exit(0);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string deviceId = "telescope_01";
    std::string serverHost = "localhost";
    uint16_t serverPort = 8000;

    if (argc >= 2) {
        deviceId = argv[1];
    }
    if (argc >= 3) {
        serverHost = argv[2];
    }
    if (argc >= 4) {
        serverPort = static_cast<uint16_t>(std::stoi(argv[3]));
    }

    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Configure logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    std::cout << "Starting Enhanced Hydrogen Telescope Device..." << std::endl;
    std::cout << "Arguments: deviceId=" << deviceId << ", server=" << serverHost << ":" << serverPort << std::endl;
    std::cout << std::endl;

    try {
        // Create and start telescope device
        g_telescope = std::make_unique<EnhancedTelescopeDevice>(deviceId, serverHost, serverPort);

        if (!g_telescope->start()) {
            std::cerr << "Failed to start telescope device" << std::endl;
            return 1;
        }

        // Run the device
        g_telescope->run();

    } catch (const std::exception& e) {
        std::cerr << "Telescope device error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
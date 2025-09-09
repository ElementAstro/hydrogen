#include "hydrogen/core/device/enhanced_device_connection_manager.h"
#include <algorithm>
#include <sstream>
#include <random>

#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif

namespace hydrogen {
namespace core {
namespace device {

// Enhanced Device Connection Manager Implementation
EnhancedDeviceConnectionManager::EnhancedDeviceConnectionManager(const DeviceInitConfig& config)
    : config_(config) {
    
    // Initialize connection manager
    connectionManager_ = std::make_unique<connection::UnifiedConnectionManager>();
    
    // Set up connection callbacks
    connectionManager_->setStateCallback(
        [this](connection::ConnectionState state, const std::string& error) {
            handleConnectionStateChange(state, error);
        });
    
    connectionManager_->setMessageCallback(
        [this](const std::string& message) {
            handleConnectionMessage(message);
        });
    
    connectionManager_->setErrorCallback(
        [this](const std::string& error, int code) {
            handleConnectionError(error, code);
        });
    
    // Initialize device status
    currentStatus_.deviceId = config_.deviceId;
    currentStatus_.connectionState = DeviceConnectionState::DISCONNECTED;
    currentStatus_.lastUpdate = std::chrono::system_clock::now();
    
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("EnhancedDeviceConnectionManager: Created for device {} ({})", 
                config_.deviceId, static_cast<int>(config_.deviceType));
#endif
}

EnhancedDeviceConnectionManager::~EnhancedDeviceConnectionManager() {
    disconnect();
    
    if (commandProcessingThread_.joinable()) {
        commandProcessingThread_.join();
    }
    
    if (statusMonitoringThread_.joinable()) {
        statusMonitoringThread_.join();
    }
}

bool EnhancedDeviceConnectionManager::initialize() {
    if (initialized_.load()) {
        return true;
    }
    
    updateConnectionState(DeviceConnectionState::INITIALIZING);
    
    try {
        // Initialize device-specific settings
        setupDeviceSpecificSettings();
        
        // Enable health monitoring if requested
        if (config_.enableStatusMonitoring) {
            connectionManager_->enableHealthMonitoring(true);
        }
        
        initialized_.store(true);
        
#ifdef HYDROGEN_HAS_SPDLOG
        spdlog::info("EnhancedDeviceConnectionManager: Initialized device {}", config_.deviceId);
#endif
        return true;
        
    } catch (const std::exception& e) {
        updateConnectionState(DeviceConnectionState::ERROR, e.what());
        notifyError("Device initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool EnhancedDeviceConnectionManager::connect() {
    if (!initialized_.load()) {
        if (!initialize()) {
            return false;
        }
    }
    
    if (connectionState_.load() == DeviceConnectionState::CONNECTED ||
        connectionState_.load() == DeviceConnectionState::READY) {
        return true;
    }
    
    updateConnectionState(DeviceConnectionState::CONNECTING);
    
    try {
        // Connect using the unified connection manager
        if (!connectionManager_->connect(config_.connectionConfig)) {
            updateConnectionState(DeviceConnectionState::ERROR, "Connection failed");
            return false;
        }
        
        // Start background threads
        running_.store(true);
        commandProcessingThread_ = std::thread(&EnhancedDeviceConnectionManager::processCommandQueue, this);
        
        if (config_.enableStatusMonitoring) {
            startStatusMonitoring();
        }
        
        updateConnectionState(DeviceConnectionState::CONNECTED);
        
        // Perform device initialization
        if (!initializeDevice()) {
            updateConnectionState(DeviceConnectionState::ERROR, "Device initialization failed");
            return false;
        }
        
        updateConnectionState(DeviceConnectionState::READY);
        
#ifdef HYDROGEN_HAS_SPDLOG
        spdlog::info("EnhancedDeviceConnectionManager: Device {} connected and ready", config_.deviceId);
#endif
        return true;
        
    } catch (const std::exception& e) {
        updateConnectionState(DeviceConnectionState::ERROR, e.what());
        notifyError("Device connection failed: " + std::string(e.what()));
        return false;
    }
}

void EnhancedDeviceConnectionManager::disconnect() {
    if (connectionState_.load() == DeviceConnectionState::DISCONNECTED) {
        return;
    }
    
    updateConnectionState(DeviceConnectionState::SHUTTING_DOWN);
    
    // Stop background threads
    running_.store(false);
    
    // Abort all pending commands
    abortAllCommands();
    
    // Stop status monitoring
    stopStatusMonitoring();
    
    // Disconnect connection manager
    if (connectionManager_) {
        connectionManager_->disconnect();
    }
    
    // Wait for threads to finish
    if (commandProcessingThread_.joinable()) {
        commandProcessingThread_.join();
    }
    
    updateConnectionState(DeviceConnectionState::DISCONNECTED);
    
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("EnhancedDeviceConnectionManager: Device {} disconnected", config_.deviceId);
#endif
}

bool EnhancedDeviceConnectionManager::isConnected() const {
    auto state = connectionState_.load();
    return state == DeviceConnectionState::CONNECTED || 
           state == DeviceConnectionState::READY ||
           state == DeviceConnectionState::BUSY;
}

bool EnhancedDeviceConnectionManager::isReady() const {
    return connectionState_.load() == DeviceConnectionState::READY;
}

bool EnhancedDeviceConnectionManager::sendCommand(const DeviceCommand& command) {
    if (!isConnected()) {
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(commandMutex_);
        commandQueue_.push(command);
        pendingCommands_[command.commandId] = command;
    }
    
    commandCondition_.notify_one();
    return true;
}

DeviceResponse EnhancedDeviceConnectionManager::sendCommandSync(const DeviceCommand& command) {
    DeviceResponse response;
    response.commandId = command.commandId;
    
    if (!isConnected()) {
        response.success = false;
        response.errorMessage = "Device not connected";
        return response;
    }
    
    auto start = std::chrono::steady_clock::now();
    
    // Create a unique command ID for synchronous operation
    std::string syncCommandId = command.commandId + "_sync_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    
    DeviceCommand syncCommand = command;
    syncCommand.commandId = syncCommandId;
    
    // Send command and wait for response
    std::mutex responseMutex;
    std::condition_variable responseCondition;
    bool responseReceived = false;
    DeviceResponse finalResponse;
    
    // Set up temporary callback for this command
    auto originalCallback = commandCallback_;
    setCommandCallback([&](const DeviceResponse& resp) {
        if (resp.commandId == syncCommandId) {
            std::lock_guard<std::mutex> lock(responseMutex);
            finalResponse = resp;
            responseReceived = true;
            responseCondition.notify_one();
        }
        if (originalCallback) {
            originalCallback(resp);
        }
    });
    
    // Send the command
    if (!sendCommand(syncCommand)) {
        response.success = false;
        response.errorMessage = "Failed to send command";
        setCommandCallback(originalCallback);
        return response;
    }
    
    // Wait for response with timeout
    std::unique_lock<std::mutex> lock(responseMutex);
    if (responseCondition.wait_for(lock, command.timeout, [&] { return responseReceived; })) {
        response = finalResponse;
    } else {
        response.success = false;
        response.errorMessage = "Command timeout";
        abortCommand(syncCommandId);
    }
    
    auto end = std::chrono::steady_clock::now();
    response.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Restore original callback
    setCommandCallback(originalCallback);
    
    return response;
}

bool EnhancedDeviceConnectionManager::abortCommand(const std::string& commandId) {
    std::lock_guard<std::mutex> lock(commandMutex_);
    
    auto it = pendingCommands_.find(commandId);
    if (it != pendingCommands_.end()) {
        pendingCommands_.erase(it);
        
        DeviceResponse response;
        response.commandId = commandId;
        response.success = false;
        response.errorMessage = "Command aborted";
        
        notifyCommandComplete(response);
        return true;
    }
    
    return false;
}

void EnhancedDeviceConnectionManager::abortAllCommands() {
    std::lock_guard<std::mutex> lock(commandMutex_);
    
    // Clear command queue
    std::queue<DeviceCommand> empty;
    commandQueue_.swap(empty);
    
    // Notify all pending commands as aborted
    for (const auto& pair : pendingCommands_) {
        DeviceResponse response;
        response.commandId = pair.first;
        response.success = false;
        response.errorMessage = "Command aborted - device disconnecting";
        
        notifyCommandComplete(response);
    }
    
    pendingCommands_.clear();
}

DeviceStatus EnhancedDeviceConnectionManager::getStatus() const {
    std::lock_guard<std::mutex> lock(statusMutex_);
    return currentStatus_;
}

DeviceConnectionState EnhancedDeviceConnectionManager::getConnectionState() const {
    return connectionState_.load();
}

connection::ConnectionStatistics EnhancedDeviceConnectionManager::getConnectionStatistics() const {
    if (connectionManager_) {
        return connectionManager_->getStatistics();
    }
    return connection::ConnectionStatistics{};
}

void EnhancedDeviceConnectionManager::updateConfig(const DeviceInitConfig& config) {
    config_ = config;
    
    if (connectionManager_) {
        connectionManager_->updateConfig(config.connectionConfig);
    }
}

DeviceInitConfig EnhancedDeviceConnectionManager::getConfig() const {
    return config_;
}

void EnhancedDeviceConnectionManager::setStateCallback(DeviceStateCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    stateCallback_ = std::move(callback);
}

void EnhancedDeviceConnectionManager::setStatusCallback(DeviceStatusCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    statusCallback_ = std::move(callback);
}

void EnhancedDeviceConnectionManager::setCommandCallback(DeviceCommandCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    commandCallback_ = std::move(callback);
}

void EnhancedDeviceConnectionManager::setErrorCallback(DeviceErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = std::move(callback);
}

void EnhancedDeviceConnectionManager::enableHealthMonitoring(bool enable) {
    statusMonitoringEnabled_.store(enable);
    
    if (connectionManager_) {
        connectionManager_->enableHealthMonitoring(enable);
    }
    
    if (enable && !statusMonitoringThread_.joinable() && running_.load()) {
        startStatusMonitoring();
    } else if (!enable && statusMonitoringThread_.joinable()) {
        stopStatusMonitoring();
    }
}

bool EnhancedDeviceConnectionManager::isHealthy() const {
    std::lock_guard<std::mutex> lock(statusMutex_);
    return currentStatus_.isHealthy && 
           connectionManager_ && connectionManager_->isHealthy();
}

void EnhancedDeviceConnectionManager::performSelfTest() {
    if (!isConnected()) {
        notifyError("Cannot perform self-test: device not connected");
        return;
    }
    
    DeviceCommand selfTestCommand;
    selfTestCommand.commandId = "self_test_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    selfTestCommand.command = "SELF_TEST";
    selfTestCommand.timeout = config_.selfTestTimeout;
    
    auto response = sendCommandSync(selfTestCommand);
    
    if (response.success) {
#ifdef HYDROGEN_HAS_SPDLOG
        spdlog::info("EnhancedDeviceConnectionManager: Self-test passed for device {}", config_.deviceId);
#endif
    } else {
        notifyError("Self-test failed: " + response.errorMessage);
    }
}

bool EnhancedDeviceConnectionManager::validateDevice() {
    if (!isConnected()) {
        return false;
    }
    
    return performDeviceValidation();
}

bool EnhancedDeviceConnectionManager::resetDevice() {
    if (!isConnected()) {
        return false;
    }
    
    DeviceCommand resetCommand;
    resetCommand.commandId = "reset_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    resetCommand.command = "RESET";
    resetCommand.timeout = std::chrono::milliseconds(10000);
    
    auto response = sendCommandSync(resetCommand);
    return response.success;
}

std::string EnhancedDeviceConnectionManager::getDeviceInfo() const {
    std::ostringstream info;
    info << "Device ID: " << config_.deviceId << "\n";
    info << "Type: " << static_cast<int>(config_.deviceType) << "\n";
    info << "Manufacturer: " << config_.manufacturer << "\n";
    info << "Model: " << config_.model << "\n";
    info << "Serial: " << config_.serialNumber << "\n";
    info << "State: " << static_cast<int>(connectionState_.load()) << "\n";
    
    if (connectionManager_) {
        auto stats = connectionManager_->getStatistics();
        info << "Messages Sent: " << stats.messagesSent.load() << "\n";
        info << "Messages Received: " << stats.messagesReceived.load() << "\n";
        info << "Errors: " << stats.errorCount.load() << "\n";
    }
    
    return info.str();
}

std::vector<std::string> EnhancedDeviceConnectionManager::getSupportedCommands() const {
    // Return device-type specific commands
    switch (config_.deviceType) {
        case DeviceType::TELESCOPE:
            return {"SLEW", "PARK", "UNPARK", "ABORT", "GET_POSITION", "SET_TRACKING", "SYNC"};
        case DeviceType::CAMERA:
            return {"EXPOSE", "ABORT", "GET_TEMPERATURE", "SET_TEMPERATURE", "GET_BINNING", "SET_BINNING"};
        case DeviceType::FOCUSER:
            return {"MOVE_ABSOLUTE", "MOVE_RELATIVE", "HALT", "GET_POSITION", "SET_TEMPERATURE_COMPENSATION"};
        case DeviceType::FILTER_WHEEL:
            return {"SET_FILTER", "GET_FILTER", "GET_FILTER_COUNT", "GET_FILTER_NAMES"};
        case DeviceType::ROTATOR:
            return {"MOVE_ABSOLUTE", "MOVE_RELATIVE", "HALT", "GET_POSITION", "SYNC"};
        default:
            return {"STATUS", "RESET", "GET_INFO"};
    }
}

// Private method implementations
bool EnhancedDeviceConnectionManager::initializeDevice() {
    try {
        // Perform device validation if enabled
        if (config_.validateOnConnect) {
            if (!performDeviceValidation()) {
                return false;
            }
        }

        // Perform self-test if enabled
        if (config_.performSelfTest) {
            if (!performDeviceSelfTest()) {
                return false;
            }
        }

        // Update device status
        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            currentStatus_.isHealthy = true;
            currentStatus_.lastUpdate = std::chrono::system_clock::now();
        }

        return true;

    } catch (const std::exception& e) {
        notifyError("Device initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool EnhancedDeviceConnectionManager::performDeviceValidation() {
    DeviceCommand validateCommand;
    validateCommand.commandId = "validate_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    validateCommand.command = "GET_INFO";
    validateCommand.timeout = config_.commandTimeout;

    auto response = sendCommandSync(validateCommand);

    if (response.success) {
        // Parse device info and validate
        // This would be device-specific validation logic
        return true;
    }

    return false;
}

bool EnhancedDeviceConnectionManager::performDeviceSelfTest() {
    DeviceCommand selfTestCommand;
    selfTestCommand.commandId = "selftest_init_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    selfTestCommand.command = "SELF_TEST";
    selfTestCommand.timeout = config_.selfTestTimeout;

    auto response = sendCommandSync(selfTestCommand);
    return response.success;
}

void EnhancedDeviceConnectionManager::setupDeviceSpecificSettings() {
    switch (config_.deviceType) {
        case DeviceType::TELESCOPE:
            handleTelescopeSpecific();
            break;
        case DeviceType::CAMERA:
            handleCameraSpecific();
            break;
        case DeviceType::FOCUSER:
            handleFocuserSpecific();
            break;
        case DeviceType::FILTER_WHEEL:
            handleFilterWheelSpecific();
            break;
        case DeviceType::ROTATOR:
            handleRotatorSpecific();
            break;
        default:
            // Generic device settings
            break;
    }
}

void EnhancedDeviceConnectionManager::handleConnectionStateChange(
    connection::ConnectionState state, const std::string& error) {

    switch (state) {
        case connection::ConnectionState::CONNECTING:
            updateConnectionState(DeviceConnectionState::CONNECTING);
            break;
        case connection::ConnectionState::CONNECTED:
            updateConnectionState(DeviceConnectionState::CONNECTED);
            break;
        case connection::ConnectionState::DISCONNECTED:
            updateConnectionState(DeviceConnectionState::DISCONNECTED);
            break;
        case connection::ConnectionState::ERROR:
            updateConnectionState(DeviceConnectionState::ERROR, error);
            break;
        case connection::ConnectionState::RECONNECTING:
            updateConnectionState(DeviceConnectionState::RECONNECTING);
            break;
        default:
            break;
    }
}

void EnhancedDeviceConnectionManager::handleConnectionMessage(const std::string& message) {
    // Process incoming message as potential command response
    handleCommandResponse(message);
}

void EnhancedDeviceConnectionManager::handleConnectionError(const std::string& error, int code) {
    notifyError(error, code);

    // Update device status
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        currentStatus_.isHealthy = false;
        currentStatus_.lastError = error;
        currentStatus_.errorsEncountered++;
        currentStatus_.lastUpdate = std::chrono::system_clock::now();
    }
}

void EnhancedDeviceConnectionManager::processCommandQueue() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(commandMutex_);

        commandCondition_.wait(lock, [this] {
            return !commandQueue_.empty() || !running_.load();
        });

        if (!running_.load()) {
            break;
        }

        if (!commandQueue_.empty()) {
            DeviceCommand command = commandQueue_.front();
            commandQueue_.pop();
            lock.unlock();

            // Update state to busy
            if (connectionState_.load() == DeviceConnectionState::READY) {
                updateConnectionState(DeviceConnectionState::BUSY);
            }

            // Send command through connection manager
            if (connectionManager_ && connectionManager_->isConnected()) {
                std::string commandMessage = command.command;
                if (!command.parameters.empty()) {
                    commandMessage += " " + command.parameters;
                }

                if (connectionManager_->sendMessage(commandMessage)) {
                    lastCommandTime_ = std::chrono::system_clock::now();

                    // Update statistics
                    {
                        std::lock_guard<std::mutex> statusLock(statusMutex_);
                        currentStatus_.commandsExecuted++;
                    }
                } else {
                    // Command send failed
                    DeviceResponse response;
                    response.commandId = command.commandId;
                    response.success = false;
                    response.errorMessage = "Failed to send command";

                    notifyCommandComplete(response);

                    // Remove from pending commands
                    std::lock_guard<std::mutex> cmdLock(commandMutex_);
                    pendingCommands_.erase(command.commandId);
                }
            }

            // Return to ready state if no more commands
            if (commandQueue_.empty() && connectionState_.load() == DeviceConnectionState::BUSY) {
                updateConnectionState(DeviceConnectionState::READY);
            }
        }
    }
}

void EnhancedDeviceConnectionManager::handleCommandResponse(const std::string& response) {
    // Parse response and match with pending commands
    // This is a simplified implementation - real implementation would parse protocol-specific responses

    std::lock_guard<std::mutex> lock(commandMutex_);

    if (!pendingCommands_.empty()) {
        // For simplicity, assume FIFO response matching
        auto it = pendingCommands_.begin();
        DeviceCommand command = it->second;
        pendingCommands_.erase(it);

        DeviceResponse deviceResponse = parseResponse(response, command);

        // Update performance metrics
        auto now = std::chrono::system_clock::now();
        auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastCommandTime_);

        deviceResponse.executionTime = responseTime;

        {
            std::lock_guard<std::mutex> perfLock(performanceMutex_);
            responseTimeHistory_.push_back(responseTime);
            if (responseTimeHistory_.size() > 100) {
                responseTimeHistory_.erase(responseTimeHistory_.begin());
            }

            // Calculate average response time
            auto total = std::chrono::milliseconds(0);
            for (const auto& time : responseTimeHistory_) {
                total += time;
            }

            std::lock_guard<std::mutex> statusLock(statusMutex_);
            currentStatus_.averageResponseTime = total / responseTimeHistory_.size();
        }

        notifyCommandComplete(deviceResponse);
    }
}

DeviceResponse EnhancedDeviceConnectionManager::parseResponse(
    const std::string& response, const DeviceCommand& command) {

    DeviceResponse deviceResponse;
    deviceResponse.commandId = command.commandId;
    deviceResponse.response = response;

    // Simple response parsing - real implementation would be protocol-specific
    if (response.find("OK") != std::string::npos ||
        response.find("SUCCESS") != std::string::npos) {
        deviceResponse.success = true;
    } else if (response.find("ERROR") != std::string::npos ||
               response.find("FAIL") != std::string::npos) {
        deviceResponse.success = false;
        deviceResponse.errorMessage = response;
    } else {
        // Assume success for other responses
        deviceResponse.success = true;
    }

    return deviceResponse;
}

void EnhancedDeviceConnectionManager::startStatusMonitoring() {
    if (statusMonitoringEnabled_.load() && !statusMonitoringThread_.joinable()) {
        statusMonitoringThread_ = std::thread(&EnhancedDeviceConnectionManager::statusMonitoringLoop, this);
    }
}

void EnhancedDeviceConnectionManager::stopStatusMonitoring() {
    statusMonitoringEnabled_.store(false);

    if (statusMonitoringThread_.joinable()) {
        statusMonitoringThread_.join();
    }
}

void EnhancedDeviceConnectionManager::updateDeviceStatus() {
    std::lock_guard<std::mutex> lock(statusMutex_);

    currentStatus_.connectionState = connectionState_.load();
    currentStatus_.lastUpdate = std::chrono::system_clock::now();

    // Update health status based on connection and recent errors
    if (connectionManager_) {
        currentStatus_.isHealthy = connectionManager_->isHealthy() &&
                                  (currentStatus_.errorsEncountered == 0 ||
                                   (std::chrono::system_clock::now() - currentStatus_.lastUpdate) > std::chrono::minutes(5));
    }

    notifyStatusUpdate(currentStatus_);
}

void EnhancedDeviceConnectionManager::statusMonitoringLoop() {
    while (statusMonitoringEnabled_.load() && running_.load()) {
        updateDeviceStatus();
        std::this_thread::sleep_for(config_.statusUpdateInterval);
    }
}

// Device-specific handlers
void EnhancedDeviceConnectionManager::handleTelescopeSpecific() {
    // Telescope-specific initialization
    config_.commandTimeout = std::chrono::milliseconds(10000); // Longer timeout for slew commands
    config_.statusUpdateInterval = std::chrono::seconds(2); // More frequent updates during slewing
}

void EnhancedDeviceConnectionManager::handleCameraSpecific() {
    // Camera-specific initialization
    config_.commandTimeout = std::chrono::milliseconds(30000); // Long timeout for exposures
    config_.enableStatusMonitoring = true; // Monitor temperature
}

void EnhancedDeviceConnectionManager::handleFocuserSpecific() {
    // Focuser-specific initialization
    config_.commandTimeout = std::chrono::milliseconds(15000); // Timeout for focus moves
}

void EnhancedDeviceConnectionManager::handleFilterWheelSpecific() {
    // Filter wheel-specific initialization
    config_.commandTimeout = std::chrono::milliseconds(5000); // Quick filter changes
}

void EnhancedDeviceConnectionManager::handleRotatorSpecific() {
    // Rotator-specific initialization
    config_.commandTimeout = std::chrono::milliseconds(20000); // Timeout for rotation
}

// State management and notifications
void EnhancedDeviceConnectionManager::updateConnectionState(
    DeviceConnectionState newState, const std::string& error) {

    DeviceConnectionState oldState = connectionState_.exchange(newState);

    if (oldState != newState) {
        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            currentStatus_.connectionState = newState;
            if (!error.empty()) {
                currentStatus_.lastError = error;
            }
            currentStatus_.lastUpdate = std::chrono::system_clock::now();
        }

        notifyStateChange(newState, error);
    }
}

void EnhancedDeviceConnectionManager::notifyStateChange(
    DeviceConnectionState state, const std::string& error) {

    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (stateCallback_) {
        stateCallback_(state, error);
    }
}

void EnhancedDeviceConnectionManager::notifyStatusUpdate(const DeviceStatus& status) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (statusCallback_) {
        statusCallback_(status);
    }
}

void EnhancedDeviceConnectionManager::notifyCommandComplete(const DeviceResponse& response) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (commandCallback_) {
        commandCallback_(response);
    }
}

void EnhancedDeviceConnectionManager::notifyError(const std::string& error, int code) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (errorCallback_) {
        errorCallback_(error, code);
    }

#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("EnhancedDeviceConnectionManager: Device {} error - {} (Code: {})",
                 config_.deviceId, error, code);
#endif
}

} // namespace device
} // namespace core
} // namespace hydrogen

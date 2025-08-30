#include "enhanced_device_base.h"
#include <astrocomm/core/utils.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <set>

namespace astrocomm {
namespace device {
namespace core {

EnhancedDeviceBase::EnhancedDeviceBase(const DeviceConfiguration& config)
    : config_(config), initialized_(false), running_(false), 
      healthStatus_(DeviceHealthStatus::UNKNOWN), monitoringActive_(false), recoveryAttempts_(0) {
    
    // Initialize error recovery manager
    errorRecoveryManager_ = std::make_unique<astrocomm::core::EnhancedErrorRecoveryManager>();
    
    // Initialize default properties and commands
    initializeDefaultProperties();
    initializeDefaultCommands();
    
    SPDLOG_INFO("Enhanced device base created: {} ({})", config_.deviceId, config_.deviceType);
}

EnhancedDeviceBase::~EnhancedDeviceBase() {
    stop();
    SPDLOG_INFO("Enhanced device base destroyed: {}", config_.deviceId);
}

bool EnhancedDeviceBase::initialize() {
    if (initialized_.load()) {
        return true;
    }
    
    try {
        // Initialize communication
        initializeCommunication();
        
        // Setup event handlers
        setupEventHandlers();
        
        // Initialize device-specific functionality
        if (!initializeDevice()) {
            SPDLOG_ERROR("Failed to initialize device-specific functionality for {}", config_.deviceId);
            return false;
        }
        
        initialized_.store(true);
        updateHealthStatus(DeviceHealthStatus::EXCELLENT);
        
        SPDLOG_INFO("Enhanced device {} initialized successfully", config_.deviceId);
        return true;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to initialize device {}: {}", config_.deviceId, e.what());
        updateHealthStatus(DeviceHealthStatus::CRITICAL);
        return false;
    }
}

bool EnhancedDeviceBase::start() {
    if (!initialized_.load()) {
        SPDLOG_ERROR("Cannot start device {}: not initialized", config_.deviceId);
        return false;
    }
    
    if (running_.load()) {
        return true;
    }
    
    try {
        // Start device-specific functionality
        if (!startDevice()) {
            SPDLOG_ERROR("Failed to start device-specific functionality for {}", config_.deviceId);
            return false;
        }
        
        // Start monitoring threads
        if (config_.enableHealthMonitoring) {
            startHealthMonitoring();
        }
        
        if (config_.enablePerformanceMetrics) {
            startMetricsCollection();
        }
        
        running_.store(true);
        updateHealthStatus(DeviceHealthStatus::EXCELLENT);

        SPDLOG_INFO("Enhanced device {} started successfully", config_.deviceId);
        return true;

    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to start device {}: {}", config_.deviceId, e.what());
        updateHealthStatus(DeviceHealthStatus::CRITICAL);
        return false;
    }
}

void EnhancedDeviceBase::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Stop monitoring
    stopHealthMonitoring();
    stopMetricsCollection();
    
    // Disconnect all protocols
    disconnect();
    
    // Stop device-specific functionality
    stopDevice();
    
    updateHealthStatus(DeviceHealthStatus::UNKNOWN);
    SPDLOG_INFO("Enhanced device {} stopped", config_.deviceId);
}

bool EnhancedDeviceBase::isRunning() const {
    return running_.load();
}

bool EnhancedDeviceBase::connect() {
    if (!communicationManager_) {
        SPDLOG_ERROR("Communication manager not initialized for device {}", config_.deviceId);
        return false;
    }
    
    bool success = communicationManager_->connect();
    if (success) {
        success = connectDevice();
        if (success) {
            updateHealthStatus(DeviceHealthStatus::EXCELLENT);
            recordMetric(MetricType::CUSTOM, "connection_established", 1.0);
        }
    }
    
    return success;
}

bool EnhancedDeviceBase::connect(CommunicationProtocol protocol) {
    if (!communicationManager_) {
        return false;
    }
    
    return communicationManager_->connect(protocol);
}

void EnhancedDeviceBase::disconnect() {
    if (communicationManager_) {
        communicationManager_->disconnect();
    }
    disconnectDevice();
    updateHealthStatus(DeviceHealthStatus::UNKNOWN);
}

void EnhancedDeviceBase::disconnect(CommunicationProtocol protocol) {
    if (communicationManager_) {
        communicationManager_->disconnect(protocol);
    }
}

bool EnhancedDeviceBase::isConnected() const {
    return communicationManager_ && communicationManager_->isConnected();
}

bool EnhancedDeviceBase::isConnected(CommunicationProtocol protocol) const {
    return communicationManager_ && communicationManager_->isConnected(protocol);
}

std::string EnhancedDeviceBase::getDeviceId() const {
    return config_.deviceId;
}

std::string EnhancedDeviceBase::getDeviceType() const {
    return config_.deviceType;
}

json EnhancedDeviceBase::getDeviceInfo() const {
    json info;
    info["deviceId"] = config_.deviceId;
    info["deviceType"] = config_.deviceType;
    info["manufacturer"] = config_.manufacturer;
    info["model"] = config_.model;
    info["firmwareVersion"] = config_.firmwareVersion;
    info["healthStatus"] = static_cast<int>(healthStatus_.load());
    info["isRunning"] = running_.load();
    info["isConnected"] = isConnected();
    info["activeProtocols"] = getActiveProtocols();
    info["capabilities"] = getCapabilities();
    
    // Add device-specific info
    json deviceSpecific = getDeviceSpecificInfo();
    if (!deviceSpecific.empty()) {
        info["deviceSpecific"] = deviceSpecific;
    }
    
    return info;
}

std::vector<EnhancedDeviceBase::DeviceCapability> EnhancedDeviceBase::getCapabilities() const {
    std::vector<DeviceCapability> capabilities = {
        DeviceCapability::CONNECT,
        DeviceCapability::DISCONNECT,
        DeviceCapability::GET_PROPERTIES,
        DeviceCapability::SET_PROPERTIES,
        DeviceCapability::EXECUTE_COMMANDS
    };
    
    if (config_.enableHealthMonitoring) {
        capabilities.push_back(DeviceCapability::HEALTH_MONITORING);
        capabilities.push_back(DeviceCapability::SELF_DIAGNOSTICS);
    }
    
    if (config_.enableAutoRecovery) {
        capabilities.push_back(DeviceCapability::AUTO_RECOVERY);
    }
    
    if (config_.enablePerformanceMetrics) {
        capabilities.push_back(DeviceCapability::PERFORMANCE_METRICS);
    }
    
    capabilities.push_back(DeviceCapability::PROTOCOL_SWITCHING);
    
    // Add device-specific capabilities
    auto deviceSpecific = getDeviceSpecificCapabilities();
    capabilities.insert(capabilities.end(), deviceSpecific.begin(), deviceSpecific.end());
    
    return capabilities;
}

bool EnhancedDeviceBase::setProperty(const std::string& name, const json& value) {
    if (name.empty()) {
        return false;
    }
    
    if (!validateProperty(name, value)) {
        SPDLOG_WARN("Property validation failed for device {} property {}", config_.deviceId, name);
        return false;
    }
    
    json oldValue;
    bool hasOldValue = false;
    
    {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        auto it = properties_.find(name);
        if (it != properties_.end()) {
            oldValue = it->second;
            hasOldValue = true;
        }
        properties_[name] = value;
    }
    
    // Notify property change
    if (!hasOldValue || oldValue != value) {
        notifyPropertyChange(name, oldValue, value);
    }
    
    return true;
}

json EnhancedDeviceBase::getProperty(const std::string& name) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    auto it = properties_.find(name);
    return it != properties_.end() ? it->second : json();
}

json EnhancedDeviceBase::getAllProperties() const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return json(properties_);
}

bool EnhancedDeviceBase::hasProperty(const std::string& name) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return properties_.find(name) != properties_.end();
}

bool EnhancedDeviceBase::registerCommand(const std::string& command, CommandHandler handler) {
    if (command.empty() || !handler) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(commandsMutex_);
    commands_[command] = handler;
    
    SPDLOG_DEBUG("Registered command '{}' for device {}", command, config_.deviceId);
    return true;
}

bool EnhancedDeviceBase::unregisterCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    return commands_.erase(command) > 0;
}

json EnhancedDeviceBase::executeCommand(const std::string& command, const json& parameters) {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    auto it = commands_.find(command);
    
    if (it == commands_.end()) {
        json error;
        error["error"] = "Command not found";
        error["command"] = command;
        return error;
    }
    
    try {
        auto startTime = std::chrono::high_resolution_clock::now();
        json result = it->second(parameters);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        // Record performance metric
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        recordMetric(MetricType::RESPONSE_TIME, "command_execution_time", duration.count(), "microseconds");
        
        return result;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error executing command '{}' for device {}: {}", command, config_.deviceId, e.what());
        json error;
        error["error"] = e.what();
        error["command"] = command;
        return error;
    }
}

std::vector<std::string> EnhancedDeviceBase::getAvailableCommands() const {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    std::vector<std::string> commands;
    commands.reserve(commands_.size());
    
    for (const auto& [command, handler] : commands_) {
        commands.push_back(command);
    }
    
    return commands;
}

DeviceHealthStatus EnhancedDeviceBase::getHealthStatus() const {
    return healthStatus_.load();
}

DeviceMetrics EnhancedDeviceBase::getMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return metrics_;
}

json EnhancedDeviceBase::getHealthReport() const {
    json report;
    report["deviceId"] = config_.deviceId;
    report["healthStatus"] = static_cast<int>(healthStatus_.load());
    report["isRunning"] = running_.load();
    report["isConnected"] = isConnected();
    // report["metrics"] = getMetrics(); // Commented out due to type incompatibility
    report["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return report;
}

bool EnhancedDeviceBase::performSelfDiagnostics() {
    SPDLOG_INFO("Performing self-diagnostics for device {}", config_.deviceId);
    
    bool result = true;
    
    // Check basic functionality
    if (!initialized_.load()) {
        SPDLOG_WARN("Device {} not initialized", config_.deviceId);
        result = false;
    }
    
    // Check communication
    if (!isConnected()) {
        SPDLOG_WARN("Device {} not connected", config_.deviceId);
        result = false;
    }
    
    // Perform device-specific diagnostics
    if (!performDeviceSpecificDiagnostics()) {
        SPDLOG_WARN("Device-specific diagnostics failed for {}", config_.deviceId);
        result = false;
    }
    
    if (result) {
        updateHealthStatus(DeviceHealthStatus::EXCELLENT);
    } else {
        updateHealthStatus(DeviceHealthStatus::POOR);
    }
    
    return result;
}

bool EnhancedDeviceBase::performHealthCheck() {
    return performDeviceSpecificHealthCheck() && performSelfDiagnostics();
}

void EnhancedDeviceBase::recordMetric(MetricType type, const std::string& name, double value, const std::string& unit) {
    if (!config_.enablePerformanceMetrics) {
        return;
    }
    
    PerformanceMeasurement measurement;
    measurement.type = type;
    measurement.name = name;
    measurement.value = value;
    measurement.unit = unit;
    measurement.timestamp = std::chrono::system_clock::now();
    measurement.metadata["deviceId"] = config_.deviceId;
    
    std::lock_guard<std::mutex> lock(metricsMutex_);
    performanceHistory_.push_back(measurement);
    
    // Limit history size
    if (performanceHistory_.size() > config_.metricsHistorySize) {
        performanceHistory_.erase(performanceHistory_.begin());
    }
}

// Private methods
void EnhancedDeviceBase::initializeCommunication() {
    communicationManager_ = std::make_unique<MultiProtocolCommunicationManager>(config_.deviceId);
    
    // Add configured protocols
    for (const auto& protocolConfig : config_.protocols) {
        communicationManager_->addProtocol(protocolConfig);
    }
    
    // Set primary protocol and fallbacks
    communicationManager_->setPrimaryProtocol(config_.primaryProtocol);
    communicationManager_->setFallbackProtocols(config_.fallbackProtocols);
    
    // Enable auto-reconnect if auto-recovery is enabled
    if (config_.enableAutoRecovery) {
        communicationManager_->enableAutoReconnect(true);
        communicationManager_->setReconnectInterval(config_.recoveryDelay.count());
        communicationManager_->setMaxRetries(config_.maxRecoveryAttempts);
    }
}

void EnhancedDeviceBase::setupEventHandlers() {
    if (communicationManager_) {
        communicationManager_->setMessageHandler(
            [this](const std::string& message, CommunicationProtocol protocol) {
                handleCommunicationMessage(message, protocol);
            });
        
        communicationManager_->setConnectionStateHandler(
            [this](ConnectionState state, CommunicationProtocol protocol, const std::string& error) {
                handleConnectionStateChange(state, protocol, error);
            });
    }
}

void EnhancedDeviceBase::initializeDefaultProperties() {
    setProperty("deviceId", config_.deviceId);
    setProperty("deviceType", config_.deviceType);
    setProperty("manufacturer", config_.manufacturer);
    setProperty("model", config_.model);
    setProperty("firmwareVersion", config_.firmwareVersion);
    setProperty("healthStatus", static_cast<int>(DeviceHealthStatus::UNKNOWN));
    setProperty("isRunning", false);
    setProperty("isConnected", false);
}

void EnhancedDeviceBase::initializeDefaultCommands() {
    // Register standard commands
    registerCommand("get_device_info", [this]([[maybe_unused]] const json& params) {
        return getDeviceInfo();
    });

    registerCommand("get_health_status", [this]([[maybe_unused]] const json& params) {
        return json{{"healthStatus", static_cast<int>(getHealthStatus())}};
    });

    registerCommand("perform_diagnostics", [this]([[maybe_unused]] const json& params) {
        bool result = performSelfDiagnostics();
        return json{{"success", result}, {"healthStatus", static_cast<int>(getHealthStatus())}};
    });

    registerCommand("get_capabilities", [this]([[maybe_unused]] const json& params) {
        return json{{"capabilities", getCapabilities()}};
    });

    registerCommand("get_properties", [this](const json& params) {
        if (params.contains("properties") && params["properties"].is_array()) {
            json result;
            for (const auto& prop : params["properties"]) {
                if (prop.is_string()) {
                    result[prop] = getProperty(prop);
                }
            }
            return result;
        } else {
            return getAllProperties();
        }
    });
}

void EnhancedDeviceBase::startHealthMonitoring() {
    if (monitoringActive_.load()) {
        return;
    }

    monitoringActive_.store(true);
    healthMonitorThread_ = std::thread(&EnhancedDeviceBase::healthMonitorLoop, this);
}

void EnhancedDeviceBase::stopHealthMonitoring() {
    monitoringActive_.store(false);
    if (healthMonitorThread_.joinable()) {
        healthMonitorThread_.join();
    }
}

void EnhancedDeviceBase::startMetricsCollection() {
    if (monitoringActive_.load()) {
        return;
    }

    metricsCollectionThread_ = std::thread(&EnhancedDeviceBase::metricsCollectionLoop, this);
}

void EnhancedDeviceBase::stopMetricsCollection() {
    if (metricsCollectionThread_.joinable()) {
        metricsCollectionThread_.join();
    }
}

void EnhancedDeviceBase::healthMonitorLoop() {
    while (monitoringActive_.load() && running_.load()) {
        try {
            performHealthCheck();
            std::this_thread::sleep_for(config_.healthCheckInterval);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in health monitor loop for device {}: {}", config_.deviceId, e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void EnhancedDeviceBase::metricsCollectionLoop() {
    while (monitoringActive_.load() && running_.load()) {
        try {
            updateMetrics();
            std::this_thread::sleep_for(config_.metricsCollectionInterval);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in metrics collection loop for device {}: {}", config_.deviceId, e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void EnhancedDeviceBase::updateMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    // Update connection metrics
    if (communicationManager_) {
        auto stats = communicationManager_->getStatistics();
        // Update connection-related metrics based on stats
        if (stats.contains("messages_sent")) {
            // Update metrics based on available statistics
        }
    }

    // Record system metrics
    recordMetric(MetricType::MEMORY_USAGE, "memory_usage", getCurrentMemoryUsage(), "bytes");
    recordMetric(MetricType::CPU_USAGE, "cpu_usage", getCurrentCpuUsage(), "percent");
}

void EnhancedDeviceBase::handleCommunicationMessage(const std::string& message, CommunicationProtocol protocol) {
    try {
        json messageJson = json::parse(message);

        // Handle standard message types
        if (messageJson.contains("command")) {
            std::string command = messageJson["command"];
            json parameters = messageJson.contains("parameters") ? messageJson["parameters"] : json::object();

            json response = executeCommand(command, parameters);

            // Send response back if needed
            if (messageJson.contains("messageId")) {
                json responseMessage;
                responseMessage["messageId"] = messageJson["messageId"];
                responseMessage["response"] = response;

                if (communicationManager_) {
                    communicationManager_->sendMessage(responseMessage, protocol);
                }
            }
        }

        // Call user message handler
        if (messageHandler_) {
            messageHandler_(message, protocol);
        }

    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error handling message for device {}: {}", config_.deviceId, e.what());
    }
}

void EnhancedDeviceBase::handleConnectionStateChange(ConnectionState state, CommunicationProtocol protocol, const std::string& error) {
    switch (state) {
        case ConnectionState::CONNECTED:
            SPDLOG_INFO("Device {} connected via protocol {}", config_.deviceId, static_cast<int>(protocol));
            updateHealthStatus(DeviceHealthStatus::EXCELLENT);
            break;

        case ConnectionState::DISCONNECTED:
            SPDLOG_WARN("Device {} disconnected from protocol {}", config_.deviceId, static_cast<int>(protocol));
            if (!isConnected()) {
                updateHealthStatus(DeviceHealthStatus::POOR);
            }
            break;

        case ConnectionState::ERROR:
            SPDLOG_ERROR("Connection error for device {} on protocol {}: {}", config_.deviceId, static_cast<int>(protocol), error);
            updateHealthStatus(DeviceHealthStatus::CRITICAL);
            if (config_.enableAutoRecovery) {
                handleError("CONNECTION_ERROR", error);
            }
            break;

        default:
            break;
    }
}

bool EnhancedDeviceBase::validateProperty(const std::string& name, [[maybe_unused]] const json& value) const {
    // Basic validation - can be overridden by derived classes
    if (name.empty()) {
        return false;
    }

    // Validate read-only properties
    static const std::set<std::string> readOnlyProperties = {
        "deviceId", "deviceType", "manufacturer", "model"
    };

    if (readOnlyProperties.count(name) > 0) {
        SPDLOG_WARN("Attempt to modify read-only property '{}' for device {}", name, config_.deviceId);
        return false;
    }

    return true;
}

void EnhancedDeviceBase::notifyPropertyChange(const std::string& property, const json& oldValue, const json& newValue) {
    if (propertyChangeHandler_) {
        propertyChangeHandler_(property, oldValue, newValue);
    }
}

void EnhancedDeviceBase::notifyHealthChange(DeviceHealthStatus oldStatus, DeviceHealthStatus newStatus) {
    if (healthChangeHandler_) {
        healthChangeHandler_(oldStatus, newStatus);
    }
}

void EnhancedDeviceBase::notifyError(const std::string& errorCode, const std::string& errorMessage) {
    if (errorHandler_) {
        errorHandler_(errorCode, errorMessage);
    }
}

void EnhancedDeviceBase::updateHealthStatus(DeviceHealthStatus status) {
    DeviceHealthStatus oldStatus = healthStatus_.exchange(status);
    if (oldStatus != status) {
        setProperty("healthStatus", static_cast<int>(status));
        notifyHealthChange(oldStatus, status);
    }
}

// Helper functions for metrics
double EnhancedDeviceBase::getCurrentMemoryUsage() const {
    // Platform-specific implementation would go here
    // For now, return a placeholder value
    return 0.0;
}

double EnhancedDeviceBase::getCurrentCpuUsage() const {
    // Platform-specific implementation would go here
    // For now, return a placeholder value
    return 0.0;
}

} // namespace core
} // namespace device
} // namespace astrocomm

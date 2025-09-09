#include "multi_protocol_communication_manager.h"
#include "hydrogen/core/infrastructure/utils.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <thread>
#include <memory>
#include <string>
#include <mutex>
#include <chrono>

using hydrogen::core::MessageType;
using hydrogen::core::CommunicationMessage;

// Fix Windows header pollution
#ifdef ERROR
#undef ERROR
#endif
#ifdef CONST
#undef CONST
#endif

namespace hydrogen {
namespace device {
namespace core {

MultiProtocolCommunicationManager::MultiProtocolCommunicationManager(const std::string& deviceId)
    : deviceId_(deviceId), primaryProtocol_(CommunicationProtocol::WEBSOCKET),
      autoReconnectEnabled_(true), reconnectInterval_(5000), maxRetries_(3), running_(false) {
    
    initializeCommunicator();
    setupProtocolHandlers();
    
    SPDLOG_INFO("Multi-protocol communication manager initialized for device: {}", deviceId_);
}

MultiProtocolCommunicationManager::~MultiProtocolCommunicationManager() {
    disconnect();
    running_.store(false);
    
    if (reconnectThread_.joinable()) {
        reconnectThread_.join();
    }
    
    SPDLOG_INFO("Multi-protocol communication manager destroyed for device: {}", deviceId_);
}

void MultiProtocolCommunicationManager::initializeCommunicator() {
    // Create a WebSocket communicator as the default
    communicator_ = DeviceCommunicatorFactory::createCommunicator(CommunicationProtocol::WEBSOCKET);
}

void MultiProtocolCommunicationManager::setupProtocolHandlers() {
    communicator_->setMessageCallback([this](const CommunicationMessage& message) {
        // Convert CommunicationMessage to Message and determine protocol
        Message msg(MessageType::COMMAND);
        msg.setMessageId(message.messageId);
        msg.setDeviceId(message.deviceId);
        // Note: CommunicationMessage has command and payload fields that don't directly map to Message
        // This is a simplified conversion - in practice, you'd need proper mapping logic

        // For now, assume WebSocket protocol - this could be enhanced to detect protocol
        handleMessage(msg, CommunicationProtocol::WEBSOCKET);
    });

    communicator_->setConnectionStatusCallback([this](bool connected) {
        // For now, assume WebSocket protocol - this could be enhanced to detect protocol
        handleConnectionChange(CommunicationProtocol::WEBSOCKET, connected);
    });
}

bool MultiProtocolCommunicationManager::addProtocol(const ProtocolConfiguration& protocolConfig) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    // Note: IDeviceCommunicator doesn't have addProtocol method
    // This is a simplified implementation that just tracks the protocol config
    // In a full implementation, you'd need a different architecture
    
    protocolConfigs_[protocolConfig.protocol] = protocolConfig;
    
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        protocolStates_[protocolConfig.protocol] = ConnectionState::DISCONNECTED;
        currentRetries_[protocolConfig.protocol] = 0;
    }
    
    SPDLOG_INFO("Added protocol {} for device {}", 
                static_cast<int>(protocolConfig.protocol), deviceId_);
    return true;
}

bool MultiProtocolCommunicationManager::removeProtocol(CommunicationProtocol protocol) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    // Note: IDeviceCommunicator doesn't have removeProtocol method
    // This is a simplified implementation that just removes the protocol config
    
    protocolConfigs_.erase(protocol);
    
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        protocolStates_.erase(protocol);
        currentRetries_.erase(protocol);
    }
    
    SPDLOG_INFO("Removed protocol {} for device {}", static_cast<int>(protocol), deviceId_);
    return true;
}

bool MultiProtocolCommunicationManager::enableProtocol(CommunicationProtocol protocol, bool enable) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = protocolConfigs_.find(protocol);
    if (it != protocolConfigs_.end()) {
        it->second.enabled = enable;
        SPDLOG_INFO("{} protocol {} for device {}", 
                   enable ? "Enabled" : "Disabled", static_cast<int>(protocol), deviceId_);
        return true;
    }
    
    return false;
}

std::vector<CommunicationProtocol> MultiProtocolCommunicationManager::getActiveProtocols() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    std::vector<CommunicationProtocol> protocols;
    
    for (const auto& [protocol, config] : protocolConfigs_) {
        if (config.enabled) {
            protocols.push_back(protocol);
        }
    }
    
    return protocols;
}

std::vector<CommunicationProtocol> MultiProtocolCommunicationManager::getConnectedProtocols() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    std::vector<CommunicationProtocol> protocols;
    
    for (const auto& [protocol, state] : protocolStates_) {
        if (state == ConnectionState::CONNECTED) {
            protocols.push_back(protocol);
        }
    }
    
    return protocols;
}

bool MultiProtocolCommunicationManager::connect() {
    std::lock_guard<std::mutex> lock(configMutex_);
    bool anySuccess = false;
    
    for (const auto& [protocol, config] : protocolConfigs_) {
        if (config.enabled && config.autoConnect) {
            if (connect(protocol)) {
                anySuccess = true;
            }
        }
    }
    
    if (anySuccess && autoReconnectEnabled_.load() && !running_.load()) {
        running_.store(true);
        reconnectThread_ = std::thread(&MultiProtocolCommunicationManager::reconnectLoop, this);
    }
    
    return anySuccess;
}

bool MultiProtocolCommunicationManager::connect(CommunicationProtocol protocol) {
    updateConnectionState(protocol, ConnectionState::CONNECTING);
    
    // The actual connection is handled by the MultiProtocolDeviceCommunicator
    // We just need to track the state here
    bool success = communicator_->isConnected();
    
    if (success) {
        updateConnectionState(protocol, ConnectionState::CONNECTED);
        std::lock_guard<std::mutex> lock(stateMutex_);
        currentRetries_[protocol] = 0;
    } else {
        updateConnectionState(protocol, ConnectionState::ERROR, "Connection failed");
    }
    
    return success;
}

void MultiProtocolCommunicationManager::disconnect() {
    running_.store(false);
    
    std::lock_guard<std::mutex> lock(configMutex_);
    for (const auto& [protocol, config] : protocolConfigs_) {
        disconnect(protocol);
    }
}

void MultiProtocolCommunicationManager::disconnect(CommunicationProtocol protocol) {
    updateConnectionState(protocol, ConnectionState::DISCONNECTED);
    // The actual disconnection is handled by the MultiProtocolDeviceCommunicator
}

bool MultiProtocolCommunicationManager::isConnected() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    for (const auto& [protocol, state] : protocolStates_) {
        if (state == ConnectionState::CONNECTED) {
            return true;
        }
    }
    return false;
}

bool MultiProtocolCommunicationManager::isConnected(CommunicationProtocol protocol) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = protocolStates_.find(protocol);
    return it != protocolStates_.end() && it->second == ConnectionState::CONNECTED;
}

ConnectionState MultiProtocolCommunicationManager::getConnectionState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    bool hasConnected = false;
    bool hasDisconnected = false;
    bool hasError = false;
    
    for (const auto& [protocol, state] : protocolStates_) {
        switch (state) {
            case ConnectionState::CONNECTED:
                hasConnected = true;
                break;
            case ConnectionState::DISCONNECTED:
                hasDisconnected = true;
                break;
            case ConnectionState::ERROR:
                hasError = true;
                break;
            default:
                break;
        }
    }
    
    if (hasError) return ConnectionState::ERROR;
    if (hasConnected && hasDisconnected) return ConnectionState::PARTIAL_CONNECTION;
    if (hasConnected) return ConnectionState::CONNECTED;
    return ConnectionState::DISCONNECTED;
}

ConnectionState MultiProtocolCommunicationManager::getConnectionState(CommunicationProtocol protocol) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = protocolStates_.find(protocol);
    return it != protocolStates_.end() ? it->second : ConnectionState::DISCONNECTED;
}

bool MultiProtocolCommunicationManager::sendMessage(const std::string& message) {
    return sendMessage(message, primaryProtocol_);
}

bool MultiProtocolCommunicationManager::sendMessage(const std::string& message, CommunicationProtocol protocol) {
    if (!isConnected(protocol)) {
        // Try fallback protocols
        return tryFallbackProtocols(message);
    }
    
    CommunicationMessage msg;
    msg.messageId = hydrogen::core::generateUuid();
    msg.deviceId = deviceId_;
    msg.command = "device_message";
    msg.payload = json{{"message", message}};
    msg.timestamp = std::chrono::system_clock::now();
    
    // sendMessage returns a future, so we need to handle it properly
    auto future = communicator_->sendMessage(msg);
    // For simplicity, we'll assume success if the future is valid
    bool success = future.valid();
    
    if (success) {
        std::lock_guard<std::mutex> lock(statsMutex_);
        messagesSent_[protocol]++;
        lastActivity_[protocol] = std::chrono::system_clock::now();
    }
    
    return success;
}

bool MultiProtocolCommunicationManager::sendMessage(const json& jsonMessage) {
    return sendMessage(jsonMessage.dump());
}

bool MultiProtocolCommunicationManager::sendMessage(const json& jsonMessage, CommunicationProtocol protocol) {
    return sendMessage(jsonMessage.dump(), protocol);
}

bool MultiProtocolCommunicationManager::broadcastMessage(const std::string& message) {
    bool anySuccess = false;
    auto connectedProtocols = getConnectedProtocols();
    
    for (auto protocol : connectedProtocols) {
        if (sendMessage(message, protocol)) {
            anySuccess = true;
        }
    }
    
    return anySuccess;
}

bool MultiProtocolCommunicationManager::broadcastMessage(const json& jsonMessage) {
    return broadcastMessage(jsonMessage.dump());
}

void MultiProtocolCommunicationManager::setPrimaryProtocol(CommunicationProtocol protocol) {
    primaryProtocol_ = protocol;
    SPDLOG_INFO("Set primary protocol to {} for device {}", static_cast<int>(protocol), deviceId_);
}

CommunicationProtocol MultiProtocolCommunicationManager::getPrimaryProtocol() const {
    return primaryProtocol_;
}

void MultiProtocolCommunicationManager::setFallbackProtocols(const std::vector<CommunicationProtocol>& protocols) {
    fallbackProtocols_ = protocols;
}

std::vector<CommunicationProtocol> MultiProtocolCommunicationManager::getFallbackProtocols() const {
    return fallbackProtocols_;
}

void MultiProtocolCommunicationManager::setMessageHandler(ProtocolMessageHandler handler) {
    messageHandler_ = handler;
}

void MultiProtocolCommunicationManager::setConnectionStateHandler(ProtocolConnectionStateHandler handler) {
    connectionStateHandler_ = handler;
}

void MultiProtocolCommunicationManager::enableAutoReconnect(bool enable) {
    autoReconnectEnabled_.store(enable);
}

void MultiProtocolCommunicationManager::setReconnectInterval(int intervalMs) {
    reconnectInterval_.store(intervalMs);
}

void MultiProtocolCommunicationManager::setMaxRetries(int maxRetries) {
    maxRetries_.store(maxRetries);
}

// Internal methods
void MultiProtocolCommunicationManager::updateConnectionState(CommunicationProtocol protocol, ConnectionState state, const std::string& error) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        protocolStates_[protocol] = state;
    }
    
    if (connectionStateHandler_) {
        connectionStateHandler_(state, protocol, error);
    }
    
    SPDLOG_DEBUG("Protocol {} state changed to {} for device {}", 
                static_cast<int>(protocol), static_cast<int>(state), deviceId_);
}

void MultiProtocolCommunicationManager::handleMessage(const Message& message, CommunicationProtocol protocol) {
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        messagesReceived_[protocol]++;
        lastActivity_[protocol] = std::chrono::system_clock::now();
    }
    
    if (messageHandler_) {
        // Convert Message to string using its JSON representation
        std::string messageStr = message.toString();
        messageHandler_(messageStr, protocol);
    }

    // Legacy handler support
    if (legacyMessageHandler_) {
        // Convert Message to string using its JSON representation
        std::string messageStr = message.toString();
        legacyMessageHandler_(messageStr);
    }
}

void MultiProtocolCommunicationManager::handleConnectionChange(CommunicationProtocol protocol, bool connected) {
    ConnectionState newState = connected ? ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
    updateConnectionState(protocol, newState);
}

bool MultiProtocolCommunicationManager::tryFallbackProtocols(const std::string& message) {
    for (auto protocol : fallbackProtocols_) {
        if (isConnected(protocol)) {
            return sendMessage(message, protocol);
        }
    }
    return false;
}

void MultiProtocolCommunicationManager::reconnectLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(reconnectInterval_.load()));

        if (!running_.load()) break;

        std::lock_guard<std::mutex> lock(configMutex_);
        for (const auto& [protocol, config] : protocolConfigs_) {
            if (!config.enabled || !config.autoConnect) continue;

            if (!isConnected(protocol)) {
                std::lock_guard<std::mutex> stateLock(stateMutex_);
                int& retries = currentRetries_[protocol];

                if (retries < maxRetries_.load()) {
                    SPDLOG_INFO("Attempting to reconnect protocol {} for device {} (attempt {}/{})",
                               static_cast<int>(protocol), deviceId_, retries + 1, maxRetries_.load());

                    if (connect(protocol)) {
                        retries = 0;
                    } else {
                        retries++;
                    }
                }
            }
        }
    }
}

json MultiProtocolCommunicationManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    json stats = json::object();

    for (const auto& [protocol, sent] : messagesSent_) {
        json protocolStats;
        protocolStats["messages_sent"] = sent;
        protocolStats["messages_received"] = messagesReceived_.count(protocol) ? messagesReceived_.at(protocol) : 0;

        if (lastActivity_.count(protocol)) {
            auto timePoint = lastActivity_.at(protocol);
            auto timeT = std::chrono::system_clock::to_time_t(timePoint);
            protocolStats["last_activity"] = std::ctime(&timeT);
        }

        stats[std::to_string(static_cast<int>(protocol))] = protocolStats;
    }

    return stats;
}

json MultiProtocolCommunicationManager::getProtocolStatus() const {
    json status = json::object();

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        for (const auto& [protocol, state] : protocolStates_) {
            json protocolStatus;
            protocolStatus["state"] = static_cast<int>(state);
            protocolStatus["connected"] = (state == ConnectionState::CONNECTED);
            protocolStatus["retries"] = currentRetries_.count(protocol) ? currentRetries_.at(protocol) : 0;

            status[std::to_string(static_cast<int>(protocol))] = protocolStatus;
        }
    }

    status["primary_protocol"] = static_cast<int>(primaryProtocol_);
    status["overall_state"] = static_cast<int>(getConnectionState());

    return status;
}

void MultiProtocolCommunicationManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    messagesSent_.clear();
    messagesReceived_.clear();
    lastActivity_.clear();
}

// Legacy compatibility methods
bool MultiProtocolCommunicationManager::connectWebSocket(const std::string& host, uint16_t port) {
    ProtocolConfiguration config;
    config.protocol = CommunicationProtocol::WEBSOCKET;
    config.config = createWebSocketConfig(host, port);
    config.enabled = true;
    config.autoConnect = true;
    config.priority = 10;

    if (addProtocol(config)) {
        setPrimaryProtocol(CommunicationProtocol::WEBSOCKET);
        return connect(CommunicationProtocol::WEBSOCKET);
    }

    return false;
}

void MultiProtocolCommunicationManager::startMessageLoop() {
    // This is handled automatically by the multi-protocol communicator
    if (!running_.load()) {
        connect();
    }
}

void MultiProtocolCommunicationManager::stopMessageLoop() {
    disconnect();
}

void MultiProtocolCommunicationManager::setMessageHandler(std::function<void(const std::string&)> handler) {
    legacyMessageHandler_ = handler;
}

// Helper methods
json MultiProtocolCommunicationManager::createWebSocketConfig(const std::string& host, uint16_t port) const {
    json config;
    config["host"] = host;
    config["port"] = port;
    config["auto_reconnect"] = true;
    config["reconnect_interval"] = 5000;
    config["max_retries"] = 3;
    return config;
}

json MultiProtocolCommunicationManager::createTcpConfig(const std::string& host, uint16_t port, bool isServer) const {
    json config;
    config["serverAddress"] = host;
    config["serverPort"] = port;
    config["isServer"] = isServer;
    config["connectTimeout"] = 5000;
    config["readTimeout"] = 30000;
    config["writeTimeout"] = 5000;
    config["bufferSize"] = 8192;
    config["enableKeepAlive"] = true;
    return config;
}

json MultiProtocolCommunicationManager::createStdioConfig() const {
    json config;
    config["enableLineBuffering"] = true;
    config["enableBinaryMode"] = false;
    config["readTimeout"] = 1000;
    config["writeTimeout"] = 1000;
    config["bufferSize"] = 4096;
    config["lineTerminator"] = "\n";
    config["enableEcho"] = false;
    config["enableFlush"] = true;
    return config;
}

// Factory implementations
std::unique_ptr<MultiProtocolCommunicationManager> CommunicationManagerFactory::createWebSocketOnly(
    const std::string& deviceId, const std::string& host, uint16_t port) {

    auto manager = std::make_unique<MultiProtocolCommunicationManager>(deviceId);

    MultiProtocolCommunicationManager::ProtocolConfiguration config;
    config.protocol = CommunicationProtocol::WEBSOCKET;
    config.config = manager->createWebSocketConfig(host, port);
    config.enabled = true;
    config.autoConnect = true;
    config.priority = 10;

    manager->addProtocol(config);
    manager->setPrimaryProtocol(CommunicationProtocol::WEBSOCKET);

    return manager;
}

std::unique_ptr<MultiProtocolCommunicationManager> CommunicationManagerFactory::createTcpOnly(
    const std::string& deviceId, const std::string& host, uint16_t port, bool isServer) {

    auto manager = std::make_unique<MultiProtocolCommunicationManager>(deviceId);

    MultiProtocolCommunicationManager::ProtocolConfiguration config;
    config.protocol = CommunicationProtocol::TCP;
    config.config = manager->createTcpConfig(host, port, isServer);
    config.enabled = true;
    config.autoConnect = true;
    config.priority = 10;

    manager->addProtocol(config);
    manager->setPrimaryProtocol(CommunicationProtocol::TCP);

    return manager;
}

std::unique_ptr<MultiProtocolCommunicationManager> CommunicationManagerFactory::createStdioOnly(
    const std::string& deviceId) {

    auto manager = std::make_unique<MultiProtocolCommunicationManager>(deviceId);

    MultiProtocolCommunicationManager::ProtocolConfiguration config;
    config.protocol = CommunicationProtocol::STDIO;
    config.config = manager->createStdioConfig();
    config.enabled = true;
    config.autoConnect = true;
    config.priority = 10;

    manager->addProtocol(config);
    manager->setPrimaryProtocol(CommunicationProtocol::STDIO);

    return manager;
}

std::unique_ptr<MultiProtocolCommunicationManager> CommunicationManagerFactory::createWithDefaults(
    const std::string& deviceId, const std::string& host, uint16_t port) {

    auto manager = std::make_unique<MultiProtocolCommunicationManager>(deviceId);

    // Add WebSocket as primary
    MultiProtocolCommunicationManager::ProtocolConfiguration wsConfig;
    wsConfig.protocol = CommunicationProtocol::WEBSOCKET;
    wsConfig.config = manager->createWebSocketConfig(host, port);
    wsConfig.enabled = true;
    wsConfig.autoConnect = true;
    wsConfig.priority = 10;
    manager->addProtocol(wsConfig);

    // Add TCP as fallback
    MultiProtocolCommunicationManager::ProtocolConfiguration tcpConfig;
    tcpConfig.protocol = CommunicationProtocol::TCP;
    tcpConfig.config = manager->createTcpConfig(host, port + 1, false);
    tcpConfig.enabled = true;
    tcpConfig.autoConnect = true;
    tcpConfig.priority = 5;
    manager->addProtocol(tcpConfig);

    manager->setPrimaryProtocol(CommunicationProtocol::WEBSOCKET);
    manager->setFallbackProtocols({CommunicationProtocol::TCP});

    return manager;
}

} // namespace core
} // namespace device
} // namespace hydrogen

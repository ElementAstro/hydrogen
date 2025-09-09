#include "hydrogen/core/connection/unified_connection_architecture.h"
#include "hydrogen/core/connection/protocol_connections.h"
#include <algorithm>
#include <random>
#include <sstream>

#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif

namespace hydrogen {
namespace core {
namespace connection {

// Connection Health Monitor Implementation
ConnectionHealthMonitor::ConnectionHealthMonitor(std::shared_ptr<IProtocolConnection> connection)
    : connection_(std::move(connection))
    , startTime_(std::chrono::system_clock::now()) {
}

ConnectionHealthMonitor::~ConnectionHealthMonitor() {
    stop();
}

void ConnectionHealthMonitor::start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    monitoringThread_ = std::thread(&ConnectionHealthMonitor::monitoringLoop, this);
    
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("ConnectionHealthMonitor: Started health monitoring");
#endif
}

void ConnectionHealthMonitor::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("ConnectionHealthMonitor: Stopped health monitoring");
#endif
}

bool ConnectionHealthMonitor::isHealthy() const {
    return healthy_.load() && connection_ && connection_->isConnected();
}

std::chrono::milliseconds ConnectionHealthMonitor::getLatency() const {
    std::lock_guard<std::mutex> lock(latencyMutex_);
    return currentLatency_;
}

double ConnectionHealthMonitor::getUptime() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime_);
    return duration.count();
}

void ConnectionHealthMonitor::setHealthCallback(std::function<void(bool)> callback) {
    healthCallback_ = std::move(callback);
}

void ConnectionHealthMonitor::monitoringLoop() {
    while (running_.load()) {
        performHealthCheck();
        std::this_thread::sleep_for(checkInterval_);
    }
}

void ConnectionHealthMonitor::performHealthCheck() {
    if (!connection_) {
        healthy_.store(false);
        return;
    }
    
    auto start = std::chrono::steady_clock::now();
    bool isConnected = connection_->isConnected();
    auto end = std::chrono::steady_clock::now();
    
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    updateLatency(latency);
    
    bool wasHealthy = healthy_.load();
    healthy_.store(isConnected);
    
    if (wasHealthy != isConnected && healthCallback_) {
        healthCallback_(isConnected);
    }
}

void ConnectionHealthMonitor::updateLatency(std::chrono::milliseconds latency) {
    std::lock_guard<std::mutex> lock(latencyMutex_);
    // Simple moving average
    currentLatency_ = std::chrono::milliseconds(
        (currentLatency_.count() * 3 + latency.count()) / 4
    );
}

// Connection Pool Implementation
ConnectionPool::ConnectionPool(size_t maxConnections)
    : maxConnections_(maxConnections) {
}

ConnectionPool::~ConnectionPool() {
    cleanup();
}

std::shared_ptr<IProtocolConnection> ConnectionPool::acquireConnection(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    // Look for available connection
    for (auto& pooled : connections_) {
        if (!pooled.inUse && pooled.connection && pooled.connection->isConnected()) {
            pooled.inUse = true;
            pooled.lastUsed = std::chrono::system_clock::now();
            return pooled.connection;
        }
    }
    
    // Create new connection if under limit
    if (connections_.size() < maxConnections_) {
        auto connection = createConnection(config);
        if (connection && connection->connect(config)) {
            PooledConnection pooled;
            pooled.connection = connection;
            pooled.inUse = true;
            pooled.lastUsed = std::chrono::system_clock::now();
            connections_.push_back(pooled);
            return connection;
        }
    }
    
    return nullptr;
}

void ConnectionPool::releaseConnection(std::shared_ptr<IProtocolConnection> connection) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    for (auto& pooled : connections_) {
        if (pooled.connection == connection) {
            pooled.inUse = false;
            pooled.lastUsed = std::chrono::system_clock::now();
            break;
        }
    }
}

void ConnectionPool::setMaxConnections(size_t maxConnections) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    maxConnections_ = maxConnections;
    
    // Clean up excess connections
    while (connections_.size() > maxConnections_) {
        auto it = std::find_if(connections_.begin(), connections_.end(),
            [](const PooledConnection& pooled) { return !pooled.inUse; });
        
        if (it != connections_.end()) {
            if (it->connection) {
                it->connection->disconnect();
            }
            connections_.erase(it);
        } else {
            break; // All remaining connections are in use
        }
    }
}

size_t ConnectionPool::getActiveConnections() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    return std::count_if(connections_.begin(), connections_.end(),
        [](const PooledConnection& pooled) { return pooled.inUse; });
}

size_t ConnectionPool::getAvailableConnections() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    return std::count_if(connections_.begin(), connections_.end(),
        [](const PooledConnection& pooled) { return !pooled.inUse && pooled.connection && pooled.connection->isConnected(); });
}

void ConnectionPool::cleanup() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    for (auto& pooled : connections_) {
        if (pooled.connection) {
            pooled.connection->disconnect();
        }
    }
    connections_.clear();
}

std::shared_ptr<IProtocolConnection> ConnectionPool::createConnection(const ConnectionConfig& config) {
    return ConnectionFactory::createConnection(config.protocol);
}

void ConnectionPool::cleanupIdleConnections() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    auto now = std::chrono::system_clock::now();
    auto idleTimeout = std::chrono::minutes(5); // 5 minutes idle timeout
    
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [now, idleTimeout](const PooledConnection& pooled) {
                return !pooled.inUse && 
                       (now - pooled.lastUsed) > idleTimeout;
            }),
        connections_.end()
    );
}

// Unified Connection Manager Implementation
UnifiedConnectionManager::UnifiedConnectionManager() {
    // Initialize with default configuration
    config_.protocol = ProtocolType::WEBSOCKET;
    config_.host = "localhost";
    config_.port = 8000;
}

UnifiedConnectionManager::~UnifiedConnectionManager() {
    disconnect();
    stopBackgroundThreads();
}

bool UnifiedConnectionManager::connect(const ConnectionConfig& config) {
    if (state_.load() == ConnectionState::CONNECTED) {
        return true;
    }
    
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        config_ = config;
    }
    
    state_.store(ConnectionState::CONNECTING);
    handleStateChange(ConnectionState::CONNECTING);
    
    try {
        initializeConnection();
        
        if (connection_ && connection_->connect(config)) {
            state_.store(ConnectionState::CONNECTED);
            handleStateChange(ConnectionState::CONNECTED);
            
            startBackgroundThreads();
            
#ifdef HYDROGEN_HAS_SPDLOG
            spdlog::info("UnifiedConnectionManager: Connected to {}:{}", 
                        config.host, config.port);
#endif
            return true;
        }
    } catch (const std::exception& e) {
        handleError("Connection failed: " + std::string(e.what()));
    }
    
    state_.store(ConnectionState::ERROR);
    handleStateChange(ConnectionState::ERROR, "Connection failed");
    return false;
}

void UnifiedConnectionManager::disconnect() {
    if (state_.load() == ConnectionState::DISCONNECTED) {
        return;
    }
    
    state_.store(ConnectionState::DISCONNECTING);
    handleStateChange(ConnectionState::DISCONNECTING);
    
    stopBackgroundThreads();
    
    if (connection_) {
        connection_->disconnect();
    }
    
    state_.store(ConnectionState::DISCONNECTED);
    handleStateChange(ConnectionState::DISCONNECTED);
    
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::info("UnifiedConnectionManager: Disconnected");
#endif
}

bool UnifiedConnectionManager::isConnected() const {
    return state_.load() == ConnectionState::CONNECTED && 
           connection_ && connection_->isConnected();
}

bool UnifiedConnectionManager::sendMessage(const std::string& message) {
    if (!isConnected()) {
        return false;
    }
    
    return connection_->sendMessage(message);
}

std::string UnifiedConnectionManager::receiveMessage() {
    if (!isConnected()) {
        return "";
    }
    
    return connection_->receiveMessage();
}

bool UnifiedConnectionManager::hasMessage() const {
    if (!isConnected()) {
        return false;
    }
    
    return connection_->hasMessage();
}

ConnectionState UnifiedConnectionManager::getState() const {
    return state_.load();
}

ConnectionStatistics UnifiedConnectionManager::getStatistics() const {
    if (connection_) {
        return connection_->getStatistics();
    }
    return ConnectionStatistics{};
}

void UnifiedConnectionManager::updateConfig(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

ConnectionConfig UnifiedConnectionManager::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void UnifiedConnectionManager::setStateCallback(ConnectionStateCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    stateCallback_ = std::move(callback);
}

void UnifiedConnectionManager::setMessageCallback(MessageReceivedCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    messageCallback_ = std::move(callback);
}

void UnifiedConnectionManager::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = std::move(callback);
}

void UnifiedConnectionManager::enableHealthMonitoring(bool enable) {
    healthMonitoringEnabled_.store(enable);
    
    if (enable && !healthMonitor_ && connection_) {
        healthMonitor_ = std::make_unique<ConnectionHealthMonitor>(connection_);
        healthMonitor_->start();
    } else if (!enable && healthMonitor_) {
        healthMonitor_->stop();
        healthMonitor_.reset();
    }
}

bool UnifiedConnectionManager::isHealthy() const {
    return healthMonitor_ ? healthMonitor_->isHealthy() : isConnected();
}

std::chrono::milliseconds UnifiedConnectionManager::getLatency() const {
    return healthMonitor_ ? healthMonitor_->getLatency() : std::chrono::milliseconds{0};
}

void UnifiedConnectionManager::enableConnectionPooling(bool enable, size_t maxConnections) {
    connectionPoolingEnabled_.store(enable);
    
    if (enable && !connectionPool_) {
        connectionPool_ = std::make_unique<ConnectionPool>(maxConnections);
    } else if (!enable && connectionPool_) {
        connectionPool_->cleanup();
        connectionPool_.reset();
    }
}

void UnifiedConnectionManager::initializeConnection() {
    ConnectionConfig config;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        config = config_;
    }
    
    if (connectionPoolingEnabled_.load() && connectionPool_) {
        connection_ = connectionPool_->acquireConnection(config);
    } else {
        connection_ = ConnectionFactory::createConnection(config.protocol);
    }
    
    if (connection_) {
        // Set up callbacks
        connection_->setStateCallback([this](ConnectionState state, const std::string& error) {
            handleStateChange(state, error);
        });
        
        connection_->setErrorCallback([this](const std::string& error, int code) {
            handleError(error, code);
        });
        
        if (healthMonitoringEnabled_.load()) {
            healthMonitor_ = std::make_unique<ConnectionHealthMonitor>(connection_);
            healthMonitor_->start();
        }
    }
}

void UnifiedConnectionManager::startBackgroundThreads() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    
    if (config_.enableAutoReconnect) {
        reconnectionThread_ = std::thread(&UnifiedConnectionManager::reconnectionLoop, this);
    }
    
    messageThread_ = std::thread(&UnifiedConnectionManager::messageProcessingLoop, this);
}

void UnifiedConnectionManager::stopBackgroundThreads() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (reconnectionThread_.joinable()) {
        reconnectionThread_.join();
    }
    
    if (messageThread_.joinable()) {
        messageThread_.join();
    }
    
    if (healthMonitor_) {
        healthMonitor_->stop();
        healthMonitor_.reset();
    }
}

void UnifiedConnectionManager::reconnectionLoop() {
    while (running_.load()) {
        if (state_.load() == ConnectionState::ERROR && config_.enableAutoReconnect) {
            std::this_thread::sleep_for(config_.retryInterval);
            
            if (!running_.load()) break;
            
            state_.store(ConnectionState::RECONNECTING);
            handleStateChange(ConnectionState::RECONNECTING);
            
            ConnectionConfig config;
            {
                std::lock_guard<std::mutex> lock(configMutex_);
                config = config_;
            }
            
            if (connect(config)) {
#ifdef HYDROGEN_HAS_SPDLOG
                spdlog::info("UnifiedConnectionManager: Reconnection successful");
#endif
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void UnifiedConnectionManager::messageProcessingLoop() {
    while (running_.load()) {
        if (connection_ && connection_->hasMessage()) {
            std::string message = connection_->receiveMessage();
            if (!message.empty()) {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (messageCallback_) {
                    messageCallback_(message);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void UnifiedConnectionManager::handleStateChange(ConnectionState newState, const std::string& error) {
    state_.store(newState);
    
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (stateCallback_) {
        stateCallback_(newState, error);
    }
}

void UnifiedConnectionManager::handleError(const std::string& error, int errorCode) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (errorCallback_) {
        errorCallback_(error, errorCode);
    }
    
#ifdef HYDROGEN_HAS_SPDLOG
    spdlog::error("UnifiedConnectionManager: Error - {} (Code: {})", error, errorCode);
#endif
}

// Connection Factory Implementation
std::shared_ptr<IProtocolConnection> ConnectionFactory::createConnection(ProtocolType protocol) {
    switch (protocol) {
        case ProtocolType::WEBSOCKET:
            return std::make_shared<WebSocketConnection>();
        case ProtocolType::HTTP:
            return std::make_shared<HttpConnection>();
        case ProtocolType::GRPC:
            return std::make_shared<GrpcConnection>();
        case ProtocolType::STDIO:
            return std::make_shared<StdioConnection>();
        case ProtocolType::FIFO:
            return std::make_shared<FifoConnection>();
        case ProtocolType::TCP:
            return std::make_shared<TcpConnection>();
        case ProtocolType::UDP:
            return std::make_shared<UdpConnection>();
        default:
            return nullptr;
    }
}

std::vector<ProtocolType> ConnectionFactory::getSupportedProtocols() {
    return {
        ProtocolType::WEBSOCKET,
        ProtocolType::HTTP,
        ProtocolType::GRPC,
        ProtocolType::STDIO,
        ProtocolType::FIFO,
        ProtocolType::TCP,
        ProtocolType::UDP
    };
}

std::string ConnectionFactory::getProtocolName(ProtocolType protocol) {
    switch (protocol) {
        case ProtocolType::WEBSOCKET: return "WebSocket";
        case ProtocolType::HTTP: return "HTTP";
        case ProtocolType::GRPC: return "gRPC";
        case ProtocolType::STDIO: return "STDIO";
        case ProtocolType::FIFO: return "FIFO";
        case ProtocolType::TCP: return "TCP";
        case ProtocolType::UDP: return "UDP";
        case ProtocolType::MQTT: return "MQTT";
        case ProtocolType::ZMQ: return "ZeroMQ";
        default: return "Unknown";
    }
}

ProtocolType ConnectionFactory::getProtocolFromName(const std::string& name) {
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    if (lowerName == "websocket" || lowerName == "ws") return ProtocolType::WEBSOCKET;
    if (lowerName == "http") return ProtocolType::HTTP;
    if (lowerName == "grpc") return ProtocolType::GRPC;
    if (lowerName == "stdio") return ProtocolType::STDIO;
    if (lowerName == "fifo") return ProtocolType::FIFO;
    if (lowerName == "tcp") return ProtocolType::TCP;
    if (lowerName == "udp") return ProtocolType::UDP;
    if (lowerName == "mqtt") return ProtocolType::MQTT;
    if (lowerName == "zmq" || lowerName == "zeromq") return ProtocolType::ZMQ;

    return ProtocolType::WEBSOCKET; // Default fallback
}

} // namespace connection
} // namespace core
} // namespace hydrogen

#include "hydrogen/core/performance/tcp_connection.h"

#include <random>
#include <sstream>
#include <thread>

namespace hydrogen {
namespace core {
namespace performance {

// MockConnection implementation

MockConnection::MockConnection(const std::string& id)
    : connectionId_(id.empty() ? "" : id)
    , createdAt_(std::chrono::system_clock::now()) {
    if (connectionId_.empty()) {
        connectionId_ = this->generateConnectionId();
    }
    spdlog::debug("MockConnection: Created connection {}", connectionId_);
}

bool MockConnection::isConnected() const {
    return connected_.load();
}

bool MockConnection::connect() {
    if (connectShouldFail_.load()) {
        spdlog::debug("MockConnection: Connect failed (simulated) for {}", connectionId_);
        return false;
    }
    
    // Simulate connection latency
    if (simulatedLatency_.count() > 0) {
        std::this_thread::sleep_for(simulatedLatency_);
    }
    
    connected_.store(true);
    operationCount_.fetch_add(1);
    
    spdlog::debug("MockConnection: Connected {}", connectionId_);
    return true;
}

void MockConnection::disconnect() {
    connected_.store(false);
    operationCount_.fetch_add(1);
    
    spdlog::debug("MockConnection: Disconnected {}", connectionId_);
}

bool MockConnection::isHealthy() const {
    if (healthCheckShouldFail_.load()) {
        return false;
    }
    
    return healthy_.load() && connected_.load();
}

std::string MockConnection::getId() const {
    return connectionId_;
}

json MockConnection::getMetadata() const {
    json metadata;
    metadata["id"] = connectionId_;
    metadata["type"] = "mock";
    metadata["connected"] = connected_.load();
    metadata["healthy"] = healthy_.load();
    metadata["createdAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        createdAt_.time_since_epoch()).count();
    metadata["operationCount"] = operationCount_.load();
    metadata["simulatedLatency"] = simulatedLatency_.count();
    return metadata;
}

void MockConnection::setConnected(bool connected) {
    connected_.store(connected);
}

void MockConnection::setHealthy(bool healthy) {
    healthy_.store(healthy);
}

void MockConnection::setConnectShouldFail(bool shouldFail) {
    connectShouldFail_.store(shouldFail);
}

void MockConnection::setHealthCheckShouldFail(bool shouldFail) {
    healthCheckShouldFail_.store(shouldFail);
}

void MockConnection::simulateLatency(std::chrono::milliseconds latency) {
    simulatedLatency_ = latency;
}

std::string MockConnection::generateConnectionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "mock_";
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

// MockConnectionFactory implementation

MockConnectionFactory::MockConnectionFactory() {
    spdlog::debug("MockConnectionFactory: Created");
}

std::shared_ptr<IConnection> MockConnectionFactory::createConnection() {
    connectionsCreated_.fetch_add(1);
    
    if (createShouldFail_.load()) {
        spdlog::debug("MockConnectionFactory: Create failed (simulated)");
        return nullptr;
    }
    
    if (connectionsCreated_.load() > maxConnections_.load()) {
        spdlog::debug("MockConnectionFactory: Max connections reached");
        return nullptr;
    }
    
    auto connection = std::make_shared<MockConnection>();
    
    // Apply factory configuration to connection
    if (connectionLatency_.count() > 0) {
        connection->simulateLatency(connectionLatency_);
    }
    
    spdlog::debug("MockConnectionFactory: Created connection {}", connection->getId());
    
    return connection;
}

bool MockConnectionFactory::validateConnection(const std::shared_ptr<IConnection>& connection) {
    validationCalls_.fetch_add(1);
    
    if (validationShouldFail_.load()) {
        spdlog::debug("MockConnectionFactory: Validation failed (simulated)");
        return false;
    }
    
    if (!connection) {
        return false;
    }
    
    // Basic validation
    bool isValid = connection->isConnected() && connection->isHealthy();
    
    spdlog::debug("MockConnectionFactory: Validated connection {} - {}", 
                  connection->getId(), isValid ? "valid" : "invalid");
    
    return isValid;
}

std::string MockConnectionFactory::getConnectionType() const {
    return "mock";
}

void MockConnectionFactory::setCreateShouldFail(bool shouldFail) {
    createShouldFail_.store(shouldFail);
}

void MockConnectionFactory::setValidationShouldFail(bool shouldFail) {
    validationShouldFail_.store(shouldFail);
}

void MockConnectionFactory::setConnectionLatency(std::chrono::milliseconds latency) {
    connectionLatency_ = latency;
}

void MockConnectionFactory::setMaxConnections(size_t maxConnections) {
    maxConnections_.store(maxConnections);
}

size_t MockConnectionFactory::getConnectionsCreated() const {
    return connectionsCreated_.load();
}

size_t MockConnectionFactory::getValidationCalls() const {
    return validationCalls_.load();
}

void MockConnectionFactory::resetStatistics() {
    connectionsCreated_.store(0);
    validationCalls_.store(0);
}

std::string MockConnectionFactory::generateConnectionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "factory_";
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

// ConnectionPoolBuilder implementation

ConnectionPoolBuilder::ConnectionPoolBuilder() {
    // Set default configuration
    config_.minConnections = 5;
    config_.maxConnections = 50;
    config_.initialConnections = 10;
    config_.acquireTimeout = std::chrono::milliseconds{30000};
    config_.idleTimeout = std::chrono::milliseconds{300000};
    config_.maxLifetime = std::chrono::milliseconds{3600000};
    config_.enableHealthChecks = true;
    config_.enableMetrics = true;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withTcpFactory(const std::string& host, int port) {
    factory_ = std::make_shared<TcpConnectionFactory>(host, port);
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withMockFactory() {
    factory_ = std::make_shared<MockConnectionFactory>();
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withCustomFactory(std::shared_ptr<IConnectionFactory> factory) {
    factory_ = factory;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withMinConnections(size_t min) {
    config_.minConnections = min;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withMaxConnections(size_t max) {
    config_.maxConnections = max;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withInitialConnections(size_t initial) {
    config_.initialConnections = initial;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withAcquireTimeout(std::chrono::milliseconds timeout) {
    config_.acquireTimeout = timeout;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withIdleTimeout(std::chrono::milliseconds timeout) {
    config_.idleTimeout = timeout;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withMaxLifetime(std::chrono::milliseconds lifetime) {
    config_.maxLifetime = lifetime;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withHealthChecks(bool enabled) {
    config_.enableHealthChecks = enabled;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withMetrics(bool enabled) {
    config_.enableMetrics = enabled;
    return *this;
}

std::shared_ptr<ConnectionPool> ConnectionPoolBuilder::build() {
    if (!factory_) {
        spdlog::error("ConnectionPoolBuilder: No factory specified, using mock factory");
        factory_ = std::make_shared<MockConnectionFactory>();
    }
    
    auto pool = std::make_shared<ConnectionPool>(factory_, config_);
    
    spdlog::info("ConnectionPoolBuilder: Built connection pool with factory type: {}", 
                 factory_->getConnectionType());
    
    return pool;
}

// TcpConnectionFactory implementation (basic stub)

TcpConnectionFactory::TcpConnectionFactory(const std::string& host, int port)
    : host_(host), port_(port) {
    spdlog::debug("TcpConnectionFactory: Created for {}:{}", host, port);
}

std::shared_ptr<IConnection> TcpConnectionFactory::createConnection() {
    // For now, return a mock connection
    // TODO: Implement actual TCP connection
    spdlog::warn("TcpConnectionFactory: Using mock connection (TCP not implemented yet)");
    return std::make_shared<MockConnection>();
}

bool TcpConnectionFactory::validateConnection(const std::shared_ptr<IConnection>& connection) {
    if (!connection) {
        return false;
    }
    
    return connection->isConnected() && connection->isHealthy();
}

std::string TcpConnectionFactory::getConnectionType() const {
    return "tcp";
}

void TcpConnectionFactory::setTimeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
}

void TcpConnectionFactory::setKeepAlive(bool enabled) {
    keepAlive_ = enabled;
}

void TcpConnectionFactory::setNoDelay(bool enabled) {
    noDelay_ = enabled;
}

} // namespace performance
} // namespace core
} // namespace hydrogen

#pragma once

#include "hydrogen/core/performance/connection_pool.h"

#include <atomic>
#include <string>
#include <memory>

namespace hydrogen {
namespace core {
namespace performance {

/**
 * @brief TCP connection implementation for connection pooling
 */
class TcpConnection : public IConnection {
public:
    explicit TcpConnection(const std::string& host, int port);
    ~TcpConnection() override;

    // IConnection implementation
    bool isConnected() const override;
    bool connect() override;
    void disconnect() override;
    bool isHealthy() const override;
    std::string getId() const override;
    json getMetadata() const override;

    // TCP-specific methods
    bool sendData(const std::string& data);
    std::string receiveData(size_t maxSize = 4096);
    void setTimeout(std::chrono::milliseconds timeout);
    std::string getRemoteAddress() const;
    int getRemotePort() const;

private:
    std::string host_;
    int port_;
    std::string connectionId_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> healthy_{true};
    std::chrono::milliseconds timeout_{30000};
    
    // Platform-specific socket handle
#ifdef _WIN32
    uintptr_t socket_ = 0; // SOCKET on Windows
#else
    int socket_ = -1;      // file descriptor on Unix
#endif
    
    // Connection metadata
    std::chrono::system_clock::time_point connectedAt_;
    std::atomic<size_t> bytesSent_{0};
    std::atomic<size_t> bytesReceived_{0};
    std::atomic<size_t> operationCount_{0};
    
    // Internal methods
    bool performConnect();
    void performDisconnect();
    bool performHealthCheck();
    std::string generateConnectionId();
};

/**
 * @brief TCP connection factory
 */
class TcpConnectionFactory : public IConnectionFactory {
public:
    explicit TcpConnectionFactory(const std::string& host, int port);
    ~TcpConnectionFactory() override = default;

    // IConnectionFactory implementation
    std::shared_ptr<IConnection> createConnection() override;
    bool validateConnection(const std::shared_ptr<IConnection>& connection) override;
    std::string getConnectionType() const override;

    // Configuration
    void setTimeout(std::chrono::milliseconds timeout);
    void setKeepAlive(bool enabled);
    void setNoDelay(bool enabled);

private:
    std::string host_;
    int port_;
    std::chrono::milliseconds timeout_{30000};
    bool keepAlive_ = true;
    bool noDelay_ = true;
};

/**
 * @brief Mock connection for testing
 */
class MockConnection : public IConnection {
public:
    explicit MockConnection(const std::string& id = "");
    ~MockConnection() override = default;

    // IConnection implementation
    bool isConnected() const override;
    bool connect() override;
    void disconnect() override;
    bool isHealthy() const override;
    std::string getId() const override;
    json getMetadata() const override;

    // Mock-specific methods for testing
    void setConnected(bool connected);
    void setHealthy(bool healthy);
    void setConnectShouldFail(bool shouldFail);
    void setHealthCheckShouldFail(bool shouldFail);
    void simulateLatency(std::chrono::milliseconds latency);

private:
    std::string connectionId_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> healthy_{true};
    std::atomic<bool> connectShouldFail_{false};
    std::atomic<bool> healthCheckShouldFail_{false};
    std::chrono::milliseconds simulatedLatency_{0};
    std::chrono::system_clock::time_point createdAt_;
    std::atomic<size_t> operationCount_{0};

    // Internal methods
    std::string generateConnectionId();
};

/**
 * @brief Mock connection factory for testing
 */
class MockConnectionFactory : public IConnectionFactory {
public:
    explicit MockConnectionFactory();
    ~MockConnectionFactory() override = default;

    // IConnectionFactory implementation
    std::shared_ptr<IConnection> createConnection() override;
    bool validateConnection(const std::shared_ptr<IConnection>& connection) override;
    std::string getConnectionType() const override;

    // Mock-specific configuration for testing
    void setCreateShouldFail(bool shouldFail);
    void setValidationShouldFail(bool shouldFail);
    void setConnectionLatency(std::chrono::milliseconds latency);
    void setMaxConnections(size_t maxConnections);
    
    // Statistics
    size_t getConnectionsCreated() const;
    size_t getValidationCalls() const;
    void resetStatistics();

private:
    std::atomic<bool> createShouldFail_{false};
    std::atomic<bool> validationShouldFail_{false};
    std::chrono::milliseconds connectionLatency_{0};
    std::atomic<size_t> maxConnections_{1000};
    
    // Statistics
    mutable std::atomic<size_t> connectionsCreated_{0};
    mutable std::atomic<size_t> validationCalls_{0};
    
    std::string generateConnectionId();
};

/**
 * @brief Connection pool builder for easy configuration
 */
class ConnectionPoolBuilder {
public:
    ConnectionPoolBuilder();
    
    // Factory configuration
    ConnectionPoolBuilder& withTcpFactory(const std::string& host, int port);
    ConnectionPoolBuilder& withMockFactory();
    ConnectionPoolBuilder& withCustomFactory(std::shared_ptr<IConnectionFactory> factory);
    
    // Pool configuration
    ConnectionPoolBuilder& withMinConnections(size_t min);
    ConnectionPoolBuilder& withMaxConnections(size_t max);
    ConnectionPoolBuilder& withInitialConnections(size_t initial);
    ConnectionPoolBuilder& withAcquireTimeout(std::chrono::milliseconds timeout);
    ConnectionPoolBuilder& withIdleTimeout(std::chrono::milliseconds timeout);
    ConnectionPoolBuilder& withMaxLifetime(std::chrono::milliseconds lifetime);
    ConnectionPoolBuilder& withHealthChecks(bool enabled);
    ConnectionPoolBuilder& withMetrics(bool enabled);
    
    // Build the pool
    std::shared_ptr<ConnectionPool> build();

private:
    std::shared_ptr<IConnectionFactory> factory_;
    ConnectionPoolConfig config_;
};

} // namespace performance
} // namespace core
} // namespace hydrogen

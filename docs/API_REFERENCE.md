# Hydrogen API Reference

This document provides a comprehensive reference for the Hydrogen Unified Architecture C++ API.

## Unified Core Classes

### UnifiedDeviceClient

The primary interface for all device interactions across multiple protocols.

```cpp
class UnifiedDeviceClient {
public:
    explicit UnifiedDeviceClient(const ClientConnectionConfig& config);
    ~UnifiedDeviceClient();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;
    void updateConfig(const ClientConnectionConfig& config);
    
    // Device discovery and management
    json discoverDevices(const std::vector<std::string>& deviceTypes = {});
    json getDeviceInfo(const std::string& deviceId);
    json getDeviceProperties(const std::string& deviceId);
    json setDeviceProperties(const std::string& deviceId, const json& properties);
    
    // Device commands
    json executeCommand(const std::string& deviceId, const std::string& command, const json& params);
    json executeCommandAsync(const std::string& deviceId, const std::string& command, const json& params);
    
    // Event handling
    void setMessageHandler(std::function<void(const json&)> handler);
    void setConnectionHandler(std::function<void(bool)> handler);
    void setErrorHandler(std::function<void(const std::string&)> handler);
    void setDeviceEventHandler(std::function<void(const std::string&, const json&)> handler);
    
    // Statistics and monitoring
    ClientStatistics getStatistics() const;
    json getConnectionInfo() const;
    bool isDeviceOnline(const std::string& deviceId) const;
};
```

### UnifiedConnectionManager

Centralized connection management with multi-protocol support.

```cpp
class UnifiedConnectionManager {
public:
    UnifiedConnectionManager();
    ~UnifiedConnectionManager();
    
    // Connection lifecycle
    std::string createConnection(const ConnectionConfig& config);
    bool removeConnection(const std::string& connectionId);
    bool isConnectionActive(const std::string& connectionId) const;
    
    // Message handling
    bool sendMessage(const std::string& connectionId, std::shared_ptr<Message> message);
    bool sendMessageAsync(const std::string& connectionId, std::shared_ptr<Message> message,
                         std::function<void(bool)> callback);
    
    // Connection management
    void setMessageHandler(std::function<void(const std::string&, std::shared_ptr<Message>)> handler);
    void setConnectionStateHandler(std::function<void(const std::string&, ConnectionState)> handler);
    
    // Statistics and monitoring
    ConnectionStatistics getConnectionStatistics(const std::string& connectionId) const;
    GlobalConnectionStatistics getAllStatistics() const;
    PerformanceStatistics getPerformanceStatistics() const;
    
    // Lifecycle management
    void shutdown();
    bool isShutdown() const;
};
```

### ClientConfiguration

Comprehensive configuration management with validation and environment support.

```cpp
class ClientConfiguration : public ConfigurationBase {
public:
    ClientConfiguration();
    ~ClientConfiguration() override = default;
    
    // Configuration sections
    NetworkConfig network;
    AuthConfig authentication;
    LoggingConfig logging;
    PerformanceConfig performance;
    DiscoveryConfig discovery;
    
    // Protocol settings
    MessageFormat defaultProtocol = MessageFormat::HTTP_JSON;
    std::unordered_map<MessageFormat, json> protocolConfigs;
    
    // Feature management
    void enableFeature(const std::string& feature, bool enable = true);
    bool isFeatureEnabled(const std::string& feature) const;
    std::vector<std::string> getEnabledFeatures() const;
    
    // File operations
    bool loadFromFile(const std::string& filePath);
    bool saveToFile(const std::string& filePath) const;
    bool loadFromString(const std::string& jsonString);
    std::string saveToString() const;
    
    // Configuration merging
    void merge(const ClientConfiguration& other);
    void mergeFromJson(const json& j);
    
    // Environment support
    void loadFromEnvironment(const std::string& prefix = "HYDROGEN_");
    
    // Validation
    ConfigValidationResult validate() const override;
    
    // Factory methods
    static ClientConfiguration createDefault();
    static ClientConfiguration createSecure();
    static ClientConfiguration createHighPerformance();
    static ClientConfiguration createDebug();
};
```

### UnifiedWebSocketErrorHandler

Standardized error handling across all components with circuit breaker support.

```cpp
class UnifiedWebSocketErrorHandler : public WebSocketErrorHandler {
public:
    UnifiedWebSocketErrorHandler();
    ~UnifiedWebSocketErrorHandler() override = default;
    
    // Connection management
    void registerConnection(const WebSocketConnectionContext& context);
    void unregisterConnection(const std::string& connectionId);
    void updateConnectionActivity(const std::string& connectionId);
    
    // Enhanced error handling
    void handleError(const WebSocketError& error) override;
    void handleEnhancedError(const EnhancedWebSocketError& error);
    WebSocketRecoveryAction determineRecoveryAction(const WebSocketError& error) override;
    bool shouldRetry(const WebSocketError& error, int attemptCount) override;
    std::chrono::milliseconds getRetryDelay(const WebSocketError& error, int attemptCount) override;
    
    // Circuit breaker management
    std::shared_ptr<WebSocketCircuitBreaker> getCircuitBreaker(const std::string& connectionId);
    void resetCircuitBreaker(const std::string& connectionId);
    
    // Configuration
    void setGlobalRetryPolicy(int maxAttempts, std::chrono::milliseconds baseDelay, bool exponentialBackoff = true);
    void enableCircuitBreaker(bool enable);
    
    // Statistics and monitoring
    UnifiedErrorStatistics getStatistics() const;
    json generateErrorReport(const std::string& connectionId = "") const;
    std::vector<std::string> getTopErrorPatterns(size_t limit = 10) const;
    bool isConnectionHealthy(const std::string& connectionId) const;
};
```

## Configuration Structures

### NetworkConfig

Network connection configuration.

```cpp
struct NetworkConfig {
    std::string host = "localhost";
    uint16_t port = 8080;
    std::string endpoint = "/ws";
    bool useTls = false;
    std::string tlsCertPath;
    std::string tlsKeyPath;
    std::string tlsCaPath;
    bool verifyTlsCertificate = true;
    
    // Timeouts
    std::chrono::milliseconds connectTimeout{5000};
    std::chrono::milliseconds readTimeout{30000};
    std::chrono::milliseconds writeTimeout{30000};
    std::chrono::milliseconds keepAliveInterval{30000};
    
    // Connection limits
    int maxConnections = 100;
    int maxReconnectAttempts = 0; // 0 = unlimited
    std::chrono::milliseconds reconnectInterval{5000};
};
```

### AuthConfig

Authentication configuration with support for multiple auth types.

```cpp
struct AuthConfig {
    enum class AuthType {
        NONE, BASIC, BEARER_TOKEN, API_KEY, OAUTH2, CERTIFICATE, CUSTOM
    };
    
    AuthType type = AuthType::NONE;
    std::string username;
    std::string password;
    std::string token;
    std::string apiKey;
    std::string apiKeyHeader = "X-API-Key";
    
    // OAuth2 settings
    std::string clientId;
    std::string clientSecret;
    std::string authUrl;
    std::string tokenUrl;
    std::vector<std::string> scopes;
    
    // Certificate settings
    std::string certPath;
    std::string keyPath;
    std::string keyPassword;
};
```

### PerformanceConfig

Performance and resource configuration.

```cpp
struct PerformanceConfig {
    // Threading
    size_t workerThreads = 0; // 0 = auto-detect
    size_t ioThreads = 1;
    
    // Memory management
    size_t maxMessageQueueSize = 1000;
    size_t maxCacheSize = 100 * 1024 * 1024; // 100MB
    std::chrono::milliseconds cacheExpiry{300000}; // 5 minutes
    
    // Rate limiting
    size_t maxRequestsPerSecond = 0; // 0 = unlimited
    size_t burstSize = 10;
    
    // Compression
    bool enableCompression = false;
    std::string compressionAlgorithm = "gzip";
    int compressionLevel = 6;
};
```

## Factory Classes

### ConnectionManagerFactory

Factory for creating connection managers with different configurations.

```cpp
class ConnectionManagerFactory {
public:
    static std::unique_ptr<UnifiedConnectionManager> createManagerWithDefaults();
    static std::unique_ptr<UnifiedConnectionManager> createHighPerformanceManager();
    static std::unique_ptr<UnifiedConnectionManager> createSecureManager();
    
    static ConnectionConfig createWebSocketConfig(const std::string& host, uint16_t port);
    static ConnectionConfig createHttpConfig(const std::string& host, uint16_t port);
    static ConnectionConfig createGrpcConfig(const std::string& host, uint16_t port);
    static ConnectionConfig createMqttConfig(const std::string& host, uint16_t port, const std::string& clientId);
    static ConnectionConfig createZmqConfig(const std::string& endpoint, ZmqSocketType socketType);
};
```

### ConfigurationBuilder

Fluent interface for building configurations.

```cpp
class ConfigurationBuilder {
public:
    ConfigurationBuilder();
    
    // Network configuration
    ConfigurationBuilder& withHost(const std::string& host);
    ConfigurationBuilder& withPort(uint16_t port);
    ConfigurationBuilder& withEndpoint(const std::string& endpoint);
    ConfigurationBuilder& withTls(bool enable = true);
    ConfigurationBuilder& withTimeout(std::chrono::milliseconds timeout);
    
    // Authentication
    ConfigurationBuilder& withBasicAuth(const std::string& username, const std::string& password);
    ConfigurationBuilder& withBearerToken(const std::string& token);
    ConfigurationBuilder& withApiKey(const std::string& key, const std::string& header = "X-API-Key");
    
    // Protocol
    ConfigurationBuilder& withProtocol(MessageFormat protocol);
    ConfigurationBuilder& withProtocolConfig(MessageFormat protocol, const json& config);
    
    // Features
    ConfigurationBuilder& withFeature(const std::string& feature, bool enable = true);
    
    // Performance
    ConfigurationBuilder& withWorkerThreads(size_t threads);
    ConfigurationBuilder& withMaxQueueSize(size_t size);
    
    // Logging
    ConfigurationBuilder& withLogLevel(LoggingConfig::LogLevel level);
    ConfigurationBuilder& withLogFile(const std::string& filePath);
    
    // Build
    std::unique_ptr<ClientConfiguration> build();
};
```

## Testing Framework

### ComprehensiveTestFixture

Enhanced test fixture with comprehensive testing capabilities.

```cpp
class ComprehensiveTestFixture : public ::testing::Test {
public:
    ComprehensiveTestFixture();
    virtual ~ComprehensiveTestFixture();

protected:
    void SetUp() override;
    void TearDown() override;
    
    // Configuration
    TestConfig& getConfig();
    const TestConfig& getConfig() const;
    
    // Test utilities
    std::string generateTestId() const;
    std::string createTempFile(const std::string& content = "");
    std::string createTempDirectory();
    
    // Timing utilities
    void startTimer();
    void stopTimer();
    std::chrono::milliseconds getElapsedTime() const;
    
    // Assertion helpers
    void expectWithinTimeout(std::function<bool()> condition, 
                           std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});
    void expectEventually(std::function<bool()> condition, 
                         std::chrono::milliseconds timeout = std::chrono::milliseconds{10000},
                         std::chrono::milliseconds interval = std::chrono::milliseconds{100});
    
    // Performance testing
    void measurePerformance(std::function<void()> operation, const std::string& operationName = "");
    void benchmarkOperation(std::function<void()> operation, size_t iterations, const std::string& name = "");
    
    // Concurrency testing
    void runConcurrentTest(std::function<void(int)> testFunction, size_t threadCount = 4);
    void runStressTest(std::function<void(int)> testFunction, size_t iterations = 1000);
    
    // Data generation
    json generateTestData(const std::string& schema = "");
    std::vector<uint8_t> generateRandomData(size_t size);
    std::string generateRandomString(size_t length);
};
```

## Convenience Macros

### Test Macros

```cpp
// Enhanced test fixture creation
#define HYDROGEN_TEST_FIXTURE(TestClass) \
    class TestClass : public hydrogen::testing::ComprehensiveTestFixture

// Performance test declaration
#define HYDROGEN_PERFORMANCE_TEST(TestClass, TestName) \
    TEST_F(TestClass, TestName) { \
        if (!getConfig().enablePerformanceTesting) { \
            GTEST_SKIP() << "Performance testing disabled"; \
        } \
        /* Test implementation */ \
    }

// Integration test declaration
#define HYDROGEN_INTEGRATION_TEST(TestClass, TestName) \
    TEST_F(TestClass, TestName) { \
        if (!getConfig().enableIntegrationTesting) { \
            GTEST_SKIP() << "Integration testing disabled"; \
        } \
        skipIfNetworkUnavailable(); \
        /* Test implementation */ \
    }

// Stress test declaration
#define HYDROGEN_STRESS_TEST(TestClass, TestName) \
    TEST_F(TestClass, TestName) { \
        if (!getConfig().enableStressTesting) { \
            GTEST_SKIP() << "Stress testing disabled"; \
        } \
        runStressTest([this](int iteration) { \
            /* Test implementation */ \
        }, getConfig().stressTestIterations); \
    }

// Timeout-based assertions
#define EXPECT_WITHIN_TIMEOUT(condition, timeout) \
    expectWithinTimeout([&]() { return condition; }, timeout)

#define EXPECT_EVENTUALLY(condition) \
    expectEventually([&]() { return condition; })

// Performance benchmarking
#define BENCHMARK_OPERATION(operation, name) \
    measurePerformance([&]() { operation; }, name)
```

### Error Handling Macros

```cpp
// Unified error handling
#define HANDLE_WEBSOCKET_ERROR(error, component) \
    UnifiedWebSocketErrorRegistry::getInstance().handleError(error, component)

#define HANDLE_ENHANCED_WEBSOCKET_ERROR(error, component) \
    UnifiedWebSocketErrorRegistry::getInstance().handleEnhancedError(error, component)

// Connection context creation
#define CREATE_WEBSOCKET_CONNECTION_CONTEXT(id, component, endpoint, isClient) \
    WebSocketConnectionContext{id, component, endpoint, isClient, \
                              std::chrono::system_clock::now(), \
                              std::chrono::system_clock::now(), 0, json::object()}
```

## Usage Examples

### Basic Client Usage

```cpp
#include <hydrogen/core/unified_device_client.h>

// Create configuration
auto config = ConfigurationBuilder()
    .withHost("localhost")
    .withPort(8080)
    .withBearerToken("your-token")
    .withFeature("auto_reconnect", true)
    .build();

// Create client
ClientConnectionConfig clientConfig;
clientConfig.host = config->network.host;
clientConfig.port = config->network.port;
clientConfig.enableAutoReconnect = config->isFeatureEnabled("auto_reconnect");

auto client = std::make_unique<UnifiedDeviceClient>(clientConfig);

// Connect and use
if (client->connect()) {
    auto devices = client->discoverDevices();
    auto result = client->executeCommand("camera_001", "start_exposure", {{"duration", 5.0}});
}
```

### Error Handling Setup

```cpp
#include <hydrogen/core/unified_websocket_error_handler.h>

// Create and configure error handler
auto errorHandler = UnifiedWebSocketErrorHandlerFactory::createClientHandler();

errorHandler->setErrorEventCallback([](const WebSocketErrorEvent& event) {
    std::cout << "Error: " << event.error.message << std::endl;
});

// Register with global registry
UnifiedWebSocketErrorRegistry::getInstance().setGlobalHandler(errorHandler);
```

### Testing Example

```cpp
#include "comprehensive_test_framework.h"
#include "mock_objects.h"

HYDROGEN_TEST_FIXTURE(MyTest) {
protected:
    void SetUp() override {
        ComprehensiveTestFixture::SetUp();
        getConfig().enablePerformanceTesting = true;
        
        mockDevice_ = std::make_unique<MockDevice>();
        mockDevice_->setupDefaultBehavior();
    }
    
    std::unique_ptr<MockDevice> mockDevice_;
};

HYDROGEN_PERFORMANCE_TEST(MyTest, DevicePerformance) {
    BENCHMARK_OPERATION({
        mockDevice_->executeCommand("test", json::object());
    }, "device_command");
}
```

For more detailed examples, see the [examples directory](../examples/) and the [Unified Architecture Guide](UNIFIED_ARCHITECTURE.md).

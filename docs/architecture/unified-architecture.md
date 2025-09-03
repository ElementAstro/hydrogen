---
title: Unified Architecture
description: Deep dive into Hydrogen's unified architecture design and core components
---

# Hydrogen Unified Architecture

## Overview

The Hydrogen project has been completely redesigned with a unified architecture that provides consistent interfaces, standardized error handling, and comprehensive testing capabilities across all components. This document describes the new unified architecture and its key components.

## Key Design Principles

### 1. Unified Interfaces
- **Single Point of Entry**: All device interactions go through the `UnifiedDeviceClient`
- **Consistent API**: Same interface patterns across all protocols and device types
- **Protocol Abstraction**: Users don't need to know about underlying protocols

### 2. Centralized Management
- **Connection Management**: `UnifiedConnectionManager` handles all connections
- **Configuration Management**: `ClientConfiguration` provides unified settings
- **Error Handling**: `UnifiedWebSocketErrorHandler` standardizes error processing

### 3. Comprehensive Testing
- **Multi-Level Testing**: Unit, Integration, Performance, and Stress testing
- **Mock Framework**: Complete mock objects for all components
- **Automated Validation**: Built-in test data generation and validation

## Core Components

### UnifiedDeviceClient

The `UnifiedDeviceClient` is the primary interface for all device interactions:

```cpp
#include <hydrogen/core/unified_device_client.h>

// Create client with configuration
ClientConnectionConfig config;
config.host = "localhost";
config.port = 8080;
config.defaultProtocol = MessageFormat::HTTP_JSON;

auto client = std::make_unique<UnifiedDeviceClient>(config);

// Connect and discover devices
client->connect();
auto devices = client->discoverDevices();

// Interact with devices
client->executeCommand("camera_001", "start_exposure", {{"duration", 5.0}});
auto properties = client->getDeviceProperties("telescope_001");
```

**Key Features:**
- Protocol-agnostic device interaction
- Automatic connection management with retry logic
- Built-in error handling and recovery
- Real-time device monitoring
- Comprehensive logging and metrics

### UnifiedConnectionManager

Centralized connection management with advanced features:

```cpp
#include <hydrogen/core/unified_connection_manager.h>

auto manager = std::make_unique<UnifiedConnectionManager>();

// Register connections
ConnectionConfig wsConfig = ConnectionManagerFactory::createWebSocketConfig("localhost", 8080);
std::string connId = manager->createConnection(wsConfig);

// Send messages
auto message = std::make_shared<CommandMessage>("get_status");
manager->sendMessage(connId, message);

// Monitor connections
auto stats = manager->getAllStatistics();
bool healthy = manager->isConnectionHealthy(connId);
```

**Key Features:**
- Multi-protocol support (WebSocket, gRPC, MQTT, ZeroMQ)
- Circuit breaker pattern for fault tolerance
- Connection pooling and reuse
- Automatic reconnection with exponential backoff
- Comprehensive connection statistics

### ClientConfiguration

Unified configuration system with validation and environment support:

```cpp
#include <hydrogen/core/client_configuration.h>

// Create configuration
auto config = ClientConfiguration::createDefault();

// Load from file
config.loadFromFile("config.json");

// Load from environment variables
config.loadFromEnvironment("HYDROGEN_");

// Validate configuration
auto validation = config.validate();
if (!validation.isValid) {
    for (const auto& error : validation.errors) {
        std::cerr << "Config error: " << error << std::endl;
    }
}

// Use configuration builder
auto builderConfig = ConfigurationBuilder()
    .withHost("localhost")
    .withPort(8080)
    .withTls(true)
    .withBearerToken("your-token")
    .withLogLevel(LoggingConfig::LogLevel::DEBUG)
    .build();
```

**Key Features:**
- Hierarchical configuration with defaults
- Environment variable support
- JSON schema validation
- Configuration templates for common scenarios
- Hot-reloading capabilities

### UnifiedWebSocketErrorHandler

Standardized error handling across all components:

```cpp
#include <hydrogen/core/unified_websocket_error_handler.h>

// Create error handler
auto errorHandler = UnifiedWebSocketErrorHandlerFactory::createClientHandler();

// Register connection
WebSocketConnectionContext context = CREATE_WEBSOCKET_CONNECTION_CONTEXT(
    "conn_001", "DeviceClient", "/ws", true);
errorHandler->registerConnection(context);

// Handle errors
WebSocketError error = WebSocketErrorFactory::createConnectionError(
    "Connection timeout", "DeviceClient");
errorHandler->handleError(error);

// Get statistics
auto stats = errorHandler->getStatistics();
auto report = errorHandler->generateErrorReport();
```

**Key Features:**
- Circuit breaker pattern for connection management
- Error correlation and analysis
- Automatic recovery strategies
- Comprehensive error reporting
- Performance impact monitoring

## Testing Framework

### Comprehensive Test Framework

The new testing framework provides extensive testing capabilities:

```cpp
#include "comprehensive_test_framework.h"
#include "mock_objects.h"

HYDROGEN_TEST_FIXTURE(MyComponentTest) {
protected:
    void SetUp() override {
        ComprehensiveTestFixture::SetUp();
        
        // Enable testing modes
        getConfig().enablePerformanceTesting = true;
        getConfig().enableIntegrationTesting = true;
        
        // Setup mocks
        mockClient_ = std::make_unique<MockWebSocketClient>();
        mockClient_->setupDefaultBehavior();
    }
    
    std::unique_ptr<MockWebSocketClient> mockClient_;
};

// Performance test
HYDROGEN_PERFORMANCE_TEST(MyComponentTest, ConnectionPerformance) {
    BENCHMARK_OPERATION({
        client_->connect();
        client_->disconnect();
    }, "connection_cycle");
}

// Stress test
HYDROGEN_STRESS_TEST(MyComponentTest, HighLoadTest) {
    // Framework runs this multiple times
    performOperation();
}

// Integration test
HYDROGEN_INTEGRATION_TEST(MyComponentTest, EndToEndTest) {
    skipIfNetworkUnavailable();
    // Test with real network
}
```

**Key Features:**
- Multiple test types (Unit, Performance, Stress, Integration)
- Comprehensive mock framework
- Automatic performance measurement
- Test data generation and management
- Detailed test reporting

### Mock Objects

Complete mock framework for all components:

```cpp
// Create and configure mocks
auto mockDevice = MockFactory::createMockDevice("test_device");
mockDevice->setupDefaultBehavior();

auto mockServer = MockFactory::createMockWebSocketServer();
mockServer->setupDefaultBehavior();

// Simulate behaviors
mockDevice->simulateOffline();
mockServer->simulateClientConnection("client_001");

// Verify interactions
EXPECT_CALL(*mockDevice, executeCommand(_, _))
    .Times(AtLeast(1))
    .WillRepeatedly(Return(json{{"success", true}}));
```

## Protocol Support

### Multi-Protocol Architecture

The unified architecture supports multiple protocols transparently:

- **HTTP/WebSocket**: RESTful API and real-time communication
- **gRPC**: High-performance RPC with Protocol Buffers
- **MQTT**: Lightweight pub/sub messaging
- **ZeroMQ**: Multiple messaging patterns

### Protocol Converters

Automatic message format conversion between protocols:

```cpp
#include <hydrogen/core/protocol_converters.h>

auto& registry = ConverterRegistry::getInstance();

// Convert message between formats
auto converter = registry.getConverter(MessageFormat::PROTOBUF);
auto protobufData = converter->convertFromJson(jsonMessage);

// Transform messages
auto& transformer = getGlobalMessageTransformer();
auto result = transformer.transform(*message, MessageFormat::HTTP_JSON);
```

## Configuration Examples

### Basic Client Configuration

```json
{
  "network": {
    "host": "localhost",
    "port": 8080,
    "endpoint": "/ws",
    "useTls": false,
    "connectTimeout": 5000
  },
  "authentication": {
    "type": 1,
    "token": "your-bearer-token"
  },
  "logging": {
    "level": 2,
    "enableConsole": true,
    "enableFile": true,
    "logFile": "hydrogen_client.log"
  },
  "performance": {
    "workerThreads": 0,
    "maxMessageQueueSize": 1000,
    "enableCompression": false
  },
  "features": {
    "auto_reconnect": true,
    "device_discovery": true,
    "heartbeat": true,
    "statistics": true
  }
}
```

### Environment Variables

```bash
# Network settings
export HYDROGEN_HOST=localhost
export HYDROGEN_PORT=8080
export HYDROGEN_USE_TLS=false

# Authentication
export HYDROGEN_TOKEN=your-bearer-token

# Logging
export HYDROGEN_LOG_LEVEL=DEBUG
export HYDROGEN_LOG_FILE=hydrogen.log

# Performance
export HYDROGEN_WORKER_THREADS=4
export HYDROGEN_MAX_QUEUE_SIZE=5000
```

## Migration Guide

### From Legacy Components

If you're migrating from legacy components:

1. **Replace DeviceClient with UnifiedDeviceClient**:
   ```cpp
   // Old
   DeviceClient client;
   client.connect("localhost", 8080);
   
   // New
   ClientConnectionConfig config;
   config.host = "localhost";
   config.port = 8080;
   UnifiedDeviceClient client(config);
   client.connect();
   ```

2. **Use Unified Configuration**:
   ```cpp
   // Old
   // Manual configuration management
   
   // New
   auto config = ClientConfiguration::createDefault();
   config.loadFromFile("config.json");
   ```

3. **Leverage Unified Error Handling**:
   ```cpp
   // Old
   try {
       client.connect();
   } catch (const std::exception& e) {
       // Manual error handling
   }
   
   // New
   auto errorHandler = UnifiedWebSocketErrorRegistry::getInstance().getGlobalHandler();
   // Automatic error handling and recovery
   ```

## Best Practices

### 1. Configuration Management
- Use configuration files for environment-specific settings
- Leverage environment variables for deployment flexibility
- Validate configurations before use
- Use configuration templates for common scenarios

### 2. Error Handling
- Register connections with the unified error handler
- Implement custom recovery strategies when needed
- Monitor error statistics and patterns
- Use circuit breakers for external dependencies

### 3. Testing
- Write comprehensive tests using the testing framework
- Use mock objects for unit testing
- Include performance tests for critical paths
- Run integration tests in CI/CD pipelines

### 4. Performance
- Monitor connection statistics regularly
- Use connection pooling for high-throughput scenarios
- Enable compression for large message payloads
- Implement proper resource cleanup

## Troubleshooting

### Common Issues

1. **Connection Failures**
   - Check network connectivity
   - Verify configuration settings
   - Review error handler logs
   - Check circuit breaker status

2. **Performance Issues**
   - Monitor connection statistics
   - Check message queue sizes
   - Review thread pool configuration
   - Analyze performance metrics

3. **Configuration Problems**
   - Validate configuration files
   - Check environment variables
   - Review configuration merge order
   - Use configuration builder for complex setups

For more detailed troubleshooting, see the [Troubleshooting Guide](../user-guide/troubleshooting.md).

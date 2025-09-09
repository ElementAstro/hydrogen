# Hydrogen TCP Communication Protocol

## Overview

The Hydrogen TCP Communication Protocol provides a high-performance, feature-rich TCP/IP communication system designed for real-time applications. It integrates seamlessly with Hydrogen's performance optimization components to deliver exceptional throughput, low latency, and robust error handling.

## Key Features

### 🚀 High Performance
- **Connection Pooling**: Efficient reuse of TCP connections
- **Message Batching**: Automatic batching of messages for improved throughput
- **Memory Pooling**: Optimized memory allocation and reuse
- **Serialization Optimization**: Fast JSON serialization/deserialization

### 🔧 Flexible Architecture
- **Client/Server Modes**: Support for both TCP client and server implementations
- **Multi-Client Support**: Server can handle multiple concurrent client connections
- **Asynchronous Operations**: Non-blocking message sending with futures
- **Thread-Safe Design**: Safe for use in multi-threaded applications

### 🛡️ Robust Error Handling
- **Connection Recovery**: Automatic reconnection on network failures
- **Timeout Management**: Configurable timeouts for all operations
- **Error Callbacks**: Comprehensive error reporting and handling
- **Graceful Degradation**: System remains stable under error conditions

### 📊 Comprehensive Monitoring
- **Real-time Metrics**: Connection, message, and performance statistics
- **Detailed Logging**: Structured logging with configurable levels
- **Health Monitoring**: Connection status and error tracking
- **Performance Analytics**: Latency, throughput, and error rate analysis

## Quick Start

### Basic TCP Server

```cpp
#include "hydrogen/core/communication/protocols/tcp_communicator.h"

using namespace hydrogen::core::communication::protocols;

// Create server configuration
auto config = TcpCommunicatorFactory::createDefaultServerConfig(8001);
auto server = TcpCommunicatorFactory::createServer(config);

// Set up message callback
server->setMessageCallback([](const CommunicationMessage& message) {
    std::cout << "Received: " << message.command << std::endl;
});

// Start server
ConnectionConfig connConfig;
if (server->connect(connConfig)) {
    std::cout << "Server started on port 8001" << std::endl;
}
```

### Basic TCP Client

```cpp
// Create client configuration
auto config = TcpCommunicatorFactory::createDefaultClientConfig("localhost", 8001);
auto client = TcpCommunicatorFactory::createClient(config);

// Connect to server
ConnectionConfig connConfig;
if (client->connect(connConfig)) {
    std::cout << "Connected to server" << std::endl;
    
    // Send message
    CommunicationMessage message;
    message.messageId = "test_001";
    message.deviceId = "client_001";
    message.command = "hello";
    message.payload = json{{"data", "Hello Server!"}};
    message.timestamp = std::chrono::system_clock::now();
    
    auto future = client->sendMessage(message);
    auto response = future.get();
    
    if (response.success) {
        std::cout << "Message sent successfully" << std::endl;
    }
}
```

## Advanced Configuration

### High-Performance Configuration

```cpp
// Create high-performance configuration
auto config = TcpCommunicatorFactory::createHighPerformanceConfig();
config.serverAddress = "localhost";
config.serverPort = 8001;

// Create client with all optimizations enabled
auto client = TcpCommunicatorFactory::createWithPerformanceOptimization(
    config, 
    true,  // Connection pooling
    true,  // Message batching
    true,  // Memory pooling
    true   // Serialization optimization
);
```

### Custom Configuration

```cpp
TcpConnectionConfig config;
config.serverAddress = "192.168.1.100";
config.serverPort = 9001;
config.connectTimeout = std::chrono::milliseconds{3000};
config.readTimeout = std::chrono::milliseconds{15000};
config.writeTimeout = std::chrono::milliseconds{3000};
config.bufferSize = 32768;  // 32KB buffer
config.enableKeepAlive = true;
config.keepAliveInterval = std::chrono::seconds{30};
config.enableNagle = false;  // Disable for low latency
config.enableMessageBatching = true;
config.maxBatchSize = 100;
config.batchTimeout = std::chrono::milliseconds{50};

auto communicator = std::make_shared<TcpCommunicator>(config);
```

## Connection Management

### Using Connection Manager

```cpp
// Get connection manager instance
auto& manager = TcpConnectionManager::getInstance();

// Register connections
manager.registerConnection("primary_server", server);
manager.registerConnection("backup_client", client);

// Retrieve connections
auto retrievedServer = manager.getConnection("primary_server");

// Get all connection metrics
auto allMetrics = manager.getAllConnectionMetrics();

// Start all registered connections
manager.startAllConnections();

// Stop all connections
manager.stopAllConnections();
```

## Performance Optimization

### Message Batching

```cpp
// Enable message batching for improved throughput
client->enableMessageBatching(true);

// Send multiple messages - they will be automatically batched
for (int i = 0; i < 100; ++i) {
    CommunicationMessage message;
    message.messageId = "batch_msg_" + std::to_string(i);
    message.command = "batch_test";
    message.payload = json{{"index", i}};
    message.timestamp = std::chrono::system_clock::now();
    
    client->sendMessage(message);  // Batched automatically
}
```

### Memory Pooling

```cpp
// Enable memory pooling for reduced allocations
client->enableMemoryPooling(true);

// Memory pools are used automatically for string allocations
// No changes needed in application code
```

### Connection Pooling

```cpp
// Enable connection pooling for connection reuse
client->enableConnectionPooling(true);

// Connections are automatically pooled and reused
// Improves performance for frequent connect/disconnect scenarios
```

## Error Handling

### Connection Status Monitoring

```cpp
client->setConnectionStatusCallback([](bool connected) {
    if (connected) {
        std::cout << "Connected to server" << std::endl;
    } else {
        std::cout << "Disconnected from server" << std::endl;
        // Implement reconnection logic here
    }
});
```

### Error Callbacks

```cpp
client->setErrorCallback([](const std::string& error) {
    std::cerr << "Communication error: " << error << std::endl;
    // Log error, trigger alerts, etc.
});
```

### Timeout Handling

```cpp
// Configure timeouts
TcpConnectionConfig config;
config.connectTimeout = std::chrono::milliseconds{5000};
config.readTimeout = std::chrono::milliseconds{30000};
config.writeTimeout = std::chrono::milliseconds{5000};

// Timeouts are automatically handled by the communicator
// Failed operations will be reported through error callbacks
```

## Monitoring and Metrics

### Real-time Statistics

```cpp
// Get communication statistics
auto stats = client->getStatistics();
std::cout << "Messages Sent: " << stats.messagesSent << std::endl;
std::cout << "Messages Received: " << stats.messagesReceived << std::endl;
std::cout << "Average Response Time: " << stats.averageResponseTime << "ms" << std::endl;
std::cout << "Error Count: " << stats.messagesError << std::endl;

// Get TCP-specific metrics
auto tcpMetrics = client->getTcpMetrics();
std::cout << "Connections Established: " << tcpMetrics.connectionsEstablished << std::endl;
std::cout << "Bytes Sent: " << tcpMetrics.bytesSent << std::endl;
std::cout << "Bytes Received: " << tcpMetrics.bytesReceived << std::endl;
```

### Detailed Metrics Export

```cpp
// Export metrics as JSON
json metricsJson = client->getTcpMetrics().toJson();
std::cout << metricsJson.dump(2) << std::endl;

// Server detailed metrics (includes per-client breakdown)
if (server->isServerMode()) {
    json serverMetrics = server->getDetailedTcpMetrics();
    std::cout << serverMetrics.dump(2) << std::endl;
}
```

## Security Features

### SSL/TLS Support

```cpp
// Create secure configuration
auto secureConfig = TcpCommunicatorFactory::createSecureConfig(
    "/path/to/cert.pem",
    "/path/to/key.pem"
);

auto secureClient = std::make_shared<TcpCommunicator>(secureConfig);
```

### Encryption

```cpp
// Enable message encryption
client->setEncryptionEnabled(true, "encryption_key_here");

// Messages are automatically encrypted/decrypted
// No changes needed in application code
```

## Best Practices

### 1. Connection Management
- Use connection pooling for applications with frequent connections
- Implement proper connection status monitoring
- Handle reconnection scenarios gracefully

### 2. Performance Optimization
- Enable message batching for high-throughput scenarios
- Use memory pooling for applications with frequent allocations
- Configure appropriate buffer sizes based on message patterns

### 3. Error Handling
- Always implement error callbacks
- Set appropriate timeouts for your use case
- Log errors for debugging and monitoring

### 4. Resource Management
- Properly disconnect clients and servers on shutdown
- Use RAII principles for automatic cleanup
- Monitor resource usage in production

### 5. Testing
- Test with realistic network conditions
- Verify error handling scenarios
- Performance test under expected load

## Integration with Hydrogen Framework

The TCP communicator integrates seamlessly with other Hydrogen components:

- **Performance Optimization**: Automatic integration with memory pools, connection pools, and serialization optimizers
- **Device Communication**: Implements the standard `IDeviceCommunicator` interface
- **Logging**: Uses Hydrogen's structured logging system
- **Configuration**: Supports JSON-based configuration management

## Platform Support

- **Windows**: Full support with Winsock2
- **Linux**: Full support with standard POSIX sockets
- **macOS**: Full support with BSD sockets
- **Cross-platform**: Consistent API across all platforms

## Dependencies

- **nlohmann/json**: JSON serialization/deserialization
- **spdlog**: High-performance logging
- **Hydrogen Performance Components**: Memory pools, connection pools, etc.
- **Standard Library**: C++17 features (threads, futures, chrono)

## License

This component is part of the Hydrogen framework and follows the same licensing terms.

## Contributing

Please refer to the main Hydrogen framework contribution guidelines for information on how to contribute to this component.

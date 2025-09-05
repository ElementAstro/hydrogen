# Stdio Communication in Hydrogen

This directory contains examples and documentation for using stdio-based communication in the Hydrogen project. The stdio communication protocol allows devices and servers to communicate through standard input/output streams, providing a simple yet powerful communication channel.

## Overview

The stdio communication system in Hydrogen provides:

- **Standard I/O Protocol**: Communication through stdin/stdout streams
- **Message Serialization**: JSON-based message format with multiple framing options
- **Error Handling**: Robust error recovery and connection management
- **Configuration Management**: Flexible configuration with presets for different use cases
- **Logging and Debugging**: Comprehensive logging and tracing capabilities
- **Server Integration**: Full integration with Hydrogen's server architecture

## Features

### Core Features
- Line-buffered and binary communication modes
- Multiple message framing options (delimiter, length-prefix, JSON lines)
- Compression support (GZIP, ZLIB, LZ4)
- Authentication and encryption capabilities
- Heartbeat and reconnection mechanisms
- Flow control and backpressure handling

### Advanced Features
- Performance metrics and monitoring
- Message tracing and debugging
- Circuit breaker pattern for error handling
- Configurable logging with multiple sinks
- Multi-client server support
- Process isolation for client connections

## Configuration

### Configuration Presets

The system provides several built-in configuration presets:

1. **DEFAULT**: Basic stdio configuration suitable for most use cases
2. **HIGH_PERFORMANCE**: Optimized for high throughput scenarios
3. **LOW_LATENCY**: Optimized for minimal latency
4. **RELIABLE**: Maximum reliability with error recovery
5. **SECURE**: Security-focused with authentication and encryption
6. **DEBUG**: Debug-friendly with extensive logging
7. **EMBEDDED**: Optimized for resource-constrained environments

### Configuration Files

Example configuration files are provided:

- `basic_stdio_config.json`: Basic configuration template
- `high_performance_stdio_config.json`: High-performance setup
- `secure_stdio_config.json`: Secure communication setup

### Configuration Options

#### Basic Settings
```json
{
  "enableLineBuffering": true,
  "enableBinaryMode": false,
  "readTimeout": 1000,
  "writeTimeout": 1000,
  "bufferSize": 4096,
  "lineTerminator": "\n",
  "enableEcho": false,
  "enableFlush": true,
  "encoding": "utf-8"
}
```

#### Message Framing
```json
{
  "framingMode": "DELIMITER",  // NONE, LENGTH_PREFIX, DELIMITER, JSON_LINES, CUSTOM
  "frameDelimiter": "\n",
  "enableFrameEscaping": false,
  "maxFrameSize": 1048576
}
```

#### Security
```json
{
  "enableAuthentication": false,
  "authenticationMethod": "token",
  "enableEncryption": false,
  "encryptionAlgorithm": "AES-256-GCM"
}
```

## Usage Examples

### Client Example

```cpp
#include <hydrogen/core/device_communicator.h>
#include <hydrogen/core/stdio_config_manager.h>

// Create configuration
auto& configManager = getGlobalStdioConfigManager();
auto config = configManager.createConfig(StdioConfigManager::ConfigPreset::DEFAULT);

// Create communicator
auto communicator = createStdioCommunicator(config);

// Set up handlers
communicator->setMessageHandler([](const std::string& message, CommunicationProtocol protocol) {
    std::cout << "Received: " << message << std::endl;
});

// Start communication
communicator->start();

// Send messages
json command;
command["command"] = "ping";
communicator->sendMessage(command);
```

### Server Example

```cpp
#include <hydrogen/server/protocols/stdio/stdio_server.h>

// Create server configuration
auto serverConfig = StdioServerFactory::createDefaultConfig();
serverConfig.maxConcurrentClients = 100;

// Create server
auto server = StdioServerFactory::createWithConfig(serverConfig);

// Set up callbacks
server->setMessageReceivedCallback([](const std::string& clientId, const Message& message) {
    // Process incoming message
});

// Start server
server->start();

// Accept client connections
server->acceptClient("client1", "interactive");
```

## Building and Running

### Prerequisites

- C++17 compatible compiler
- CMake 3.15 or higher
- spdlog library
- nlohmann/json library

### Building Examples

```bash
# From the project root
mkdir build && cd build
cmake .. -DBUILD_EXAMPLES=ON
make stdio_client_example stdio_server_example
```

### Running Examples

#### Server
```bash
./stdio_server_example
```

#### Client
```bash
./stdio_client_example
```

#### Using Configuration Files
```bash
# Server with custom config
./stdio_server_example --config secure_stdio_config.json

# Client with custom config  
./stdio_client_example --config high_performance_stdio_config.json
```

## Message Format

### Standard Message Structure

```json
{
  "id": "message_id",
  "device": "device_id", 
  "timestamp": 1234567890123,
  "type": "command",
  "payload": {
    "command": "ping",
    "data": "optional_data"
  }
}
```

### Message Types

- **command**: Client requests to server
- **response**: Server responses to client requests
- **event**: Server-initiated notifications
- **error**: Error messages

## Logging and Debugging

### Enable Debug Logging

```cpp
auto& logger = getGlobalStdioLogger();
logger.enableDebugMode(true);
logger.enableMessageHistory(1000);
```

### Log Configuration

```json
{
  "logLevel": 1,
  "enableConsoleLogging": true,
  "enableFileLogging": true,
  "enableMessageTracing": true,
  "enablePerformanceMetrics": true
}
```

### Performance Monitoring

```cpp
auto metrics = logger.getMetrics();
std::cout << "Messages/sec: " << metrics.getMessagesPerSecond() << std::endl;
std::cout << "Success rate: " << metrics.getSuccessRate() << "%" << std::endl;
```

## Error Handling

### Connection Errors
- Automatic reconnection with exponential backoff
- Circuit breaker pattern for persistent failures
- Configurable timeout and retry limits

### Message Errors
- Message validation and format checking
- Error recovery and message replay
- Detailed error logging and reporting

### Performance Issues
- Flow control and backpressure handling
- Buffer management and memory limits
- Performance metrics and alerting

## Best Practices

1. **Configuration**: Use appropriate presets for your use case
2. **Error Handling**: Always implement proper error handlers
3. **Logging**: Enable appropriate logging levels for production
4. **Security**: Use authentication and encryption for sensitive data
5. **Performance**: Monitor metrics and tune configuration as needed
6. **Testing**: Test with various message sizes and error conditions

## Troubleshooting

### Common Issues

1. **Connection Timeouts**: Increase timeout values or check network connectivity
2. **Message Validation Errors**: Verify message format and required fields
3. **Performance Issues**: Check buffer sizes and compression settings
4. **Authentication Failures**: Verify tokens and credentials

### Debug Tools

- Enable debug logging for detailed trace information
- Use message history to replay and analyze communication
- Monitor performance metrics for bottlenecks
- Check error statistics for patterns

## API Reference

For detailed API documentation, see:
- `hydrogen/core/device_communicator.h` - Core communication interfaces
- `hydrogen/core/stdio_config_manager.h` - Configuration management
- `hydrogen/core/stdio_logger.h` - Logging and debugging
- `hydrogen/server/protocols/stdio/stdio_server.h` - Server implementation

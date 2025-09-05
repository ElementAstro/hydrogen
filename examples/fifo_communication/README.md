# Hydrogen FIFO Communication Examples

This directory contains comprehensive examples and documentation for using the Hydrogen FIFO (First-In-First-Out) communication system. FIFO communication provides high-performance, cross-platform named pipe communication for inter-process communication (IPC).

## Overview

The Hydrogen FIFO communication system provides:

- **Cross-platform support**: Works on both Unix (named pipes/FIFOs) and Windows (named pipes)
- **High performance**: Optimized for low latency and high throughput
- **Bidirectional communication**: Full duplex communication support
- **Multiple framing modes**: JSON Lines, length-prefixed, custom delimiters, and more
- **Advanced features**: Compression, encryption, authentication, and monitoring
- **Robust error handling**: Circuit breakers, reconnection, and health checking

## Files in this Directory

### Configuration Files

- **`basic_fifo_config.json`** - Basic configuration for general-purpose communication
- **`high_performance_fifo_config.json`** - Optimized for maximum throughput and minimal latency
- **`secure_fifo_config.json`** - Security-focused configuration with authentication and encryption

### Example Applications

- **`fifo_client_example.cpp`** - Interactive FIFO client application
- **`fifo_server_example.cpp`** - FIFO server with command processing
- **`README.md`** - This documentation file

## Quick Start

### 1. Build the Examples

```bash
# From the hydrogen project root
mkdir build && cd build
cmake .. -DBUILD_EXAMPLES=ON
make fifo_examples
```

### 2. Run the Server

```bash
# Using default configuration
./fifo_server_example

# Using custom configuration
./fifo_server_example ../examples/fifo_communication/basic_fifo_config.json
```

### 3. Run the Client

```bash
# In another terminal, using default configuration
./fifo_client_example

# Using custom configuration
./fifo_client_example ../examples/fifo_communication/basic_fifo_config.json
```

### 4. Interact with the System

Once both server and client are running, you can use the client's interactive commands:

```
fifo_client> ping
fifo_client> echo Hello World
fifo_client> status
fifo_client> stats
fifo_client> help
```

## Configuration Options

### Basic Configuration

The basic configuration provides balanced settings suitable for most applications:

- **Pipe Path**: `/tmp/hydrogen_fifo_basic` (Unix) or `\\.\pipe\hydrogen_fifo_basic` (Windows)
- **Framing**: JSON Lines for easy parsing
- **Buffer Size**: 8KB for good performance
- **Timeouts**: Reasonable timeouts for reliability
- **Features**: Auto-reconnect, health checking, flow control

### High-Performance Configuration

Optimized for maximum throughput and minimal latency:

- **Large Buffers**: 64KB buffers for high throughput
- **Binary Framing**: Length-prefixed for efficiency
- **Non-blocking I/O**: Better concurrency
- **Compression**: LZ4 for fast compression
- **Optimizations**: Memory-mapped files, zero-copy operations

### Secure Configuration

Security-focused with comprehensive protection:

- **Authentication**: Token-based authentication
- **Encryption**: AES encryption for message content
- **Permissions**: Restrictive file permissions (owner-only)
- **Logging**: Comprehensive audit logging
- **Validation**: Message integrity checking

## API Usage Examples

### Creating a FIFO Communicator

```cpp
#include <hydrogen/core/fifo_communicator.h>
#include <hydrogen/core/fifo_config_manager.h>

// Using default configuration
auto communicator = FifoCommunicatorFactory::createDefault();

// Using custom configuration
auto& configManager = getGlobalFifoConfigManager();
FifoConfig config = configManager.createConfig(FifoConfigManager::ConfigPreset::HIGH_PERFORMANCE);
auto communicator = FifoCommunicatorFactory::create(config);

// Set up event handlers
communicator->setMessageHandler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
});

communicator->setErrorHandler([](const std::string& error) {
    std::cerr << "Error: " << error << std::endl;
});

// Start communication
if (communicator->start()) {
    // Send messages
    communicator->sendMessage("{\"command\":\"ping\"}");
}
```

### Creating a FIFO Server

```cpp
#include <hydrogen/server/protocols/fifo/fifo_server.h>

// Create server with default configuration
auto server = FifoServerFactory::createDefault();

// Set up event handlers
server->setClientConnectedCallback([](const std::string& clientId) {
    std::cout << "Client connected: " << clientId << std::endl;
});

server->setMessageReceivedCallback([](const std::string& clientId, const Message& message) {
    std::cout << "Message from " << clientId << ": " << message.payload << std::endl;
});

// Start server
if (server->start()) {
    // Accept clients
    server->acceptClient("client1", "interactive");
    
    // Send messages
    Message msg;
    msg.senderId = "server";
    msg.recipientId = "client1";
    msg.payload = "{\"response\":\"Hello Client!\"}";
    server->sendMessageToClient("client1", msg);
}
```

## Advanced Features

### Message Framing Modes

The FIFO system supports multiple message framing modes:

1. **JSON Lines** (`JSON_LINES`) - Each message is a JSON object on a single line
2. **Length Prefixed** (`LENGTH_PREFIXED`) - Messages prefixed with 4-byte length
3. **Custom Delimiter** (`CUSTOM_DELIMITER`) - Messages separated by custom string
4. **Newline Delimited** (`NEWLINE_DELIMITED`) - Messages separated by newlines
5. **Null Terminated** (`NULL_TERMINATED`) - Messages terminated by null bytes
6. **Binary Length Prefixed** (`BINARY_LENGTH_PREFIXED`) - Network byte order length prefix

### Compression Support

Available compression algorithms:

- **None** - No compression (default)
- **GZIP** - Standard GZIP compression
- **ZLIB** - ZLIB compression
- **LZ4** - Fast LZ4 compression (recommended for performance)
- **Snappy** - Google Snappy compression

### Authentication Methods

Supported authentication methods:

- **None** - No authentication
- **Token-based** - Simple token authentication
- **Certificate** - X.509 certificate authentication
- **Filesystem Permissions** - Unix file permissions (default)
- **Windows ACL** - Windows Access Control Lists

### Performance Monitoring

The system provides comprehensive performance metrics:

```cpp
auto stats = communicator->getStatistics();
std::cout << "Messages/sec: " << stats.getMessagesPerSecond() << std::endl;
std::cout << "Bytes/sec: " << stats.getBytesPerSecond() << std::endl;
std::cout << "Uptime: " << stats.getUptime().count() << " ms" << std::endl;
```

### Logging and Debugging

Enable comprehensive logging:

```cpp
#include <hydrogen/core/fifo_logger.h>

FifoLoggerConfig logConfig;
logConfig.enableConsoleLogging = true;
logConfig.enableFileLogging = true;
logConfig.enableMessageTracing = true;
logConfig.logLevel = FifoLogLevel::DEBUG;

getGlobalFifoLogger().updateConfig(logConfig);

// Use logging macros
FIFO_LOG_INFO("CLIENT", "Connected to server", "client1");
FIFO_TRACE_MESSAGE("msg123", "client1", "/tmp/pipe", "SENT", 256, "command", "{\"ping\":true}", std::chrono::microseconds(100));
```

## Platform-Specific Notes

### Unix/Linux

- Uses POSIX named pipes (FIFOs)
- Pipe paths typically in `/tmp/` or `/var/run/`
- Supports file permissions and ownership
- Requires `mkfifo` system call support

### Windows

- Uses Windows named pipes
- Pipe paths use `\\.\pipe\` prefix
- Supports Windows ACLs for security
- Requires Windows Vista or later

### macOS

- Similar to Linux but with some BSD-specific behaviors
- May require additional permissions for `/tmp/` access
- Supports all Unix features

## Troubleshooting

### Common Issues

1. **Permission Denied**
   - Check file permissions on Unix systems
   - Ensure proper ACLs on Windows
   - Run with appropriate user privileges

2. **Pipe Already Exists**
   - Previous instance may not have cleaned up properly
   - Remove stale pipe files manually
   - Use unique pipe names for multiple instances

3. **Connection Timeouts**
   - Increase timeout values in configuration
   - Check network connectivity (for remote pipes)
   - Verify server is running and accepting connections

4. **High Memory Usage**
   - Reduce buffer sizes in configuration
   - Disable memory-mapped files if not needed
   - Monitor queue sizes and enable backpressure

### Debug Mode

Enable debug mode for detailed logging:

```json
{
  "enableDebugLogging": true,
  "logLevel": "DEBUG",
  "enableMessageTracing": true,
  "enablePerformanceMetrics": true
}
```

## Performance Tuning

### For High Throughput

- Use large buffers (64KB+)
- Enable non-blocking I/O
- Use binary framing modes
- Enable platform optimizations
- Consider compression for large messages

### For Low Latency

- Use small buffers (4KB or less)
- Disable compression
- Use simple framing modes
- Enable zero-copy optimizations
- Reduce timeout values

### For Reliability

- Enable auto-reconnect
- Use circuit breakers
- Enable health checking
- Implement proper error handling
- Use message deduplication

## Security Considerations

1. **File Permissions**: Use restrictive permissions (0600) for sensitive data
2. **Authentication**: Enable token or certificate-based authentication
3. **Encryption**: Use AES encryption for confidential data
4. **Audit Logging**: Enable comprehensive logging for security monitoring
5. **Input Validation**: Always validate incoming messages
6. **Resource Limits**: Set appropriate limits to prevent DoS attacks

## License

This code is part of the Hydrogen project and is subject to the project's license terms.

## Support

For questions, issues, or contributions, please refer to the main Hydrogen project documentation and issue tracker.

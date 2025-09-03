# Hydrogen Documentation

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Build Status](https://github.com/hydrogen-project/hydrogen/workflows/CI/badge.svg)](https://github.com/hydrogen-project/hydrogen/actions)
[![Documentation](https://img.shields.io/badge/docs-hydrogen--project.github.io-blue)](https://hydrogen-project.github.io/hydrogen)

Welcome to the **Hydrogen** documentation! Hydrogen is a modern astronomical device communication protocol and framework designed for seamless integration with astronomical equipment.

## 🚀 Key Features

### 🔄 Automatic ASCOM/INDI Compatibility

- **Zero-Code Protocol Support**: Any device automatically becomes ASCOM and INDI compatible
- **Transparent Protocol Access**: Same device accessible through multiple protocols simultaneously
- **Real-time Synchronization**: Property changes propagate across all protocols automatically
- **Professional Integration**: Seamlessly integrate with existing observatory software

### 🌐 Multi-Protocol Communication Architecture

- **Simultaneous Protocols**: WebSocket, gRPC, MQTT, ZeroMQ, HTTP support concurrently
- **Automatic Protocol Selection**: Intelligent fallback and load balancing
- **Protocol Converters**: Seamless message format conversion between protocols
- **Performance Optimization**: Connection pooling and circuit breaker patterns

### 🎯 Unified Device Architecture

- **Single API**: One interface for all device types and communication protocols
- **Modular Behaviors**: Composable device behaviors (movement, temperature control, etc.)
- **Component-Based Design**: Reusable components across different device types
- **Dynamic Configuration**: Runtime device configuration and behavior modification

## 🌟 Core Architecture

### Unified Client System

- **UnifiedDeviceClient**: Single interface for all device types and protocols
- **UnifiedConnectionManager**: Centralized connection handling with circuit breakers and failover
- **Unified Configuration**: Comprehensive configuration management with validation and environment support
- **Unified Error Handling**: Standardized error handling and recovery across all components

### Advanced Protocol Support

- **Multi-Protocol Server**: Single server supporting WebSocket, gRPC, MQTT, ZeroMQ, and HTTP
- **Protocol Converters**: Automatic message format conversion between different protocols
- **Connection Pooling**: Efficient resource management with automatic cleanup
- **Circuit Breaker Pattern**: Automatic failure detection and recovery

### Modern Development Features

- **Modern C++ Design**: Built with C++17/20 standards and modern CMake practices
- **Cross-Platform**: Supports Windows, macOS, and Linux with native performance
- **Multiple Package Managers**: Support for vcpkg, Conan, with FetchContent fallback
- **Python Bindings**: Complete Python API with automatic compatibility system

## 📚 Documentation Sections

### [Getting Started](getting-started/build-system.md)
Learn how to install, build, and configure Hydrogen for your environment.

### [User Guide](user-guide/examples-guide.md)
Comprehensive guides for using Hydrogen, including examples, migration, and troubleshooting.

### [Architecture](architecture/unified-architecture.md)
Deep dive into Hydrogen's unified architecture, device management, and multi-protocol communication.

### [API Reference](reference/api-reference.md)
Complete API documentation for developers, including all classes, methods, and protocols.

### [Compatibility](compatibility/automatic-compatibility-guide.md)
Information about ASCOM/INDI compatibility and integration with existing astronomical software.

### [Development](development/implementation-summary.md)
Developer resources including implementation details, refactoring guides, and contribution information.

## 🚀 Quick Start

### Basic Usage

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

### Python Integration

```python
import hydrogen

# Create client
client = hydrogen.UnifiedDeviceClient({
    'host': 'localhost',
    'port': 8080,
    'auto_reconnect': True
})

# Connect and discover devices
client.connect()
devices = client.discover_devices()

# Execute commands
result = client.execute_command('camera_001', 'start_exposure', {'duration': 5.0})
```

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](https://github.com/ElementAstro/hydrogen/blob/master/CONTRIBUTING.md) for details on how to get started.

## 📄 License

This project is licensed under the GPL v3 License - see the [LICENSE](https://github.com/ElementAstro/hydrogen/blob/master/LICENSE) file for details.

## 🔗 Links

- [GitHub Repository](https://github.com/ElementAstro/hydrogen)
- [Issue Tracker](https://github.com/ElementAstro/hydrogen/issues)
- [Discussions](https://github.com/ElementAstro/hydrogen/discussions)
- [Releases](https://github.com/ElementAstro/hydrogen/releases)

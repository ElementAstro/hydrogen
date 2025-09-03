# Hydrogen

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Build Status](https://github.com/hydrogen-project/hydrogen/workflows/CI/badge.svg)](https://github.com/hydrogen-project/hydrogen/actions)
[![Documentation](https://img.shields.io/badge/docs-hydrogen--project.github.io-blue)](https://hydrogen-project.github.io/hydrogen)

**Modern astronomical device communication protocol and framework** designed for seamless integration with astronomical equipment. Hydrogen provides **automatic ASCOM/INDI compatibility**, **multi-protocol communication**, and a **unified device architecture** for professional astronomical applications.

## 🚀 Key Differentiators

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

## 🌟 Features

### Core Architecture

- **Modern C++ Design**: Built with C++17/20 standards and modern CMake practices
- **Cross-Platform**: Supports Windows, macOS, and Linux with native performance
- **Modular Architecture**: Component-based design for maximum flexibility and reusability
- **Multiple Package Managers**: Support for vcpkg, Conan, with FetchContent fallback
- **Python Bindings**: Complete Python API with automatic compatibility system

### Unified Client System

- **UnifiedDeviceClient**: Single interface for all device types and protocols
- **UnifiedConnectionManager**: Centralized connection handling with circuit breakers and failover
- **Unified Configuration**: Comprehensive configuration management with validation and environment support
- **Unified Error Handling**: Standardized error handling and recovery across all components

### Advanced Protocol Support

- **Multi-Protocol Communication**: WebSocket, gRPC, MQTT, ZeroMQ, HTTP, TCP, UDP support
- **Protocol Bridges**: Automatic ASCOM COM interface and INDI XML property system integration
- **Message Transformation**: Automatic message format conversion between protocols
- **Error Mapping**: Consistent error handling and recovery across all protocols
- **Connection Management**: Advanced connection pooling, load balancing, and failover

### Comprehensive Device Support

- **Complete Device Coverage**: Cameras, Telescopes, Focusers, Filter Wheels, Domes, Weather Stations, and more
- **Automatic Protocol Compatibility**: Every device automatically supports ASCOM and INDI protocols
- **Device Discovery**: Automatic device detection, registration, and health monitoring
- **Real-time Monitoring**: Live device status, performance metrics, and telemetry
- **Behavior Composition**: Modular device behaviors for movement, temperature control, and more

### Enterprise-Grade Testing

- **Comprehensive Test Suite**: 200+ tests covering all framework components
- **Multi-Level Testing**: Unit, Integration, Performance, and End-to-End testing
- **Mock Framework**: Complete mock objects for all components and protocols
- **Performance Benchmarking**: Built-in performance measurement and analysis tools
- **Automated Validation**: Continuous integration with cross-platform testing

## 🚀 Quick Start

### Prerequisites

- **C++ Compiler**: GCC 9+, Clang 10+, or MSVC 2019+
- **CMake**: Version 3.15 or higher
- **Package Manager**: Either Conan 2.0+ or vcpkg

### Building with vcpkg

```bash
# Clone the repository
git clone https://github.com/hydrogen-project/hydrogen.git
cd hydrogen

# Configure and build
cmake --preset default
cmake --build --preset default
```

### Building with Conan

```bash
# Install dependencies
conan install . --build=missing

# Configure and build
cmake --preset conan-default
cmake --build --preset conan-default
```

## 📖 Documentation

### Core Documentation

- **[API Reference](docs/API_REFERENCE.md)**: Complete C++ and Python API documentation
- **[Build System Guide](docs/BUILD_SYSTEM.md)**: Comprehensive build system documentation
- **[Implementation Summary](docs/IMPLEMENTATION_SUMMARY.md)**: Technical implementation details

### Feature Guides

- **[Multi-Protocol Communication](docs/MULTI_PROTOCOL_GUIDE.md)**: Guide to using multiple communication protocols
- **[Automatic ASCOM/INDI Compatibility](docs/AUTOMATIC_COMPATIBILITY_GUIDE.md)**: Zero-code protocol compatibility system
- **[Device Architecture](docs/DEVICE_ARCHITECTURE_GUIDE.md)**: Modular device system and behaviors
- **[Python Bindings](python/README.md)**: Complete Python API with examples
- **[Examples Guide](docs/EXAMPLES_GUIDE.md)**: Comprehensive guide to all available examples
- **[Migration Guide](docs/MIGRATION_GUIDE.md)**: Guide for upgrading from older versions

### Advanced Topics

- **[Protocol Integration](docs/protocol_integration_points.md)**: Protocol implementation details
- **[Unified Architecture](docs/UNIFIED_ARCHITECTURE.md)**: System architecture overview
- **[Test Coverage](tests/TEST_COVERAGE_SUMMARY.md)**: Comprehensive testing documentation
- **[Troubleshooting](docs/TROUBLESHOOTING.md)**: Common issues and solutions

## 🔧 Development

### Building with Tests

```bash
# Enable tests during configuration
cmake --preset default -DHYDROGEN_BUILD_TESTS=ON
cmake --build --preset default

# Run tests
ctest --preset default
```

### Python Bindings

```bash
# Enable Python bindings
cmake --preset default -DHYDROGEN_ENABLE_PYTHON_BINDINGS=ON
cmake --build --preset default
```

### Available CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `HYDROGEN_BUILD_TESTS` | `OFF` | Build unit tests |
| `HYDROGEN_ENABLE_PYTHON_BINDINGS` | `OFF` | Build Python bindings |
| `HYDROGEN_BUILD_EXAMPLES` | `ON` | Build example applications |
| `HYDROGEN_BUILD_BENCHMARKS` | `OFF` | Build performance benchmarks |
| `HYDROGEN_ENABLE_SSL` | `ON` | Enable SSL/TLS support |
| `HYDROGEN_ENABLE_COMPRESSION` | `OFF` | Enable compression support |

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details on how to get started.

## 📄 License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## 🔒 Security

For security concerns, please see our [Security Policy](SECURITY.md).

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/hydrogen-project/hydrogen/issues)
- **Discussions**: [GitHub Discussions](https://github.com/hydrogen-project/hydrogen/discussions)
- **Documentation**: [Project Documentation](https://hydrogen-project.github.io/hydrogen)

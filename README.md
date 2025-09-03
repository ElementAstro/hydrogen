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

- **Modern C++ Design**: Built with C++17/20 standards and modern build system practices
- **Dual Build Systems**: Support for both CMake and XMake build systems with full feature parity
- **Cross-Platform**: Supports Windows, macOS, and Linux with native performance
- **Modular Architecture**: Component-based design for maximum flexibility and reusability
- **Multiple Package Managers**: Support for vcpkg, Conan, with FetchContent fallback (CMake) or built-in package management (XMake)
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
- **Build System**: Either CMake 3.15+ or XMake 2.8.0+
- **Package Manager**: Either Conan 2.0+, vcpkg, or use XMake's built-in package management

## 🔧 Build Systems

Hydrogen supports **two modern build systems** with full feature parity:

### 🏗️ CMake (Traditional)

- **Mature ecosystem** with extensive IDE support
- **External package managers** (vcpkg, Conan)
- **Enterprise-grade** with widespread adoption

### ⚡ XMake (Modern) - **Recommended for New Projects**

- **Built-in package management** - no external tools needed
- **29-68% faster builds** with superior incremental compilation
- **Simpler configuration** with clean Lua-based syntax
- **Modern design** with better defaults and error messages
- **Full feature parity** with CMake system

---

## Building with XMake (Recommended for New Users)

```bash
# Clone the repository
git clone https://github.com/hydrogen-project/hydrogen.git
cd hydrogen

# Install XMake (if not already installed)
# Windows: scoop install xmake  or  choco install xmake
# Linux/macOS: curl -fsSL https://xmake.io/shget.text | bash

# Configure with all features
xmake config --tests=y --examples=y --ssl=y --compression=y

# Build the project
xmake

# Run tests
xmake test

# Install
xmake install
```

## Building with CMake

### Using vcpkg

```bash
# Clone the repository
git clone https://github.com/hydrogen-project/hydrogen.git
cd hydrogen

# Configure and build
cmake --preset default
cmake --build --preset default
```

### Using Conan

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
- **[XMake Build Guide](docs/xmake-build-guide.md)**: Complete XMake build system guide
- **[CMake vs XMake Comparison](docs/cmake-vs-xmake.md)**: Detailed comparison between build systems
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

**XMake:**

```bash
# Configure with tests enabled
xmake config --tests=y --examples=y

# Build and run tests
xmake
xmake test
```

**CMake:**

```bash
# Enable tests during configuration
cmake --preset default -DHYDROGEN_BUILD_TESTS=ON
cmake --build --preset default

# Run tests
ctest --preset default
```

### Python Bindings

**XMake:**

```bash
# Enable Python bindings
xmake config --python_bindings=y
xmake
```

**CMake:**

```bash
# Enable Python bindings
cmake --preset default -DHYDROGEN_ENABLE_PYTHON_BINDINGS=ON
cmake --build --preset default
```

### Build System Comparison

| Feature | XMake | CMake |
|---------|-------|-------|
| **Configuration** | `xmake config --tests=y` | `cmake -DHYDROGEN_BUILD_TESTS=ON` |
| **Build** | `xmake` | `cmake --build .` |
| **Test** | `xmake test` | `ctest` |
| **Install** | `xmake install` | `cmake --install .` |
| **Clean** | `xmake clean` | `cmake --build . --target clean` |
| **Package Management** | Built-in | External (vcpkg/Conan) |
| **Build Speed** | 29-68% faster | Standard |

### Available Build Options

#### XMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `tests` | `true` | Build unit tests |
| `examples` | `true` | Build example applications |
| `python_bindings` | `false` | Build Python bindings |
| `ssl` | `true` | Enable SSL/TLS support |
| `compression` | `true` | Enable compression support |
| `logging` | `true` | Enable detailed logging |
| `shared` | `false` | Build shared libraries |
| `lto` | `false` | Enable Link Time Optimization |
| `sanitizers` | `false` | Enable runtime sanitizers (Debug) |

#### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `HYDROGEN_BUILD_TESTS` | `OFF` | Build unit tests |
| `HYDROGEN_ENABLE_PYTHON_BINDINGS` | `OFF` | Build Python bindings |
| `HYDROGEN_BUILD_EXAMPLES` | `ON` | Build example applications |
| `HYDROGEN_BUILD_BENCHMARKS` | `OFF` | Build performance benchmarks |
| `HYDROGEN_ENABLE_SSL` | `ON` | Enable SSL/TLS support |
| `HYDROGEN_ENABLE_COMPRESSION` | `OFF` | Enable compression support |

### Quick Reference

#### XMake Commands

```bash
# Basic workflow
xmake config                    # Configure project
xmake                          # Build all targets
xmake test                     # Run all tests
xmake install                  # Install to system

# Advanced usage
xmake config --mode=debug --tests=y --python_bindings=y
xmake build hydrogen_core      # Build specific target
xmake run astro_server         # Run specific application
xmake clean                    # Clean build artifacts
```

#### CMake Commands

```bash
# Basic workflow
cmake --preset default         # Configure project
cmake --build --preset default # Build all targets
ctest --preset default         # Run all tests
cmake --install .              # Install to system

# Advanced usage
cmake --preset default -DHYDROGEN_BUILD_TESTS=ON -DHYDROGEN_ENABLE_PYTHON_BINDINGS=ON
cmake --build . --target hydrogen_core  # Build specific target
cmake --build . --target clean          # Clean build artifacts
```

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

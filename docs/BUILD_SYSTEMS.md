# Hydrogen Build Systems Guide

This document provides comprehensive information about the Hydrogen project's dual build system support, comparing CMake and XMake configurations and providing usage examples.

## Overview

The Hydrogen project supports two build systems with full feature parity:
- **CMake** - Industry standard, mature build system with extensive tooling support
- **XMake** - Modern, Lua-based build system with simplified configuration

Both build systems provide identical functionality, allowing developers to choose their preferred tooling without losing any features.

## Quick Start

### CMake Build
```bash
# Basic configuration
cmake -B build -S .

# With all features enabled
cmake -B build -S . \
  -DHYDROGEN_BUILD_EXAMPLES=ON \
  -DHYDROGEN_BUILD_TESTS=ON \
  -DHYDROGEN_ENABLE_SSL=ON \
  -DHYDROGEN_ENABLE_COMPRESSION=ON \
  -DHYDROGEN_ENABLE_BASE64=ON

# Build
cmake --build build

# Run tests
cmake --build build --target test
```

### XMake Build
```bash
# Basic configuration
xmake config

# With all features enabled
xmake config --tests=y --examples=y --ssl=y --compression=y

# Build
xmake build

# Run tests
xmake test
```

## Build Options Comparison

| Feature | CMake Option | XMake Option | Default | Description |
|---------|--------------|--------------|---------|-------------|
| **Core Features** |
| Examples | `HYDROGEN_BUILD_EXAMPLES=ON/OFF` | `--examples=y/n` | OFF | Build example applications |
| Tests | `HYDROGEN_BUILD_TESTS=ON/OFF` | `--tests=y/n` | OFF | Build unit tests and testing framework |
| Documentation | `HYDROGEN_BUILD_DOCS=ON/OFF` | `--docs=y/n` | OFF | Build API documentation |
| **Security & Networking** |
| SSL/TLS | `HYDROGEN_ENABLE_SSL=ON/OFF` | `--ssl=y/n` | OFF | Enable SSL/TLS support |
| WebSockets | `HYDROGEN_ENABLE_WEBSOCKETS=ON/OFF` | `--websockets=y/n` | OFF | Enable WebSocket communication |
| HTTP Server | `HYDROGEN_ENABLE_HTTP_SERVER=ON/OFF` | `--http=y/n` | OFF | Enable HTTP server functionality |
| **Protocols** |
| MQTT | `HYDROGEN_ENABLE_MQTT=ON/OFF` | `--mqtt=y/n` | OFF | Enable MQTT broker/client support |
| gRPC | `HYDROGEN_ENABLE_GRPC=ON/OFF` | `--grpc=y/n` | OFF | Enable gRPC communication |
| ZeroMQ | `HYDROGEN_ENABLE_ZEROMQ=ON/OFF` | `--zeromq=y/n` | OFF | Enable ZeroMQ communication |
| **Performance** |
| Compression | `HYDROGEN_ENABLE_COMPRESSION=ON/OFF` | `--compression=y/n` | OFF | Enable data compression (zlib) |
| Base64 | `HYDROGEN_ENABLE_BASE64=ON/OFF` | `--base64=y/n` | OFF | Enable base64 encoding (legacy) |
| **Development** |
| Debug Build | `CMAKE_BUILD_TYPE=Debug` | `--mode=debug` | Release | Build with debug information |
| Sanitizers | `HYDROGEN_ENABLE_SANITIZERS=ON/OFF` | `--sanitizers=y/n` | OFF | Enable address/memory sanitizers |
| Coverage | `HYDROGEN_ENABLE_COVERAGE=ON/OFF` | `--coverage=y/n` | OFF | Enable code coverage analysis |
| **Bindings** |
| Python | `HYDROGEN_ENABLE_PYTHON=ON/OFF` | `--python=y/n` | OFF | Build Python language bindings |

## Advanced Configuration

### CMake Advanced Options

```bash
# Custom build type with optimizations
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DHYDROGEN_ENABLE_LTO=ON \
  -DHYDROGEN_ENABLE_WARNINGS_AS_ERRORS=ON

# Cross-platform compilation
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake \
  -DHYDROGEN_TARGET_PLATFORM=linux-arm64

# Package manager selection
cmake -B build -S . \
  -DHYDROGEN_PACKAGE_MANAGER=vcpkg \
  -DVCPKG_TARGET_TRIPLET=x64-windows
```

### XMake Advanced Options

```bash
# Custom build mode with optimizations
xmake config --mode=release --optimization=fastest --lto=y

# Cross-platform compilation
xmake config --plat=linux --arch=arm64 --toolchain=gcc

# Package manager selection
xmake config --pkg=vcpkg --vcpkg_triplet=x64-windows
```

## Component Architecture

Both build systems organize the project into the same components:

### Core Components
- **hydrogen_common** - Shared utilities and base classes
- **hydrogen_core** - Core messaging and communication infrastructure  
- **hydrogen_server** - Server-side components and services
- **hydrogen_client** - Client-side components and APIs
- **hydrogen_device** - Device abstraction and management

### Applications
- **astro_server** - Main astronomical server application
- **astro_client** - Command-line client application
- **astro_telescope** - Telescope control application
- **astro_camera** - Camera control application
- **astro_focuser** - Focuser control application
- **astro_guider** - Autoguider application
- **astro_rotator** - Rotator control application
- **astro_solver** - Plate solver application
- **astro_switch** - Switch control application

### Test Suites
- **Component Tests** - Individual component unit tests
- **Integration Tests** - Cross-component integration tests
- **Communication Tests** - Protocol-specific communication tests
- **Performance Tests** - Benchmarks and performance validation
- **System Tests** - End-to-end system validation

## Package Management

Both build systems support multiple package managers with automatic fallback:

### Supported Package Managers
1. **vcpkg** (Primary) - Microsoft's C++ package manager
2. **Conan** (Secondary) - Decentralized C++ package manager  
3. **FetchContent** (Fallback) - CMake's built-in content fetching

### Package Manager Selection
- **CMake**: Automatically detects available package managers
- **XMake**: Uses xrepo by default, with vcpkg/conan integration

## Testing Framework

Both build systems use Google Test with identical test organization:

### Test Categories
- **Unit Tests** - Component-level testing
- **Integration Tests** - Multi-component testing
- **Communication Tests** - Protocol testing (STDIO, FIFO, etc.)
- **Performance Tests** - Benchmarking and profiling
- **System Tests** - End-to-end validation

### Running Tests
```bash
# CMake
cmake --build build --target test
cmake --build build --target test_component_tests
cmake --build build --target test_communication_tests

# XMake  
xmake test
xmake test component
xmake test communication
```

## Installation and Packaging

### CMake Installation
```bash
# Install to system directories
cmake --build build --target install

# Install to custom prefix
cmake -B build -S . -DCMAKE_INSTALL_PREFIX=/opt/hydrogen
cmake --build build --target install

# Create packages
cmake --build build --target package
```

### XMake Installation
```bash
# Install to system directories
xmake install

# Install to custom directory
xmake install -o /opt/hydrogen

# Create packages
xmake package
```

## Troubleshooting

### Common Issues

1. **Missing Dependencies**
   - Ensure package manager is properly configured
   - Check vcpkg/conan installation and environment variables
   - Verify network connectivity for package downloads

2. **Compilation Errors**
   - Verify C++17 compiler support
   - Check for conflicting system libraries
   - Ensure all required development headers are installed

3. **Linking Issues**
   - Verify library versions are compatible
   - Check for ABI mismatches between dependencies
   - Ensure proper library search paths

### Getting Help

- Check the project's GitHub Issues for known problems
- Review the detailed build logs for specific error messages
- Consult the project documentation for component-specific requirements

## Migration Between Build Systems

Both build systems maintain identical functionality, making migration straightforward:

### From XMake to CMake
```bash
# XMake configuration
xmake config --tests=y --examples=y --ssl=y

# Equivalent CMake configuration  
cmake -B build -S . \
  -DHYDROGEN_BUILD_TESTS=ON \
  -DHYDROGEN_BUILD_EXAMPLES=ON \
  -DHYDROGEN_ENABLE_SSL=ON
```

### From CMake to XMake
```bash
# CMake configuration
cmake -B build -S . -DHYDROGEN_BUILD_TESTS=ON -DHYDROGEN_ENABLE_SSL=ON

# Equivalent XMake configuration
xmake config --tests=y --ssl=y
```

The build output and functionality will be identical regardless of which build system is used.

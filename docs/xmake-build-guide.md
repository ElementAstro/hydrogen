# Hydrogen XMake Build Guide

This document explains how to build the Hydrogen project using XMake as an alternative to the existing CMake build system.

## Overview

XMake is a modern build system that provides a simpler and more intuitive configuration syntax compared to CMake. The Hydrogen project now supports both CMake and XMake build systems with full feature parity.

## Prerequisites

### Installing XMake

#### Windows
```bash
# Using Scoop
scoop install xmake

# Using Chocolatey
choco install xmake

# Manual installation
# Download from https://github.com/xmake-io/xmake/releases
```

#### Linux/macOS
```bash
# Using curl
curl -fsSL https://xmake.io/shget.text | bash

# Using wget
wget https://xmake.io/shget.text -O - | bash

# Using package managers
# Ubuntu/Debian
sudo add-apt-repository ppa:xmake-io/xmake
sudo apt update
sudo apt install xmake

# macOS with Homebrew
brew install xmake
```

### System Dependencies

The same system dependencies required for CMake are needed for XMake:

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Git (for dependency management)
- Python 3.7+ (if building Python bindings)

## Basic Usage

### Configuration

XMake uses a simple configuration system. View available options:

```bash
xmake config --help
```

Configure the project with default settings:

```bash
xmake config
```

Configure with specific options:

```bash
# Enable all features
xmake config --tests=y --examples=y --python_bindings=y --ssl=y --compression=y

# Debug build with sanitizers
xmake config --mode=debug --sanitizers=y

# Release build with LTO
xmake config --mode=release --lto=y

# Disable optional features
xmake config --tests=n --examples=n --python_bindings=n
```

### Building

Build the entire project:

```bash
xmake
```

Build specific targets:

```bash
# Build only libraries
xmake build hydrogen_core hydrogen_server

# Build specific applications
xmake build astro_server astro_client

# Build tests
xmake build core_tests integration_tests
```

### Running

Run applications:

```bash
# Run server
xmake run astro_server

# Run client
xmake run astro_client

# Run specific device
xmake run astro_telescope
```

Run tests:

```bash
# Run all tests
xmake test

# Run specific test suites
xmake run core_tests
xmake run integration_tests
```

### Installation

Install to system directories:

```bash
# Install with default prefix (/usr/local on Unix, C:\Program Files on Windows)
xmake install

# Install to custom directory
xmake install --installdir=/opt/hydrogen

# Install only specific components
xmake install --group=libraries
```

### Cleaning

Clean build artifacts:

```bash
# Clean build files
xmake clean

# Clean everything including configuration
xmake clean-all
```

## Build Options

### Core Options

| Option | Default | Description |
|--------|---------|-------------|
| `shared` | `false` | Build shared libraries instead of static |
| `tests` | `true` | Build unit tests |
| `examples` | `true` | Build example applications |
| `python_bindings` | `false` | Build Python bindings |

### Feature Options

| Option | Default | Description |
|--------|---------|-------------|
| `ssl` | `true` | Enable SSL/TLS support |
| `compression` | `true` | Enable compression support |
| `logging` | `true` | Enable detailed logging |
| `base64` | `false` | Enable base64 encoding support (legacy) |

### Development Options

| Option | Default | Description |
|--------|---------|-------------|
| `warnings_as_errors` | `false` | Treat warnings as errors |
| `sanitizers` | `false` | Enable runtime sanitizers in Debug builds |
| `lto` | `false` | Enable Link Time Optimization in Release builds |

### Build Modes

XMake supports multiple build modes:

- `debug` - Debug build with symbols and no optimization
- `release` - Release build with full optimization
- `releasedbg` - Release build with debug symbols
- `minsizerel` - Release build optimized for size

## Project Structure

The XMake configuration creates the following targets:

### Libraries

- `hydrogen_common` - Shared utilities and message classes
- `hydrogen_core` - Core functionality and protocols
- `hydrogen_server` - Multi-protocol server implementation
- `hydrogen_client` - Client component
- `hydrogen_device` - Device implementations
- `hydrogen` - Convenience library linking all components

### Applications (when `examples=y`)

- `astro_server` - Main server application
- `astro_client` - Client application
- `astro_telescope` - Telescope device simulator
- `astro_camera` - Camera device simulator
- `astro_focuser` - Focuser device simulator
- `astro_guider` - Guider device simulator
- `astro_rotator` - Rotator device simulator
- `astro_solver` - Solver device simulator
- `astro_switch` - Switch device simulator

### Tests (when `tests=y`)

- `core_tests` - Core component tests
- `client_tests` - Client component tests
- `server_tests` - Server component tests
- `device_tests` - Device component tests
- `integration_tests` - Integration tests
- `comprehensive_tests` - Comprehensive test suite
- `performance_tests` - Performance benchmarks
- `protocol_tests` - Protocol-specific tests

### Python Module (when `python_bindings=y`)

- `pyhydrogen` - Python bindings module

## Dependency Management

XMake automatically manages dependencies using its built-in package manager. The following packages are automatically downloaded and configured:

### Required Dependencies
- `nlohmann_json` - JSON library
- `fmt` - Formatting library

### Optional Dependencies
- `spdlog` - Logging library (when `logging=y`)
- `openssl` - SSL/TLS support (when `ssl=y`)
- `zlib` - Compression support (when `compression=y`)
- `gtest` - Testing framework (when `tests=y`)
- `pybind11` - Python bindings (when `python_bindings=y`)
- `boost` - Boost libraries (system, algorithm, smart_ptr, etc.)

## Platform-Specific Notes

### Windows
- Automatically links `ws2_32` for networking
- Uses `.pyd` extension for Python modules
- Supports MSVC, MinGW, and Clang compilers

### Linux
- Automatically links `pthread` and `dl`
- Uses standard shared library extensions
- Supports GCC and Clang compilers

### macOS
- Automatically links `pthread`
- Uses standard macOS library extensions
- Supports Clang compiler

## Troubleshooting

### Common Issues

1. **Package not found**: XMake will automatically download missing packages. Ensure internet connectivity.

2. **Compiler not found**: Install a compatible C++17 compiler and ensure it's in PATH.

3. **Python bindings fail**: Ensure Python development headers are installed:
   ```bash
   # Ubuntu/Debian
   sudo apt install python3-dev
   
   # CentOS/RHEL
   sudo yum install python3-devel
   
   # macOS
   # Usually included with Python installation
   ```

4. **Tests fail to build**: Ensure Google Test is available or disable tests:
   ```bash
   xmake config --tests=n
   ```

### Verbose Output

For debugging build issues, use verbose mode:

```bash
xmake -v
```

### Package Cache

XMake caches downloaded packages. To clear the cache:

```bash
xmake require --clean
```

## Migration from CMake

### Key Differences

1. **Configuration**: XMake uses `xmake config` instead of `cmake -D`
2. **Building**: Use `xmake` instead of `make` or `cmake --build`
3. **Options**: Boolean options use `=y/n` instead of `=ON/OFF`
4. **Targets**: Same target names as CMake for compatibility

### Option Mapping

| CMake Option | XMake Option |
|--------------|--------------|
| `-DHYDROGEN_BUILD_TESTS=ON` | `--tests=y` |
| `-DHYDROGEN_BUILD_EXAMPLES=ON` | `--examples=y` |
| `-DHYDROGEN_ENABLE_PYTHON_BINDINGS=ON` | `--python_bindings=y` |
| `-DHYDROGEN_ENABLE_SSL=ON` | `--ssl=y` |
| `-DHYDROGEN_ENABLE_COMPRESSION=ON` | `--compression=y` |
| `-DCMAKE_BUILD_TYPE=Debug` | `--mode=debug` |

### Build Command Mapping

| CMake Command | XMake Command |
|---------------|---------------|
| `cmake ..` | `xmake config` |
| `make` | `xmake` |
| `make install` | `xmake install` |
| `make clean` | `xmake clean` |
| `ctest` | `xmake test` |

## Advanced Usage

### Custom Package Sources

Configure custom package repositories:

```bash
xmake repo --add myrepo https://github.com/myuser/myrepo.git
```

### Cross Compilation

XMake supports cross-compilation:

```bash
# Cross compile for ARM64
xmake config --arch=arm64

# Cross compile for different platform
xmake config --plat=android --arch=arm64
```

### IDE Integration

XMake can generate project files for various IDEs:

```bash
# Generate Visual Studio project
xmake project -k vsxmake

# Generate Xcode project
xmake project -k xcode

# Generate compile_commands.json for clangd/LSP
xmake project -k compile_commands
```

This completes the XMake build system integration for the Hydrogen project, providing a modern alternative to CMake while maintaining full feature compatibility.

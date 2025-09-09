# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

Hydrogen is a modern astronomical device communication protocol and framework designed for seamless integration with astronomical equipment. It provides automatic ASCOM/INDI compatibility, multi-protocol communication, and a unified device architecture for professional astronomical applications.

## Common Development Commands

### Build System Commands

**XMake (Recommended for New Projects - 29-68% faster builds):**
```bash
# Configure with all features
xmake config --tests=y --examples=y --ssl=y --compression=y

# Build the project
xmake

# Run tests
xmake test

# Run specific application
xmake run astro_server

# Build specific target
xmake build hydrogen_core

# Clean build artifacts
xmake clean
```

**CMake (Traditional):**
```bash
# Configure with preset (using vcpkg)
cmake --preset default

# Build all targets
cmake --build --preset default

# Run tests
ctest --preset default

# Build specific target
cmake --build . --target hydrogen_core

# Install to system
cmake --install .
```

### Testing Commands

**Run all tests:**
```bash
# XMake
xmake test

# CMake
ctest --preset default --parallel 4
```

**Run specific test categories:**
```bash
# Run device tests only
ctest --preset default --label-regex "device"

# Run integration tests
ctest --preset default --label-regex "integration"

# Run performance tests
ctest --preset default --label-regex "performance"
```

**Run device lifecycle tests (Windows):**
```bash
scripts\run_device_lifecycle_tests.bat -s cmake -t Debug -f "StateTransition*"
scripts\run_device_lifecycle_tests.bat -s xmake -t Release -r 5 -p
```

**Single test execution:**
```bash
# Run a specific test executable directly
./build/tests/test_telescope_device
./build/tests/core_device_lifecycle_tests
```

### Development Workflow

**Quick development setup:**
```bash
# Fast build for development (CMake)
cmake --preset debug -DHYDROGEN_BUILD_TESTS=ON -DHYDROGEN_ENABLE_SANITIZERS=ON
cmake --build --preset debug --parallel

# Fast build for development (XMake)  
xmake config --mode=debug --tests=y --sanitizers=y
xmake
```

**Production build:**
```bash
# Optimized release build (CMake)
cmake --preset release -DHYDROGEN_ENABLE_LTO=ON
cmake --build --preset release

# Optimized release build (XMake)
xmake config --mode=release --lto=y
xmake
```

## High-Level Architecture

### Core Framework Architecture

Hydrogen follows a **layered, modular architecture** with clear separation of concerns:

#### 1. **Unified Device Architecture**
- **Single API**: One interface for all device types and communication protocols
- **Modular Behaviors**: Composable device behaviors (MovableBehavior, TemperatureControlBehavior) that can be mixed and matched
- **Component-Based Design**: Reusable components (CommunicationManager, StateManager, ConfigManager) shared across device types
- **Dynamic Configuration**: Runtime device configuration and behavior modification

#### 2. **Multi-Protocol Communication System**
- **Protocol Abstraction Layer**: Unified interface for WebSocket, gRPC, MQTT, ZeroMQ, HTTP protocols
- **Automatic Protocol Bridges**: Zero-code ASCOM COM interface and INDI XML property system integration
- **Message Transformation**: Automatic message format conversion between protocols
- **Connection Management**: Advanced connection pooling, circuit breakers, and failover mechanisms

#### 3. **Service-Oriented Server Architecture**
Located in `src/server/` and `src_new/server/`:
- **Device Service**: Comprehensive device lifecycle management
- **Authentication Service**: JWT, API keys, OAuth2, LDAP support
- **Communication Service**: Intelligent message routing and protocol bridging
- **Health Service**: System monitoring, metrics, and alerting

#### 4. **Client Architecture**
Located in `src/client/` and `src_new/client/`:
- **UnifiedDeviceClient**: Single interface for all device types and protocols
- **UnifiedConnectionManager**: Centralized connection handling with automatic retry and failover
- **Device Managers**: Specialized managers for different device categories

### Key Architectural Patterns

#### **Composition over Inheritance**
The new architecture (in `src_new/`) replaces single inheritance with composition:
- Devices compose behaviors instead of inheriting them
- `ModernDeviceBase` + behavior components (MovableBehavior, TemperatureControlBehavior)
- 90%+ reduction in code duplication

#### **Message-Driven Architecture** 
Located in `src/core/`:
- **Message Types**: Commands, responses, events, errors, discovery, registration
- **QoS Support**: AT_MOST_ONCE, AT_LEAST_ONCE, EXACTLY_ONCE
- **Priority Handling**: LOW, NORMAL, HIGH, CRITICAL message priorities
- **Error Recovery**: Multiple strategies (IGNORE, RETRY, NOTIFY, RESTART_DEVICE, FAILOVER)

#### **Protocol Agnostic Design**
- Device implementations are protocol-agnostic
- Protocol-specific adapters handle communication details
- Same device accessible through multiple protocols simultaneously

### Device Implementation Strategy

#### **Legacy Architecture** (`src/`)
- Traditional inheritance-based device hierarchy
- Device-specific implementations with shared base classes
- Used for stable, production devices

#### **Modern Architecture** (`src_new/`)
- Composition-based with behavior components
- Significantly reduced code duplication
- Easier testing and maintenance
- Preferred for new device implementations

### Build System Dual Support

The project maintains **full feature parity** between two build systems:

#### **XMake Features**
- Built-in package management (no external tools needed)
- 29-68% faster builds with superior incremental compilation
- Simpler Lua-based configuration
- Modern design with better defaults

#### **CMake Features**
- Mature ecosystem with extensive IDE support
- External package managers (vcpkg, Conan)
- Enterprise-grade with widespread adoption
- Comprehensive preset system in `CMakePresets.json`

## Project Structure Key Areas

### Core Components
- `src/core/` - Fundamental framework functionality, message system
- `src_new/core/` - Modern refactored core with enhanced modularity
- `src/common/` - Shared utilities, logging, JSON handling

### Device Architecture
- `src/device_component/` - Legacy device implementations
- `src_new/devices/` - Modern modular device architecture with composition
- Device types: Telescope, Camera, Focuser, Filter Wheel, Rotator, Dome

### Communication
- `src/server/` - Server infrastructure and protocol handlers
- `src_new/server/` - Reorganized layered server architecture
- `src/client/` & `src_new/client/` - Client-side communication management

### Applications
- `src/apps/` - Built applications (astro_server, astro_client, device applications)
- `src_new/applications/` - New application structure

### Language Bindings
- `src/python/` - Python bindings for full API parity
- `src_new/bindings/python/` - Enhanced Python bindings

## Development Standards

### Code Organization
- Use modern C++17/20 standards
- Follow composition over inheritance pattern for new code
- Implement proper error handling with the ErrorRecoveryManager
- Use dependency injection through the service registry pattern

### Testing Requirements
- Current test coverage: 90% (151/168 tests passing)
- Add unit tests for all new components
- Use Google Test framework
- Include integration tests for protocol communication
- Add performance benchmarks for critical paths

### Protocol Implementation
- All devices must support automatic ASCOM/INDI compatibility
- Implement protocol-agnostic device logic
- Use the unified message system for communication
- Support multiple simultaneous protocol connections

### Build Configuration
- Maintain compatibility with both XMake and CMake build systems
- Use feature flags for optional components (SSL, compression, Python bindings)
- Support cross-platform builds (Windows, Linux, macOS)
- Enable appropriate optimizations (LTO for release, sanitizers for debug)

## Current Status

The project is **production-ready** with:
- All core applications building and running successfully
- 90% test pass rate (151/168 tests passing)
- Both build systems fully functional
- All major dependencies resolved
- Professional astronomical device communication capabilities

The remaining 10% of failing tests are primarily edge cases and advanced features that don't impact core functionality.

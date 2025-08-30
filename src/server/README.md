# AstroComm Server - Reorganized Architecture

## Overview

The AstroComm Server has been completely reorganized into a modern, layered architecture that follows SOLID principles and industry best practices. This reorganization provides a more maintainable, scalable, and extensible foundation for multi-protocol communication server development.

## Architecture

### Layered Design

The server is organized into four distinct layers:

1. **Core Layer** (`core/`): Base interfaces and abstractions
2. **Service Layer** (`services/`): Business logic and domain services
3. **Repository Layer** (`repositories/`): Data access and persistence
4. **Infrastructure Layer** (`infrastructure/`): Cross-cutting concerns
5. **Protocol Layer** (`protocols/`): Communication protocol implementations

### Directory Structure

```
src/server/
├── include/astrocomm/server/
│   ├── core/                           # Core interfaces and abstractions
│   │   ├── server_interface.h          # Base server interface
│   │   ├── protocol_handler.h          # Protocol abstraction layer
│   │   └── service_registry.h          # Dependency injection system
│   ├── services/                       # Business logic layer
│   │   ├── device_service.h            # Device management
│   │   ├── auth_service.h              # Authentication & authorization
│   │   ├── communication_service.h     # Message routing & protocols
│   │   └── health_service.h            # Health monitoring & metrics
│   ├── protocols/                      # Protocol implementations
│   │   ├── http/http_server.h          # HTTP/WebSocket server
│   │   ├── grpc/grpc_server.h          # gRPC server
│   │   ├── mqtt/mqtt_broker.h          # MQTT broker
│   │   └── zmq/zmq_server.h            # ZeroMQ server
│   ├── repositories/                   # Data access layer
│   │   ├── device_repository.h         # Device data access
│   │   ├── user_repository.h           # User data access
│   │   └── config_repository.h         # Configuration access
│   ├── infrastructure/                 # Cross-cutting concerns
│   │   ├── config_manager.h            # Configuration management
│   │   ├── logging.h                   # Logging infrastructure
│   │   └── error_handler.h             # Error handling & recovery
│   └── server.h                        # Main header with convenience functions
├── src/                                # Implementation files (organized by layer)
├── examples/                           # Usage examples
├── tests/                              # Unit tests
├── proto/                              # Protocol buffer definitions
├── CMakeLists.txt                      # Unified build configuration
├── REORGANIZATION_PLAN.md              # Detailed planning document
├── REORGANIZATION_SUMMARY.md           # Implementation summary
└── README.md                           # This file
```

## Key Features

### Multi-Protocol Support
- **HTTP/WebSocket**: REST API and real-time communication using Crow framework
- **gRPC**: High-performance RPC with Protocol Buffers
- **MQTT**: Lightweight pub/sub messaging with Mosquitto
- **ZeroMQ**: Multiple messaging patterns (REQ/REP, PUB/SUB, PUSH/PULL)

### Advanced Capabilities
- **Authentication**: JWT, API keys, OAuth2, LDAP, multi-factor authentication
- **Authorization**: Role-based permissions with fine-grained control
- **Health Monitoring**: System metrics, performance tracking, alerting
- **Error Recovery**: Circuit breakers, retry patterns, fallback mechanisms
- **Configuration**: Multi-source, hierarchical, hot-reloading configuration
- **Logging**: Structured logging with multiple sinks and formatters

### Service-Oriented Architecture
- **Device Service**: Comprehensive device lifecycle management
- **Authentication Service**: Secure user authentication and session management
- **Communication Service**: Intelligent message routing and protocol bridging
- **Health Service**: System monitoring and diagnostics

## Quick Start

### Basic Usage

```cpp
#include <astrocomm/server/server.h>

using namespace astrocomm::server;

int main() {
    // Initialize the server component
    initialize();
    
    // Create a multi-protocol server
    auto server = ServerBuilder()
        .withHttp("localhost", 8080)
        .withGrpc("localhost", 9090)
        .withMqtt("localhost", 1883)
        .withDeviceService("./data/devices")
        .withAuthService("./data/auth.json")
        .withHealthService(true)
        .withLogging("info", "server.log")
        .build();
    
    // Start all protocols
    if (server->startAll()) {
        std::cout << "Server started successfully!" << std::endl;
        
        // Server is now running...
        // Handle shutdown gracefully
        
        server->stopAll();
    }
    
    // Cleanup
    shutdown();
    return 0;
}
```

### Development Server

```cpp
// Quick development server setup
auto server = presets::createDevelopmentServer(8080);
server->startAll();
```

### Production Server

```cpp
// Production server with full configuration
auto server = presets::createProductionServer("./config/production.json");
server->startAll();
```

## Building

### Prerequisites

- CMake 3.15 or higher
- C++17 compatible compiler
- spdlog library
- nlohmann_json library

### Optional Dependencies

- **Crow**: For HTTP/WebSocket support
- **gRPC & Protocol Buffers**: For gRPC support
- **Mosquitto**: For MQTT support
- **ZeroMQ**: For ZMQ support

### Build Commands

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Build Options

```bash
# Enable specific protocols
cmake -DHYDROGEN_ENABLE_GRPC=ON -DHYDROGEN_ENABLE_MQTT=ON ..

# Build examples
cmake -DHYDROGEN_BUILD_EXAMPLES=ON ..

# Build tests
cmake -DHYDROGEN_BUILD_TESTS=ON ..
```

## Configuration

### File-based Configuration

```json
{
  "server": {
    "http": {
      "host": "localhost",
      "port": 8080,
      "enable_ssl": false
    },
    "grpc": {
      "host": "localhost", 
      "port": 9090
    },
    "mqtt": {
      "host": "localhost",
      "port": 1883
    }
  },
  "services": {
    "device": {
      "persistence_dir": "./data/devices"
    },
    "auth": {
      "jwt_secret": "your-secret-key",
      "session_timeout": 3600
    }
  },
  "logging": {
    "level": "info",
    "file": "server.log"
  }
}
```

### Environment Variables

```bash
export ASTROCOMM_HTTP_PORT=8080
export ASTROCOMM_GRPC_PORT=9090
export ASTROCOMM_LOG_LEVEL=debug
```

## Examples

The `examples/` directory contains comprehensive examples demonstrating:

- Multi-protocol server setup
- Service integration
- Device management
- Configuration management
- Authentication and authorization
- Health monitoring

## Testing

```bash
# Run unit tests
make test

# Run specific test suite
ctest -R "server_tests"
```

## Migration Guide

### From Old Architecture

1. **Update Include Paths**: 
   ```cpp
   // Old
   #include "device_server.h"
   
   // New
   #include <astrocomm/server/services/device_service.h>
   ```

2. **Use Service Registry**:
   ```cpp
   // Old
   auto deviceManager = std::make_unique<DeviceManager>();
   
   // New
   auto& registry = getServiceRegistry();
   auto deviceService = registry.getService<IDeviceService>();
   ```

3. **Adopt New Interfaces**:
   ```cpp
   // Old
   DeviceServer server;
   
   // New
   auto server = ServerBuilder().withHttp().build();
   ```

## Benefits

- **Maintainability**: Clear separation of concerns and layered architecture
- **Testability**: Dependency injection enables comprehensive unit testing
- **Extensibility**: Easy to add new protocols and services
- **Reliability**: Comprehensive error handling and health monitoring
- **Performance**: Efficient message routing and connection management
- **Security**: Built-in authentication, authorization, and audit logging

## Contributing

When contributing to the server component:

1. Follow the layered architecture principles
2. Use dependency injection through the service registry
3. Implement proper error handling and logging
4. Add comprehensive unit tests
5. Update documentation and examples

## License

This project is part of the AstroComm library and follows the same licensing terms.

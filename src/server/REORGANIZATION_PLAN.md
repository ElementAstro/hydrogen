# Server Directory Reorganization Plan

## Current State Analysis

### Existing Directories
- `src/server/` - Multi-protocol server implementation (primary)
- `src/server_component/` - Simpler server component (secondary)

### Current Issues Identified
1. **Duplication**: Two separate server implementations
2. **Incomplete Implementation**: Many TODOs in gRPC server
3. **Mixed Architecture**: Files scattered across locations
4. **Inconsistent Naming**: Different conventions across files
5. **Missing Abstractions**: No clear service layer or interfaces
6. **Build System Complexity**: Two different CMake approaches

### Current Tech Stack
- **Web Framework**: Crow (HTTP/WebSocket)
- **Protocols**: MQTT (Mosquitto), gRPC (Protocol Buffers), ZeroMQ
- **Logging**: spdlog
- **JSON**: nlohmann_json
- **Threading**: std::thread, std::atomic
- **Build System**: CMake with modern practices
- **Authentication**: Custom auth manager
- **Device Management**: Custom device manager

## Target Architecture

### New Directory Structure
```
src/server/
├── include/astrocomm/server/
│   ├── core/                           # Core interfaces and base classes
│   │   ├── server_interface.h          # Base server interface
│   │   ├── protocol_handler.h          # Protocol abstraction
│   │   └── service_registry.h          # Service discovery
│   ├── services/                       # Business logic services
│   │   ├── device_service.h            # Device management service
│   │   ├── auth_service.h              # Authentication service
│   │   ├── communication_service.h     # Message routing service
│   │   └── health_service.h            # Health monitoring service
│   ├── protocols/                      # Protocol-specific implementations
│   │   ├── http/                       # HTTP/WebSocket server
│   │   ├── grpc/                       # gRPC server
│   │   ├── mqtt/                       # MQTT broker
│   │   └── zmq/                        # ZeroMQ server
│   ├── repositories/                   # Data access layer
│   │   ├── device_repository.h         # Device data access
│   │   ├── user_repository.h           # User data access
│   │   └── config_repository.h         # Configuration access
│   └── infrastructure/                 # Cross-cutting concerns
│       ├── logging.h                   # Logging infrastructure
│       ├── config_manager.h            # Configuration management
│       └── error_handler.h             # Error handling
├── src/                                # Implementation files
│   ├── core/
│   ├── services/
│   ├── protocols/
│   ├── repositories/
│   └── infrastructure/
├── examples/                           # Usage examples
├── tests/                              # Unit tests
├── proto/                              # Protocol buffer definitions
├── CMakeLists.txt                      # Unified build configuration
└── README.md                           # Documentation
```

### Architecture Layers
1. **Presentation Layer**: Protocol-specific servers (HTTP, MQTT, gRPC, ZMQ)
2. **Service Layer**: Business logic for device management, authentication, communication routing
3. **Repository Layer**: Data access for device configurations, user management
4. **Infrastructure Layer**: Logging, configuration, error handling

### Design Principles
- **SOLID Principles**: Single responsibility, dependency inversion, interface segregation
- **Factory Pattern**: For creating protocol-specific servers
- **Repository Pattern**: For data access abstraction
- **Service Layer**: For business logic encapsulation
- **Dependency Injection**: For loose coupling between components

## Migration Strategy

### Phase 1: Structure Analysis and Preparation
1. Backup current implementations
2. Analyze file dependencies and create migration map
3. Create new directory structure
4. Design interface abstractions and service contracts

### Phase 2: Core Infrastructure Setup
1. Implement core interfaces and base classes
2. Create service layer abstractions
3. Set up repository pattern for data access
4. Implement configuration management system

### Phase 3: Protocol Implementation Migration
1. Migrate and enhance HTTP/WebSocket server (Crow-based)
2. Complete and migrate gRPC server implementation
3. Migrate and enhance MQTT broker
4. Migrate and enhance ZeroMQ server

### Phase 4: Service Layer Implementation
1. Implement device management service
2. Implement authentication service
3. Implement communication routing service
4. Implement health monitoring service

### Phase 5: Build System Consolidation
1. Create unified CMakeLists.txt
2. Configure protocol-specific dependencies
3. Set up examples and testing infrastructure
4. Configure installation and packaging

### Phase 6: Integration and Testing
1. Integrate all components into unified server
2. Update examples and documentation
3. Perform comprehensive testing
4. Clean up obsolete files and directories

## Risk Mitigation

### Potential Issues
- **Code Signature Changes**: Moving files will require updating include paths
- **Performance Impact**: Additional abstraction layers may introduce overhead
- **Build System Changes**: Consolidating CMake files may affect existing scripts
- **Temporary Instability**: Extensive reorganization could introduce issues

### Mitigation Strategies
- Use gradual migration with compatibility headers
- Implement templates and compile-time optimization
- Maintain backward compatibility during transition
- Implement changes incrementally
- Comprehensive testing at each phase

## Success Criteria

1. **Single Unified Server Implementation**: No duplication between directories
2. **Complete Protocol Support**: All protocols (HTTP, MQTT, gRPC, ZMQ) fully implemented
3. **Clean Architecture**: Clear separation of concerns with proper layering
4. **Modern CMake**: Single, maintainable build system
5. **Comprehensive Testing**: Full test coverage for all components
6. **Documentation**: Clear examples and API documentation
7. **Backward Compatibility**: Existing applications continue to work

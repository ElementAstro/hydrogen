# Hydrogen Server Component - Unified Multi-Protocol Communication Server
# Reorganized and enhanced server architecture with layered design

cmake_minimum_required(VERSION 3.15)

# =============================================================================
# Project Configuration
# =============================================================================

set(COMPONENT_NAME "Server")
set(TARGET_NAME "hydrogen_server")

message(STATUS "Configuring Hydrogen ${COMPONENT_NAME} component (reorganized)...")

# =============================================================================
# Dependencies
# =============================================================================

# Core dependencies
find_package(Threads REQUIRED)
find_package(spdlog QUIET)
find_package(nlohmann_json REQUIRED)

# Protocol-specific dependencies (optional)
find_package(PkgConfig QUIET)

# MQTT dependencies
if(PkgConfig_FOUND)
    pkg_check_modules(MOSQUITTO libmosquitto)
endif()

# gRPC dependencies - rely on global dependency resolution
# These should already be found by hydrogen_find_all_dependencies() in main CMakeLists.txt
# No need to call find_package again here as it causes target conflicts

# ZeroMQ dependencies
if(PkgConfig_FOUND)
    pkg_check_modules(ZMQ libzmq)
endif()

# HTTP/WebSocket dependencies (Crow framework)
find_package(Crow QUIET)

# =============================================================================
# Source Files Organization
# =============================================================================

# Core infrastructure sources
set(CORE_SOURCES
    src/core/protocol_handler.cpp
    src/core/service_registry.cpp
)

# Service layer sources
set(SERVICE_SOURCES
    src/services/device_service_impl.cpp
    src/services/auth_service_impl.cpp
    src/services/communication_service_impl.cpp
    src/services/health_service_impl.cpp
)

# Repository layer sources
set(REPOSITORY_SOURCES
    src/repositories/device_repository_impl.cpp
    src/repositories/user_repository_impl.cpp
    src/repositories/config_repository_impl.cpp
)

# Infrastructure sources
set(INFRASTRUCTURE_SOURCES
    src/infrastructure/config_manager_impl.cpp
    src/infrastructure/logging_impl.cpp
    src/infrastructure/error_handler_impl.cpp
)

# Protocol implementation sources
set(PROTOCOL_SOURCES
    src/protocols/http/http_server_impl.cpp
    src/protocols/grpc/grpc_server_impl.cpp
    src/protocols/mqtt/mqtt_broker_impl.cpp
    src/protocols/zmq/zmq_server_impl.cpp
)

# Combine all sources
set(SERVER_SOURCES
    ${CORE_SOURCES}
    ${SERVICE_SOURCES}
    ${REPOSITORY_SOURCES}
    ${INFRASTRUCTURE_SOURCES}
    ${PROTOCOL_SOURCES}
)

# Header files
set(SERVER_HEADERS
    # Core headers
    include/Hydrogen/server/core/server_interface.h
    include/Hydrogen/server/core/protocol_handler.h
    include/Hydrogen/server/core/service_registry.h
    
    # Service headers
    include/Hydrogen/server/services/device_service.h
    include/Hydrogen/server/services/auth_service.h
    include/Hydrogen/server/services/communication_service.h
    include/Hydrogen/server/services/health_service.h
    
    # Repository headers
    include/Hydrogen/server/repositories/device_repository.h
    include/Hydrogen/server/repositories/user_repository.h
    include/Hydrogen/server/repositories/config_repository.h
    
    # Infrastructure headers
    include/Hydrogen/server/infrastructure/config_manager.h
    include/Hydrogen/server/infrastructure/logging.h
    include/Hydrogen/server/infrastructure/error_handler.h
    
    # Protocol headers
    include/Hydrogen/server/protocols/http/http_server.h
    include/Hydrogen/server/protocols/grpc/grpc_server.h
    include/Hydrogen/server/protocols/mqtt/mqtt_broker.h
    include/Hydrogen/server/protocols/zmq/zmq_server.h
)

# =============================================================================
# Protocol Buffers Generation (for gRPC)
# =============================================================================

if(Protobuf_FOUND AND gRPC_FOUND)
    set(PROTO_FILES
        proto/device_service.proto
        proto/communication.proto
        proto/health.proto
    )
    
    # Generate protobuf and gRPC files
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
    
    # Generate gRPC sources
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
    if(GRPC_CPP_PLUGIN)
        foreach(PROTO_FILE ${PROTO_FILES})
            get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
            set(GRPC_SRC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
            set(GRPC_HDR "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")
            
            add_custom_command(
                OUTPUT ${GRPC_SRC} ${GRPC_HDR}
                COMMAND protobuf::protoc
                ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
                     --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                     --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
                     -I${CMAKE_CURRENT_SOURCE_DIR}
                     ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
                COMMENT "Generating gRPC sources for ${PROTO_FILE}"
            )
            
            list(APPEND GRPC_SRCS ${GRPC_SRC})
            list(APPEND GRPC_HDRS ${GRPC_HDR})
        endforeach()
    endif()
    
    list(APPEND SERVER_SOURCES ${PROTO_SRCS} ${GRPC_SRCS})
    message(STATUS "Hydrogen Server: gRPC support enabled")
else()
    message(STATUS "Hydrogen Server: gRPC support disabled")
endif()

# =============================================================================
# Library Target
# =============================================================================

add_library(${TARGET_NAME} ${SERVER_SOURCES} ${SERVER_HEADERS})
add_library(Hydrogen::Server ALIAS ${TARGET_NAME})

# =============================================================================
# Target Configuration
# =============================================================================

set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME Server
)

# Include directories
target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# =============================================================================
# Link Dependencies
# =============================================================================

# Core dependencies
target_link_libraries(${TARGET_NAME}
    PUBLIC
        Hydrogen::Core
        nlohmann_json::nlohmann_json
        Threads::Threads
    PRIVATE
        Hydrogen::Common
)

# Optional logging dependency
if(spdlog_FOUND)
    target_link_libraries(${TARGET_NAME} PUBLIC spdlog::spdlog)
    target_compile_definitions(${TARGET_NAME} PUBLIC HYDROGEN_HAS_SPDLOG)
endif()

# Protocol-specific dependencies
if(MOSQUITTO_FOUND)
    target_link_libraries(${TARGET_NAME} PRIVATE ${MOSQUITTO_LIBRARIES})
    target_include_directories(${TARGET_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS})
    target_compile_definitions(${TARGET_NAME} PRIVATE hydrogen_HAS_MOSQUITTO)
    message(STATUS "Hydrogen Server: Mosquitto MQTT support enabled")
endif()

if(Protobuf_FOUND AND gRPC_FOUND)
    target_link_libraries(${TARGET_NAME} PRIVATE
        protobuf::libprotobuf
        gRPC::grpc++
        gRPC::grpc++_reflection
    )
    target_compile_definitions(${TARGET_NAME} PRIVATE hydrogen_HAS_GRPC)
endif()

if(ZMQ_FOUND)
    target_link_libraries(${TARGET_NAME} PRIVATE ${ZMQ_LIBRARIES})
    target_include_directories(${TARGET_NAME} PRIVATE ${ZMQ_INCLUDE_DIRS})
    target_compile_definitions(${TARGET_NAME} PRIVATE hydrogen_HAS_ZMQ)
    message(STATUS "Hydrogen Server: ZeroMQ support enabled")
endif()

if(Crow_FOUND)
    target_link_libraries(${TARGET_NAME} PRIVATE Crow::Crow)
    target_compile_definitions(${TARGET_NAME} PRIVATE hydrogen_HAS_CROW)
    message(STATUS "Hydrogen Server: Crow HTTP/WebSocket support enabled")
endif()

# =============================================================================
# Compiler Definitions
# =============================================================================

target_compile_definitions(${TARGET_NAME}
    PRIVATE
        hydrogen_SERVER_COMPONENT
        hydrogen_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        hydrogen_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        hydrogen_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# =============================================================================
# Installation
# =============================================================================

# Install the library
install(TARGETS ${TARGET_NAME}
    EXPORT HydrogenServerTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers with proper structure
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Install generated protobuf headers
if(Protobuf_FOUND AND gRPC_FOUND)
    install(FILES ${PROTO_HDRS} ${GRPC_HDRS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Hydrogen/server/proto
    )
endif()

# =============================================================================
# Examples and Testing
# =============================================================================

if(HYDROGEN_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(HYDROGEN_BUILD_TESTS)
    # Only add tests subdirectory if it exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    else()
        message(STATUS "Hydrogen Server: Tests directory not found, skipping server tests")
    endif()
endif()

# =============================================================================
# Summary
# =============================================================================

message(STATUS "Hydrogen ${COMPONENT_NAME} component configured successfully (reorganized)")
message(STATUS "  - Target: ${TARGET_NAME}")
message(STATUS "  - Architecture: Layered (Core/Services/Repositories/Infrastructure/Protocols)")
message(STATUS "  - MQTT support: ${MOSQUITTO_FOUND}")
message(STATUS "  - gRPC support: ${Protobuf_FOUND}")
message(STATUS "  - ZeroMQ support: ${ZMQ_FOUND}")
message(STATUS "  - HTTP/WebSocket support: ${Crow_FOUND}")
message(STATUS "  - Source files: ${CMAKE_CURRENT_LIST_LENGTH} organized in layers")

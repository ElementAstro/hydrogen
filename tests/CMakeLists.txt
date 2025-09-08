# Hydrogen Test Suite
# Comprehensive testing for all components using Google Test

cmake_minimum_required(VERSION 3.10)

# Include Google Test
find_package(GTest REQUIRED)
include(GoogleTest)

# Set up test include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/device_component/include
    ${CMAKE_SOURCE_DIR}/src/server_component/include
    ${CMAKE_SOURCE_DIR}/src/client_component/include
)

# Simple test utilities library
add_library(test_utils STATIC
    utils/simple_helpers.cpp
)

target_link_libraries(test_utils
    PUBLIC
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(test_utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# Comprehensive testing framework
add_library(hydrogen_test_framework STATIC
    framework/comprehensive_test_framework.cpp
    framework/mock_objects.cpp
)

target_include_directories(hydrogen_test_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/framework
        ${CMAKE_SOURCE_DIR}/src/core/include
        ${CMAKE_SOURCE_DIR}/src/client_component/include
)

target_link_libraries(hydrogen_test_framework
    PUBLIC
        GTest::gtest
        GTest::gmock
        nlohmann_json::nlohmann_json
        hydrogen_core
        test_utils
)

# Optional spdlog dependency (if available)
if(TARGET spdlog::spdlog)
    target_link_libraries(hydrogen_test_framework PUBLIC spdlog::spdlog)
    target_compile_definitions(hydrogen_test_framework PUBLIC HYDROGEN_HAS_SPDLOG)
endif()

# Core component tests
add_subdirectory(core)

# Client component tests
add_subdirectory(client)

# Server component tests (create if missing)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/server/CMakeLists.txt)
    add_subdirectory(server)
else()
    message(STATUS "Hydrogen: Server tests directory exists but no CMakeLists.txt found")
endif()

# Device component tests
add_subdirectory(device)

# Integration tests
add_subdirectory(integration)

# Communication protocol tests
add_subdirectory(fifo_communication)

# Build system tests
add_subdirectory(build_system)

# Performance tests (create if missing)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/performance/CMakeLists.txt)
    add_subdirectory(performance)
else()
    message(STATUS "Hydrogen: Performance tests directory exists but no CMakeLists.txt found")
endif()

# Protocol tests (create if missing)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/protocols/CMakeLists.txt)
    add_subdirectory(protocols)
else()
    message(STATUS "Hydrogen: Protocol tests directory exists but no CMakeLists.txt found")
endif()

# Comprehensive tests (create if missing)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/comprehensive/CMakeLists.txt)
    add_subdirectory(comprehensive)
else()
    message(STATUS "Hydrogen: Comprehensive tests directory exists but no CMakeLists.txt found")
endif()

# STDIO tests
add_subdirectory(stdio)

# =============================================================================
# Meta Test Targets
# =============================================================================

# Create individual test suite targets
add_custom_target(component_tests
    DEPENDS core_tests client_tests device_tests server_tests
    COMMENT "Building all component tests"
)

add_custom_target(communication_tests
    DEPENDS fifo_tests test_stdio_all
    COMMENT "Building all communication protocol tests"
)

add_custom_target(system_tests
    DEPENDS integration_tests test_build_system
    COMMENT "Building all system-level tests"
)

# Create a target to run all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS
        core_tests
        client_tests
        device_tests
        server_tests
        integration_tests
        fifo_tests
        test_stdio_all
        test_build_system
        performance_tests
        protocol_tests
        comprehensive_tests
    COMMENT "Running all Hydrogen tests"
)

# Create a comprehensive test target that builds everything
add_custom_target(all_tests
    DEPENDS
        component_tests
        communication_tests
        system_tests
    COMMENT "Building all Hydrogen test suites"
)

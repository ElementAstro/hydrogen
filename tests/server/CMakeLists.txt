# Hydrogen Server Tests
# Tests for server component functionality

cmake_minimum_required(VERSION 3.15)

# Include Hydrogen build system utilities
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenUtils.cmake)

# Find required testing dependencies
find_package(GTest REQUIRED)
include(GoogleTest)

# Server tests executable
add_executable(server_tests
    test_enhanced_device_server.cpp
    test_stdio_server.cpp
    test_mqtt_broker.cpp
    test_main.cpp
)

# Apply modern CMake configuration
hydrogen_apply_modern_cmake(server_tests)

# Configure target with standard settings
hydrogen_configure_target_standard(server_tests)

# Link dependencies
target_link_libraries(server_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Hydrogen::Server
    Hydrogen::Core
    hydrogen_test_framework
)

# Link system dependencies
hydrogen_link_system_dependencies(server_tests)

# Apply feature definitions
if(COMMAND hydrogen_apply_feature_definitions)
    hydrogen_apply_feature_definitions(server_tests)
endif()

# Register tests with CTest
gtest_discover_tests(server_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "server;component"
        TIMEOUT 120
)

message(STATUS "Hydrogen: Server tests configured")

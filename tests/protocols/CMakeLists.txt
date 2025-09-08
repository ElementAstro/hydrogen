# Hydrogen Protocol Tests
# Tests for various communication protocols

cmake_minimum_required(VERSION 3.15)

# Include Hydrogen build system utilities
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenUtils.cmake)

# Find required testing dependencies
find_package(GTest REQUIRED)
include(GoogleTest)

# Protocol tests executable
add_executable(protocol_tests
    placeholder_test.cpp
    test_protocol_implementations.cpp
)

# Apply modern CMake configuration
hydrogen_apply_modern_cmake(protocol_tests)

# Configure target with standard settings
hydrogen_configure_target_standard(protocol_tests)

# Link dependencies
target_link_libraries(protocol_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Hydrogen::Core
    Hydrogen::Server
    hydrogen_test_framework
)

# Link system dependencies
hydrogen_link_system_dependencies(protocol_tests)

# Apply feature definitions
if(COMMAND hydrogen_apply_feature_definitions)
    hydrogen_apply_feature_definitions(protocol_tests)
endif()

# Register tests with CTest
gtest_discover_tests(protocol_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "protocol;communication"
        TIMEOUT 180
)

message(STATUS "Hydrogen: Protocol tests configured")

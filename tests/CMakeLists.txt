# AstroComm Test Suite
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

# Core component tests
add_subdirectory(core)

# Device component tests (TODO: implement)
# add_subdirectory(device)

# Server component tests (TODO: implement)
# add_subdirectory(server)

# Client component tests (TODO: implement)
# add_subdirectory(client)

# Integration tests (TODO: implement)
# add_subdirectory(integration)

# Create a target to run all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS
        core_tests
        # device_tests
        # server_tests
        # client_tests
        # integration_tests
    COMMENT "Running all AstroComm tests"
)

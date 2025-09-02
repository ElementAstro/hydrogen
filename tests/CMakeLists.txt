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

# Device component tests (TODO: implement)
# add_subdirectory(device)

# Server component tests (TODO: implement)
# add_subdirectory(server)

# Client component tests
add_subdirectory(client)

# Integration tests
add_subdirectory(integration)

# Create a target to run all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS
        core_tests
        client_tests
        integration_tests
        # device_tests
        # server_tests
    COMMENT "Running all Hydrogen tests"
)

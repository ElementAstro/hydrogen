# CMake configuration for Hydrogen build system integration tests

cmake_minimum_required(VERSION 3.15)

# Find required packages
find_package(GTest REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/server/include
    ${CMAKE_SOURCE_DIR}/src/device/include
    ${GTEST_INCLUDE_DIRS}
)

# Common test libraries
set(BUILD_SYSTEM_TEST_LIBRARIES
    ${GTEST_LIBRARIES}
    pthread
)

# Platform-specific libraries
if(WIN32)
    list(APPEND BUILD_SYSTEM_TEST_LIBRARIES ws2_32)
endif()

# Build system integration tests
add_executable(test_build_integration
    test_build_integration.cpp
)
target_link_libraries(test_build_integration ${BUILD_SYSTEM_TEST_LIBRARIES})

# Test discovery for CTest
include(GoogleTest)

gtest_discover_tests(test_build_integration
    PROPERTIES
    LABELS "build_system;integration"
    TIMEOUT 60
)

# Custom test targets
add_custom_target(test_build_system
    COMMAND ${CMAKE_CTEST_COMMAND} -L build_system --output-on-failure
    DEPENDS test_build_integration
    COMMENT "Running build system integration tests"
)

# Test summary
message(STATUS "Build system integration tests configured:")
message(STATUS "  Integration tests: test_build_integration")
message(STATUS "  Run all: make test_build_system")

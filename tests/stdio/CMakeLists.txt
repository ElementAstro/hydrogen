# CMake configuration for Hydrogen stdio communication tests

cmake_minimum_required(VERSION 3.15)

# Find required packages
find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/server/include
    ${CMAKE_SOURCE_DIR}/src/device/include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

# Common test libraries
set(STDIO_TEST_LIBRARIES
    hydrogen_core
    hydrogen_server
    hydrogen_device
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    pthread
)

# Core stdio tests
add_executable(test_stdio_communicator
    ../core/test_stdio_communicator.cpp
)
target_link_libraries(test_stdio_communicator ${STDIO_TEST_LIBRARIES})

add_executable(test_stdio_message_transformer
    ../core/test_stdio_message_transformer.cpp
)
target_link_libraries(test_stdio_message_transformer ${STDIO_TEST_LIBRARIES})

# Server stdio tests
add_executable(test_stdio_server
    ../server/test_stdio_server.cpp
)
target_link_libraries(test_stdio_server ${STDIO_TEST_LIBRARIES})

# Integration tests
add_executable(test_stdio_integration
    ../integration/test_stdio_integration.cpp
)
target_link_libraries(test_stdio_integration ${STDIO_TEST_LIBRARIES})

# Test discovery for CTest
include(GoogleTest)

gtest_discover_tests(test_stdio_communicator
    PROPERTIES
    LABELS "stdio;core;unit"
    TIMEOUT 30
)

gtest_discover_tests(test_stdio_message_transformer
    PROPERTIES
    LABELS "stdio;core;unit"
    TIMEOUT 30
)

gtest_discover_tests(test_stdio_server
    PROPERTIES
    LABELS "stdio;server;unit"
    TIMEOUT 30
)

gtest_discover_tests(test_stdio_integration
    PROPERTIES
    LABELS "stdio;integration"
    TIMEOUT 60
)

# Custom test targets for stdio
add_custom_target(test_stdio_all
    COMMAND ${CMAKE_CTEST_COMMAND} -L stdio --output-on-failure
    DEPENDS 
        test_stdio_communicator
        test_stdio_message_transformer
        test_stdio_server
        test_stdio_integration
    COMMENT "Running all stdio communication tests"
)

add_custom_target(test_stdio_unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L "stdio;unit" --output-on-failure
    DEPENDS 
        test_stdio_communicator
        test_stdio_message_transformer
        test_stdio_server
    COMMENT "Running stdio unit tests"
)

add_custom_target(test_stdio_integration_only
    COMMAND ${CMAKE_CTEST_COMMAND} -L "stdio;integration" --output-on-failure
    DEPENDS test_stdio_integration
    COMMENT "Running stdio integration tests"
)

# Test summary
message(STATUS "Stdio communication tests configured:")
message(STATUS "  Core tests: test_stdio_communicator, test_stdio_message_transformer")
message(STATUS "  Server tests: test_stdio_server")
message(STATUS "  Integration tests: test_stdio_integration")
message(STATUS "  Run all: make test_stdio_all")

# Hydrogen Server Tests
cmake_minimum_required(VERSION 3.15)

# Find required testing dependencies
find_package(GTest REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
)

# Test source files
set(SERVER_TEST_SOURCES
    test_main.cpp
    test_communication_service.cpp
    test_health_service.cpp
    test_device_repository.cpp
    test_user_repository.cpp
    test_config_repository.cpp
    test_config_manager.cpp
    test_logging_manager.cpp
    test_error_handler.cpp
    test_grpc_server.cpp
    test_mqtt_broker.cpp
    test_zmq_server.cpp
)

# Create test executable
add_executable(test_hydrogen_server ${SERVER_TEST_SOURCES})

# Link libraries
target_link_libraries(test_hydrogen_server
    PRIVATE
        hydrogen_server
        hydrogen_core
        hydrogen_common
        GTest::gtest
        GTest::gtest_main
        ${CMAKE_THREAD_LIBS_INIT}
)

# Set C++ standard
target_compile_features(test_hydrogen_server PRIVATE cxx_std_17)

# Add compiler flags
target_compile_options(test_hydrogen_server PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(test_hydrogen_server
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "server"
        TIMEOUT 60
)

# Add custom test target
add_custom_target(run_server_tests
    COMMAND test_hydrogen_server
    DEPENDS test_hydrogen_server
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running Hydrogen Server tests"
)

# Test data directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_data)

# Copy test configuration files
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_config.json
    ${CMAKE_CURRENT_BINARY_DIR}/test_config.json
    COPYONLY
)

message(STATUS "Hydrogen Server tests configured")
message(STATUS "  - Test executable: test_hydrogen_server")
message(STATUS "  - Test sources: ${SERVER_TEST_SOURCES}")
message(STATUS "  - Test framework: Google Test")

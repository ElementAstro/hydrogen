# Device Component Tests
# Comprehensive unit tests for the optimized device module

# Find required testing dependencies
find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src/device_component/include
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

# Helper function to create a test executable
function(add_device_test test_name source_file)
    add_executable(${test_name} ${source_file})
    
    target_link_libraries(${test_name}
        PRIVATE
            hydrogen_device
            Hydrogen::Core
            ${GTEST_LIBRARIES}
            ${GMOCK_LIBRARIES}
            Threads::Threads
    )
    
    # Add Boost libraries if available
    if(TARGET Boost::system)
        target_link_libraries(${test_name} PRIVATE Boost::system)
    endif()
    
    # Set test properties
    set_target_properties(${test_name} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
    
    # Add compiler flags for testing
    target_compile_options(${test_name} PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
    
    # Add test definitions
    target_compile_definitions(${test_name} PRIVATE
        HYDROGEN_TESTING
        HYDROGEN_HAS_WEBSOCKETS
    )
    
    # Register with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    # Set test timeout (5 minutes)
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 300)
endfunction()

# Create individual test executables
add_device_test(test_websocket_device test_websocket_device.cpp)
add_device_test(test_telescope test_telescope.cpp)
add_device_test(test_device_logger test_device_logger.cpp)
add_device_test(test_performance_monitor test_performance_monitor.cpp)

# Create a comprehensive test suite
add_executable(device_component_tests
    test_websocket_device.cpp
    test_telescope.cpp
    test_device_logger.cpp
    test_performance_monitor.cpp
)

target_link_libraries(device_component_tests
    PRIVATE
        hydrogen_device
        Hydrogen::Core
        ${GTEST_LIBRARIES}
        ${GMOCK_LIBRARIES}
        Threads::Threads
)

if(TARGET Boost::system)
    target_link_libraries(device_component_tests PRIVATE Boost::system)
endif()

set_target_properties(device_component_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

target_compile_options(device_component_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

target_compile_definitions(device_component_tests PRIVATE
    HYDROGEN_TESTING
    HYDROGEN_HAS_WEBSOCKETS
)

# Register comprehensive test suite
add_test(NAME device_component_tests COMMAND device_component_tests)
set_tests_properties(device_component_tests PROPERTIES TIMEOUT 600) # 10 minutes for full suite

# Create performance benchmark executable
add_executable(device_performance_benchmark
    benchmark_device_performance.cpp
)

target_link_libraries(device_performance_benchmark
    PRIVATE
        hydrogen_device
        Hydrogen::Core
        Threads::Threads
)

if(TARGET Boost::system)
    target_link_libraries(device_performance_benchmark PRIVATE Boost::system)
endif()

set_target_properties(device_performance_benchmark PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks
)

target_compile_definitions(device_performance_benchmark PRIVATE
    HYDROGEN_BENCHMARKING
    HYDROGEN_HAS_WEBSOCKETS
)

# Add custom target to run all device tests
add_custom_target(test_device_component
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "test_.*device.*|device_component_tests"
    DEPENDS 
        test_websocket_device
        test_telescope
        test_device_logger
        test_performance_monitor
        device_component_tests
    COMMENT "Running all device component tests"
)

# Add custom target to run performance benchmarks
add_custom_target(benchmark_device_component
    COMMAND device_performance_benchmark
    DEPENDS device_performance_benchmark
    COMMENT "Running device component performance benchmarks"
)

# Test data and configuration
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_config.json.in
    ${CMAKE_BINARY_DIR}/tests/test_config.json
    @ONLY
)

# Create test output directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/output)

# Summary
message(STATUS "Device Component Tests configured:")
message(STATUS "  - Individual tests: test_websocket_device, test_telescope, test_device_logger, test_performance_monitor")
message(STATUS "  - Comprehensive suite: device_component_tests")
message(STATUS "  - Performance benchmark: device_performance_benchmark")
message(STATUS "  - Custom targets: test_device_component, benchmark_device_component")
message(STATUS "  - Test output directory: ${CMAKE_BINARY_DIR}/tests")
message(STATUS "  - Benchmark output directory: ${CMAKE_BINARY_DIR}/benchmarks")

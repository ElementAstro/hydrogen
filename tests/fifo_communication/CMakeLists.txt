# FIFO Communication Tests CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/server/include
    ${GTEST_INCLUDE_DIRS}
)

# Common test libraries
set(FIFO_TEST_LIBRARIES
    hydrogen_core
    hydrogen_server
    GTest::gtest
    GTest::gtest_main
    nlohmann_json::nlohmann_json
    pthread
)

# Platform-specific libraries
if(WIN32)
    list(APPEND FIFO_TEST_LIBRARIES ws2_32)
endif()

# Test source files
set(FIFO_CONFIG_TEST_SOURCES
    test_fifo_config.cpp
)

set(FIFO_COMMUNICATOR_TEST_SOURCES
    test_fifo_communicator.cpp
)

set(FIFO_INTEGRATION_TEST_SOURCES
    test_fifo_integration.cpp
)

set(FIFO_PERFORMANCE_TEST_SOURCES
    test_fifo_performance.cpp
)

# FIFO Configuration Tests
add_executable(test_fifo_config ${FIFO_CONFIG_TEST_SOURCES})
target_link_libraries(test_fifo_config ${FIFO_TEST_LIBRARIES})
target_compile_features(test_fifo_config PRIVATE cxx_std_17)

# Set test properties
set_target_properties(test_fifo_config PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# FIFO Communicator Tests
add_executable(test_fifo_communicator ${FIFO_COMMUNICATOR_TEST_SOURCES})
target_link_libraries(test_fifo_communicator ${FIFO_TEST_LIBRARIES})
target_compile_features(test_fifo_communicator PRIVATE cxx_std_17)

set_target_properties(test_fifo_communicator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# FIFO Integration Tests
add_executable(test_fifo_integration ${FIFO_INTEGRATION_TEST_SOURCES})
target_link_libraries(test_fifo_integration ${FIFO_TEST_LIBRARIES})
target_compile_features(test_fifo_integration PRIVATE cxx_std_17)

set_target_properties(test_fifo_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# FIFO Performance Tests
add_executable(test_fifo_performance ${FIFO_PERFORMANCE_TEST_SOURCES})
target_link_libraries(test_fifo_performance ${FIFO_TEST_LIBRARIES})
target_compile_features(test_fifo_performance PRIVATE cxx_std_17)

set_target_properties(test_fifo_performance PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add tests to CTest
enable_testing()

# Configuration tests
add_test(NAME FifoConfigTest COMMAND test_fifo_config)
set_tests_properties(FifoConfigTest PROPERTIES
    TIMEOUT 60
    LABELS "unit;fifo;config"
)

# Communicator tests
add_test(NAME FifoCommunicatorTest COMMAND test_fifo_communicator)
set_tests_properties(FifoCommunicatorTest PROPERTIES
    TIMEOUT 120
    LABELS "unit;fifo;communicator"
)

# Integration tests
add_test(NAME FifoIntegrationTest COMMAND test_fifo_integration)
set_tests_properties(FifoIntegrationTest PROPERTIES
    TIMEOUT 180
    LABELS "integration;fifo;server"
)

# Performance tests (longer timeout)
add_test(NAME FifoPerformanceTest COMMAND test_fifo_performance)
set_tests_properties(FifoPerformanceTest PROPERTIES
    TIMEOUT 300
    LABELS "performance;fifo;benchmark"
)

# Custom test targets
add_custom_target(fifo_tests
    DEPENDS test_fifo_config test_fifo_communicator test_fifo_integration test_fifo_performance
    COMMENT "Building all FIFO communication tests"
)

add_custom_target(run_fifo_unit_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L "unit" --output-on-failure
    DEPENDS fifo_tests
    COMMENT "Running FIFO unit tests"
)

add_custom_target(run_fifo_integration_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L "integration" --output-on-failure
    DEPENDS fifo_tests
    COMMENT "Running FIFO integration tests"
)

add_custom_target(run_fifo_performance_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L "performance" --output-on-failure
    DEPENDS fifo_tests
    COMMENT "Running FIFO performance tests"
)

add_custom_target(run_all_fifo_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L "fifo" --output-on-failure
    DEPENDS fifo_tests
    COMMENT "Running all FIFO tests"
)

# Test coverage (if available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(GCOV_PATH gcov)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(GCOV_PATH AND LCOV_PATH AND GENHTML_PATH)
        # Add coverage flags
        target_compile_options(test_fifo_config PRIVATE --coverage)
        target_link_options(test_fifo_config PRIVATE --coverage)
        
        target_compile_options(test_fifo_communicator PRIVATE --coverage)
        target_link_options(test_fifo_communicator PRIVATE --coverage)
        
        target_compile_options(test_fifo_integration PRIVATE --coverage)
        target_link_options(test_fifo_integration PRIVATE --coverage)
        
        target_compile_options(test_fifo_performance PRIVATE --coverage)
        target_link_options(test_fifo_performance PRIVATE --coverage)
        
        # Coverage target
        add_custom_target(fifo_coverage
            COMMAND ${LCOV_PATH} --directory . --zerocounters
            COMMAND ${CMAKE_CTEST_COMMAND} -L "fifo"
            COMMAND ${LCOV_PATH} --directory . --capture --output-file fifo_coverage.info
            COMMAND ${LCOV_PATH} --remove fifo_coverage.info '/usr/*' --output-file fifo_coverage.info.cleaned
            COMMAND ${GENHTML_PATH} -o fifo_coverage_html fifo_coverage.info.cleaned
            COMMAND ${CMAKE_COMMAND} -E remove fifo_coverage.info fifo_coverage.info.cleaned
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS fifo_tests
            COMMENT "Generating FIFO test coverage report"
        )
    endif()
endif()

# Valgrind memory check (if available)
find_program(VALGRIND_PATH valgrind)
if(VALGRIND_PATH)
    add_custom_target(fifo_memcheck
        COMMAND ${VALGRIND_PATH} --tool=memcheck --leak-check=full --show-leak-kinds=all 
                --track-origins=yes --verbose --error-exitcode=1 
                ${CMAKE_BINARY_DIR}/tests/test_fifo_config
        COMMAND ${VALGRIND_PATH} --tool=memcheck --leak-check=full --show-leak-kinds=all 
                --track-origins=yes --verbose --error-exitcode=1 
                ${CMAKE_BINARY_DIR}/tests/test_fifo_communicator
        DEPENDS fifo_tests
        COMMENT "Running FIFO tests with Valgrind memory check"
    )
endif()

# Address sanitizer builds
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_custom_target(fifo_tests_asan
        COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug 
                -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" 
                -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" 
                ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} --build . --target fifo_tests
        COMMAND ${CMAKE_CTEST_COMMAND} -L "fifo"
        COMMENT "Building and running FIFO tests with AddressSanitizer"
    )
endif()

# Thread sanitizer builds
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_custom_target(fifo_tests_tsan
        COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug 
                -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" 
                -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" 
                ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} --build . --target fifo_tests
        COMMAND ${CMAKE_CTEST_COMMAND} -L "fifo"
        COMMENT "Building and running FIFO tests with ThreadSanitizer"
    )
endif()

# Install test binaries (optional)
if(INSTALL_TESTS)
    install(TARGETS test_fifo_config test_fifo_communicator test_fifo_integration test_fifo_performance
        RUNTIME DESTINATION bin/tests
        COMPONENT tests
    )
endif()

# Test data files (if any)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data 
         DESTINATION ${CMAKE_BINARY_DIR}/tests)
endif()

# Print test configuration summary
message(STATUS "FIFO Communication Tests Configuration:")
message(STATUS "  Unit tests: test_fifo_config, test_fifo_communicator")
message(STATUS "  Integration tests: test_fifo_integration")
message(STATUS "  Performance tests: test_fifo_performance")
message(STATUS "  Test timeout: 60-300 seconds")
message(STATUS "  Coverage support: ${GCOV_PATH}")
message(STATUS "  Valgrind support: ${VALGRIND_PATH}")
message(STATUS "  Sanitizer support: ${CMAKE_CXX_COMPILER_ID}")

# Help target
add_custom_target(fifo_test_help
    COMMAND ${CMAKE_COMMAND} -E echo "Available FIFO test targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  fifo_tests                - Build all FIFO tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  run_fifo_unit_tests      - Run unit tests only"
    COMMAND ${CMAKE_COMMAND} -E echo "  run_fifo_integration_tests - Run integration tests only"
    COMMAND ${CMAKE_COMMAND} -E echo "  run_fifo_performance_tests - Run performance tests only"
    COMMAND ${CMAKE_COMMAND} -E echo "  run_all_fifo_tests       - Run all FIFO tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  fifo_coverage            - Generate coverage report (if available)"
    COMMAND ${CMAKE_COMMAND} -E echo "  fifo_memcheck            - Run tests with Valgrind (if available)"
    COMMAND ${CMAKE_COMMAND} -E echo "  fifo_tests_asan          - Run tests with AddressSanitizer"
    COMMAND ${CMAKE_COMMAND} -E echo "  fifo_tests_tsan          - Run tests with ThreadSanitizer"
    COMMENT "Displaying FIFO test help"
)

# Common Module Tests
# Tests for shared utilities, message classes, logging, and other common functionality

cmake_minimum_required(VERSION 3.10)

# Find required testing dependencies
find_package(GTest REQUIRED)
include(GoogleTest)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/core/include
)

# Logger tests
add_executable(test_logger
    test_logger.cpp
)

target_link_libraries(test_logger
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_common
        test_utils
)

# Message queue tests
add_executable(test_message_queue
    test_message_queue.cpp
)

target_link_libraries(test_message_queue
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_common
        test_utils
)

# Utilities tests
add_executable(test_utilities
    test_utilities.cpp
)

target_link_libraries(test_utilities
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_common
        test_utils
)

# Message system tests
add_executable(test_message_system
    test_message_system.cpp
)

target_link_libraries(test_message_system
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_common
        test_utils
)

# Error recovery tests (common module specific)
add_executable(test_error_recovery_common
    test_error_recovery_common.cpp
)

target_link_libraries(test_error_recovery_common
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_common
        test_utils
)

# Set C++ standard for all test targets
set(COMMON_TEST_TARGETS
    test_logger
    test_message_queue
    test_utilities
    test_message_system
    test_error_recovery_common
)

foreach(target ${COMMON_TEST_TARGETS})
    target_compile_features(${target} PRIVATE cxx_std_17)
    
    # Add compiler flags
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    )
    
    # Register tests with CTest
    gtest_discover_tests(${target}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PROPERTIES
            LABELS "common"
            TIMEOUT 60
    )
endforeach()

# Create a target for all common tests
add_custom_target(common_tests
    DEPENDS ${COMMON_TEST_TARGETS}
    COMMENT "Building all common module tests"
)

# Optional: Add JSON utilities tests if nlohmann_json is available
if(TARGET nlohmann_json::nlohmann_json)
    add_executable(test_json_utilities
        test_json_utilities.cpp
    )
    
    target_link_libraries(test_json_utilities
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            hydrogen_common
            nlohmann_json::nlohmann_json
            test_utils
    )
    
    target_compile_features(test_json_utilities PRIVATE cxx_std_17)
    
    gtest_discover_tests(test_json_utilities
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PROPERTIES
            LABELS "common"
            TIMEOUT 60
    )
    
    # Add to common tests target
    add_dependencies(common_tests test_json_utilities)
endif()

# Optional: Add string utilities tests
add_executable(test_string_utilities
    test_string_utilities.cpp
)

target_link_libraries(test_string_utilities
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_common
        test_utils
)

target_compile_features(test_string_utilities PRIVATE cxx_std_17)

gtest_discover_tests(test_string_utilities
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "common"
        TIMEOUT 60
)

# Optional: Add time utilities tests
add_executable(test_time_utilities
    test_time_utilities.cpp
)

target_link_libraries(test_time_utilities
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_common
        test_utils
)

target_compile_features(test_time_utilities PRIVATE cxx_std_17)

gtest_discover_tests(test_time_utilities
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "common"
        TIMEOUT 60
)

# Optional: Add validation utilities tests
add_executable(test_validation_utilities
    test_validation_utilities.cpp
)

target_link_libraries(test_validation_utilities
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_common
        test_utils
)

target_compile_features(test_validation_utilities PRIVATE cxx_std_17)

gtest_discover_tests(test_validation_utilities
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "common"
        TIMEOUT 60
)

# Optional: Add configuration tests
add_executable(test_configuration
    test_configuration.cpp
)

target_link_libraries(test_configuration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_common
        test_utils
)

target_compile_features(test_configuration PRIVATE cxx_std_17)

gtest_discover_tests(test_configuration
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "common"
        TIMEOUT 60
)

# Add all optional tests to common tests target
add_dependencies(common_tests 
    test_string_utilities
    test_time_utilities
    test_validation_utilities
    test_configuration
)

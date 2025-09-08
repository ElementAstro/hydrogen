# Integration Tests
# Tests for component interactions and end-to-end functionality

# Include Google Test
find_package(GTest REQUIRED)
include(GoogleTest)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/device_component/include
    ${CMAKE_SOURCE_DIR}/src/server/include
    ${CMAKE_SOURCE_DIR}/src/client_component/include
    ${CMAKE_SOURCE_DIR}/src/common
)

# Integration test executable - DISABLED due to architectural issues
# TODO: Fix class name conflicts between MessageQueueManager classes
# add_executable(test_client_integration
#     test_client_integration.cpp
# )

# # Link libraries
# target_link_libraries(test_client_integration
#     PRIVATE
#         GTest::gtest
#         GTest::gtest_main
#         Hydrogen::Client
#         Hydrogen::Core
#         test_utils
# )

# # Include directories
# target_include_directories(test_client_integration
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/src
#         ${CMAKE_SOURCE_DIR}/src/client
#         ${CMAKE_SOURCE_DIR}/src/client_component/include
#         ${CMAKE_SOURCE_DIR}/src/core/include
# )

# # Register test with CTest
# gtest_discover_tests(test_client_integration
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
# )

# Multi-protocol integration tests
add_executable(test_multi_protocol_integration
    test_multi_protocol_integration.cpp
)

target_link_libraries(test_multi_protocol_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        hydrogen_core
        hydrogen_common
        nlohmann_json::nlohmann_json
)

# Register test with CTest
gtest_discover_tests(test_multi_protocol_integration
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Add to the integration_tests target
add_custom_target(integration_tests
    DEPENDS test_multi_protocol_integration
    COMMENT "Running integration tests"
)

message(STATUS "Hydrogen: Integration tests configured")
message(STATUS "  - Target: test_multi_protocol_integration")
message(STATUS "  - Framework: Google Test")

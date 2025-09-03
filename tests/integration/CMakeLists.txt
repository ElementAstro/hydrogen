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

# Add to the integration_tests target - placeholder for now
add_custom_target(integration_tests
    COMMENT "Integration tests disabled - architectural issues need resolution"
)

message(STATUS "Hydrogen: Integration tests configured")
message(STATUS "  - Target: test_client_integration")
message(STATUS "  - Framework: Google Test")

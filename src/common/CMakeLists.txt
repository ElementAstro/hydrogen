# AstroComm Common Library
# Shared utilities and message classes

cmake_minimum_required(VERSION 3.10)

# Create the common library
add_library(astrocomm_common STATIC
    message.cpp
    message_queue.cpp
    utils.cpp
    logger.cpp
    error_recovery.cpp
)

# Set target properties
set_target_properties(astrocomm_common PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(astrocomm_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find required dependencies
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

# Link dependencies
target_link_libraries(astrocomm_common
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        fmt::fmt
)

# Create alias for modern CMake usage
add_library(AstroComm::Common ALIAS astrocomm_common)

message(STATUS "AstroComm: Common library configured")
message(STATUS "  - Target: astrocomm_common (AstroComm::Common)")
message(STATUS "  - Type: Static library")
message(STATUS "  - Dependencies: nlohmann_json, spdlog, fmt")

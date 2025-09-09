# Hydrogen Applications
# This directory contains example applications and device implementations

# Only build applications if examples are enabled
if(NOT HYDROGEN_BUILD_EXAMPLES)
    message(STATUS "Hydrogen: Skipping applications (examples disabled)")
    return()
endif()

message(STATUS "Hydrogen: Building example applications...")

# Include Hydrogen build system utilities
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenUtils.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenCompiler.cmake)

# =============================================================================
# Application Helper Function
# =============================================================================

# Helper function to create application targets with consistent configuration
function(hydrogen_add_application app_name source_file)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs DEPENDENCIES EXTRA_LIBRARIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Create the executable
    add_executable(${app_name} ${source_file})

    # Apply modern CMake configuration
    hydrogen_apply_modern_cmake(${app_name} NAMESPACE Hydrogen EXPORT_NAME ${app_name})

    # Set standard target properties
    hydrogen_configure_target_standard(${app_name})

    # Link dependencies
    if(ARG_DEPENDENCIES)
        target_link_libraries(${app_name} PRIVATE ${ARG_DEPENDENCIES})
    endif()

    # Link extra libraries
    if(ARG_EXTRA_LIBRARIES)
        target_link_libraries(${app_name} PRIVATE ${ARG_EXTRA_LIBRARIES})
    endif()

    # Link system dependencies
    hydrogen_link_system_dependencies(${app_name})

    # Apply feature definitions
    if(COMMAND hydrogen_apply_feature_definitions)
        hydrogen_apply_feature_definitions(${app_name})
    endif()

    message(STATUS "Hydrogen: Created application target: ${app_name}")
endfunction()

# =============================================================================
# Application Targets
# =============================================================================

# Find fmt library for logging support
find_package(fmt QUIET)

# Server application
hydrogen_add_application(astro_server simple_server.cpp
    DEPENDENCIES Hydrogen::Core
    EXTRA_LIBRARIES $<$<TARGET_EXISTS:fmt::fmt>:fmt::fmt>
)

# Client application
hydrogen_add_application(astro_client client_app.cpp
    DEPENDENCIES Hydrogen::Client Hydrogen::Core
)

# Device applications
hydrogen_add_application(astro_telescope telescope_device.cpp
    DEPENDENCIES Hydrogen::Device Hydrogen::Core
)

hydrogen_add_application(astro_camera camera_device.cpp
    DEPENDENCIES Hydrogen::Device Hydrogen::Core
)

hydrogen_add_application(astro_focuser focuser_device.cpp
    DEPENDENCIES Hydrogen::Device Hydrogen::Core
)

hydrogen_add_application(astro_guider guider_device.cpp
    DEPENDENCIES Hydrogen::Device Hydrogen::Core
)

hydrogen_add_application(astro_rotator rotator_device.cpp
    DEPENDENCIES Hydrogen::Device Hydrogen::Core
)

hydrogen_add_application(astro_solver solver_device.cpp
    DEPENDENCIES Hydrogen::Device Hydrogen::Core
)

hydrogen_add_application(astro_switch switch_device.cpp
    DEPENDENCIES Hydrogen::Device Hydrogen::Core
)

# =============================================================================
# Installation Rules
# =============================================================================

# Installation rules for applications
include(GNUInstallDirs)

install(TARGETS
    astro_server
    astro_client
    astro_telescope
    astro_camera
    astro_focuser
    astro_guider
    astro_rotator
    astro_solver
    astro_switch
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Applications
)

message(STATUS "Hydrogen: Application build configuration complete")

# AstroComm Applications
# This directory contains example applications and device implementations

# Add include directories for device headers
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/.cmake-cache/crow-v1.2.1.2/include)

# Find required libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(FMT REQUIRED fmt)

# Server application (temporarily disabled due to Crow/ASIO compatibility issues)
# add_executable(astro_server server_main.cpp)
# target_link_libraries(astro_server PRIVATE AstroComm::Server AstroComm::Core ${FMT_LIBRARIES})
# if(WIN32)
#     target_link_libraries(astro_server PRIVATE ws2_32)
# endif()

# Set target properties (temporarily disabled)
# set_target_properties(astro_server PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED ON
# )

# Client application
add_executable(astro_client client_app.cpp)
target_link_libraries(astro_client PRIVATE AstroComm::Client AstroComm::Core ${FMT_LIBRARIES})
if(WIN32)
    target_link_libraries(astro_client PRIVATE ws2_32)
endif()

set_target_properties(astro_client PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Device applications
add_executable(astro_telescope telescope_device.cpp)
target_link_libraries(astro_telescope PRIVATE AstroComm::Device AstroComm::Core ${FMT_LIBRARIES})
if(WIN32)
    target_link_libraries(astro_telescope PRIVATE ws2_32)
endif()

set_target_properties(astro_telescope PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_executable(astro_camera camera_device.cpp)
target_link_libraries(astro_camera PRIVATE AstroComm::Device AstroComm::Core ${FMT_LIBRARIES})
if(WIN32)
    target_link_libraries(astro_camera PRIVATE ws2_32)
endif()

set_target_properties(astro_camera PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_executable(astro_focuser focuser_device.cpp)
target_link_libraries(astro_focuser PRIVATE AstroComm::Device AstroComm::Core ${FMT_LIBRARIES})
if(WIN32)
    target_link_libraries(astro_focuser PRIVATE ws2_32)
endif()

set_target_properties(astro_focuser PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_executable(astro_guider guider_device.cpp)
target_link_libraries(astro_guider PRIVATE AstroComm::Device AstroComm::Core ${FMT_LIBRARIES})
if(WIN32)
    target_link_libraries(astro_guider PRIVATE ws2_32)
endif()

set_target_properties(astro_guider PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_executable(astro_rotator rotator_device.cpp)
target_link_libraries(astro_rotator PRIVATE AstroComm::Device AstroComm::Core ${FMT_LIBRARIES})
if(WIN32)
    target_link_libraries(astro_rotator PRIVATE ws2_32)
endif()

set_target_properties(astro_rotator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_executable(astro_solver solver_device.cpp)
target_link_libraries(astro_solver PRIVATE AstroComm::Device AstroComm::Core ${FMT_LIBRARIES})
if(WIN32)
    target_link_libraries(astro_solver PRIVATE ws2_32)
endif()

set_target_properties(astro_solver PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_executable(astro_switch switch_device.cpp)
target_link_libraries(astro_switch PRIVATE AstroComm::Device AstroComm::Core ${FMT_LIBRARIES})
if(WIN32)
    target_link_libraries(astro_switch PRIVATE ws2_32)
endif()

set_target_properties(astro_switch PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Installation rules for applications
install(TARGETS
    # astro_server  # Temporarily disabled
    astro_client
    astro_telescope
    astro_camera
    astro_focuser 
    astro_guider 
    astro_rotator 
    astro_solver 
    astro_switch
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

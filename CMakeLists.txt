cmake_minimum_required(VERSION 3.10)
project(AstroDeviceProtocol VERSION 1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 寻找依赖库
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# 第三方库设置
include(FetchContent)

# 添加nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# 添加Crow
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.0+5
)
FetchContent_MakeAvailable(crow)

# 添加base64
FetchContent_Declare(
    cpp-base64
    GIT_REPOSITORY https://github.com/ReneNyffenegger/cpp-base64.git
    GIT_TAG master
)
FetchContent_MakeAvailable(cpp-base64)

# 添加头文件路径
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${cpp-base64_SOURCE_DIR}
)

# 源文件分组
file(GLOB COMMON_SOURCES "src/common/*.cpp")
file(GLOB SERVER_SOURCES "src/server/*.cpp")
file(GLOB DEVICE_SOURCES "src/device/*.cpp")
file(GLOB CLIENT_SOURCES "src/client/*.cpp")

# 公共库
add_library(astrocomm STATIC
    ${COMMON_SOURCES}
    ${SERVER_SOURCES}
    ${DEVICE_SOURCES}
    ${CLIENT_SOURCES}
    ${cpp-base64_SOURCE_DIR}/base64.cpp
)

target_link_libraries(astrocomm
    PRIVATE
    Threads::Threads
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    Crow::Crow
)

# 服务器应用程序
add_executable(astro_server apps/server_main.cpp)
target_link_libraries(astro_server PRIVATE astrocomm)

# 设备模拟器
add_executable(telescope_device apps/telescope_device.cpp)
target_link_libraries(telescope_device PRIVATE astrocomm)

# 客户端应用程序
add_executable(astro_client apps/client_app.cpp)
target_link_libraries(astro_client PRIVATE astrocomm)

# 安装规则
install(TARGETS astro_server telescope_device astro_client
    RUNTIME DESTINATION bin
)

FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.10.4
)
FetchContent_MakeAvailable(pybind11)

# 查找Python解释器
find_package(Python COMPONENTS Interpreter Development REQUIRED)
message(STATUS "Found Python: ${Python_VERSION}")

# Python模块
pybind11_add_module(pyastrodevice src/python/pyastrodevice.cpp)
target_link_libraries(pyastrodevice PRIVATE astrocomm)

# 安装Python模块
install(TARGETS pyastrodevice
    LIBRARY DESTINATION ${Python_SITEARCH}
)
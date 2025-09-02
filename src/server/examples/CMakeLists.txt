# Hydrogen Server Examples - Reorganized Architecture
# Demonstrates usage of the new layered server architecture

cmake_minimum_required(VERSION 3.15)

message(STATUS "Configuring Hydrogen Server examples (reorganized architecture)...")

# =============================================================================
# Example Applications
# =============================================================================

# Multi-protocol server example
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../examples/multi_protocol_server_example.cpp")
    add_executable(multi_protocol_server_example
        ../../../examples/multi_protocol_server_example.cpp
    )
    
    target_link_libraries(multi_protocol_server_example
        PRIVATE
            hydrogen_server
            hydrogen_core
    )

    # Link spdlog if available
    if(TARGET spdlog::spdlog)
        target_link_libraries(multi_protocol_server_example PRIVATE spdlog::spdlog)
    endif()
    
    set_target_properties(multi_protocol_server_example PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    # Install example
    install(TARGETS multi_protocol_server_example
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
    )
    
    message(STATUS "  - Multi-protocol server example configured")
endif()

# MQTT broker example
add_executable(mqtt_broker_example mqtt_broker_example.cpp)
target_link_libraries(mqtt_broker_example
    PRIVATE
        hydrogen_server
        hydrogen_core
)

# gRPC server example
if(Protobuf_FOUND AND gRPC_FOUND)
    add_executable(grpc_server_example grpc_server_example.cpp)
    target_link_libraries(grpc_server_example
        PRIVATE
            hydrogen_server
            hydrogen_core
    )
    message(STATUS "  - gRPC server example configured")
endif()

# ZeroMQ server example
add_executable(zmq_server_example zmq_server_example.cpp)
target_link_libraries(zmq_server_example
    PRIVATE
        hydrogen_server
        hydrogen_core
)

# Enhanced device manager example
add_executable(enhanced_device_manager_example enhanced_device_manager_example.cpp)
target_link_libraries(enhanced_device_manager_example
    PRIVATE
        hydrogen_server
        hydrogen_core
)

# Set properties for all examples
set(EXAMPLE_TARGETS
    mqtt_broker_example
    zmq_server_example
    enhanced_device_manager_example
)

if(Protobuf_FOUND AND gRPC_FOUND)
    list(APPEND EXAMPLE_TARGETS grpc_server_example)
endif()

foreach(TARGET ${EXAMPLE_TARGETS})
    set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    install(TARGETS ${TARGET}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
    )
endforeach()

# =============================================================================
# Example Source Files
# =============================================================================

# Create example source files if they don't exist
set(MQTT_EXAMPLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mqtt_broker_example.cpp")
if(NOT EXISTS ${MQTT_EXAMPLE_SOURCE})
    file(WRITE ${MQTT_EXAMPLE_SOURCE}
"#include <hydrogen/server/mqtt_broker.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace hydrogen::server;
using namespace hydrogen::core;

int main() {
    std::cout << \"MQTT Broker Example\" << std::endl;
    
    MqttConfig config;
    config.brokerHost = \"localhost\";
    config.brokerPort = 1883;
    
    auto broker = MqttBrokerFactory::createBroker(config);
    
    broker->setMessageHandler([](const MqttMessage& message) {
        std::cout << \"Received: \" << message.payload << \" on topic: \" << message.topic << std::endl;
    });
    
    if (broker->start()) {
        std::cout << \"MQTT Broker started on port 1883\" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        broker->stop();
    }
    
    return 0;
}
")
endif()

set(ZMQ_EXAMPLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/zmq_server_example.cpp")
if(NOT EXISTS ${ZMQ_EXAMPLE_SOURCE})
    file(WRITE ${ZMQ_EXAMPLE_SOURCE}
"#include <hydrogen/server/zmq_server.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace hydrogen::server;
using namespace hydrogen::core;

int main() {
    std::cout << \"ZeroMQ Server Example\" << std::endl;
    
    ZmqConfig config;
    config.bindAddress = \"tcp://*:5555\";
    config.socketType = static_cast<int>(ZmqCommunicator::SocketType::REP);
    
    auto server = ZmqServerFactory::createServer(config);
    
    if (server->start()) {
        std::cout << \"ZeroMQ Server started on port 5555\" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        server->stop();
    }
    
    return 0;
}
")
endif()

set(DEVICE_MANAGER_EXAMPLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/enhanced_device_manager_example.cpp")
if(NOT EXISTS ${DEVICE_MANAGER_EXAMPLE_SOURCE})
    file(WRITE ${DEVICE_MANAGER_EXAMPLE_SOURCE}
"#include <hydrogen/core/enhanced_device_manager.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace hydrogen::core;

int main() {
    std::cout << \"Enhanced Device Manager Example\" << std::endl;
    
    auto manager = EnhancedDeviceManagerFactory::createManagerWithHealthMonitoring();
    
    // Create device group
    std::string groupId = manager->createDeviceGroup(\"Test Sensors\", \"Example sensor group\");
    
    // Register some devices
    DeviceInfo sensor1;
    sensor1.deviceId = \"sensor_001\";
    sensor1.deviceType = \"temperature_sensor\";
    sensor1.deviceName = \"Temperature Sensor 1\";
    manager->registerDevice(sensor1);
    
    manager->addDeviceToGroup(\"sensor_001\", groupId);
    
    std::cout << \"Device manager running with health monitoring...\" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    auto stats = manager->getEnhancedStatistics();
    std::cout << \"Health checks performed: \" << stats[\"healthChecksPerformed\"] << std::endl;
    
    return 0;
}
")
endif()

if(Protobuf_FOUND AND gRPC_FOUND)
    set(GRPC_EXAMPLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/grpc_server_example.cpp")
    if(NOT EXISTS ${GRPC_EXAMPLE_SOURCE})
        file(WRITE ${GRPC_EXAMPLE_SOURCE}
"#include <hydrogen/server/grpc_server.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace hydrogen::server;
using namespace hydrogen::core;

int main() {
    std::cout << \"gRPC Server Example\" << std::endl;
    
    GrpcConfig config;
    config.serverAddress = \"localhost:50051\";
    config.useTls = false;
    
    auto server = GrpcServerFactory::createServer(config);
    
    if (server->start()) {
        std::cout << \"gRPC Server started on port 50051\" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        server->stop();
    }
    
    return 0;
}
")
    endif()
endif()

message(STATUS \"Hydrogen Server examples configured successfully\")

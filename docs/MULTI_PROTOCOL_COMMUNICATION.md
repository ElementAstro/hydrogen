# Multi-Protocol Communication System

## Overview

The AstroComm Multi-Protocol Communication System provides a comprehensive framework for device communication using multiple protocols including MQTT, gRPC, ZeroMQ, WebSocket, and HTTP. This system enables seamless integration of diverse devices and services with automatic protocol bridging, load balancing, and failover capabilities.

## Supported Protocols

### 1. MQTT (Message Queuing Telemetry Transport)
- **Use Case**: IoT device communication, telemetry data, pub/sub messaging
- **Features**: QoS levels, retained messages, topic-based routing, client authentication
- **Port**: 1883 (default), 8883 (TLS)

### 2. gRPC (Google Remote Procedure Call)
- **Use Case**: High-performance RPC, streaming data, service-to-service communication
- **Features**: Bidirectional streaming, protocol buffers, reflection, authentication
- **Port**: 50051 (default)

### 3. ZeroMQ
- **Use Case**: High-performance messaging, distributed systems, message queuing
- **Features**: Multiple socket patterns (REQ/REP, PUB/SUB, PUSH/PULL), load balancing
- **Port**: 5555 (default)

### 4. WebSocket
- **Use Case**: Real-time web applications, browser-based clients
- **Features**: Full-duplex communication, low latency, web browser compatibility
- **Port**: 8080 (default)

### 5. HTTP/REST
- **Use Case**: Web APIs, RESTful services, standard web communication
- **Features**: RESTful endpoints, JSON payloads, CORS support
- **Port**: 8080 (default)

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                Enhanced Device Server                        │
├─────────────────────────────────────────────────────────────┤
│  Protocol Servers:                                          │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │ MQTT Broker │ │ gRPC Server │ │ ZMQ Server  │           │
│  └─────────────┘ └─────────────┘ └─────────────┘           │
│  ┌─────────────┐ ┌─────────────┐                           │
│  │ WebSocket   │ │ HTTP Server │                           │
│  │ Server      │ │             │                           │
│  └─────────────┘ └─────────────┘                           │
├─────────────────────────────────────────────────────────────┤
│  Protocol Bridging & Message Routing                        │
├─────────────────────────────────────────────────────────────┤
│  Enhanced Device Manager                                     │
│  - Health Monitoring    - Device Discovery                  │
│  - Device Grouping      - Configuration Templates           │
│  - Bulk Operations      - Advanced Search                   │
└─────────────────────────────────────────────────────────────┘
```

## Quick Start

### 1. Basic Server Setup

```cpp
#include <astrocomm/server/enhanced_device_server.h>

using namespace astrocomm::server;

int main() {
    // Create server with default configuration
    auto server = EnhancedDeviceServerFactory::createServerWithDefaults();
    
    // Start the server
    if (server->start()) {
        std::cout << "Multi-protocol server started successfully!" << std::endl;
        
        // Keep server running
        std::this_thread::sleep_for(std::chrono::hours(24));
        
        server->stop();
    }
    
    return 0;
}
```

### 2. Custom Protocol Configuration

```cpp
// Create server with specific protocols
std::vector<CommunicationProtocol> protocols = {
    CommunicationProtocol::MQTT,
    CommunicationProtocol::GRPC,
    CommunicationProtocol::ZEROMQ
};

auto server = EnhancedDeviceServerFactory::createMultiProtocolServer(protocols);

// Configure MQTT
json mqttConfig = {
    {"brokerHost", "localhost"},
    {"brokerPort", 1883},
    {"useTls", false},
    {"keepAliveInterval", 60}
};
server->enableProtocol(CommunicationProtocol::MQTT, mqttConfig);

// Configure gRPC
json grpcConfig = {
    {"serverAddress", "localhost:50051"},
    {"useTls", false},
    {"enableReflection", true}
};
server->enableProtocol(CommunicationProtocol::GRPC, grpcConfig);

server->start();
```

### 3. Protocol Bridging

```cpp
// Enable message bridging between protocols
server->enableProtocolBridging(
    CommunicationProtocol::MQTT, 
    CommunicationProtocol::WEBSOCKET
);

// Messages received via MQTT will be forwarded to WebSocket clients
```

## Protocol-Specific Usage

### MQTT Usage

```cpp
// Setup MQTT broker
MqttConfig config;
config.brokerHost = "localhost";
config.brokerPort = 1883;
config.useTls = false;

auto broker = MqttBrokerFactory::createBroker(config);

// Set event handlers
broker->setMessageHandler([](const MqttMessage& message) {
    std::cout << "Received: " << message.payload << " on topic: " << message.topic << std::endl;
});

broker->start();

// Publish messages
broker->publish("device/sensor1/data", "{\"temperature\": 25.5}", 1, false);

// Subscribe to topics
broker->subscribe("client1", "device/+/data", 1);
```

### gRPC Usage

```cpp
// Setup gRPC server
GrpcConfig config;
config.serverAddress = "localhost:50051";
config.useTls = false;

auto grpcServer = GrpcServerFactory::createServer(config);

// Set event handlers
grpcServer->setMessageHandler([](const std::string& clientId, const std::string& method, const std::string& data) {
    std::cout << "gRPC call: " << method << " from " << clientId << std::endl;
});

grpcServer->start();

// Send messages to clients
grpcServer->sendToClient("client1", "notification", "Hello from server");
```

### ZeroMQ Usage

```cpp
// Setup ZeroMQ server
ZmqConfig config;
config.bindAddress = "tcp://*:5555";
config.socketType = static_cast<int>(ZmqCommunicator::SocketType::REP);

auto zmqServer = ZmqServerFactory::createServer(config);

// Create and bind socket
std::string socketId = zmqServer->createSocket(ZmqCommunicator::SocketType::REP, "tcp://*:5555");
zmqServer->bindSocket(socketId, "tcp://*:5555");

zmqServer->start();

// Send messages
zmqServer->sendMessage(socketId, "Hello ZeroMQ");
```

## Enhanced Device Management

### Health Monitoring

```cpp
auto deviceManager = EnhancedDeviceManagerFactory::createManagerWithHealthMonitoring();

// Set health change handler
deviceManager->setHealthChangeHandler([](const std::string& deviceId, HealthStatus oldStatus, HealthStatus newStatus) {
    std::cout << "Device " << deviceId << " health changed from " 
              << static_cast<int>(oldStatus) << " to " << static_cast<int>(newStatus) << std::endl;
});

// Get unhealthy devices
auto unhealthyDevices = deviceManager->getUnhealthyDevices();
for (const auto& health : unhealthyDevices) {
    std::cout << "Unhealthy device: " << health.deviceId 
              << " Status: " << static_cast<int>(health.status) << std::endl;
}
```

### Device Grouping

```cpp
// Create device group
std::string groupId = deviceManager->createDeviceGroup("Sensors", "All sensor devices");

// Add devices to group
deviceManager->addDeviceToGroup("sensor1", groupId);
deviceManager->addDeviceToGroup("sensor2", groupId);

// Get group devices
auto groupDevices = deviceManager->getGroupDevices(groupId);
```

### Configuration Templates

```cpp
// Create configuration template
json baseConfig = {
    {"sampling_rate", 1000},
    {"precision", "high"},
    {"enabled", true}
};

std::string templateId = deviceManager->createConfigTemplate("Sensor Template", "sensor", baseConfig);

// Register device with template
std::unordered_map<std::string, json> variables = {
    {"sampling_rate", 2000}
};

deviceManager->registerDeviceWithTemplate("new_sensor", templateId, variables);
```

### Bulk Operations

```cpp
std::vector<std::string> deviceIds = {"device1", "device2", "device3"};

// Bulk configuration update
json newConfig = {{"new_setting", "value"}};
std::string operationId = deviceManager->startBulkOperation("update_config", deviceIds, newConfig);

// Monitor operation status
json status = deviceManager->getBulkOperationStatus(operationId);
std::cout << "Operation progress: " << status["completed"] << "/" << status["totalDevices"] << std::endl;
```

## Event Handling

```cpp
// Set connection handler
server->setConnectionHandler([](const std::string& clientId, CommunicationProtocol protocol, bool connected) {
    std::cout << "Client " << clientId << " " << (connected ? "connected" : "disconnected") 
              << " via " << ProtocolServerUtils::protocolToString(protocol) << std::endl;
});

// Set message handler
server->setMessageHandler([](const std::string& clientId, CommunicationProtocol protocol, const std::string& message) {
    std::cout << "Message from " << clientId << " via " 
              << ProtocolServerUtils::protocolToString(protocol) << ": " << message << std::endl;
});

// Set protocol status handler
server->setProtocolStatusHandler([](CommunicationProtocol protocol, bool running) {
    std::cout << "Protocol " << ProtocolServerUtils::protocolToString(protocol) 
              << " is " << (running ? "running" : "stopped") << std::endl;
});
```

## Monitoring and Metrics

```cpp
// Enable metrics collection
server->enableMetricsCollection(true);
server->enableRealTimeMonitoring(std::chrono::milliseconds(1000));

// Get server metrics
json serverMetrics = server->getServerMetrics();
std::cout << "Total connections: " << serverMetrics["total_connections"] << std::endl;
std::cout << "Messages received: " << serverMetrics["total_messages_received"] << std::endl;

// Get protocol-specific metrics
json mqttMetrics = server->getProtocolMetrics(CommunicationProtocol::MQTT);
std::cout << "MQTT connections: " << mqttMetrics["connections"] << std::endl;

// Get all metrics
json allMetrics = server->getAllMetrics();
```

## Configuration Management

```cpp
// Save server configuration
server->saveConfiguration("server_config.json");

// Load configuration
server->loadConfiguration("server_config.json");

// Update protocol configuration
json newMqttConfig = {
    {"brokerHost", "mqtt.example.com"},
    {"brokerPort", 8883},
    {"useTls", true}
};
server->updateProtocolConfig(CommunicationProtocol::MQTT, newMqttConfig);
```

## Best Practices

1. **Protocol Selection**: Choose protocols based on your use case:
   - MQTT for IoT devices and telemetry
   - gRPC for high-performance service communication
   - ZeroMQ for distributed systems and message queuing
   - WebSocket for real-time web applications
   - HTTP for standard web APIs

2. **Security**: Always enable TLS/SSL in production environments
3. **Monitoring**: Enable metrics collection and health monitoring
4. **Resource Management**: Use device grouping for efficient management
5. **Configuration**: Use templates for consistent device configuration
6. **Error Handling**: Implement proper error handling and logging

## Troubleshooting

### Common Issues

1. **Port Conflicts**: Ensure different protocols use different ports
2. **Authentication Failures**: Check client credentials and authentication handlers
3. **Connection Timeouts**: Verify network connectivity and firewall settings
4. **Memory Usage**: Monitor device count and message throughput

### Debugging

```cpp
// Enable debug logging
spdlog::set_level(spdlog::level::debug);

// Check server status
json status = server->getStatus();
std::cout << "Server running: " << status["running"] << std::endl;

// Check protocol status
for (const auto& protocol : server->getEnabledProtocols()) {
    bool running = false;
    switch (protocol) {
        case CommunicationProtocol::MQTT:
            running = server->isMqttBrokerRunning();
            break;
        case CommunicationProtocol::GRPC:
            running = server->isGrpcServerRunning();
            break;
        case CommunicationProtocol::ZEROMQ:
            running = server->isZmqServerRunning();
            break;
    }
    std::cout << ProtocolServerUtils::protocolToString(protocol) 
              << " running: " << running << std::endl;
}
```

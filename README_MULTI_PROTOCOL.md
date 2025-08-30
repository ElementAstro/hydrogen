# AstroComm Multi-Protocol Communication System

## 🚀 Overview

The AstroComm Multi-Protocol Communication System is a comprehensive framework that enables seamless device communication across multiple protocols including MQTT, gRPC, ZeroMQ, WebSocket, and HTTP. This system provides a unified interface for managing diverse astronomical devices with advanced features like protocol bridging, load balancing, health monitoring, and device discovery.

## ✨ Key Features

### Multi-Protocol Support
- **MQTT**: IoT device communication with QoS levels and topic-based routing
- **gRPC**: High-performance RPC with bidirectional streaming
- **ZeroMQ**: Distributed messaging with multiple socket patterns
- **WebSocket**: Real-time web communication
- **HTTP/REST**: Standard web API endpoints

### Advanced Device Management
- **Health Monitoring**: Real-time device health tracking and alerting
- **Device Discovery**: Automatic network device discovery via UDP multicast, mDNS
- **Device Grouping**: Logical organization of devices for bulk operations
- **Configuration Templates**: Reusable device configuration patterns
- **Bulk Operations**: Efficient management of multiple devices

### Protocol Integration
- **Protocol Bridging**: Seamless message routing between different protocols
- **Load Balancing**: Distribute traffic across multiple protocol endpoints
- **Failover Support**: Automatic fallback to backup protocols
- **Unified Metrics**: Comprehensive monitoring across all protocols

## 🏗️ Architecture

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

## 🚀 Quick Start

### 1. Build the System

```bash
# Clone the repository
git clone https://github.com/your-org/hydrogen.git
cd hydrogen

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DHYDROGEN_BUILD_TESTS=ON -DHYDROGEN_BUILD_EXAMPLES=ON

# Build the project
cmake --build . --parallel

# Run tests
ctest --parallel
```

### 2. Run the Multi-Protocol Server

```bash
# Run the example server
./bin/examples/multi_protocol_server_example
```

### 3. Connect Clients

```bash
# MQTT client
mosquitto_pub -h localhost -t 'astrocomm/test' -m 'Hello World'
mosquitto_sub -h localhost -t 'astrocomm/+'

# WebSocket (using wscat)
wscat -c ws://localhost:8080/ws

# HTTP REST API
curl http://localhost:8080/api/status
```

## 📚 Usage Examples

### Basic Server Setup

```cpp
#include <astrocomm/server/enhanced_device_server.h>

// Create server with default protocols
auto server = EnhancedDeviceServerFactory::createServerWithDefaults();

// Start the server
server->start();

// Server now runs MQTT (1883), WebSocket (8080), and HTTP (8080)
```

### Custom Protocol Configuration

```cpp
// Create server
auto server = std::make_unique<EnhancedDeviceServer>();

// Configure MQTT
json mqttConfig = {
    {"brokerHost", "localhost"},
    {"brokerPort", 1883},
    {"useTls", false}
};
server->enableProtocol(CommunicationProtocol::MQTT, mqttConfig);

// Configure gRPC
json grpcConfig = {
    {"serverAddress", "localhost:50051"},
    {"enableReflection", true}
};
server->enableProtocol(CommunicationProtocol::GRPC, grpcConfig);

// Enable protocol bridging
server->enableProtocolBridging(
    CommunicationProtocol::MQTT, 
    CommunicationProtocol::WEBSOCKET
);

server->start();
```

### Enhanced Device Management

```cpp
// Get enhanced device manager
auto deviceManager = server->getEnhancedDeviceManager();

// Start health monitoring
deviceManager->startHealthMonitoring();

// Create device group
std::string groupId = deviceManager->createDeviceGroup("Sensors");

// Register device with template
json sensorConfig = {{"sampling_rate", 1000}};
std::string templateId = deviceManager->createConfigTemplate(
    "Standard Sensor", "sensor", sensorConfig);

deviceManager->registerDeviceWithTemplate("sensor_001", templateId);
deviceManager->addDeviceToGroup("sensor_001", groupId);

// Bulk operations
std::vector<std::string> deviceIds = {"sensor_001", "sensor_002"};
json newConfig = {{"enabled", true}};
deviceManager->startBulkOperation("update_config", deviceIds, newConfig);
```

## 🔧 Configuration

### Server Configuration File

```json
{
  "server": {
    "name": "AstroComm Multi-Protocol Server",
    "version": "1.0.0",
    "metrics_enabled": true,
    "monitoring_enabled": true
  },
  "protocols": {
    "mqtt": {
      "enabled": true,
      "configuration": {
        "brokerHost": "localhost",
        "brokerPort": 1883,
        "useTls": false,
        "keepAliveInterval": 60
      }
    },
    "grpc": {
      "enabled": true,
      "configuration": {
        "serverAddress": "localhost:50051",
        "useTls": false,
        "enableReflection": true
      }
    },
    "zeromq": {
      "enabled": true,
      "configuration": {
        "bindAddress": "tcp://*:5555",
        "socketType": 1
      }
    }
  }
}
```

### Device Discovery Configuration

```cpp
DeviceDiscoveryConfig discoveryConfig;
discoveryConfig.enableUdpMulticast = true;
discoveryConfig.multicastAddress = "239.255.255.250";
discoveryConfig.multicastPort = 1900;
discoveryConfig.enableMdns = true;
discoveryConfig.discoveryInterval = std::chrono::milliseconds(30000);

deviceManager->startDeviceDiscovery(discoveryConfig);
```

## 📊 Monitoring and Metrics

### Real-time Metrics

```cpp
// Enable metrics collection
server->enableMetricsCollection(true);
server->enableRealTimeMonitoring(std::chrono::milliseconds(1000));

// Get server metrics
json metrics = server->getAllMetrics();
std::cout << "Total connections: " << metrics["server"]["total_connections"] << std::endl;
std::cout << "Messages received: " << metrics["server"]["total_messages_received"] << std::endl;

// Protocol-specific metrics
json mqttMetrics = server->getProtocolMetrics(CommunicationProtocol::MQTT);
std::cout << "MQTT connections: " << mqttMetrics["connections"] << std::endl;
```

### Health Monitoring

```cpp
// Set health change handler
deviceManager->setHealthChangeHandler([](const std::string& deviceId, 
                                        HealthStatus oldStatus, 
                                        HealthStatus newStatus) {
    std::cout << "Device " << deviceId << " health changed from " 
              << static_cast<int>(oldStatus) << " to " 
              << static_cast<int>(newStatus) << std::endl;
});

// Get unhealthy devices
auto unhealthyDevices = deviceManager->getUnhealthyDevices();
for (const auto& health : unhealthyDevices) {
    std::cout << "Unhealthy device: " << health.deviceId 
              << " Status: " << static_cast<int>(health.status) << std::endl;
}
```

## 🧪 Testing

### Run Unit Tests

```bash
# Build and run all tests
cd build
ctest --parallel --output-on-failure

# Run specific test suites
ctest -R "EnhancedDeviceServer" --verbose
ctest -R "MqttBroker" --verbose
ctest -R "EnhancedDeviceManager" --verbose
```

### Integration Testing

```bash
# Start the test server
./bin/examples/multi_protocol_server_example &

# Run integration tests
python3 tests/integration/test_multi_protocol.py

# Stop the server
pkill multi_protocol_server_example
```

## 📖 API Documentation

### Protocol Endpoints

- **MQTT**: `mqtt://localhost:1883`
- **gRPC**: `localhost:50051`
- **ZeroMQ**: `tcp://localhost:5555`
- **WebSocket**: `ws://localhost:8080/ws`
- **HTTP REST**: `http://localhost:8080/api/`

### MQTT Topics

- Device commands: `astrocomm/device/{device_id}/command`
- Device status: `astrocomm/device/{device_id}/status`
- Device data: `astrocomm/device/{device_id}/data/{data_type}`
- System health: `astrocomm/system/health`
- Discovery: `astrocomm/discovery`

### HTTP REST Endpoints

- `GET /api/status` - Server status
- `GET /api/devices` - List all devices
- `POST /api/devices` - Register new device
- `GET /api/devices/{id}` - Get device info
- `PUT /api/devices/{id}` - Update device
- `DELETE /api/devices/{id}` - Remove device
- `GET /api/metrics` - Server metrics

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🆘 Support

- **Documentation**: [docs/MULTI_PROTOCOL_COMMUNICATION.md](docs/MULTI_PROTOCOL_COMMUNICATION.md)
- **Issues**: [GitHub Issues](https://github.com/your-org/hydrogen/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-org/hydrogen/discussions)

## 🎯 Roadmap

- [ ] WebRTC support for peer-to-peer communication
- [ ] CoAP support for constrained devices
- [ ] Advanced security features (OAuth2, JWT)
- [ ] Kubernetes deployment manifests
- [ ] Performance benchmarking suite
- [ ] Protocol adapter plugins
- [ ] Cloud integration (AWS IoT, Azure IoT Hub)

---

**Built with ❤️ for the astronomical community**

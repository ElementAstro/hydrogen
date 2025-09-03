# Multi-Protocol Communication Guide

## Overview

Hydrogen's multi-protocol communication system allows devices to communicate simultaneously through multiple protocols, providing unprecedented flexibility, reliability, and performance optimization. This guide covers how to use and configure the multi-protocol system.

## Supported Protocols

### Core Protocols

- **WebSocket**: Real-time bidirectional communication with low latency
- **gRPC**: High-performance RPC with strong typing and streaming support
- **MQTT**: Lightweight publish-subscribe messaging for IoT integration
- **ZeroMQ**: High-performance asynchronous messaging library
- **HTTP**: RESTful API for simple request-response operations

### Additional Protocols

- **TCP**: Raw socket communication for custom protocols
- **UDP**: Connectionless communication for high-speed data transfer
- **Serial**: Direct serial port communication for legacy devices
- **USB**: Direct USB device communication

## Key Features

### Simultaneous Protocol Support

```cpp
// Create a multi-protocol communication manager
MultiProtocolCommunicationManager manager("device_001");

// Configure multiple protocols simultaneously
manager.configureProtocol(CommunicationProtocol::WEBSOCKET, {
    {"host", "localhost"},
    {"port", 8080}
});

manager.configureProtocol(CommunicationProtocol::GRPC, {
    {"host", "localhost"},
    {"port", 9090}
});

manager.configureProtocol(CommunicationProtocol::MQTT, {
    {"broker", "localhost"},
    {"port", 1883},
    {"topic_prefix", "hydrogen/devices"}
});

// Connect to all configured protocols
manager.connectAll();
```

### Automatic Protocol Selection

The system automatically selects the best protocol based on:
- **Message Type**: Commands vs. streaming data vs. events
- **Network Conditions**: Latency, bandwidth, reliability
- **Protocol Availability**: Automatic failover to available protocols
- **Performance Requirements**: Throughput vs. latency optimization

```cpp
// Send message with automatic protocol selection
CommunicationMessage message;
message.command = "get_temperature";
message.payload = json{{"sensor", "primary"}};

// System automatically chooses the best available protocol
bool success = manager.sendMessage(message);
```

### Protocol Fallback and Load Balancing

```cpp
// Configure fallback chain
manager.setFallbackChain({
    CommunicationProtocol::GRPC,      // Primary: High performance
    CommunicationProtocol::WEBSOCKET, // Secondary: Reliable
    CommunicationProtocol::HTTP       // Fallback: Universal
});

// Configure load balancing
manager.enableLoadBalancing(true);
manager.setLoadBalancingStrategy(LoadBalancingStrategy::ROUND_ROBIN);
```

## Protocol-Specific Configuration

### WebSocket Configuration

```cpp
json websocketConfig = {
    {"host", "localhost"},
    {"port", 8080},
    {"path", "/ws"},
    {"reconnect_interval", 5000},
    {"max_reconnect_attempts", 10},
    {"ping_interval", 30000},
    {"compression", true}
};

manager.configureProtocol(CommunicationProtocol::WEBSOCKET, websocketConfig);
```

### gRPC Configuration

```cpp
json grpcConfig = {
    {"host", "localhost"},
    {"port", 9090},
    {"use_tls", false},
    {"max_message_size", 4194304},
    {"keepalive_time", 30000},
    {"keepalive_timeout", 5000}
};

manager.configureProtocol(CommunicationProtocol::GRPC, grpcConfig);
```

### MQTT Configuration

```cpp
json mqttConfig = {
    {"broker", "localhost"},
    {"port", 1883},
    {"client_id", "hydrogen_device_001"},
    {"topic_prefix", "hydrogen/devices"},
    {"qos", 1},
    {"retain", false},
    {"clean_session", true},
    {"keepalive", 60}
};

manager.configureProtocol(CommunicationProtocol::MQTT, mqttConfig);
```

### ZeroMQ Configuration

```cpp
json zmqConfig = {
    {"pattern", "REQ-REP"},  // or "PUB-SUB", "PUSH-PULL"
    {"bind_address", "tcp://*:5555"},
    {"connect_address", "tcp://localhost:5555"},
    {"high_water_mark", 1000},
    {"linger", 0}
};

manager.configureProtocol(CommunicationProtocol::ZEROMQ, zmqConfig);
```

## Message Routing and Broadcasting

### Selective Protocol Messaging

```cpp
// Send to specific protocol
manager.sendMessage(message, CommunicationProtocol::GRPC);

// Send to multiple protocols
std::vector<CommunicationProtocol> protocols = {
    CommunicationProtocol::WEBSOCKET,
    CommunicationProtocol::MQTT
};
manager.sendMessage(message, protocols);
```

### Message Broadcasting

```cpp
// Broadcast to all connected protocols
manager.broadcastMessage(message);

// Broadcast with protocol filtering
manager.broadcastMessage(message, [](CommunicationProtocol protocol) {
    return protocol != CommunicationProtocol::HTTP; // Exclude HTTP
});
```

## Performance Optimization

### Connection Pooling

```cpp
// Enable connection pooling
manager.enableConnectionPooling(true);
manager.setMaxConnectionsPerProtocol(5);
manager.setConnectionIdleTimeout(300000); // 5 minutes
```

### Circuit Breaker Pattern

```cpp
// Configure circuit breaker
CircuitBreakerConfig cbConfig;
cbConfig.failureThreshold = 5;
cbConfig.recoveryTimeout = 30000;
cbConfig.halfOpenMaxCalls = 3;

manager.configureCircuitBreaker(CommunicationProtocol::GRPC, cbConfig);
```

### Message Compression

```cpp
// Enable compression for specific protocols
manager.enableCompression(CommunicationProtocol::WEBSOCKET, true);
manager.enableCompression(CommunicationProtocol::HTTP, true);
```

## Error Handling and Recovery

### Protocol-Specific Error Handling

```cpp
// Set error handlers for each protocol
manager.setErrorHandler(CommunicationProtocol::WEBSOCKET, 
    [](const std::string& error) {
        SPDLOG_ERROR("WebSocket error: {}", error);
        // Handle WebSocket-specific errors
    });

manager.setErrorHandler(CommunicationProtocol::GRPC,
    [](const std::string& error) {
        SPDLOG_ERROR("gRPC error: {}", error);
        // Handle gRPC-specific errors
    });
```

### Automatic Recovery

```cpp
// Configure automatic recovery
manager.enableAutoRecovery(true);
manager.setRecoveryInterval(10000); // 10 seconds
manager.setMaxRecoveryAttempts(5);
```

## Monitoring and Statistics

### Protocol Statistics

```cpp
// Get statistics for all protocols
auto stats = manager.getStatistics();

for (const auto& [protocol, protocolStats] : stats) {
    std::cout << "Protocol: " << static_cast<int>(protocol) << std::endl;
    std::cout << "  Messages Sent: " << protocolStats.messagesSent << std::endl;
    std::cout << "  Messages Received: " << protocolStats.messagesReceived << std::endl;
    std::cout << "  Errors: " << protocolStats.errors << std::endl;
    std::cout << "  Uptime: " << protocolStats.uptime.count() << "ms" << std::endl;
}
```

### Health Monitoring

```cpp
// Check protocol health
for (auto protocol : manager.getConfiguredProtocols()) {
    bool isHealthy = manager.isProtocolHealthy(protocol);
    std::cout << "Protocol " << static_cast<int>(protocol) 
              << " is " << (isHealthy ? "healthy" : "unhealthy") << std::endl;
}
```

## Best Practices

### Protocol Selection Guidelines

1. **gRPC**: Use for high-performance, type-safe communication
2. **WebSocket**: Use for real-time bidirectional communication
3. **MQTT**: Use for event-driven architectures and IoT integration
4. **HTTP**: Use for simple request-response operations
5. **ZeroMQ**: Use for high-throughput, low-latency messaging

### Configuration Recommendations

1. **Always configure fallback protocols** for reliability
2. **Use connection pooling** for high-traffic applications
3. **Enable circuit breakers** to prevent cascade failures
4. **Monitor protocol health** and performance metrics
5. **Use compression** for large message payloads

### Error Handling Best Practices

1. **Implement protocol-specific error handlers**
2. **Use automatic recovery** with reasonable retry limits
3. **Log all communication errors** for debugging
4. **Implement graceful degradation** when protocols fail
5. **Monitor error rates** and adjust configurations accordingly

## Troubleshooting

### Common Issues

**Protocol Connection Failures**
- Check network connectivity and firewall settings
- Verify protocol-specific configuration parameters
- Check server availability and port accessibility

**Message Delivery Failures**
- Verify message format and serialization
- Check protocol-specific message size limits
- Monitor network conditions and latency

**Performance Issues**
- Enable connection pooling and compression
- Adjust protocol-specific timeout settings
- Monitor resource usage and optimize accordingly

### Debug Logging

```cpp
// Enable detailed protocol logging
manager.setLogLevel(LogLevel::DEBUG);

// This will log:
// - Protocol connection attempts and results
// - Message sending and receiving details
// - Error conditions and recovery attempts
// - Performance metrics and statistics
```

## Integration Examples

See the `examples/` directory for complete working examples:

- `examples/multi_protocol_server_example.cpp` - Multi-protocol server setup
- `examples/cpp/unified_client_example.cpp` - Client-side multi-protocol usage
- `examples/python/multi_protocol_example.py` - Python multi-protocol integration

# Troubleshooting Guide

## Overview

This guide covers common issues, solutions, and debugging techniques for the Hydrogen astronomical device communication framework.

## Build and Installation Issues

### CMake Configuration Problems

**Issue**: CMake fails to find dependencies
```
CMake Error: Could not find package XYZ
```

**Solutions**:
1. **vcpkg**: Ensure `VCPKG_ROOT` is set and vcpkg is properly installed
   ```bash
   export VCPKG_ROOT=/path/to/vcpkg
   cmake --preset default
   ```

2. **Conan**: Install dependencies first
   ```bash
   conan install . --build=missing
   cmake --preset conan-default
   ```

3. **FetchContent Fallback**: Enable automatic dependency download
   ```bash
   cmake -DHYDROGEN_ALLOW_FETCHCONTENT=ON -S . -B build
   ```

**Issue**: Package manager conflicts
```
Error: Multiple package managers detected
```

**Solution**: Specify the preferred package manager explicitly
```bash
cmake -DHYDROGEN_PREFER_VCPKG=ON -S . -B build
# or
cmake -DHYDROGEN_PREFER_CONAN=ON -S . -B build
```

### Compilation Errors

**Issue**: C++ standard version errors
```
error: 'std::optional' is not a member of 'std'
```

**Solution**: Ensure C++17 or later compiler
```bash
# Check compiler version
g++ --version
clang++ --version

# Specify C++ standard explicitly
cmake -DCMAKE_CXX_STANDARD=17 -S . -B build
```

**Issue**: Missing headers or libraries
```
fatal error: 'boost/asio.hpp' file not found
```

**Solution**: Install missing dependencies through package manager
```bash
# vcpkg
vcpkg install boost-asio

# Conan
conan install boost/1.82.0@
```

### Python Bindings Issues

**Issue**: pybind11 not found
```
CMake Error: Could not find pybind11
```

**Solution**: Install pybind11 or disable Python bindings
```bash
# Install pybind11
pip install pybind11

# Or disable Python bindings
cmake -DHYDROGEN_ENABLE_PYTHON_BINDINGS=OFF -S . -B build
```

**Issue**: Python module import errors
```python
ModuleNotFoundError: No module named 'pyhydrogen'
```

**Solution**: Build and install Python bindings
```bash
# Build Python bindings
cmake --build build --target pyhydrogen

# Install Python module
python python/setup.py install
```

## Runtime Issues

### Connection Problems

**Issue**: WebSocket connection failures
```
Error: Failed to connect to WebSocket server at localhost:8080
```

**Solutions**:
1. **Check server status**: Ensure the server is running
   ```bash
   netstat -an | grep 8080
   ```

2. **Firewall settings**: Check firewall rules
   ```bash
   # Windows
   netsh advfirewall firewall show rule name="Hydrogen"
   
   # Linux
   sudo ufw status
   ```

3. **Network connectivity**: Test basic connectivity
   ```bash
   telnet localhost 8080
   curl -I http://localhost:8080
   ```

**Issue**: gRPC connection failures
```
Error: gRPC connection failed: UNAVAILABLE
```

**Solutions**:
1. **Check gRPC server**: Verify gRPC server is running on correct port
2. **TLS configuration**: Check TLS/SSL settings
   ```cpp
   grpcConfig["use_tls"] = false;  // For local testing
   ```

3. **Port conflicts**: Ensure port is not in use by another service

### ASCOM/INDI Compatibility Issues

**Issue**: ASCOM devices not appearing in client software
```
No ASCOM devices found in device selection dialog
```

**Solutions**:
1. **ASCOM Platform**: Ensure ASCOM Platform is installed
   - Download from: https://ascom-standards.org/
   - Verify installation: Check Windows Registry under `HKEY_LOCAL_MACHINE\SOFTWARE\ASCOM`

2. **COM Registration**: Check COM registration
   ```bash
   # Run as Administrator
   regsvr32 /s hydrogen_ascom.dll
   ```

3. **Device Registration**: Verify device registration
   ```cpp
   // Enable ASCOM registration logging
   hydrogen::compatibility::setLogLevel(LogLevel::DEBUG);
   ```

**Issue**: INDI clients cannot connect
```
INDI client: Connection refused to localhost:7624
```

**Solutions**:
1. **INDI Server**: Check if INDI server is running
   ```bash
   ps aux | grep indiserver
   netstat -an | grep 7624
   ```

2. **Port Configuration**: Verify INDI port settings
   ```cpp
   IndiConfig indiConfig;
   indiConfig.port = 7624;  // Default INDI port
   ```

3. **Firewall**: Check firewall settings for INDI ports
   ```bash
   sudo ufw allow 7624
   ```

### Device Communication Issues

**Issue**: Device not responding to commands
```
Error: Device timeout - no response received
```

**Solutions**:
1. **Connection Status**: Check device connection
   ```cpp
   if (!device->isConnected()) {
       device->reconnect();
   }
   ```

2. **Command Format**: Verify command format
   ```cpp
   json command = {
       {"command", "get_status"},
       {"device_id", "cam1"},
       {"params", {}}
   };
   ```

3. **Network Latency**: Increase timeout values
   ```cpp
   device->setCommandTimeout(30000);  // 30 seconds
   ```

**Issue**: Property synchronization problems
```
Warning: Property values out of sync between protocols
```

**Solutions**:
1. **Enable Synchronization**: Ensure property sync is enabled
   ```cpp
   bridge->enablePropertySync(true);
   ```

2. **Check Mapping**: Verify property mappings
   ```cpp
   auto mappings = bridge->getPropertyMappings();
   for (const auto& mapping : mappings) {
       std::cout << mapping.internalName << " -> " 
                 << mapping.ascomName << " / " 
                 << mapping.indiName << std::endl;
   }
   ```

## Performance Issues

### High CPU Usage

**Issue**: Hydrogen process consuming excessive CPU
```
hydrogen process using 100% CPU
```

**Solutions**:
1. **Reduce Polling Frequency**: Adjust update intervals
   ```cpp
   device->setUpdateInterval(1000);  // 1 second instead of 100ms
   ```

2. **Optimize Logging**: Reduce log level in production
   ```cpp
   hydrogen::setLogLevel(LogLevel::WARNING);
   ```

3. **Connection Pooling**: Enable connection pooling
   ```cpp
   manager->enableConnectionPooling(true);
   manager->setMaxConnectionsPerProtocol(3);
   ```

### Memory Leaks

**Issue**: Memory usage continuously increasing
```
Memory usage growing over time
```

**Solutions**:
1. **Smart Pointers**: Ensure proper use of smart pointers
   ```cpp
   auto device = std::make_unique<Camera>("cam1", "ZWO", "ASI294");
   ```

2. **Event Handler Cleanup**: Remove event handlers properly
   ```cpp
   device->removeEventHandler(handlerId);
   ```

3. **Resource Management**: Implement proper RAII
   ```cpp
   class ResourceManager {
       ~ResourceManager() {
           cleanup();  // Ensure cleanup in destructor
       }
   };
   ```

### Network Performance

**Issue**: Slow network communication
```
High latency in device communication
```

**Solutions**:
1. **Protocol Selection**: Use appropriate protocol for use case
   - **gRPC**: For high-performance, low-latency communication
   - **WebSocket**: For real-time bidirectional communication
   - **HTTP**: For simple request-response operations

2. **Compression**: Enable compression for large payloads
   ```cpp
   manager->enableCompression(CommunicationProtocol::WEBSOCKET, true);
   ```

3. **Connection Reuse**: Enable connection pooling
   ```cpp
   manager->enableConnectionPooling(true);
   ```

## Debugging Techniques

### Enable Debug Logging

```cpp
// Enable comprehensive debug logging
hydrogen::setLogLevel(LogLevel::DEBUG);

// Enable protocol-specific logging
hydrogen::compatibility::setLogLevel(LogLevel::DEBUG);
hydrogen::communication::setLogLevel(LogLevel::DEBUG);
```

### Network Traffic Analysis

```bash
# Monitor network traffic
sudo tcpdump -i lo port 8080
wireshark  # GUI network analyzer

# Check port usage
netstat -tulpn | grep hydrogen
ss -tulpn | grep hydrogen
```

### System Resource Monitoring

```bash
# Monitor CPU and memory usage
top -p $(pgrep hydrogen)
htop -p $(pgrep hydrogen)

# Monitor file descriptors
lsof -p $(pgrep hydrogen)

# Monitor system calls
strace -p $(pgrep hydrogen)
```

### Application-Level Debugging

```cpp
// Add debug checkpoints
void debugCheckpoint(const std::string& location) {
    static int counter = 0;
    SPDLOG_DEBUG("Debug checkpoint {}: {} (call #{})", 
                 counter++, location, counter);
}

// Monitor device state
void printDeviceState(const Device& device) {
    auto properties = device.getAllProperties();
    SPDLOG_DEBUG("Device state: {}", properties.dump(2));
}

// Track message flow
void logMessage(const std::string& direction, const json& message) {
    SPDLOG_DEBUG("Message {}: {}", direction, message.dump());
}
```

## Common Error Messages

### "Protocol not supported"
**Cause**: Attempting to use a protocol that wasn't compiled in
**Solution**: Rebuild with required protocol support enabled
```bash
cmake -DHYDROGEN_ENABLE_GRPC=ON -DHYDROGEN_ENABLE_MQTT=ON -S . -B build
```

### "Device already registered"
**Cause**: Attempting to register a device with an ID that's already in use
**Solution**: Use unique device IDs or unregister the existing device first
```cpp
hydrogen::compatibility::unregisterDevice("existing_device_id");
```

### "Invalid property mapping"
**Cause**: Property mapping configuration is incorrect
**Solution**: Check property mapping syntax and supported properties
```cpp
// Correct mapping format
mappingConfig.addMapping("internal_name", "ascom_name", "indi_property.element");
```

### "Connection pool exhausted"
**Cause**: All connections in the pool are in use
**Solution**: Increase pool size or implement connection queuing
```cpp
manager->setMaxConnectionsPerProtocol(10);  // Increase pool size
```

## Getting Help

### Log Collection

When reporting issues, collect relevant logs:

```bash
# Enable debug logging and reproduce the issue
export HYDROGEN_LOG_LEVEL=DEBUG
export HYDROGEN_LOG_FILE=/tmp/hydrogen_debug.log

# Run your application
./your_hydrogen_app

# Collect logs
tar -czf hydrogen_logs.tar.gz /tmp/hydrogen_debug.log
```

### System Information

Collect system information for bug reports:

```bash
# System info
uname -a
lsb_release -a  # Linux
sw_vers        # macOS

# Compiler info
g++ --version
cmake --version

# Package manager info
vcpkg version
conan --version

# Network info
ifconfig
netstat -rn
```

### Minimal Reproduction Case

Create a minimal example that reproduces the issue:

```cpp
#include <hydrogen/hydrogen.h>

int main() {
    // Minimal code that reproduces the issue
    auto device = hydrogen::createDevice("test", "Test", "Device");
    
    // Add only the necessary steps to reproduce the problem
    device->connect("localhost", 8080);
    
    return 0;
}
```

## Support Resources

- **GitHub Issues**: [Report bugs and issues](https://github.com/hydrogen-project/hydrogen/issues)
- **GitHub Discussions**: [Ask questions and get help](https://github.com/hydrogen-project/hydrogen/discussions)
- **Documentation**: [Complete documentation](https://hydrogen-project.github.io/hydrogen)
- **Examples**: Check the `examples/` directory for working code samples
- **Tests**: Review the `tests/` directory for usage patterns and expected behavior

# Migration Guide

## Overview

This guide helps you migrate from older versions of Hydrogen to the latest version with the unified architecture, automatic ASCOM/INDI compatibility, and multi-protocol communication system.

## Version Compatibility

### Current Version: 1.0.0
- **Unified Architecture**: Complete rewrite with unified client system
- **Automatic Compatibility**: Zero-code ASCOM/INDI support
- **Multi-Protocol**: Simultaneous protocol support
- **Modular Devices**: Component-based device architecture

### Breaking Changes from 0.x

1. **API Changes**: New unified client API
2. **Configuration Format**: Updated configuration structure
3. **Device Architecture**: New component-based device system
4. **Build System**: Modern CMake with multiple package managers

## Migration Steps

### 1. Update Dependencies

#### Old Build System (0.x)
```bash
# Old manual dependency management
sudo apt-get install libboost-all-dev
sudo apt-get install libwebsocketpp-dev
make
```

#### New Build System (1.0+)
```bash
# Modern CMake with package managers
cmake --preset default
cmake --build --preset default

# Or with Conan
conan install . --build=missing
cmake --preset conan-default
cmake --build --preset conan-default
```

### 2. Update Client Code

#### Old Client API (0.x)
```cpp
// Old separate client classes
#include "websocket_client.h"
#include "device_manager.h"

WebSocketClient client;
client.connect("localhost", 8080);

DeviceManager deviceManager(&client);
auto devices = deviceManager.discoverDevices();

// Manual device interaction
client.sendCommand("camera_001", "start_exposure", {{"duration", 5.0}});
```

#### New Unified Client API (1.0+)
```cpp
// New unified client
#include <hydrogen/core/unified_device_client.h>

ClientConnectionConfig config;
config.host = "localhost";
config.port = 8080;

auto client = std::make_unique<UnifiedDeviceClient>(config);
client->connect();

// Unified device interaction
auto devices = client->discoverDevices();
auto result = client->executeCommand("camera_001", "start_exposure", {{"duration", 5.0}});
```

### 3. Update Device Implementation

#### Old Device Architecture (0.x)
```cpp
// Old inheritance-based devices
class OldCamera : public DeviceBase {
private:
    // All functionality mixed together
    bool coolerEnabled_;
    double temperature_;
    WebSocketClient* client_;
    
public:
    void startExposure(double duration) {
        // Manual message construction
        json message = {
            {"command", "start_exposure"},
            {"duration", duration}
        };
        client_->sendMessage(message);
    }
    
    void setCoolerEnabled(bool enabled) {
        coolerEnabled_ = enabled;
        // Manual property synchronization
        notifyPropertyChange("coolerEnabled", enabled);
    }
};
```

#### New Modular Device Architecture (1.0+)
```cpp
// New composition-based devices
#include <hydrogen/device/modern_device_base.h>
#include <hydrogen/device/behaviors/temperature_control_behavior.h>
#include <hydrogen/device/behaviors/imaging_behavior.h>

class ModernCamera : public ModernDeviceBase {
private:
    std::unique_ptr<ImagingBehavior> imagingBehavior_;
    std::unique_ptr<TemperatureControlBehavior> coolingBehavior_;
    
public:
    ModernCamera(const std::string& id, const std::string& manufacturer, const std::string& model)
        : ModernDeviceBase(id, manufacturer, model) {
        
        // Add behaviors
        imagingBehavior_ = std::make_unique<ImagingBehavior>("imaging");
        coolingBehavior_ = std::make_unique<TemperatureControlBehavior>("cooling");
        
        addBehavior(std::move(imagingBehavior_));
        addBehavior(std::move(coolingBehavior_));
    }
    
    void startExposure(double duration) {
        getBehavior<ImagingBehavior>("imaging")->startExposure(duration);
    }
    
    void setCoolerEnabled(bool enabled) {
        getBehavior<TemperatureControlBehavior>("cooling")->enableCooler(enabled);
    }
};
```

### 4. Update Configuration

#### Old Configuration (0.x)
```json
{
    "server": {
        "host": "localhost",
        "port": 8080
    },
    "devices": [
        {
            "type": "camera",
            "id": "cam1",
            "driver": "zwo_asi"
        }
    ]
}
```

#### New Configuration (1.0+)
```json
{
    "client": {
        "network": {
            "host": "localhost",
            "port": 8080,
            "endpoint": "/ws",
            "connectTimeout": 5000,
            "enableAutoReconnect": true
        },
        "protocols": {
            "websocket": {"enabled": true},
            "grpc": {"enabled": true, "port": 9090},
            "mqtt": {"enabled": false}
        }
    },
    "compatibility": {
        "ascom": {"enabled": true, "autoRegister": true},
        "indi": {"enabled": true, "port": 7624}
    },
    "devices": [
        {
            "type": "camera",
            "id": "cam1",
            "manufacturer": "ZWO",
            "model": "ASI294MM Pro",
            "behaviors": {
                "imaging": {"enabled": true},
                "cooling": {"enabled": true, "targetTemp": -10.0}
            }
        }
    ]
}
```

### 5. Enable Automatic Compatibility

#### Add ASCOM/INDI Support (New in 1.0+)
```cpp
// Initialize compatibility system
#include <hydrogen/compatibility.h>

hydrogen::compatibility::initialize();

// Create devices with automatic compatibility
auto camera = hydrogen::createCompatibleCamera("cam1", "ZWO", "ASI294MM Pro");

// Device is now automatically available through:
// - Native Hydrogen protocol
// - ASCOM COM interface (Windows)
// - INDI XML properties (Linux/cross-platform)

// Cleanup
hydrogen::compatibility::shutdown();
```

### 6. Update Python Code

#### Old Python API (0.x)
```python
# Old separate modules
from hydrogen_client import WebSocketClient
from hydrogen_devices import CameraDevice

client = WebSocketClient()
client.connect("localhost", 8080)

camera = CameraDevice("cam1", client)
camera.start_exposure(60.0)
```

#### New Python API (1.0+)
```python
# New unified Python API with automatic compatibility
import pyhydrogen as hydrogen

# Initialize compatibility system
hydrogen.init_compatibility_system()

# Create devices with automatic ASCOM/INDI compatibility
camera = hydrogen.create_compatible_camera("cam1", "ZWO", "ASI294MM Pro")

# Use device - ASCOM/INDI support is automatic
camera.set_property("coolerOn", True)
camera.start_exposure(60.0)

# Cleanup
hydrogen.shutdown_compatibility_system()
```

## Common Migration Issues

### 1. Missing Dependencies

**Issue**: Build fails with missing dependencies
```
CMake Error: Could not find package Boost
```

**Solution**: Use the new package manager system
```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=/path/to/vcpkg

# Or use Conan
pip install conan
conan profile detect --force
```

### 2. API Method Changes

**Issue**: Old method names not found
```cpp
error: 'class UnifiedDeviceClient' has no member named 'sendCommand'
```

**Solution**: Update to new method names
```cpp
// Old
client.sendCommand("device", "command", params);

// New
client->executeCommand("device", "command", params);
```

### 3. Configuration Format Changes

**Issue**: Old configuration files not working
```
Error: Invalid configuration format
```

**Solution**: Update configuration format or use migration tool
```bash
# Use configuration migration tool (if available)
hydrogen-migrate-config old_config.json new_config.json

# Or manually update using the new format
```

### 4. Device Registration Changes

**Issue**: Devices not appearing in client software
```
No devices found in ASCOM/INDI clients
```

**Solution**: Enable automatic compatibility system
```cpp
// Ensure compatibility system is initialized
hydrogen::compatibility::initialize();

// Verify device registration
auto stats = hydrogen::compatibility::getStatistics();
std::cout << "ASCOM devices: " << stats.ascomDevices << std::endl;
std::cout << "INDI devices: " << stats.indiDevices << std::endl;
```

## Migration Checklist

### Pre-Migration
- [ ] Backup existing code and configuration
- [ ] Review breaking changes documentation
- [ ] Plan migration timeline
- [ ] Set up new build environment

### Code Migration
- [ ] Update build system to use CMake presets
- [ ] Replace old client classes with UnifiedDeviceClient
- [ ] Update device implementations to use new architecture
- [ ] Convert configuration files to new format
- [ ] Update Python code to use new API

### Testing
- [ ] Test basic connectivity and device discovery
- [ ] Verify device functionality with new API
- [ ] Test ASCOM compatibility (Windows)
- [ ] Test INDI compatibility (Linux)
- [ ] Run comprehensive test suite

### Deployment
- [ ] Update deployment scripts
- [ ] Update documentation
- [ ] Train users on new features
- [ ] Monitor for issues

## New Features Available After Migration

### 1. Automatic Protocol Compatibility
- Zero-code ASCOM and INDI support
- Simultaneous protocol access
- Real-time property synchronization

### 2. Multi-Protocol Communication
- WebSocket, gRPC, MQTT, ZeroMQ support
- Automatic protocol selection and fallback
- Performance optimization

### 3. Enhanced Device Architecture
- Modular, component-based design
- Reusable behaviors across device types
- Dynamic configuration and composition

### 4. Improved Build System
- Modern CMake with multiple package managers
- Cross-platform compatibility
- Automatic dependency management

### 5. Comprehensive Testing
- 200+ automated tests
- Performance benchmarking
- Mock framework for testing

## Getting Help

### Migration Support
- **GitHub Issues**: Report migration problems
- **GitHub Discussions**: Ask migration questions
- **Documentation**: Comprehensive guides and examples
- **Examples**: Working code samples in `examples/` directory

### Migration Tools
- **Configuration Migrator**: Tool to convert old config files
- **API Compatibility Layer**: Temporary compatibility for old APIs
- **Migration Scripts**: Automated migration assistance

### Professional Support
For complex migrations or enterprise deployments, consider:
- Professional consulting services
- Custom migration assistance
- Training and support packages

## Timeline Recommendations

### Small Projects (< 10 devices)
- **Planning**: 1-2 days
- **Migration**: 3-5 days
- **Testing**: 2-3 days
- **Total**: 1-2 weeks

### Medium Projects (10-50 devices)
- **Planning**: 1 week
- **Migration**: 2-3 weeks
- **Testing**: 1 week
- **Total**: 4-5 weeks

### Large Projects (50+ devices)
- **Planning**: 2 weeks
- **Migration**: 4-6 weeks
- **Testing**: 2 weeks
- **Total**: 8-10 weeks

## Post-Migration Benefits

After successful migration, you'll have access to:

1. **Zero-Configuration Protocol Support**: Automatic ASCOM/INDI compatibility
2. **Enhanced Reliability**: Multi-protocol fallback and error recovery
3. **Better Performance**: Optimized communication and connection pooling
4. **Easier Maintenance**: Modular architecture and comprehensive testing
5. **Future-Proof Design**: Modern architecture ready for new features

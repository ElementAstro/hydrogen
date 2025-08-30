# Automatic ASCOM/INDI Compatibility System

## Overview

The Hydrogen Automatic ASCOM/INDI Compatibility System provides seamless, transparent integration between internal device implementations and standard astronomical protocols (ASCOM and INDI) without requiring any changes to existing device code.

## Key Features

### 🔄 **Transparent Protocol Translation**
- Automatic mapping between internal properties and ASCOM/INDI standard properties
- Bidirectional communication supporting both protocol directions
- Real-time property synchronization across all protocols

### 🔌 **Zero-Code Integration**
- No changes required to existing device implementations
- One-line enablement for any device
- Automatic discovery and registration of devices

### 🛠️ **Automatic Type Conversion**
- Intelligent parameter validation and type conversion
- Protocol-specific error handling and translation
- Automatic range checking and constraint validation

### 🌐 **Full Protocol Support**
- Complete ASCOM COM interface implementation with automatic registration
- Full INDI XML protocol with automatic property synchronization
- Support for all standard device types (Camera, Telescope, Focuser, etc.)

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   ASCOM Client  │    │   INDI Client    │    │ Internal Client │
└─────────┬───────┘    └─────────┬────────┘    └─────────┬───────┘
          │                      │                       │
          ▼                      ▼                       ▼
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│  ASCOM Bridge   │    │   INDI Bridge    │    │ Direct Access   │
└─────────┬───────┘    └─────────┬────────┘    └─────────┬───────┘
          │                      │                       │
          └──────────────────────┼───────────────────────┘
                                 ▼
                    ┌──────────────────────┐
                    │ Transparent Protocol │
                    │       Bridge         │
                    └──────────┬───────────┘
                               ▼
                    ┌──────────────────────┐
                    │ Automatic Adapter    │
                    └──────────┬───────────┘
                               ▼
                    ┌──────────────────────┐
                    │  Internal Device     │
                    │   Implementation     │
                    └──────────────────────┘
```

## Quick Start

### 1. Initialize the System

```cpp
#include "device/interfaces/automatic_compatibility.h"

// Initialize with default settings
INIT_COMPATIBILITY_SYSTEM();

// Or with custom configuration
astrocomm::device::interfaces::compatibility::initializeCompatibilitySystem(
    true,  // enableAutoDiscovery
    true,  // enableASCOM
    true,  // enableINDI
    7624   // indiBasePort
);
```

### 2. Enable Compatibility for Devices

```cpp
// Create your device as usual
auto camera = std::make_shared<Camera>("cam1", "Manufacturer", "Model");
camera->initializeDevice();
camera->startDevice();

// Enable automatic ASCOM/INDI compatibility with one line
ENABLE_ASCOM_INDI_COMPATIBILITY(camera, "camera1");

// That's it! Your device is now accessible via ASCOM and INDI
```

### 3. Use Through Any Protocol

```cpp
// Access through internal API (unchanged)
camera->setProperty("coolerOn", true);
double temp = camera->getProperty("temperature").get<double>();

// Access through ASCOM (automatic translation)
auto bridge = GET_DEVICE_AUTO(Camera, "camera1");
bridge->setProperty<bool>("CoolerOn", true, ProtocolType::ASCOM);
double ascomTemp = bridge->getProperty<double>("CCDTemperature", ProtocolType::ASCOM);

// Access through INDI (automatic translation)
bridge->setProperty<bool>("CCD_COOLER", true, ProtocolType::INDI);
double indiTemp = bridge->getProperty<double>("CCD_TEMPERATURE", ProtocolType::INDI);
```

## Advanced Usage

### Custom Bridge Configuration

```cpp
bridge::BridgeConfiguration config;
config.deviceName = "MyCustomCamera";
config.deviceDescription = "High-Performance Astronomy Camera";
config.enableASCOM = true;
config.enableINDI = true;
config.customProperties["manufacturer"] = "Custom Corp";
config.customProperties["model"] = "SuperCam Pro";

auto bridge = bridge::ProtocolBridgeFactory::createAndStartBridge(camera, config);
```

### RAII-Style Management

```cpp
{
    // System automatically initializes
    compatibility::CompatibilitySystemManager manager;
    
    auto camera = std::make_shared<Camera>("cam1", "Mfg", "Model");
    auto telescope = std::make_shared<Telescope>("tel1", "Mfg", "Model");
    
    // Enable devices
    auto cameraBridge = manager.enableDevice(camera, "camera1");
    auto telescopeBridge = manager.enableDevice(telescope, "telescope1");
    
    // Use devices through any protocol
    cameraBridge->setProperty<double>("ExposureDuration", 5.0, ProtocolType::ASCOM);
    telescopeBridge->invokeMethod<void>("SlewToCoordinates", ProtocolType::ASCOM, 12.5, 45.0);
    
    // System automatically shuts down when manager goes out of scope
}
```

### Integration Manager Access

```cpp
auto& manager = integration::AutomaticIntegrationManager::getInstance();

// Direct property access
double temp = manager.getDeviceProperty<Camera, double>("camera1", "temperature");
manager.setDeviceProperty<Camera>("camera1", "coolerOn", true);

// Method invocation
manager.invokeDeviceMethod<Camera, void>("camera1", "START_EXPOSURE", 
                                        ProtocolType::INTERNAL, 5.0, true);

// Statistics
auto stats = manager.getStatistics();
std::cout << "Total devices: " << stats.totalDevices << std::endl;
std::cout << "ASCOM devices: " << stats.ascomDevices << std::endl;
std::cout << "INDI devices: " << stats.indiDevices << std::endl;
```

## Property Mapping

The system automatically maps between internal properties and protocol-specific names:

| Internal Property | ASCOM Property | INDI Property | Description |
|------------------|----------------|---------------|-------------|
| `temperature` | `CCDTemperature` | `CCD_TEMPERATURE` | Sensor temperature |
| `coolerOn` | `CoolerOn` | `CCD_COOLER` | Cooler state |
| `exposureDuration` | `ExposureDuration` | `CCD_EXPOSURE` | Exposure time |
| `rightAscension` | `RightAscension` | `EQUATORIAL_EOD_COORD` | RA coordinate |
| `declination` | `Declination` | `EQUATORIAL_EOD_COORD` | Dec coordinate |
| `position` | `Position` | `ABS_FOCUS_POSITION` | Focuser position |

## Error Handling

The system automatically translates errors between protocols:

```cpp
try {
    bridge->setProperty<double>("InvalidProperty", 123.0, ProtocolType::ASCOM);
} catch (const std::exception& e) {
    // Automatically translated to ASCOM error format:
    // "0x80040005: Invalid parameter value"
}
```

## Device Discovery

The system can automatically discover and register devices:

```cpp
manager.addDeviceDiscoveryCallback([](const std::string& deviceId, std::shared_ptr<core::IDevice> device) {
    std::cout << "New device discovered: " << deviceId << std::endl;
    // Device is automatically registered with ASCOM/INDI
});
```

## Protocol-Specific Features

### ASCOM Features
- Automatic COM registration and class factory creation
- Full IDispatch interface implementation
- ASCOM standard error codes and exceptions
- Setup dialog support
- Action/Command interface support

### INDI Features
- XML property vector management
- Automatic property synchronization
- BLOB (Binary Large Object) support
- Client connection management
- Message routing and processing

## Performance Considerations

- **Minimal Overhead**: Property access adds ~1-2μs overhead
- **Lazy Initialization**: Bridges created only when needed
- **Efficient Synchronization**: Properties synchronized only when changed
- **Thread-Safe**: All operations are thread-safe with minimal locking

## Supported Device Types

- ✅ Camera (ICamera)
- ✅ Telescope (ITelescope)  
- ✅ Focuser (IFocuser)
- ✅ Rotator (IRotator)
- ✅ Filter Wheel (IFilterWheel)
- ✅ Dome (IDome)
- ✅ Switch (ISwitch)
- ✅ Safety Monitor (ISafetyMonitor)
- ✅ Cover Calibrator (ICoverCalibrator)
- ✅ Observing Conditions (IObservingConditions)

## Best Practices

1. **Initialize Early**: Call `INIT_COMPATIBILITY_SYSTEM()` at application startup
2. **Use RAII**: Prefer `CompatibilitySystemManager` for automatic cleanup
3. **Check Capabilities**: Verify protocol support before accessing properties
4. **Handle Exceptions**: Always wrap protocol access in try-catch blocks
5. **Monitor Statistics**: Use system statistics for debugging and monitoring

## Troubleshooting

### Common Issues

**Device not appearing in ASCOM clients:**
- Ensure COM registration is enabled: `config.autoRegisterCOM = true`
- Check Windows registry for device entries
- Verify ASCOM Platform is installed

**INDI clients cannot connect:**
- Check firewall settings for INDI port (default 7624)
- Verify INDI server is started: `bridge->start(port)`
- Check device name matches INDI client expectations

**Property access fails:**
- Verify property name mapping in adapter configuration
- Check device supports the requested property
- Ensure device is connected and initialized

### Debug Logging

Enable detailed logging for troubleshooting:

```cpp
spdlog::set_level(spdlog::level::debug);
```

This will show detailed information about property mappings, protocol translations, and bridge operations.

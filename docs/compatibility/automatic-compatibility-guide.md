# Automatic ASCOM/INDI Compatibility Guide

## Overview

Hydrogen's **Automatic ASCOM/INDI Compatibility System** is a revolutionary feature that makes any device automatically compatible with both ASCOM and INDI protocols without requiring any additional code. This zero-configuration system provides seamless integration with existing astronomical software.

## Key Benefits

### Zero-Code Protocol Support
- **Automatic Registration**: Devices automatically appear in ASCOM and INDI clients
- **Property Mapping**: Internal properties automatically mapped to ASCOM/INDI equivalents
- **Protocol Translation**: Messages automatically converted between protocol formats
- **Synchronization**: Property changes propagate across all protocols in real-time

### Professional Integration
- **ASCOM Platform**: Full COM interface registration and discovery
- **INDI Server**: Automatic XML property system integration
- **Concurrent Access**: Same device accessible through multiple protocols simultaneously
- **Standard Compliance**: Full compliance with ASCOM and INDI specifications

## How It Works

### Automatic Bridge Creation

When you create a device, the compatibility system automatically:

1. **Analyzes Device Properties**: Scans device capabilities and properties
2. **Creates Protocol Bridges**: Generates ASCOM COM interface and INDI XML properties
3. **Registers Interfaces**: Registers with Windows Registry (ASCOM) and INDI server
4. **Maintains Synchronization**: Keeps all protocol views synchronized

```cpp
// Create any device - automatic compatibility is enabled by default
auto camera = std::make_shared<Camera>("cam1", "ZWO", "ASI294MM Pro");

// The device is now automatically available through:
// - Native Hydrogen protocol
// - ASCOM COM interface (Windows)
// - INDI XML properties (Linux/cross-platform)
```

### Property Mapping System

The system automatically maps internal properties to protocol-specific equivalents:

| Internal Property | ASCOM Property | INDI Property |
|------------------|----------------|---------------|
| `coolerOn` | `CoolerOn` | `CCD_COOLER.COOLER_ON` |
| `targetTemperature` | `SetCCDTemperature` | `CCD_TEMPERATURE.CCD_TEMPERATURE_VALUE` |
| `currentTemperature` | `CCDTemperature` | `CCD_TEMPERATURE.CCD_TEMPERATURE_VALUE` |
| `binning` | `BinX`, `BinY` | `CCD_BINNING.HOR_BIN`, `CCD_BINNING.VER_BIN` |
| `gain` | `Gain` | `CCD_GAIN.GAIN` |

## Using the Compatibility System

### C++ API

```cpp
#include <hydrogen/compatibility.h>

// Initialize the compatibility system
hydrogen::compatibility::initialize();

// Create devices with automatic compatibility
auto camera = hydrogen::createCompatibleCamera("cam1", "ZWO", "ASI294MM Pro");
auto telescope = hydrogen::createCompatibleTelescope("tel1", "Celestron", "CGX-L");
auto focuser = hydrogen::createCompatibleFocuser("foc1", "ZWO", "EAF");

// Use devices normally - ASCOM/INDI support is automatic
camera->setProperty("coolerOn", true);
camera->setProperty("targetTemperature", -10.0);

// The same changes are automatically available through ASCOM and INDI
```

### Python API

```python
import pyhydrogen as hydrogen

# Initialize the compatibility system
hydrogen.init_compatibility_system()

# Create devices with automatic compatibility
camera = hydrogen.create_compatible_camera("cam1", "ZWO", "ASI294MM Pro")
telescope = hydrogen.create_compatible_telescope("tel1", "Celestron", "CGX-L")

# Use devices - ASCOM/INDI support is automatic
camera.set_property("coolerOn", True)
camera.set_property("targetTemperature", -10.0)

# Cleanup
hydrogen.shutdown_compatibility_system()
```

### Protocol-Specific Access

You can also access devices through specific protocols:

```cpp
// Get the integration manager
auto manager = hydrogen::compatibility::getIntegrationManager();

// Get device bridge for protocol-specific access
auto bridge = manager->getDeviceBridge("cam1");

// Access through different protocols
bridge->setProperty("coolerOn", true, ProtocolType::INTERNAL);
bridge->setProperty("CoolerOn", true, ProtocolType::ASCOM);
bridge->setProperty("CCD_COOLER", true, ProtocolType::INDI);

// All three calls affect the same underlying property!
```

## Device Type Support

### Camera Devices

**Supported Features:**
- Image capture and download
- Cooling control and temperature monitoring
- Binning and subframe selection
- Gain and offset control
- Filter wheel integration (if present)

**ASCOM Interface:** `ASCOM.Hydrogen.Camera`
**INDI Device Type:** `CCD`

```cpp
auto camera = hydrogen::createCompatibleCamera("cam1", "ZWO", "ASI294MM Pro");

// These properties are automatically mapped:
camera->setProperty("coolerOn", true);        // → ASCOM: CoolerOn, INDI: CCD_COOLER
camera->setProperty("targetTemperature", -10); // → ASCOM: SetCCDTemperature, INDI: CCD_TEMPERATURE
camera->setProperty("binning", 2);            // → ASCOM: BinX/BinY, INDI: CCD_BINNING
```

### Telescope Devices

**Supported Features:**
- Slewing and tracking control
- Coordinate system management
- Parking and homing
- Pier side and meridian flip handling

**ASCOM Interface:** `ASCOM.Hydrogen.Telescope`
**INDI Device Type:** `Telescope`

```cpp
auto telescope = hydrogen::createCompatibleTelescope("tel1", "Celestron", "CGX-L");

// Coordinate properties automatically mapped:
telescope->setProperty("targetRA", 5.59);     // → ASCOM: TargetRightAscension, INDI: EQUATORIAL_EOD_COORD
telescope->setProperty("targetDec", -5.39);   // → ASCOM: TargetDeclination, INDI: EQUATORIAL_EOD_COORD
telescope->setProperty("tracking", true);     // → ASCOM: Tracking, INDI: TELESCOPE_TRACK_STATE
```

### Focuser Devices

**Supported Features:**
- Position control and movement
- Temperature compensation
- Backlash compensation
- Speed control

**ASCOM Interface:** `ASCOM.Hydrogen.Focuser`
**INDI Device Type:** `Focuser`

```cpp
auto focuser = hydrogen::createCompatibleFocuser("foc1", "ZWO", "EAF");

// Position properties automatically mapped:
focuser->setProperty("position", 5000);       // → ASCOM: Position, INDI: ABS_FOCUS_POSITION
focuser->setProperty("tempComp", true);       // → ASCOM: TempComp, INDI: FOCUS_TEMPERATURE
```

### Additional Device Types

- **Filter Wheel**: `ASCOM.Hydrogen.FilterWheel` / INDI `Filter Wheel`
- **Dome**: `ASCOM.Hydrogen.Dome` / INDI `Dome`
- **Weather Station**: `ASCOM.Hydrogen.ObservingConditions` / INDI `Weather`
- **Switch**: `ASCOM.Hydrogen.Switch` / INDI `Switch`

## Configuration and Customization

### Bridge Configuration

```cpp
// Create custom bridge configuration
BridgeConfiguration config("MyCamera", "Custom Camera Description");
config.enableAscom = true;
config.enableIndi = true;
config.indiPort = 7625;
config.customProperties["manufacturer"] = "Custom Corp";
config.customProperties["model"] = "SuperCam Pro";

// Create device with custom configuration
auto camera = std::make_shared<Camera>("cam1", "Custom", "SuperCam");
auto bridge = hydrogen::compatibility::createBridgeWithConfig(camera, config);
```

### Property Mapping Customization

```cpp
// Customize property mappings
PropertyMappingConfig mappingConfig;
mappingConfig.addMapping("customTemp", "CCDTemperature", "CCD_TEMPERATURE.TEMPERATURE");
mappingConfig.addMapping("customGain", "Gain", "CCD_GAIN.GAIN");

bridge->setPropertyMappings(mappingConfig);
```

### Protocol-Specific Settings

```cpp
// ASCOM-specific settings
AscomConfig ascomConfig;
ascomConfig.progId = "ASCOM.MyCompany.Camera";
ascomConfig.description = "My Custom Camera";
ascomConfig.driverVersion = "1.0.0";

// INDI-specific settings
IndiConfig indiConfig;
indiConfig.deviceName = "My Camera";
indiConfig.port = 7624;
indiConfig.autoStart = true;

bridge->configureAscom(ascomConfig);
bridge->configureIndi(indiConfig);
```

## Client Integration

### ASCOM Client Access (Windows)

```vb
' VB.NET example using ASCOM
Dim camera As Object = CreateObject("ASCOM.Hydrogen.Camera.cam1")

' Connect to the device
camera.Connected = True

' Use standard ASCOM properties
camera.CoolerOn = True
camera.SetCCDTemperature = -10
camera.BinX = 2
camera.BinY = 2

' Start exposure
camera.StartExposure(60.0, True)
```

### INDI Client Access (Cross-platform)

```python
# Python example using PyINDI
import PyIndi

# Connect to INDI server
indiclient = PyIndi.BaseClient()
indiclient.setServer("localhost", 7624)
indiclient.connectServer()

# Get device
camera = indiclient.getDevice("cam1")

# Use standard INDI properties
cooler = camera.getSwitch("CCD_COOLER")
cooler[0].s = PyIndi.ISS_ON  # Turn on cooler
indiclient.sendNewSwitch(cooler)

# Set temperature
temp = camera.getNumber("CCD_TEMPERATURE")
temp[0].value = -10.0
indiclient.sendNewNumber(temp)
```

## Monitoring and Statistics

### System Statistics

```cpp
// Get compatibility system statistics
auto stats = hydrogen::compatibility::getStatistics();

std::cout << "Total devices: " << stats.totalDevices << std::endl;
std::cout << "ASCOM-enabled devices: " << stats.ascomDevices << std::endl;
std::cout << "INDI-enabled devices: " << stats.indiDevices << std::endl;
std::cout << "System uptime: " << stats.uptime.count() << " seconds" << std::endl;

// Device type breakdown
for (const auto& [deviceType, count] : stats.deviceTypeCount) {
    std::cout << deviceType << ": " << count << std::endl;
}
```

### Event Monitoring

```cpp
// Monitor compatibility events
hydrogen::compatibility::setEventHandler([](const CompatibilityEvent& event) {
    switch (event.type) {
        case EventType::DEVICE_REGISTERED:
            std::cout << "Device registered: " << event.deviceId << std::endl;
            break;
        case EventType::ASCOM_CLIENT_CONNECTED:
            std::cout << "ASCOM client connected to: " << event.deviceId << std::endl;
            break;
        case EventType::INDI_CLIENT_CONNECTED:
            std::cout << "INDI client connected to: " << event.deviceId << std::endl;
            break;
    }
});
```

## Troubleshooting

### Common Issues

**ASCOM devices not appearing in client software:**
1. Ensure ASCOM Platform is installed
2. Check Windows Registry for device entries
3. Verify COM registration is enabled
4. Run client software as administrator if needed

**INDI clients cannot connect:**
1. Check firewall settings for INDI ports (default 7624+)
2. Verify INDI server is started
3. Check device names match client expectations
4. Ensure proper network connectivity

**Property synchronization issues:**
1. Enable debug logging to trace property changes
2. Check property mapping configuration
3. Verify protocol-specific property formats
4. Monitor for timing-related synchronization issues

### Debug Logging

```cpp
// Enable detailed compatibility logging
hydrogen::compatibility::setLogLevel(LogLevel::DEBUG);

// This will show:
// - Device registration and bridge creation
// - Property mappings and translations
// - Protocol-specific communication
// - Client connection and disconnection events
// - Error conditions and recovery attempts
```

## Best Practices

1. **Initialize Early**: Initialize the compatibility system before creating devices
2. **Use Standard Properties**: Stick to standard property names for automatic mapping
3. **Monitor Events**: Implement event handlers to track system status
4. **Handle Errors Gracefully**: Implement proper error handling for protocol failures
5. **Test with Real Clients**: Test with actual ASCOM and INDI client software
6. **Document Custom Properties**: Document any custom property mappings
7. **Version Compatibility**: Ensure compatibility with target ASCOM/INDI versions

## Examples

See the `examples/` directory for complete working examples:

- `examples/python/compatibility_example.py` - Basic compatibility demonstration
- `examples/python/complete_observatory_example.py` - Full observatory with compatibility
- `examples/cpp/ascom_indi_example.cpp` - C++ compatibility usage

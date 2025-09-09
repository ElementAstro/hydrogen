#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>

#include "../device/interfaces/device_interface.h"
#include "../device/interfaces/automatic_compatibility.h"

namespace py = pybind11;
using namespace hydrogen::device;
using namespace hydrogen::device::interfaces;

/**
 * @brief Comprehensive docstring definitions for Python bindings
 * 
 * This file contains detailed docstrings that match the C++ interface
 * documentation and provide complete API documentation for Python users.
 */

namespace docstrings {

// Camera docstrings (ASCOM ICameraV4 compliant)
namespace camera {
    const char* class_doc = R"doc(
Camera device following ASCOM ICameraV4 standard.

This class provides complete control over astronomical cameras with automatic
ASCOM/INDI compatibility. All methods and properties follow the ASCOM standard
for maximum compatibility with existing software.

Example:
    >>> camera = hydrogen.create_compatible_camera("cam1", "ZWO", "ASI294MM Pro")
    >>> camera.set_target_temperature(Temperature(-10.0))
    >>> settings = ExposureSettings(60.0, True, 1)  # 60s light frame, 1x1 binning
    >>> camera.start_exposure(settings)
)doc";

    const char* start_exposure_doc = R"doc(
Start an exposure with specified duration and frame type.

This method initiates an exposure following ASCOM standard behavior. The camera
must be connected and not currently exposing. Use get_camera_state() to monitor
exposure progress and get_image_ready() to check when the image is available.

Args:
    duration (float): Exposure duration in seconds (0.001 to 3600.0)
    light (bool, optional): True for light frame, False for dark frame. Defaults to True.

Raises:
    ASCOMNotConnectedException: Camera is not connected
    ASCOMInvalidValueException: Duration is out of valid range
    ASCOMInvalidOperationException: Camera is already exposing

Example:
    >>> camera.start_exposure(5.0, True)  # 5-second light frame
    >>> while camera.get_camera_state() == CameraState.EXPOSING:
    ...     time.sleep(0.1)
    >>> if camera.get_image_ready():
    ...     image_data = camera.get_image_array()
)doc";

    const char* get_camera_state_doc = R"doc(
Get the current camera state.

Returns the current operational state of the camera following ASCOM standard
states. This property is essential for monitoring exposure progress and
determining when operations can be performed.

Returns:
    CameraState: Current camera state (IDLE, WAITING, EXPOSING, READING, DOWNLOAD, ERROR)

Example:
    >>> state = camera.get_camera_state()
    >>> if state == CameraState.IDLE:
    ...     camera.start_exposure(10.0)
    >>> elif state == CameraState.EXPOSING:
    ...     print(f"Exposure {camera.get_percent_completed()}% complete")
)doc";

    const char* set_binning_doc = R"doc(
Set camera binning factors for X and Y axes.

Binning combines adjacent pixels to increase sensitivity and reduce readout time
at the cost of resolution. The camera must support the requested binning factors.
Use get_max_bin_x() and get_max_bin_y() to determine supported values.

Args:
    binning (Binning): Binning configuration with X and Y factors

Raises:
    ASCOMNotConnectedException: Camera is not connected
    ASCOMInvalidValueException: Binning factors are not supported
    ASCOMInvalidOperationException: Cannot change binning while exposing

Example:
    >>> binning = Binning(2, 2)  # 2x2 binning
    >>> camera.set_binning(binning)
    >>> current = camera.get_binning()
    >>> print(f"Binning set to {current}")
)doc";

    const char* set_target_temperature_doc = R"doc(
Set the target CCD temperature for cooling.

This method sets the target temperature for the camera's cooling system.
The camera must have cooling capability (check with appropriate capability method).
Temperature regulation may take several minutes to stabilize.

Args:
    temperature (Temperature): Target temperature with automatic validation

Raises:
    ASCOMNotConnectedException: Camera is not connected
    ASCOMNotImplementedException: Camera does not support temperature control
    ASCOMInvalidValueException: Temperature is outside valid range

Example:
    >>> target_temp = Temperature(-10.0)  # -10°C
    >>> camera.set_target_temperature(target_temp)
    >>> camera.set_property("coolerOn", True)
    >>> # Monitor temperature regulation
    >>> while abs(camera.get_current_temperature().celsius - target_temp.celsius) > 0.5:
    ...     time.sleep(5)
)doc";
}

// Telescope docstrings (ASCOM ITelescopeV4 compliant)
namespace telescope {
    const char* class_doc = R"doc(
Telescope device following ASCOM ITelescopeV4 standard.

This class provides complete control over astronomical telescopes with automatic
ASCOM/INDI compatibility. All methods and properties follow the ASCOM standard
for maximum compatibility with existing software.

Example:
    >>> telescope = hydrogen.create_compatible_telescope("tel1", "Celestron", "CGX-L")
    >>> coords = Coordinates(5.59, -5.39)  # M42 Orion Nebula
    >>> telescope.slew_to_coordinates(coords)
)doc";

    const char* slew_to_coordinates_doc = R"doc(
Slew telescope to specified equatorial coordinates (synchronous).

This method slews the telescope to the specified right ascension and declination
coordinates. The method blocks until the slew is complete or an error occurs.
Use slew_to_coordinates_async() for non-blocking operation.

Args:
    coordinates (Coordinates): Target coordinates with automatic validation

Raises:
    ASCOMNotConnectedException: Telescope is not connected
    ASCOMNotImplementedException: Telescope cannot slew
    ASCOMInvalidOperationException: Telescope is parked or slaved
    ASCOMInvalidValueException: Coordinates are invalid

Example:
    >>> coords = Coordinates(12.5, 45.0)  # RA=12.5h, Dec=45°
    >>> telescope.slew_to_coordinates(coords)
    >>> print("Slew completed")
    >>> current = telescope.get_current_coordinates()
    >>> print(f"Current position: {current}")
)doc";

    const char* set_tracking_doc = R"doc(
Enable or disable telescope tracking.

Tracking compensates for Earth's rotation to keep celestial objects stationary
in the field of view. This is essential for long-exposure imaging and visual
observation of celestial objects.

Args:
    enabled (bool): True to enable tracking, False to disable

Raises:
    ASCOMNotConnectedException: Telescope is not connected
    ASCOMNotImplementedException: Telescope cannot control tracking
    ASCOMInvalidOperationException: Cannot change tracking while slewing

Example:
    >>> telescope.set_tracking(True)
    >>> print(f"Tracking enabled: {telescope.get_tracking()}")
    >>> # Tracking rate can be adjusted if supported
    >>> if telescope.get_can_set_tracking_rate():
    ...     telescope.set_tracking_rate(DriveRate.SIDEREAL)
)doc";

    const char* pulse_guide_doc = R"doc(
Send a pulse guide command in the specified direction.

Pulse guiding provides fine corrections to telescope pointing for autoguiding
systems. The duration should be short (typically 10-5000ms) to avoid
disrupting tracking.

Args:
    direction (GuideDirection): Direction to guide (NORTH, SOUTH, EAST, WEST)
    duration (float): Pulse duration in seconds (0.001 to 10.0)

Raises:
    ASCOMNotConnectedException: Telescope is not connected
    ASCOMNotImplementedException: Telescope cannot pulse guide
    ASCOMInvalidValueException: Duration is out of valid range

Example:
    >>> # Correct for drift to the east
    >>> telescope.pulse_guide(GuideDirection.WEST, 0.5)  # 500ms pulse west
    >>> # Wait for pulse to complete
    >>> while telescope.get_is_pulse_guiding():
    ...     time.sleep(0.01)
)doc";
}

// Focuser docstrings (ASCOM IFocuserV4 compliant)
namespace focuser {
    const char* class_doc = R"doc(
Focuser device following ASCOM IFocuserV4 standard.

This class provides complete control over astronomical focusers with automatic
ASCOM/INDI compatibility. Supports both absolute and relative positioning
depending on focuser capabilities.

Example:
    >>> focuser = hydrogen.create_compatible_focuser("foc1", "ZWO", "EAF")
    >>> focuser.move(25000)  # Move to absolute position 25000
    >>> focuser.move_relative(100)  # Move 100 steps outward
)doc";

    const char* move_doc = R"doc(
Move focuser to specified absolute position.

This method moves the focuser to an absolute position. The focuser must support
absolute positioning (check with get_absolute()). The method returns immediately;
use get_is_moving() to monitor completion.

Args:
    position (int): Target position in focuser steps

Raises:
    ASCOMNotConnectedException: Focuser is not connected
    ASCOMNotImplementedException: Focuser does not support absolute positioning
    ASCOMInvalidValueException: Position is outside valid range

Example:
    >>> if focuser.get_absolute():
    ...     focuser.move(25000)
    ...     while focuser.get_is_moving():
    ...         time.sleep(0.1)
    ...     print(f"Focuser at position {focuser.get_position()}")
)doc";

    const char* move_relative_doc = R"doc(
Move focuser by specified number of steps.

This method moves the focuser relative to its current position. Positive values
move outward (away from telescope), negative values move inward. All focusers
support relative movement.

Args:
    steps (int): Number of steps to move (positive=outward, negative=inward)

Raises:
    ASCOMNotConnectedException: Focuser is not connected
    ASCOMInvalidValueException: Step count exceeds maximum increment

Example:
    >>> # Fine focus adjustment
    >>> focuser.move_relative(50)   # Move 50 steps outward
    >>> time.sleep(2)
    >>> focuser.move_relative(-25)  # Move 25 steps back inward
)doc";
}

// Type safety docstrings
namespace type_safety {
    const char* coordinates_doc = R"doc(
Type-safe coordinate wrapper with automatic validation.

This class ensures that right ascension and declination values are always
within valid ranges (RA: 0-24 hours, Dec: -90 to +90 degrees). Values are
automatically validated when set.

Args:
    ra (float): Right ascension in hours (0.0 to 24.0)
    dec (float): Declination in degrees (-90.0 to +90.0)

Raises:
    ASCOMInvalidValueException: Coordinates are outside valid ranges

Example:
    >>> coords = Coordinates(12.5, 45.0)  # Valid coordinates
    >>> coords.ra = 25.0  # Raises exception - RA must be 0-24
)doc";

    const char* temperature_doc = R"doc(
Type-safe temperature wrapper with unit conversion.

This class provides automatic temperature validation and conversion between
Celsius, Kelvin, and Fahrenheit. Temperatures below absolute zero are rejected.

Args:
    celsius (float): Temperature in Celsius (-273.15 to 100.0)

Raises:
    ASCOMInvalidValueException: Temperature below absolute zero

Example:
    >>> temp = Temperature(-10.0)  # -10°C
    >>> print(f"Temperature: {temp.celsius}°C = {temp.kelvin}K = {temp.fahrenheit}°F")
    >>> temp_f = Temperature.from_fahrenheit(32.0)  # 0°C from Fahrenheit
)doc";

    const char* exposure_settings_doc = R"doc(
Type-safe exposure settings wrapper with validation.

This class combines all exposure parameters with automatic validation to ensure
consistent and valid exposure configurations. All parameters are validated
when set.

Args:
    duration (float): Exposure duration in seconds (0.001 to 3600.0)
    is_light (bool, optional): True for light frame, False for dark. Defaults to True.
    binning (int, optional): Symmetric binning factor (1-16). Defaults to 1.
    width (int, optional): ROI width in pixels. Defaults to full frame.
    height (int, optional): ROI height in pixels. Defaults to full frame.
    start_x (int, optional): ROI X start position. Defaults to 0.
    start_y (int, optional): ROI Y start position. Defaults to 0.

Example:
    >>> # 5-minute light frame with 2x2 binning
    >>> settings = ExposureSettings(300.0, True, 2)
    >>> # Custom ROI for planetary imaging
    >>> planetary = ExposureSettings(0.1, True, 1, 512, 512, 256, 256)
)doc";
}

// Error handling docstrings
namespace error_handling {
    const char* ascom_exception_doc = R"doc(
Base class for ASCOM-compliant exceptions.

All ASCOM exceptions include a standard error code that matches the ASCOM
specification. This ensures compatibility with ASCOM clients and provides
consistent error reporting.

Attributes:
    error_code (int): ASCOM standard error code
)doc";

    const char* type_validator_doc = R"doc(
Utility class for type validation with ASCOM-compliant error messages.

This class provides static methods for validating common parameter types
and ranges. All validation methods throw ASCOMInvalidValueException with
descriptive error messages when validation fails.

Example:
    >>> TypeValidator.validate_range(5.0, 0.0, 10.0, "exposure_time")  # OK
    >>> TypeValidator.validate_range(15.0, 0.0, 10.0, "exposure_time")  # Raises exception
)doc";
}

} // namespace docstrings

/**
 * @brief Add comprehensive docstrings to Python bindings
 */
void add_comprehensive_docstrings(py::module& m) {
    // Module-level documentation
    m.doc() = R"doc(
Hydrogen Python Bindings with Automatic ASCOM/INDI Compatibility

This module provides complete Python access to the Hydrogen astronomical device
control system with revolutionary automatic ASCOM/INDI compatibility. All devices
created through this module automatically support both ASCOM and INDI protocols
without any additional code.

Key Features:
- Complete device support: Camera, Telescope, Focuser, Dome, ObservingConditions
- Automatic ASCOM/INDI compatibility with zero additional code
- Type-safe wrappers with automatic validation
- Comprehensive error handling with ASCOM-compliant exceptions
- Real-time property synchronization across all protocols
- Thread-safe operations with full concurrent access support

Quick Start:
    >>> import pyhydrogen as hydrogen
    >>> 
    >>> # Initialize automatic compatibility system
    >>> hydrogen.init_compatibility_system()
    >>> 
    >>> # Create devices with automatic ASCOM/INDI support
    >>> camera = hydrogen.create_compatible_camera("cam1", "ZWO", "ASI294MM Pro")
    >>> telescope = hydrogen.create_compatible_telescope("tel1", "Celestron", "CGX-L")
    >>> 
    >>> # Use devices normally - ASCOM/INDI support is automatic!
    >>> camera.set_property("coolerOn", True)
    >>> telescope.slew_to_coordinates(Coordinates(12.5, 45.0))
    >>> 
    >>> # Cleanup
    >>> hydrogen.shutdown_compatibility_system()

For complete documentation and examples, see:
- examples/python/compatibility_example.py
- examples/python/complete_observatory_example.py
- docs/automatic_compatibility_system.md
)doc";

    // Add version information
    m.attr("__version__") = "1.0.0";
    m.attr("__author__") = "ElementAstro Hydrogen Team";
    m.attr("__email__") = "support@elementastro.com";
    
    // Add compatibility information
    m.attr("ASCOM_COMPATIBLE") = true;
    m.attr("INDI_COMPATIBLE") = true;
    m.attr("ASCOM_VERSION") = "6.6";
    m.attr("INDI_VERSION") = "1.9.9";
    
    // Add build information
    m.attr("BUILD_DATE") = __DATE__;
    m.attr("BUILD_TIME") = __TIME__;
    
    #ifdef NDEBUG
    m.attr("DEBUG_BUILD") = false;
    #else
    m.attr("DEBUG_BUILD") = true;
    #endif
}

/**
 * @brief Generate API reference documentation
 */
std::string generate_api_reference() {
    return R"doc(
# Hydrogen Python API Reference

## Device Classes

### Camera (ASCOM ICameraV4 Compliant)
Complete camera control with automatic ASCOM/INDI compatibility.

**Key Methods:**
- `start_exposure(duration, light=True)` - Start exposure
- `abort_exposure()` - Abort current exposure  
- `get_camera_state()` - Get current state
- `set_binning(binning)` - Set binning factors
- `set_target_temperature(temperature)` - Set cooling target

### Telescope (ASCOM ITelescopeV4 Compliant)
Complete telescope control with automatic ASCOM/INDI compatibility.

**Key Methods:**
- `slew_to_coordinates(coordinates)` - Slew to RA/Dec
- `slew_to_alt_az(alt_az)` - Slew to Alt/Az
- `set_tracking(enabled)` - Enable/disable tracking
- `pulse_guide(direction, duration)` - Send guide pulse
- `park()` / `unpark()` - Park/unpark telescope

### Focuser (ASCOM IFocuserV4 Compliant)
Complete focuser control with automatic ASCOM/INDI compatibility.

**Key Methods:**
- `move(position)` - Move to absolute position
- `move_relative(steps)` - Move relative steps
- `halt()` - Stop movement immediately
- `get_position()` - Get current position

## Type-Safe Wrappers

### Coordinates
Type-safe coordinate wrapper with validation.
- `Coordinates(ra, dec)` - RA in hours (0-24), Dec in degrees (-90 to +90)

### Temperature  
Temperature wrapper with unit conversion.
- `Temperature(celsius)` - Temperature in Celsius
- `Temperature.from_fahrenheit(f)` - Create from Fahrenheit
- `Temperature.from_kelvin(k)` - Create from Kelvin

### ExposureSettings
Complete exposure configuration with validation.
- `ExposureSettings(duration, is_light, binning, width, height, start_x, start_y)`

## Error Handling

### Exception Hierarchy
- `DeviceException` - Base device exception
- `ASCOMException` - ASCOM-compliant exceptions with error codes
- `INDIException` - INDI-specific exceptions
- `ASCOMNotConnectedException` - Device not connected
- `ASCOMInvalidValueException` - Invalid parameter value
- `ASCOMInvalidOperationException` - Invalid operation
- `ASCOMNotImplementedException` - Feature not implemented

## Compatibility System

### Initialization
```python
hydrogen.init_compatibility_system(
    enable_auto_discovery=True,
    enable_ascom=True, 
    enable_indi=True,
    indi_base_port=7624
)
```

### Device Creation
```python
# Devices with automatic ASCOM/INDI compatibility
camera = hydrogen.create_compatible_camera("cam1", "ZWO", "ASI294")
telescope = hydrogen.create_compatible_telescope("tel1", "Celestron", "CGX")
```

### Protocol Access
```python
# Same device accessible through all protocols
bridge = hydrogen.get_integration_manager().get_device_bridge("cam1")
bridge.set_property("coolerOn", True, hydrogen.ProtocolType.ASCOM)
bridge.set_property("CCD_COOLER", True, hydrogen.ProtocolType.INDI)
```

For complete examples, see the examples/ directory.
)doc";
}

} // namespace docstrings

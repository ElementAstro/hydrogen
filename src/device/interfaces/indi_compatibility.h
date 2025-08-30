#pragma once

#include "device_interface.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace astrocomm {
namespace device {
namespace interfaces {
namespace indi {

using json = nlohmann::json;

/**
 * @brief INDI Property Types
 */
enum class PropertyType {
    TEXT = 0,
    NUMBER = 1,
    SWITCH = 2,
    LIGHT = 3,
    BLOB = 4
};

/**
 * @brief INDI Property States
 */
enum class PropertyState {
    IDLE = 0,
    OK = 1,
    BUSY = 2,
    ALERT = 3
};

/**
 * @brief INDI Property Permissions
 */
enum class PropertyPermission {
    RO = 0,  // Read Only
    WO = 1,  // Write Only
    RW = 2   // Read Write
};

/**
 * @brief INDI Switch Rules
 */
enum class SwitchRule {
    ONE_OF_MANY = 0,
    AT_MOST_ONE = 1,
    ANY_OF_MANY = 2
};

/**
 * @brief INDI Property Element
 */
struct PropertyElement {
    std::string name;
    std::string label;
    std::string format;
    json value;
    json min;
    json max;
    json step;
    
    json toXML() const;
    static PropertyElement fromXML(const std::string& xml);
};

/**
 * @brief INDI Property Vector
 */
struct PropertyVector {
    std::string device;
    std::string name;
    std::string label;
    std::string group;
    PropertyType type;
    PropertyState state;
    PropertyPermission permission;
    SwitchRule rule;  // Only for switch properties
    std::string timeout;
    std::string timestamp;
    std::string message;
    std::vector<PropertyElement> elements;
    
    json toXML() const;
    static PropertyVector fromXML(const std::string& xml);
};

/**
 * @brief INDI Device Interface Adapter
 * 
 * Provides INDI-standard XML property definitions and protocol communication patterns
 */
class INDIDeviceAdapter {
public:
    virtual ~INDIDeviceAdapter() = default;

    // INDI Standard Device Properties
    virtual std::string getDeviceName() const = 0;
    virtual std::string getDriverName() const = 0;
    virtual std::string getDriverExec() const = 0;
    virtual std::string getDriverVersion() const = 0;
    virtual std::string getDriverInterface() const = 0;

    // Property Management
    virtual void defineProperty(const PropertyVector& property) = 0;
    virtual void deleteProperty(const std::string& name) = 0;
    virtual PropertyVector getProperty(const std::string& name) const = 0;
    virtual std::vector<PropertyVector> getAllProperties() const = 0;
    virtual void updateProperty(const PropertyVector& property) = 0;

    // Message Handling
    virtual void sendMessage(const std::string& message) = 0;
    virtual void sendAlert(const std::string& message) = 0;
    virtual void sendDebug(const std::string& message) = 0;

    // Connection Management
    virtual bool isConnected() const = 0;
    virtual void setConnected(bool connected) = 0;

    // Standard INDI Properties
    virtual PropertyVector getConnectionProperty() const = 0;
    virtual PropertyVector getDriverInfoProperty() const = 0;
    virtual PropertyVector getDebugProperty() const = 0;
    virtual PropertyVector getSimulationProperty() const = 0;
    virtual PropertyVector getConfigProcessProperty() const = 0;
};

/**
 * @brief INDI Camera Interface Adapter
 * 
 * Adapts internal camera interface to INDI CCD standard
 */
class INDICameraAdapter : public INDIDeviceAdapter {
public:
    virtual ~INDICameraAdapter() = default;

    // Standard INDI CCD Properties
    virtual PropertyVector getCCDInfoProperty() const = 0;
    virtual PropertyVector getCCDExposureProperty() const = 0;
    virtual PropertyVector getCCDAbortExposureProperty() const = 0;
    virtual PropertyVector getCCDFrameProperty() const = 0;
    virtual PropertyVector getCCDBinningProperty() const = 0;
    virtual PropertyVector getCCDFrameTypeProperty() const = 0;
    virtual PropertyVector getCCDCompressionProperty() const = 0;
    virtual PropertyVector getCCD1Property() const = 0;  // Image blob
    virtual PropertyVector getCCDTemperatureProperty() const = 0;
    virtual PropertyVector getCCDCoolerProperty() const = 0;
    virtual PropertyVector getCCDCoolerPowerProperty() const = 0;
    virtual PropertyVector getCCDGainProperty() const = 0;
    virtual PropertyVector getCCDOffsetProperty() const = 0;
    virtual PropertyVector getCCDControlsProperty() const = 0;
    virtual PropertyVector getCCDVideoStreamProperty() const = 0;

    // Guider Properties (if supported)
    virtual PropertyVector getGuiderInfoProperty() const = 0;
    virtual PropertyVector getGuiderExposureProperty() const = 0;
    virtual PropertyVector getGuiderFrameProperty() const = 0;
    virtual PropertyVector getGuider1Property() const = 0;  // Guider image blob

    // Methods
    virtual void startExposure(double duration) = 0;
    virtual void abortExposure() = 0;
    virtual void updateCCDFrame(int x, int y, int w, int h) = 0;
    virtual void updateBinning(int binX, int binY) = 0;
    virtual void updateTemperature(double temperature) = 0;
    virtual void updateCooler(bool enabled) = 0;
    virtual void updateGain(double gain) = 0;
    virtual void updateOffset(double offset) = 0;
};

/**
 * @brief INDI Telescope Interface Adapter
 * 
 * Adapts internal telescope interface to INDI Telescope standard
 */
class INDITelescopeAdapter : public INDIDeviceAdapter {
public:
    virtual ~INDITelescopeAdapter() = default;

    // Standard INDI Telescope Properties
    virtual PropertyVector getEquatorialCoordsProperty() const = 0;
    virtual PropertyVector getEquatorialEODCoordsProperty() const = 0;
    virtual PropertyVector getHorizontalCoordsProperty() const = 0;
    virtual PropertyVector getTelescopeInfoProperty() const = 0;
    virtual PropertyVector getTelescopeMotionNSProperty() const = 0;
    virtual PropertyVector getTelescopeMotionWEProperty() const = 0;
    virtual PropertyVector getTelescopeAbortMotionProperty() const = 0;
    virtual PropertyVector getTelescopeParkProperty() const = 0;
    virtual PropertyVector getTelescopeParkPositionProperty() const = 0;
    virtual PropertyVector getTelescopeTrackModeProperty() const = 0;
    virtual PropertyVector getTelescopeTrackRateProperty() const = 0;
    virtual PropertyVector getTelescopeTrackStateProperty() const = 0;
    virtual PropertyVector getTimeProperty() const = 0;
    virtual PropertyVector getGeographicCoordsProperty() const = 0;
    virtual PropertyVector getAtmosphereProperty() const = 0;
    virtual PropertyVector getPierSideProperty() const = 0;
    virtual PropertyVector getGuideNSProperty() const = 0;
    virtual PropertyVector getGuideWEProperty() const = 0;
    virtual PropertyVector getGuideRateProperty() const = 0;

    // Methods
    virtual void slewToCoordinates(double ra, double dec) = 0;
    virtual void syncToCoordinates(double ra, double dec) = 0;
    virtual void abortSlew() = 0;
    virtual void park() = 0;
    virtual void unpark() = 0;
    virtual void findHome() = 0;
    virtual void setTracking(bool enabled) = 0;
    virtual void setTrackMode(int mode) = 0;
    virtual void pulseGuide(int direction, int duration) = 0;
    virtual void updateCoordinates(double ra, double dec) = 0;
    virtual void updateLocation(double lat, double lon, double elevation) = 0;
    virtual void updateTime(const std::chrono::system_clock::time_point& time) = 0;
};

/**
 * @brief INDI Focuser Interface Adapter
 * 
 * Adapts internal focuser interface to INDI Focuser standard
 */
class INDIFocuserAdapter : public INDIDeviceAdapter {
public:
    virtual ~INDIFocuserAdapter() = default;

    // Standard INDI Focuser Properties
    virtual PropertyVector getFocuserSpeedProperty() const = 0;
    virtual PropertyVector getFocuserTimerProperty() const = 0;
    virtual PropertyVector getFocuserMotionProperty() const = 0;
    virtual PropertyVector getAbsFocusPositionProperty() const = 0;
    virtual PropertyVector getRelFocusPositionProperty() const = 0;
    virtual PropertyVector getFocuserAbortMotionProperty() const = 0;
    virtual PropertyVector getFocuserSyncProperty() const = 0;
    virtual PropertyVector getFocuserReverseMotionProperty() const = 0;
    virtual PropertyVector getFocuserTemperatureProperty() const = 0;
    virtual PropertyVector getFocuserBacklashProperty() const = 0;
    virtual PropertyVector getFocuserMaxPositionProperty() const = 0;

    // Methods
    virtual void moveToPosition(int position) = 0;
    virtual void moveRelative(int steps) = 0;
    virtual void abortMotion() = 0;
    virtual void syncToPosition(int position) = 0;
    virtual void setReverse(bool reversed) = 0;
    virtual void updatePosition(int position) = 0;
    virtual void updateTemperature(double temperature) = 0;
};

/**
 * @brief INDI Filter Wheel Interface Adapter
 * 
 * Adapts internal filter wheel interface to INDI Filter Wheel standard
 */
class INDIFilterWheelAdapter : public INDIDeviceAdapter {
public:
    virtual ~INDIFilterWheelAdapter() = default;

    // Standard INDI Filter Wheel Properties
    virtual PropertyVector getFilterSlotProperty() const = 0;
    virtual PropertyVector getFilterNameProperty() const = 0;

    // Methods
    virtual void setFilterSlot(int slot) = 0;
    virtual void setFilterName(int slot, const std::string& name) = 0;
    virtual void updateFilterSlot(int slot) = 0;
};

/**
 * @brief INDI Dome Interface Adapter
 * 
 * Adapts internal dome interface to INDI Dome standard
 */
class INDIDomeAdapter : public INDIDeviceAdapter {
public:
    virtual ~INDIDomeAdapter() = default;

    // Standard INDI Dome Properties
    virtual PropertyVector getDomeSpeedProperty() const = 0;
    virtual PropertyVector getDomeMotionProperty() const = 0;
    virtual PropertyVector getDomeAbortMotionProperty() const = 0;
    virtual PropertyVector getAbsDomePositionProperty() const = 0;
    virtual PropertyVector getRelDomePositionProperty() const = 0;
    virtual PropertyVector getDomeParkProperty() const = 0;
    virtual PropertyVector getDomeParkPositionProperty() const = 0;
    virtual PropertyVector getDomeAutoParkProperty() const = 0;
    virtual PropertyVector getDomeShutterProperty() const = 0;
    virtual PropertyVector getDomeGotoProperty() const = 0;
    virtual PropertyVector getDomeParamsProperty() const = 0;
    virtual PropertyVector getDomeMeasurementsProperty() const = 0;

    // Methods
    virtual void moveToAzimuth(double azimuth) = 0;
    virtual void moveRelative(double degrees) = 0;
    virtual void abortMotion() = 0;
    virtual void park() = 0;
    virtual void unpark() = 0;
    virtual void openShutter() = 0;
    virtual void closeShutter() = 0;
    virtual void updateAzimuth(double azimuth) = 0;
    virtual void updateShutterStatus(int status) = 0;
};

} // namespace indi
} // namespace interfaces
} // namespace device
} // namespace astrocomm

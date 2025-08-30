#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <future>
#include <nlohmann/json.hpp>

namespace astrocomm {
namespace device {
namespace interfaces {

using json = nlohmann::json;

// ASCOM/INDI Standard Enumerations
enum class DeviceState {
    IDLE = 0,
    BUSY = 1,
    ERROR = 2,
    UNKNOWN = 3
};

enum class CameraState {
    IDLE = 0,
    WAITING = 1,
    EXPOSING = 2,
    READING = 3,
    DOWNLOAD = 4,
    ERROR = 5
};

enum class GuideDirection {
    NORTH = 0,
    SOUTH = 1,
    EAST = 2,
    WEST = 3
};

enum class SensorType {
    MONOCHROME = 0,
    COLOR = 1,
    RGGB = 2,
    CMYG = 3,
    CMYG2 = 4,
    LRGB = 5
};

enum class ShutterState {
    OPEN = 0,
    CLOSED = 1,
    OPENING = 2,
    CLOSING = 3,
    ERROR = 4
};

enum class CalibratorState {
    NOT_PRESENT = 0,
    OFF = 1,
    NOT_READY = 2,
    READY = 3,
    UNKNOWN = 4,
    ERROR = 5
};

enum class CoverState {
    NOT_PRESENT = 0,
    CLOSED = 1,
    MOVING = 2,
    OPEN = 3,
    UNKNOWN = 4,
    ERROR = 5
};

enum class PierSide {
    EAST = 0,
    WEST = 1,
    UNKNOWN = -1
};

enum class AlignmentMode {
    ALT_AZ = 0,
    POLAR = 1,
    GERMAN_POLAR = 2
};

enum class DriveRate {
    SIDEREAL = 0,
    LUNAR = 1,
    SOLAR = 2,
    KING = 3
};

// ASCOM/INDI Standard Structures
struct Rate {
    double minimum;
    double maximum;

    json toJson() const {
        return json{{"minimum", minimum}, {"maximum", maximum}};
    }

    static Rate fromJson(const json& j) {
        return {j.at("minimum"), j.at("maximum")};
    }
};

/**
 * @brief Base device interface following ASCOM/INDI standards
 *
 * Defines the common interface that all astronomical devices must implement
 */
class IDevice {
public:
    virtual ~IDevice() = default;

    // Basic device identification (ASCOM standard)
    virtual std::string getDeviceId() const = 0;
    virtual std::string getDeviceType() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::string getDriverInfo() const = 0;
    virtual std::string getDriverVersion() const = 0;
    virtual int getInterfaceVersion() const = 0;

    // Device information and capabilities
    virtual json getDeviceInfo() const = 0;
    virtual std::vector<std::string> getSupportedActions() const = 0;

    // Connection management (ASCOM standard)
    virtual bool initialize() = 0;
    virtual bool connect(const std::string& host, uint16_t port) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual bool isConnecting() const = 0;

    // Device lifecycle
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual DeviceState getDeviceState() const = 0;

    // ASCOM standard methods
    virtual std::string action(const std::string& actionName, const std::string& actionParameters = "") = 0;
    virtual void commandBlind(const std::string& command, bool raw = false) = 0;
    virtual bool commandBool(const std::string& command, bool raw = false) = 0;
    virtual std::string commandString(const std::string& command, bool raw = false) = 0;
    virtual void setupDialog() = 0;
};

/**
 * @brief 可配置设备接口
 */
class IConfigurable {
public:
    virtual ~IConfigurable() = default;

    /**
     * @brief 设置配置
     */
    virtual bool setConfig(const std::string& name, const json& value) = 0;

    /**
     * @brief 获取配置
     */
    virtual json getConfig(const std::string& name) const = 0;

    /**
     * @brief 获取所有配置
     */
    virtual json getAllConfigs() const = 0;

    /**
     * @brief 保存配置
     */
    virtual bool saveConfig() = 0;

    /**
     * @brief 加载配置
     */
    virtual bool loadConfig() = 0;
};

/**
 * @brief 有状态设备接口
 */
class IStateful {
public:
    virtual ~IStateful() = default;

    /**
     * @brief 设置属性
     */
    virtual bool setProperty(const std::string& property, const json& value) = 0;

    /**
     * @brief 获取属性
     */
    virtual json getProperty(const std::string& property) const = 0;

    /**
     * @brief 获取所有属性
     */
    virtual json getAllProperties() const = 0;

    /**
     * @brief 获取设备能力
     */
    virtual std::vector<std::string> getCapabilities() const = 0;
};

/**
 * @brief 可移动设备接口
 */
class IMovable {
public:
    virtual ~IMovable() = default;

    /**
     * @brief 移动到绝对位置
     */
    virtual bool moveToPosition(int position) = 0;

    /**
     * @brief 相对移动
     */
    virtual bool moveRelative(int steps) = 0;

    /**
     * @brief 停止移动
     */
    virtual bool stopMovement() = 0;

    /**
     * @brief 归零
     */
    virtual bool home() = 0;

    /**
     * @brief 获取当前位置
     */
    virtual int getCurrentPosition() const = 0;

    /**
     * @brief 是否正在移动
     */
    virtual bool isMoving() const = 0;
};

/**
 * @brief 温度控制设备接口
 */
class ITemperatureControlled {
public:
    virtual ~ITemperatureControlled() = default;

    /**
     * @brief 设置目标温度
     */
    virtual bool setTargetTemperature(double temperature) = 0;

    /**
     * @brief 获取当前温度
     */
    virtual double getCurrentTemperature() const = 0;

    /**
     * @brief 获取目标温度
     */
    virtual double getTargetTemperature() const = 0;

    /**
     * @brief 停止温度控制
     */
    virtual bool stopTemperatureControl() = 0;

    /**
     * @brief 温度是否稳定
     */
    virtual bool isTemperatureStable() const = 0;
};

/**
 * @brief Camera device interface following ASCOM ICameraV4 standard
 */
class ICamera {
public:
    virtual ~ICamera() = default;

    // Exposure control (ASCOM standard)
    virtual void startExposure(double duration, bool light = true) = 0;
    virtual void abortExposure() = 0;
    virtual void stopExposure() = 0;
    virtual CameraState getCameraState() const = 0;
    virtual bool getImageReady() const = 0;
    virtual double getLastExposureDuration() const = 0;
    virtual std::chrono::system_clock::time_point getLastExposureStartTime() const = 0;
    virtual double getPercentCompleted() const = 0;

    // Image properties (ASCOM standard)
    virtual int getCameraXSize() const = 0;
    virtual int getCameraYSize() const = 0;
    virtual double getPixelSizeX() const = 0;
    virtual double getPixelSizeY() const = 0;
    virtual int getMaxBinX() const = 0;
    virtual int getMaxBinY() const = 0;
    virtual bool getCanAsymmetricBin() const = 0;

    // Binning (ASCOM standard)
    virtual int getBinX() const = 0;
    virtual void setBinX(int value) = 0;
    virtual int getBinY() const = 0;
    virtual void setBinY(int value) = 0;

    // Subframe (ASCOM standard)
    virtual int getStartX() const = 0;
    virtual void setStartX(int value) = 0;
    virtual int getStartY() const = 0;
    virtual void setStartY(int value) = 0;
    virtual int getNumX() const = 0;
    virtual void setNumX(int value) = 0;
    virtual int getNumY() const = 0;
    virtual void setNumY(int value) = 0;

    // Gain and offset (ASCOM standard)
    virtual int getGain() const = 0;
    virtual void setGain(int value) = 0;
    virtual int getGainMin() const = 0;
    virtual int getGainMax() const = 0;
    virtual std::vector<std::string> getGains() const = 0;
    virtual int getOffset() const = 0;
    virtual void setOffset(int value) = 0;
    virtual int getOffsetMin() const = 0;
    virtual int getOffsetMax() const = 0;
    virtual std::vector<std::string> getOffsets() const = 0;

    // Readout modes (ASCOM standard)
    virtual int getReadoutMode() const = 0;
    virtual void setReadoutMode(int value) = 0;
    virtual std::vector<std::string> getReadoutModes() const = 0;
    virtual bool getFastReadout() const = 0;
    virtual void setFastReadout(bool value) = 0;
    virtual bool getCanFastReadout() const = 0;

    // Image data (ASCOM standard)
    virtual std::vector<std::vector<int>> getImageArray() const = 0;
    virtual json getImageArrayVariant() const = 0;
    virtual std::vector<uint8_t> getImageData() const = 0;

    // Sensor information (ASCOM standard)
    virtual SensorType getSensorType() const = 0;
    virtual std::string getSensorName() const = 0;
    virtual int getBayerOffsetX() const = 0;
    virtual int getBayerOffsetY() const = 0;
    virtual double getMaxADU() const = 0;
    virtual double getElectronsPerADU() const = 0;
    virtual double getFullWellCapacity() const = 0;

    // Exposure limits (ASCOM standard)
    virtual double getExposureMin() const = 0;
    virtual double getExposureMax() const = 0;
    virtual double getExposureResolution() const = 0;

    // Shutter and guiding (ASCOM standard)
    virtual bool getHasShutter() const = 0;
    virtual bool getCanAbortExposure() const = 0;
    virtual bool getCanStopExposure() const = 0;
    virtual bool getCanPulseGuide() const = 0;
    virtual void pulseGuide(GuideDirection direction, int duration) = 0;
    virtual bool getIsPulseGuiding() const = 0;

    // Subexposure (ASCOM standard)
    virtual double getSubExposureDuration() const = 0;
    virtual void setSubExposureDuration(double value) = 0;

    // Additional methods
    virtual bool setROI(int x, int y, int width, int height) = 0;

    // Convenience methods
    virtual bool isExposing() const {
        auto state = getCameraState();
        return state == CameraState::EXPOSING || state == CameraState::READING || state == CameraState::DOWNLOAD;
    }
};

/**
 * @brief Telescope device interface following ASCOM ITelescopeV4 standard
 */
class ITelescope {
public:
    virtual ~ITelescope() = default;

    // Coordinate properties (ASCOM standard)
    virtual double getRightAscension() const = 0;
    virtual double getDeclination() const = 0;
    virtual double getAltitude() const = 0;
    virtual double getAzimuth() const = 0;
    virtual double getTargetRightAscension() const = 0;
    virtual void setTargetRightAscension(double value) = 0;
    virtual double getTargetDeclination() const = 0;
    virtual void setTargetDeclination(double value) = 0;

    // Slewing methods (ASCOM standard)
    virtual void slewToCoordinates(double ra, double dec) = 0;
    virtual void slewToCoordinatesAsync(double ra, double dec) = 0;
    virtual void slewToTarget() = 0;
    virtual void slewToTargetAsync() = 0;
    virtual void slewToAltAz(double altitude, double azimuth) = 0;
    virtual void slewToAltAzAsync(double altitude, double azimuth) = 0;
    virtual void abortSlew() = 0;
    virtual bool getSlewing() const = 0;

    // Synchronization (ASCOM standard)
    virtual void syncToCoordinates(double ra, double dec) = 0;
    virtual void syncToTarget() = 0;
    virtual void syncToAltAz(double altitude, double azimuth) = 0;

    // Capabilities (ASCOM standard)
    virtual bool getCanSlew() const = 0;
    virtual bool getCanSlewAsync() const = 0;
    virtual bool getCanSlewAltAz() const = 0;
    virtual bool getCanSlewAltAzAsync() const = 0;
    virtual bool getCanSync() const = 0;
    virtual bool getCanSyncAltAz() const = 0;
    virtual bool getCanPark() const = 0;
    virtual bool getCanUnpark() const = 0;
    virtual bool getCanFindHome() const = 0;
    virtual bool getCanSetPark() const = 0;
    virtual bool getCanSetTracking() const = 0;
    virtual bool getCanSetGuideRates() const = 0;
    virtual bool getCanSetRightAscensionRate() const = 0;
    virtual bool getCanSetDeclinationRate() const = 0;
    virtual bool getCanSetPierSide() const = 0;
    virtual bool getCanPulseGuide() const = 0;

    // Tracking (ASCOM standard)
    virtual bool getTracking() const = 0;
    virtual void setTracking(bool value) = 0;
    virtual DriveRate getTrackingRate() const = 0;
    virtual void setTrackingRate(DriveRate value) = 0;
    virtual std::vector<DriveRate> getTrackingRates() const = 0;
    virtual double getRightAscensionRate() const = 0;
    virtual void setRightAscensionRate(double value) = 0;
    virtual double getDeclinationRate() const = 0;
    virtual void setDeclinationRate(double value) = 0;

    // Parking and homing (ASCOM standard)
    virtual void park() = 0;
    virtual void unpark() = 0;
    virtual bool getAtPark() const = 0;
    virtual void setPark() = 0;
    virtual void findHome() = 0;
    virtual bool getAtHome() const = 0;

    // Guide rates and pulse guiding (ASCOM standard)
    virtual double getGuideRateRightAscension() const = 0;
    virtual void setGuideRateRightAscension(double value) = 0;
    virtual double getGuideRateDeclination() const = 0;
    virtual void setGuideRateDeclination(double value) = 0;
    virtual void pulseGuide(GuideDirection direction, int duration) = 0;
    virtual bool getIsPulseGuiding() const = 0;

    // Site information (ASCOM standard)
    virtual double getSiteLatitude() const = 0;
    virtual void setSiteLatitude(double value) = 0;
    virtual double getSiteLongitude() const = 0;
    virtual void setSiteLongitude(double value) = 0;
    virtual double getSiteElevation() const = 0;
    virtual void setSiteElevation(double value) = 0;
    virtual double getSiderealTime() const = 0;
    virtual std::chrono::system_clock::time_point getUTCDate() const = 0;
    virtual void setUTCDate(const std::chrono::system_clock::time_point& value) = 0;

    // Pier side and alignment (ASCOM standard)
    virtual PierSide getSideOfPier() const = 0;
    virtual void setSideOfPier(PierSide value) = 0;
    virtual PierSide getDestinationSideOfPier(double ra, double dec) const = 0;
    virtual AlignmentMode getAlignmentMode() const = 0;
    virtual int getEquatorialSystem() const = 0;
    virtual double getFocalLength() const = 0;
    virtual double getApertureArea() const = 0;
    virtual double getApertureDiameter() const = 0;
    virtual bool getDoesRefraction() const = 0;
    virtual void setDoesRefraction(bool value) = 0;

    // Axis control (ASCOM standard)
    virtual bool canMoveAxis(int axis) const = 0;
    virtual std::vector<Rate> axisRates(int axis) const = 0;
    virtual void moveAxis(int axis, double rate) = 0;

    // Slew settle time (ASCOM standard)
    virtual double getSlewSettleTime() const = 0;
    virtual void setSlewSettleTime(double value) = 0;

    // Additional convenience methods
    virtual void getCurrentCoordinates(double& ra, double& dec) const = 0;

    // Convenience methods with different signatures to avoid conflicts
    virtual bool slewToCoordinatesSync(double ra, double dec) = 0;
    virtual bool syncToCoordinatesSync(double ra, double dec) = 0;
    virtual bool stopSlewingSync() = 0;
    virtual bool setTrackingMode(bool enabled) = 0;
};

/**
 * @brief 调焦器设备接口
 */
class IFocuser : public IMovable {
public:
    virtual ~IFocuser() = default;

    /**
     * @brief 获取温度（如果支持）
     */
    virtual double getTemperature() const = 0;

    /**
     * @brief 是否支持温度补偿
     */
    virtual bool supportsTemperatureCompensation() const = 0;

    /**
     * @brief 设置温度补偿
     */
    virtual bool setTemperatureCompensation(bool enabled) = 0;
};

/**
 * @brief 滤镜轮设备接口
 */
class IFilterWheel : public IMovable {
public:
    virtual ~IFilterWheel() = default;

    /**
     * @brief 获取滤镜数量
     */
    virtual int getFilterCount() const = 0;

    /**
     * @brief 获取当前滤镜位置
     */
    virtual int getCurrentFilter() const = 0;

    /**
     * @brief 设置滤镜位置
     */
    virtual bool setFilter(int position) = 0;

    /**
     * @brief 获取滤镜名称
     */
    virtual std::string getFilterName(int position) const = 0;

    /**
     * @brief 设置滤镜名称
     */
    virtual bool setFilterName(int position, const std::string& name) = 0;
};

/**
 * @brief Rotator device interface following ASCOM IRotatorV4 standard
 */
class IRotator : public IMovable {
public:
    virtual ~IRotator() = default;

    // Position control (ASCOM standard)
    virtual double getPosition() const = 0;
    virtual double getMechanicalPosition() const = 0;
    virtual double getTargetPosition() const = 0;
    virtual void move(double position) = 0;
    virtual void moveAbsolute(double position) = 0;
    virtual void moveMechanical(double position) = 0;
    virtual void halt() = 0;
    virtual bool getIsMoving() const = 0;

    // Configuration (ASCOM standard)
    virtual bool getCanReverse() const = 0;
    virtual bool getReverse() const = 0;
    virtual void setReverse(bool value) = 0;
    virtual double getStepSize() const = 0;
    virtual void sync(double position) = 0;

    // Legacy methods for backward compatibility
    virtual double getCurrentAngle() const = 0;
    virtual bool rotateToAngle(double angle) = 0;
    virtual bool rotateRelative(double angle) = 0;
    virtual bool supportsReverse() const = 0;
    virtual bool setReverseMode(bool reversed) = 0;
};

/**
 * @brief Dome device interface following ASCOM IDomeV3 standard
 */
class IDome {
public:
    virtual ~IDome() = default;

    // Azimuth control (ASCOM standard)
    virtual double getAzimuth() const = 0;
    virtual bool getCanSetAzimuth() const = 0;
    virtual void slewToAzimuth(double azimuth) = 0;
    virtual void syncToAzimuth(double azimuth) = 0;
    virtual bool getCanSyncAzimuth() const = 0;
    virtual void abortSlew() = 0;
    virtual bool getSlewing() const = 0;

    // Altitude control (ASCOM standard)
    virtual double getAltitude() const = 0;
    virtual bool getCanSetAltitude() const = 0;
    virtual void slewToAltitude(double altitude) = 0;

    // Shutter control (ASCOM standard)
    virtual ShutterState getShutterStatus() const = 0;
    virtual bool getCanSetShutter() const = 0;
    virtual void openShutter() = 0;
    virtual void closeShutter() = 0;

    // Parking and homing (ASCOM standard)
    virtual bool getCanPark() const = 0;
    virtual void park() = 0;
    virtual void setPark() = 0;
    virtual bool getAtPark() const = 0;
    virtual bool getCanFindHome() const = 0;
    virtual void findHome() = 0;
    virtual bool getAtHome() const = 0;

    // Slaving (ASCOM standard)
    virtual bool getCanSlave() const = 0;
    virtual bool getSlaved() const = 0;
    virtual void setSlaved(bool value) = 0;
};

/**
 * @brief Cover calibrator device interface following ASCOM ICoverCalibratorV2 standard
 */
class ICoverCalibrator {
public:
    virtual ~ICoverCalibrator() = default;

    // Cover control (ASCOM standard)
    virtual CoverState getCoverState() const = 0;
    virtual void openCover() = 0;
    virtual void closeCover() = 0;
    virtual void haltCover() = 0;
    virtual bool getCoverMoving() const = 0;

    // Calibrator control (ASCOM standard)
    virtual CalibratorState getCalibratorState() const = 0;
    virtual int getBrightness() const = 0;
    virtual void setBrightness(int value) = 0;
    virtual int getMaxBrightness() const = 0;
    virtual void calibratorOn(int brightness) = 0;
    virtual void calibratorOff() = 0;
    virtual bool getCalibratorChanging() const = 0;
};

/**
 * @brief Observing conditions device interface following ASCOM IObservingConditionsV2 standard
 */
class IObservingConditions {
public:
    virtual ~IObservingConditions() = default;

    // Environmental readings (ASCOM standard)
    virtual double getCloudCover() const = 0;
    virtual double getDewPoint() const = 0;
    virtual double getHumidity() const = 0;
    virtual double getPressure() const = 0;
    virtual double getRainRate() const = 0;
    virtual double getSkyBrightness() const = 0;
    virtual double getSkyQuality() const = 0;
    virtual double getSkyTemperature() const = 0;
    virtual double getStarFWHM() const = 0;
    virtual double getTemperature() const = 0;
    virtual double getWindDirection() const = 0;
    virtual double getWindGust() const = 0;
    virtual double getWindSpeed() const = 0;

    // Sensor management (ASCOM standard)
    virtual double getAveragePeriod() const = 0;
    virtual void setAveragePeriod(double value) = 0;
    virtual void refresh() = 0;
    virtual std::string sensorDescription(const std::string& propertyName) const = 0;
    virtual double timeSinceLastUpdate(const std::string& propertyName) const = 0;
};

/**
 * @brief Safety monitor device interface following ASCOM ISafetyMonitorV3 standard
 */
class ISafetyMonitor {
public:
    virtual ~ISafetyMonitor() = default;

    // Safety status (ASCOM standard)
    virtual bool getIsSafe() const = 0;
};

/**
 * @brief Switch device interface following ASCOM ISwitchV3 standard
 */
class ISwitch {
public:
    virtual ~ISwitch() = default;

    // Switch management (ASCOM standard)
    virtual int getMaxSwitch() const = 0;
    virtual bool canWrite(int id) const = 0;
    virtual bool getSwitch(int id) const = 0;
    virtual std::string getSwitchName(int id) const = 0;
    virtual void setSwitchName(int id, const std::string& name) = 0;
    virtual std::string getSwitchDescription(int id) const = 0;
    virtual double getSwitchValue(int id) const = 0;
    virtual double getMinSwitchValue(int id) const = 0;
    virtual double getMaxSwitchValue(int id) const = 0;
    virtual double getSwitchStep(int id) const = 0;
    virtual void setSwitch(int id, bool value) = 0;
    virtual void setSwitchValue(int id, double value) = 0;

    // Asynchronous operations (ASCOM standard)
    virtual bool canAsync(int id) const = 0;
    virtual void setAsync(int id, bool value) = 0;
    virtual void setAsyncValue(int id, double value) = 0;
    virtual bool stateChangeComplete(int id) const = 0;
    virtual void cancelAsync(int id) = 0;
};

} // namespace interfaces
} // namespace device
} // namespace astrocomm

#pragma once

#include "device_interface.h"
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
namespace ascom {

using json = nlohmann::json;

/**
 * @brief ASCOM Device Interface Adapter
 * 
 * Provides ASCOM-standard property names and method signatures,
 * translating between internal API and ASCOM requirements.
 */
class ASCOMDeviceAdapter {
public:
    virtual ~ASCOMDeviceAdapter() = default;

    // ASCOM Standard Device Properties
    virtual std::string Name() const = 0;
    virtual std::string Description() const = 0;
    virtual std::string DriverInfo() const = 0;
    virtual std::string DriverVersion() const = 0;
    virtual int InterfaceVersion() const = 0;
    virtual bool Connected() const = 0;
    virtual void setConnected(bool value) = 0;
    virtual bool Connecting() const = 0;
    virtual DeviceState DeviceState() const = 0;
    virtual std::vector<std::string> SupportedActions() const = 0;

    // ASCOM Standard Methods
    virtual std::string Action(const std::string& actionName, const std::string& actionParameters = "") = 0;
    virtual void CommandBlind(const std::string& command, bool raw = false) = 0;
    virtual bool CommandBool(const std::string& command, bool raw = false) = 0;
    virtual std::string CommandString(const std::string& command, bool raw = false) = 0;
    virtual void SetupDialog() = 0;
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;
};

/**
 * @brief ASCOM Camera Interface Adapter
 * 
 * Adapts internal camera interface to ASCOM ICameraV4 standard
 */
class ASCOMCameraAdapter : public ASCOMDeviceAdapter {
public:
    virtual ~ASCOMCameraAdapter() = default;

    // ASCOM Camera Properties
    virtual int BayerOffsetX() const = 0;
    virtual int BayerOffsetY() const = 0;
    virtual int BinX() const = 0;
    virtual void setBinX(int value) = 0;
    virtual int BinY() const = 0;
    virtual void setBinY(int value) = 0;
    virtual CameraState CameraState() const = 0;
    virtual int CameraXSize() const = 0;
    virtual int CameraYSize() const = 0;
    virtual bool CanAbortExposure() const = 0;
    virtual bool CanAsymmetricBin() const = 0;
    virtual bool CanFastReadout() const = 0;
    virtual bool CanGetCoolerPower() const = 0;
    virtual bool CanPulseGuide() const = 0;
    virtual bool CanSetCCDTemperature() const = 0;
    virtual bool CanStopExposure() const = 0;
    virtual double CCDTemperature() const = 0;
    virtual bool CoolerOn() const = 0;
    virtual void setCoolerOn(bool value) = 0;
    virtual double CoolerPower() const = 0;
    virtual double ElectronsPerADU() const = 0;
    virtual double ExposureMax() const = 0;
    virtual double ExposureMin() const = 0;
    virtual double ExposureResolution() const = 0;
    virtual bool FastReadout() const = 0;
    virtual void setFastReadout(bool value) = 0;
    virtual double FullWellCapacity() const = 0;
    virtual int Gain() const = 0;
    virtual void setGain(int value) = 0;
    virtual int GainMax() const = 0;
    virtual int GainMin() const = 0;
    virtual std::vector<std::string> Gains() const = 0;
    virtual bool HasShutter() const = 0;
    virtual double HeatSinkTemperature() const = 0;
    virtual std::vector<std::vector<int>> ImageArray() const = 0;
    virtual json ImageArrayVariant() const = 0;
    virtual bool ImageReady() const = 0;
    virtual bool IsPulseGuiding() const = 0;
    virtual double LastExposureDuration() const = 0;
    virtual std::chrono::system_clock::time_point LastExposureStartTime() const = 0;
    virtual int MaxADU() const = 0;
    virtual int MaxBinX() const = 0;
    virtual int MaxBinY() const = 0;
    virtual int NumX() const = 0;
    virtual void setNumX(int value) = 0;
    virtual int NumY() const = 0;
    virtual void setNumY(int value) = 0;
    virtual int Offset() const = 0;
    virtual void setOffset(int value) = 0;
    virtual int OffsetMax() const = 0;
    virtual int OffsetMin() const = 0;
    virtual std::vector<std::string> Offsets() const = 0;
    virtual double PercentCompleted() const = 0;
    virtual double PixelSizeX() const = 0;
    virtual double PixelSizeY() const = 0;
    virtual int ReadoutMode() const = 0;
    virtual void setReadoutMode(int value) = 0;
    virtual std::vector<std::string> ReadoutModes() const = 0;
    virtual std::string SensorName() const = 0;
    virtual SensorType SensorType() const = 0;
    virtual double SetCCDTemperature() const = 0;
    virtual void setSetCCDTemperature(double value) = 0;
    virtual int StartX() const = 0;
    virtual void setStartX(int value) = 0;
    virtual int StartY() const = 0;
    virtual void setStartY(int value) = 0;
    virtual double SubExposureDuration() const = 0;
    virtual void setSubExposureDuration(double value) = 0;

    // ASCOM Camera Methods
    virtual void AbortExposure() = 0;
    virtual void PulseGuide(GuideDirection direction, int duration) = 0;
    virtual void StartExposure(double duration, bool light = true) = 0;
    virtual void StopExposure() = 0;
};

/**
 * @brief ASCOM Telescope Interface Adapter
 * 
 * Adapts internal telescope interface to ASCOM ITelescopeV4 standard
 */
class ASCOMTelescopeAdapter : public ASCOMDeviceAdapter {
public:
    virtual ~ASCOMTelescopeAdapter() = default;

    // ASCOM Telescope Properties
    virtual AlignmentMode AlignmentMode() const = 0;
    virtual double Altitude() const = 0;
    virtual double ApertureArea() const = 0;
    virtual double ApertureDiameter() const = 0;
    virtual bool AtHome() const = 0;
    virtual bool AtPark() const = 0;
    virtual double Azimuth() const = 0;
    virtual bool CanFindHome() const = 0;
    virtual bool CanPark() const = 0;
    virtual bool CanPulseGuide() const = 0;
    virtual bool CanSetDeclinationRate() const = 0;
    virtual bool CanSetGuideRates() const = 0;
    virtual bool CanSetPark() const = 0;
    virtual bool CanSetPierSide() const = 0;
    virtual bool CanSetRightAscensionRate() const = 0;
    virtual bool CanSetTracking() const = 0;
    virtual bool CanSlew() const = 0;
    virtual bool CanSlewAltAz() const = 0;
    virtual bool CanSlewAltAzAsync() const = 0;
    virtual bool CanSlewAsync() const = 0;
    virtual bool CanSync() const = 0;
    virtual bool CanSyncAltAz() const = 0;
    virtual bool CanUnpark() const = 0;
    virtual double Declination() const = 0;
    virtual double DeclinationRate() const = 0;
    virtual void setDeclinationRate(double value) = 0;
    virtual bool DoesRefraction() const = 0;
    virtual void setDoesRefraction(bool value) = 0;
    virtual int EquatorialSystem() const = 0;
    virtual double FocalLength() const = 0;
    virtual double GuideRateDeclination() const = 0;
    virtual void setGuideRateDeclination(double value) = 0;
    virtual double GuideRateRightAscension() const = 0;
    virtual void setGuideRateRightAscension(double value) = 0;
    virtual bool IsPulseGuiding() const = 0;
    virtual double RightAscension() const = 0;
    virtual double RightAscensionRate() const = 0;
    virtual void setRightAscensionRate(double value) = 0;
    virtual PierSide SideOfPier() const = 0;
    virtual void setSideOfPier(PierSide value) = 0;
    virtual double SiderealTime() const = 0;
    virtual double SiteElevation() const = 0;
    virtual void setSiteElevation(double value) = 0;
    virtual double SiteLatitude() const = 0;
    virtual void setSiteLatitude(double value) = 0;
    virtual double SiteLongitude() const = 0;
    virtual void setSiteLongitude(double value) = 0;
    virtual double SlewSettleTime() const = 0;
    virtual void setSlewSettleTime(double value) = 0;
    virtual bool Slewing() const = 0;
    virtual double TargetDeclination() const = 0;
    virtual void setTargetDeclination(double value) = 0;
    virtual double TargetRightAscension() const = 0;
    virtual void setTargetRightAscension(double value) = 0;
    virtual bool Tracking() const = 0;
    virtual void setTracking(bool value) = 0;
    virtual DriveRate TrackingRate() const = 0;
    virtual void setTrackingRate(DriveRate value) = 0;
    virtual std::vector<DriveRate> TrackingRates() const = 0;
    virtual std::chrono::system_clock::time_point UTCDate() const = 0;
    virtual void setUTCDate(const std::chrono::system_clock::time_point& value) = 0;

    // ASCOM Telescope Methods
    virtual void AbortSlew() = 0;
    virtual std::vector<Rate> AxisRates(int axis) const = 0;
    virtual bool CanMoveAxis(int axis) const = 0;
    virtual PierSide DestinationSideOfPier(double ra, double dec) const = 0;
    virtual void FindHome() = 0;
    virtual void MoveAxis(int axis, double rate) = 0;
    virtual void Park() = 0;
    virtual void PulseGuide(GuideDirection direction, int duration) = 0;
    virtual void SetPark() = 0;
    virtual void SlewToAltAz(double altitude, double azimuth) = 0;
    virtual void SlewToAltAzAsync(double altitude, double azimuth) = 0;
    virtual void SlewToCoordinates(double ra, double dec) = 0;
    virtual void SlewToCoordinatesAsync(double ra, double dec) = 0;
    virtual void SlewToTarget() = 0;
    virtual void SlewToTargetAsync() = 0;
    virtual void SyncToAltAz(double altitude, double azimuth) = 0;
    virtual void SyncToCoordinates(double ra, double dec) = 0;
    virtual void SyncToTarget() = 0;
    virtual void Unpark() = 0;
};

/**
 * @brief ASCOM Focuser Interface Adapter
 * 
 * Adapts internal focuser interface to ASCOM IFocuserV4 standard
 */
class ASCOMFocuserAdapter : public ASCOMDeviceAdapter {
public:
    virtual ~ASCOMFocuserAdapter() = default;

    // ASCOM Focuser Properties
    virtual bool Absolute() const = 0;
    virtual bool IsMoving() const = 0;
    virtual bool Link() const = 0;
    virtual void setLink(bool value) = 0;
    virtual int MaxIncrement() const = 0;
    virtual int MaxStep() const = 0;
    virtual int Position() const = 0;
    virtual double StepSize() const = 0;
    virtual bool TempComp() const = 0;
    virtual void setTempComp(bool value) = 0;
    virtual bool TempCompAvailable() const = 0;
    virtual double Temperature() const = 0;

    // ASCOM Focuser Methods
    virtual void Halt() = 0;
    virtual void Move(int position) = 0;
};

} // namespace ascom
} // namespace interfaces
} // namespace device
} // namespace astrocomm

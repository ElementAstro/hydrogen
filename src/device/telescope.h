#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace device {

using json = nlohmann::json;

/**
 * @brief Telescope mount type enumeration
 */
enum class MountType {
  EQUATORIAL,     // Equatorial mount
  ALT_AZIMUTH,    // Alt-azimuth mount
  DOBSONIAN,      // Dobsonian mount
  FORK,           // Fork mount
  GERMAN_EQUATORIAL // German equatorial mount
};

/**
 * @brief Tracking mode enumeration
 */
enum class TrackingMode {
  SIDEREAL,       // Sidereal tracking
  LUNAR,          // Lunar tracking
  SOLAR,          // Solar tracking
  CUSTOM,         // Custom tracking rate
  OFF             // Tracking off
};

/**
 * @brief Slewing speed enumeration
 */
enum class SlewSpeed {
  GUIDE = 1,      // Guide speed (0.5x sidereal)
  CENTERING = 2,  // Centering speed (8x sidereal)
  FIND = 3,       // Find speed (64x sidereal)
  MAX = 4         // Maximum speed
};

/**
 * @brief Telescope coordinates structure
 */
struct TelescopeCoordinates {
  double ra;      // Right ascension (hours)
  double dec;     // Declination (degrees)
  double alt;     // Altitude (degrees)
  double az;      // Azimuth (degrees)
  double lst;     // Local sidereal time (hours)
  std::string timestamp; // Timestamp of coordinates
};

/**
 * @brief 望远镜设备实�? *
 * 基于新架构的望远镜实现，提供完整的望远镜控制功能�? * 支持多种制造商的望远镜设备，提供统一的控制接口�? */
class Telescope : public core::ModernDeviceBase, 
                  public interfaces::ITelescope {
public:
  /**
   * @brief Constructor
   * @param deviceId Device identifier
   * @param manufacturer Manufacturer name
   * @param model Model name
   */
  Telescope(const std::string &deviceId, 
            const std::string &manufacturer = "Celestron",
            const std::string &model = "NexStar Evolution");

  /**
   * @brief Virtual destructor
   */
  virtual ~Telescope();

  /**
   * @brief 获取设备类型名称
   */
  static std::string getDeviceTypeName() { return "TELESCOPE"; }

  /**
   * @brief 获取支持的制造商列表
   */
  static std::vector<std::string> getSupportedManufacturers() {
    return {"Celestron", "Meade", "Skywatcher", "Orion", "Losmandy", "Generic"};
  }

  /**
   * @brief 获取支持的型号列�?   */
  static std::vector<std::string> getSupportedModels(const std::string& manufacturer) {
    if (manufacturer == "Celestron") return {"NexStar Evolution", "CGX", "CGX-L", "AVX"};
    if (manufacturer == "Meade") return {"LX200", "LX600", "LX850", "ETX"};
    if (manufacturer == "Skywatcher") return {"EQ6-R", "EQM-35", "AZ-EQ6", "Star Adventurer"};
    if (manufacturer == "Orion") return {"Atlas EQ-G", "Sirius EQ-G", "SkyView Pro"};
    if (manufacturer == "Losmandy") return {"G11", "GM8", "Titan"};
    return {"Generic Telescope"};
  }

  // Implement ITelescope interface - Coordinate properties
  double getRightAscension() const override;
  double getDeclination() const override;
  double getAltitude() const override;
  double getAzimuth() const override;
  double getTargetRightAscension() const override;
  void setTargetRightAscension(double value) override;
  double getTargetDeclination() const override;
  void setTargetDeclination(double value) override;

  // Slewing methods
  void slewToCoordinates(double ra, double dec) override;
  void slewToCoordinatesAsync(double ra, double dec) override;
  void slewToTarget() override;
  void slewToTargetAsync() override;
  void slewToAltAz(double altitude, double azimuth) override;
  void slewToAltAzAsync(double altitude, double azimuth) override;
  void abortSlew() override;
  bool getSlewing() const override;

  // Synchronization
  void syncToCoordinates(double ra, double dec) override;
  void syncToTarget() override;
  void syncToAltAz(double altitude, double azimuth) override;

  // Capabilities
  bool getCanSlew() const override;
  bool getCanSlewAsync() const override;
  bool getCanSlewAltAz() const override;
  bool getCanSlewAltAzAsync() const override;
  bool getCanSync() const override;
  bool getCanSyncAltAz() const override;
  bool getCanPark() const override;
  bool getCanUnpark() const override;
  bool getCanFindHome() const override;
  bool getCanSetPark() const override;
  bool getCanSetTracking() const override;
  bool getCanSetGuideRates() const override;
  bool getCanSetRightAscensionRate() const override;
  bool getCanSetDeclinationRate() const override;
  bool getCanSetPierSide() const override;
  bool getCanPulseGuide() const override;

  // Tracking
  bool getTracking() const override;
  void setTracking(bool value) override;
  interfaces::DriveRate getTrackingRate() const override;
  void setTrackingRate(interfaces::DriveRate value) override;
  std::vector<interfaces::DriveRate> getTrackingRates() const override;
  double getRightAscensionRate() const override;
  void setRightAscensionRate(double value) override;
  double getDeclinationRate() const override;
  void setDeclinationRate(double value) override;

  // Parking and homing
  void park() override;
  void unpark() override;
  bool getAtPark() const override;
  void setPark() override;
  void findHome() override;
  bool getAtHome() const override;

  // Guide rates and pulse guiding
  double getGuideRateRightAscension() const override;
  void setGuideRateRightAscension(double value) override;
  double getGuideRateDeclination() const override;
  void setGuideRateDeclination(double value) override;
  void pulseGuide(interfaces::GuideDirection direction, int duration) override;
  bool getIsPulseGuiding() const override;

  // Site information
  double getSiteLatitude() const override;
  void setSiteLatitude(double value) override;
  double getSiteLongitude() const override;
  void setSiteLongitude(double value) override;
  double getSiteElevation() const override;
  void setSiteElevation(double value) override;
  double getSiderealTime() const override;
  std::chrono::system_clock::time_point getUTCDate() const override;
  void setUTCDate(const std::chrono::system_clock::time_point& value) override;

  // Pier side and alignment
  interfaces::PierSide getSideOfPier() const override;
  void setSideOfPier(interfaces::PierSide value) override;
  interfaces::PierSide getDestinationSideOfPier(double ra, double dec) const override;
  interfaces::AlignmentMode getAlignmentMode() const override;
  int getEquatorialSystem() const override;
  double getFocalLength() const override;
  double getApertureArea() const override;
  double getApertureDiameter() const override;
  bool getDoesRefraction() const override;
  void setDoesRefraction(bool value) override;

  // Axis control
  bool canMoveAxis(int axis) const override;
  std::vector<interfaces::Rate> axisRates(int axis) const override;
  void moveAxis(int axis, double rate) override;

  // Slew settle time
  double getSlewSettleTime() const override;
  void setSlewSettleTime(double value) override;

  // Additional convenience methods
  void getCurrentCoordinates(double& ra, double& dec) const override;
  bool slewToCoordinatesSync(double ra, double dec) override;
  bool syncToCoordinatesSync(double ra, double dec) override;
  bool stopSlewingSync() override;
  bool setTrackingMode(bool enabled) override;

  // ==== 扩展功能接口（向后兼容） ====

  /**
   * @brief Goto position (向后兼容)
   */
  virtual void gotoPosition(double ra, double dec);

  // setTracking is already declared above in ITelescope interface

  /**
   * @brief Set slew rate (向后兼容)
   */
  virtual void setSlewRate(int rate);

  /**
   * @brief Abort movement (向后兼容)
   */
  virtual void abort();

  // ==== 新架构扩展功�?====

  /**
   * @brief Set mount type
   */
  virtual bool setMountType(MountType type);

  /**
   * @brief Get mount type
   */
  virtual MountType getMountType() const;

  /**
   * @brief Set tracking mode
   */
  virtual bool setTrackingMode(TrackingMode mode);

  /**
   * @brief Get tracking mode
   */
  virtual TrackingMode getTrackingMode() const;

  /**
   * @brief Set custom tracking rate
   */
  virtual bool setCustomTrackingRate(double raRate, double decRate);

  /**
   * @brief Get custom tracking rate
   */
  virtual void getCustomTrackingRate(double& raRate, double& decRate) const;

  /**
   * @brief Start slewing at specified speed
   */
  virtual bool startSlewing(SlewSpeed speed, bool north, bool south, bool east, bool west);

  /**
   * @brief Stop slewing in specific direction
   */
  virtual bool stopSlewing(bool north, bool south, bool east, bool west);

  /**
   * @brief Set slewing speed
   */
  virtual bool setSlewSpeed(SlewSpeed speed);

  /**
   * @brief Get current slewing speed
   */
  virtual SlewSpeed getSlewSpeed() const;

  /**
   * @brief Perform goto operation
   */
  virtual bool gotoCoordinates(double ra, double dec, bool synchronous = false);

  /**
   * @brief Perform sync operation
   */
  virtual bool syncCoordinates(double ra, double dec);

  /**
   * @brief Get current coordinates
   */
  virtual TelescopeCoordinates getCurrentCoordinates() const;

  /**
   * @brief Set site location
   */
  virtual bool setSiteLocation(double latitude, double longitude, double elevation);

  /**
   * @brief Get site location
   */
  virtual void getSiteLocation(double& latitude, double& longitude, double& elevation) const;

  /**
   * @brief Set time and date
   */
  virtual bool setDateTime(const std::string& datetime);

  /**
   * @brief Get time and date
   */
  virtual std::string getDateTime() const;

  /**
   * @brief Perform alignment
   */
  virtual bool performAlignment(const std::vector<std::pair<double, double>>& alignmentStars);

  /**
   * @brief Get alignment status
   */
  virtual json getAlignmentStatus() const;

  /**
   * @brief Home telescope
   */
  virtual bool home();

  // unpark is already declared above in ITelescope interface

  /**
   * @brief Get pier side
   */
  virtual std::string getPierSide() const;

  /**
   * @brief Perform meridian flip
   */
  virtual bool performMeridianFlip();

  /**
   * @brief Check if meridian flip is needed
   */
  virtual bool isMeridianFlipNeeded() const;

  /**
   * @brief Wait for slewing to complete
   */
  bool waitForSlewComplete(int timeoutMs = 0);

protected:
  // 重写基类方法
  bool initializeDevice() override;
  bool startDevice() override;
  void stopDevice() override;
  bool handleDeviceCommand(const std::string& command,
                          const json& parameters,
                          json& result) override;
  void updateDevice() override;

private:
  // 硬件抽象接口
  virtual bool executeSlew(double ra, double dec);
  virtual bool executeSync(double ra, double dec);
  virtual bool executeStop();
  virtual bool executePark();
  virtual bool executeUnpark();
  virtual bool executeHome();
  virtual TelescopeCoordinates readCurrentCoordinates();
  virtual bool setTrackingEnabled(bool enabled);

  // 坐标转换函数
  void equatorialToAltAz(double ra, double dec, double& alt, double& az) const;
  void altAzToEquatorial(double alt, double az, double& ra, double& dec) const;
  double calculateLST() const;

  // 移动线程函数
  void slewThreadFunction();

private:
  // 望远镜参数
  MountType mountType_;
  TrackingMode trackingMode_;
  SlewSpeed slewSpeed_;
  
  // 位置信息
  std::atomic<double> currentRA_;
  std::atomic<double> currentDec_;
  std::atomic<double> targetRA_;
  std::atomic<double> targetDec_;
  
  // 站点信息
  std::atomic<double> siteLatitude_;
  std::atomic<double> siteLongitude_;
  std::atomic<double> siteElevation_;
  
  // 状态信息
  std::atomic<bool> isSlewing_;
  std::atomic<bool> isTracking_;
  std::atomic<bool> isParked_;
  std::atomic<bool> isAligned_;
  
  // 跟踪速率
  std::atomic<double> customRARate_;
  std::atomic<double> customDecRate_;
  
  // 移动线程
  std::thread slewThread_;
  std::atomic<bool> slewThreadRunning_;
  
  // 移动完成条件变量
  mutable std::mutex slewCompleteMutex_;
  std::condition_variable slewCompleteCV_;
  
  // 对齐数据
  mutable std::mutex alignmentMutex_;
  std::vector<std::pair<std::pair<double, double>, std::pair<double, double>>> alignmentData_;
};

/**
 * @brief 望远镜工�? */
class TelescopeFactory : public core::TypedDeviceFactory<Telescope> {
public:
  TelescopeFactory(const std::string& manufacturer = "Generic", 
                   const std::string& model = "Telescope")
      : TypedDeviceFactory<Telescope>(manufacturer, model) {}
};

} // namespace device
} // namespace hydrogen

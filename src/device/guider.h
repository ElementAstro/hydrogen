#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace astrocomm {
namespace device {

using json = nlohmann::json;

/**
 * @brief Guide direction enumeration
 */
enum class GuideDirection {
  NORTH,    // North direction
  SOUTH,    // South direction
  EAST,     // East direction
  WEST      // West direction
};

/**
 * @brief Guide command structure
 */
struct GuideCommand {
  GuideDirection direction;   // Guide direction
  int duration;              // Guide duration (milliseconds)
  std::string commandId;     // Command identifier
  std::chrono::system_clock::time_point timestamp; // Command timestamp
};

/**
 * @brief Guide statistics structure
 */
struct GuideStatistics {
  double rmsRA;              // RMS error in RA (arcseconds)
  double rmsDec;             // RMS error in Dec (arcseconds)
  double rmsTotal;           // Total RMS error (arcseconds)
  double maxRA;              // Maximum RA error (arcseconds)
  double maxDec;             // Maximum Dec error (arcseconds)
  int totalCommands;         // Total guide commands sent
  double averageDuration;    // Average guide duration (milliseconds)
  std::string sessionStart; // Session start time
  double sessionDuration;   // Session duration (seconds)
};

/**
 * @brief 导星器设备实现
 *
 * 基于新架构的导星器实现，提供完整的导星控制功能。
 * 支持多种制造商的导星器设备，提供统一的控制接口。
 */
class Guider : public core::ModernDeviceBase {
public:
  /**
   * @brief Constructor
   * @param deviceId Device identifier
   * @param manufacturer Manufacturer name
   * @param model Model name
   */
  Guider(const std::string &deviceId, 
         const std::string &manufacturer = "ZWO",
         const std::string &model = "ASI120MM-Mini");

  /**
   * @brief Virtual destructor
   */
  virtual ~Guider();

  /**
   * @brief 获取设备类型名称
   */
  static std::string getDeviceTypeName() { return "GUIDER"; }

  /**
   * @brief 获取支持的制造商列表
   */
  static std::vector<std::string> getSupportedManufacturers() {
    return {"ZWO", "QHY", "SBIG", "Atik", "Lodestar", "Generic"};
  }

  /**
   * @brief 获取支持的型号列表
   */
  static std::vector<std::string> getSupportedModels(const std::string& manufacturer) {
    if (manufacturer == "ZWO") return {"ASI120MM-Mini", "ASI290MM-Mini", "ASI174MM-Mini"};
    if (manufacturer == "QHY") return {"QHY5L-II", "QHY5P-II", "QHY174GPS"};
    if (manufacturer == "SBIG") return {"ST-i", "STF-8050"};
    if (manufacturer == "Atik") return {"Titan", "GP"};
    if (manufacturer == "Lodestar") return {"Lodestar", "Lodestar X2"};
    return {"Generic Guider"};
  }

  // ==== 导星控制接口 ====

  /**
   * @brief Send guide pulse
   */
  virtual bool guide(GuideDirection direction, int duration);

  /**
   * @brief Send guide pulse (async)
   */
  virtual bool guideAsync(GuideDirection direction, int duration, const std::string& commandId = "");

  /**
   * @brief Stop all guide pulses
   */
  virtual bool stopGuiding();

  /**
   * @brief Check if currently guiding
   */
  virtual bool isGuiding() const;

  /**
   * @brief Set guide rates
   */
  virtual bool setGuideRates(double raRate, double decRate);

  /**
   * @brief Get guide rates
   */
  virtual void getGuideRates(double& raRate, double& decRate) const;

  /**
   * @brief Set maximum guide duration
   */
  virtual bool setMaxGuideDuration(int maxDuration);

  /**
   * @brief Get maximum guide duration
   */
  virtual int getMaxGuideDuration() const;

  /**
   * @brief Enable/disable guide output
   */
  virtual bool setGuideOutputEnabled(bool enabled);

  /**
   * @brief Check if guide output is enabled
   */
  virtual bool isGuideOutputEnabled() const;

  /**
   * @brief Calibrate guider
   */
  virtual bool calibrate(int calibrationSteps = 10, int stepDuration = 1000);

  /**
   * @brief Check if calibrated
   */
  virtual bool isCalibrated() const;

  /**
   * @brief Get calibration data
   */
  virtual json getCalibrationData() const;

  /**
   * @brief Clear calibration
   */
  virtual bool clearCalibration();

  // ==== 统计和监控接口 ====

  /**
   * @brief Get guide statistics
   */
  virtual GuideStatistics getGuideStatistics() const;

  /**
   * @brief Reset guide statistics
   */
  virtual bool resetGuideStatistics();

  /**
   * @brief Get recent guide commands
   */
  virtual std::vector<GuideCommand> getRecentGuideCommands(int count = 100) const;

  /**
   * @brief Start guide session
   */
  virtual bool startGuideSession();

  /**
   * @brief Stop guide session
   */
  virtual bool stopGuideSession();

  /**
   * @brief Check if guide session is active
   */
  virtual bool isGuideSessionActive() const;

  /**
   * @brief Export guide log
   */
  virtual bool exportGuideLog(const std::string& filename) const;

  /**
   * @brief Set guide logging enabled
   */
  virtual bool setGuideLoggingEnabled(bool enabled);

  /**
   * @brief Check if guide logging is enabled
   */
  virtual bool isGuideLoggingEnabled() const;

  // ==== 高级功能接口 ====

  /**
   * @brief Set guide algorithm parameters
   */
  virtual bool setGuideAlgorithmParameters(const json& parameters);

  /**
   * @brief Get guide algorithm parameters
   */
  virtual json getGuideAlgorithmParameters() const;

  /**
   * @brief Enable/disable dithering
   */
  virtual bool setDitheringEnabled(bool enabled);

  /**
   * @brief Check if dithering is enabled
   */
  virtual bool isDitheringEnabled() const;

  /**
   * @brief Perform dithering
   */
  virtual bool dither(double amount = 5.0);

  /**
   * @brief Set backlash compensation
   */
  virtual bool setBacklashCompensation(int northSteps, int southSteps, int eastSteps, int westSteps);

  /**
   * @brief Get backlash compensation
   */
  virtual void getBacklashCompensation(int& northSteps, int& southSteps, int& eastSteps, int& westSteps) const;

  /**
   * @brief Wait for guide command to complete
   */
  bool waitForGuideComplete(const std::string& commandId, int timeoutMs = 0);

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
  virtual bool executeGuide(GuideDirection direction, int duration);
  virtual bool executeStopGuide();

  // 导星线程函数
  void guideThreadFunction();

  // 统计更新函数
  void updateStatistics(const GuideCommand& command, bool success);

  // 校准函数
  bool performCalibration(int steps, int duration);

private:
  // 导星参数
  std::atomic<double> raGuideRate_;        // RA guide rate (arcsec/sec)
  std::atomic<double> decGuideRate_;       // Dec guide rate (arcsec/sec)
  std::atomic<int> maxGuideDuration_;      // Maximum guide duration (ms)
  std::atomic<bool> guideOutputEnabled_;   // Guide output enabled
  std::atomic<bool> calibrated_;           // Calibration status
  std::atomic<bool> ditheringEnabled_;     // Dithering enabled
  std::atomic<bool> loggingEnabled_;       // Logging enabled

  // 导星状态
  std::atomic<bool> isGuiding_;
  std::atomic<bool> sessionActive_;
  std::chrono::system_clock::time_point sessionStartTime_;

  // 导星队列
  mutable std::mutex guideQueueMutex_;
  std::queue<GuideCommand> guideQueue_;
  std::condition_variable guideQueueCV_;

  // 导星线程
  std::thread guideThread_;
  std::atomic<bool> guideThreadRunning_;

  // 统计数据
  mutable std::mutex statisticsMutex_;
  GuideStatistics statistics_;
  std::vector<GuideCommand> recentCommands_;

  // 校准数据
  mutable std::mutex calibrationMutex_;
  json calibrationData_;

  // 反冲补偿
  std::atomic<int> backlashNorth_;
  std::atomic<int> backlashSouth_;
  std::atomic<int> backlashEast_;
  std::atomic<int> backlashWest_;

  // 算法参数
  mutable std::mutex algorithmMutex_;
  json algorithmParameters_;

  // 命令完成条件变量
  mutable std::mutex commandCompleteMutex_;
  std::condition_variable commandCompleteCV_;
  std::unordered_map<std::string, bool> completedCommands_;
};

/**
 * @brief 导星器工厂
 */
class GuiderFactory : public core::TypedDeviceFactory<Guider> {
public:
  GuiderFactory(const std::string& manufacturer = "Generic", 
                const std::string& model = "Guider")
      : TypedDeviceFactory<Guider>(manufacturer, model) {}
};

} // namespace device
} // namespace astrocomm

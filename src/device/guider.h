#pragma once
#include "device/device_base.h"
#include <array>
#include <atomic>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace astrocomm {

// 导星状态枚举
enum class GuiderState { IDLE, CALIBRATING, GUIDING, PAUSED, ERROR };

// 校准状态
enum class CalibrationState {
  IDLE,
  NORTH_MOVING,
  NORTH_ANALYZING,
  SOUTH_MOVING,
  SOUTH_ANALYZING,
  EAST_MOVING,
  EAST_ANALYZING,
  WEST_MOVING,
  WEST_ANALYZING,
  COMPLETED,
  FAILED
};

// 导星器设备类
class Guider : public DeviceBase {
public:
  Guider(const std::string &deviceId, const std::string &manufacturer = "QHY",
         const std::string &model = "QHY5-II");
  virtual ~Guider();

  // 重写启动和停止方法
  virtual bool start() override;
  virtual void stop() override;

  // 导星器特定方法
  void startGuiding();
  void stopGuiding();
  void pauseGuiding();
  void resumeGuiding();
  void startCalibration();
  void cancelCalibration();
  void dither(double amount, bool settle = true);
  void setCalibratedPixelScale(double scale);
  void setAggressiveness(double ra, double dec);
  void setGuideRate(double ra, double dec);

protected:
  // 模拟导星器的状态更新线程
  void updateLoop();

  // 处理导星计算
  void calculateGuidingCorrections();

  // 模拟恒星漂移
  std::array<double, 2> simulateDrift();

  // 模拟导星捕获图像
  void captureGuideImage();

  // 处理校准步骤
  void processCalibration();

  // 生成导星图像数据
  std::vector<uint8_t> generateGuideImageData();

  // 发送校准完成事件
  void sendCalibrationCompletedEvent(const std::string &relatedMessageId);

  // 发送导星状态事件
  void sendGuidingStatusEvent();

  // 命令处理器
  void handleStartGuidingCommand(const CommandMessage &cmd,
                                 ResponseMessage &response);
  void handleStopGuidingCommand(const CommandMessage &cmd,
                                ResponseMessage &response);
  void handlePauseGuidingCommand(const CommandMessage &cmd,
                                 ResponseMessage &response);
  void handleResumeGuidingCommand(const CommandMessage &cmd,
                                  ResponseMessage &response);
  void handleStartCalibrationCommand(const CommandMessage &cmd,
                                     ResponseMessage &response);
  void handleCancelCalibrationCommand(const CommandMessage &cmd,
                                      ResponseMessage &response);
  void handleDitherCommand(const CommandMessage &cmd,
                           ResponseMessage &response);
  void handleSetParametersCommand(const CommandMessage &cmd,
                                  ResponseMessage &response);

private:
  // 状态
  GuiderState state;
  CalibrationState calibrationState;

  // 星体跟踪参数
  double guideStarX;  // 导星星体X位置
  double guideStarY;  // 导星星体Y位置
  double targetStarX; // 目标X位置
  double targetStarY; // 目标Y位置
  double driftX;      // X方向漂移
  double driftY;      // Y方向漂移

  // 校准参数
  struct CalibrationData {
    double raAngle;  // RA轴角度
    double decAngle; // DEC轴角度
    double raRate;   // RA速率 (像素/秒)
    double decRate;  // DEC速率 (像素/秒)
    bool flipped;    // 镜像翻转
    bool calibrated; // 是否已校准
  } calibration;

  // 控制参数
  double raAggressiveness;  // RA轴修正积极性 (0-1)
  double decAggressiveness; // DEC轴修正积极性 (0-1)
  double raGuideRate;       // RA轴导星速率 (恒星速率的倍数)
  double decGuideRate;      // DEC轴导星速率 (恒星速率的倍数)
  double pixelScale;        // 像素尺度 (角秒/像素)

  // 修正输出
  double raCorrection;      // RA修正量 (毫秒)
  double decCorrection;     // DEC修正量 (毫秒)
  double lastRaCorrection;  // 上次RA修正
  double lastDecCorrection; // 上次DEC修正

  // 性能指标
  double rms;  // 均方根误差
  double peak; // 峰值误差

  // 抖动相关
  bool isSettling;          // 是否正在稳定
  double settleThreshold;   // 稳定阈值
  int settleFrames;         // 达到阈值的帧数
  int requiredSettleFrames; // 需要的稳定帧数

  // 图像相关
  int imageWidth;
  int imageHeight;
  std::vector<uint8_t> imageData;

  // 时间相关
  double exposureTime;     // 曝光时间 (秒)
  int64_t lastCaptureTime; // 上次拍摄时间
  int64_t guideStartTime;  // 导星开始时间
  int totalFramesCapured;  // 总捕获帧数

  // 校准相关
  std::string currentCalibrationMessageId;

  // 随机数生成器
  std::mt19937 rng;

  // 更新线程
  std::thread updateThread;
  std::atomic<bool> updateRunning;

  // 线程安全的状态更新
  mutable std::mutex statusMutex;

  // 辅助函数
  std::string guiderStateToString(GuiderState state) const;
  std::string calibrationStateToString(CalibrationState state) const;
};

} // namespace astrocomm
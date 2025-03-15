#pragma once

#include "device/device_base.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace astrocomm {

// 导星状态枚举
enum class GuiderState {
  DISCONNECTED, // 未连接
  CONNECTED,    // 已连接但未导星
  CALIBRATING,  // 校准中
  GUIDING,      // 导星中
  PAUSED,       // 导星暂停
  SETTLING,     // 抖动后稳定中
  ERROR         // 错误状态
};

// 校准状态
enum class CalibrationState {
  IDLE,
  NORTH_MOVING,
  NORTH_COMPLETE,
  SOUTH_MOVING,
  SOUTH_COMPLETE,
  EAST_MOVING,
  EAST_COMPLETE,
  WEST_MOVING,
  WEST_COMPLETE,
  COMPLETED,
  FAILED
};

// 导星器接口类型
enum class GuiderInterfaceType {
  PHD2,                  // PHD2
  LINGUIDER,             // Lin-guider
  METAGUIDE,             // MetaGuide
  DIREKTGUIDER,          // DirectGuide
  ASTROPHOTOGRAPHY_TOOL, // APT
  KSTARS_EKOS,           // KStars/EKOS
  MAXIM_DL,              // MaxIm DL
  ASTROART,              // AstroArt
  ASTAP,                 // ASTAP
  VOYAGER,               // Voyager
  NINA,                  // N.I.N.A
  CUSTOM                 // 自定义接口
};

// 导星修正结构
struct GuidingCorrection {
  double raCorrection;  // RA修正 (毫秒)
  double decCorrection; // DEC修正 (毫秒)
  double raRaw;         // 原始RA误差 (像素)
  double decRaw;        // 原始DEC误差 (像素)

  GuidingCorrection()
      : raCorrection(0), decCorrection(0), raRaw(0), decRaw(0) {}
};

// 校准数据结构
struct CalibrationData {
  double raAngle;  // RA轴角度
  double decAngle; // DEC轴角度
  double raRate;   // RA速率 (像素/秒)
  double decRate;  // DEC速率 (像素/秒)
  bool flipped;    // 镜像翻转
  bool calibrated; // 是否已校准

  CalibrationData()
      : raAngle(0), decAngle(90), raRate(0), decRate(0), flipped(false),
        calibrated(false) {}
};

// 导星星点信息
struct StarInfo {
  double x;    // X坐标
  double y;    // Y坐标
  double flux; // 亮度
  double snr;  // 信噪比
  bool locked; // 是否锁定

  StarInfo() : x(0), y(0), flux(0), snr(0), locked(false) {}
  StarInfo(double x, double y) : x(x), y(y), flux(0), snr(0), locked(false) {}
};

// 导星统计数据
struct GuiderStats {
  double rms;         // 总体RMS (像素)
  double rmsRa;       // RA方向RMS (像素)
  double rmsDec;      // DEC方向RMS (像素)
  double peak;        // 峰值误差 (像素)
  int totalFrames;    // 总帧数
  double snr;         // 信噪比
  double elapsedTime; // 导星持续时间(秒)

  GuiderStats()
      : rms(0), rmsRa(0), rmsDec(0), peak(0), totalFrames(0), snr(0),
        elapsedTime(0) {}
};

// 导星接口基类 - 负责与外部导星程序通信
class GuiderInterface {
public:
  virtual ~GuiderInterface() = default;

  // 连接到导星软件
  virtual bool connect(const std::string &host, int port) = 0;

  // 断开连接
  virtual void disconnect() = 0;

  // 是否已连接
  virtual bool isConnected() const = 0;

  // 开始导星
  virtual bool startGuiding() = 0;

  // 停止导星
  virtual bool stopGuiding() = 0;

  // 暂停导星
  virtual bool pauseGuiding() = 0;

  // 恢复导星
  virtual bool resumeGuiding() = 0;

  // 开始校准
  virtual bool startCalibration() = 0;

  // 取消校准
  virtual bool cancelCalibration() = 0;

  // 执行抖动
  virtual bool dither(double amount, double settleTime = 5.0,
                      double settlePixels = 1.5) = 0;

  // 获取当前状态
  virtual GuiderState getGuiderState() const = 0;

  // 获取校准状态
  virtual CalibrationState getCalibrationState() const = 0;

  // 获取导星统计
  virtual GuiderStats getStats() const = 0;

  // 获取当前导星星点
  virtual StarInfo getGuideStar() const = 0;

  // 获取校准数据
  virtual CalibrationData getCalibrationData() const = 0;

  // 设置像素比例
  virtual void setPixelScale(double scaleArcsecPerPixel) = 0;

  // 设置导星速率
  virtual void setGuideRate(double raRateMultiplier,
                            double decRateMultiplier) = 0;

  // 获取当前修正
  virtual GuidingCorrection getCurrentCorrection() const = 0;

  // 获取接口类型
  virtual GuiderInterfaceType getInterfaceType() const = 0;

  // 获取接口名称
  virtual std::string getInterfaceName() const = 0;

  // 更新（非阻塞轮询）
  virtual void update() = 0;
};

// 导星设备类 - 本地设备与远程导星软件的接口
class GuiderDevice : public DeviceBase {
public:
  GuiderDevice(const std::string &deviceId,
               const std::string &manufacturer = "Generic",
               const std::string &model = "Guider");
  virtual ~GuiderDevice();

  // 重写DeviceBase方法
  virtual bool start() override;
  virtual void stop() override;

  // 连接到导星软件
  bool connectToGuider(GuiderInterfaceType type, const std::string &host,
                       int port);

  // 断开导星软件
  void disconnectFromGuider();

  // 获取接口类型
  GuiderInterfaceType getInterfaceType() const;

  // 接口类型转换为字符串
  static std::string interfaceTypeToString(GuiderInterfaceType type);
  static GuiderInterfaceType stringToInterfaceType(const std::string &typeStr);

  // 导星状态转换为字符串
  static std::string guiderStateToString(GuiderState state);
  static std::string calibrationStateToString(CalibrationState state);

  // 获取接口实例
  std::shared_ptr<GuiderInterface> getInterface() const;

protected:
  // 处理导星软件状态变更
  void statusUpdateLoop();
  void handleStateChanged(GuiderState newState);
  void handleCorrectionReceived(const GuidingCorrection &correction);
  void handleCalibrationChanged(CalibrationState newState,
                                const CalibrationData &data);
  void handleStatsUpdated(const GuiderStats &newStats);

  // 命令处理器
  void handleConnectCommand(const CommandMessage &cmd,
                            ResponseMessage &response);
  void handleDisconnectCommand(const CommandMessage &cmd,
                               ResponseMessage &response);
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
  void handleGetStatusCommand(const CommandMessage &cmd,
                              ResponseMessage &response);

  // 接口实例
  std::shared_ptr<GuiderInterface> guiderInterface;
  GuiderInterfaceType interfaceType;

  // 状态变量
  GuiderState lastState;
  CalibrationState lastCalState;

  // 状态更新线程
  std::thread statusThread;
  std::atomic<bool> running;

  // 线程安全
  mutable std::mutex interfaceMutex;

  // 状态变更事件间隔
  int statusUpdateInterval; // 毫秒
};

// 用于创建适当接口的工厂函数
std::shared_ptr<GuiderInterface>
createGuiderInterface(GuiderInterfaceType type);

} // namespace astrocomm
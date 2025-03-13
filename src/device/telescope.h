#pragma once
#include "device/device_base.h"
#include <atomic>
#include <cmath>
#include <thread>

namespace astrocomm {

// 望远镜设备类
class Telescope : public DeviceBase {
public:
  Telescope(const std::string &deviceId,
            const std::string &manufacturer = "Celestron",
            const std::string &model = "NexStar 8SE");
  virtual ~Telescope();

  // 重写启动和停止方法
  virtual bool start() override;
  virtual void stop() override;

  // 望远镜特定方法
  void gotoPosition(double ra, double dec);
  void setTracking(bool enabled);
  void setSlewRate(int rate);
  void abort();
  void park();
  void unpark();
  void sync(double ra, double dec);

protected:
  // 模拟望远镜的状态更新线程
  void updateLoop();

  // 更新高度角和方位角（基于赤经赤纬）
  void updateAltAz();

  // 更新赤经赤纬（基于高度角和方位角）
  void updateRaDec();

  // 发送GOTO完成事件
  void sendGotoCompletedEvent(const std::string &relatedMessageId);

  // 命令处理器
  void handleGotoCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleTrackingCommand(const CommandMessage &cmd,
                             ResponseMessage &response);
  void handleParkCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleSyncCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleAbortCommand(const CommandMessage &cmd, ResponseMessage &response);

private:
  // 望远镜状态
  double ra;       // 赤经 (小时, 0-24)
  double dec;      // 赤纬 (度, -90 到 +90)
  double altitude; // 高度角 (度, 0-90)
  double azimuth;  // 方位角 (度, 0-360)
  int slew_rate;   // 速度 (1-10)
  bool tracking;   // 跟踪状态
  bool is_parked;  // 停放状态

  // GOTO相关状态
  std::atomic<bool> is_moving;
  double target_ra;
  double target_dec;
  std::string current_goto_message_id;

  // 更新线程
  std::thread update_thread;
  std::atomic<bool> update_running;

  // 观测者位置（用于坐标转换）
  double observer_latitude;
  double observer_longitude;
};

} // namespace astrocomm
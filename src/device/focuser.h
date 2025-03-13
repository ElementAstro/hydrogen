#pragma once
#include "device/device_base.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace astrocomm {

// 调焦器设备类
class Focuser : public DeviceBase {
public:
  Focuser(const std::string &deviceId, const std::string &manufacturer = "ZWO",
          const std::string &model = "EAF");
  virtual ~Focuser();

  // 重写启动和停止方法
  virtual bool start() override;
  virtual void stop() override;

  // 调焦器特定方法
  void moveAbsolute(int position);
  void moveRelative(int steps);
  void abort();
  void setMaxPosition(int maxPos);
  void setSpeed(int speedValue);
  void setBacklash(int backlashValue);
  void setTemperatureCompensation(bool enabled, double coefficient = 0.0);

protected:
  // 模拟调焦器的状态更新线程
  void updateLoop();

  // 发送移动完成事件
  void sendMoveCompletedEvent(const std::string &relatedMessageId);

  // 应用温度补偿
  int applyTemperatureCompensation(int position);

  // 命令处理器
  void handleMoveAbsoluteCommand(const CommandMessage &cmd,
                                 ResponseMessage &response);
  void handleMoveRelativeCommand(const CommandMessage &cmd,
                                 ResponseMessage &response);
  void handleAbortCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleSetMaxPositionCommand(const CommandMessage &cmd,
                                   ResponseMessage &response);
  void handleSetSpeedCommand(const CommandMessage &cmd,
                             ResponseMessage &response);
  void handleSetBacklashCommand(const CommandMessage &cmd,
                                ResponseMessage &response);
  void handleSetTempCompCommand(const CommandMessage &cmd,
                                ResponseMessage &response);

private:
  // 调焦器状态
  int position;               // 当前位置
  int targetPosition;         // 目标位置
  int maxPosition;            // 最大位置
  int speed;                  // 移动速度 (1-10)
  int backlash;               // 反向间隙补偿
  bool tempCompEnabled;       // 温度补偿是否启用
  double tempCompCoefficient; // 温度补偿系数
  double temperature;         // 当前温度

  // 移动相关状态
  std::atomic<bool> isMoving;
  std::string currentMoveMessageId;
  bool movingDirection; // true = 向外, false = 向内

  // 温度模拟参数
  double ambientTemperature; // 环境温度
  double temperatureDrift;   // 温度变化趋势

  // 更新线程
  std::thread updateThread;
  std::atomic<bool> updateRunning;

  // 线程安全的状态更新
  mutable std::mutex statusMutex;
};

} // namespace astrocomm
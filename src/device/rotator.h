#pragma once
#include "device/device_base.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace astrocomm {

// 旋转器设备类
class Rotator : public DeviceBase {
public:
  Rotator(const std::string &deviceId,
          const std::string &manufacturer = "Generic",
          const std::string &model = "Field Rotator");
  virtual ~Rotator();

  // 重写启动和停止方法
  virtual bool start() override;
  virtual void stop() override;

  // 旋转器特定方法
  void moveTo(double position);
  void moveBy(double offset);
  void halt();
  void setReverse(bool reversed);
  void sync(double position);
  void setStepSize(double stepSize);

  // 获取当前状态
  double getPosition() const;
  double getTargetPosition() const;
  bool isMoving() const;
  bool isReversed() const;

protected:
  // 模拟旋转器的状态更新线程
  void updateLoop();

  // 命令处理器
  void handleMoveToCommand(const CommandMessage &cmd,
                           ResponseMessage &response);
  void handleMoveByCommand(const CommandMessage &cmd,
                           ResponseMessage &response);
  void handleHaltCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleReverseCommand(const CommandMessage &cmd,
                            ResponseMessage &response);
  void handleSyncCommand(const CommandMessage &cmd, ResponseMessage &response);

  // 发送移动完成事件
  void sendMoveCompletedEvent(const std::string &relatedMessageId);

private:
  // 旋转器状态
  double position;       // 当前位置（度, 0-359.99）
  double targetPosition; // 目标位置（度）
  double stepSize;       // 每步移动的大小（度）
  double maxSpeed;       // 最大旋转速度（度/秒）
  double currentSpeed;   // 当前旋转速度（度/秒）

  std::atomic<bool> isMovingFlag;   // 是否正在移动
  std::atomic<bool> isReversedFlag; // 是否反向

  // 更新线程
  std::thread update_thread;
  std::atomic<bool> update_running;

  // 当前命令消息ID（用于完成事件）
  std::string current_move_message_id;

  // 规范化角度到0-360度范围
  double normalizeAngle(double angle) const;

  // 计算最短路径旋转方向和距离
  std::pair<double, bool> calculateRotationPath(double current,
                                                double target) const;
};

} // namespace astrocomm
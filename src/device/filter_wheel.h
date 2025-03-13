#pragma once
#include "device/device_base.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace astrocomm {

// 滤镜轮设备类
class FilterWheel : public DeviceBase {
public:
  FilterWheel(const std::string &deviceId,
              const std::string &manufacturer = "QHY",
              const std::string &model = "CFW3");
  virtual ~FilterWheel();

  // 重写启动和停止方法
  virtual bool start() override;
  virtual void stop() override;

  // 滤镜轮特定方法
  void setPosition(int position);
  void setFilterNames(const std::vector<std::string> &names);
  void setFilterOffsets(const std::vector<int> &offsets);
  void abort();

protected:
  // 模拟滤镜轮的状态更新线程
  void updateLoop();

  // 发送位置变化完成事件
  void sendPositionChangeCompletedEvent(const std::string &relatedMessageId);

  // 获取当前滤镜名称
  std::string getCurrentFilterName() const;

  // 获取当前滤镜偏移量
  int getCurrentFilterOffset() const;

  // 命令处理器
  void handleSetPositionCommand(const CommandMessage &cmd,
                                ResponseMessage &response);
  void handleSetFilterNamesCommand(const CommandMessage &cmd,
                                   ResponseMessage &response);
  void handleSetFilterOffsetsCommand(const CommandMessage &cmd,
                                     ResponseMessage &response);
  void handleAbortCommand(const CommandMessage &cmd, ResponseMessage &response);

private:
  // 滤镜轮状态
  int position;                         // 当前位置
  int targetPosition;                   // 目标位置
  int filterCount;                      // 滤镜数量
  std::vector<std::string> filterNames; // 滤镜名称
  std::vector<int> filterOffsets;       // 聚焦偏移量

  // 移动相关状态
  std::atomic<bool> isMoving;
  std::string currentMoveMessageId;
  int moveDirection; // 1 = 顺时针, -1 = 逆时针

  // 更新线程
  std::thread updateThread;
  std::atomic<bool> updateRunning;

  // 线程安全的状态更新
  mutable std::mutex statusMutex;

  // 决定最短路径移动方向
  int determineShortestPath(int from, int to);
};

} // namespace astrocomm
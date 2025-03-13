#pragma once
#include "device/device_base.h"
#include <atomic>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace astrocomm {

// 解析状态枚举
enum class SolverState { IDLE, SOLVING, COMPLETE, FAILED };

// 天文解析器设备类
class Solver : public DeviceBase {
public:
  Solver(const std::string &deviceId,
         const std::string &manufacturer = "AstroCode",
         const std::string &model = "AstroSolver");
  virtual ~Solver();

  // 重写启动和停止方法
  virtual bool start() override;
  virtual void stop() override;

  // 解析器特定方法
  void solve(const std::vector<uint8_t> &imageData, int width, int height);
  void solveFromFile(const std::string &filePath);
  void abort();
  void setParameters(const json &params);
  void setSolverPath(const std::string &path);
  void setSolverOptions(const std::map<std::string, std::string> &options);
  json getLastSolution() const;

protected:
  // 解析线程
  void solveThread(std::vector<uint8_t> imageData, int width, int height);

  // 从文件解析线程
  void solveFileThread(std::string filePath);

  // 模拟解析过程
  bool performSolve(const std::vector<uint8_t> &imageData, int width,
                    int height);

  // 生成模拟解析结果
  json generateSolution(bool success);

  // 发送解析完成事件
  void sendSolveCompletedEvent(const std::string &relatedMessageId);

  // 命令处理器
  void handleSolveCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleSolveFileCommand(const CommandMessage &cmd,
                              ResponseMessage &response);
  void handleAbortCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleSetParametersCommand(const CommandMessage &cmd,
                                  ResponseMessage &response);

private:
  // 解析状态
  SolverState state;
  std::atomic<int> progress;

  // 解析参数
  double fovMin;      // 最小视场 (角分)
  double fovMax;      // 最大视场 (角分)
  double scaleMin;    // 最小像素尺度 (角秒/像素)
  double scaleMax;    // 最大像素尺度 (角秒/像素)
  bool useDistortion; // 是否考虑畸变
  int downsample;     // 下采样因子
  double raHint;      // RA提示 (小时)
  double decHint;     // DEC提示 (度)
  double radiusHint;  // 搜索半径提示 (度)

  // 外部解析器设置
  std::string solverPath;
  std::map<std::string, std::string> solverOptions;

  // 解析结果
  json lastSolution;
  bool hasValidSolution;

  // 当前解析任务ID
  std::string currentSolveMessageId;

  // 解析线程
  std::thread solveThreadObj;

  // 随机数生成器
  std::mt19937 rng;

  // 线程安全的状态更新
  mutable std::mutex statusMutex;
  std::mutex solveMutex;

  // 辅助函数
  std::string solverStateToString(SolverState state) const;
};

} // namespace astrocomm
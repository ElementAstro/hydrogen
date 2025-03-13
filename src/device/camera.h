#pragma once
#include "device/device_base.h"
#include <atomic>
#include <random>
#include <thread>
#include <vector>

namespace astrocomm {

// 相机设备类
class Camera : public DeviceBase {
public:
  Camera(const std::string &deviceId, const std::string &manufacturer = "ZWO",
         const std::string &model = "ASI294MC Pro");
  virtual ~Camera();

  // 重写启动和停止方法
  virtual bool start() override;
  virtual void stop() override;

  // 相机特定方法
  void startExposure(double duration, bool isLight = true);
  void abortExposure();
  void setGain(int value);
  void setCoolerTemperature(double targetTemp);
  void setCoolerEnabled(bool enabled);
  std::vector<uint8_t> getImageData() const;

protected:
  // 模拟相机曝光和冷却的状态更新线程
  void updateLoop();

  // 生成模拟图像数据
  void generateImageData();

  // 命令处理器
  void handleStartExposureCommand(const CommandMessage &cmd,
                                  ResponseMessage &response);
  void handleAbortExposureCommand(const CommandMessage &cmd,
                                  ResponseMessage &response);
  void handleSetCoolerCommand(const CommandMessage &cmd,
                              ResponseMessage &response);
  void handleGetImageCommand(const CommandMessage &cmd,
                             ResponseMessage &response);

private:
  // 相机状态
  enum class ExposureState { IDLE, EXPOSING, READING, COMPLETE };

  ExposureState exposureState;
  double exposureDuration;
  double exposureProgress;
  bool isLightFrame;
  int gain;
  int offset;
  double sensorTemperature;
  double targetTemperature;
  bool coolerEnabled;
  double coolerPower;

  // 图像属性
  int imageWidth;
  int imageHeight;
  int bitDepth;
  std::vector<uint8_t> imageData;

  // 更新线程
  std::thread update_thread;
  std::atomic<bool> update_running;

  // 用于生成模拟星场图像的随机数生成器
  std::mt19937 rng;

  // 发送曝光状态事件
  void sendExposureProgressEvent(double progress);
  void sendExposureCompleteEvent(const std::string &relatedMessageId);
};

} // namespace astrocomm
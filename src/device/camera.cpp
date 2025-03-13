#include "device/camera.h"
#include "common/logger.h"
#include <algorithm>
#include <cmath>

namespace astrocomm {

Camera::Camera(const std::string &deviceId, const std::string &manufacturer,
               const std::string &model)
    : DeviceBase(deviceId, "CAMERA", manufacturer, model),
      exposureState(ExposureState::IDLE), exposureDuration(0.0),
      exposureProgress(0.0), isLightFrame(true), gain(0), offset(10),
      sensorTemperature(20.0), targetTemperature(0.0), coolerEnabled(false),
      coolerPower(0.0), imageWidth(1920), imageHeight(1080), bitDepth(16),
      update_running(false), rng(std::random_device{}()) {

  // 初始化属性
  setProperty("connected", false);
  setProperty("exposure_time", 1.0);
  setProperty("gain", gain);
  setProperty("offset", offset);
  setProperty("sensor_temperature", sensorTemperature);
  setProperty("cooler_enabled", coolerEnabled);
  setProperty("cooler_power", coolerPower);
  setProperty("target_temperature", targetTemperature);
  setProperty("exposure_state", "IDLE");
  setProperty("exposure_progress", 0.0);
  setProperty("image_width", imageWidth);
  setProperty("image_height", imageHeight);
  setProperty("bit_depth", bitDepth);
  setProperty("image_ready", false);

  // 设置相机功能
  capabilities = {"EXPOSURE", "COOLING", "GAIN_CONTROL", "OFFSET_CONTROL",
                  "RGB_COLOR"};

  // 注册命令处理器
  registerCommandHandler("START_EXPOSURE", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleStartExposureCommand(cmd, response);
  });

  registerCommandHandler("ABORT_EXPOSURE", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleAbortExposureCommand(cmd, response);
  });

  registerCommandHandler("SET_COOLER", [this](const CommandMessage &cmd,
                                              ResponseMessage &response) {
    handleSetCoolerCommand(cmd, response);
  });

  registerCommandHandler("GET_IMAGE", [this](const CommandMessage &cmd,
                                             ResponseMessage &response) {
    handleGetImageCommand(cmd, response);
  });

  logInfo("Camera device initialized", deviceId);
}

Camera::~Camera() { stop(); }

bool Camera::start() {
  if (!DeviceBase::start()) {
    return false;
  }

  // 开始更新线程
  update_running = true;
  update_thread = std::thread(&Camera::updateLoop, this);

  setProperty("connected", true);
  logInfo("Camera started", deviceId);
  return true;
}

void Camera::stop() {
  // 停止更新线程
  update_running = false;
  if (update_thread.joinable()) {
    update_thread.join();
  }

  // 关闭制冷器
  coolerEnabled = false;
  setProperty("cooler_enabled", false);
  setProperty("cooler_power", 0.0);

  setProperty("connected", false);
  DeviceBase::stop();
  logInfo("Camera stopped", deviceId);
}

void Camera::startExposure(double duration, bool isLight) {
  // 检查当前状态
  if (exposureState != ExposureState::IDLE) {
    logWarning("Cannot start exposure: camera busy", deviceId);
    return;
  }

  exposureDuration = std::max(0.001, duration); // 最小曝光时间为1毫秒
  exposureProgress = 0.0;
  isLightFrame = isLight;
  exposureState = ExposureState::EXPOSING;

  setProperty("exposure_time", exposureDuration);
  setProperty("exposure_state", "EXPOSING");
  setProperty("exposure_progress", exposureProgress);
  setProperty("image_ready", false);

  logInfo("Starting " + std::string(isLightFrame ? "light" : "dark") +
              " exposure, duration: " + std::to_string(exposureDuration) +
              " seconds",
          deviceId);

  // 发送事件
  EventMessage event("EXPOSURE_STARTED");
  event.setDetails({{"duration", exposureDuration}, {"isLight", isLightFrame}});
  sendEvent(event);
}

void Camera::abortExposure() {
  // 只有在曝光中才能中止
  if (exposureState != ExposureState::EXPOSING &&
      exposureState != ExposureState::READING) {
    logWarning("No exposure to abort", deviceId);
    return;
  }

  exposureState = ExposureState::IDLE;
  exposureProgress = 0.0;

  setProperty("exposure_state", "IDLE");
  setProperty("exposure_progress", exposureProgress);

  logInfo("Exposure aborted", deviceId);

  // 发送事件
  EventMessage event("EXPOSURE_ABORTED");
  sendEvent(event);
}

void Camera::setGain(int value) {
  // 假设有效范围是0-100
  gain = std::max(0, std::min(100, value));
  setProperty("gain", gain);

  logInfo("Gain set to " + std::to_string(gain), deviceId);
}

void Camera::setCoolerTemperature(double targetTemp) {
  // 假设有效范围是-50到50摄氏度
  targetTemperature = std::max(-50.0, std::min(50.0, targetTemp));
  setProperty("target_temperature", targetTemperature);

  logInfo("Target temperature set to " + std::to_string(targetTemperature) +
              "°C",
          deviceId);
}

void Camera::setCoolerEnabled(bool enabled) {
  coolerEnabled = enabled;
  setProperty("cooler_enabled", coolerEnabled);

  if (!coolerEnabled) {
    setProperty("cooler_power", 0.0);
    coolerPower = 0.0;
  }

  logInfo("Cooler " + std::string(coolerEnabled ? "enabled" : "disabled"),
          deviceId);

  // 发送事件
  EventMessage event("COOLER_" +
                     std::string(coolerEnabled ? "ENABLED" : "DISABLED"));
  sendEvent(event);
}

std::vector<uint8_t> Camera::getImageData() const {
  // 返回当前图像数据的拷贝
  std::lock_guard<std::mutex> lock(propertiesMutex);
  return imageData;
}

void Camera::updateLoop() {
  logInfo("Update loop started", deviceId);

  // 用于计算时间间隔的变量
  auto lastUpdateTime = std::chrono::steady_clock::now();

  while (update_running) {
    // 计算时间间隔
    auto now = std::chrono::steady_clock::now();
    double elapsedSeconds =
        std::chrono::duration<double>(now - lastUpdateTime).count();
    lastUpdateTime = now;

    // 更新传感器温度（模拟）
    if (coolerEnabled) {
      // 制冷器打开时，温度向目标温度靠近
      double tempDiff = sensorTemperature - targetTemperature;

      // 计算所需功率 (0-100%)
      coolerPower = std::min(100.0, std::max(0.0, std::abs(tempDiff) * 10.0));
      setProperty("cooler_power", coolerPower);

      // 温度变化速率取决于温差和功率
      double coolingRate = 0.5 * coolerPower / 100.0 * elapsedSeconds;
      if (tempDiff > 0) {
        sensorTemperature -= std::min(tempDiff, coolingRate);
      } else {
        // 如果当前温度低于目标温度，将缓慢回升
        sensorTemperature += std::min(-tempDiff, 0.1 * elapsedSeconds);
      }
    } else {
      // 制冷器关闭时，温度缓慢回升到环境温度（假设为20摄氏度）
      double ambientTemp = 20.0;
      double tempDiff = ambientTemp - sensorTemperature;
      sensorTemperature += tempDiff * 0.1 * elapsedSeconds;
      coolerPower = 0.0;
    }

    setProperty("sensor_temperature", sensorTemperature);

    // 处理曝光状态更新
    if (exposureState == ExposureState::EXPOSING) {
      // 更新曝光进度
      double progressIncrement = elapsedSeconds / exposureDuration;
      exposureProgress += progressIncrement;

      // 发送进度更新事件
      if (progressIncrement > 0) {
        sendExposureProgressEvent(exposureProgress);
      }

      if (exposureProgress >= 1.0) {
        // 曝光完成，转入读出状态
        exposureProgress = 1.0;
        exposureState = ExposureState::READING;
        setProperty("exposure_state", "READING");

        logInfo("Exposure complete, reading out image", deviceId);

        // 模拟读出延迟 (在下一个更新循环中完成)
      }
    } else if (exposureState == ExposureState::READING) {
      // 模拟图像读出过程
      generateImageData();

      // 更新状态
      exposureState = ExposureState::COMPLETE;
      setProperty("exposure_state", "COMPLETE");
      setProperty("image_ready", true);

      logInfo("Image readout complete", deviceId);

      // 发送曝光完成事件
      EventMessage event("EXPOSURE_COMPLETE");
      event.setDetails({{"duration", exposureDuration},
                        {"temperature", sensorTemperature},
                        {"imageWidth", imageWidth},
                        {"imageHeight", imageHeight},
                        {"bitDepth", bitDepth}});
      sendEvent(event);
    }

    // 固定更新频率
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  logInfo("Update loop ended", deviceId);
}

void Camera::generateImageData() {
  // 创建模拟的图像数据
  // 对于真实应用，这将是从相机硬件读取的数据
  // 这里我们只是生成一个模拟的星场

  // 假设16位图像深度，计算图像大小 (2字节/像素 * 3通道)
  size_t imageSize = imageWidth * imageHeight * 3 * 2;
  imageData.resize(imageSize);

  // 如果是暗场，生成低电平随机噪声
  if (!isLightFrame) {
    std::uniform_int_distribution<uint16_t> dist(0, 1000); // 低电平噪声

    // 每个像素3通道，每通道2字节
    for (size_t i = 0; i < imageSize; i += 2) {
      uint16_t value = dist(rng);
      imageData[i] = value & 0xFF;            // 低字节
      imageData[i + 1] = (value >> 8) & 0xFF; // 高字节
    }

    return;
  }

  // 如果是亮场，生成模拟的星场
  // 1. 填充背景天空
  std::uniform_int_distribution<uint16_t> skyDist(1000, 3000); // 背景亮度

  // 每个像素3通道，每通道2字节
  for (size_t i = 0; i < imageSize; i += 6) {
    // 为RGB三个通道设置略有不同的背景色
    uint16_t r = skyDist(rng);
    uint16_t g = skyDist(rng);
    uint16_t b = skyDist(rng);

    // R通道
    imageData[i] = r & 0xFF;
    imageData[i + 1] = (r >> 8) & 0xFF;

    // G通道
    imageData[i + 2] = g & 0xFF;
    imageData[i + 3] = (g >> 8) & 0xFF;

    // B通道
    imageData[i + 4] = b & 0xFF;
    imageData[i + 5] = (b >> 8) & 0xFF;
  }

  // 2. 添加一些随机的星星
  int numStars = imageWidth * imageHeight / 1000; // 星星数量
  std::uniform_int_distribution<int> posDist(0, imageWidth * imageHeight - 1);
  std::uniform_int_distribution<int> brightDist(20000, 65000); // 星星亮度
  std::uniform_int_distribution<int> sizeDist(1, 5);           // 星星大小

  for (int i = 0; i < numStars; ++i) {
    int pos = posDist(rng);
    int x = pos % imageWidth;
    int y = pos / imageWidth;

    uint16_t brightness = brightDist(rng);
    int size = sizeDist(rng);

    // 在星星周围创建高斯分布
    for (int dy = -size; dy <= size; ++dy) {
      for (int dx = -size; dx <= size; ++dx) {
        int nx = x + dx;
        int ny = y + dy;

        if (nx >= 0 && nx < imageWidth && ny >= 0 && ny < imageHeight) {
          double distance = std::sqrt(dx * dx + dy * dy);
          double factor = std::exp(-distance * distance / (size * 0.5));

          int pixelPos = (ny * imageWidth + nx) * 3 * 2;

          // 为星星增加亮度，带有颜色差异
          for (int c = 0; c < 3; ++c) {
            uint16_t currentValue = imageData[pixelPos + c * 2] |
                                    (imageData[pixelPos + c * 2 + 1] << 8);

            // 根据通道调整星星颜色（蓝色恒星、红色恒星等）
            double colorFactor = 1.0;
            if (c == 0)
              colorFactor = 1.0 + (rng() % 100) / 100.0; // 红色
            else if (c == 1)
              colorFactor = 0.8 + (rng() % 40) / 100.0; // 绿色
            else
              colorFactor = 0.6 + (rng() % 80) / 100.0; // 蓝色

            uint16_t starBrightness = brightness * factor * colorFactor;
            uint16_t newValue = std::min(
                65535, static_cast<int>(currentValue + starBrightness));

            imageData[pixelPos + c * 2] = newValue & 0xFF;
            imageData[pixelPos + c * 2 + 1] = (newValue >> 8) & 0xFF;
          }
        }
      }
    }
  }

  // 3. 应用增益系数
  double gainFactor = 1.0 + gain / 50.0; // 增益因子

  for (size_t i = 0; i < imageSize; i += 2) {
    uint16_t value = imageData[i] | (imageData[i + 1] << 8);
    value = std::min(65535, static_cast<int>(value * gainFactor));

    imageData[i] = value & 0xFF;
    imageData[i + 1] = (value >> 8) & 0xFF;
  }

  // 4. 加入读出噪声
  std::normal_distribution<double> noiseDist(0, 50 + offset * 2);

  for (size_t i = 0; i < imageSize; i += 2) {
    uint16_t value = imageData[i] | (imageData[i + 1] << 8);

    int noise = static_cast<int>(noiseDist(rng));
    int newValue = static_cast<int>(value) + noise;
    newValue = std::max(0, std::min(65535, newValue));

    imageData[i] = newValue & 0xFF;
    imageData[i + 1] = (newValue >> 8) & 0xFF;
  }
}

void Camera::sendExposureProgressEvent(double progress) {
  // 限制进度在0-1之间
  progress = std::max(0.0, std::min(1.0, progress));

  // 更新属性
  setProperty("exposure_progress", progress);

  // 发送进度事件
  EventMessage event("EXPOSURE_PROGRESS");
  event.setDetails({{"progress", progress},
                    {"remainingSeconds", exposureDuration * (1.0 - progress)}});
  sendEvent(event);
}

void Camera::sendExposureCompleteEvent(const std::string &relatedMessageId) {
  EventMessage event("COMMAND_COMPLETED");
  event.setRelatedMessageId(relatedMessageId);
  event.setDetails({{"command", "START_EXPOSURE"},
                    {"status", "SUCCESS"},
                    {"imageReady", true}});
  sendEvent(event);
}

// 命令处理器
void Camera::handleStartExposureCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("duration")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'duration'"}});
    return;
  }

  double duration = params["duration"];
  bool isLight = true;

  if (params.contains("isLight")) {
    isLight = params["isLight"];
  }

  // 检查当前状态
  if (exposureState != ExposureState::IDLE) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "CAMERA_BUSY"},
         {"message", "Camera is busy, current state: " +
                         getProperty("exposure_state").get<std::string>()}});
    return;
  }

  // 开始曝光
  startExposure(duration, isLight);

  // 计算预计完成时间
  auto now = std::chrono::system_clock::now();
  auto completeTime =
      now + std::chrono::milliseconds(static_cast<int>(duration * 1000));
  auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
  std::ostringstream est_ss;
  est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

  response.setStatus("IN_PROGRESS");
  response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                       {"exposureId", generateUuid()}});
}

void Camera::handleAbortExposureCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  // 检查当前状态
  if (exposureState != ExposureState::EXPOSING &&
      exposureState != ExposureState::READING) {
    response.setStatus("SUCCESS");
    response.setDetails({{"message", "No exposure to abort"}});
    return;
  }

  // 中止曝光
  abortExposure();

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Exposure aborted"}});
}

void Camera::handleSetCoolerCommand(const CommandMessage &cmd,
                                    ResponseMessage &response) {
  json params = cmd.getParameters();

  if (params.contains("enabled")) {
    bool enabled = params["enabled"];
    setCoolerEnabled(enabled);
  }

  if (params.contains("temperature") && coolerEnabled) {
    double temperature = params["temperature"];
    setCoolerTemperature(temperature);
  }

  response.setStatus("SUCCESS");
  response.setDetails({{"coolerEnabled", coolerEnabled},
                       {"targetTemperature", targetTemperature},
                       {"currentTemperature", sensorTemperature}});
}

void Camera::handleGetImageCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  // 检查图像是否准备好
  bool imageReady = getProperty("image_ready");

  if (!imageReady) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "IMAGE_NOT_READY"}, {"message", "No image is available"}});
    return;
  }

  // 在实际应用中，这里会返回图像数据或保存路径
  // 由于WebSocket不适合传输大型二进制数据，通常会提供下载链接或保存到指定位置

  response.setStatus("SUCCESS");
  response.setDetails({{"imageWidth", imageWidth},
                       {"imageHeight", imageHeight},
                       {"bitDepth", bitDepth},
                       {"size", imageData.size()},
                       {"downloadUrl", "/api/images/" + deviceId + "/latest"}});
}

} // namespace astrocomm

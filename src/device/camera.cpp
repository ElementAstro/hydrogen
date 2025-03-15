#include "device/camera.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace astrocomm {

Camera::Camera(const std::string &deviceId, const std::string &manufacturer,
               const std::string &model, const CameraParameters &params)
    : DeviceBase(deviceId, "CAMERA", manufacturer, model),
      cameraState(CameraState::IDLE), cameraParams(params),
      exposureDuration(0.0), exposureProgress(0.0),
      currentImageType(ImageType::LIGHT), autoSave(false), gain(0), offset(0),
      sensorTemperature(20.0), targetTemperature(0.0), coolerEnabled(false),
      coolerPower(0), currentFilterPosition(0), exposureDelay(0.0),
      updateRunning(false), baseImplementation(true) {

  // 设置随机数生成器种子
  std::random_device rd;
  rng.seed(rd());

  // 初始化ROI为全幅面
  roi.x = 0;
  roi.y = 0;
  roi.width = params.width;
  roi.height = params.height;
  roi.binX = 1;
  roi.binY = 1;

  // 初始化默认滤镜名称
  if (params.hasFilterWheel && params.numFilters > 0) {
    for (int i = 0; i < params.numFilters; ++i) {
      filterNames[i] = "Filter " + std::to_string(i + 1);
    }
  }

  // 初始化属性
  setProperty("sensorWidth", params.width);
  setProperty("sensorHeight", params.height);
  setProperty("pixelSizeX", params.pixelSizeX);
  setProperty("pixelSizeY", params.pixelSizeY);
  setProperty("bitDepth", params.bitDepth);
  setProperty("coolerEnabled", coolerEnabled.load());
  setProperty("targetTemperature", targetTemperature.load());
  setProperty("temperature", sensorTemperature.load());
  setProperty("coolerPower", coolerPower.load());
  setProperty("gain", gain.load());
  setProperty("offset", offset.load());
  setProperty("state", cameraStateToString(cameraState));
  setProperty("exposureProgress", 0.0);
  setProperty("exposureDuration", 0.0);
  setProperty("binningX", roi.binX);
  setProperty("binningY", roi.binY);
  setProperty("hasColorSensor", params.hasColorSensor);
  setProperty("hasCooler", params.hasCooler);
  setProperty("hasFilterWheel", params.hasFilterWheel);
  setProperty("connected", false);
  setProperty("imageReady", false);

  if (params.hasFilterWheel) {
    setProperty("filterPosition", currentFilterPosition.load());

    json filtersJson = json::array();
    for (int i = 0; i < params.numFilters; ++i) {
      filtersJson.push_back(filterNames[i]);
    }
    setProperty("filterNames", filtersJson);
  }

  // 设置能力
  capabilities = {"EXPOSURE", "GAIN_CONTROL", "ROI", "BINNING"};

  if (params.hasCooler) {
    capabilities.push_back("COOLING");
  }

  if (params.hasFilterWheel) {
    capabilities.push_back("FILTER_WHEEL");
  }

  if (params.hasColorSensor) {
    capabilities.push_back("COLOR");
  } else {
    capabilities.push_back("MONOCHROME");
  }

  // 注册命令处理器
  registerCommandHandler("START_EXPOSURE", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleStartExposureCommand(cmd, response);
  });

  registerCommandHandler("ABORT_EXPOSURE", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleAbortExposureCommand(cmd, response);
  });

  registerCommandHandler("GET_IMAGE", [this](const CommandMessage &cmd,
                                             ResponseMessage &response) {
    handleGetImageCommand(cmd, response);
  });

  registerCommandHandler("SET_COOLER", [this](const CommandMessage &cmd,
                                              ResponseMessage &response) {
    handleSetCoolerCommand(cmd, response);
  });

  registerCommandHandler(
      "SET_ROI", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSetROICommand(cmd, response);
      });

  registerCommandHandler("SET_GAIN_OFFSET", [this](const CommandMessage &cmd,
                                                   ResponseMessage &response) {
    handleSetGainOffsetCommand(cmd, response);
  });

  registerCommandHandler("SET_FILTER", [this](const CommandMessage &cmd,
                                              ResponseMessage &response) {
    handleSetFilterCommand(cmd, response);
  });

  SPDLOG_INFO("Camera device initialized: {}", deviceId);
}

Camera::~Camera() { stop(); }

bool Camera::start() {
  if (!DeviceBase::start()) {
    return false;
  }

  // 启动更新线程
  updateRunning = true;
  updateThread = std::thread(&Camera::updateLoop, this);

  setProperty("connected", true);
  SPDLOG_INFO("Camera started: {}", deviceId);
  return true;
}

void Camera::stop() {
  // 停止更新线程
  updateRunning = false;

  // 通知任何等待的条件变量
  exposureCV.notify_all();

  if (updateThread.joinable()) {
    updateThread.join();
  }

  setProperty("connected", false);
  DeviceBase::stop();
  SPDLOG_INFO("Camera stopped: {}", deviceId);
}

bool Camera::startExposure(double duration, ImageType type, bool saveImage) {
  std::lock_guard lock(stateMutex);

  if (cameraState != CameraState::IDLE) {
    SPDLOG_WARN("Cannot start exposure: camera is busy: {}", deviceId);
    return false;
  }

  if (duration < cameraParams.minExposureTime ||
      duration > cameraParams.maxExposureTime) {
    SPDLOG_WARN("Invalid exposure time: {}", duration);
    return false;
  }

  exposureDuration = duration;
  currentImageType = type;
  autoSave = saveImage;
  exposureProgress = 0.0;

  // 设置相机状态
  cameraState = CameraState::EXPOSING;
  setProperty("state", cameraStateToString(cameraState));
  setProperty("exposureProgress", 0.0);
  setProperty("exposureDuration", duration);
  setProperty("imageType", imageTypeToString(type));
  setProperty("imageReady", false);

  SPDLOG_INFO("Starting exposure: {}s, type: {}", duration,
              imageTypeToString(type));

  // 触发条件变量来启动曝光
  exposureCV.notify_one();
  return true;
}

bool Camera::abortExposure() {
  std::lock_guard lock(stateMutex);

  if (cameraState != CameraState::EXPOSING &&
      cameraState != CameraState::WAITING_TRIGGER) {
    SPDLOG_WARN("No exposure to abort: {}", deviceId);
    return false;
  }

  // 重置状态
  cameraState = CameraState::IDLE;
  setProperty("state", cameraStateToString(cameraState));
  setProperty("exposureProgress", 0.0);
  currentExposureMessageId.clear();

  SPDLOG_INFO("Exposure aborted: {}", deviceId);

  // 发送中止事件
  EventMessage event("EXPOSURE_ABORTED");
  sendEvent(event);

  // 调用回调函数(如果设置了)
  if (exposureCallback) {
    std::lock_guard cbLock(callbackMutex);
    exposureCallback(false, "Exposure aborted");
  }

  return true;
}

std::vector<uint8_t> Camera::getImageData() const {
  std::lock_guard lock(imageDataMutex);
  return imageData;
}

bool Camera::saveImage(const std::string &filename, const std::string &format) {
  std::lock_guard lock(imageDataMutex);

  if (imageData.empty()) {
    SPDLOG_WARN("No image data to save: {}", deviceId);
    return false;
  }

  try {
    // 生成文件名(如果未提供)
    std::string actualFilename = filename;
    if (actualFilename.empty()) {
      // 使用日期时间创建文件名
      auto now = std::chrono::system_clock::now();
      auto now_time_t = std::chrono::system_clock::to_time_t(now);
      std::stringstream ss;
      ss << "image_"
         << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S");

      // 添加图像类型
      ss << "_" << imageTypeToString(currentImageType);

      // 添加扩展名
      if (format == "FITS") {
        ss << ".fits";
      } else if (format == "JPEG" || format == "JPG") {
        ss << ".jpg";
      } else if (format == "PNG") {
        ss << ".png";
      } else if (format == "RAW") {
        ss << ".raw";
      } else {
        ss << ".dat"; // 默认二进制格式
      }

      actualFilename = ss.str();
    }

    // 打开文件
    std::ofstream outFile(actualFilename, std::ios::binary);
    if (!outFile) {
      SPDLOG_ERROR("Failed to open file for writing: {}: {}", actualFilename,
                   deviceId);
      return false;
    }

    // 这里可以根据格式添加特定的文件头处理
    // 但对于简单的演示，我们只是写入原始数据
    outFile.write(reinterpret_cast<const char *>(imageData.data()),
                  imageData.size());

    SPDLOG_INFO("Image saved to {}: {}", actualFilename, deviceId);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error saving image: {}: {}", e.what(), deviceId);
    return false;
  }
}

bool Camera::setGain(int gainValue) {
  if (gainValue < 0 || gainValue > cameraParams.maxGain) {
    SPDLOG_WARN("Invalid gain value: {}", gainValue);
    return false;
  }

  gain = gainValue;
  setProperty("gain", gain.load());
  SPDLOG_INFO("Gain set to {}", gain.load());
  return true;
}

bool Camera::setOffset(int offsetValue) {
  if (offsetValue < 0 || offsetValue > cameraParams.maxOffset) {
    SPDLOG_WARN("Invalid offset value: {}", offsetValue);
    return false;
  }

  offset = offsetValue;
  setProperty("offset", offset.load());
  SPDLOG_INFO("Offset set to {}", offset.load());
  return true;
}

bool Camera::setROI(int x, int y, int width, int height) {
  // 验证参数
  if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > cameraParams.width || y + height > cameraParams.height) {
    SPDLOG_WARN("Invalid ROI parameters: {}", deviceId);
    return false;
  }

  // 更新ROI
  std::lock_guard lock(stateMutex);
  roi.x = x;
  roi.y = y;
  roi.width = width;
  roi.height = height;

  // 更新属性
  json roiJson = {{"x", x}, {"y", y}, {"width", width}, {"height", height}};
  setProperty("roi", roiJson);

  SPDLOG_INFO("ROI set to x={}, y={}, width={}, height={}", x, y, width,
              height);
  return true;
}

bool Camera::setBinning(int binX, int binY) {
  // 验证参数
  if (binX < 1 || binX > cameraParams.maxBinningX || binY < 1 ||
      binY > cameraParams.maxBinningY) {
    SPDLOG_WARN("Invalid binning parameters: {}", deviceId);
    return false;
  }

  // 更新合并设置
  std::lock_guard lock(stateMutex);
  roi.binX = binX;
  roi.binY = binY;

  // 更新属性
  setProperty("binningX", binX);
  setProperty("binningY", binY);

  SPDLOG_INFO("Binning set to {}x{}", binX, binY);
  return true;
}

bool Camera::setCoolerTemperature(double temperature) {
  if (!cameraParams.hasCooler) {
    SPDLOG_WARN("Camera does not have a cooler: {}", deviceId);
    return false;
  }

  if (temperature < cameraParams.minCoolerTemp) {
    SPDLOG_WARN("Temperature too low: {}", temperature);
    return false;
  }

  targetTemperature = temperature;
  setProperty("targetTemperature", targetTemperature.load());
  SPDLOG_INFO("Target temperature set to {}", targetTemperature.load());
  return true;
}

bool Camera::setCoolerEnabled(bool enabled) {
  if (!cameraParams.hasCooler) {
    SPDLOG_WARN("Camera does not have a cooler: {}", deviceId);
    return false;
  }

  coolerEnabled = enabled;
  setProperty("coolerEnabled", coolerEnabled.load());

  if (!enabled) {
    // 如果禁用冷却器，冷却功率设为0
    coolerPower = 0;
    setProperty("coolerPower", coolerPower.load());
  }

  SPDLOG_INFO("Cooler {}", enabled ? "enabled" : "disabled");
  return true;
}

double Camera::getCurrentTemperature() const { return sensorTemperature; }

int Camera::getCoolerPower() const { return coolerPower; }

bool Camera::setFilterPosition(int position) {
  if (!cameraParams.hasFilterWheel) {
    SPDLOG_WARN("Camera does not have a filter wheel: {}", deviceId);
    return false;
  }

  if (position < 0 || position >= cameraParams.numFilters) {
    SPDLOG_WARN("Invalid filter position: {}", position);
    return false;
  }

  // 在实际相机中，这可能是一个需要时间的操作
  // 在这个模拟中，我们立即更改位置
  currentFilterPosition = position;
  setProperty("filterPosition", position);

  // 获取并设置当前滤镜名称
  std::string filterName;
  {
    std::lock_guard lock(filterMutex);
    filterName = filterNames[position];
  }
  setProperty("currentFilter", filterName);

  SPDLOG_INFO("Filter set to position {} ({})", position, filterName);
  return true;
}

int Camera::getFilterPosition() const { return currentFilterPosition; }

bool Camera::setFilterName(int position, const std::string &name) {
  if (!cameraParams.hasFilterWheel) {
    SPDLOG_WARN("Camera does not have a filter wheel: {}", deviceId);
    return false;
  }

  if (position < 0 || position >= cameraParams.numFilters) {
    SPDLOG_WARN("Invalid filter position: {}", position);
    return false;
  }

  // 更新滤镜名称
  {
    std::lock_guard lock(filterMutex);
    filterNames[position] = name;
  }

  // 更新所有滤镜名称的属性
  json filtersJson = json::array();
  {
    std::lock_guard lock(filterMutex);
    for (int i = 0; i < cameraParams.numFilters; ++i) {
      filtersJson.push_back(filterNames[i]);
    }
  }
  setProperty("filterNames", filtersJson);

  // 如果当前位置就是被更新的位置，也更新当前滤镜名称
  if (currentFilterPosition == position) {
    setProperty("currentFilter", name);
  }

  SPDLOG_INFO("Filter {} name set to \"{}\"", position, name);
  return true;
}

std::string Camera::getFilterName(int position) const {
  if (!cameraParams.hasFilterWheel || position < 0 ||
      position >= cameraParams.numFilters) {
    return "";
  }

  std::lock_guard lock(filterMutex);
  auto it = filterNames.find(position);
  if (it != filterNames.end()) {
    return it->second;
  }
  return "";
}

CameraState Camera::getState() const { return cameraState; }

double Camera::getExposureProgress() const { return exposureProgress; }

const CameraParameters &Camera::getCameraParameters() const {
  return cameraParams;
}

bool Camera::isExposing() const {
  return (cameraState == CameraState::EXPOSING ||
          cameraState == CameraState::READING_OUT ||
          cameraState == CameraState::DOWNLOADING);
}

bool Camera::setAutoExposure(int targetBrightness, int tolerance) {
  if (targetBrightness < 0 || targetBrightness > 255 || tolerance < 0) {
    SPDLOG_WARN("Invalid auto exposure parameters: {}", deviceId);
    return false;
  }

  autoExposure.enabled = true;
  autoExposure.targetBrightness = targetBrightness;
  autoExposure.tolerance = tolerance;

  setProperty("autoExposure", autoExposure.enabled);
  setProperty("autoExposureTarget", autoExposure.targetBrightness);
  setProperty("autoExposureTolerance", autoExposure.tolerance);

  SPDLOG_INFO("Auto exposure enabled with target brightness {} ± {}",
              targetBrightness, tolerance);
  return true;
}

bool Camera::setExposureDelay(double delaySeconds) {
  if (delaySeconds < 0) {
    SPDLOG_WARN("Invalid exposure delay: {}", delaySeconds);
    return false;
  }

  exposureDelay = delaySeconds;
  setProperty("exposureDelay", delaySeconds);
  SPDLOG_INFO("Exposure delay set to {} seconds", delaySeconds);
  return true;
}

void Camera::setExposureCallback(
    std::function<void(bool, const std::string &)> callback) {
  std::lock_guard lock(callbackMutex);
  exposureCallback = callback;
}

bool Camera::isBaseImplementation() const { return baseImplementation; }

void Camera::updateLoop() {
  SPDLOG_INFO("Update loop started: {}", deviceId);

  // 随机数分布用于温度和噪声模拟
  std::uniform_real_distribution<> tempDist(-0.1, 0.1);

  while (updateRunning) {
    // 更新温度模拟
    if (cameraParams.hasCooler) {
      if (coolerEnabled) {
        // 计算需要的冷却功率
        double tempDiff = sensorTemperature - targetTemperature;
        if (std::abs(tempDiff) > 0.1) {
          // 根据温差调整冷却功率
          if (tempDiff > 0) {
            // 需要冷却
            coolerPower = std::min(100, static_cast<int>(tempDiff * 10));
            // 模拟冷却效果
            sensorTemperature.store(0.1 * coolerPower / 100.0);
          } else {
            // 需要增温(停止冷却)
            coolerPower = 0;
            // 自然升温
            sensorTemperature.store(0.05);
          }
        } else {
          // 温度已达到目标，调整为维持功率
          coolerPower = 30; // 假设30%功率足以维持温度
          sensorTemperature.store(tempDist(rng) * 0.05); // 微小波动
        }
      } else {
        // 冷却器关闭，温度自然上升至室温
        if (sensorTemperature < 20.0) {
          sensorTemperature.store(0.1);
        } else {
          sensorTemperature = 20.0 + (tempDist(rng) * 0.2); // 室温波动
        }
        coolerPower = 0;
      }

      // 更新温度和冷却功率属性
      setProperty("temperature", sensorTemperature.load());
      setProperty("coolerPower", coolerPower.load());
    }

    // 曝光状态机处理
    std::unique_lock lock(stateMutex);

    switch (cameraState) {
    case CameraState::IDLE:
      // 等待新的曝光命令
      exposureCV.wait_for(lock, std::chrono::milliseconds(100), [this] {
        return cameraState != CameraState::IDLE || !updateRunning;
      });
      break;

    case CameraState::EXPOSING: {
      // 处理曝光延迟
      if (exposureDelay > 0) {
        cameraState = CameraState::WAITING_TRIGGER;
        setProperty("state", cameraStateToString(cameraState));

        // 等待延迟时间
        exposureCV.wait_for(
            lock,
            std::chrono::milliseconds(static_cast<int>(exposureDelay * 1000)),
            [this] {
              return cameraState != CameraState::WAITING_TRIGGER ||
                     !updateRunning;
            });

        if (cameraState == CameraState::WAITING_TRIGGER && updateRunning) {
          // 延迟结束，重新开始曝光
          cameraState = CameraState::EXPOSING;
          setProperty("state", cameraStateToString(cameraState));
          exposureProgress = 0.0;
          break;
        } else {
          // 可能已被中止
          break;
        }
      }

      // 更新曝光进度
      double increment = 0.05 / exposureDuration; // 每100ms的进度增量
      double newProgress = exposureProgress + increment;

      if (newProgress >= 1.0) {
        // 曝光完成
        exposureProgress = 1.0;
        cameraState = CameraState::READING_OUT;
        setProperty("state", cameraStateToString(cameraState));
        setProperty("exposureProgress", 1.0);

        SPDLOG_INFO("Exposure completed, reading out...", deviceId);
      } else {
        // 曝光进行中
        exposureProgress = newProgress;
        setProperty("exposureProgress", exposureProgress.load());

        // 定期发送进度事件
        if (static_cast<int>(exposureProgress * 100) % 10 == 0) {
          sendExposureProgressEvent(exposureProgress);
        }
      }
      break;
    }

    case CameraState::READING_OUT: {
      // 模拟读出时间(约1秒)
      exposureCV.wait_for(lock, std::chrono::milliseconds(1000), [this] {
        return cameraState != CameraState::READING_OUT || !updateRunning;
      });

      // 如果状态仍然是READING_OUT，表示读出完成
      if (cameraState == CameraState::READING_OUT && updateRunning) {
        // 生成图像数据
        cameraState = CameraState::PROCESSING;
        setProperty("state", cameraStateToString(cameraState));

        SPDLOG_INFO("Reading out completed, processing...", deviceId);

        // 处理图像(在锁外完成，避免阻塞)
        lock.unlock();
        generateImageData();
        lock.lock();

        // 处理完成
        cameraState = CameraState::IDLE;
        setProperty("state", cameraStateToString(cameraState));
        setProperty("imageReady", true);

        SPDLOG_INFO("Image ready", deviceId);

        // 如果配置了自动保存
        if (autoSave) {
          lock.unlock();
          saveImage();
          lock.lock();
        }

        // 发送完成事件
        if (!currentExposureMessageId.empty()) {
          sendExposureCompleteEvent(currentExposureMessageId);
          currentExposureMessageId.clear();
        }

        // 调用回调函数(如果设置了)
        if (exposureCallback) {
          std::lock_guard cbLock(callbackMutex);
          exposureCallback(true, "Exposure completed successfully");
        }
      }
      break;
    }

    case CameraState::WAITING_TRIGGER:
    case CameraState::DOWNLOADING:
    case CameraState::PROCESSING:
    case CameraState::ERROR:
    case CameraState::COOLING:
    case CameraState::WARMING_UP:
      // 其他状态的处理...
      exposureCV.wait_for(lock, std::chrono::milliseconds(100));
      break;
    }
  }

  SPDLOG_INFO("Update loop ended: {}", deviceId);
}

void Camera::generateImageData() {
  // 计算图像大小
  int effectiveWidth = roi.width / roi.binX;
  int effectiveHeight = roi.height / roi.binY;
  int bytesPerPixel = cameraParams.bitDepth / 8;
  if (bytesPerPixel < 1)
    bytesPerPixel = 1;

  // 颜色通道数
  int channels = cameraParams.hasColorSensor ? 3 : 1;

  // 计算总数据大小
  size_t dataSize = effectiveWidth * effectiveHeight * bytesPerPixel * channels;

  {
    std::lock_guard lock(imageDataMutex);
    imageData.resize(dataSize);

    // 基础亮度取决于曝光时间和增益
    double baseIntensity =
        std::min(1.0, exposureDuration / 10.0) * (1.0 + gain / 100.0);

    // 根据图像类型调整
    if (currentImageType == ImageType::DARK) {
      baseIntensity = 0.05; // 暗场有微小信号
    } else if (currentImageType == ImageType::BIAS) {
      baseIntensity = 0.02; // 偏置帧只有偏置
    } else if (currentImageType == ImageType::FLAT) {
      baseIntensity = 0.7; // 平场相对均匀且亮度中等
    }

    // 生成图像数据
    std::uniform_real_distribution<> noiseDist(-0.1, 0.1);
    std::normal_distribution<> starDist(0.5, 0.2);

    for (int y = 0; y < effectiveHeight; ++y) {
      for (int x = 0; x < effectiveWidth; ++x) {
        // 基础值(加上噪声)
        double value = baseIntensity * (1.0 + noiseDist(rng) * 0.1);

        // 对于光照和平场图像，添加一些结构
        if (currentImageType == ImageType::LIGHT) {
          // 添加一些随机星星
          if (starDist(rng) > 0.95) {
            value = std::min(1.0, value * (5.0 + starDist(rng) * 10.0));
          }

          // 添加一些渐变
          value *= 1.0 + 0.1 * std::sin(x * 0.01) * std::cos(y * 0.01);
        } else if (currentImageType == ImageType::FLAT) {
          // 平场有轻微的渐变
          value *=
              1.0 -
              0.1 *
                  ((x - effectiveWidth / 2.0) * (x - effectiveWidth / 2.0) +
                   (y - effectiveHeight / 2.0) * (y - effectiveHeight / 2.0)) /
                  (effectiveWidth * effectiveHeight / 4.0);
        }

        // 将值转换为像素值
        int pixelValue =
            static_cast<int>(value * ((1 << cameraParams.bitDepth) - 1));
        pixelValue =
            std::max(0, std::min(pixelValue, (1 << cameraParams.bitDepth) - 1));

        // 填充像素数据
        for (int c = 0; c < channels; ++c) {
          size_t index = (y * effectiveWidth + x) * bytesPerPixel * channels +
                         c * bytesPerPixel;

          // 对于颜色传感器，为不同通道添加不同强度
          double channelFactor = 1.0;
          if (channels > 1) {
            // 在彩色传感器上模拟RGB通道不同响应
            if (c == 0)
              channelFactor = 1.0; // 红色通道
            else if (c == 1)
              channelFactor = 0.8; // 绿色通道
            else
              channelFactor = 0.6; // 蓝色通道
          }

          int channelValue = static_cast<int>(pixelValue * channelFactor);

          // 填充字节(按大端序)
          for (int b = 0; b < bytesPerPixel; ++b) {
            imageData[index + b] =
                (channelValue >> (8 * (bytesPerPixel - b - 1))) & 0xFF;
          }
        }
      }
    }

    // 应用一些额外图像效果
    applyImageEffects(imageData);
  }

  // 设置图像参数属性
  setProperty("imageWidth", effectiveWidth);
  setProperty("imageHeight", effectiveHeight);
  setProperty("imageChannels", channels);
  setProperty("imageDepth", cameraParams.bitDepth);
  setProperty("imageSize", dataSize);
}

void Camera::applyImageEffects(std::vector<uint8_t> &imageData) {
  // 这个方法可以应用各种图像效果，如热像素、坏列、渐晕等

  // 添加一些热像素(只在曝光时间长的情况下)
  if (exposureDuration > 1.0) {
    int effectiveWidth = roi.width / roi.binX;
    int effectiveHeight = roi.height / roi.binY;
    int bytesPerPixel = cameraParams.bitDepth / 8;
    if (bytesPerPixel < 1)
      bytesPerPixel = 1;
    int channels = cameraParams.hasColorSensor ? 3 : 1;

    // 热像素数量与曝光时间和传感器温度有关
    int hotPixelCount = static_cast<int>(
        5.0 * exposureDuration * std::exp((sensorTemperature + 20) / 30.0));

    std::uniform_int_distribution<> xDist(0, effectiveWidth - 1);
    std::uniform_int_distribution<> yDist(0, effectiveHeight - 1);

    for (int i = 0; i < hotPixelCount; ++i) {
      int x = xDist(rng);
      int y = yDist(rng);

      // 设置为较高值
      for (int c = 0; c < channels; ++c) {
        size_t index = (y * effectiveWidth + x) * bytesPerPixel * channels +
                       c * bytesPerPixel;

        // 将所有字节设为最大值(热像素)
        for (int b = 0; b < bytesPerPixel; ++b) {
          imageData[index + b] = 0xFF;
        }
      }
    }
  }

  // 这里可以添加更多效果...
}

void Camera::sendExposureProgressEvent(double progress) {
  EventMessage event("EXPOSURE_PROGRESS");

  json details = {{"progress", progress},
                  {"duration", exposureDuration.load()},
                  {"remainingTime", exposureDuration * (1.0 - progress)},
                  {"imageType", imageTypeToString(currentImageType)}};
  event.setDetails(details);

  sendEvent(event);
}

void Camera::sendExposureCompleteEvent(const std::string &relatedMessageId,
                                       bool success,
                                       const std::string &errorMessage) {
  EventMessage event("COMMAND_COMPLETED");
  event.setRelatedMessageId(relatedMessageId);

  json details = {{"command", "START_EXPOSURE"},
                  {"status", success ? "SUCCESS" : "ERROR"},
                  {"duration", exposureDuration.load()},
                  {"imageType", imageTypeToString(currentImageType)},
                  {"width", roi.width / roi.binX},
                  {"height", roi.height / roi.binY},
                  {"imageReady", success}};

  if (!success && !errorMessage.empty()) {
    details["errorMessage"] = errorMessage;
  }

  event.setDetails(details);
  sendEvent(event);
}

ImageType Camera::stringToImageType(const std::string &typeStr) {
  if (typeStr == "DARK")
    return ImageType::DARK;
  if (typeStr == "BIAS")
    return ImageType::BIAS;
  if (typeStr == "FLAT")
    return ImageType::FLAT;
  if (typeStr == "TEST")
    return ImageType::TEST;
  return ImageType::LIGHT; // Default
}

std::string Camera::imageTypeToString(ImageType type) {
  switch (type) {
  case ImageType::DARK:
    return "DARK";
  case ImageType::BIAS:
    return "BIAS";
  case ImageType::FLAT:
    return "FLAT";
  case ImageType::TEST:
    return "TEST";
  case ImageType::LIGHT:
    return "LIGHT";
  default:
    return "UNKNOWN";
  }
}

CameraState Camera::stringToCameraState(const std::string &stateStr) {
  if (stateStr == "EXPOSING")
    return CameraState::EXPOSING;
  if (stateStr == "READING_OUT")
    return CameraState::READING_OUT;
  if (stateStr == "DOWNLOADING")
    return CameraState::DOWNLOADING;
  if (stateStr == "PROCESSING")
    return CameraState::PROCESSING;
  if (stateStr == "ERROR")
    return CameraState::ERROR;
  if (stateStr == "WAITING_TRIGGER")
    return CameraState::WAITING_TRIGGER;
  if (stateStr == "COOLING")
    return CameraState::COOLING;
  if (stateStr == "WARMING_UP")
    return CameraState::WARMING_UP;
  return CameraState::IDLE; // Default
}

std::string Camera::cameraStateToString(CameraState state) {
  switch (state) {
  case CameraState::EXPOSING:
    return "EXPOSING";
  case CameraState::READING_OUT:
    return "READING_OUT";
  case CameraState::DOWNLOADING:
    return "DOWNLOADING";
  case CameraState::PROCESSING:
    return "PROCESSING";
  case CameraState::ERROR:
    return "ERROR";
  case CameraState::WAITING_TRIGGER:
    return "WAITING_TRIGGER";
  case CameraState::COOLING:
    return "COOLING";
  case CameraState::WARMING_UP:
    return "WARMING_UP";
  case CameraState::IDLE:
    return "IDLE";
  default:
    return "UNKNOWN";
  }
}

// 命令处理器实现
void Camera::handleStartExposureCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();

  // 检查必要参数
  if (!params.contains("duration")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'duration'"}});
    return;
  }

  double duration = params["duration"];

  // 图像类型(可选)
  ImageType type = ImageType::LIGHT;
  if (params.contains("type") && params["type"].is_string()) {
    type = stringToImageType(params["type"]);
  }

  // 是否自动保存(可选)
  bool saveImage = false;
  if (params.contains("autoSave") && params["autoSave"].is_boolean()) {
    saveImage = params["autoSave"];
  }

  // 存储消息ID以便完成事件
  currentExposureMessageId = cmd.getMessageId();

  // 开始曝光
  bool success = startExposure(duration, type, saveImage);

  if (success) {
    // 计算估计完成时间
    auto now = std::chrono::system_clock::now();
    auto completeTime = now + std::chrono::milliseconds(static_cast<int>(
                                  (duration + 1.0) * 1000)); // 加1秒用于读出
    auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
    std::ostringstream est_ss;
    est_ss << std::put_time(std::localtime(&complete_itt), "%FT%T") << "Z";

    response.setStatus("IN_PROGRESS");
    response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                         {"progressPercentage", 0.0},
                         {"duration", duration},
                         {"imageType", imageTypeToString(type)}});
  } else {
    response.setStatus("ERROR");
    response.setDetails({{"error", "EXPOSURE_FAILED"},
                         {"message", "Failed to start exposure"}});
  }
}

void Camera::handleAbortExposureCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  bool success = abortExposure();

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"message", "Exposure aborted"}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "ABORT_FAILED"},
         {"message", "No exposure in progress or abort failed"}});
  }
}

void Camera::handleGetImageCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  std::lock_guard lock(imageDataMutex);

  if (imageData.empty()) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "NO_IMAGE"}, {"message", "No image data available"}});
    return;
  }

  // 此处我们不发送实际图像数据(可能很大)
  // 而是提供一个下载URL或数据摘要信息
  response.setStatus("SUCCESS");
  response.setDetails({{"imageReady", true},
                       {"imageSize", imageData.size()},
                       {"imageWidth", roi.width / roi.binX},
                       {"imageHeight", roi.height / roi.binY},
                       {"imageDepth", cameraParams.bitDepth},
                       {"imageChannels", cameraParams.hasColorSensor ? 3 : 1}});

  // 在真实实现中，这里可以设置一个数据传输会话ID或URL
  // 客户端随后可以使用它来下载完整图像
  SPDLOG_INFO("Image data info sent to client: {}", deviceId);
}

void Camera::handleSetCoolerCommand(const CommandMessage &cmd,
                                    ResponseMessage &response) {
  json params = cmd.getParameters();
  bool success = true;
  std::string errorMessage;

  // 设置制冷器启用状态
  if (params.contains("enabled")) {
    if (!params["enabled"].is_boolean()) {
      success = false;
      errorMessage = "Invalid 'enabled' parameter, must be boolean";
    } else if (!setCoolerEnabled(params["enabled"])) {
      success = false;
      errorMessage = "Failed to set cooler state";
    }
  }

  // 设置目标温度
  if (success && params.contains("temperature")) {
    if (!params["temperature"].is_number()) {
      success = false;
      errorMessage = "Invalid 'temperature' parameter, must be number";
    } else if (!setCoolerTemperature(params["temperature"])) {
      success = false;
      errorMessage = "Failed to set cooler temperature";
    }
  }

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"coolerEnabled", coolerEnabled.load()},
                         {"targetTemperature", targetTemperature.load()},
                         {"currentTemperature", sensorTemperature.load()},
                         {"coolerPower", coolerPower.load()}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "COOLER_COMMAND_FAILED"}, {"message", errorMessage}});
  }
}

void Camera::handleSetROICommand(const CommandMessage &cmd,
                                 ResponseMessage &response) {
  json params = cmd.getParameters();
  bool success = true;
  std::string errorMessage;

  // 设置ROI
  if (params.contains("x") && params.contains("y") &&
      params.contains("width") && params.contains("height")) {

    if (!params["x"].is_number() || !params["y"].is_number() ||
        !params["width"].is_number() || !params["height"].is_number()) {
      success = false;
      errorMessage = "ROI parameters must be numbers";
    } else {
      int x = params["x"];
      int y = params["y"];
      int width = params["width"];
      int height = params["height"];

      if (!setROI(x, y, width, height)) {
        success = false;
        errorMessage = "Failed to set ROI";
      }
    }
  }

  // 设置像素合并
  if (success && params.contains("binX") && params.contains("binY")) {
    if (!params["binX"].is_number() || !params["binY"].is_number()) {
      success = false;
      errorMessage = "Binning parameters must be numbers";
    } else {
      int binX = params["binX"];
      int binY = params["binY"];

      if (!setBinning(binX, binY)) {
        success = false;
        errorMessage = "Failed to set binning";
      }
    }
  }

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"x", roi.x},
                         {"y", roi.y},
                         {"width", roi.width},
                         {"height", roi.height},
                         {"binX", roi.binX},
                         {"binY", roi.binY},
                         {"effectiveWidth", roi.width / roi.binX},
                         {"effectiveHeight", roi.height / roi.binY}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "ROI_COMMAND_FAILED"}, {"message", errorMessage}});
  }
}

void Camera::handleSetGainOffsetCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();
  bool success = true;
  std::string errorMessage;

  // 设置增益
  if (params.contains("gain")) {
    if (!params["gain"].is_number()) {
      success = false;
      errorMessage = "Invalid 'gain' parameter, must be number";
    } else if (!setGain(params["gain"])) {
      success = false;
      errorMessage = "Failed to set gain";
    }
  }

  // 设置偏移量
  if (success && params.contains("offset")) {
    if (!params["offset"].is_number()) {
      success = false;
      errorMessage = "Invalid 'offset' parameter, must be number";
    } else if (!setOffset(params["offset"])) {
      success = false;
      errorMessage = "Failed to set offset";
    }
  }

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"gain", gain.load()}, {"offset", offset.load()}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "GAIN_OFFSET_COMMAND_FAILED"}, {"message", errorMessage}});
  }
}

void Camera::handleSetFilterCommand(const CommandMessage &cmd,
                                    ResponseMessage &response) {
  if (!cameraParams.hasFilterWheel) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "NOT_SUPPORTED"},
                         {"message", "Camera does not have a filter wheel"}});
    return;
  }

  json params = cmd.getParameters();
  bool success = true;
  std::string errorMessage;

  // 设置滤镜位置
  if (params.contains("position")) {
    if (!params["position"].is_number()) {
      success = false;
      errorMessage = "Invalid 'position' parameter, must be number";
    } else if (!setFilterPosition(params["position"])) {
      success = false;
      errorMessage = "Failed to set filter position";
    }
  }

  // 设置滤镜名称
  if (success && params.contains("name") && params.contains("namePosition")) {
    int pos = params["namePosition"];
    std::string name = params["name"];

    if (!setFilterName(pos, name)) {
      success = false;
      errorMessage = "Failed to set filter name";
    }
  }

  if (success) {
    response.setStatus("SUCCESS");

    // 获取滤镜名称列表
    json filterNamesJson = json::array();
    {
      std::lock_guard lock(filterMutex);
      for (int i = 0; i < cameraParams.numFilters; ++i) {
        filterNamesJson.push_back(filterNames[i]);
      }
    }

    response.setDetails(
        {{"filterPosition", currentFilterPosition.load()},
         {"currentFilter", getFilterName(currentFilterPosition)},
         {"filterNames", filterNamesJson}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "FILTER_COMMAND_FAILED"}, {"message", errorMessage}});
  }
}

} // namespace astrocomm
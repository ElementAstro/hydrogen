#include "asi_camera.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <spdlog/spdlog.h>

namespace astrocomm {

CameraParameters ASICamera::getDefaultParams() {
  // ASI294MM Pro 默认参数
  CameraParameters params;
  params.width = 4144;
  params.height = 2822;
  params.bitDepth = 14;          // 通常为14位ADC
  params.hasColorSensor = true;  // ASI相机有彩色和单色版本
  params.hasCooler = true;       // Pro型号通常有制冷功能
  params.hasFilterWheel = false; // ASI相机通常需要外部滤镜轮
  params.maxBinningX = 4;
  params.maxBinningY = 4;
  params.pixelSizeX = 4.63;
  params.pixelSizeY = 4.63;
  params.maxGain = 600; // ASI相机通常具有较高的增益范围
  params.maxOffset = 100;
  params.minExposureTime = 0.00001; // ASI支持非常短的曝光
  params.maxExposureTime = 2000.0;
  params.minCoolerTemp = -40.0;
  params.numFilters = 0;
  return params;
}

ASICamera::ASICamera(const std::string &deviceId, const std::string &model,
                     const CameraParameters &params)
    : Camera(deviceId, "ZWO", model, params), gamma(50), whiteBalanceR(52),
      whiteBalanceB(95), hardwareBin(true), highSpeedMode(false),
      fanEnabled(true), antiDewHeater(0), humidity(20.0f), pressure(1013.25f) {

  // 初始化翻转模式
  flipMode.horizontal = false;
  flipMode.vertical = false;

  // 初始化自动曝光参数
  autoExposureParams.maxGain = 300;
  autoExposureParams.maxExposure = 30.0;

  // 设为非基类实现
  baseImplementation = false;

  // 设置ASI相机特有的属性
  setProperty("gamma", gamma);
  setProperty("whiteBalanceR", whiteBalanceR);
  setProperty("whiteBalanceB", whiteBalanceB);
  setProperty("hardwareBin", hardwareBin);
  setProperty("highSpeedMode", highSpeedMode);
  setProperty("fanEnabled", fanEnabled);
  setProperty("antiDewHeater", antiDewHeater);
  setProperty("humidity", humidity);
  setProperty("pressure", pressure);
  setProperty("flipHorizontal", flipMode.horizontal);
  setProperty("flipVertical", flipMode.vertical);
  setProperty("autoExposureMaxGain", autoExposureParams.maxGain);
  setProperty("autoExposureMaxTime", autoExposureParams.maxExposure);

  // 添加ASI相机特有的能力
  capabilities.push_back("GAMMA_CONTROL");
  capabilities.push_back("WHITE_BALANCE");
  capabilities.push_back("HIGH_SPEED_MODE");
  capabilities.push_back("HARDWARE_BIN");
  capabilities.push_back("FAN_CONTROL");
  capabilities.push_back("ANTI_DEW_HEATER");
  if (params.hasCooler) {
    capabilities.push_back("HUMIDITY_SENSOR");
  }

  // 初始化支持的控制功能
  initSupportedControls();

  // 注册ASI相机特有的命令处理器
  registerCommandHandler("ASI_CONTROL", [this](const CommandMessage &cmd,
                                               ResponseMessage &response) {
    handleASISpecificCommand(cmd, response);
  });

  SPDLOG_INFO("ASI Camera initialized: {}", model);
}

ASICamera::~ASICamera() {
  // 特定清理工作
}

bool ASICamera::isBaseImplementation() const { return false; }

bool ASICamera::setGamma(int value) {
  if (value < 0 || value > 100) {
    SPDLOG_WARN("Invalid gamma value: {}", value);
    return false;
  }

  gamma = value;
  setProperty("gamma", gamma);
  SPDLOG_INFO("Gamma set to {}", gamma);
  return true;
}

int ASICamera::getGamma() const { return gamma; }

bool ASICamera::setWhiteBalanceR(int value) {
  if (value < 0 || value > 100) {
    SPDLOG_WARN("Invalid white balance R value: {}", value);
    return false;
  }

  whiteBalanceR = value;
  setProperty("whiteBalanceR", whiteBalanceR);
  SPDLOG_INFO("White balance R set to {}", whiteBalanceR);
  return true;
}

int ASICamera::getWhiteBalanceR() const { return whiteBalanceR; }

bool ASICamera::setWhiteBalanceB(int value) {
  if (value < 0 || value > 100) {
    SPDLOG_WARN("Invalid white balance B value: {}", value);
    return false;
  }

  whiteBalanceB = value;
  setProperty("whiteBalanceB", whiteBalanceB);
  SPDLOG_INFO("White balance B set to {}", whiteBalanceB);
  return true;
}

int ASICamera::getWhiteBalanceB() const { return whiteBalanceB; }

bool ASICamera::setHardwareBin(bool enabled) {
  hardwareBin = enabled;
  setProperty("hardwareBin", hardwareBin);
  SPDLOG_INFO("Hardware bin {}", hardwareBin ? "enabled" : "disabled");
  return true;
}

bool ASICamera::getHardwareBin() const { return hardwareBin; }

bool ASICamera::setHighSpeedMode(bool enabled) {
  highSpeedMode = enabled;
  setProperty("highSpeedMode", highSpeedMode);
  SPDLOG_INFO("High speed mode {}", highSpeedMode ? "enabled" : "disabled");
  return true;
}

bool ASICamera::getHighSpeedMode() const { return highSpeedMode; }

bool ASICamera::setFanEnabled(bool enabled) {
  if (!cameraParams.hasCooler) {
    SPDLOG_WARN("Camera does not have cooling system with fan");
    return false;
  }

  fanEnabled = enabled;
  setProperty("fanEnabled", fanEnabled);
  SPDLOG_INFO("Fan {}", fanEnabled ? "enabled" : "disabled");
  return true;
}

bool ASICamera::getFanEnabled() const { return fanEnabled; }

bool ASICamera::setAntiDewHeater(int value) {
  if (!cameraParams.hasCooler) {
    SPDLOG_WARN("Camera does not have cooling system with heater");
    return false;
  }

  if (value < 0 || value > 100) {
    SPDLOG_WARN("Invalid anti-dew heater value: {}", value);
    return false;
  }

  antiDewHeater = value;
  setProperty("antiDewHeater", antiDewHeater);
  SPDLOG_INFO("Anti-dew heater set to {}", antiDewHeater);
  return true;
}

int ASICamera::getAntiDewHeater() const { return antiDewHeater; }

bool ASICamera::setAutoExposure(bool enabled, int maxGain, double maxExposure,
                                int targetBrightness) {
  // 更新Camera基类的autoExposure结构
  autoExposure.enabled = enabled;

  if (targetBrightness >= 0 && targetBrightness <= 255) {
    autoExposure.targetBrightness = targetBrightness;
  }

  // 更新ASI特有的自动曝光参数
  if (maxGain >= 0 && maxGain <= cameraParams.maxGain) {
    autoExposureParams.maxGain = maxGain;
  }

  if (maxExposure >= cameraParams.minExposureTime &&
      maxExposure <= cameraParams.maxExposureTime) {
    autoExposureParams.maxExposure = maxExposure;
  }

  // 更新属性
  setProperty("autoExposure", autoExposure.enabled);
  setProperty("autoExposureTarget", autoExposure.targetBrightness);
  setProperty("autoExposureMaxGain", autoExposureParams.maxGain);
  setProperty("autoExposureMaxTime", autoExposureParams.maxExposure);

  SPDLOG_INFO(
      "Auto exposure {}, target brightness: {}, max gain: {}, max exposure: {}",
      enabled ? "enabled" : "disabled", autoExposure.targetBrightness,
      autoExposureParams.maxGain, autoExposureParams.maxExposure);

  return true;
}

json ASICamera::getAutoExposureParameters() const {
  return {{"enabled", autoExposure.enabled},
          {"targetBrightness", autoExposure.targetBrightness},
          {"maxGain", autoExposureParams.maxGain},
          {"maxExposure", autoExposureParams.maxExposure},
          {"tolerance", autoExposure.tolerance}};
}

json ASICamera::getSupportedControls() const {
  json controlsJson = json::array();

  for (const auto &control : supportedControls) {
    controlsJson.push_back({{"type", controlTypeToString(control.type)},
                            {"name", control.name},
                            {"minValue", control.minValue},
                            {"maxValue", control.maxValue},
                            {"defaultValue", control.defaultValue},
                            {"isAutoSupported", control.isAutoSupported},
                            {"isWritable", control.isWritable}});
  }

  return controlsJson;
}

float ASICamera::getHumidity() const { return humidity; }

float ASICamera::getPressure() const { return pressure; }

bool ASICamera::setFlipMode(bool horizontal, bool vertical) {
  flipMode.horizontal = horizontal;
  flipMode.vertical = vertical;

  setProperty("flipHorizontal", horizontal);
  setProperty("flipVertical", vertical);

  SPDLOG_INFO("Flip mode set to {}{}", horizontal ? "horizontal" : "none",
              vertical ? ", vertical" : "");

  return true;
}

json ASICamera::getFlipMode() const {
  return {{"horizontal", flipMode.horizontal}, {"vertical", flipMode.vertical}};
}

void ASICamera::generateImageData() {
  // 先调用基类方法生成基础图像
  Camera::generateImageData();

  // 然后应用ASI特有的处理
  {
    std::lock_guard<std::mutex> lock(imageDataMutex);

    // 应用伽马校正
    if (gamma != 50) { // 50为默认中性值
      double gammaFactor = gamma / 50.0;

      int effectiveWidth = roi.width / roi.binX;
      int effectiveHeight = roi.height / roi.binY;
      int bytesPerPixel = cameraParams.bitDepth / 8;
      if (bytesPerPixel < 1)
        bytesPerPixel = 1;
      int channels = cameraParams.hasColorSensor ? 3 : 1;

      for (int y = 0; y < effectiveHeight; ++y) {
        for (int x = 0; x < effectiveWidth; ++x) {
          for (int c = 0; c < channels; ++c) {
            size_t index = (y * effectiveWidth + x) * bytesPerPixel * channels +
                           c * bytesPerPixel;

            if (bytesPerPixel == 2) {
              // 16位数据
              uint16_t value = (imageData[index] << 8) | imageData[index + 1];
              double normalizedValue = value / 65535.0;
              double adjustedValue =
                  std::pow(normalizedValue, 1.0 / gammaFactor);
              int newValue = static_cast<int>(adjustedValue * 65535.0);
              newValue = std::max(0, std::min(65535, newValue));

              imageData[index] = (newValue >> 8) & 0xFF;
              imageData[index + 1] = newValue & 0xFF;
            } else {
              // 8位数据
              double normalizedValue = imageData[index] / 255.0;
              double adjustedValue =
                  std::pow(normalizedValue, 1.0 / gammaFactor);
              int newValue = static_cast<int>(adjustedValue * 255.0);
              newValue = std::max(0, std::min(255, newValue));

              imageData[index] = static_cast<uint8_t>(newValue);
            }
          }
        }
      }
    }

    // 应用白平衡（只对彩色传感器）
    if (cameraParams.hasColorSensor &&
        (whiteBalanceR != 52 || whiteBalanceB != 95)) {
      double redFactor = whiteBalanceR / 52.0;
      double blueFactor = whiteBalanceB / 95.0;

      int effectiveWidth = roi.width / roi.binX;
      int effectiveHeight = roi.height / roi.binY;
      int bytesPerPixel = cameraParams.bitDepth / 8;
      if (bytesPerPixel < 1)
        bytesPerPixel = 1;

      for (int y = 0; y < effectiveHeight; ++y) {
        for (int x = 0; x < effectiveWidth; ++x) {
          // 红色通道
          size_t redIndex = (y * effectiveWidth + x) * bytesPerPixel * 3;
          if (bytesPerPixel == 2) {
            uint16_t value =
                (imageData[redIndex] << 8) | imageData[redIndex + 1];
            int newValue = static_cast<int>(value * redFactor);
            newValue = std::max(0, std::min(65535, newValue));
            imageData[redIndex] = (newValue >> 8) & 0xFF;
            imageData[redIndex + 1] = newValue & 0xFF;
          } else {
            int newValue = static_cast<int>(imageData[redIndex] * redFactor);
            newValue = std::max(0, std::min(255, newValue));
            imageData[redIndex] = static_cast<uint8_t>(newValue);
          }

          // 蓝色通道
          size_t blueIndex = redIndex + 2 * bytesPerPixel;
          if (bytesPerPixel == 2) {
            uint16_t value =
                (imageData[blueIndex] << 8) | imageData[blueIndex + 1];
            int newValue = static_cast<int>(value * blueFactor);
            newValue = std::max(0, std::min(65535, newValue));
            imageData[blueIndex] = (newValue >> 8) & 0xFF;
            imageData[blueIndex + 1] = newValue & 0xFF;
          } else {
            int newValue = static_cast<int>(imageData[blueIndex] * blueFactor);
            newValue = std::max(0, std::min(255, newValue));
            imageData[blueIndex] = static_cast<uint8_t>(newValue);
          }
        }
      }
    }

    // 应用图像翻转
    int effectiveWidth = roi.width / roi.binX;
    int effectiveHeight = roi.height / roi.binY;
    int bytesPerPixel = cameraParams.bitDepth / 8;
    if (bytesPerPixel < 1)
      bytesPerPixel = 1;
    int channels = cameraParams.hasColorSensor ? 3 : 1;
    int rowSize = effectiveWidth * bytesPerPixel * channels;

    // 水平翻转
    if (flipMode.horizontal) {
      std::vector<uint8_t> tempRow(rowSize);

      for (int y = 0; y < effectiveHeight; ++y) {
        for (int x = 0; x < effectiveWidth / 2; ++x) {
          for (int c = 0; c < channels; ++c) {
            size_t leftIndex =
                (y * effectiveWidth + x) * bytesPerPixel * channels +
                c * bytesPerPixel;
            size_t rightIndex =
                (y * effectiveWidth + (effectiveWidth - 1 - x)) *
                    bytesPerPixel * channels +
                c * bytesPerPixel;

            // 交换左右像素
            for (int b = 0; b < bytesPerPixel; ++b) {
              std::swap(imageData[leftIndex + b], imageData[rightIndex + b]);
            }
          }
        }
      }
    }

    // 垂直翻转
    if (flipMode.vertical) {
      std::vector<uint8_t> tempRow(rowSize);

      for (int y = 0; y < effectiveHeight / 2; ++y) {
        // 复制上行
        memcpy(tempRow.data(), &imageData[y * rowSize], rowSize);

        // 下行复制到上行
        memcpy(&imageData[y * rowSize],
               &imageData[(effectiveHeight - 1 - y) * rowSize], rowSize);

        // 临时行复制回下行
        memcpy(&imageData[(effectiveHeight - 1 - y) * rowSize], tempRow.data(),
               rowSize);
      }
    }
  }
}

void ASICamera::applyImageEffects(std::vector<uint8_t> &imageData) {
  // 先调用基类方法
  Camera::applyImageEffects(imageData);

  // 应用ASI相机特有效果
  int effectiveWidth = roi.width / roi.binX;
  int effectiveHeight = roi.height / roi.binY;
  int bytesPerPixel = cameraParams.bitDepth / 8;
  if (bytesPerPixel < 1)
    bytesPerPixel = 1;
  int channels = cameraParams.hasColorSensor ? 3 : 1;

  // 在高速模式下，增加一定的随机噪声
  if (highSpeedMode) {
    std::uniform_real_distribution<> noiseDist(-0.05, 0.05);

    for (size_t i = 0; i < imageData.size(); i += bytesPerPixel) {
      if (bytesPerPixel == 2) {
        uint16_t value = (imageData[i] << 8) | imageData[i + 1];
        double noise = noiseDist(rng) * 1000; // 高速模式下噪声增加
        int newValue = static_cast<int>(value + noise);
        newValue = std::max(0, std::min(65535, newValue));

        imageData[i] = (newValue >> 8) & 0xFF;
        imageData[i + 1] = newValue & 0xFF;
      } else {
        double noise = noiseDist(rng) * 10;
        int newValue = static_cast<int>(imageData[i] + noise);
        newValue = std::max(0, std::min(255, newValue));

        imageData[i] = static_cast<uint8_t>(newValue);
      }
    }
  }

  // CMOS传感器的特有模式：行噪声
  // ASI相机使用CMOS传感器，可能出现行噪声
  std::uniform_real_distribution<> rowNoiseDist(-0.01, 0.01);
  std::uniform_int_distribution<> rowDist(0, effectiveHeight - 1);
  int noisyRows = effectiveHeight / 20; // 约5%的行有额外噪声

  for (int i = 0; i < noisyRows; ++i) {
    int row = rowDist(rng);
    double rowOffset = rowNoiseDist(rng);

    for (int x = 0; x < effectiveWidth; ++x) {
      for (int c = 0; c < channels; ++c) {
        size_t index = (row * effectiveWidth + x) * bytesPerPixel * channels +
                       c * bytesPerPixel;

        if (bytesPerPixel == 2) {
          uint16_t value = (imageData[index] << 8) | imageData[index + 1];
          int newValue = static_cast<int>(value * (1.0 + rowOffset));
          newValue = std::max(0, std::min(65535, newValue));

          imageData[index] = (newValue >> 8) & 0xFF;
          imageData[index + 1] = newValue & 0xFF;
        } else {
          int newValue = static_cast<int>(imageData[index] * (1.0 + rowOffset));
          newValue = std::max(0, std::min(255, newValue));

          imageData[index] = static_cast<uint8_t>(newValue);
        }
      }
    }
  }
}

void ASICamera::updateLoop() {
  // 先调用基类的更新循环
  Camera::updateLoop();
}

void ASICamera::initSupportedControls() {
  // 初始化ASI相机支持的控制功能
  supportedControls.push_back({ASIControlType::GAIN, "Gain", 0,
                               static_cast<long>(cameraParams.maxGain), 0, true,
                               true});

  supportedControls.push_back(
      {ASIControlType::EXPOSURE, "Exposure",
       static_cast<long>(cameraParams.minExposureTime * 1000000), // 微秒
       static_cast<long>(cameraParams.maxExposureTime * 1000000), 10000, true,
       true});

  supportedControls.push_back(
      {ASIControlType::GAMMA, "Gamma", 0, 100, 50, false, true});

  if (cameraParams.hasColorSensor) {
    supportedControls.push_back(
        {ASIControlType::WB_R, "WB_R", 0, 100, 52, true, true});

    supportedControls.push_back(
        {ASIControlType::WB_B, "WB_B", 0, 100, 95, true, true});
  }

  supportedControls.push_back({ASIControlType::OFFSET, "Offset", 0,
                               static_cast<long>(cameraParams.maxOffset), 10,
                               false, true});

  supportedControls.push_back({ASIControlType::BANDWIDTHOVERLOAD, "BandWidth",
                               0, 100, 50, false, true});

  supportedControls.push_back(
      {ASIControlType::OVERCLOCK, "Overclock", 0, 2, 0, false, true});

  supportedControls.push_back({
      ASIControlType::TEMPERATURE, "Temperature", -500, 1000, 0, false,
      false // 温度以0.1°C为单位
  });

  supportedControls.push_back(
      {ASIControlType::FLIP, "Flip", 0, 3, 0, false, true});

  supportedControls.push_back({ASIControlType::AUTO_MAX_GAIN, "AutoMaxGain", 0,
                               static_cast<long>(cameraParams.maxGain),
                               static_cast<long>(cameraParams.maxGain / 2),
                               false, true});

  supportedControls.push_back(
      {ASIControlType::AUTO_MAX_EXP, "AutoMaxExp", 1000, // 1ms
       static_cast<long>(cameraParams.maxExposureTime * 1000000),
       static_cast<long>(30 * 1000000), // 30秒
       false, true});

  supportedControls.push_back({ASIControlType::AUTO_TARGET_BRIGHTNESS,
                               "AutoTargetBr", 0, 255, 128, false, true});

  supportedControls.push_back(
      {ASIControlType::HARDWARE_BIN, "HardwareBin", 0, 1, 1, false, true});

  supportedControls.push_back(
      {ASIControlType::HIGH_SPEED_MODE, "HighSpeedMode", 0, 1, 0, false, true});

  if (cameraParams.hasCooler) {
    supportedControls.push_back(
        {ASIControlType::COOLER_POWER, "CoolerPower", 0, 100, 0, false, false});

    supportedControls.push_back(
        {ASIControlType::TARGET_TEMP, "TargetTemp",
         static_cast<long>(cameraParams.minCoolerTemp * 10),
         400, // 40°C
         200, // 20°C
         false, true});

    supportedControls.push_back(
        {ASIControlType::COOLER_ON, "CoolerOn", 0, 1, 0, false, true});

    supportedControls.push_back(
        {ASIControlType::FAN_ON, "FanOn", 0, 1, 1, false, true});

    supportedControls.push_back({ASIControlType::ANTI_DEW_HEATER,
                                 "AntiDewHeater", 0, 100, 0, false, true});
  }

  supportedControls.push_back(
      {ASIControlType::PATTERN_ADJUST, "PatternAdjust", 0, 1, 1, false, true});

  if (cameraParams.hasCooler) {
    supportedControls.push_back(
        {ASIControlType::HUMIDITY, "Humidity", 0, 100, 0, false, false});

    supportedControls.push_back(
        {ASIControlType::PRESSURE, "Pressure", 0, 2000, 0, false, false});
  }

  // 设置控制功能属性
  setProperty("supportedControls", getSupportedControls());
}

void ASICamera::handleASISpecificCommand(const CommandMessage &cmd,
                                         ResponseMessage &response) {
  json params = cmd.getParameters();
  bool success = true;
  std::string errorMessage;

  // 处理ASI特有命令
  if (params.contains("control")) {
    std::string controlTypeStr = params["control"];

    try {
      ASIControlType controlType = stringToControlType(controlTypeStr);

      // 根据控制类型处理
      switch (controlType) {
      case ASIControlType::GAMMA:
        if (!params.contains("value")) {
          success = false;
          errorMessage = "Missing 'value' parameter for gamma control";
        } else if (!setGamma(params["value"])) {
          success = false;
          errorMessage = "Failed to set gamma value";
        }
        break;

      case ASIControlType::WB_R:
        if (!params.contains("value")) {
          success = false;
          errorMessage = "Missing 'value' parameter for white balance R";
        } else if (!setWhiteBalanceR(params["value"])) {
          success = false;
          errorMessage = "Failed to set white balance R";
        }
        break;

      case ASIControlType::WB_B:
        if (!params.contains("value")) {
          success = false;
          errorMessage = "Missing 'value' parameter for white balance B";
        } else if (!setWhiteBalanceB(params["value"])) {
          success = false;
          errorMessage = "Failed to set white balance B";
        }
        break;

      case ASIControlType::HARDWARE_BIN:
        if (!params.contains("value")) {
          success = false;
          errorMessage = "Missing 'value' parameter for hardware bin";
        } else if (!setHardwareBin(params["value"])) {
          success = false;
          errorMessage = "Failed to set hardware bin";
        }
        break;

      case ASIControlType::HIGH_SPEED_MODE:
        if (!params.contains("value")) {
          success = false;
          errorMessage = "Missing 'value' parameter for high speed mode";
        } else if (!setHighSpeedMode(params["value"])) {
          success = false;
          errorMessage = "Failed to set high speed mode";
        }
        break;

      case ASIControlType::FAN_ON:
        if (!params.contains("value")) {
          success = false;
          errorMessage = "Missing 'value' parameter for fan control";
        } else if (!setFanEnabled(params["value"])) {
          success = false;
          errorMessage = "Failed to set fan state";
        }
        break;

      case ASIControlType::ANTI_DEW_HEATER:
        if (!params.contains("value")) {
          success = false;
          errorMessage = "Missing 'value' parameter for anti-dew heater";
        } else if (!setAntiDewHeater(params["value"])) {
          success = false;
          errorMessage = "Failed to set anti-dew heater";
        }
        break;

      case ASIControlType::AUTO_MAX_GAIN:
      case ASIControlType::AUTO_MAX_EXP:
      case ASIControlType::AUTO_TARGET_BRIGHTNESS: {
        // 处理自动曝光参数
        bool autoEnabled = autoExposure.enabled;
        int maxGain = autoExposureParams.maxGain;
        double maxExp = autoExposureParams.maxExposure;
        int targetBr = autoExposure.targetBrightness;

        if (controlType == ASIControlType::AUTO_MAX_GAIN &&
            params.contains("value")) {
          maxGain = params["value"];
        } else if (controlType == ASIControlType::AUTO_MAX_EXP &&
                   params.contains("value")) {
          maxExp = params["value"].get<double>() / 1000000.0; // 从微秒转为秒
        } else if (controlType == ASIControlType::AUTO_TARGET_BRIGHTNESS &&
                   params.contains("value")) {
          targetBr = params["value"];
        }

        if (!setAutoExposure(autoEnabled, maxGain, maxExp, targetBr)) {
          success = false;
          errorMessage = "Failed to set auto exposure parameter";
        }
      } break;

      case ASIControlType::FLIP: {
        // 处理图像翻转
        int flipValue = params["value"];
        bool horizontal = (flipValue & 1) != 0;
        bool vertical = (flipValue & 2) != 0;

        if (!setFlipMode(horizontal, vertical)) {
          success = false;
          errorMessage = "Failed to set flip mode";
        }
      } break;

      default:
        success = false;
        errorMessage = "Unsupported control type: " + controlTypeStr;
      }
    } catch (const std::exception &e) {
      success = false;
      errorMessage =
          std::string("Error processing control command: ") + e.what();
    }
  } else if (params.contains("action")) {
    std::string action = params["action"];

    if (action == "GET_ALL_CONTROLS") {
      // 返回所有控制参数的当前值
      json controlValues = json::object();

      controlValues["GAMMA"] = gamma;
      controlValues["WB_R"] = whiteBalanceR;
      controlValues["WB_B"] = whiteBalanceB;
      controlValues["HARDWARE_BIN"] = hardwareBin ? 1 : 0;
      controlValues["HIGH_SPEED_MODE"] = highSpeedMode ? 1 : 0;
      controlValues["FAN_ON"] = fanEnabled ? 1 : 0;
      controlValues["ANTI_DEW_HEATER"] = antiDewHeater;
      controlValues["AUTO_MAX_GAIN"] = autoExposureParams.maxGain;
      controlValues["AUTO_MAX_EXP"] = static_cast<long>(
          autoExposureParams.maxExposure * 1000000); // 秒转微秒
      controlValues["AUTO_TARGET_BRIGHTNESS"] = autoExposure.targetBrightness;
      controlValues["TEMPERATURE"] =
          static_cast<int>(sensorTemperature * 10); // 转为0.1°C单位
      controlValues["COOLER_POWER"] = coolerPower.load();
      controlValues["COOLER_ON"] = coolerEnabled ? 1 : 0;
      controlValues["HUMIDITY"] =
          static_cast<int>(humidity * 10); // 转为0.1%单位
      controlValues["PRESSURE"] =
          static_cast<int>(pressure * 10); // 转为0.1hPa单位

      int flipValue = 0;
      if (flipMode.horizontal)
        flipValue |= 1;
      if (flipMode.vertical)
        flipValue |= 2;
      controlValues["FLIP"] = flipValue;

      response.setStatus("SUCCESS");
      response.setDetails({{"controlValues", controlValues}});
      return;
    } else {
      success = false;
      errorMessage = "Unknown action: " + action;
    }
  } else {
    success = false;
    errorMessage = "Missing 'control' or 'action' parameter";
  }

  // 处理结果
  if (success) {
    response.setStatus("SUCCESS");

    // 构建详细响应
    json details = {{"message", "ASI control command executed successfully"}};

    // 添加特定控制的当前值
    if (params.contains("control")) {
      std::string controlTypeStr = params["control"];
      ASIControlType controlType = stringToControlType(controlTypeStr);

      switch (controlType) {
      case ASIControlType::GAMMA:
        details["gamma"] = gamma;
        break;
      case ASIControlType::WB_R:
        details["whiteBalanceR"] = whiteBalanceR;
        break;
      case ASIControlType::WB_B:
        details["whiteBalanceB"] = whiteBalanceB;
        break;
      case ASIControlType::HARDWARE_BIN:
        details["hardwareBin"] = hardwareBin;
        break;
      case ASIControlType::HIGH_SPEED_MODE:
        details["highSpeedMode"] = highSpeedMode;
        break;
      case ASIControlType::FAN_ON:
        details["fanEnabled"] = fanEnabled;
        break;
      case ASIControlType::ANTI_DEW_HEATER:
        details["antiDewHeater"] = antiDewHeater;
        break;
      case ASIControlType::AUTO_MAX_GAIN:
      case ASIControlType::AUTO_MAX_EXP:
      case ASIControlType::AUTO_TARGET_BRIGHTNESS:
        details["autoExposure"] = getAutoExposureParameters();
        break;
      case ASIControlType::FLIP:
        details["flipMode"] = getFlipMode();
        break;
      default:
        break;
      }
    }

    response.setDetails(details);
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "ASI_COMMAND_FAILED"}, {"message", errorMessage}});
  }
}

std::string ASICamera::controlTypeToString(ASIControlType type) const {
  switch (type) {
  case ASIControlType::GAIN:
    return "GAIN";
  case ASIControlType::EXPOSURE:
    return "EXPOSURE";
  case ASIControlType::GAMMA:
    return "GAMMA";
  case ASIControlType::WB_R:
    return "WB_R";
  case ASIControlType::WB_B:
    return "WB_B";
  case ASIControlType::OFFSET:
    return "OFFSET";
  case ASIControlType::BANDWIDTHOVERLOAD:
    return "BANDWIDTHOVERLOAD";
  case ASIControlType::OVERCLOCK:
    return "OVERCLOCK";
  case ASIControlType::TEMPERATURE:
    return "TEMPERATURE";
  case ASIControlType::FLIP:
    return "FLIP";
  case ASIControlType::AUTO_MAX_GAIN:
    return "AUTO_MAX_GAIN";
  case ASIControlType::AUTO_MAX_EXP:
    return "AUTO_MAX_EXP";
  case ASIControlType::AUTO_TARGET_BRIGHTNESS:
    return "AUTO_TARGET_BRIGHTNESS";
  case ASIControlType::HARDWARE_BIN:
    return "HARDWARE_BIN";
  case ASIControlType::HIGH_SPEED_MODE:
    return "HIGH_SPEED_MODE";
  case ASIControlType::COOLER_POWER:
    return "COOLER_POWER";
  case ASIControlType::TARGET_TEMP:
    return "TARGET_TEMP";
  case ASIControlType::COOLER_ON:
    return "COOLER_ON";
  case ASIControlType::MONO_BIN:
    return "MONO_BIN";
  case ASIControlType::FAN_ON:
    return "FAN_ON";
  case ASIControlType::PATTERN_ADJUST:
    return "PATTERN_ADJUST";
  case ASIControlType::ANTI_DEW_HEATER:
    return "ANTI_DEW_HEATER";
  case ASIControlType::HUMIDITY:
    return "HUMIDITY";
  case ASIControlType::PRESSURE:
    return "PRESSURE";
  default:
    return "UNKNOWN";
  }
}

ASIControlType
ASICamera::stringToControlType(const std::string &typeStr) const {
  if (typeStr == "GAIN")
    return ASIControlType::GAIN;
  if (typeStr == "EXPOSURE")
    return ASIControlType::EXPOSURE;
  if (typeStr == "GAMMA")
    return ASIControlType::GAMMA;
  if (typeStr == "WB_R")
    return ASIControlType::WB_R;
  if (typeStr == "WB_B")
    return ASIControlType::WB_B;
  if (typeStr == "OFFSET")
    return ASIControlType::OFFSET;
  if (typeStr == "BANDWIDTHOVERLOAD")
    return ASIControlType::BANDWIDTHOVERLOAD;
  if (typeStr == "OVERCLOCK")
    return ASIControlType::OVERCLOCK;
  if (typeStr == "TEMPERATURE")
    return ASIControlType::TEMPERATURE;
  if (typeStr == "FLIP")
    return ASIControlType::FLIP;
  if (typeStr == "AUTO_MAX_GAIN")
    return ASIControlType::AUTO_MAX_GAIN;
  if (typeStr == "AUTO_MAX_EXP")
    return ASIControlType::AUTO_MAX_EXP;
  if (typeStr == "AUTO_TARGET_BRIGHTNESS")
    return ASIControlType::AUTO_TARGET_BRIGHTNESS;
  if (typeStr == "HARDWARE_BIN")
    return ASIControlType::HARDWARE_BIN;
  if (typeStr == "HIGH_SPEED_MODE")
    return ASIControlType::HIGH_SPEED_MODE;
  if (typeStr == "COOLER_POWER")
    return ASIControlType::COOLER_POWER;
  if (typeStr == "TARGET_TEMP")
    return ASIControlType::TARGET_TEMP;
  if (typeStr == "COOLER_ON")
    return ASIControlType::COOLER_ON;
  if (typeStr == "MONO_BIN")
    return ASIControlType::MONO_BIN;
  if (typeStr == "FAN_ON")
    return ASIControlType::FAN_ON;
  if (typeStr == "PATTERN_ADJUST")
    return ASIControlType::PATTERN_ADJUST;
  if (typeStr == "ANTI_DEW_HEATER")
    return ASIControlType::ANTI_DEW_HEATER;
  if (typeStr == "HUMIDITY")
    return ASIControlType::HUMIDITY;
  if (typeStr == "PRESSURE")
    return ASIControlType::PRESSURE;

  throw std::invalid_argument("Unknown control type: " + typeStr);
}

} // namespace astrocomm
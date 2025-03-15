#include "ccd_camera.h"
#include <spdlog/spdlog.h>

namespace astrocomm {

CameraParameters CCDCamera::getDefaultParams() {
  CameraParameters params;
  params.width = 3326;
  params.height = 2504;
  params.bitDepth = 16;
  params.hasColorSensor = false; // CCD通常是单色的
  params.hasCooler = true;       // CCD通常有制冷功能
  params.hasFilterWheel = true;  // 通常配有滤镜轮
  params.maxBinningX = 4;
  params.maxBinningY = 4;
  params.pixelSizeX = 5.4;
  params.pixelSizeY = 5.4;
  params.maxGain = 63;
  params.maxOffset = 511;
  params.minExposureTime = 0.001;
  params.maxExposureTime = 3600.0;
  params.minCoolerTemp = -50.0; // CCD可以更冷
  params.numFilters = 5;        // 典型的LRGBC滤镜轮
  return params;
}

CCDCamera::CCDCamera(const std::string &deviceId,
                     const std::string &manufacturer, const std::string &model,
                     const CameraParameters &params)
    : Camera(deviceId, manufacturer, model, params), invertReadout(false),
      antiBlooming(false), preExposureFlush(true) {

  // 将基类实现标志设置为false
  baseImplementation = false;

  // 设置CCD特有的属性
  setProperty("invertReadout", invertReadout);
  setProperty("antiBlooming", antiBlooming);
  setProperty("preExposureFlush", preExposureFlush);

  // 添加CCD特有的能力
  capabilities.push_back("ANTI_BLOOMING");
  capabilities.push_back("PRE_FLUSH");

  // 如果是单色CCD且有滤镜轮，设置默认滤镜名称
  if (!cameraParams.hasColorSensor && cameraParams.hasFilterWheel) {
    setFilterName(0, "Luminance");
    setFilterName(1, "Red");
    setFilterName(2, "Green");
    setFilterName(3, "Blue");
    setFilterName(4, "H-alpha");
  }

  // 注册CCD特有的命令处理器
  registerCommandHandler("CCD_SPECIFIC", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
    handleCCDSpecificCommand(cmd, response);
  });

  SPDLOG_INFO("CCD Camera initialized: {}", deviceId);
}

CCDCamera::~CCDCamera() {
  // 特有的清理工作
}

bool CCDCamera::isBaseImplementation() const { return false; }

bool CCDCamera::setInvertReadout(bool enabled) {
  invertReadout = enabled;
  setProperty("invertReadout", invertReadout);
  SPDLOG_INFO("Invert readout {}", enabled ? "enabled" : "disabled");
  return true;
}

bool CCDCamera::setAntiBlooming(bool enabled) {
  antiBlooming = enabled;
  setProperty("antiBlooming", antiBlooming);
  SPDLOG_INFO("Anti-blooming {}", enabled ? "enabled" : "disabled");
  return true;
}

bool CCDCamera::setPreExposureFlush(bool enabled) {
  preExposureFlush = enabled;
  setProperty("preExposureFlush", preExposureFlush);
  SPDLOG_INFO("Pre-exposure flush {}", enabled ? "enabled" : "disabled");
  return true;
}

void CCDCamera::generateImageData() {
  // 首先调用基类方法
  Camera::generateImageData();

  // 然后进行CCD特有的处理
  if (preExposureFlush) {
    // 模拟预曝光冲洗减少了热噪声
    std::lock_guard<std::mutex> lock(imageDataMutex);

    // 降低整体黑电平噪声
    for (size_t i = 0; i < imageData.size(); i += 2) { // 假设16位数据
      // 减少较低字节的波动
      if (imageData[i + 1] < 10) {       // 如果高字节较低(暗区域)
        imageData[i] = imageData[i] / 2; // 降低噪声
      }
    }
  }
}

void CCDCamera::applyImageEffects(std::vector<uint8_t> &imageData) {
  // 调用基类方法
  Camera::applyImageEffects(imageData);

  // 添加CCD特有的效果
  int effectiveWidth = roi.width / roi.binX;
  int effectiveHeight = roi.height / roi.binY;
  int bytesPerPixel = cameraParams.bitDepth / 8;
  if (bytesPerPixel < 1)
    bytesPerPixel = 1;

  // CCD典型的列状噪声
  if (currentImageType == ImageType::DARK ||
      currentImageType == ImageType::BIAS) {
    std::uniform_real_distribution<> noiseDist(-0.05, 0.05);
    std::uniform_int_distribution<> columnDist(0, effectiveWidth - 1);
    int noisyColumns = effectiveWidth / 30; // 约3%的列有额外噪声

    for (int i = 0; i < noisyColumns; ++i) {
      int column = columnDist(rng);
      double columnOffset = noiseDist(rng);

      for (int y = 0; y < effectiveHeight; ++y) {
        size_t index = (y * effectiveWidth + column) * bytesPerPixel;

        // 为这一列添加固定偏移
        if (bytesPerPixel == 2) {
          // 16位数据
          uint16_t value = (imageData[index] << 8) | imageData[index + 1];
          int newValue = static_cast<int>(value * (1.0 + columnOffset));
          newValue = std::max(0, std::min(65535, newValue));

          imageData[index] = (newValue >> 8) & 0xFF;
          imageData[index + 1] = newValue & 0xFF;
        } else {
          // 8位数据
          int newValue =
              static_cast<int>(imageData[index] * (1.0 + columnOffset));
          newValue = std::max(0, std::min(255, newValue));
          imageData[index] = static_cast<uint8_t>(newValue);
        }
      }
    }
  }

  // 如果启用了反转读出，反转图像
  if (invertReadout) {
    int channels = cameraParams.hasColorSensor ? 3 : 1;
    int rowSize = effectiveWidth * bytesPerPixel * channels;

    for (int y = 0; y < effectiveHeight / 2; ++y) {
      for (int x = 0; x < rowSize; ++x) {
        size_t topIndex = y * rowSize + x;
        size_t bottomIndex = (effectiveHeight - y - 1) * rowSize + x;
        std::swap(imageData[topIndex], imageData[bottomIndex]);
      }
    }
  }

  // 如果启用了抗反冲洗，限制过亮像素
  if (antiBlooming && currentImageType == ImageType::LIGHT) {
    int channels = cameraParams.hasColorSensor ? 3 : 1;

    // 抗反冲洗会限制最大像素值(但不是彻底切断)
    int maxValue = (1 << cameraParams.bitDepth) - 1;
    int threshold = static_cast<int>(maxValue * 0.9); // 90%的最大值作为阈值

    for (int y = 0; y < effectiveHeight; ++y) {
      for (int x = 0; x < effectiveWidth; ++x) {
        for (int c = 0; c < channels; ++c) {
          size_t index = (y * effectiveWidth + x) * bytesPerPixel * channels +
                         c * bytesPerPixel;

          if (bytesPerPixel == 2) {
            // 16位数据
            uint16_t value = (imageData[index] << 8) | imageData[index + 1];
            if (value > threshold) {
              // 计算新值，使得它永远不会达到maxValue
              int newValue = threshold + (value - threshold) / 4;

              imageData[index] = (newValue >> 8) & 0xFF;
              imageData[index + 1] = newValue & 0xFF;
            }
          } else {
            // 8位数据
            if (imageData[index] > threshold) {
              imageData[index] = threshold + (imageData[index] - threshold) / 4;
            }
          }
        }
      }
    }
  }
}

void CCDCamera::handleCCDSpecificCommand(const CommandMessage &cmd,
                                         ResponseMessage &response) {
  json params = cmd.getParameters();
  bool success = true;
  std::string errorMessage;

  // 处理CCD特有的命令
  if (params.contains("invertReadout")) {
    if (!params["invertReadout"].is_boolean()) {
      success = false;
      errorMessage = "Invalid 'invertReadout' parameter, must be boolean";
    } else if (!setInvertReadout(params["invertReadout"])) {
      success = false;
      errorMessage = "Failed to set invert readout";
    }
  }

  if (success && params.contains("antiBlooming")) {
    if (!params["antiBlooming"].is_boolean()) {
      success = false;
      errorMessage = "Invalid 'antiBlooming' parameter, must be boolean";
    } else if (!setAntiBlooming(params["antiBlooming"])) {
      success = false;
      errorMessage = "Failed to set anti-blooming";
    }
  }

  if (success && params.contains("preExposureFlush")) {
    if (!params["preExposureFlush"].is_boolean()) {
      success = false;
      errorMessage = "Invalid 'preExposureFlush' parameter, must be boolean";
    } else if (!setPreExposureFlush(params["preExposureFlush"])) {
      success = false;
      errorMessage = "Failed to set pre-exposure flush";
    }
  }

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"invertReadout", invertReadout},
                         {"antiBlooming", antiBlooming},
                         {"preExposureFlush", preExposureFlush}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "CCD_COMMAND_FAILED"}, {"message", errorMessage}});
  }
}

} // namespace astrocomm
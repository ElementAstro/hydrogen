#include "device/guider.h"
#include "common/logger.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace astrocomm {

Guider::Guider(const std::string &deviceId, const std::string &manufacturer,
               const std::string &model)
    : DeviceBase(deviceId, "GUIDER", manufacturer, model),
      state(GuiderState::IDLE), calibrationState(CalibrationState::IDLE),
      guideStarX(0), guideStarY(0), targetStarX(0), targetStarY(0), driftX(0),
      driftY(0), raAggressiveness(0.7), decAggressiveness(0.5),
      raGuideRate(0.5), decGuideRate(0.5), pixelScale(1.0), raCorrection(0),
      decCorrection(0), lastRaCorrection(0), lastDecCorrection(0), rms(0),
      peak(0), isSettling(false), settleThreshold(0.5), settleFrames(0),
      requiredSettleFrames(5), imageWidth(640), imageHeight(480),
      exposureTime(0.1), lastCaptureTime(0), guideStartTime(0),
      totalFramesCapured(0), updateRunning(false) {

  // 初始化随机数生成器
  std::random_device rd;
  rng = std::mt19937(rd());

  // 初始化校准数据
  calibration.raAngle = 0;
  calibration.decAngle = 90;
  calibration.raRate = 5.0;
  calibration.decRate = 5.0;
  calibration.flipped = false;
  calibration.calibrated = false;

  // 初始化图像数据
  imageData.resize(imageWidth * imageHeight);

  // 初始化属性
  setProperty("state", guiderStateToString(state));
  setProperty("calibrationState", calibrationStateToString(calibrationState));
  setProperty("calibrated", calibration.calibrated);
  setProperty("raAggressiveness", raAggressiveness);
  setProperty("decAggressiveness", decAggressiveness);
  setProperty("raGuideRate", raGuideRate);
  setProperty("decGuideRate", decGuideRate);
  setProperty("pixelScale", pixelScale);
  setProperty("rms", rms);
  setProperty("peak", peak);
  setProperty("exposureTime", exposureTime);
  setProperty("imageWidth", imageWidth);
  setProperty("imageHeight", imageHeight);
  setProperty("connected", false);

  // 设置能力
  capabilities = {"CALIBRATION", "GUIDING", "DITHERING"};

  // 注册命令处理器
  registerCommandHandler("START_GUIDING", [this](const CommandMessage &cmd,
                                                 ResponseMessage &response) {
    handleStartGuidingCommand(cmd, response);
  });

  registerCommandHandler("STOP_GUIDING", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
    handleStopGuidingCommand(cmd, response);
  });

  registerCommandHandler("PAUSE_GUIDING", [this](const CommandMessage &cmd,
                                                 ResponseMessage &response) {
    handlePauseGuidingCommand(cmd, response);
  });

  registerCommandHandler("RESUME_GUIDING", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleResumeGuidingCommand(cmd, response);
  });

  registerCommandHandler(
      "START_CALIBRATION",
      [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleStartCalibrationCommand(cmd, response);
      });

  registerCommandHandler(
      "CANCEL_CALIBRATION",
      [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleCancelCalibrationCommand(cmd, response);
      });

  registerCommandHandler(
      "DITHER", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleDitherCommand(cmd, response);
      });

  registerCommandHandler("SET_PARAMETERS", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleSetParametersCommand(cmd, response);
  });

  logInfo("Guider device initialized", deviceId);
}

Guider::~Guider() { stop(); }

bool Guider::start() {
  if (!DeviceBase::start()) {
    return false;
  }

  // 初始化导星星体位置
  guideStarX = imageWidth / 2.0;
  guideStarY = imageHeight / 2.0;
  targetStarX = guideStarX;
  targetStarY = guideStarY;

  // 启动更新线程
  updateRunning = true;
  updateThread = std::thread(&Guider::updateLoop, this);

  setProperty("connected", true);
  logInfo("Guider started", deviceId);
  return true;
}

void Guider::stop() {
  // 停止导星和校准
  if (state == GuiderState::GUIDING || state == GuiderState::PAUSED) {
    stopGuiding();
  }

  if (state == GuiderState::CALIBRATING) {
    cancelCalibration();
  }

  // 停止更新线程
  updateRunning = false;
  if (updateThread.joinable()) {
    updateThread.join();
  }

  setProperty("connected", false);
  DeviceBase::stop();
  logInfo("Guider stopped", deviceId);
}

void Guider::startGuiding() {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 检查是否已校准
  if (!calibration.calibrated) {
    logWarning("Cannot start guiding: not calibrated", deviceId);
    return;
  }

  // 检查当前状态
  if (state == GuiderState::GUIDING) {
    logInfo("Guiding already active", deviceId);
    return;
  }

  if (state == GuiderState::CALIBRATING) {
    logWarning("Cannot start guiding while calibrating", deviceId);
    return;
  }

  // 重置导星参数
  rms = 0;
  peak = 0;
  raCorrection = 0;
  decCorrection = 0;
  driftX = 0;
  driftY = 0;
  totalFramesCapured = 0;

  // 将目标位置设为当前位置
  targetStarX = guideStarX;
  targetStarY = guideStarY;

  // 记录开始时间
  guideStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();

  // 开始导星
  state = GuiderState::GUIDING;
  setProperty("state", guiderStateToString(state));

  logInfo("Guiding started", deviceId);

  // 发送状态事件
  sendGuidingStatusEvent();
}

void Guider::stopGuiding() {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (state != GuiderState::GUIDING && state != GuiderState::PAUSED) {
    logInfo("Guiding not active", deviceId);
    return;
  }

  // 停止导星
  state = GuiderState::IDLE;
  isSettling = false;
  settleFrames = 0;
  setProperty("state", guiderStateToString(state));

  logInfo("Guiding stopped", deviceId);

  // 发送状态事件
  sendGuidingStatusEvent();
}

void Guider::pauseGuiding() {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (state != GuiderState::GUIDING) {
    logInfo("Guiding not active, cannot pause", deviceId);
    return;
  }

  // 暂停导星
  state = GuiderState::PAUSED;
  setProperty("state", guiderStateToString(state));

  logInfo("Guiding paused", deviceId);

  // 发送状态事件
  sendGuidingStatusEvent();
}

void Guider::resumeGuiding() {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (state != GuiderState::PAUSED) {
    logInfo("Guiding not paused, cannot resume", deviceId);
    return;
  }

  // 恢复导星
  state = GuiderState::GUIDING;
  setProperty("state", guiderStateToString(state));

  logInfo("Guiding resumed", deviceId);

  // 发送状态事件
  sendGuidingStatusEvent();
}

void Guider::startCalibration() {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (state == GuiderState::CALIBRATING) {
    logInfo("Calibration already in progress", deviceId);
    return;
  }

  if (state == GuiderState::GUIDING || state == GuiderState::PAUSED) {
    logWarning("Cannot start calibration while guiding", deviceId);
    return;
  }

  // 重置校准数据
  calibration.calibrated = false;
  calibration.raAngle = 0;
  calibration.decAngle = 90;
  calibration.raRate = 0;
  calibration.decRate = 0;

  // 重置星体位置
  guideStarX = imageWidth / 2.0;
  guideStarY = imageHeight / 2.0;
  targetStarX = guideStarX;
  targetStarY = guideStarY;

  // 开始校准
  state = GuiderState::CALIBRATING;
  calibrationState = CalibrationState::NORTH_MOVING;
  setProperty("state", guiderStateToString(state));
  setProperty("calibrationState", calibrationStateToString(calibrationState));
  setProperty("calibrated", calibration.calibrated);

  logInfo("Calibration started", deviceId);

  // 发送校准开始事件
  EventMessage event("CALIBRATION_STARTED");
  sendEvent(event);
}

void Guider::cancelCalibration() {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (state != GuiderState::CALIBRATING) {
    logInfo("No calibration in progress", deviceId);
    return;
  }

  // 取消校准
  state = GuiderState::IDLE;
  calibrationState = CalibrationState::IDLE;
  setProperty("state", guiderStateToString(state));
  setProperty("calibrationState", calibrationStateToString(calibrationState));

  logInfo("Calibration cancelled", deviceId);

  // 发送校准取消事件
  EventMessage event("CALIBRATION_CANCELLED");
  sendEvent(event);
}

void Guider::dither(double amount, bool settle) {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (state != GuiderState::GUIDING) {
    logWarning("Cannot dither: not guiding", deviceId);
    return;
  }

  // 生成随机方向和大小的抖动
  std::uniform_real_distribution<> angleDist(0, 2 * M_PI);
  double angle = angleDist(rng);

  // 添加抖动到目标位置
  targetStarX += amount * cos(angle);
  targetStarY += amount * sin(angle);

  // 确保目标位置在有效范围内
  targetStarX =
      std::max(0.0, std::min(static_cast<double>(imageWidth), targetStarX));
  targetStarY =
      std::max(0.0, std::min(static_cast<double>(imageHeight), targetStarY));

  // 设置稳定参数
  isSettling = settle;
  settleFrames = 0;

  logInfo("Applied dither of " + std::to_string(amount) + " pixels", deviceId);

  // 发送抖动事件
  EventMessage event("DITHER_APPLIED");
  event.setDetails({{"amount", amount}, {"settling", settle}});
  sendEvent(event);
}

void Guider::setCalibratedPixelScale(double scale) {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (scale <= 0) {
    logWarning("Invalid pixel scale: " + std::to_string(scale), deviceId);
    return;
  }

  pixelScale = scale;
  setProperty("pixelScale", pixelScale);

  logInfo("Pixel scale set to " + std::to_string(pixelScale) + " arcsec/pixel",
          deviceId);
}

void Guider::setAggressiveness(double ra, double dec) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 验证参数
  if (ra < 0 || ra > 1) {
    logWarning("Invalid RA aggressiveness: " + std::to_string(ra), deviceId);
    return;
  }

  if (dec < 0 || dec > 1) {
    logWarning("Invalid DEC aggressiveness: " + std::to_string(dec), deviceId);
    return;
  }

  raAggressiveness = ra;
  decAggressiveness = dec;
  setProperty("raAggressiveness", raAggressiveness);
  setProperty("decAggressiveness", decAggressiveness);

  logInfo("Aggressiveness set to RA: " + std::to_string(ra) +
              ", DEC: " + std::to_string(dec),
          deviceId);
}

void Guider::setGuideRate(double ra, double dec) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 验证参数
  if (ra <= 0 || ra > 1) {
    logWarning("Invalid RA guide rate: " + std::to_string(ra), deviceId);
    return;
  }

  if (dec <= 0 || dec > 1) {
    logWarning("Invalid DEC guide rate: " + std::to_string(dec), deviceId);
    return;
  }

  raGuideRate = ra;
  decGuideRate = dec;
  setProperty("raGuideRate", raGuideRate);
  setProperty("decGuideRate", decGuideRate);

  logInfo("Guide rate set to RA: " + std::to_string(ra) +
              ", DEC: " + std::to_string(dec),
          deviceId);
}

void Guider::updateLoop() {
  logInfo("Update loop started", deviceId);

  auto lastTime = std::chrono::steady_clock::now();

  while (updateRunning) {
    // 约每50ms更新一次
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto currentTime = std::chrono::steady_clock::now();
    double elapsedSec =
        std::chrono::duration<double>(currentTime - lastTime).count();
    lastTime = currentTime;

    // 当前时间戳
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();

    // 线程安全的状态更新
    {
      std::lock_guard<std::mutex> lock(statusMutex);

      // 根据导星状态处理
      switch (state) {
      case GuiderState::IDLE:
        // 空闲状态，模拟星体轻微漂移
        if (now - lastCaptureTime > exposureTime * 1000) {
          // 拍摄新帧
          captureGuideImage();
          lastCaptureTime = now;
        }
        break;

      case GuiderState::CALIBRATING:
        // 校准状态，处理校准步骤
        if (now - lastCaptureTime > exposureTime * 1000) {
          // 拍摄新帧
          captureGuideImage();
          lastCaptureTime = now;

          // 处理校准
          processCalibration();
        }
        break;

      case GuiderState::GUIDING:
        // 导星状态，计算和应用修正
        if (now - lastCaptureTime > exposureTime * 1000) {
          // 拍摄新帧
          captureGuideImage();
          lastCaptureTime = now;
          totalFramesCapured++;

          // 计算导星修正
          calculateGuidingCorrections();

          // 更新统计数据
          double errorX = guideStarX - targetStarX;
          double errorY = guideStarY - targetStarY;
          double totalError = sqrt(errorX * errorX + errorY * errorY);

          peak = std::max(peak, totalError);

          // 更新RMS (指数移动平均)
          if (totalFramesCapured == 1) {
            rms = totalError;
          } else {
            rms = rms * 0.9 + totalError * 0.1;
          }

          setProperty("rms", rms);
          setProperty("peak", peak);

          // 处理稳定
          if (isSettling) {
            if (totalError < settleThreshold) {
              settleFrames++;
              if (settleFrames >= requiredSettleFrames) {
                isSettling = false;
                logInfo("Dither settled", deviceId);

                EventMessage event("DITHER_SETTLED");
                sendEvent(event);
              }
            } else {
              settleFrames = 0;
            }
          }

          // 每10帧发送一次状态更新
          if (totalFramesCapured % 10 == 0) {
            sendGuidingStatusEvent();
          }
        }
        break;

      case GuiderState::PAUSED:
        // 暂停状态，不计算修正但仍然捕获图像
        if (now - lastCaptureTime > exposureTime * 1000) {
          captureGuideImage();
          lastCaptureTime = now;
        }
        break;

      case GuiderState::ERROR:
        // 错误状态，不执行操作
        break;
      }
    }
  }

  logInfo("Update loop ended", deviceId);
}

void Guider::captureGuideImage() {
  // 模拟自然漂移
  std::array<double, 2> drift = simulateDrift();

  // 根据状态应用不同的位置更新
  switch (state) {
  case GuiderState::IDLE:
    // 空闲时只应用自然漂移
    guideStarX += drift[0];
    guideStarY += drift[1];
    break;

  case GuiderState::CALIBRATING:
    // 校准时应用特定方向的移动
    switch (calibrationState) {
    case CalibrationState::NORTH_MOVING:
      guideStarY -= 1.0; // 北方向移动
      break;
    case CalibrationState::SOUTH_MOVING:
      guideStarY += 1.0; // 南方向移动
      break;
    case CalibrationState::EAST_MOVING:
      guideStarX += 1.0; // 东方向移动
      break;
    case CalibrationState::WEST_MOVING:
      guideStarX -= 1.0; // 西方向移动
      break;
    default:
      // 其他校准状态不移动
      break;
    }
    break;

  case GuiderState::GUIDING:
    // 导星时应用自然漂移和修正
    guideStarX += drift[0] - lastRaCorrection * calibration.raRate / 1000.0;
    guideStarY += drift[1] - lastDecCorrection * calibration.decRate / 1000.0;
    break;

  case GuiderState::PAUSED:
    // 暂停时只应用自然漂移
    guideStarX += drift[0];
    guideStarY += drift[1];
    break;

  case GuiderState::ERROR:
    // 错误状态不变化
    break;
  }

  // 确保星体位置在图像范围内
  guideStarX =
      std::max(0.0, std::min(static_cast<double>(imageWidth - 1), guideStarX));
  guideStarY =
      std::max(0.0, std::min(static_cast<double>(imageHeight - 1), guideStarY));

  // 生成模拟图像
  imageData = generateGuideImageData();
}

std::array<double, 2> Guider::simulateDrift() {
  std::array<double, 2> result = {0, 0};

  // 模拟周期性赤道漂移 (主要是RA方向)
  double period = 60.0; // 周期秒数
  double now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count() /
               1000.0;

  // 周期性漂移
  double baseDrift = 0.2; // 基础漂移速率
  driftX = baseDrift * sin(2 * M_PI * now / period);

  // 随机扰动
  std::normal_distribution<> distX(0, 0.05);
  std::normal_distribution<> distY(0, 0.02);
  driftX += distX(rng);
  driftY += distY(rng);

  // 限制漂移范围
  driftX = std::max(-0.5, std::min(0.5, driftX));
  driftY = std::max(-0.2, std::min(0.2, driftY));

  result[0] = driftX;
  result[1] = driftY;

  return result;
}

void Guider::calculateGuidingCorrections() {
  // 计算误差
  double errorX = guideStarX - targetStarX;
  double errorY = guideStarY - targetStarY;

  // 将像素误差转换为赤道坐标系误差
  double raError = 0, decError = 0;

  if (calibration.calibrated) {
    // 将误差旋转到赤道坐标系
    double raAngleRad = calibration.raAngle * M_PI / 180.0;
    double decAngleRad = calibration.decAngle * M_PI / 180.0;

    raError = errorX * cos(raAngleRad) + errorY * sin(raAngleRad);
    decError = -errorX * sin(decAngleRad) + errorY * cos(decAngleRad);

    // 应用镜像翻转
    if (calibration.flipped) {
      decError = -decError;
    }
  } else {
    // 未校准时使用简化模型
    raError = errorX;
    decError = errorY;
  }

  // 应用修正积极性
  raCorrection = -raError * raAggressiveness * 1000 / calibration.raRate;
  decCorrection = -decError * decAggressiveness * 1000 / calibration.decRate;

  // 存储修正以便下次捕获时使用
  lastRaCorrection = raCorrection;
  lastDecCorrection = decCorrection;
}

void Guider::processCalibration() {
  // 存储校准相关位置
  static double northStartX = 0, northStartY = 0;
  static double southStartX = 0, southStartY = 0;
  static double eastStartX = 0, eastStartY = 0;
  static double westStartX = 0, westStartY = 0;

  static double northEndX = 0, northEndY = 0;
  static double southEndX = 0, southEndY = 0;
  static double eastEndX = 0, eastEndY = 0;
  static double westEndX = 0, westEndY = 0;

  static int moveFrames = 0;
  const int requiredMoveFrames = 20; // 每个方向移动的帧数

  // 处理当前校准状态
  switch (calibrationState) {
  case CalibrationState::NORTH_MOVING:
    if (moveFrames == 0) {
      northStartX = guideStarX;
      northStartY = guideStarY;
      moveFrames++;
    } else if (moveFrames < requiredMoveFrames) {
      moveFrames++;
    } else {
      northEndX = guideStarX;
      northEndY = guideStarY;
      moveFrames = 0;
      calibrationState = CalibrationState::NORTH_ANALYZING;
    }
    break;

  case CalibrationState::NORTH_ANALYZING:
    // 分析北向移动
    calibrationState = CalibrationState::SOUTH_MOVING;
    break;

  case CalibrationState::SOUTH_MOVING:
    if (moveFrames == 0) {
      southStartX = guideStarX;
      southStartY = guideStarY;
      moveFrames++;
    } else if (moveFrames < requiredMoveFrames) {
      moveFrames++;
    } else {
      southEndX = guideStarX;
      southEndY = guideStarY;
      moveFrames = 0;
      calibrationState = CalibrationState::SOUTH_ANALYZING;
    }
    break;

  case CalibrationState::SOUTH_ANALYZING:
    // 分析南向移动
    calibrationState = CalibrationState::EAST_MOVING;
    break;

  case CalibrationState::EAST_MOVING:
    if (moveFrames == 0) {
      eastStartX = guideStarX;
      eastStartY = guideStarY;
      moveFrames++;
    } else if (moveFrames < requiredMoveFrames) {
      moveFrames++;
    } else {
      eastEndX = guideStarX;
      eastEndY = guideStarY;
      moveFrames = 0;
      calibrationState = CalibrationState::EAST_ANALYZING;
    }
    break;

  case CalibrationState::EAST_ANALYZING:
    // 分析东向移动
    calibrationState = CalibrationState::WEST_MOVING;
    break;

  case CalibrationState::WEST_MOVING:
    if (moveFrames == 0) {
      westStartX = guideStarX;
      westStartY = guideStarY;
      moveFrames++;
    } else if (moveFrames < requiredMoveFrames) {
      moveFrames++;
    } else {
      westEndX = guideStarX;
      westEndY = guideStarY;
      moveFrames = 0;
      calibrationState = CalibrationState::WEST_ANALYZING;
    }
    break;

  case CalibrationState::WEST_ANALYZING:
    // 分析西向移动和完成校准

    // 计算DEC轴角度 (北-南)
    double decDX = southEndX - northEndX;
    double decDY = southEndY - northEndY;
    calibration.decAngle = atan2(decDY, decDX) * 180.0 / M_PI;

    // 计算RA轴角度 (东-西)
    double raDX = westEndX - eastEndX;
    double raDY = westEndY - eastEndY;
    calibration.raAngle = atan2(raDY, raDX) * 180.0 / M_PI;

    // 检查是否正交
    double angleDiff = fabs(
        fmod(fabs(calibration.raAngle - calibration.decAngle), 180.0) - 90.0);
    calibration.flipped = angleDiff > 20.0; // 如果差异大于20度，认为是镜像

    // 计算像素速率
    double decDistance = sqrt(decDX * decDX + decDY * decDY);
    double raDistance = sqrt(raDX * raDX + raDY * raDY);

    calibration.decRate = decDistance / requiredMoveFrames;
    calibration.raRate = raDistance / requiredMoveFrames;

    // 成功完成校准
    calibration.calibrated = true;
    calibrationState = CalibrationState::COMPLETED;

    // 更新状态
    state = GuiderState::IDLE;
    setProperty("state", guiderStateToString(state));
    setProperty("calibrationState", calibrationStateToString(calibrationState));
    setProperty("calibrated", calibration.calibrated);

    // 发送校准完成事件
    if (!currentCalibrationMessageId.empty()) {
      sendCalibrationCompletedEvent(currentCalibrationMessageId);
      currentCalibrationMessageId.clear();
    }

    logInfo("Calibration completed successfully", deviceId);
    break;

  default:
    break;
  }

  // 更新校准状态属性
  setProperty("calibrationState", calibrationStateToString(calibrationState));
}

std::vector<uint8_t> Guider::generateGuideImageData() {
  std::vector<uint8_t> data(imageWidth * imageHeight, 0);

  // 随机背景噪声
  std::normal_distribution<> noiseDist(20, 5);

  for (int y = 0; y < imageHeight; y++) {
    for (int x = 0; x < imageWidth; x++) {
      // 添加背景噪声
      data[y * imageWidth + x] =
          static_cast<uint8_t>(std::max(0.0, std::min(255.0, noiseDist(rng))));
    }
  }

  // 添加导星星体和一些随机星体
  const int numStars = 20;
  std::uniform_real_distribution<> xDist(0, imageWidth - 1);
  std::uniform_real_distribution<> yDist(0, imageHeight - 1);
  std::uniform_real_distribution<> brightnessDist(40, 200);
  std::uniform_real_distribution<> sizeDist(1.0, 3.0);

  // 添加随机星体
  for (int i = 0; i < numStars; i++) {
    double starX = xDist(rng);
    double starY = yDist(rng);
    double brightness = brightnessDist(rng);
    double size = sizeDist(rng);

    // 添加高斯形状的星体
    for (int dy = -5; dy <= 5; dy++) {
      for (int dx = -5; dx <= 5; dx++) {
        int px = static_cast<int>(starX) + dx;
        int py = static_cast<int>(starY) + dy;

        if (px >= 0 && px < imageWidth && py >= 0 && py < imageHeight) {
          double dist = sqrt(dx * dx + dy * dy);
          double value = brightness * exp(-dist * dist / (2 * size * size));
          data[py * imageWidth + px] = static_cast<uint8_t>(
              std::min(255.0, data[py * imageWidth + px] + value));
        }
      }
    }
  }

  // 添加导星星体 (比其他星体更亮)
  for (int dy = -5; dy <= 5; dy++) {
    for (int dx = -5; dx <= 5; dx++) {
      int px = static_cast<int>(guideStarX) + dx;
      int py = static_cast<int>(guideStarY) + dy;

      if (px >= 0 && px < imageWidth && py >= 0 && py < imageHeight) {
        double dist = sqrt(dx * dx + dy * dy);
        double value =
            230 * exp(-dist * dist / (2 * 2.0 * 2.0)); // 更亮、更清晰的导星星体
        data[py * imageWidth + px] = static_cast<uint8_t>(
            std::min(255.0, data[py * imageWidth + px] + value));
      }
    }
  }

  return data;
}

void Guider::sendCalibrationCompletedEvent(
    const std::string &relatedMessageId) {
  EventMessage event("CALIBRATION_COMPLETED");
  event.setRelatedMessageId(relatedMessageId);

  event.setDetails({{"success", true},
                    {"raAngle", calibration.raAngle},
                    {"decAngle", calibration.decAngle},
                    {"raRate", calibration.raRate},
                    {"decRate", calibration.decRate},
                    {"flipped", calibration.flipped}});

  sendEvent(event);
}

void Guider::sendGuidingStatusEvent() {
  EventMessage event("GUIDING_STATUS");

  // 计算导星持续时间
  int64_t guideDuration = 0;
  if (state == GuiderState::GUIDING || state == GuiderState::PAUSED) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    guideDuration = (now - guideStartTime) / 1000; // 秒
  }

  event.setDetails({{"state", guiderStateToString(state)},
                    {"rms", rms},
                    {"peak", peak},
                    {"duration", guideDuration},
                    {"framesCapured", totalFramesCapured},
                    {"settling", isSettling},
                    {"settleProgress", isSettling ? settleFrames : 0},
                    {"settleRequired", requiredSettleFrames},
                    {"raCorrection", raCorrection},
                    {"decCorrection", decCorrection},
                    {"starX", guideStarX},
                    {"starY", guideStarY},
                    {"targetX", targetStarX},
                    {"targetY", targetStarY}});

  sendEvent(event);
}

std::string Guider::guiderStateToString(GuiderState state) const {
  switch (state) {
  case GuiderState::IDLE:
    return "IDLE";
  case GuiderState::CALIBRATING:
    return "CALIBRATING";
  case GuiderState::GUIDING:
    return "GUIDING";
  case GuiderState::PAUSED:
    return "PAUSED";
  case GuiderState::ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

std::string Guider::calibrationStateToString(CalibrationState state) const {
  switch (state) {
  case CalibrationState::IDLE:
    return "IDLE";
  case CalibrationState::NORTH_MOVING:
    return "NORTH_MOVING";
  case CalibrationState::NORTH_ANALYZING:
    return "NORTH_ANALYZING";
  case CalibrationState::SOUTH_MOVING:
    return "SOUTH_MOVING";
  case CalibrationState::SOUTH_ANALYZING:
    return "SOUTH_ANALYZING";
  case CalibrationState::EAST_MOVING:
    return "EAST_MOVING";
  case CalibrationState::EAST_ANALYZING:
    return "EAST_ANALYZING";
  case CalibrationState::WEST_MOVING:
    return "WEST_MOVING";
  case CalibrationState::WEST_ANALYZING:
    return "WEST_ANALYZING";
  case CalibrationState::COMPLETED:
    return "COMPLETED";
  case CalibrationState::FAILED:
    return "FAILED";
  default:
    return "UNKNOWN";
  }
}

// 命令处理器实现
void Guider::handleStartGuidingCommand(const CommandMessage &cmd,
                                       ResponseMessage &response) {
  json params = cmd.getParameters();

  startGuiding();

  if (state == GuiderState::GUIDING) {
    response.setStatus("SUCCESS");
    response.setDetails({{"message", "Guiding started"}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "GUIDING_FAILED"},
         {"message",
          "Failed to start guiding, ensure the device is calibrated"}});
  }
}

void Guider::handleStopGuidingCommand(const CommandMessage &cmd,
                                      ResponseMessage &response) {
  stopGuiding();

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Guiding stopped"}});
}

void Guider::handlePauseGuidingCommand(const CommandMessage &cmd,
                                       ResponseMessage &response) {
  pauseGuiding();

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Guiding paused"}});
}

void Guider::handleResumeGuidingCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  resumeGuiding();

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Guiding resumed"}});
}

void Guider::handleStartCalibrationCommand(const CommandMessage &cmd,
                                           ResponseMessage &response) {
  // 存储消息ID以便完成事件
  currentCalibrationMessageId = cmd.getMessageId();

  startCalibration();

  if (state == GuiderState::CALIBRATING) {
    response.setStatus("IN_PROGRESS");
    response.setDetails(
        {{"message", "Calibration started"},
         {"calibrationState", calibrationStateToString(calibrationState)}});
  } else {
    response.setStatus("ERROR");
    response.setDetails({{"error", "CALIBRATION_FAILED"},
                         {"message", "Failed to start calibration"}});
  }
}

void Guider::handleCancelCalibrationCommand(const CommandMessage &cmd,
                                            ResponseMessage &response) {
  cancelCalibration();

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Calibration cancelled"}});
}

void Guider::handleDitherCommand(const CommandMessage &cmd,
                                 ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("amount")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'amount'"}});
    return;
  }

  double amount = params["amount"];
  bool settle = true;

  if (params.contains("settle")) {
    settle = params["settle"];
  }

  if (state != GuiderState::GUIDING) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "NOT_GUIDING"}, {"message", "Cannot dither: not guiding"}});
    return;
  }

  dither(amount, settle);

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Dither applied"},
                       {"amount", amount},
                       {"settling", settle}});
}

void Guider::handleSetParametersCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();
  json updated;

  if (params.contains("raAggressiveness")) {
    double ra = params["raAggressiveness"];
    if (ra >= 0 && ra <= 1) {
      raAggressiveness = ra;
      updated["raAggressiveness"] = ra;
    }
  }

  if (params.contains("decAggressiveness")) {
    double dec = params["decAggressiveness"];
    if (dec >= 0 && dec <= 1) {
      decAggressiveness = dec;
      updated["decAggressiveness"] = dec;
    }
  }

  if (params.contains("raGuideRate")) {
    double ra = params["raGuideRate"];
    if (ra > 0 && ra <= 1) {
      raGuideRate = ra;
      updated["raGuideRate"] = ra;
    }
  }

  if (params.contains("decGuideRate")) {
    double dec = params["decGuideRate"];
    if (dec > 0 && dec <= 1) {
      decGuideRate = dec;
      updated["decGuideRate"] = dec;
    }
  }

  if (params.contains("pixelScale")) {
    double scale = params["pixelScale"];
    if (scale > 0) {
      pixelScale = scale;
      updated["pixelScale"] = scale;
    }
  }

  if (params.contains("exposureTime")) {
    double exp = params["exposureTime"];
    if (exp > 0 && exp <= 10) {
      exposureTime = exp;
      updated["exposureTime"] = exp;
    }
  }

  // 更新属性
  for (auto it = updated.begin(); it != updated.end(); ++it) {
    setProperty(it.key(), it.value());
  }

  response.setStatus("SUCCESS");
  response.setDetails(
      {{"message", "Parameters updated"}, {"updated", updated}});
}

} // namespace astrocomm
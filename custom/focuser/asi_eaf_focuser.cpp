#include "asi_eaf_focuser.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>

namespace astrocomm {

AsiEafFocuser::AsiEafFocuser(const std::string &deviceId,
                             const std::string &hwVersion,
                             const std::string &fwVersion)
    : Focuser(deviceId, "ZWO", "ASI EAF"), hardwareVersion(hwVersion),
      firmwareVersion(fwVersion), eafStatus(EafStatus::IDLE),
      motorType(EafMotorType::STEPPER), reverseDirection(false),
      handControllerEnabled(false), highPrecisionEnabled(false),
      holdOnBootEnabled(true), motorSpeed(128), motorAcceleration(40),
      motorCurrentLimit(500), isHoming(false), homePosition(0), voltage(12.0f),
      motorCurrent(0.0f), controllerTemp(25.0f), positionIndex(0),
      rng(std::random_device()()), voltageNoise(0.0, 0.1),
      currentNoise(0.0, 0.05), tempNoise(-0.2, 0.2) {

  serialNumber = generateSerialNumber();

  std::fill(recentPositions.begin(), recentPositions.end(), 0);

  setProperty("hardwareVersion", hardwareVersion);
  setProperty("firmwareVersion", firmwareVersion);
  setProperty("serialNumber", serialNumber);
  setProperty("motorType", static_cast<int>(motorType));
  setProperty("reverse", reverseDirection);
  setProperty("handController", handControllerEnabled);
  setProperty("highPrecision", highPrecisionEnabled);
  setProperty("holdOnBoot", holdOnBootEnabled);
  setProperty("motorSpeed", motorSpeed);
  setProperty("motorAcceleration", motorAcceleration);
  setProperty("motorCurrentLimit", motorCurrentLimit);
  setProperty("deviceVoltage", voltage);
  setProperty("motorCurrent", motorCurrent);
  setProperty("controllerTemp", controllerTemp);
  setProperty("homePosition", homePosition);

  maxPosition = 100000;
  setProperty("maxPosition", maxPosition);

  registerCommandHandler("SET_REVERSE", [this](const CommandMessage &cmd,
                                               ResponseMessage &response) {
    handleSetReverse(cmd, response);
  });

  registerCommandHandler(
      "SET_HAND_CONTROLLER",
      [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSetHandController(cmd, response);
      });

  registerCommandHandler(
      "SET_HIGH_PRECISION",
      [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSetHighPrecision(cmd, response);
      });

  registerCommandHandler("SET_HOLD_ON_BOOT", [this](const CommandMessage &cmd,
                                                    ResponseMessage &response) {
    handleSetHoldOnBoot(cmd, response);
  });

  registerCommandHandler("FIND_HOME", [this](const CommandMessage &cmd,
                                             ResponseMessage &response) {
    handleFindHome(cmd, response);
  });

  registerCommandHandler("GET_MOTOR_PARAMS", [this](const CommandMessage &cmd,
                                                    ResponseMessage &response) {
    handleGetMotorParams(cmd, response);
  });

  registerCommandHandler("SET_MOTOR_PARAMS", [this](const CommandMessage &cmd,
                                                    ResponseMessage &response) {
    handleSetMotorParams(cmd, response);
  });

  registerCommandHandler("RESET_DEVICE", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
    handleResetDevice(cmd, response);
  });

  SPDLOG_INFO("ASI EAF Focuser initialized. SN: {}", serialNumber);
}

AsiEafFocuser::~AsiEafFocuser() { stop(); }

bool AsiEafFocuser::start() {
  if (!Focuser::start()) {
    return false;
  }

  // ASI EAF设备特定初始化
  initializeDevice();

  SPDLOG_INFO("ASI EAF Focuser started");
  return true;
}

void AsiEafFocuser::stop() {
  // 特定的ASI EAF停止逻辑
  setStatus(EafStatus::STOPPED);

  // 如果启用了保持，保存当前位置
  if (holdOnBootEnabled) {
    setProperty("lastPosition", position);
  }

  Focuser::stop();
  SPDLOG_INFO("ASI EAF Focuser stopped");
}

bool AsiEafFocuser::setReverse(bool reverse) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 正在移动时不允许更改方向
  if (isMoving) {
    SPDLOG_WARN("Cannot change direction while moving");
    return false;
  }

  bool oldReverse = reverseDirection;
  reverseDirection = reverse;
  setProperty("reverse", reverseDirection);

  SPDLOG_INFO("Reverse direction {} (was: {})",
              reverseDirection ? "enabled" : "disabled",
              oldReverse ? "enabled" : "disabled");

  return true;
}

bool AsiEafFocuser::setHandController(bool enable) {
  std::lock_guard<std::mutex> lock(statusMutex);

  handControllerEnabled = enable;
  setProperty("handController", handControllerEnabled);

  SPDLOG_INFO("Hand controller {}",
              handControllerEnabled ? "enabled" : "disabled");

  return true;
}

bool AsiEafFocuser::setHighPrecision(bool enable) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 如果当前正在移动，不允许更改高精度模式
  if (isMoving) {
    SPDLOG_WARN("Cannot change precision mode while moving");
    return false;
  }

  highPrecisionEnabled = enable;
  setProperty("highPrecision", highPrecisionEnabled);

  // 如果启用高精度，重置历史位置数组
  if (highPrecisionEnabled) {
    std::fill(recentPositions.begin(), recentPositions.end(), position);
  }

  // 适配步进模式（高精度使用1/16步进）
  stepMode =
      highPrecisionEnabled ? StepMode::SIXTEENTH_STEP : StepMode::QUARTER_STEP;
  setProperty("stepMode", static_cast<int>(stepMode));

  SPDLOG_INFO("High precision mode {}, using {} step mode",
              highPrecisionEnabled ? "enabled" : "disabled",
              highPrecisionEnabled ? "1/16" : "1/4");

  return true;
}

bool AsiEafFocuser::setHoldOnBoot(bool enable) {
  std::lock_guard<std::mutex> lock(statusMutex);

  holdOnBootEnabled = enable;
  setProperty("holdOnBoot", holdOnBootEnabled);

  SPDLOG_INFO("Motor hold on boot {}",
              holdOnBootEnabled ? "enabled" : "disabled");

  return true;
}

bool AsiEafFocuser::findHome() {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 如果已经在寻找零位或正在移动，返回错误
  if (isHoming || isMoving) {
    SPDLOG_WARN("Cannot start homing: device is busy");
    return false;
  }

  // 开始寻找零位
  isHoming = true;
  setStatus(EafStatus::MOVING);

  // 发送开始寻找零位事件
  EventMessage event("HOMING_STARTED");
  sendEvent(event);

  // 模拟零位寻找（在updateLoop中处理）
  // 目标是走到0位置
  targetPosition = 0;
  isMoving = true;
  setProperty("isMoving", true);

  SPDLOG_INFO("Homing procedure started");
  return true;
}

json AsiEafFocuser::getMotorParams() const {
  std::lock_guard<std::mutex> lock(statusMutex);

  return {{"speed", motorSpeed},
          {"acceleration", motorAcceleration},
          {"currentLimit", motorCurrentLimit},
          {"type", static_cast<int>(motorType)}};
}

bool AsiEafFocuser::setMotorParams(const json &params) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 正在移动时不允许更改电机参数
  if (isMoving) {
    SPDLOG_WARN("Cannot change motor parameters while moving");
    return false;
  }

  // 更新各参数
  if (params.contains("speed")) {
    uint16_t newSpeed = params["speed"];
    if (newSpeed <= 255) {
      motorSpeed = newSpeed;
      setProperty("motorSpeed", motorSpeed);

      // 更新基类的speed属性（将0-255映射到1-10）
      speed = static_cast<int>(motorSpeed / 25.5) + 1;
      if (speed > 10)
        speed = 10;
      if (speed < 1)
        speed = 1;
      setProperty("speed", speed);
    } else {
      SPDLOG_WARN("Invalid motor speed: {}", newSpeed);
      return false;
    }
  }

  if (params.contains("acceleration")) {
    uint8_t newAccel = params["acceleration"];
    motorAcceleration = newAccel;
    setProperty("motorAcceleration", motorAcceleration);
  }

  if (params.contains("currentLimit")) {
    uint16_t newLimit = params["currentLimit"];
    if (newLimit >= 100 && newLimit <= 1000) {
      motorCurrentLimit = newLimit;
      setProperty("motorCurrentLimit", motorCurrentLimit);
    } else {
      SPDLOG_WARN("Invalid current limit: {}", newLimit);
      return false;
    }
  }

  if (params.contains("type")) {
    int type = params["type"];
    if (type == 0 || type == 1) {
      motorType = static_cast<EafMotorType>(type);
      setProperty("motorType", static_cast<int>(motorType));
    } else {
      SPDLOG_WARN("Invalid motor type: {}", type);
      return false;
    }
  }

  SPDLOG_INFO("Motor parameters updated");
  return true;
}

bool AsiEafFocuser::resetDevice(bool hardReset) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 取消所有移动
  isMoving = false;
  isHoming = false;

  // 发送事件
  setStatus(EafStatus::IDLE);

  // 只有硬复位会重置位置
  if (hardReset) {
    SPDLOG_INFO("Performing hard device reset");

    // 恢复默认电机参数
    motorSpeed = 128;
    motorAcceleration = 40;
    motorCurrentLimit = 500;

    // 更新属性
    setProperty("motorSpeed", motorSpeed);
    setProperty("motorAcceleration", motorAcceleration);
    setProperty("motorCurrentLimit", motorCurrentLimit);

    // 禁用特殊功能
    highPrecisionEnabled = false;
    setProperty("highPrecision", highPrecisionEnabled);

    // 重置步进模式
    stepMode = StepMode::QUARTER_STEP;
    setProperty("stepMode", static_cast<int>(stepMode));

    // 更新速度
    speed = static_cast<int>(motorSpeed / 25.5) + 1;
    setProperty("speed", speed);

    // 将位置设置为中间
    position = maxPosition / 2;
    targetPosition = position;
    setProperty("position", position);
  } else {
    SPDLOG_INFO("Performing soft device reset");
  }

  // 发送复位完成事件
  EventMessage event("DEVICE_RESET");
  event.setDetails({{"hardReset", hardReset}});
  sendEvent(event);

  return true;
}

void AsiEafFocuser::updateLoop() {
  SPDLOG_INFO("ASI EAF update loop started");

  // 高精度模式下的位置平均计算周期
  const int highPrecisionUpdateInterval = 5;
  int highPrecisionCounter = 0;

  auto lastTime = std::chrono::steady_clock::now();
  auto lastHealthUpdate = lastTime;

  while (updateRunning) {
    // 使用标准刷新率
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto currentTime = std::chrono::steady_clock::now();
    double elapsedSec =
        std::chrono::duration<double>(currentTime - lastTime).count();
    lastTime = currentTime;

    // 每2秒更新一次健康指标
    double healthElapsedSec =
        std::chrono::duration<double>(currentTime - lastHealthUpdate).count();
    if (healthElapsedSec >= 2.0) {
      updateHealthMetrics();
      lastHealthUpdate = currentTime;
    }

    // 线程安全的状态更新
    {
      std::lock_guard<std::mutex> lock(statusMutex);

      // 更新温度相关逻辑（使用控制器温度代替环境温度）
      temperature = controllerTemp;
      setProperty("temperature", temperature);

      // 如果正在移动
      if (isMoving) {
        // 计算步进大小，考虑步进模式和电机速度
        double stepMultiplier =
            1.0 / static_cast<double>(static_cast<int>(stepMode));
        double speedFactor = motorSpeed / 128.0; // 将128作为标准速度
        int step = std::max(1, static_cast<int>(speedFactor * 20 * elapsedSec *
                                                stepMultiplier));

        // 考虑方向反转
        if (reverseDirection) {
          // 反转目标位置的效果
          int actualTarget = maxPosition - targetPosition;
          int actualPosition = maxPosition - position;

          if (std::abs(actualTarget - actualPosition) <= step) {
            // 到达反转后的目标
            actualPosition = actualTarget;
            // 转换回实际位置
            position = maxPosition - actualPosition;
            isMoving = false;

            // 处理寻找零位完成
            if (isHoming) {
              isHoming = false;
              homePosition = position;
              setProperty("homePosition", homePosition);

              // 发送寻找零位完成事件
              EventMessage event("HOMING_COMPLETED");
              event.setDetails({{"position", position}});
              sendEvent(event);

              SPDLOG_INFO("Homing completed, position set to {}", position);
            }

            setStatus(EafStatus::IDLE);
            setProperty("isMoving", false);

            // 通知等待的线程
            moveCompleteCv.notify_all();

            // 发送移动完成事件
            if (!currentMoveMessageId.empty()) {
              sendMoveCompletedEvent(currentMoveMessageId);
              currentMoveMessageId.clear();
            }

            SPDLOG_INFO("Move completed at position: {}", position);
          } else {
            // 继续移动
            if (actualPosition < actualTarget) {
              actualPosition += step;
            } else {
              actualPosition -= step;
            }
            // 转换回实际位置
            position = maxPosition - actualPosition;
          }
        } else {
          // 标准方向
          if (std::abs(targetPosition - position) <= step) {
            // 到达目标位置
            position = targetPosition;
            isMoving = false;

            // 处理寻找零位完成
            if (isHoming) {
              isHoming = false;
              homePosition = position;
              setProperty("homePosition", homePosition);

              // 发送寻找零位完成事件
              EventMessage event("HOMING_COMPLETED");
              event.setDetails({{"position", position}});
              sendEvent(event);

              SPDLOG_INFO("Homing completed, position set to {}", position);
            }

            setStatus(EafStatus::IDLE);
            setProperty("isMoving", false);

            // 通知等待的线程
            moveCompleteCv.notify_all();

            // 发送移动完成事件
            if (!currentMoveMessageId.empty()) {
              sendMoveCompletedEvent(currentMoveMessageId);
              currentMoveMessageId.clear();
            }

            SPDLOG_INFO("Move completed at position: {}", position);
          } else {
            // 继续移动
            if (position < targetPosition) {
              position += step;
            } else {
              position -= step;
            }
          }
        }

        // 模拟移动时电机电流的变化
        motorCurrent = 0.3f + (0.5f * motorSpeed / 255.0f) + currentNoise(rng);
        if (motorCurrent > motorCurrentLimit / 1000.0f) {
          motorCurrent = motorCurrentLimit / 1000.0f;
        }
        setProperty("motorCurrent", motorCurrent);

        // 更新位置属性 - 高精度模式下使用均值过滤
        if (highPrecisionEnabled) {
          // 存储当前位置
          recentPositions[positionIndex] = position;
          positionIndex = (positionIndex + 1) % recentPositions.size();

          highPrecisionCounter++;
          if (highPrecisionCounter >= highPrecisionUpdateInterval) {
            highPrecisionCounter = 0;

            // 计算平均位置
            int avgPosition = getAveragedPosition();
            setProperty("position", avgPosition);
          }
        } else {
          // 标准模式直接更新
          setProperty("position", position);
        }
      } else if (tempCompEnabled) {
        // 应用温度补偿
        int compensatedPosition = applyTemperatureCompensation(position);

        if (compensatedPosition != position) {
          // 温度补偿需要移动
          int originalPosition = position;
          position = compensatedPosition;

          // 高精度模式下也需要更新历史数组
          if (highPrecisionEnabled) {
            std::fill(recentPositions.begin(), recentPositions.end(), position);
          }

          setProperty("position", position);

          SPDLOG_INFO(
              "Temperature compensation adjusted position from {} to {}",
              originalPosition, position);
        }
      } else {
        // 空闲状态，电机电流低
        motorCurrent = 0.1f + currentNoise(rng);
        if (motorCurrent < 0)
          motorCurrent = 0;
        setProperty("motorCurrent", motorCurrent);
      }

      // 手控制器移动逻辑
      if (handControllerEnabled && !isMoving && !isHoming) {
        // 模拟随机的手控制器操作
        static int handControlCounter = 0;
        static bool handMoving = false;

        if (handMoving) {
          handControlCounter--;
          if (handControlCounter <= 0) {
            handMoving = false;
          } else {
            // 随机方向移动
            position += (handControlCounter % 2) ? 1 : -1;
            // 限制在有效范围内
            position = std::max(0, std::min(maxPosition, position));
            setProperty("position", position);
          }
        } else {
          // 小概率触发手控制移动
          std::uniform_real_distribution<> chance(0, 100);
          if (chance(rng) < 0.2) { // 0.2%几率
            handMoving = true;
            handControlCounter = 20 + (rng() % 50); // 移动10-60步

            SPDLOG_INFO("Hand controller movement triggered");
          }
        }
      }
    }
  }

  SPDLOG_INFO("ASI EAF update loop ended");
}

int AsiEafFocuser::applyTemperatureCompensation(int currentPosition) {
  // ASI EAF使用不同的温度补偿算法
  static double referenceTemp = temperature;

  // 只有当温差超过阈值才进行补偿
  if (std::abs(temperature - referenceTemp) < 0.5) {
    return currentPosition;
  }

  // 记录新的参考温度
  double oldReferenceTemp = referenceTemp;
  referenceTemp = temperature;

  // 计算温差
  double tempDiff = temperature - oldReferenceTemp;

  // 应用非线性补偿模型 (ASI EAF的特性)
  // 温度升高时收缩，温度降低时扩张，并且变化是非线性的
  double factor = tempDiff > 0 ? 0.8 : 1.2; // 不同方向有不同系数
  int steps = static_cast<int>(tempDiff * tempCompCoefficient * factor);

  // 考虑方向反转
  if (reverseDirection) {
    steps = -steps;
  }

  // 返回补偿后的位置，确保在有效范围内
  return std::max(0, std::min(maxPosition, currentPosition + steps));
}

void AsiEafFocuser::handleSetReverse(const CommandMessage &cmd,
                                     ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("reverse")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'reverse'"}});
    return;
  }

  bool reverse = params["reverse"];
  bool success = setReverse(reverse);

  response.setStatus(success ? "SUCCESS" : "ERROR");
  if (success) {
    response.setDetails({{"reverse", reverseDirection}});
  } else {
    response.setDetails({{"error", "OPERATION_FAILED"},
                         {"message", "Cannot change direction while moving"}});
  }
}

void AsiEafFocuser::handleSetHandController(const CommandMessage &cmd,
                                            ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("enable")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'enable'"}});
    return;
  }

  bool enable = params["enable"];
  bool success = setHandController(enable);

  response.setStatus(success ? "SUCCESS" : "ERROR");
  response.setDetails({{"handController", handControllerEnabled}});
}

void AsiEafFocuser::handleSetHighPrecision(const CommandMessage &cmd,
                                           ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("enable")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'enable'"}});
    return;
  }

  bool enable = params["enable"];
  bool success = setHighPrecision(enable);

  response.setStatus(success ? "SUCCESS" : "ERROR");
  if (success) {
    response.setDetails({{"highPrecision", highPrecisionEnabled},
                         {"stepMode", static_cast<int>(stepMode)}});
  } else {
    response.setDetails(
        {{"error", "OPERATION_FAILED"},
         {"message", "Cannot change precision mode while moving"}});
  }
}

void AsiEafFocuser::handleSetHoldOnBoot(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("enable")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'enable'"}});
    return;
  }

  bool enable = params["enable"];
  bool success = setHoldOnBoot(enable);

  response.setStatus(success ? "SUCCESS" : "ERROR");
  response.setDetails({{"holdOnBoot", holdOnBootEnabled}});
}

void AsiEafFocuser::handleFindHome(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  bool success = findHome();

  response.setStatus(success ? "IN_PROGRESS" : "ERROR");
  if (success) {
    response.setDetails({{"message", "Homing procedure started"}});
  } else {
    response.setDetails({{"error", "DEVICE_BUSY"},
                         {"message", "Device is busy, cannot start homing"}});
  }
}

void AsiEafFocuser::handleGetMotorParams(const CommandMessage &cmd,
                                         ResponseMessage &response) {
  json params = getMotorParams();

  response.setStatus("SUCCESS");
  response.setDetails(params);
}

void AsiEafFocuser::handleSetMotorParams(const CommandMessage &cmd,
                                         ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("speed") && !params.contains("acceleration") &&
      !params.contains("currentLimit") && !params.contains("type")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "No valid motor parameters provided"}});
    return;
  }

  bool success = setMotorParams(params);

  response.setStatus(success ? "SUCCESS" : "ERROR");
  if (success) {
    response.setDetails(getMotorParams());
  } else {
    response.setDetails(
        {{"error", "INVALID_PARAMETERS"},
         {"message", "Invalid motor parameters or device is busy"}});
  }
}

void AsiEafFocuser::handleResetDevice(const CommandMessage &cmd,
                                      ResponseMessage &response) {
  json params = cmd.getParameters();

  bool hardReset =
      params.contains("hardReset") ? params["hardReset"].get<bool>() : false;
  bool success = resetDevice(hardReset);

  response.setStatus(success ? "SUCCESS" : "ERROR");
  response.setDetails({{"hardReset", hardReset},
                       {"message", success ? "Device reset successful"
                                           : "Device reset failed"}});
}

void AsiEafFocuser::initializeDevice() {
  // 初始化设备特定行为
  SPDLOG_INFO("Initializing ASI EAF device");

  // 设置初始状态
  setStatus(EafStatus::IDLE);

  // 如果启用了保持，尝试恢复上次位置
  if (holdOnBootEnabled) {
    json lastPosJson = getProperty("lastPosition");
    if (!lastPosJson.is_null()) {
      int lastPos = lastPosJson.get<int>();
      if (lastPos >= 0 && lastPos <= maxPosition) {
        position = lastPos;
        targetPosition = position;
        setProperty("position", position);
        SPDLOG_INFO("Restored last position: {}", position);
      }
    }
  }

  // 更新电机曲线参数
  updateDeviceStatus();
}

void AsiEafFocuser::updateDeviceStatus() {
  // 更新电机和设备状态
  std::lock_guard<std::mutex> lock(statusMutex);

  // 根据电机类型调整参数
  if (motorType == EafMotorType::DC_MOTOR) {
    // DC马达的调整
    maxPosition = 50000; // DC马达行程通常较小
    setProperty("maxPosition", maxPosition);
  } else {
    // 步进电机的调整
    maxPosition = 100000; // 步进马达可以有更大行程
    setProperty("maxPosition", maxPosition);
  }

  // 发送设备状态更新事件
  EventMessage event("DEVICE_STATUS_UPDATED");
  event.setDetails({{"status", static_cast<int>(eafStatus)},
                    {"motorType", static_cast<int>(motorType)},
                    {"maxPosition", maxPosition}});
  sendEvent(event);
}

void AsiEafFocuser::updateHealthMetrics() {
  // 更新设备健康度量
  std::lock_guard<std::mutex> lock(statusMutex);

  // 更新电压
  voltage = 12.0f + voltageNoise(rng);
  setProperty("deviceVoltage", voltage);

  // 控制器温度缓慢变化
  controllerTemp += tempNoise(rng);
  // 确保温度在合理范围内
  if (controllerTemp < 0.0f)
    controllerTemp = 0.0f;
  if (controllerTemp > 60.0f)
    controllerTemp = 60.0f;
  setProperty("controllerTemp", controllerTemp);

  // 检测电压过低警告
  if (voltage < 11.0f) {
    SPDLOG_WARN("Low voltage detected: {}V", voltage);

    // 发送低电压警告事件
    EventMessage event("VOLTAGE_WARNING");
    event.setDetails({{"voltage", voltage}, {"minVoltage", 11.0f}});
    sendEvent(event);
  }

  // Detect temperature warnings
  if (controllerTemp > 50.0f) {
    SPDLOG_WARN("High temperature detected: {}°C", controllerTemp);

    // Send high temperature warning event
    EventMessage event("TEMPERATURE_WARNING");
    event.setDetails(
        {{"temperature", controllerTemp}, {"maxTemperature", 50.0f}});
    sendEvent(event);
  }
}

std::string AsiEafFocuser::generateSerialNumber() {
  // Generate simulation serial number (ASI + 8 digits)
  std::stringstream ss;
  ss << "ASI" << std::setfill('0') << std::setw(2) << (1 + (rng() % 99));
  ss << std::setfill('0') << std::setw(6) << (rng() % 1000000);
  return ss.str();
}

void AsiEafFocuser::setStatus(EafStatus status) {
  eafStatus = status;
  setProperty("status", static_cast<int>(eafStatus));

  // 发送状态变更事件
  EventMessage event("STATUS_CHANGED");
  event.setDetails({{"status", static_cast<int>(eafStatus)},
                    {"statusText", [this]() -> std::string {
                       switch (eafStatus) {
                       case EafStatus::IDLE:
                         return "Idle";
                       case EafStatus::MOVING:
                         return "Moving";
                       case EafStatus::ERROR:
                         return "Error";
                       case EafStatus::STOPPED:
                         return "Stopped";
                       default:
                         return "Unknown";
                       }
                     }()}});
  sendEvent(event);
}

int AsiEafFocuser::getAveragedPosition() const {
  // 高精度模式的位置平均计算
  long sum = 0;
  for (int pos : recentPositions) {
    sum += pos;
  }
  return static_cast<int>(sum / recentPositions.size());
}

} // namespace astrocomm
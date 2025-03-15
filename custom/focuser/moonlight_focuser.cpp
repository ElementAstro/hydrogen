#include "moonlight_focuser.h"

#include <algorithm>
#include <cmath>
#include <random>

#include <spdlog/spdlog.h>

namespace astrocomm {

MoonlightFocuser::MoonlightFocuser(const std::string &deviceId,
                                   const std::string &model)
    : Focuser(deviceId, "Moonlight", model),
      currentLimit(80), // 默认电流限制为80%
      driveMode(MoonlightDriveMode::NORMAL), knobControlEnabled(false),
      motorPowerDownEnabled(true), motorTemperature(25.0),
      controllerVoltage(12.0), motorCurrent(0.0), isCalibrated(true) {

  // 设置Moonlight特有的属性
  setProperty("currentLimit", currentLimit);
  setProperty("driveMode", static_cast<int>(driveMode));
  setProperty("knobControlEnabled", knobControlEnabled);
  setProperty("motorPowerDownEnabled", motorPowerDownEnabled);
  setProperty("motorTemperature", motorTemperature);
  setProperty("controllerVoltage", controllerVoltage);
  setProperty("motorCurrent", motorCurrent);
  setProperty("isCalibrated", isCalibrated);

  // 添加Moonlight特有的能力
  capabilities.push_back("CURRENT_LIMITING");
  capabilities.push_back("MOTOR_MODES");
  capabilities.push_back("TEMPERATURE_MONITORING");
  capabilities.push_back("KNOB_CONTROL");

  // 修改默认设置以匹配Moonlight调焦器
  maxPosition = 20000; // Moonlight通常有更大的行程范围
  backlash = 20;       // 默认反向间隙补偿
  stepMode = StepMode::SIXTEENTH_STEP; // Moonlight通常使用1/16微步

  // 更新基类的相应属性
  setProperty("maxPosition", maxPosition);
  setProperty("backlash", backlash);
  setProperty("stepMode", static_cast<int>(stepMode));

  // 注册Moonlight特有的命令处理器
  registerCommandHandler(
      "SET_CURRENT_LIMIT",
      [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSetCurrentLimitCommand(cmd, response);
      });

  registerCommandHandler("SET_DRIVE_MODE", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleSetDriveModeCommand(cmd, response);
  });

  registerCommandHandler("SET_KNOB_CONTROL", [this](const CommandMessage &cmd,
                                                    ResponseMessage &response) {
    handleSetKnobControlCommand(cmd, response);
  });

  registerCommandHandler(
      "SET_MOTOR_POWER_DOWN",
      [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSetMotorPowerDownCommand(cmd, response);
      });

  registerCommandHandler("CALIBRATE", [this](const CommandMessage &cmd,
                                             ResponseMessage &response) {
    handleCalibrateCommand(cmd, response);
  });

  registerCommandHandler("RESET_DEFAULTS", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleResetCommand(cmd, response);
  });

  SPDLOG_INFO("Moonlight Focuser initialized: {}", model);
}

MoonlightFocuser::~MoonlightFocuser() {
  // 确保电机断电，防止过热
  motorCurrent = 0.0;
  setProperty("motorCurrent", motorCurrent);

  SPDLOG_INFO("Moonlight Focuser shutdown completed");
}

bool MoonlightFocuser::setCurrentLimit(int limit) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 验证范围
  if (limit < 10 || limit > 100) {
    SPDLOG_WARN("Invalid current limit: {} (must be between 10 and 100)",
                limit);
    return false;
  }

  currentLimit = limit;
  setProperty("currentLimit", currentLimit);

  SPDLOG_INFO("Current limit set to {}%", currentLimit);
  return true;
}

bool MoonlightFocuser::setDriveMode(MoonlightDriveMode mode) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 验证模式
  int modeVal = static_cast<int>(mode);
  if (modeVal < 0 || modeVal > 3) {
    SPDLOG_WARN("Invalid drive mode: {}", modeVal);
    return false;
  }

  driveMode = mode;
  setProperty("driveMode", static_cast<int>(driveMode));

  // 根据驱动模式调整其他参数
  switch (driveMode) {
  case MoonlightDriveMode::NORMAL:
    // 正常模式 - 默认设置
    break;

  case MoonlightDriveMode::HIGH_TORQUE:
    // 高扭矩模式 - 增加电流限制
    currentLimit = std::min(currentLimit + 20, 100);
    setProperty("currentLimit", currentLimit);
    break;

  case MoonlightDriveMode::SILENT:
    // 静音模式 - 降低速度，使用更小的微步模式
    setSpeed(std::max(1, speed - 2));
    setStepMode(StepMode::THIRTYSECOND_STEP);
    break;

  case MoonlightDriveMode::POWER_SAVE:
    // 省电模式 - 启用电机断电，降低电流限制
    motorPowerDownEnabled = true;
    setProperty("motorPowerDownEnabled", motorPowerDownEnabled);
    currentLimit = std::max(10, currentLimit - 20);
    setProperty("currentLimit", currentLimit);
    break;
  }

  std::string modeName;
  switch (driveMode) {
  case MoonlightDriveMode::NORMAL:
    modeName = "Normal";
    break;
  case MoonlightDriveMode::HIGH_TORQUE:
    modeName = "High Torque";
    break;
  case MoonlightDriveMode::SILENT:
    modeName = "Silent";
    break;
  case MoonlightDriveMode::POWER_SAVE:
    modeName = "Power Save";
    break;
  default:
    modeName = "Unknown";
  }

  SPDLOG_INFO("Drive mode set to {}", modeName);
  return true;
}

bool MoonlightFocuser::setKnobControl(bool enabled) {
  std::lock_guard<std::mutex> lock(statusMutex);

  knobControlEnabled = enabled;
  setProperty("knobControlEnabled", knobControlEnabled);

  SPDLOG_INFO("Knob control {}", enabled ? "enabled" : "disabled");
  return true;
}

bool MoonlightFocuser::setMotorPowerDown(bool enabled) {
  std::lock_guard<std::mutex> lock(statusMutex);

  motorPowerDownEnabled = enabled;
  setProperty("motorPowerDownEnabled", motorPowerDownEnabled);

  SPDLOG_INFO("Motor power down {}", enabled ? "enabled" : "disabled");
  return true;
}

double MoonlightFocuser::getMotorTemperature() const {
  std::lock_guard<std::mutex> lock(statusMutex);
  return motorTemperature;
}

bool MoonlightFocuser::calibrate() {
  // 已经在移动中不能校准
  if (isMoving) {
    SPDLOG_WARN("Cannot calibrate: focuser is moving");
    return false;
  }

  SPDLOG_INFO("Starting calibration...");

  // 发送校准开始事件
  EventMessage event("CALIBRATION_STARTED");
  sendEvent(event);

  // 模拟校准过程
  isCalibrated = false;
  setProperty("isCalibrated", isCalibrated);

  // 移动到最小位置
  moveAbsolute(0, true);

  // 然后移动到最大位置以测量行程
  moveAbsolute(maxPosition, true);

  // 最后回到中间位置
  moveAbsolute(maxPosition / 2, true);

  // 校准完成
  isCalibrated = true;
  setProperty("isCalibrated", isCalibrated);

  // 发送校准完成事件
  EventMessage completeEvent("CALIBRATION_COMPLETED");
  completeEvent.setDetails({{"success", true}, {"maxPosition", maxPosition}});
  sendEvent(completeEvent);

  SPDLOG_INFO("Calibration completed successfully");
  return true;
}

bool MoonlightFocuser::resetToFactoryDefaults() {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 重置到默认设置
  currentLimit = 80;
  driveMode = MoonlightDriveMode::NORMAL;
  knobControlEnabled = false;
  motorPowerDownEnabled = true;
  backlash = 20;
  stepMode = StepMode::SIXTEENTH_STEP;
  speed = 5;
  tempCompEnabled = false;
  tempCompCoefficient = 0.0;

  // 更新所有属性
  setProperty("currentLimit", currentLimit);
  setProperty("driveMode", static_cast<int>(driveMode));
  setProperty("knobControlEnabled", knobControlEnabled);
  setProperty("motorPowerDownEnabled", motorPowerDownEnabled);
  setProperty("backlash", backlash);
  setProperty("stepMode", static_cast<int>(stepMode));
  setProperty("speed", speed);
  setProperty("temperatureCompensation", tempCompEnabled);
  setProperty("tempCompCoefficient", tempCompCoefficient);

  SPDLOG_INFO("Reset to factory defaults completed");

  // 发送重置完成事件
  EventMessage event("FACTORY_RESET_COMPLETED");
  sendEvent(event);

  return true;
}

void MoonlightFocuser::updateLoop() {
  SPDLOG_INFO("Moonlight focuser update loop started");

  // 随机数生成器用于模拟温度和电压变化
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> tempDist(-0.2, 0.2);
  std::uniform_real_distribution<> voltDist(-0.1, 0.1);

  auto lastTime = std::chrono::steady_clock::now();

  // 上次发送电机参数的时间
  auto lastMotorParamsUpdate = std::chrono::steady_clock::now();

  while (updateRunning) {
    // 约每50ms更新一次
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto currentTime = std::chrono::steady_clock::now();
    double elapsedSec =
        std::chrono::duration<double>(currentTime - lastTime).count();
    lastTime = currentTime;

    // 首先调用基类的更新循环处理标准功能
    Focuser::updateLoop();

    // Moonlight特有的更新逻辑
    {
      std::lock_guard<std::mutex> lock(statusMutex);

      // 模拟电机温度变化（移动时温度上升，静止时慢慢降低）
      if (isMoving) {
        // 移动时温度上升，与电流成正比
        motorTemperature += (0.05 * (motorCurrent / 100.0) * elapsedSec);
      } else {
        // 静止时温度缓慢回归到环境温度
        motorTemperature +=
            ((ambientTemperature - motorTemperature) * 0.01 * elapsedSec);
      }
      motorTemperature += tempDist(gen) * elapsedSec;

      // 模拟控制器电压变化
      controllerVoltage += voltDist(gen) * elapsedSec;
      controllerVoltage = std::max(11.0, std::min(13.0, controllerVoltage));

      // 更新电机电流
      if (isMoving) {
        // 移动时电流较高，根据速度和负载变化
        motorCurrent = (currentLimit / 100.0) * (0.5 + 0.5 * (speed / 10.0));
      } else if (motorPowerDownEnabled) {
        // 电机断电功能启用，且停止移动3秒后断电
        auto idleTime = std::chrono::duration_cast<std::chrono::seconds>(
                            currentTime - lastMotorParamsUpdate)
                            .count();

        if (idleTime > 3) {
          motorCurrent = 0.0;
        } else {
          motorCurrent *= 0.95; // 逐渐减小
        }
      } else {
        // 保持低电流以保持位置
        motorCurrent = (currentLimit / 100.0) * 0.2;
      }

      // 检查电机参数（过热、过载等）
      checkMotorParameters();

      // 模拟旋钮控制
      if (knobControlEnabled) {
        simulateKnobControl();
      }

      // 每秒更新一次电机参数属性（减少不必要的更新）
      auto timeSinceLastUpdate =
          std::chrono::duration_cast<std::chrono::seconds>(
              currentTime - lastMotorParamsUpdate)
              .count();

      if (timeSinceLastUpdate >= 1) {
        setProperty("motorTemperature", motorTemperature);
        setProperty("controllerVoltage", controllerVoltage);
        setProperty("motorCurrent", motorCurrent);

        lastMotorParamsUpdate = currentTime;
      }
    }
  }

  SPDLOG_INFO("Moonlight focuser update loop ended");
}

int MoonlightFocuser::applyTemperatureCompensation(int currentPosition) {
  // Moonlight调焦器有自己特定的温度补偿算法
  // 这里我们使用一个更复杂的基于温度差和系数的公式

  static double lastTemp = temperature;
  double tempDiff = temperature - lastTemp;

  // 如果温差太小，不进行补偿
  if (std::abs(tempDiff) < 0.1) {
    return currentPosition;
  }

  lastTemp = temperature;

  // Moonlight补偿公式：步数 = 温差 × 系数 × (当前位置 / 最大位置)
  // 这使补偿与位置成比例 - 在较大位置时补偿更大
  int steps =
      static_cast<int>(tempDiff * tempCompCoefficient *
                       (static_cast<double>(currentPosition) / maxPosition));

  // 返回补偿后的位置，确保在有效范围内
  return std::max(0, std::min(maxPosition, currentPosition + steps));
}

void MoonlightFocuser::checkMotorParameters() {
  // 检查电机温度是否过高
  if (motorTemperature > 60.0) {
    // 电机过热
    if (isMoving) {
      // 中止移动
      SPDLOG_WARN("Motor overheating detected, aborting movement");
      abort();

      // 发送过热事件
      EventMessage event("MOTOR_OVERHEATING");
      event.setDetails({{"temperature", motorTemperature}, {"limit", 60.0}});
      sendEvent(event);
    }

    // 降低电流限制
    int oldLimit = currentLimit;
    currentLimit = std::max(10, currentLimit - 20);

    if (oldLimit != currentLimit) {
      setProperty("currentLimit", currentLimit);

      SPDLOG_WARN("Reducing current limit to {}% due to high temperature",
                  currentLimit);
    }
  }

  // 检查控制器电压是否太低
  if (controllerVoltage < 11.5) {
    // 电压过低
    SPDLOG_WARN("Low voltage detected: {:.2f}V", controllerVoltage);

    // 发送低电压事件
    EventMessage event("LOW_VOLTAGE");
    event.setDetails(
        {{"voltage", controllerVoltage}, {"minimumVoltage", 11.5}});
    sendEvent(event);
  }
}

void MoonlightFocuser::simulateKnobControl() {
  // 只有在启用旋钮控制且未移动时才模拟
  if (!knobControlEnabled || isMoving) {
    return;
  }

  // 随机生成旋钮事件
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dist(0, 1000);

  // 0.5%的概率生成旋钮事件
  if (dist(gen) < 5) {
    // 50%概率向内，50%概率向外
    bool directionOut = (dist(gen) % 2 == 0);

    // 生成1-10步的移动
    int steps = (dist(gen) % 10) + 1;
    if (!directionOut)
      steps = -steps;

    // 模拟旋钮移动
    int newPosition = std::max(0, std::min(maxPosition, position + steps));

    if (newPosition != position) {
      position = newPosition;
      setProperty("position", position);

      SPDLOG_INFO("Knob control: moved {} steps {} to position {}",
                  std::abs(steps), (steps > 0 ? "out" : "in"), position);

      // 发送旋钮控制事件
      EventMessage event("KNOB_CONTROL");
      event.setDetails({{"steps", steps}, {"position", position}});
      sendEvent(event);
    }
  }
}

// 命令处理器实现
void MoonlightFocuser::handleSetCurrentLimitCommand(const CommandMessage &cmd,
                                                    ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("limit")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'limit'"}});
    return;
  }

  int limit = params["limit"];
  bool success = setCurrentLimit(limit);

  response.setStatus(success ? "SUCCESS" : "ERROR");
  if (success) {
    response.setDetails({{"currentLimit", currentLimit}});
  } else {
    response.setDetails(
        {{"error", "INVALID_VALUE"},
         {"message", "Current limit must be between 10 and 100"}});
  }
}

void MoonlightFocuser::handleSetDriveModeCommand(const CommandMessage &cmd,
                                                 ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("mode")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'mode'"}});
    return;
  }

  int modeVal = params["mode"];
  bool success = setDriveMode(static_cast<MoonlightDriveMode>(modeVal));

  response.setStatus(success ? "SUCCESS" : "ERROR");
  if (success) {
    std::string modeName;
    switch (driveMode) {
    case MoonlightDriveMode::NORMAL:
      modeName = "Normal";
      break;
    case MoonlightDriveMode::HIGH_TORQUE:
      modeName = "High Torque";
      break;
    case MoonlightDriveMode::SILENT:
      modeName = "Silent";
      break;
    case MoonlightDriveMode::POWER_SAVE:
      modeName = "Power Save";
      break;
    default:
      modeName = "Unknown";
    }

    response.setDetails(
        {{"driveMode", static_cast<int>(driveMode)}, {"modeName", modeName}});
  } else {
    response.setDetails(
        {{"error", "INVALID_VALUE"}, {"message", "Invalid drive mode"}});
  }
}

void MoonlightFocuser::handleSetKnobControlCommand(const CommandMessage &cmd,
                                                   ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("enabled")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'enabled'"}});
    return;
  }

  bool enabled = params["enabled"];
  bool success = setKnobControl(enabled);

  response.setStatus("SUCCESS");
  response.setDetails({{"knobControlEnabled", knobControlEnabled}});
}

void MoonlightFocuser::handleSetMotorPowerDownCommand(
    const CommandMessage &cmd, ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("enabled")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'enabled'"}});
    return;
  }

  bool enabled = params["enabled"];
  bool success = setMotorPowerDown(enabled);

  response.setStatus("SUCCESS");
  response.setDetails({{"motorPowerDownEnabled", motorPowerDownEnabled}});
}

void MoonlightFocuser::handleCalibrateCommand(const CommandMessage &cmd,
                                              ResponseMessage &response) {
  // 开始校准过程
  bool success = calibrate();

  if (success) {
    response.setStatus("IN_PROGRESS");
    response.setDetails({{"message", "Calibration started"}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "CALIBRATION_FAILED"},
         {"message", "Cannot start calibration while focuser is moving"}});
  }
}

void MoonlightFocuser::handleResetCommand(const CommandMessage &cmd,
                                          ResponseMessage &response) {
  bool success = resetToFactoryDefaults();

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Reset to factory defaults completed"}});
}

} // namespace astrocomm
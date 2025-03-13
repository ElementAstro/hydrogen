#include "device/focuser.h"
#include "common/logger.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace astrocomm {

Focuser::Focuser(const std::string &deviceId, const std::string &manufacturer,
                 const std::string &model)
    : DeviceBase(deviceId, "FOCUSER", manufacturer, model), position(5000),
      targetPosition(5000), maxPosition(10000), speed(5), backlash(0),
      tempCompEnabled(false), tempCompCoefficient(0.0), temperature(20.0),
      isMoving(false), movingDirection(true), ambientTemperature(20.0),
      temperatureDrift(0.0), updateRunning(false) {

  // 初始化属性
  setProperty("position", position);
  setProperty("maxPosition", maxPosition);
  setProperty("speed", speed);
  setProperty("backlash", backlash);
  setProperty("temperatureCompensation", tempCompEnabled);
  setProperty("tempCompCoefficient", tempCompCoefficient);
  setProperty("temperature", temperature);
  setProperty("isMoving", false);
  setProperty("connected", false);
  setProperty("absolutePosition", true); // 支持绝对位置

  // 设置能力
  capabilities = {"ABSOLUTE_POSITION", "RELATIVE_POSITION",
                  "TEMPERATURE_COMPENSATION", "BACKLASH_COMPENSATION"};

  // 注册命令处理器
  registerCommandHandler("MOVE_ABSOLUTE", [this](const CommandMessage &cmd,
                                                 ResponseMessage &response) {
    handleMoveAbsoluteCommand(cmd, response);
  });

  registerCommandHandler("MOVE_RELATIVE", [this](const CommandMessage &cmd,
                                                 ResponseMessage &response) {
    handleMoveRelativeCommand(cmd, response);
  });

  registerCommandHandler(
      "ABORT", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleAbortCommand(cmd, response);
      });

  registerCommandHandler("SET_MAX_POSITION", [this](const CommandMessage &cmd,
                                                    ResponseMessage &response) {
    handleSetMaxPositionCommand(cmd, response);
  });

  registerCommandHandler("SET_SPEED", [this](const CommandMessage &cmd,
                                             ResponseMessage &response) {
    handleSetSpeedCommand(cmd, response);
  });

  registerCommandHandler("SET_BACKLASH", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
    handleSetBacklashCommand(cmd, response);
  });

  registerCommandHandler(
      "SET_TEMPERATURE_COMPENSATION",
      [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSetTempCompCommand(cmd, response);
      });

  logInfo("Focuser device initialized", deviceId);
}

Focuser::~Focuser() { stop(); }

bool Focuser::start() {
  if (!DeviceBase::start()) {
    return false;
  }

  // 启动更新线程
  updateRunning = true;
  updateThread = std::thread(&Focuser::updateLoop, this);

  setProperty("connected", true);
  logInfo("Focuser started", deviceId);
  return true;
}

void Focuser::stop() {
  // 停止更新线程
  updateRunning = false;
  if (updateThread.joinable()) {
    updateThread.join();
  }

  setProperty("connected", false);
  DeviceBase::stop();
  logInfo("Focuser stopped", deviceId);
}

void Focuser::moveAbsolute(int newPosition) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 验证位置范围
  if (newPosition < 0 || newPosition > maxPosition) {
    logWarning("Invalid position value: " + std::to_string(newPosition),
               deviceId);
    return;
  }

  // 如果没有变化，则不移动
  if (newPosition == position && !isMoving) {
    logInfo("Already at requested position: " + std::to_string(position),
            deviceId);
    return;
  }

  // 设置移动方向和目标
  movingDirection = (newPosition > position);

  // 应用反向间隙补偿
  if (backlash > 0 && ((targetPosition > position && newPosition < position) ||
                       (targetPosition < position && newPosition > position))) {
    // 方向改变，添加反向间隙
    if (movingDirection) {
      newPosition += backlash;
    } else {
      newPosition -= backlash;
    }
    // 确保在有效范围内
    newPosition = std::max(0, std::min(maxPosition, newPosition));
  }

  targetPosition = newPosition;
  isMoving = true;
  setProperty("isMoving", true);

  logInfo("Starting absolute move to position: " +
              std::to_string(targetPosition),
          deviceId);
}

void Focuser::moveRelative(int steps) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 计算新位置
  int newPosition = position + steps;

  // 确保在有效范围内
  newPosition = std::max(0, std::min(maxPosition, newPosition));

  // 调用绝对移动
  moveAbsolute(newPosition);

  logInfo("Starting relative move by steps: " + std::to_string(steps),
          deviceId);
}

void Focuser::abort() {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (!isMoving) {
    logInfo("No movement to abort", deviceId);
    return;
  }

  isMoving = false;
  targetPosition = position; // 停在当前位置
  currentMoveMessageId.clear();
  setProperty("isMoving", false);

  logInfo("Movement aborted", deviceId);

  // 发送中止事件
  EventMessage event("ABORTED");
  event.setDetails({{"position", position}});
  sendEvent(event);
}

void Focuser::setMaxPosition(int maxPos) {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (maxPos <= 0) {
    logWarning("Invalid max position: " + std::to_string(maxPos), deviceId);
    return;
  }

  maxPosition = maxPos;
  setProperty("maxPosition", maxPosition);

  logInfo("Max position set to " + std::to_string(maxPosition), deviceId);
}

void Focuser::setSpeed(int speedValue) {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (speedValue < 1 || speedValue > 10) {
    logWarning("Invalid speed value: " + std::to_string(speedValue), deviceId);
    return;
  }

  speed = speedValue;
  setProperty("speed", speed);

  logInfo("Speed set to " + std::to_string(speed), deviceId);
}

void Focuser::setBacklash(int backlashValue) {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (backlashValue < 0 || backlashValue > 1000) {
    logWarning("Invalid backlash value: " + std::to_string(backlashValue),
               deviceId);
    return;
  }

  backlash = backlashValue;
  setProperty("backlash", backlash);

  logInfo("Backlash set to " + std::to_string(backlash), deviceId);
}

void Focuser::setTemperatureCompensation(bool enabled, double coefficient) {
  std::lock_guard<std::mutex> lock(statusMutex);

  tempCompEnabled = enabled;

  if (coefficient != 0.0) {
    tempCompCoefficient = coefficient;
  }

  setProperty("temperatureCompensation", tempCompEnabled);
  setProperty("tempCompCoefficient", tempCompCoefficient);

  logInfo("Temperature compensation " +
              std::string(enabled ? "enabled" : "disabled") +
              ", coefficient: " + std::to_string(tempCompCoefficient),
          deviceId);
}

void Focuser::updateLoop() {
  logInfo("Update loop started", deviceId);

  // 随机数生成器用于模拟温度变化
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> tempDist(-0.2, 0.2);

  auto lastTime = std::chrono::steady_clock::now();

  while (updateRunning) {
    // 约每100ms更新一次
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto currentTime = std::chrono::steady_clock::now();
    double elapsedSec =
        std::chrono::duration<double>(currentTime - lastTime).count();
    lastTime = currentTime;

    // 线程安全的状态更新
    {
      std::lock_guard<std::mutex> lock(statusMutex);

      // 模拟温度变化
      temperatureDrift += tempDist(gen) * elapsedSec;
      temperatureDrift =
          std::max(-3.0, std::min(3.0, temperatureDrift)); // 限制漂移范围

      temperature = ambientTemperature + temperatureDrift;
      setProperty("temperature", temperature);

      // 如果正在移动
      if (isMoving) {
        // 计算步进大小
        int step = std::max(1, static_cast<int>(speed * 10 * elapsedSec));

        // 朝目标位置移动
        if (std::abs(targetPosition - position) <= step) {
          // 到达目标位置
          position = targetPosition;
          isMoving = false;
          setProperty("isMoving", false);

          // 发送移动完成事件
          if (!currentMoveMessageId.empty()) {
            sendMoveCompletedEvent(currentMoveMessageId);
            currentMoveMessageId.clear();
          }

          logInfo("Move completed at position: " + std::to_string(position),
                  deviceId);
        } else {
          // 继续移动
          if (position < targetPosition) {
            position += step;
          } else {
            position -= step;
          }
        }

        // 更新位置属性
        setProperty("position", position);
      } else if (tempCompEnabled) {
        // 应用温度补偿
        int compensatedPosition = applyTemperatureCompensation(position);

        if (compensatedPosition != position) {
          // 温度补偿需要移动
          int originalPosition = position;
          position = compensatedPosition;
          setProperty("position", position);

          logInfo("Temperature compensation adjusted position from " +
                      std::to_string(originalPosition) + " to " +
                      std::to_string(position),
                  deviceId);
        }
      }
    }
  }

  logInfo("Update loop ended", deviceId);
}

int Focuser::applyTemperatureCompensation(int currentPosition) {
  // 计算上次温度和当前温度的差值
  static double lastTemp = temperature;
  double tempDiff = temperature - lastTemp;
  lastTemp = temperature;

  // 如果温差太小，不进行补偿
  if (std::abs(tempDiff) < 0.1) {
    return currentPosition;
  }

  // 计算移动步数: 温差 × 补偿系数
  int steps = static_cast<int>(tempDiff * tempCompCoefficient);

  // 返回补偿后的位置，确保在有效范围内
  return std::max(0, std::min(maxPosition, currentPosition + steps));
}

void Focuser::sendMoveCompletedEvent(const std::string &relatedMessageId) {
  EventMessage event("COMMAND_COMPLETED");
  event.setRelatedMessageId(relatedMessageId);

  event.setDetails({{"command", "MOVE"},
                    {"status", "SUCCESS"},
                    {"finalPosition", position}});

  sendEvent(event);
}

// 命令处理器实现
void Focuser::handleMoveAbsoluteCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("position")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'position'"}});
    return;
  }

  int newPosition = params["position"];

  // 存储消息ID以便完成事件
  currentMoveMessageId = cmd.getMessageId();

  // 开始移动
  moveAbsolute(newPosition);

  // 计算估计完成时间
  int moveDistance = std::abs(targetPosition - position);
  int estimatedStepsPerSecond = speed * 10;
  int estimatedSeconds = moveDistance / estimatedStepsPerSecond + 1;

  auto now = std::chrono::system_clock::now();
  auto completeTime = now + std::chrono::seconds(estimatedSeconds);
  auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
  std::ostringstream est_ss;
  est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

  double progressPercentage = 0.0;
  if (moveDistance > 0) {
    progressPercentage = 0.0; // 刚开始移动
  } else {
    progressPercentage = 100.0; // 已经在目标位置
  }

  response.setStatus("IN_PROGRESS");
  response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                       {"progressPercentage", progressPercentage},
                       {"targetPosition", targetPosition},
                       {"currentPosition", position}});
}

void Focuser::handleMoveRelativeCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("steps")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'steps'"}});
    return;
  }

  int steps = params["steps"];

  // 存储消息ID以便完成事件
  currentMoveMessageId = cmd.getMessageId();

  // 开始相对移动
  moveRelative(steps);

  // 计算估计完成时间
  int moveDistance = std::abs(targetPosition - position);
  int estimatedStepsPerSecond = speed * 10;
  int estimatedSeconds = moveDistance / estimatedStepsPerSecond + 1;

  auto now = std::chrono::system_clock::now();
  auto completeTime = now + std::chrono::seconds(estimatedSeconds);
  auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
  std::ostringstream est_ss;
  est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

  response.setStatus("IN_PROGRESS");
  response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                       {"progressPercentage", 0.0},
                       {"steps", steps},
                       {"targetPosition", targetPosition},
                       {"currentPosition", position}});
}

void Focuser::handleAbortCommand(const CommandMessage &cmd,
                                 ResponseMessage &response) {
  abort();

  response.setStatus("SUCCESS");
  response.setDetails(
      {{"message", "Movement aborted"}, {"position", position}});
}

void Focuser::handleSetMaxPositionCommand(const CommandMessage &cmd,
                                          ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("maxPosition")) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_PARAMETERS"},
         {"message", "Missing required parameter 'maxPosition'"}});
    return;
  }

  int maxPos = params["maxPosition"];

  if (maxPos <= 0) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_VALUE"},
                         {"message", "Max position must be greater than 0"}});
    return;
  }

  setMaxPosition(maxPos);

  response.setStatus("SUCCESS");
  response.setDetails({{"maxPosition", maxPosition}});
}

void Focuser::handleSetSpeedCommand(const CommandMessage &cmd,
                                    ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("speed")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'speed'"}});
    return;
  }

  int speedValue = params["speed"];

  if (speedValue < 1 || speedValue > 10) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_VALUE"},
                         {"message", "Speed must be between 1 and 10"}});
    return;
  }

  setSpeed(speedValue);

  response.setStatus("SUCCESS");
  response.setDetails({{"speed", speed}});
}

void Focuser::handleSetBacklashCommand(const CommandMessage &cmd,
                                       ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("backlash")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'backlash'"}});
    return;
  }

  int backlashValue = params["backlash"];

  if (backlashValue < 0 || backlashValue > 1000) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_VALUE"},
                         {"message", "Backlash must be between 0 and 1000"}});
    return;
  }

  setBacklash(backlashValue);

  response.setStatus("SUCCESS");
  response.setDetails({{"backlash", backlash}});
}

void Focuser::handleSetTempCompCommand(const CommandMessage &cmd,
                                       ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("enabled")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'enabled'"}});
    return;
  }

  bool enabled = params["enabled"];
  double coefficient = tempCompCoefficient;

  if (params.contains("coefficient")) {
    coefficient = params["coefficient"];
  }

  setTemperatureCompensation(enabled, coefficient);

  response.setStatus("SUCCESS");
  response.setDetails({{"temperatureCompensation", tempCompEnabled},
                       {"coefficient", tempCompCoefficient}});
}

} // namespace astrocomm
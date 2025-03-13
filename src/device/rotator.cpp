#include "device/rotator.h"
#include "common/logger.h"
#include <cmath>
#include <stdexcept>

namespace astrocomm {

Rotator::Rotator(const std::string &deviceId, const std::string &manufacturer,
                 const std::string &model)
    : DeviceBase(deviceId, "ROTATOR", manufacturer, model), position(0.0),
      targetPosition(0.0), stepSize(0.1), maxSpeed(10.0), currentSpeed(5.0),
      isMovingFlag(false), isReversedFlag(false), update_running(false) {

  // 初始化属性
  setProperty("position", position);
  setProperty("target_position", targetPosition);
  setProperty("step_size", stepSize);
  setProperty("max_speed", maxSpeed);
  setProperty("current_speed", currentSpeed);
  setProperty("is_moving", false);
  setProperty("is_reversed", false);
  setProperty("connected", false);

  // 设置设备能力
  capabilities = {"ABSOLUTE_POSITION", "RELATIVE_POSITION", "REVERSE"};

  // 注册命令处理器
  registerCommandHandler(
      "MOVE_TO", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleMoveToCommand(cmd, response);
      });

  registerCommandHandler(
      "MOVE_BY", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleMoveByCommand(cmd, response);
      });

  registerCommandHandler(
      "HALT", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleHaltCommand(cmd, response);
      });

  registerCommandHandler("SET_REVERSE", [this](const CommandMessage &cmd,
                                               ResponseMessage &response) {
    handleReverseCommand(cmd, response);
  });

  registerCommandHandler(
      "SYNC", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSyncCommand(cmd, response);
      });

  logInfo("Rotator device initialized", deviceId);
}

Rotator::~Rotator() { stop(); }

bool Rotator::start() {
  if (!DeviceBase::start()) {
    return false;
  }

  // 启动更新线程
  update_running = true;
  update_thread = std::thread(&Rotator::updateLoop, this);

  setProperty("connected", true);
  logInfo("Rotator device started", deviceId);
  return true;
}

void Rotator::stop() {
  // 停止更新线程
  update_running = false;
  if (update_thread.joinable()) {
    update_thread.join();
  }

  setProperty("connected", false);
  DeviceBase::stop();
  logInfo("Rotator device stopped", deviceId);
}

void Rotator::moveTo(double targetPos) {
  // 规范化目标位置
  targetPosition = normalizeAngle(targetPos);
  setProperty("target_position", targetPosition);

  // 启动移动
  isMovingFlag = true;
  setProperty("is_moving", true);

  logInfo("Starting movement to position: " + std::to_string(targetPosition),
          deviceId);
}

void Rotator::moveBy(double offset) {
  // 计算新的目标位置
  double newTarget = normalizeAngle(position + offset);
  moveTo(newTarget);
}

void Rotator::halt() {
  if (isMovingFlag) {
    isMovingFlag = false;
    setProperty("is_moving", false);
    current_move_message_id.clear();

    // 更新目标位置为当前位置
    targetPosition = position;
    setProperty("target_position", targetPosition);

    logInfo("Movement halted at position: " + std::to_string(position),
            deviceId);

    // 发送中止事件
    EventMessage event("MOVEMENT_HALTED");
    event.setDetails({{"position", position}});
    sendEvent(event);
  }
}

void Rotator::setReverse(bool reversed) {
  if (isReversedFlag != reversed) {
    isReversedFlag = reversed;
    setProperty("is_reversed", reversed);

    logInfo("Reverse direction set to: " +
                std::string(reversed ? "true" : "false"),
            deviceId);

    // 发送反向设置变更事件
    EventMessage event("REVERSE_CHANGED");
    event.setDetails({{"reversed", reversed}});
    sendEvent(event);
  }
}

void Rotator::sync(double newPosition) {
  // 规范化新位置
  double normalizedPosition = normalizeAngle(newPosition);

  // 如果正在移动，先停止
  if (isMovingFlag) {
    halt();
  }

  // 设置当前位置
  position = normalizedPosition;
  targetPosition = normalizedPosition;
  setProperty("position", position);
  setProperty("target_position", targetPosition);

  logInfo("Position synced to: " + std::to_string(position), deviceId);

  // 发送同步事件
  EventMessage event("POSITION_SYNCED");
  event.setDetails({{"position", position}});
  sendEvent(event);
}

void Rotator::setStepSize(double newStepSize) {
  if (newStepSize > 0.0) {
    stepSize = newStepSize;
    setProperty("step_size", stepSize);

    logInfo("Step size set to: " + std::to_string(stepSize), deviceId);
  } else {
    logWarning("Invalid step size: " + std::to_string(newStepSize), deviceId);
  }
}

double Rotator::getPosition() const { return position; }

double Rotator::getTargetPosition() const { return targetPosition; }

bool Rotator::isMoving() const { return isMovingFlag; }

bool Rotator::isReversed() const { return isReversedFlag; }

void Rotator::updateLoop() {
  logInfo("Update loop started", deviceId);

  const auto updateInterval =
      std::chrono::milliseconds(50); // 20 Hz update rate

  while (update_running) {
    auto startTime = std::chrono::steady_clock::now();

    if (isMovingFlag) {
      // 计算最短路径和方向
      auto [distance, clockwise] =
          calculateRotationPath(position, targetPosition);

      // 如果设置了反向，则反转方向
      if (isReversedFlag) {
        clockwise = !clockwise;
      }

      // 计算这一步的移动量
      double moveAmount = currentSpeed * (updateInterval.count() / 1000.0);

      // 限制移动量不超过剩余距离
      if (moveAmount > distance) {
        moveAmount = distance;
      }

      // 应用移动
      if (clockwise) {
        position = normalizeAngle(position + moveAmount);
      } else {
        position = normalizeAngle(position - moveAmount);
      }

      // 更新位置属性
      setProperty("position", position);

      // 检查是否到达目标位置
      if (std::abs(normalizeAngle(position - targetPosition)) < 0.01) {
        position = targetPosition; // 确保完全精确
        setProperty("position", position);

        isMovingFlag = false;
        setProperty("is_moving", false);

        logInfo("Movement completed at position: " + std::to_string(position),
                deviceId);

        // 发送移动完成事件
        if (!current_move_message_id.empty()) {
          sendMoveCompletedEvent(current_move_message_id);
          current_move_message_id.clear();
        }
      }
    }

    // 等待直到下一个更新间隔
    auto elapsedTime = std::chrono::steady_clock::now() - startTime;
    if (elapsedTime < updateInterval) {
      std::this_thread::sleep_for(updateInterval - elapsedTime);
    }
  }

  logInfo("Update loop ended", deviceId);
}

void Rotator::handleMoveToCommand(const CommandMessage &cmd,
                                  ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("position")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'position'"}});
    return;
  }

  double newPosition = params["position"];

  // 保存消息ID用于完成事件
  current_move_message_id = cmd.getMessageId();

  // 开始移动
  moveTo(newPosition);

  // 计算估计的完成时间
  auto [distance, clockwise] = calculateRotationPath(position, targetPosition);
  double estimatedSeconds = distance / currentSpeed;

  auto now = std::chrono::system_clock::now();
  auto completeTime = now + std::chrono::milliseconds(
                                static_cast<int>(estimatedSeconds * 1000));
  auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
  std::ostringstream est_ss;
  est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

  response.setStatus("IN_PROGRESS");
  response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                       {"progressPercentage", 0},
                       {"startPosition", position},
                       {"targetPosition", targetPosition},
                       {"distance", distance}});
}

void Rotator::handleMoveByCommand(const CommandMessage &cmd,
                                  ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("offset")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'offset'"}});
    return;
  }

  double offset = params["offset"];

  // 保存消息ID用于完成事件
  current_move_message_id = cmd.getMessageId();

  // 开始相对移动
  moveBy(offset);

  // 计算估计的完成时间
  auto [distance, clockwise] = calculateRotationPath(position, targetPosition);
  double estimatedSeconds = distance / currentSpeed;

  auto now = std::chrono::system_clock::now();
  auto completeTime = now + std::chrono::milliseconds(
                                static_cast<int>(estimatedSeconds * 1000));
  auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
  std::ostringstream est_ss;
  est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

  response.setStatus("IN_PROGRESS");
  response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                       {"progressPercentage", 0},
                       {"startPosition", position},
                       {"targetPosition", targetPosition},
                       {"offset", offset},
                       {"distance", distance}});
}

void Rotator::handleHaltCommand(const CommandMessage &cmd,
                                ResponseMessage &response) {
  // 停止当前移动
  halt();

  response.setStatus("SUCCESS");
  response.setDetails({{"position", position}, {"message", "Movement halted"}});
}

void Rotator::handleReverseCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("reversed")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'reversed'"}});
    return;
  }

  bool reversed = params["reversed"];

  // 设置反向状态
  setReverse(reversed);

  response.setStatus("SUCCESS");
  response.setDetails({{"reversed", reversed}});
}

void Rotator::handleSyncCommand(const CommandMessage &cmd,
                                ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("position")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'position'"}});
    return;
  }

  double newPosition = params["position"];

  // 同步位置
  sync(newPosition);

  response.setStatus("SUCCESS");
  response.setDetails({{"position", position}});
}

void Rotator::sendMoveCompletedEvent(const std::string &relatedMessageId) {
  EventMessage event("COMMAND_COMPLETED");
  event.setRelatedMessageId(relatedMessageId);

  event.setDetails({{"command", "MOVE_TO"},
                    {"status", "SUCCESS"},
                    {"finalPosition", position}});

  sendEvent(event);
}

double Rotator::normalizeAngle(double angle) const {
  // 规范化角度到0-360度范围
  angle = std::fmod(angle, 360.0);
  if (angle < 0.0) {
    angle += 360.0;
  }
  return angle;
}

std::pair<double, bool> Rotator::calculateRotationPath(double current,
                                                       double target) const {
  // 规范化角度
  current = normalizeAngle(current);
  target = normalizeAngle(target);

  // 计算顺时针和逆时针距离
  double clockwiseDistance =
      (target >= current) ? target - current : target + 360.0 - current;

  double counterClockwiseDistance =
      (current >= target) ? current - target : current + 360.0 - target;

  // 选择最短路径
  if (clockwiseDistance <= counterClockwiseDistance) {
    return {clockwiseDistance, true}; // 顺时针，距离
  } else {
    return {counterClockwiseDistance, false}; // 逆时针，距离
  }
}

} // namespace astrocomm
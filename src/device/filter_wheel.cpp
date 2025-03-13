#include "device/filter_wheel.h"
#include "common/logger.h"
#include <algorithm>

namespace astrocomm {

FilterWheel::FilterWheel(const std::string &deviceId,
                         const std::string &manufacturer,
                         const std::string &model)
    : DeviceBase(deviceId, "FILTER_WHEEL", manufacturer, model), position(0),
      targetPosition(0), filterCount(5), isMoving(false), moveDirection(1),
      updateRunning(false) {

  // 初始化滤镜名称和偏移量
  filterNames = {"Red", "Green", "Blue", "Luminance", "H-Alpha"};
  filterOffsets = {0, 0, 0, 0, 0}; // 默认无偏移

  // 初始化属性
  setProperty("position", position);
  setProperty("filterCount", filterCount);
  setProperty("filterNames", filterNames);
  setProperty("filterOffsets", filterOffsets);
  setProperty("isMoving", false);
  setProperty("connected", false);
  setProperty("currentFilter", getCurrentFilterName());

  // 设置能力
  capabilities = {"NAMED_FILTERS", "FILTER_OFFSETS"};

  // 注册命令处理器
  registerCommandHandler("SET_POSITION", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
    handleSetPositionCommand(cmd, response);
  });

  registerCommandHandler("SET_FILTER_NAMES", [this](const CommandMessage &cmd,
                                                    ResponseMessage &response) {
    handleSetFilterNamesCommand(cmd, response);
  });

  registerCommandHandler(
      "SET_FILTER_OFFSETS",
      [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSetFilterOffsetsCommand(cmd, response);
      });

  registerCommandHandler(
      "ABORT", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleAbortCommand(cmd, response);
      });

  logInfo("FilterWheel device initialized with " + std::to_string(filterCount) +
              " filters",
          deviceId);
}

FilterWheel::~FilterWheel() { stop(); }

bool FilterWheel::start() {
  if (!DeviceBase::start()) {
    return false;
  }

  // 启动更新线程
  updateRunning = true;
  updateThread = std::thread(&FilterWheel::updateLoop, this);

  setProperty("connected", true);
  logInfo("FilterWheel started", deviceId);
  return true;
}

void FilterWheel::stop() {
  // 停止更新线程
  updateRunning = false;
  if (updateThread.joinable()) {
    updateThread.join();
  }

  setProperty("connected", false);
  DeviceBase::stop();
  logInfo("FilterWheel stopped", deviceId);
}

void FilterWheel::setPosition(int newPosition) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 验证位置是否在有效范围内
  if (newPosition < 0 || newPosition >= filterCount) {
    logWarning("Invalid position: " + std::to_string(newPosition) +
                   ", must be between 0 and " + std::to_string(filterCount - 1),
               deviceId);
    return;
  }

  // 如果已经在目标位置则不移动
  if (newPosition == position && !isMoving) {
    logInfo("Already at position " + std::to_string(position) + " (" +
                getCurrentFilterName() + ")",
            deviceId);
    return;
  }

  // 确定最短路径移动方向
  moveDirection = determineShortestPath(position, newPosition);

  targetPosition = newPosition;
  isMoving = true;
  setProperty("isMoving", true);

  logInfo("Starting move to position " + std::to_string(targetPosition) + " (" +
              filterNames[targetPosition] + ")",
          deviceId);
}

void FilterWheel::setFilterNames(const std::vector<std::string> &names) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 确保名称数量与滤镜数量匹配
  if (names.size() != filterCount) {
    logWarning("Filter names count (" + std::to_string(names.size()) +
                   ") doesn't match filter count (" +
                   std::to_string(filterCount) + ")",
               deviceId);
    return;
  }

  filterNames = names;
  setProperty("filterNames", filterNames);
  setProperty("currentFilter", getCurrentFilterName());

  logInfo("Filter names updated", deviceId);
}

void FilterWheel::setFilterOffsets(const std::vector<int> &offsets) {
  std::lock_guard<std::mutex> lock(statusMutex);

  // 确保偏移量数量与滤镜数量匹配
  if (offsets.size() != filterCount) {
    logWarning("Filter offsets count (" + std::to_string(offsets.size()) +
                   ") doesn't match filter count (" +
                   std::to_string(filterCount) + ")",
               deviceId);
    return;
  }

  filterOffsets = offsets;
  setProperty("filterOffsets", filterOffsets);

  logInfo("Filter offsets updated", deviceId);
}

void FilterWheel::abort() {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (!isMoving) {
    logInfo("No movement to abort", deviceId);
    return;
  }

  isMoving = false;
  targetPosition = position; // 停在当前位置
  currentMoveMessageId.clear();
  setProperty("isMoving", false);

  logInfo("Movement aborted at position " + std::to_string(position), deviceId);

  // 发送中止事件
  EventMessage event("ABORTED");
  event.setDetails(
      {{"position", position}, {"filter", getCurrentFilterName()}});
  sendEvent(event);
}

std::string FilterWheel::getCurrentFilterName() const {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (position >= 0 && position < filterNames.size()) {
    return filterNames[position];
  }

  return "Unknown";
}

int FilterWheel::getCurrentFilterOffset() const {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (position >= 0 && position < filterOffsets.size()) {
    return filterOffsets[position];
  }

  return 0;
}

int FilterWheel::determineShortestPath(int from, int to) {
  // 计算顺时针和逆时针的距离
  int clockwise = (to >= from) ? (to - from) : (filterCount - from + to);
  int counterClockwise = (from >= to) ? (from - to) : (from + filterCount - to);

  // 返回距离最短的方向
  return (clockwise <= counterClockwise) ? 1 : -1;
}

void FilterWheel::updateLoop() {
  logInfo("Update loop started", deviceId);

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

      // 如果正在移动
      if (isMoving) {
        // 滤镜轮转动速度 - 每秒1个位置
        static double progressFraction = 0.0;
        progressFraction += elapsedSec;

        if (progressFraction >= 1.0) {
          // 足够时间移动到下一个位置
          progressFraction = 0.0;

          // 更新位置
          position = (position + moveDirection + filterCount) % filterCount;
          setProperty("position", position);
          setProperty("currentFilter", getCurrentFilterName());

          // 检查是否到达目标位置
          if (position == targetPosition) {
            isMoving = false;
            setProperty("isMoving", false);

            // 发送移动完成事件
            if (!currentMoveMessageId.empty()) {
              sendPositionChangeCompletedEvent(currentMoveMessageId);
              currentMoveMessageId.clear();
            }

            logInfo("Move completed at position " + std::to_string(position) +
                        " (" + getCurrentFilterName() + ")",
                    deviceId);
          }
        }
      }
    }
  }

  logInfo("Update loop ended", deviceId);
}

void FilterWheel::sendPositionChangeCompletedEvent(
    const std::string &relatedMessageId) {
  EventMessage event("COMMAND_COMPLETED");
  event.setRelatedMessageId(relatedMessageId);

  event.setDetails({{"command", "SET_POSITION"},
                    {"status", "SUCCESS"},
                    {"position", position},
                    {"filter", getCurrentFilterName()},
                    {"offset", getCurrentFilterOffset()}});

  sendEvent(event);
}

// 命令处理器实现
void FilterWheel::handleSetPositionCommand(const CommandMessage &cmd,
                                           ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("position") && !params.contains("filter")) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_PARAMETERS"},
         {"message", "Missing required parameter 'position' or 'filter'"}});
    return;
  }

  int newPosition = -1;

  // 可以通过位置编号或滤镜名称设置
  if (params.contains("position")) {
    newPosition = params["position"];
  } else if (params.contains("filter")) {
    std::string filterName = params["filter"];
    // 查找滤镜名称对应的位置
    for (size_t i = 0; i < filterNames.size(); i++) {
      if (filterNames[i] == filterName) {
        newPosition = static_cast<int>(i);
        break;
      }
    }

    if (newPosition == -1) {
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "INVALID_FILTER"},
           {"message", "Filter name not found: " + filterName}});
      return;
    }
  }

  // 验证位置是否有效
  if (newPosition < 0 || newPosition >= filterCount) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_POSITION"},
                         {"message", "Position must be between 0 and " +
                                         std::to_string(filterCount - 1)}});
    return;
  }

  // 存储消息ID以便完成事件
  currentMoveMessageId = cmd.getMessageId();

  // 开始移动
  setPosition(newPosition);

  // 计算估计完成时间，每个位置需要1秒
  int moveSteps = 0;

  // 计算最短路径的步数
  int clockwise = (newPosition >= position)
                      ? (newPosition - position)
                      : (filterCount - position + newPosition);

  int counterClockwise = (position >= newPosition)
                             ? (position - newPosition)
                             : (position + filterCount - newPosition);

  moveSteps = std::min(clockwise, counterClockwise);

  int estimatedSeconds = moveSteps;
  if (estimatedSeconds == 0)
    estimatedSeconds = 1; // 至少需要1秒

  auto now = std::chrono::system_clock::now();
  auto completeTime = now + std::chrono::seconds(estimatedSeconds);
  auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
  std::ostringstream est_ss;
  est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

  double progressPercentage = 0.0;
  if (moveSteps > 0) {
    progressPercentage = 0.0; // 刚开始移动
  } else {
    progressPercentage = 100.0; // 已经在目标位置
  }

  response.setStatus("IN_PROGRESS");
  response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                       {"progressPercentage", progressPercentage},
                       {"targetPosition", targetPosition},
                       {"targetFilter", filterNames[targetPosition]},
                       {"currentPosition", position},
                       {"currentFilter", getCurrentFilterName()},
                       {"offset", getCurrentFilterOffset()}});
}

void FilterWheel::handleSetFilterNamesCommand(const CommandMessage &cmd,
                                              ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("names") || !params["names"].is_array()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing or invalid 'names' parameter"}});
    return;
  }

  std::vector<std::string> names =
      params["names"].get<std::vector<std::string>>();

  if (names.size() != filterCount) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_COUNT"},
         {"message", "Filter names count must match filter count (" +
                         std::to_string(filterCount) + ")"}});
    return;
  }

  setFilterNames(names);

  response.setStatus("SUCCESS");
  response.setDetails({{"filterNames", filterNames}});
}

void FilterWheel::handleSetFilterOffsetsCommand(const CommandMessage &cmd,
                                                ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("offsets") || !params["offsets"].is_array()) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_PARAMETERS"},
         {"message", "Missing or invalid 'offsets' parameter"}});
    return;
  }

  std::vector<int> offsets = params["offsets"].get<std::vector<int>>();

  if (offsets.size() != filterCount) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_COUNT"},
         {"message", "Filter offsets count must match filter count (" +
                         std::to_string(filterCount) + ")"}});
    return;
  }

  setFilterOffsets(offsets);

  response.setStatus("SUCCESS");
  response.setDetails({{"filterOffsets", filterOffsets}});
}

void FilterWheel::handleAbortCommand(const CommandMessage &cmd,
                                     ResponseMessage &response) {
  abort();

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Movement aborted"},
                       {"position", position},
                       {"filter", getCurrentFilterName()}});
}

} // namespace astrocomm
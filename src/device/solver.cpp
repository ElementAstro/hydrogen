#include "device/solver.h"
#include "common/logger.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace astrocomm {

Solver::Solver(const std::string &deviceId, const std::string &manufacturer,
               const std::string &model)
    : DeviceBase(deviceId, "SOLVER", manufacturer, model),
      state(SolverState::IDLE), progress(0), fovMin(10), fovMax(180),
      scaleMin(0.1), scaleMax(10), useDistortion(false), downsample(1),
      raHint(0), decHint(0), radiusHint(180), hasValidSolution(false) {

  // 初始化随机数生成器
  std::random_device rd;
  rng = std::mt19937(rd());

  // 初始化属性
  setProperty("state", solverStateToString(state));
  setProperty("progress", 0);
  setProperty("fovMin", fovMin);
  setProperty("fovMax", fovMax);
  setProperty("scaleMin", scaleMin);
  setProperty("scaleMax", scaleMax);
  setProperty("useDistortion", useDistortion);
  setProperty("downsample", downsample);
  setProperty("raHint", raHint);
  setProperty("decHint", decHint);
  setProperty("radiusHint", radiusHint);
  setProperty("hasValidSolution", hasValidSolution);
  setProperty("connected", false);

  // 设置能力
  capabilities = {"PLATE_SOLVING", "DISTORTION_ANALYSIS",
                  "MULTI_STAR_DETECTION"};

  // 注册命令处理器
  registerCommandHandler(
      "SOLVE", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSolveCommand(cmd, response);
      });

  registerCommandHandler("SOLVE_FILE", [this](const CommandMessage &cmd,
                                              ResponseMessage &response) {
    handleSolveFileCommand(cmd, response);
  });

  registerCommandHandler(
      "ABORT", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleAbortCommand(cmd, response);
      });

  registerCommandHandler("SET_PARAMETERS", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleSetParametersCommand(cmd, response);
  });

  logInfo("Solver device initialized", deviceId);
}

Solver::~Solver() { stop(); }

bool Solver::start() {
  if (!DeviceBase::start()) {
    return false;
  }

  setProperty("connected", true);
  logInfo("Solver started", deviceId);
  return true;
}

void Solver::stop() {
  // 中止正在进行的解析
  abort();

  setProperty("connected", false);
  DeviceBase::stop();
  logInfo("Solver stopped", deviceId);
}

void Solver::solve(const std::vector<uint8_t> &imageData, int width,
                   int height) {
  std::lock_guard<std::mutex> lock(solveMutex);

  if (state == SolverState::SOLVING) {
    logWarning("Cannot start new solve while another is in progress", deviceId);
    return;
  }

  // 检查图像数据是否有效
  if (imageData.empty() || width <= 0 || height <= 0 ||
      imageData.size() < width * height) {
    logWarning("Invalid image data for solving", deviceId);
    return;
  }

  // 更新状态
  state = SolverState::SOLVING;
  progress = 0;
  setProperty("state", solverStateToString(state));
  setProperty("progress", 0);

  // 启动解析线程
  if (solveThreadObj.joinable()) {
    solveThreadObj.join();
  }
  solveThreadObj =
      std::thread(&Solver::solveThread, this, imageData, width, height);

  logInfo("Started solving image " + std::to_string(width) + "x" +
              std::to_string(height),
          deviceId);
}

void Solver::solveFromFile(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(solveMutex);

  if (state == SolverState::SOLVING) {
    logWarning("Cannot start new solve while another is in progress", deviceId);
    return;
  }

  // 检查文件是否存在
  if (!std::filesystem::exists(filePath)) {
    logWarning("File not found: " + filePath, deviceId);
    return;
  }

  // 更新状态
  state = SolverState::SOLVING;
  progress = 0;
  setProperty("state", solverStateToString(state));
  setProperty("progress", 0);

  // 启动解析线程
  if (solveThreadObj.joinable()) {
    solveThreadObj.join();
  }
  solveThreadObj = std::thread(&Solver::solveFileThread, this, filePath);

  logInfo("Started solving file: " + filePath, deviceId);
}

void Solver::abort() {
  std::lock_guard<std::mutex> lock(solveMutex);

  if (state != SolverState::SOLVING) {
    logInfo("No solving process to abort", deviceId);
    return;
  }

  // 更新状态
  state = SolverState::IDLE;
  progress = 0;
  setProperty("state", solverStateToString(state));
  setProperty("progress", 0);

  // 发送中止事件
  EventMessage event("SOLVE_ABORTED");
  sendEvent(event);

  // 等待线程完成
  if (solveThreadObj.joinable()) {
    solveThreadObj.join();
  }

  logInfo("Solving aborted", deviceId);
}

void Solver::setParameters(const json &params) {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (state == SolverState::SOLVING) {
    logWarning("Cannot change parameters while solving", deviceId);
    return;
  }

  // 更新解析参数
  if (params.contains("fovMin") && params["fovMin"].is_number()) {
    fovMin = params["fovMin"];
    setProperty("fovMin", fovMin);
  }

  if (params.contains("fovMax") && params["fovMax"].is_number()) {
    fovMax = params["fovMax"];
    setProperty("fovMax", fovMax);
  }

  if (params.contains("scaleMin") && params["scaleMin"].is_number()) {
    scaleMin = params["scaleMin"];
    setProperty("scaleMin", scaleMin);
  }

  if (params.contains("scaleMax") && params["scaleMax"].is_number()) {
    scaleMax = params["scaleMax"];
    setProperty("scaleMax", scaleMax);
  }

  if (params.contains("useDistortion") &&
      params["useDistortion"].is_boolean()) {
    useDistortion = params["useDistortion"];
    setProperty("useDistortion", useDistortion);
  }

  if (params.contains("downsample") && params["downsample"].is_number()) {
    downsample = params["downsample"];
    setProperty("downsample", downsample);
  }

  if (params.contains("raHint") && params["raHint"].is_number()) {
    raHint = params["raHint"];
    setProperty("raHint", raHint);
  }

  if (params.contains("decHint") && params["decHint"].is_number()) {
    decHint = params["decHint"];
    setProperty("decHint", decHint);
  }

  if (params.contains("radiusHint") && params["radiusHint"].is_number()) {
    radiusHint = params["radiusHint"];
    setProperty("radiusHint", radiusHint);
  }

  logInfo("Solver parameters updated", deviceId);
}

void Solver::setSolverPath(const std::string &path) {
  std::lock_guard<std::mutex> lock(statusMutex);

  solverPath = path;
  logInfo("Solver path set to: " + path, deviceId);
}

void Solver::setSolverOptions(
    const std::map<std::string, std::string> &options) {
  std::lock_guard<std::mutex> lock(statusMutex);

  solverOptions = options;
  logInfo("Solver options updated", deviceId);
}

json Solver::getLastSolution() const {
  std::lock_guard<std::mutex> lock(statusMutex);

  if (!hasValidSolution) {
    return json::object();
  }

  return lastSolution;
}

void Solver::solveThread(std::vector<uint8_t> imageData, int width,
                         int height) {
  // 模拟解析过程
  bool success = performSolve(imageData, width, height);

  // 处理结果
  {
    std::lock_guard<std::mutex> lock(statusMutex);

    if (state != SolverState::SOLVING) {
      // 解析已被中止
      return;
    }

    // 生成解析结果
    lastSolution = generateSolution(success);
    hasValidSolution = success;

    // 更新状态
    state = success ? SolverState::COMPLETE : SolverState::FAILED;
    progress = 100;
    setProperty("state", solverStateToString(state));
    setProperty("progress", progress.load());
    setProperty("hasValidSolution", hasValidSolution);

    // 发送解析完成事件
    if (!currentSolveMessageId.empty()) {
      sendSolveCompletedEvent(currentSolveMessageId);
      currentSolveMessageId.clear();
    }
  }

  logInfo("Solving " +
              std::string(success ? "completed successfully" : "failed"),
          deviceId);
}

void Solver::solveFileThread(std::string filePath) {
  // 检查文件格式
  std::filesystem::path path(filePath);
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 ::tolower);

  // 假设我们支持这些格式
  bool supportedFormat =
      (extension == ".jpg" || extension == ".jpeg" || extension == ".png" ||
       extension == ".fit" || extension == ".fits");

  if (!supportedFormat) {
    logWarning("Unsupported file format: " + extension, deviceId);

    std::lock_guard<std::mutex> lock(statusMutex);
    state = SolverState::FAILED;
    progress = 100;
    setProperty("state", solverStateToString(state));
    setProperty("progress", progress.load());

    // 发送解析完成事件（失败）
    if (!currentSolveMessageId.empty()) {
      lastSolution = generateSolution(false);
      hasValidSolution = false;
      setProperty("hasValidSolution", hasValidSolution);

      sendSolveCompletedEvent(currentSolveMessageId);
      currentSolveMessageId.clear();
    }

    return;
  }

  // 这里只是模拟解析，不实际读取文件
  // 在真实实现中，这里会读取文件并提取图像数据

  // 模拟加载和解析过程
  for (int i = 0; i <= 100; i += 5) {
    if (state != SolverState::SOLVING) {
      // 解析已被中止
      return;
    }

    progress = i;
    setProperty("progress", progress.load());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // 使用模拟的图像数据和成功率
  std::uniform_real_distribution<> successDist(0, 1);
  bool success = successDist(rng) < 0.8; // 80%的成功率

  // 处理结果
  {
    std::lock_guard<std::mutex> lock(statusMutex);

    if (state != SolverState::SOLVING) {
      // 解析已被中止
      return;
    }

    // 生成解析结果
    lastSolution = generateSolution(success);
    hasValidSolution = success;

    // 更新状态
    state = success ? SolverState::COMPLETE : SolverState::FAILED;
    progress = 100;
    setProperty("state", solverStateToString(state));
    setProperty("progress", progress.load());
    setProperty("hasValidSolution", hasValidSolution);

    // 发送解析完成事件
    if (!currentSolveMessageId.empty()) {
      sendSolveCompletedEvent(currentSolveMessageId);
      currentSolveMessageId.clear();
    }
  }

  logInfo("Solving file " + (success ? "completed successfully" : "failed") +
              ": " + filePath,
          deviceId);
}

bool Solver::performSolve(const std::vector<uint8_t> &imageData, int width,
                          int height) {
  // 这是一个模拟的解析过程

  // 模拟解析的时间和进度
  for (int i = 0; i <= 100; i += 2) {
    if (state != SolverState::SOLVING) {
      // 解析已被中止
      return false;
    }

    progress = i;
    setProperty("progress", progress.load());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  // 模拟解析的成功率，受提示参数影响
  double successProbability = 0.8; // 基础成功率

  // 如果有位置提示，增加成功率
  if (raHint != 0 || decHint != 0) {
    successProbability += 0.1;
  }

  // 如果提供了合理的视场范围，增加成功率
  if (fovMax - fovMin < 90) {
    successProbability += 0.05;
  }

  // 有10%的随机因素
  std::uniform_real_distribution<> dist(0, 0.1);
  successProbability += dist(rng) - 0.05;

  // 限制在0-1范围内
  successProbability = std::max(0.0, std::min(1.0, successProbability));

  // 决定是否成功
  std::uniform_real_distribution<> successDist(0, 1);
  bool success = successDist(rng) < successProbability;

  return success;
}

json Solver::generateSolution(bool success) {
  json solution;

  if (!success) {
    solution["success"] = false;
    solution["error"] = "Failed to match the image to the star catalog";
    return solution;
  }

  // 使用提示位置作为基础，加上一些随机偏移
  std::normal_distribution<> raDist(raHint, 0.5);
  std::normal_distribution<> decDist(decHint, 0.5);

  double solutionRa = raDist(rng);
  double solutionDec = decDist(rng);

  // 确保RA在0-24范围内
  while (solutionRa < 0)
    solutionRa += 24;
  while (solutionRa >= 24)
    solutionRa -= 24;

  // 确保DEC在-90到+90范围内
  solutionDec = std::max(-90.0, std::min(90.0, solutionDec));

  // 生成其他解析参数
  std::uniform_real_distribution<> pixelScaleDist(scaleMin, scaleMax);
  std::uniform_real_distribution<> rotationDist(0, 360);
  std::uniform_real_distribution<> fovDist(fovMin, fovMax);
  std::uniform_int_distribution<> starCountDist(10, 1000);

  double pixelScale = pixelScaleDist(rng);
  double rotation = rotationDist(rng);
  double fieldWidth = fovDist(rng);
  double fieldHeight = fieldWidth * 0.75; // 假设4:3的宽高比
  int starCount = starCountDist(rng);

  // 生成解析结果
  solution["success"] = true;
  solution["ra"] = solutionRa;
  solution["dec"] = solutionDec;
  solution["ra_hms"] = formatRAToHMS(solutionRa);
  solution["dec_dms"] = formatDecToDMS(solutionDec);
  solution["pixelScale"] = pixelScale;
  solution["rotation"] = rotation;
  solution["fieldWidth"] = fieldWidth;
  solution["fieldHeight"] = fieldHeight;
  solution["starCount"] = starCount;
  solution["solveTime"] = std::uniform_real_distribution<>(1.0, 15.0)(rng);

  // 添加一些检测到的星体
  json stars = json::array();
  for (int i = 0; i < std::min(10, starCount); i++) {
    stars.push_back(
        {{"id", std::uniform_int_distribution<>(1, 10000)(rng)},
         {"x", std::uniform_real_distribution<>(0, 1000)(rng)},
         {"y", std::uniform_real_distribution<>(0, 1000)(rng)},
         {"mag", std::uniform_real_distribution<>(2.0, 12.0)(rng)}});
  }
  solution["stars"] = stars;

  // 如果启用畸变分析，添加畸变参数
  if (useDistortion) {
    solution["distortion"] = {{"a", std::normal_distribution<>(0, 0.001)(rng)},
                              {"b", std::normal_distribution<>(0, 0.001)(rng)},
                              {"c", std::normal_distribution<>(0, 0.001)(rng)}};
  }

  // 添加解析时间戳
  solution["timestamp"] = getIsoTimestamp();

  return solution;
}

void Solver::sendSolveCompletedEvent(const std::string &relatedMessageId) {
  EventMessage event("SOLVE_COMPLETED");
  event.setRelatedMessageId(relatedMessageId);

  json details = {{"success", hasValidSolution}};

  if (hasValidSolution) {
    details["ra"] = lastSolution["ra"];
    details["dec"] = lastSolution["dec"];
    details["pixelScale"] = lastSolution["pixelScale"];
    details["rotation"] = lastSolution["rotation"];
    details["fieldWidth"] = lastSolution["fieldWidth"];
    details["fieldHeight"] = lastSolution["fieldHeight"];
    details["starCount"] = lastSolution["starCount"];
    details["solveTime"] = lastSolution["solveTime"];
  } else {
    details["error"] = "Failed to solve the image";
  }

  event.setDetails(details);
  sendEvent(event);
}

std::string Solver::solverStateToString(SolverState state) const {
  switch (state) {
  case SolverState::IDLE:
    return "IDLE";
  case SolverState::SOLVING:
    return "SOLVING";
  case SolverState::COMPLETE:
    return "COMPLETE";
  case SolverState::FAILED:
    return "FAILED";
  default:
    return "UNKNOWN";
  }
}

// 命令处理器实现
void Solver::handleSolveCommand(const CommandMessage &cmd,
                                ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("imageData") || !params["imageData"].is_string() ||
      !params.contains("width") || !params["width"].is_number() ||
      !params.contains("height") || !params["height"].is_number()) {

    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_PARAMETERS"},
         {"message", "Missing required parameters: imageData, width, height"}});
    return;
  }

  // 解码Base64图像数据
  std::string encodedData = params["imageData"];
  std::vector<uint8_t> imageData = base64_decode(encodedData);

  int width = params["width"];
  int height = params["height"];

  // 更新提示参数
  if (params.contains("raHint"))
    raHint = params["raHint"];
  if (params.contains("decHint"))
    decHint = params["decHint"];
  if (params.contains("radiusHint"))
    radiusHint = params["radiusHint"];

  // 存储消息ID以便完成事件
  currentSolveMessageId = cmd.getMessageId();

  // 启动解析
  solve(imageData, width, height);

  response.setStatus("IN_PROGRESS");
  response.setDetails({{"message", "Solving started"},
                       {"state", solverStateToString(state)},
                       {"progress", progress.load()}});
}

void Solver::handleSolveFileCommand(const CommandMessage &cmd,
                                    ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("filePath") || !params["filePath"].is_string()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter: filePath"}});
    return;
  }

  std::string filePath = params["filePath"];

  // 更新提示参数
  if (params.contains("raHint"))
    raHint = params["raHint"];
  if (params.contains("decHint"))
    decHint = params["decHint"];
  if (params.contains("radiusHint"))
    radiusHint = params["radiusHint"];

  // 存储消息ID以便完成事件
  currentSolveMessageId = cmd.getMessageId();

  // 启动文件解析
  solveFromFile(filePath);

  response.setStatus("IN_PROGRESS");
  response.setDetails({{"message", "Solving from file started"},
                       {"filePath", filePath},
                       {"state", solverStateToString(state)},
                       {"progress", progress.load()}});
}

void Solver::handleAbortCommand(const CommandMessage &cmd,
                                ResponseMessage &response) {
  abort();

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Solving aborted"}});
}

void Solver::handleSetParametersCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();

  // 更新参数
  setParameters(params);

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Parameters updated"},
                       {"fovMin", fovMin},
                       {"fovMax", fovMax},
                       {"scaleMin", scaleMin},
                       {"scaleMax", scaleMax},
                       {"useDistortion", useDistortion},
                       {"downsample", downsample},
                       {"raHint", raHint},
                       {"decHint", decHint},
                       {"radiusHint", radiusHint}});
}

// 格式化工具 - 将RA转换为时分秒格式
std::string formatRAToHMS(double ra) {
  int hours = static_cast<int>(ra);
  double minutes_d = (ra - hours) * 60.0;
  int minutes = static_cast<int>(minutes_d);
  double seconds = (minutes_d - minutes) * 60.0;

  std::ostringstream oss;
  oss << std::setw(2) << std::setfill('0') << hours << ":" << std::setw(2)
      << std::setfill('0') << minutes << ":" << std::setw(5)
      << std::setfill('0') << std::fixed << std::setprecision(2) << seconds;

  return oss.str();
}

// 格式化工具 - 将DEC转换为度分秒格式
std::string formatDecToDMS(double dec) {
  char sign = (dec >= 0) ? '+' : '-';
  dec = std::abs(dec);
  int degrees = static_cast<int>(dec);
  double minutes_d = (dec - degrees) * 60.0;
  int minutes = static_cast<int>(minutes_d);
  double seconds = (minutes_d - minutes) * 60.0;

  std::ostringstream oss;
  oss << sign << std::setw(2) << std::setfill('0') << degrees << ":"
      << std::setw(2) << std::setfill('0') << minutes << ":" << std::setw(5)
      << std::setfill('0') << std::fixed << std::setprecision(2) << seconds;

  return oss.str();
}

} // namespace astrocomm
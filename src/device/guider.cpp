#include "guider.h"

#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

namespace astrocomm {

// 导星设备实现
GuiderDevice::GuiderDevice(const std::string &deviceId,
                           const std::string &manufacturer,
                           const std::string &model)
    : DeviceBase(deviceId, "GUIDER", manufacturer, model),
      interfaceType(GuiderInterfaceType::PHD2),
      lastState(GuiderState::DISCONNECTED),
      lastCalState(CalibrationState::IDLE), running(false),
      statusUpdateInterval(500) {

  // 初始化状态属性
  setProperty("state", guiderStateToString(GuiderState::DISCONNECTED));
  setProperty("calibrationState",
              calibrationStateToString(CalibrationState::IDLE));
  setProperty("calibrated", false);
  setProperty("rms", 0.0);
  setProperty("peak", 0.0);
  setProperty("interfaceType", "None");
  setProperty("connected", false);

  // 设置能力
  capabilities = {"GUIDING", "CALIBRATION", "DITHERING"};

  // 注册命令处理器
  registerCommandHandler("CONNECT_GUIDER", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleConnectCommand(cmd, response);
  });

  registerCommandHandler(
      "DISCONNECT_GUIDER",
      [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleDisconnectCommand(cmd, response);
      });

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

  registerCommandHandler("GET_STATUS", [this](const CommandMessage &cmd,
                                              ResponseMessage &response) {
    handleGetStatusCommand(cmd, response);
  });

  spdlog::info("Guider device initialized", deviceId);
}

GuiderDevice::~GuiderDevice() { stop(); }

bool GuiderDevice::start() {
  if (!DeviceBase::start()) {
    return false;
  }

  // 启动状态更新线程
  running = true;
  statusThread = std::thread(&GuiderDevice::statusUpdateLoop, this);

  setProperty("connected", true);
  spdlog::info("Guider device started", deviceId);
  return true;
}

void GuiderDevice::stop() {
  // 停止状态更新线程
  running = false;
  if (statusThread.joinable()) {
    statusThread.join();
  }

  // 断开导星软件
  disconnectFromGuider();

  setProperty("connected", false);
  DeviceBase::stop();
  spdlog::info("Guider device stopped", deviceId);
}

bool GuiderDevice::connectToGuider(GuiderInterfaceType type,
                                   const std::string &host, int port) {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  // 断开现有连接
  if (guiderInterface && guiderInterface->isConnected()) {
    guiderInterface->disconnect();
  }

  // 创建新的接口
  guiderInterface = createGuiderInterface(type);
  if (!guiderInterface) {
    spdlog::error("Failed to create guider interface", deviceId);
    return false;
  }

  // 连接到导星软件
  bool success = guiderInterface->connect(host, port);
  if (!success) {
    spdlog::error("Failed to connect to guider software", deviceId);
    return false;
  }

  // 更新状态
  interfaceType = type;
  setProperty("interfaceType", interfaceTypeToString(type));
  setProperty("state", guiderStateToString(guiderInterface->getGuiderState()));

  spdlog::info("Connected to " + interfaceTypeToString(type), deviceId);
  return true;
}

void GuiderDevice::disconnectFromGuider() {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  if (guiderInterface) {
    if (guiderInterface->isConnected()) {
      // 如果正在导星，先停止
      if (guiderInterface->getGuiderState() == GuiderState::GUIDING ||
          guiderInterface->getGuiderState() == GuiderState::SETTLING ||
          guiderInterface->getGuiderState() == GuiderState::PAUSED) {
        guiderInterface->stopGuiding();
      }

      guiderInterface->disconnect();
    }

    guiderInterface.reset();
  }

  setProperty("state", guiderStateToString(GuiderState::DISCONNECTED));
  setProperty("calibrationState",
              calibrationStateToString(CalibrationState::IDLE));

  spdlog::info("Disconnected from guider software", deviceId);
}

GuiderInterfaceType GuiderDevice::getInterfaceType() const {
  std::lock_guard<std::mutex> lock(interfaceMutex);
  return interfaceType;
}

std::string GuiderDevice::interfaceTypeToString(GuiderInterfaceType type) {
  switch (type) {
  case GuiderInterfaceType::PHD2:
    return "PHD2";
  case GuiderInterfaceType::LINGUIDER:
    return "Lin-guider";
  case GuiderInterfaceType::METAGUIDE:
    return "MetaGuide";
  case GuiderInterfaceType::DIREKTGUIDER:
    return "DirectGuide";
  case GuiderInterfaceType::ASTROPHOTOGRAPHY_TOOL:
    return "APT";
  case GuiderInterfaceType::KSTARS_EKOS:
    return "KStars/EKOS";
  case GuiderInterfaceType::MAXIM_DL:
    return "MaxIm DL";
  case GuiderInterfaceType::ASTROART:
    return "AstroArt";
  case GuiderInterfaceType::ASTAP:
    return "ASTAP";
  case GuiderInterfaceType::VOYAGER:
    return "Voyager";
  case GuiderInterfaceType::NINA:
    return "N.I.N.A";
  case GuiderInterfaceType::CUSTOM:
    return "Custom";
  default:
    return "Unknown";
  }
}

GuiderInterfaceType
GuiderDevice::stringToInterfaceType(const std::string &typeStr) {
  if (typeStr == "PHD2")
    return GuiderInterfaceType::PHD2;
  if (typeStr == "Lin-guider")
    return GuiderInterfaceType::LINGUIDER;
  if (typeStr == "MetaGuide")
    return GuiderInterfaceType::METAGUIDE;
  if (typeStr == "DirectGuide")
    return GuiderInterfaceType::DIREKTGUIDER;
  if (typeStr == "APT")
    return GuiderInterfaceType::ASTROPHOTOGRAPHY_TOOL;
  if (typeStr == "KStars/EKOS")
    return GuiderInterfaceType::KSTARS_EKOS;
  if (typeStr == "MaxIm DL")
    return GuiderInterfaceType::MAXIM_DL;
  if (typeStr == "AstroArt")
    return GuiderInterfaceType::ASTROART;
  if (typeStr == "ASTAP")
    return GuiderInterfaceType::ASTAP;
  if (typeStr == "Voyager")
    return GuiderInterfaceType::VOYAGER;
  if (typeStr == "N.I.N.A")
    return GuiderInterfaceType::NINA;
  if (typeStr == "Custom")
    return GuiderInterfaceType::CUSTOM;

  return GuiderInterfaceType::PHD2; // 默认
}

std::string GuiderDevice::guiderStateToString(GuiderState state) {
  switch (state) {
  case GuiderState::DISCONNECTED:
    return "DISCONNECTED";
  case GuiderState::CONNECTED:
    return "CONNECTED";
  case GuiderState::CALIBRATING:
    return "CALIBRATING";
  case GuiderState::GUIDING:
    return "GUIDING";
  case GuiderState::PAUSED:
    return "PAUSED";
  case GuiderState::SETTLING:
    return "SETTLING";
  case GuiderState::ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

std::string GuiderDevice::calibrationStateToString(CalibrationState state) {
  switch (state) {
  case CalibrationState::IDLE:
    return "IDLE";
  case CalibrationState::NORTH_MOVING:
    return "NORTH_MOVING";
  case CalibrationState::NORTH_COMPLETE:
    return "NORTH_COMPLETE";
  case CalibrationState::SOUTH_MOVING:
    return "SOUTH_MOVING";
  case CalibrationState::SOUTH_COMPLETE:
    return "SOUTH_COMPLETE";
  case CalibrationState::EAST_MOVING:
    return "EAST_MOVING";
  case CalibrationState::EAST_COMPLETE:
    return "EAST_COMPLETE";
  case CalibrationState::WEST_MOVING:
    return "WEST_MOVING";
  case CalibrationState::WEST_COMPLETE:
    return "WEST_COMPLETE";
  case CalibrationState::COMPLETED:
    return "COMPLETED";
  case CalibrationState::FAILED:
    return "FAILED";
  default:
    return "UNKNOWN";
  }
}

std::shared_ptr<GuiderInterface> GuiderDevice::getInterface() const {
  std::lock_guard<std::mutex> lock(interfaceMutex);
  return guiderInterface;
}

// 状态更新循环
void GuiderDevice::statusUpdateLoop() {
  spdlog::info("Status update loop started", deviceId);

  while (running) {
    {
      std::lock_guard<std::mutex> lock(interfaceMutex);

      // 更新接口状态
      if (guiderInterface) {
        guiderInterface->update();

        // 获取当前状态
        GuiderState currentState = guiderInterface->getGuiderState();
        CalibrationState currentCalState =
            guiderInterface->getCalibrationState();

        // 检测状态变化
        if (currentState != lastState) {
          handleStateChanged(currentState);
          lastState = currentState;
        }

        // 检测校准状态变化
        if (currentCalState != lastCalState) {
          handleCalibrationChanged(currentCalState,
                                   guiderInterface->getCalibrationData());
          lastCalState = currentCalState;
        }

        // 更新修正信息
        if (currentState == GuiderState::GUIDING ||
            currentState == GuiderState::SETTLING) {
          handleCorrectionReceived(guiderInterface->getCurrentCorrection());
        }

        // 更新统计信息
        handleStatsUpdated(guiderInterface->getStats());
      }
    }

    // 间隔
    std::this_thread::sleep_for(
        std::chrono::milliseconds(statusUpdateInterval));
  }

  spdlog::info("Status update loop ended", deviceId);
}

void GuiderDevice::handleStateChanged(GuiderState newState) {
  // 更新状态属性
  setProperty("state", guiderStateToString(newState));

  // 发送状态变更事件
  EventMessage event("GUIDER_STATE_CHANGED");
  event.setDetails({{"state", guiderStateToString(newState)}});
  sendEvent(event);

  spdlog::info("Guider state changed to " + guiderStateToString(newState),
               deviceId);
}

void GuiderDevice::handleCorrectionReceived(
    const GuidingCorrection &correction) {
  // 更新修正属性
  setProperty("raCorrection", correction.raCorrection);
  setProperty("decCorrection", correction.decCorrection);
  setProperty("raRaw", correction.raRaw);
  setProperty("decRaw", correction.decRaw);

  // 只有在值有显著变化时才发送事件，以避免事件风暴
  static GuidingCorrection lastEventCorrection;
  static int correctionCounter = 0;

  double diffRaRaw = std::abs(correction.raRaw - lastEventCorrection.raRaw);
  double diffDecRaw = std::abs(correction.decRaw - lastEventCorrection.decRaw);

  // 每10帧发送一次，或者当误差变化显著时
  if (correctionCounter % 10 == 0 || diffRaRaw > 0.5 || diffDecRaw > 0.5) {
    EventMessage event("GUIDER_CORRECTION");
    event.setDetails({{"raCorrection", correction.raCorrection},
                      {"decCorrection", correction.decCorrection},
                      {"raRaw", correction.raRaw},
                      {"decRaw", correction.decRaw}});
    sendEvent(event);

    lastEventCorrection = correction;
  }

  correctionCounter++;
}

void GuiderDevice::handleCalibrationChanged(CalibrationState newState,
                                            const CalibrationData &data) {
  // 更新校准状态属性
  setProperty("calibrationState", calibrationStateToString(newState));

  if (newState == CalibrationState::COMPLETED) {
    // 校准完成，更新校准数据
    setProperty("calibrated", true);
    setProperty("raAngle", data.raAngle);
    setProperty("decAngle", data.decAngle);
    setProperty("raRate", data.raRate);
    setProperty("decRate", data.decRate);
    setProperty("flipped", data.flipped);

    // 发送校准完成事件
    EventMessage event("CALIBRATION_COMPLETED");
    event.setDetails({{"raAngle", data.raAngle},
                      {"decAngle", data.decAngle},
                      {"raRate", data.raRate},
                      {"decRate", data.decRate},
                      {"flipped", data.flipped}});
    sendEvent(event);
  } else if (newState == CalibrationState::FAILED) {
    // 校准失败
    setProperty("calibrated", false);

    // 发送校准失败事件
    EventMessage event("CALIBRATION_FAILED");
    sendEvent(event);
  }

  // 发送校准状态变更事件
  EventMessage event("CALIBRATION_STATE_CHANGED");
  event.setDetails({{"state", calibrationStateToString(newState)}});
  sendEvent(event);

  spdlog::info("Calibration state changed to " +
                   calibrationStateToString(newState),
               deviceId);
}

void GuiderDevice::handleStatsUpdated(const GuiderStats &newStats) {
  // 更新统计属性
  setProperty("rms", newStats.rms);
  setProperty("rmsRa", newStats.rmsRa);
  setProperty("rmsDec", newStats.rmsDec);
  setProperty("peak", newStats.peak);
  setProperty("frames", newStats.totalFrames);
  setProperty("snr", newStats.snr);

  // 只在显著变化时发送事件
  static int statsCounter = 0;
  if (statsCounter % this->statusUpdateInterval / 100 == 0) { // 大约每5秒一次
    EventMessage event("GUIDER_STATS");
    event.setDetails({{"rms", newStats.rms},
                      {"rmsRa", newStats.rmsRa},
                      {"rmsDec", newStats.rmsDec},
                      {"peak", newStats.peak},
                      {"frames", newStats.totalFrames},
                      {"snr", newStats.snr}});
    sendEvent(event);
  }
  statsCounter++;
}

// 命令处理器

void GuiderDevice::handleConnectCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("type")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "MISSING_PARAMETER"},
                         {"message", "Missing required parameter 'type'"}});
    return;
  }

  std::string typeStr = params["type"];
  GuiderInterfaceType type = stringToInterfaceType(typeStr);

  std::string host = params.value("host", "localhost");
  int port = params.value("port", 4400); // PHD2默认端口

  bool success = connectToGuider(type, host, port);

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails(
        {{"message", "Connected to " + typeStr},
         {"state", guiderStateToString(guiderInterface->getGuiderState())}});
  } else {
    response.setStatus("ERROR");
    response.setDetails({{"error", "CONNECTION_FAILED"},
                         {"message", "Failed to connect to " + typeStr}});
  }
}

void GuiderDevice::handleDisconnectCommand(const CommandMessage &cmd,
                                           ResponseMessage &response) {
  disconnectFromGuider();

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Disconnected from guider software"}});
}

void GuiderDevice::handleStartGuidingCommand(const CommandMessage &cmd,
                                             ResponseMessage &response) {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  if (!guiderInterface || !guiderInterface->isConnected()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "NOT_CONNECTED"},
                         {"message", "Not connected to guider software"}});
    return;
  }

  bool success = guiderInterface->startGuiding();

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails(
        {{"message", "Guiding started"},
         {"state", guiderStateToString(guiderInterface->getGuiderState())}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "GUIDING_FAILED"}, {"message", "Failed to start guiding"}});
  }
}

void GuiderDevice::handleStopGuidingCommand(const CommandMessage &cmd,
                                            ResponseMessage &response) {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  if (!guiderInterface || !guiderInterface->isConnected()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "NOT_CONNECTED"},
                         {"message", "Not connected to guider software"}});
    return;
  }

  bool success = guiderInterface->stopGuiding();

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"message", "Guiding stopped"}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "STOP_FAILED"}, {"message", "Failed to stop guiding"}});
  }
}

void GuiderDevice::handlePauseGuidingCommand(const CommandMessage &cmd,
                                             ResponseMessage &response) {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  if (!guiderInterface || !guiderInterface->isConnected()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "NOT_CONNECTED"},
                         {"message", "Not connected to guider software"}});
    return;
  }

  bool success = guiderInterface->pauseGuiding();

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"message", "Guiding paused"}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "PAUSE_FAILED"}, {"message", "Failed to pause guiding"}});
  }
}

void GuiderDevice::handleResumeGuidingCommand(const CommandMessage &cmd,
                                              ResponseMessage &response) {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  if (!guiderInterface || !guiderInterface->isConnected()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "NOT_CONNECTED"},
                         {"message", "Not connected to guider software"}});
    return;
  }

  bool success = guiderInterface->resumeGuiding();

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"message", "Guiding resumed"}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "RESUME_FAILED"}, {"message", "Failed to resume guiding"}});
  }
}

void GuiderDevice::handleStartCalibrationCommand(const CommandMessage &cmd,
                                                 ResponseMessage &response) {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  if (!guiderInterface || !guiderInterface->isConnected()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "NOT_CONNECTED"},
                         {"message", "Not connected to guider software"}});
    return;
  }

  bool success = guiderInterface->startCalibration();

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails(
        {{"message", "Calibration started"},
         {"calibrationState",
          calibrationStateToString(guiderInterface->getCalibrationState())}});
  } else {
    response.setStatus("ERROR");
    response.setDetails({{"error", "CALIBRATION_FAILED"},
                         {"message", "Failed to start calibration"}});
  }
}

void GuiderDevice::handleCancelCalibrationCommand(const CommandMessage &cmd,
                                                  ResponseMessage &response) {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  if (!guiderInterface || !guiderInterface->isConnected()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "NOT_CONNECTED"},
                         {"message", "Not connected to guider software"}});
    return;
  }

  bool success = guiderInterface->cancelCalibration();

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"message", "Calibration cancelled"}});
  } else {
    response.setStatus("ERROR");
    response.setDetails({{"error", "CANCEL_FAILED"},
                         {"message", "Failed to cancel calibration"}});
  }
}

void GuiderDevice::handleDitherCommand(const CommandMessage &cmd,
                                       ResponseMessage &response) {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  if (!guiderInterface || !guiderInterface->isConnected()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "NOT_CONNECTED"},
                         {"message", "Not connected to guider software"}});
    return;
  }

  json params = cmd.getParameters();

  if (!params.contains("amount")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "MISSING_PARAMETER"},
                         {"message", "Missing required parameter 'amount'"}});
    return;
  }

  double amount = params["amount"];
  double settleTime = params.value("settleTime", 5.0);
  double settlePixels = params.value("settlePixels", 1.5);

  bool success = guiderInterface->dither(amount, settleTime, settlePixels);

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"message", "Dither initiated"},
                         {"amount", amount},
                         {"settleTime", settleTime},
                         {"settlePixels", settlePixels}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "DITHER_FAILED"}, {"message", "Failed to initiate dither"}});
  }
}

void GuiderDevice::handleSetParametersCommand(const CommandMessage &cmd,
                                              ResponseMessage &response) {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  if (!guiderInterface || !guiderInterface->isConnected()) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "NOT_CONNECTED"},
                         {"message", "Not connected to guider software"}});
    return;
  }

  json params = cmd.getParameters();
  json updated = json::object();

  if (params.contains("pixelScale")) {
    double scale = params["pixelScale"];
    guiderInterface->setPixelScale(scale);
    updated["pixelScale"] = scale;
  }

  if (params.contains("raGuideRate")) {
    double raRate = params["raGuideRate"];

    if (params.contains("decGuideRate")) {
      double decRate = params["decGuideRate"];
      guiderInterface->setGuideRate(raRate, decRate);
      updated["raGuideRate"] = raRate;
      updated["decGuideRate"] = decRate;
    }
  }

  if (params.contains("statusUpdateInterval")) {
    int interval = params["statusUpdateInterval"];
    if (interval >= 100 && interval <= 5000) {
      statusUpdateInterval = interval;
      updated["statusUpdateInterval"] = interval;
    }
  }

  response.setStatus("SUCCESS");
  response.setDetails(
      {{"message", "Parameters updated"}, {"updated", updated}});
}

void GuiderDevice::handleGetStatusCommand(const CommandMessage &cmd,
                                          ResponseMessage &response) {
  std::lock_guard<std::mutex> lock(interfaceMutex);

  json status = json::object();

  if (guiderInterface && guiderInterface->isConnected()) {
    GuiderState state = guiderInterface->getGuiderState();
    CalibrationState calState = guiderInterface->getCalibrationState();
    GuiderStats stats = guiderInterface->getStats();
    StarInfo star = guiderInterface->getGuideStar();
    CalibrationData cal = guiderInterface->getCalibrationData();
    GuidingCorrection corr = guiderInterface->getCurrentCorrection();

    status = {{"connected", true},
              {"interfaceType", interfaceTypeToString(interfaceType)},
              {"state", guiderStateToString(state)},
              {"calibrationState", calibrationStateToString(calState)},
              {"calibrated", cal.calibrated},
              {"stats",
               {{"rms", stats.rms},
                {"rmsRa", stats.rmsRa},
                {"rmsDec", stats.rmsDec},
                {"peak", stats.peak},
                {"frames", stats.totalFrames},
                {"snr", stats.snr},
                {"elapsedTime", stats.elapsedTime}}},
              {"star",
               {{"x", star.x},
                {"y", star.y},
                {"flux", star.flux},
                {"snr", star.snr},
                {"locked", star.locked}}},
              {"calibration",
               {{"raAngle", cal.raAngle},
                {"decAngle", cal.decAngle},
                {"raRate", cal.raRate},
                {"decRate", cal.decRate},
                {"flipped", cal.flipped}}},
              {"correction",
               {{"raCorrection", corr.raCorrection},
                {"decCorrection", corr.decCorrection},
                {"raRaw", corr.raRaw},
                {"decRaw", corr.decRaw}}}};
  } else {
    status = {{"connected", false},
              {"interfaceType", interfaceTypeToString(interfaceType)},
              {"state", guiderStateToString(GuiderState::DISCONNECTED)}};
  }

  response.setStatus("SUCCESS");
  response.setDetails(status);
}

} // namespace astrocomm
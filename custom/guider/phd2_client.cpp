#include "phd2_client.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

namespace http = boost::beast::http;

namespace astrocomm {

// PHD2WebSocketClient实现
PHD2WebSocketClient::PHD2WebSocketClient()
    : nextMessageId(1), isRunning(false), connected(false) {}

PHD2WebSocketClient::~PHD2WebSocketClient() { disconnect(); }

bool PHD2WebSocketClient::connect(const std::string &host, int port) {
  if (isConnected()) {
    spdlog::info("Already connected to PHD2", "PHD2WebSocketClient");
    return true;
  }

  // 创建IO上下文
  ioContext = std::make_unique<net::io_context>();

  // 建立连接
  if (!establishConnection(host, port)) {
    return false;
  }

  // 启动消息接收线程
  isRunning = true;
  receiveThread = std::thread([this]() {
    try {
      while (isRunning && connected) {
        asyncReceive();
      }
    } catch (const std::exception &e) {
      spdlog::error("Receive thread error: " + std::string(e.what()),
                    "PHD2WebSocketClient");
      connected = false;
    }
  });

  // 启动消息发送线程
  sendThread = std::thread([this]() {
    try {
      processMessages();
    } catch (const std::exception &e) {
      spdlog::error("Send thread error: " + std::string(e.what()),
                    "PHD2WebSocketClient");
      connected = false;
    }
  });

  return true;
}

void PHD2WebSocketClient::disconnect() {
  if (!isConnected()) {
    return;
  }

  isRunning = false;

  // 通知消息线程结束
  {
    std::lock_guard<std::mutex> lock(messageMutex);
    messageCV.notify_all();
  }

  // 等待线程结束
  if (sendThread.joinable()) {
    sendThread.join();
  }

  if (receiveThread.joinable()) {
    receiveThread.join();
  }

  // 关闭WebSocket连接
  try {
    if (ws && ws->is_open()) {
      ws->close(websocket::close_code::normal);
    }
  } catch (const std::exception &e) {
    spdlog::error("Error closing WebSocket: " + std::string(e.what()),
                  "PHD2WebSocketClient");
  }

  // 清理资源
  ws.reset();
  ioContext.reset();

  // 重置状态
  connected = false;

  // 清空处理器映射
  {
    std::lock_guard<std::mutex> lock(responseHandlersMutex);
    responseHandlers.clear();
  }

  {
    std::lock_guard<std::mutex> lock(eventHandlersMutex);
    eventHandlers.clear();
  }

  spdlog::info("Disconnected from PHD2", "PHD2WebSocketClient");
}

bool PHD2WebSocketClient::isConnected() const {
  return connected && ws && ws->is_open();
}

void PHD2WebSocketClient::sendMessage(const std::string &message,
                                      PHD2ResponseHandler responseHandler) {
  if (!isConnected()) {
    if (responseHandler) {
      responseHandler(false, {{"error", "Not connected to PHD2"}});
    }
    return;
  }

  // 将消息和处理器添加到队列
  std::lock_guard<std::mutex> lock(messageMutex);
  messageQueue.push(std::make_pair(message, responseHandler));
  messageCV.notify_one();
}

void PHD2WebSocketClient::addEventHandler(const std::string &event,
                                          PHD2EventHandler handler) {
  if (!handler)
    return;

  std::lock_guard<std::mutex> lock(eventHandlersMutex);
  eventHandlers[event] = handler;
}

void PHD2WebSocketClient::removeEventHandler(const std::string &event) {
  std::lock_guard<std::mutex> lock(eventHandlersMutex);
  eventHandlers.erase(event);
}

void PHD2WebSocketClient::processMessages() {
  while (isRunning) {
    std::pair<std::string, PHD2ResponseHandler> item;

    // 从队列中获取消息
    {
      std::unique_lock<std::mutex> lock(messageMutex);
      if (messageQueue.empty()) {
        // 等待新消息或停止信号
        messageCV.wait(
            lock, [this]() { return !messageQueue.empty() || !isRunning; });
      }

      if (!isRunning)
        break;
      if (messageQueue.empty())
        continue;

      item = messageQueue.front();
      messageQueue.pop();
    }

    try {
      // 解析消息中的ID，如果有
      int msgId = 0;
      try {
        json msgJson = json::parse(item.first);
        if (msgJson.contains("id")) {
          msgId = msgJson["id"];
        }

        // 存储响应处理器
        if (item.second) {
          std::lock_guard<std::mutex> lock(responseHandlersMutex);
          responseHandlers[msgId] = item.second;
        }
      } catch (const std::exception &e) {
        spdlog::error("Error parsing message ID: " + std::string(e.what()),
                      "PHD2WebSocketClient");
      }

      // 发送消息
      if (ws && ws->is_open()) {
        ws->write(net::buffer(item.first));
        spdlog::debug("Sent to PHD2: " + item.first, "PHD2WebSocketClient");
      } else if (item.second) {
        item.second(false, {{"error", "WebSocket not connected"}});

        std::lock_guard<std::mutex> lock(responseHandlersMutex);
        responseHandlers.erase(msgId);
      }
    } catch (const std::exception &e) {
      spdlog::error("Error sending message: " + std::string(e.what()),
                    "PHD2WebSocketClient");

      // 调用错误处理器
      if (item.second) {
        item.second(false, {{"error", std::string("Send error: ") + e.what()}});
      }
    }
  }
}

void PHD2WebSocketClient::asyncReceive() {
  if (!ws || !ws->is_open()) {
    connected = false;
    return;
  }

  try {
    beast::flat_buffer buffer;
    ws->read(buffer);
    std::string message = beast::buffers_to_string(buffer.data());
    handleMessage(message);
  } catch (const beast::system_error &se) {
    if (se.code() != websocket::error::closed) {
      spdlog::error("WebSocket error: " + se.code().message(),
                    "PHD2WebSocketClient");
    }
    connected = false;
  } catch (const std::exception &e) {
    spdlog::error("Receive error: " + std::string(e.what()),
                  "PHD2WebSocketClient");
    connected = false;
  }
}

void PHD2WebSocketClient::handleMessage(const std::string &message) {
  spdlog::debug("Received from PHD2: " + message, "PHD2WebSocketClient");

  try {
    json msgJson = json::parse(message);

    // 检查是否是事件
    if (msgJson.contains("Event")) {
      std::string eventName = msgJson["Event"];

      // 查找对应的事件处理器
      PHD2EventHandler handler = nullptr;
      {
        std::lock_guard<std::mutex> lock(eventHandlersMutex);
        auto it = eventHandlers.find(eventName);
        if (it != eventHandlers.end()) {
          handler = it->second;
        }
      }

      // 如果有处理器，调用它
      if (handler) {
        handler(msgJson);
      }
    }
    // 检查是否是响应
    else if (msgJson.contains("id") && msgJson.contains("jsonrpc")) {
      int id = msgJson["id"];

      // 查找对应的响应处理器
      PHD2ResponseHandler handler = nullptr;
      {
        std::lock_guard<std::mutex> lock(responseHandlersMutex);
        auto it = responseHandlers.find(id);
        if (it != responseHandlers.end()) {
          handler = it->second;
          responseHandlers.erase(it); // 处理一次后移除
        }
      }

      // 如果有处理器，调用它
      if (handler) {
        bool success = !msgJson.contains("error");
        handler(success, msgJson);
      }
    }
  } catch (const std::exception &e) {
    spdlog::error("Error parsing message: " + std::string(e.what()),
                  "PHD2WebSocketClient");
  }
}

bool PHD2WebSocketClient::establishConnection(const std::string &host,
                                              int port) {
  try {
    // 解析地址
    tcp::resolver resolver(*ioContext);
    auto results = resolver.resolve(host, std::to_string(port));

    // 创建WebSocket流
    ws = std::make_unique<websocket::stream<tcp::socket>>(*ioContext);

    // 连接到终结点
    net::connect(ws->next_layer(), results.begin(), results.end());

    // 设置WebSocket选项
    ws->set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::client));

    ws->set_option(
        websocket::stream_base::decorator([](websocket::request_type &req) {
          req.set(http::field::user_agent, "AstroComm/PHD2Client");
        }));

    // 执行WebSocket握手
    ws->handshake(host, "/");

    connected = true;
    spdlog::info("Connected to PHD2 at " + host + ":" + std::to_string(port),
                 "PHD2WebSocketClient");
    return true;
  } catch (const std::exception &e) {
    spdlog::error("Connection error: " + std::string(e.what()),
                  "PHD2WebSocketClient");
    connected = false;
    return false;
  }
}

// PHD2接口实现
PHD2Interface::PHD2Interface()
    : state(GuiderState::DISCONNECTED), calState(CalibrationState::IDLE),
      host("localhost"), port(4400), operationCompleted(false),
      isSettling(false) {

  // 初始化事件处理器
  setupEventHandlers();
}

PHD2Interface::~PHD2Interface() { disconnect(); }

bool PHD2Interface::connect(const std::string &host, int port) {
  if (isConnected()) {
    SPDLOG_INFO("Already connected to PHD2");
    return true;
  }

  // 保存连接参数
  this->host = host;
  this->port = port;

  // 连接到PHD2
  if (!phd2Client.connect(host, port)) {
    return false;
  }

  // 向PHD2请求当前状态
  json result;
  if (!executeCommand("get_app_state", {}, result)) {
    phd2Client.disconnect();
    return false;
  }

  // 解析状态
  std::string appState;
  if (result.contains("result")) {
    appState = result["result"];
    state = phd2AppStateToGuiderState(appState);
  } else {
    phd2Client.disconnect();
    return false;
  }

  // 如果已经校准，请求校准数据
  if (state == GuiderState::GUIDING || state == GuiderState::PAUSED) {
    updateCalibrationData();
  }

  SPDLOG_INFO("Connected to PHD2, current state: {}", appState);
  return true;
}

void PHD2Interface::disconnect() {
  phd2Client.disconnect();

  std::lock_guard<std::mutex> lock(stateMutex);
  state = GuiderState::DISCONNECTED;
  calState = CalibrationState::IDLE;
}

bool PHD2Interface::isConnected() const { return phd2Client.isConnected(); }

bool PHD2Interface::startGuiding() {
  if (!isConnected()) {
    SPDLOG_WARN("Cannot start guiding: not connected to PHD2");
    return false;
  }

  if (state == GuiderState::GUIDING) {
    SPDLOG_INFO("Already guiding");
    return true;
  }

  // 设置等待标志
  {
    std::lock_guard<std::mutex> lock(waitMutex);
    operationCompleted = false;
  }

  // 发送开始导星命令
  json params = {{"settle", {{"pixels", 1.5}, {"time", 8}, {"timeout", 60}}},
                 {"recalibrate", false}};

  bool success = false;
  json result;
  success = executeCommand("guide", params, result);

  if (!success) {
    return false;
  }

  // 等待导星开始
  {
    std::unique_lock<std::mutex> lock(waitMutex);
    if (!operationCompleted) {
      // 最多等待10秒
      if (!waitCV.wait_for(lock, std::chrono::seconds(10),
                           [this]() { return operationCompleted; })) {
        spdlog::warn("Timeout waiting for guiding to start", "PHD2Interface");
      }
    }
  }

  return state == GuiderState::GUIDING;
}

bool PHD2Interface::stopGuiding() {
  if (!isConnected()) {
    spdlog::warn("Cannot stop guiding: not connected to PHD2", "PHD2Interface");
    return false;
  }

  if (state != GuiderState::GUIDING && state != GuiderState::PAUSED &&
      state != GuiderState::SETTLING) {
    spdlog::info("Not guiding, nothing to stop", "PHD2Interface");
    return true;
  }

  // 设置等待标志
  {
    std::lock_guard<std::mutex> lock(waitMutex);
    operationCompleted = false;
  }

  // 发送停止导星命令
  json result;
  if (!executeCommand("stop_capture", {}, result)) {
    return false;
  }

  // 等待导星停止
  {
    std::unique_lock<std::mutex> lock(waitMutex);
    if (!operationCompleted) {
      // 最多等待5秒
      if (!waitCV.wait_for(lock, std::chrono::seconds(5),
                           [this]() { return operationCompleted; })) {
        spdlog::warn("Timeout waiting for guiding to stop", "PHD2Interface");
      }
    }
  }

  // 导星成功停止
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    state = GuiderState::CONNECTED;
  }

  return true;
}

bool PHD2Interface::pauseGuiding() {
  if (!isConnected()) {
    spdlog::warn("Cannot pause guiding: not connected to PHD2",
                 "PHD2Interface");
    return false;
  }

  if (state != GuiderState::GUIDING) {
    spdlog::warn("Not guiding, cannot pause", "PHD2Interface");
    return false;
  }

  // 设置等待标志
  {
    std::lock_guard<std::mutex> lock(waitMutex);
    operationCompleted = false;
  }

  // 发送暂停命令
  json params = {{"paused", true}};
  json result;
  if (!executeCommand("set_paused", params, result)) {
    return false;
  }

  // 等待暂停完成
  {
    std::unique_lock<std::mutex> lock(waitMutex);
    if (!operationCompleted) {
      // 最多等待5秒
      if (!waitCV.wait_for(lock, std::chrono::seconds(5),
                           [this]() { return operationCompleted; })) {
        spdlog::warn("Timeout waiting for guiding to pause", "PHD2Interface");
      }
    }
  }

  return state == GuiderState::PAUSED;
}

bool PHD2Interface::resumeGuiding() {
  if (!isConnected()) {
    spdlog::warn("Cannot resume guiding: not connected to PHD2",
                 "PHD2Interface");
    return false;
  }

  if (state != GuiderState::PAUSED) {
    spdlog::warn("Not paused, cannot resume", "PHD2Interface");
    return false;
  }

  // 设置等待标志
  {
    std::lock_guard<std::mutex> lock(waitMutex);
    operationCompleted = false;
  }

  // 发送恢复命令
  json params = {{"paused", false}};
  json result;
  if (!executeCommand("set_paused", params, result)) {
    return false;
  }

  // 等待恢复完成
  {
    std::unique_lock<std::mutex> lock(waitMutex);
    if (!operationCompleted) {
      // 最多等待5秒
      if (!waitCV.wait_for(lock, std::chrono::seconds(5),
                           [this]() { return operationCompleted; })) {
        spdlog::warn("Timeout waiting for guiding to resume", "PHD2Interface");
      }
    }
  }

  return state == GuiderState::GUIDING;
}

bool PHD2Interface::startCalibration() {
  if (!isConnected()) {
    spdlog::warn("Cannot start calibration: not connected to PHD2",
                 "PHD2Interface");
    return false;
  }

  if (state == GuiderState::CALIBRATING) {
    spdlog::info("Calibration already in progress", "PHD2Interface");
    return true;
  }

  // 设置等待标志
  {
    std::lock_guard<std::mutex> lock(waitMutex);
    operationCompleted = false;
    calibrationStep = "";
  }

  // 先清除现有校准
  json result;
  if (!executeCommand("clear_calibration", {}, result)) {
    return false;
  }

  // 然后启动新校准
  json params = {{"recalibrate", true}};
  if (!executeCommand("guide", params, result)) {
    return false;
  }

  // 等待校准开始
  {
    std::unique_lock<std::mutex> lock(waitMutex);
    if (!operationCompleted) {
      // 最多等待10秒
      if (!waitCV.wait_for(lock, std::chrono::seconds(10),
                           [this]() { return operationCompleted; })) {
        spdlog::warn("Timeout waiting for calibration to start",
                     "PHD2Interface");
      }
    }
  }

  return state == GuiderState::CALIBRATING;
}

bool PHD2Interface::cancelCalibration() {
  return stopGuiding(); // PHD2使用相同命令停止校准和导星
}

bool PHD2Interface::dither(double amount, double settleTime,
                           double settlePixels) {
  if (!isConnected()) {
    spdlog::warn("Cannot dither: not connected to PHD2", "PHD2Interface");
    return false;
  }

  if (state != GuiderState::GUIDING) {
    spdlog::warn("Cannot dither: not guiding", "PHD2Interface");
    return false;
  }

  // 设置等待标志
  {
    std::lock_guard<std::mutex> lock(waitMutex);
    operationCompleted = false;
    isSettling = true;
  }

  // 发送抖动命令
  json params = {{"amount", amount},
                 {"raOnly", false},
                 {"settle",
                  {{"pixels", settlePixels},
                   {"time", settleTime},
                   {"timeout", settleTime * 3}}}};

  json result;
  if (!executeCommand("dither", params, result)) {
    isSettling = false;
    return false;
  }

  // 不等待完成，PHD2会发送事件通知
  return true;
}

GuiderState PHD2Interface::getGuiderState() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return state;
}

CalibrationState PHD2Interface::getCalibrationState() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return calState;
}

GuiderStats PHD2Interface::getStats() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return stats;
}

StarInfo PHD2Interface::getGuideStar() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return guideStar;
}

CalibrationData PHD2Interface::getCalibrationData() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return calibData;
}

void PHD2Interface::setPixelScale(double scaleArcsecPerPixel) {
  SPDLOG_INFO("Pixel scale set to {:.2f} arcsec/pixel (note: PHD2 uses "
              "calibration to determine scale)",
              scaleArcsecPerPixel);
}

void PHD2Interface::setGuideRate(double raRateMultiplier,
                                 double decRateMultiplier) {
  SPDLOG_INFO("Guide rates set to RA: {:.2f}, Dec: {:.2f} (note: must be "
              "configured in mount settings in PHD2)",
              raRateMultiplier, decRateMultiplier);
}

GuidingCorrection PHD2Interface::getCurrentCorrection() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return lastCorrection;
}

GuiderInterfaceType PHD2Interface::getInterfaceType() const {
  return GuiderInterfaceType::PHD2;
}

std::string PHD2Interface::getInterfaceName() const { return "PHD2"; }

void PHD2Interface::update() {
  // 检查连接是否仍然有效
  if (!phd2Client.isConnected()) {
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      if (state != GuiderState::DISCONNECTED) {
        state = GuiderState::DISCONNECTED;
        notifyStatusChanged();
      }
    }
    return;
  }

  // 距离最后通信超过一段时间，就更新一下星点位置
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::seconds>(now -
                                                       lastCommunicationTime)
          .count() > 10) {
    updateStarInfo();
    lastCommunicationTime = now;
  }
}

void PHD2Interface::setupEventHandlers() {
  // 设置各种事件处理器
  phd2Client.addEventHandler(
      "AppState", [this](const json &event) { handleAppStateEvent(event); });

  phd2Client.addEventHandler("CalibrationComplete", [this](const json &event) {
    handleCalibrationCompleteEvent(event);
  });

  phd2Client.addEventHandler("CalibrationFailed", [this](const json &event) {
    handleCalibrationFailedEvent(event);
  });

  phd2Client.addEventHandler(
      "GuideStep", [this](const json &event) { handleGuideStepEvent(event); });

  phd2Client.addEventHandler("SettleBegin", [this](const json &event) {
    handleSettleBeginEvent(event);
  });

  phd2Client.addEventHandler("SettleDone", [this](const json &event) {
    handleSettleDoneEvent(event);
  });

  phd2Client.addEventHandler(
      "StarLost", [this](const json &event) { handleStarLostEvent(event); });

  phd2Client.addEventHandler("GuidingDithered", [this](const json &event) {
    handleGuidingDitheredEvent(event);
  });

  phd2Client.addEventHandler(
      "Alert", [this](const json &event) { handleAlertEvent(event); });
}

bool PHD2Interface::executeCommand(const std::string &method,
                                   const json &params, json &result) {
  if (!isConnected()) {
    spdlog::warn("Cannot execute command: not connected to PHD2",
                 "PHD2Interface");
    return false;
  }

  int msgId = std::rand(); // 使用随机ID
  json request = {{"method", method}, {"id", msgId}, {"jsonrpc", "2.0"}};

  if (!params.is_null()) {
    request["params"] = params;
  }

  std::string requestStr = request.dump();

  // 标记为未完成
  {
    std::lock_guard<std::mutex> lock(waitMutex);
    operationCompleted = false;
    completionResult = "";
  }

  // 同步等待完成的条件变量
  std::condition_variable responseCV;
  bool receivedResponse = false;
  json responseResult;

  // 发送请求并设置完成处理器
  phd2Client.sendMessage(requestStr, [&](bool success, const json &response) {
    std::lock_guard<std::mutex> lock(waitMutex);
    receivedResponse = true;
    responseResult = response;
    responseCV.notify_all();
  });

  // 等待响应或超时
  {
    std::unique_lock<std::mutex> lock(waitMutex);
    if (!receivedResponse) {
      // 最多等待10秒
      if (!responseCV.wait_for(lock, std::chrono::seconds(10),
                               [&]() { return receivedResponse; })) {
        // 超时
        spdlog::warn("Timeout waiting for PHD2 response to " + method,
                     "PHD2Interface");
        return false;
      }
    }
  }

  // 连接丢失
  if (!isConnected()) {
    spdlog::warn("Connection lost while waiting for response to " + method,
                 "PHD2Interface");
    return false;
  }

  // 解析响应
  if (responseResult.contains("error")) {
    // 命令错误
    std::string errorMsg = "Unknown error";
    if (responseResult["error"].contains("message")) {
      errorMsg = responseResult["error"]["message"];
    }

    spdlog::error("PHD2 command error: " + errorMsg, "PHD2Interface");
    return false;
  }

  result = responseResult;
  lastCommunicationTime = std::chrono::steady_clock::now(); // 更新最后通信时间
  return true;
}

GuiderState
PHD2Interface::phd2AppStateToGuiderState(const std::string &appState) const {
  if (appState == "Stopped")
    return GuiderState::CONNECTED;
  if (appState == "Selected")
    return GuiderState::CONNECTED;
  if (appState == "Calibrating")
    return GuiderState::CALIBRATING;
  if (appState == "Guiding")
    return GuiderState::GUIDING;
  if (appState == "Paused")
    return GuiderState::PAUSED;
  if (appState == "Looping")
    return GuiderState::CONNECTED;

  return GuiderState::CONNECTED;
}

void PHD2Interface::updateCalibrationData() {
  // 请求校准详情
  json result;
  if (!executeCommand("get_calibration_data", {}, result)) {
    return;
  }

  // 解析校准数据
  if (result.contains("result")) {
    json calData = result["result"];

    std::lock_guard<std::mutex> lock(stateMutex);

    calibData.calibrated = true;

    if (calData.contains("xAngle")) {
      calibData.raAngle = calData["xAngle"];
    }

    if (calData.contains("yAngle")) {
      calibData.decAngle = calData["yAngle"];
    }

    if (calData.contains("xRate")) {
      calibData.raRate = calData["xRate"];
    }

    if (calData.contains("yRate")) {
      calibData.decRate = calData["yRate"];
    }

    if (calData.contains("decFlipped")) {
      calibData.flipped = calData["decFlipped"];
    }

    SPDLOG_INFO("Updated calibration data: RA angle={:.2f}, DEC angle={:.2f}",
                calibData.raAngle, calibData.decAngle);
  } else {
    std::lock_guard<std::mutex> lock(stateMutex);
    calibData.calibrated = false;
  }
}

void PHD2Interface::updateStarInfo() {
  // 请求当前星体位置
  json result;
  if (!executeCommand("get_star_position", {}, result)) {
    return;
  }

  // 解析星体位置
  if (result.contains("result")) {
    json starPos = result["result"];

    std::lock_guard<std::mutex> lock(stateMutex);

    if (starPos.contains("X")) {
      guideStar.x = starPos["X"];
    }

    if (starPos.contains("Y")) {
      guideStar.y = starPos["Y"];
    }

    guideStar.locked = true; // PHD2找到星体后认为它是锁定的
  } else {
    // 没有锁定星体
    std::lock_guard<std::mutex> lock(stateMutex);
    guideStar.locked = false;
  }
}

void PHD2Interface::notifyStatusChanged() {
  try {
    std::vector<std::function<void()>> listenersToCall;

    {
      std::lock_guard<std::mutex> lock(listenersMutex);
      listenersToCall = statusListeners;
    }

    for (const auto &listener : listenersToCall) {
      try {
        listener();
      } catch (const std::exception &e) {
        spdlog::error("Error in status listener: " + std::string(e.what()),
                      "PHD2Interface");
      }
    }
  } catch (const std::exception &e) {
    spdlog::error("Error notifying status change: " + std::string(e.what()),
                  "PHD2Interface");
  }
}

void PHD2Interface::handleError(const std::string &errorMsg) {
  spdlog::error("PHD2 error: " + errorMsg, "PHD2Interface");

  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (state == GuiderState::GUIDING || state == GuiderState::CALIBRATING) {
      state = GuiderState::ERROR;
      notifyStatusChanged();
    }
  }
}

// 事件处理器实现
void PHD2Interface::handleAppStateEvent(const json &event) {
  if (!event.contains("State"))
    return;

  std::string appState = event["State"];
  GuiderState newState = phd2AppStateToGuiderState(appState);

  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (state != newState) {
      state = newState;

      if (state == GuiderState::GUIDING) {
        calState = CalibrationState::COMPLETED; // 导星中意味着已校准

        // 重置统计
        stats = GuiderStats();
      } else if (state == GuiderState::CALIBRATING) {
        calState = CalibrationState::NORTH_MOVING; // 假设从北向开始
        calibData.calibrated = false;
      }

      notifyStatusChanged();

      // 通知操作完成
      std::lock_guard<std::mutex> waitLock(waitMutex);
      operationCompleted = true;
      completionResult = appState;
      waitCV.notify_all();
    }
  }

  SPDLOG_INFO("PHD2 state changed to {}", appState);
}

void PHD2Interface::handleCalibrationCompleteEvent(const json &event) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    calState = CalibrationState::COMPLETED;

    // 解析校准数据
    if (event.contains("calibration")) {
      json calData = event["calibration"];

      calibData.calibrated = true;

      if (calData.contains("xAngle")) {
        calibData.raAngle = calData["xAngle"];
      }

      if (calData.contains("yAngle")) {
        calibData.decAngle = calData["yAngle"];
      }

      if (calData.contains("xRate")) {
        calibData.raRate = calData["xRate"];
      }

      if (calData.contains("yRate")) {
        calibData.decRate = calData["yRate"];
      }

      if (calData.contains("decFlipped")) {
        calibData.flipped = calData["decFlipped"];
      }
    }

    notifyStatusChanged();
  }

  // 通知操作完成
  {
    std::lock_guard<std::mutex> lock(waitMutex);
    operationCompleted = true;
    completionResult = "CalibrationComplete";
    waitCV.notify_all();
  }

  spdlog::info("PHD2 calibration completed", "PHD2Interface");
}

void PHD2Interface::handleCalibrationFailedEvent(const json &event) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    calState = CalibrationState::FAILED;
    calibData.calibrated = false;
    state = GuiderState::CONNECTED; // 校准失败后回到连接状态

    notifyStatusChanged();
  }

  // 通知操作完成
  {
    std::lock_guard<std::mutex> lock(waitMutex);
    operationCompleted = true;
    completionResult = "CalibrationFailed";
    waitCV.notify_all();
  }

  std::string errorMsg = event.contains("Reason")
                             ? event["Reason"].get<std::string>()
                             : "Unknown reason";
  SPDLOG_ERROR("PHD2 calibration failed: {}", errorMsg);
}

void PHD2Interface::handleGuideStepEvent(const json &event) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);

    // 更新星体信息
    if (event.contains("Star")) {
      json star = event["Star"];

      if (star.contains("X")) {
        guideStar.x = star["X"];
      }

      if (star.contains("Y")) {
        guideStar.y = star["Y"];
      }

      if (star.contains("SNR")) {
        guideStar.snr = star["SNR"];
      }

      guideStar.locked = true;
    }

    // 更新修正信息
    if (event.contains("Mount")) {
      json mount = event["Mount"];

      if (mount.contains("RADistanceRaw")) {
        lastCorrection.raRaw = mount["RADistanceRaw"];
      }

      if (mount.contains("DecDistanceRaw")) {
        lastCorrection.decRaw = mount["DecDistanceRaw"];
      }

      if (mount.contains("RADuration")) {
        lastCorrection.raCorrection = mount["RADuration"];
      }

      if (mount.contains("DecDuration")) {
        lastCorrection.decCorrection = mount["DecDuration"];
      }

      // 更新统计数据
      double dx = lastCorrection.raRaw;
      double dy = lastCorrection.decRaw;
      double totalError = std::sqrt(dx * dx + dy * dy);

      stats.totalFrames++;

      // 更新RMS (指数移动平均)
      if (stats.totalFrames == 1) {
        stats.rms = totalError;
        stats.rmsRa = std::abs(dx);
        stats.rmsDec = std::abs(dy);
      } else {
        stats.rms = stats.rms * 0.9 + totalError * 0.1;
        stats.rmsRa = stats.rmsRa * 0.9 + std::abs(dx) * 0.1;
        stats.rmsDec = stats.rmsDec * 0.9 + std::abs(dy) * 0.1;
      }

      // 更新峰值误差
      stats.peak = std::max(stats.peak, totalError);

      // 更新信噪比
      stats.snr = guideStar.snr;

      // 更新持续时间
      if (event.contains("Frame") && event["Frame"].contains("Time")) {
        std::string timeStr = event["Frame"]["Time"];
        try {
          // 解析毫秒时间戳
          uint64_t timestamp = std::stoull(timeStr);
          stats.elapsedTime = timestamp / 1000.0; // 转换为秒
        } catch (...) {
          // 忽略解析错误
        }
      }
    }
  }

  lastCommunicationTime = std::chrono::steady_clock::now();
}

void PHD2Interface::handleSettleBeginEvent(const json &event) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    state = GuiderState::SETTLING;
    isSettling = true;
    notifyStatusChanged();
  }

  spdlog::info("PHD2 settle begin", "PHD2Interface");
}

void PHD2Interface::handleSettleDoneEvent(const json &event) {
  bool success = true;
  if (event.contains("Status") && event["Status"].is_number()) {
    success = (event["Status"] == 0);
  }

  {
    std::lock_guard<std::mutex> lock(stateMutex);
    isSettling = false;

    if (!success) {
      std::string error = event.contains("Error")
                              ? event["Error"].get<std::string>()
                              : "Unknown error";
      SPDLOG_WARN("PHD2 settle failed: {}", error);
    }

    if (success) {
      state = GuiderState::GUIDING;
    } else {
      // 稳定失败
      std::string error = "Unknown error";
      if (event.contains("Error")) {
        error = event["Error"];
      }
      spdlog::warn("PHD2 settle failed: " + error, "PHD2Interface");
    }

    notifyStatusChanged();
  }

  // 通知操作完成
  {
    std::lock_guard<std::mutex> lock(waitMutex);
    operationCompleted = true;
    completionResult = success ? "SettleDone" : "SettleFailed";
    waitCV.notify_all();
  }

  SPDLOG_INFO("PHD2 settle {}", success ? "completed successfully" : "failed");
}

void PHD2Interface::handleStarLostEvent(const json &event) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    guideStar.locked = false;

    // 如果正在导星，切换到错误状态
    if (state == GuiderState::GUIDING) {
      state = GuiderState::ERROR;

      std::string status = event.contains("Status")
                               ? event["Status"].get<std::string>()
                               : "Unknown reason";
      SPDLOG_WARN("PHD2 star lost: {}", status);

      notifyStatusChanged();
    }
  }
}

void PHD2Interface::handleGuidingDitheredEvent(const json &event) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    state = GuiderState::SETTLING;
    isSettling = true;
    notifyStatusChanged();
  }

  spdlog::info("PHD2 dithered, waiting for settle", "PHD2Interface");
}

void PHD2Interface::handleAlertEvent(const json &event) {
  std::string msg =
      event.contains("Msg") ? event["Msg"].get<std::string>() : "Unknown alert";
  SPDLOG_WARN("PHD2 alert: {}", msg);
}

} // namespace astrocomm
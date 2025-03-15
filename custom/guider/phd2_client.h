#pragma once
#include "device/guider.h"

#include <atomic>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <condition_variable>
#include <functional>
#include <map>
#include <queue>
#include <thread>

namespace astrocomm {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

// PHD2事件处理器类型
using PHD2EventHandler = std::function<void(const json &)>;

// PHD2响应处理器类型
using PHD2ResponseHandler = std::function<void(bool success, const json &)>;

// PHD2 WebSocket客户端封装
class PHD2WebSocketClient {
public:
  PHD2WebSocketClient();
  ~PHD2WebSocketClient();

  // 连接到PHD2 WebSocket服务器
  bool connect(const std::string &host, int port);

  // 断开连接
  void disconnect();

  // 是否已连接
  bool isConnected() const;

  // 发送消息
  void sendMessage(const std::string &message,
                   PHD2ResponseHandler responseHandler = nullptr);

  // 添加事件处理器
  void addEventHandler(const std::string &event, PHD2EventHandler handler);

  // 移除事件处理器
  void removeEventHandler(const std::string &event);

  // 处理消息循环
  void processMessages();

private:
  // 异步接收消息
  void asyncReceive();

  // 处理接收到的消息
  void handleMessage(const std::string &message);

  // 建立WebSocket连接
  bool establishConnection(const std::string &host, int port);

private:
  // IO Context和WebSocket流
  std::unique_ptr<net::io_context> ioContext;
  std::unique_ptr<websocket::stream<tcp::socket>> ws;

  // 消息ID计数器
  std::atomic<int> nextMessageId;

  // 消息响应处理映射
  std::mutex responseHandlersMutex;
  std::map<int, PHD2ResponseHandler> responseHandlers;

  // 事件处理映射
  std::mutex eventHandlersMutex;
  std::map<std::string, PHD2EventHandler> eventHandlers;

  // 接收线程和运行标志
  std::thread receiveThread;
  std::atomic<bool> isRunning;

  // 连接状态
  std::atomic<bool> connected;

  // 消息队列
  std::mutex messageMutex;
  std::condition_variable messageCV;
  std::queue<std::pair<std::string, PHD2ResponseHandler>> messageQueue;

  // 发送线程
  std::thread sendThread;
};

// PHD2接口实现
class PHD2Interface : public GuiderInterface {
public:
  PHD2Interface();
  virtual ~PHD2Interface();

  // GuiderInterface实现
  virtual bool connect(const std::string &host, int port) override;
  virtual void disconnect() override;
  virtual bool isConnected() const override;
  virtual bool startGuiding() override;
  virtual bool stopGuiding() override;
  virtual bool pauseGuiding() override;
  virtual bool resumeGuiding() override;
  virtual bool startCalibration() override;
  virtual bool cancelCalibration() override;
  virtual bool dither(double amount, double settleTime,
                      double settlePixels) override;
  virtual GuiderState getGuiderState() const override;
  virtual CalibrationState getCalibrationState() const override;
  virtual GuiderStats getStats() const override;
  virtual StarInfo getGuideStar() const override;
  virtual CalibrationData getCalibrationData() const override;
  virtual void setPixelScale(double scaleArcsecPerPixel) override;
  virtual void setGuideRate(double raRateMultiplier,
                            double decRateMultiplier) override;
  virtual GuidingCorrection getCurrentCorrection() const override;
  virtual GuiderInterfaceType getInterfaceType() const override;
  virtual std::string getInterfaceName() const override;
  virtual void update() override;

private:
  // 初始化事件处理器
  void setupEventHandlers();

  // 同步执行PHD2命令
  bool executeCommand(const std::string &method, const json &params,
                      json &result);

  // PHD2 APP状态转换为GuiderState
  GuiderState phd2AppStateToGuiderState(const std::string &appState) const;

  // 请求和更新校准数据
  void updateCalibrationData();

  // 请求当前导星星点信息
  void updateStarInfo();

  // 通知所有监听器
  void notifyStatusChanged();

  // 事件处理器
  void handleAppStateEvent(const json &event);
  void handleCalibrationCompleteEvent(const json &event);
  void handleCalibrationFailedEvent(const json &event);
  void handleGuideStepEvent(const json &event);
  void handleSettleBeginEvent(const json &event);
  void handleSettleDoneEvent(const json &event);
  void handleStarLostEvent(const json &event);
  void handleGuidingDitheredEvent(const json &event);
  void handleAlertEvent(const json &event);

private:
  // PHD2 WebSocket客户端
  PHD2WebSocketClient phd2Client;

  // 状态变量
  mutable std::mutex stateMutex;
  GuiderState state;
  CalibrationState calState;
  GuiderStats stats;
  StarInfo guideStar;
  CalibrationData calibData;
  GuidingCorrection lastCorrection;

  // 连接信息
  std::string host;
  int port;

  // 状态变更通知
  std::mutex listenersMutex;
  std::vector<std::function<void()>> statusListeners;

  // 处理PHD2错误
  void handleError(const std::string &errorMsg);

  // 完成和超时标志/条件变量
  mutable std::mutex waitMutex;
  std::condition_variable waitCV;
  bool operationCompleted;
  std::string completionResult;

  // 最后一次通信时间
  std::chrono::steady_clock::time_point lastCommunicationTime;

  // 校准进度跟踪
  std::string calibrationStep;

  // 抖动稳定跟踪
  bool isSettling;
};

} // namespace astrocomm
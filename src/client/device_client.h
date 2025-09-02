#pragma once

#include "common/message.h"
#include "common/message_queue.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace hydrogen {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

// 设备客户端类 - 用于与服务器和设备通信
class DeviceClient {
public:
  DeviceClient();
  ~DeviceClient();

  // 连接到服务器
  bool connect(const std::string &host, uint16_t port);

  // 断开连接
  void disconnect();

  // 发现设备
  json discoverDevices(const std::vector<std::string> &deviceTypes = {});

  // 获取设备列表
  json getDevices() const;

  // 获取设备属�?
  json getDeviceProperties(const std::string &deviceId,
                           const std::vector<std::string> &properties);

  // 设置设备属�?
  json setDeviceProperties(const std::string &deviceId, const json &properties);

  // 执行设备命令
  json
  executeCommand(const std::string &deviceId, const std::string &command,
                 const json &parameters = json::object(),
                 Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE);

  // 批量执行命令
  json executeBatchCommands(
      const std::string &deviceId,
      const std::vector<std::pair<std::string, json>> &commands,
      bool sequential = true,
      Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE);

  // 异步执行命令（不等待响应�?
  void executeCommandAsync(
      const std::string &deviceId, const std::string &command,
      const json &parameters = json::object(),
      Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE,
      std::function<void(const json &)> callback = nullptr);

  // 发布事件（针对作为设备的客户端使用）
  void publishEvent(const std::string &eventName,
                    const json &details = json::object(),
                    Message::Priority priority = Message::Priority::NORMAL);

  // 设置消息重试参数
  void setMessageRetryParams(int maxRetries, int retryIntervalMs);

  // 设置自动重连参数
  void setAutoReconnect(bool enable, int intervalMs = 5000, int maxAttempts = 0);

  // 订阅设备属性变�?
  using PropertyCallback =
      std::function<void(const std::string &deviceId,
                         const std::string &property, const json &value)>;

  void subscribeToProperty(const std::string &deviceId,
                           const std::string &property,
                           PropertyCallback callback);

  void unsubscribeFromProperty(const std::string &deviceId,
                               const std::string &property);

  // 订阅设备事件
  using EventCallback =
      std::function<void(const std::string &deviceId, const std::string &event,
                         const json &details)>;

  void subscribeToEvent(const std::string &deviceId, const std::string &event,
                        EventCallback callback);

  void unsubscribeFromEvent(const std::string &deviceId,
                            const std::string &event);

  // 认证
  bool authenticate(const std::string &method, const std::string &credentials);

  // 运行消息处理循环（阻塞）
  void run();

  // 启动后台消息处理线程
  void startMessageProcessing();

  // 停止后台消息处理
  void stopMessageProcessing();

  // 获取连接状�?
  bool isConnected() const { return connected; }

  // 获取客户端状态信�?
  json getStatusInfo() const;

private:
  // WebSocket连接
  net::io_context ioc;
  std::unique_ptr<websocket::stream<tcp::socket>> ws;
  bool connected;

  // 设备信息缓存
  mutable std::mutex devicesMutex;
  json devices;

  // 消息处理
  std::mutex threadMutex;
  std::thread message_thread;
  bool running;

  // 响应等待
  std::mutex responseMutex;
  std::condition_variable responseCV;
  std::map<std::string, json> responses;

  // 异步回调处理
  std::mutex callbacksMutex;
  std::map<std::string, std::function<void(const json &)>> asyncCallbacks;

  // 属性和事件订阅
  std::mutex subscriptionsMutex;
  std::map<std::string, std::map<std::string, PropertyCallback>>
      propertySubscriptions;
  std::map<std::string, std::map<std::string, EventCallback>>
      eventSubscriptions;

  // 消息队列管理�?
  std::unique_ptr<MessageQueueManager> messageQueueManager;

  // 连接断开后重�?
  bool enableAutoReconnect{true};
  int reconnectIntervalMs{5000};
  int maxReconnectAttempts{10};
  int reconnectCount{0};
  std::string lastHost;
  uint16_t lastPort{0};
  std::thread reconnectThread;
  std::atomic<bool> reconnecting{false};

  // 重连线程函数
  void reconnectLoop();

  // 尝试重新连接
  bool tryReconnect();

  // 消息处理循环
  void messageLoop();

  // 处理接收到的消息
  void handleMessage(const std::string &message);

  // 处理消息响应
  void handleResponse(const std::string &originalMessageId, const json &responseJson);

  // 处理各类消息
  void handleDiscoveryResponse(const DiscoveryResponseMessage &msg);
  void handleResponseMessage(const ResponseMessage &msg);
  void handleEventMessage(const EventMessage &msg);
  void handleErrorMessage(const ErrorMessage &msg);

  // 发送消息并等待响应
  json sendAndWaitForResponse(const Message &msg, int timeoutSeconds = 10);

  // 发送单个消�?
  bool sendMessage(const Message &msg);

  // 生成设备属性订阅键
  std::string makePropertyKey(const std::string &deviceId,
                              const std::string &property);

  // 生成设备事件订阅�?
  std::string makeEventKey(const std::string &deviceId,
                           const std::string &event);

  // 连接状态变更处�?
  void handleConnectionStateChange(bool connected);

  // 重置内部状�?
  void resetState();
};

} // namespace hydrogen
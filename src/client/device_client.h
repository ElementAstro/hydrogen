#pragma once
#include "common/message.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>

namespace astrocomm {

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

  // 获取设备属性
  json getDeviceProperties(const std::string &deviceId,
                           const std::vector<std::string> &properties);

  // 设置设备属性
  json setDeviceProperties(const std::string &deviceId, const json &properties);

  // 执行设备命令
  json executeCommand(const std::string &deviceId, const std::string &command,
                      const json &parameters = json::object());

  // 批量执行命令
  json executeBatchCommands(
      const std::string &deviceId,
      const std::vector<std::pair<std::string, json>> &commands,
      bool sequential = true);

  // 订阅设备属性变更
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

private:
  // WebSocket连接
  net::io_context ioc;
  std::unique_ptr<websocket::stream<tcp::socket>> ws;
  bool connected;

  // 设备信息缓存
  mutable std::mutex devicesMutex;
  json devices;

  // 消息处理
  std::thread message_thread;
  bool running;

  // 响应等待
  std::mutex responseMutex;
  std::condition_variable responseCV;
  std::map<std::string, json> responses;

  // 属性和事件订阅
  std::mutex subscriptionsMutex;
  std::map<std::string, std::map<std::string, PropertyCallback>>
      propertySubscriptions;
  std::map<std::string, std::map<std::string, EventCallback>>
      eventSubscriptions;

  // 消息处理循环
  void messageLoop();

  // 处理接收到的消息
  void handleMessage(const std::string &message);

  // 处理各类消息
  void handleDiscoveryResponse(const DiscoveryResponseMessage &msg);
  void handleResponseMessage(const ResponseMessage &msg);
  void handleEventMessage(const EventMessage &msg);
  void handleErrorMessage(const ErrorMessage &msg);

  // 发送消息并等待响应
  json sendAndWaitForResponse(const Message &msg, int timeoutSeconds = 10);

  // 生成设备属性订阅键
  std::string makePropertyKey(const std::string &deviceId,
                              const std::string &property);

  // 生成设备事件订阅键
  std::string makeEventKey(const std::string &deviceId,
                           const std::string &event);
};

} // namespace astrocomm
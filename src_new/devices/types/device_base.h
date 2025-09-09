#pragma once
#include "common/message.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace hydrogen {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

// 设备基类，提供基本功能和通信机制
class DeviceBase {
public:
  DeviceBase(const std::string &deviceId, const std::string &deviceType,
             const std::string &manufacturer, const std::string &model);
  virtual ~DeviceBase();

  // 连接到服务器
  virtual bool connect(const std::string &host, uint16_t port);

  // 断开连接
  virtual void disconnect();

  // 注册设备到服务器
  virtual bool registerDevice();

  // 启动设备
  virtual bool start();

  // 停止设备
  virtual void stop();

  // 运行消息循环
  virtual void run();

  // 获取设备ID
  std::string getDeviceId() const;

  // 获取设备类型
  std::string getDeviceType() const;

  // 获取设备信息JSON
  virtual json getDeviceInfo() const;

  // 设置属性�?
  virtual void setProperty(const std::string &property, const json &value);

  // 获取属性�?
  virtual json getProperty(const std::string &property) const;

  // 注册命令处理�?
  using CommandHandler =
      std::function<void(const CommandMessage &, ResponseMessage &)>;
  void registerCommandHandler(const std::string &command,
                              CommandHandler handler);

protected:
  // 处理收到的消�?
  virtual void handleMessage(const std::string &message);

  // 处理命令消息
  virtual void handleCommandMessage(const CommandMessage &cmd);

  // 发送响应消�?
  virtual void sendResponse(const ResponseMessage &response);

  // 发送事件通知
  virtual void sendEvent(const EventMessage &event);

  // 发送属性变更事�?
  virtual void sendPropertyChangedEvent(const std::string &property,
                                        const json &value,
                                        const json &previousValue);

  // 状态和属性管�?
  std::string deviceId;
  std::string deviceType;
  std::string manufacturer;
  std::string model;
  std::string firmwareVersion;

  // 属性存�?
  mutable std::mutex propertiesMutex;
  std::unordered_map<std::string, json> properties;
  std::vector<std::string> capabilities;

  // WebSocket连接
  net::io_context ioc;
  std::unique_ptr<websocket::stream<tcp::socket>> ws;
  bool connected;
  bool running;

  // 命令处理�?
  std::unordered_map<std::string, CommandHandler> commandHandlers;
};

} // namespace hydrogen
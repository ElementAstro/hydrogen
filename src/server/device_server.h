#pragma once
#include "common/message.h"
#include "server/auth_manager.h"
#include "server/device_manager.h"
#include <crow.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace astrocomm {

class DeviceServer {
public:
  DeviceServer(uint16_t port = 8000);
  ~DeviceServer();

  // 启动服务器
  void start();

  // 停止服务器
  void stop();

  // 设置消息处理回调
  using MessageHandler = std::function<void(std::shared_ptr<Message>,
                                            crow::websocket::connection &)>;
  void setMessageHandler(MessageType type, MessageHandler handler);

private:
  // Crow app实例
  crow::SimpleApp app;
  uint16_t serverPort;

  // 设备和认证管理器
  std::unique_ptr<DeviceManager> deviceManager;
  std::unique_ptr<AuthManager> authManager;

  // 连接管理
  std::mutex connectionsMutex;
  std::unordered_map<void *, std::string>
      deviceConnections; // 连接指针 -> 设备ID
  std::unordered_map<void *, std::string>
      clientConnections; // 连接指针 -> 客户端ID

  // 消息处理器
  std::unordered_map<MessageType, MessageHandler> messageHandlers;

  // WebSocket回调处理函数
  void handleWebSocketOpen(crow::websocket::connection &conn);
  void handleWebSocketClose(crow::websocket::connection &conn);
  void handleWebSocketMessage(crow::websocket::connection &conn,
                              const std::string &data, bool is_binary);

  // 处理各类消息的函数
  void handleRegistrationMessage(std::shared_ptr<RegistrationMessage> msg,
                                 crow::websocket::connection &conn);
  void handleDiscoveryRequest(std::shared_ptr<DiscoveryRequestMessage> msg,
                              crow::websocket::connection &conn);
  void handleAuthenticationMessage(std::shared_ptr<AuthenticationMessage> msg,
                                   crow::websocket::connection &conn);
  void handleCommandMessage(std::shared_ptr<CommandMessage> msg,
                            crow::websocket::connection &conn);
  void handleResponseMessage(std::shared_ptr<ResponseMessage> msg,
                             crow::websocket::connection &conn);
  void handleEventMessage(std::shared_ptr<EventMessage> msg,
                          crow::websocket::connection &conn);

  // 注册设备连接
  void registerDeviceConnection(const std::string &deviceId,
                                crow::websocket::connection &conn);

  // 注册客户端连接
  void registerClientConnection(crow::websocket::connection &conn);

  // 消息转发
  void forwardToDevice(const std::string &deviceId, const Message &msg);
  void forwardToClient(const std::string &clientId, const Message &msg);
  void broadcastToClients(const Message &msg);

  // 创建错误消息
  void sendErrorResponse(crow::websocket::connection &conn,
                         const std::string &code, const std::string &message,
                         const std::string &originalMsgId = "");
};

} // namespace astrocomm
#pragma once
#include "common/message.h"
#include "common/error_recovery.h"  // 添加新的错误恢复组件头文件
#include "server/auth_manager.h"
#include "server/device_manager.h"
#include <chrono>
#include <crow.h>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>


namespace astrocomm {

/**
 * @class DeviceServer
 * @brief HTTP server for managing astronomical devices.
 *
 * This class provides a REST API for managing and controlling astronomical
 * devices. It handles device registration, connection status, property updates,
 * and device commands. The server supports authentication and includes
 * persistence for device configurations.
 */
class DeviceServer {
public:
  /**
   * @brief Default constructor.
   *
   * Initializes the device server with default settings.
   */
  DeviceServer();

  /**
   * @brief Constructor with configuration options.
   *
   * @param port TCP port number for the server
   * @param persistenceDir Directory for storing device configurations
   * @param autosaveInterval Interval in seconds for automatic configuration
   * saving
   */
  DeviceServer(int port, const std::string &persistenceDir = "./data/devices",
               int autosaveInterval = 300);

  /**
   * @brief Start the server.
   *
   * Initializes device resources and begins listening for connections.
   * By default, loads the previous device configuration on startup.
   *
   * @param loadPreviousConfig Whether to load previous device configuration
   */
  void start(bool loadPreviousConfig = true);

  /**
   * @brief Stop the server.
   *
   * Gracefully stops the server and persists the current device configuration.
   */
  void stop();

  // 设置消息处理回调
  using MessageHandler = std::function<void(std::shared_ptr<Message>,
                                            crow::websocket::connection &)>;
  void setMessageHandler(MessageType type, MessageHandler handler);

  // 设置配置文件路径
  void setConfigPath(const std::string &path) { configPath = path; }

  // 加载配置
  bool loadConfiguration();

  // 保存配置
  bool saveConfiguration();

  // 设置心跳检测时间间隔（秒）
  void setHeartbeatInterval(int seconds) { heartbeatInterval = seconds; }

  // 启用或禁用访问控制
  void setAccessControlEnabled(bool enabled) { accessControlEnabled = enabled; }

  // 启用或禁用命令队列(延迟执行、优先级处理)
  void setCommandQueueEnabled(bool enabled) { commandQueueEnabled = enabled; }
  
  /**
   * @brief 启用分布式模式
   * 
   * @param enabled 是否启用分布式模式
   * @param discoveryPort 用于设备发现的UDP多播端口
   * @param multicastGroup 多播组地址
   */
  void enableDistributedMode(bool enabled, uint16_t discoveryPort = 8001,
                           const std::string &multicastGroup = "239.255.0.1");
                           
  /**
   * @brief 设置服务器ID
   * 
   * @param serverId 唯一的服务器ID
   */
  void setServerId(const std::string &serverId);
  
  /**
   * @brief 获取分布式设备拓扑
   * 
   * @return 包含本地和远程设备及其依赖关系的JSON对象
   */
  json getDeviceTopology() const;
  
  /**
   * @brief 设置设备间的依赖关系
   * 
   * @param dependentDeviceId 依赖其他设备的设备ID
   * @param dependencyDeviceId 被依赖的设备ID
   * @param dependencyType 依赖类型(如"REQUIRED"或"OPTIONAL")
   */
  void setDeviceDependency(const std::string &dependentDeviceId,
                         const std::string &dependencyDeviceId,
                         const std::string &dependencyType = "REQUIRED");
  
  /**
   * @brief 获取错误管理器
   * 
   * @return 错误恢复管理器的引用
   */
  ErrorRecoveryManager& getErrorManager() const { return *errorManager; }
  
  /**
   * @brief 设置特定错误代码的处理策略
   * 
   * @param errorCode 错误代码
   * @param strategy 处理策略
   */
  void setErrorStrategy(const std::string &errorCode, ErrorHandlingStrategy strategy);
  
  /**
   * @brief 获取未解决的错误列表
   * 
   * @return 未解决错误的JSON数组
   */
  json getPendingErrors() const;

private:
  /**
   * @brief Setup API routes for the server.
   *
   * Configures all HTTP endpoints for device management, authentication, and
   * control.
   */
  void setupRoutes();

  /**
   * @brief Middleware for authentication.
   *
   * @param req HTTP request to authenticate
   * @return True if authenticated, false otherwise
   */
  bool authenticate(const crow::request &req);

  // Crow app实例
  crow::SimpleApp app;
  uint16_t serverPort;
  std::string configPath;

  // 设备和认证管理器
  std::unique_ptr<DeviceManager> deviceManager;
  std::unique_ptr<AuthManager> authManager;
  std::unique_ptr<ErrorRecoveryManager> errorManager; // 新增错误恢复管理器

  // 心跳检测相关
  bool heartbeatRunning = false;
  int heartbeatInterval = 30; // 默认30秒
  std::thread heartbeatThread;

  // 命令队列相关
  bool commandQueueEnabled = false;
  std::mutex queueMutex;
  std::vector<CommandMessage> commandQueue;

  // 访问控制相关
  bool accessControlEnabled = false;
  std::unordered_map<std::string, std::vector<std::string>>
      userDevicePermissions; // 用户->允许的设备ID列表

  // 连接管理
  std::mutex connectionsMutex;
  std::unordered_map<void *, std::string>
      deviceConnections; // 连接指针 -> 设备ID
  std::unordered_map<void *, std::string>
      clientConnections; // 连接指针 -> 客户端ID

  // 速率限制
  std::unordered_map<std::string,
                     std::chrono::time_point<std::chrono::steady_clock>>
      lastRequestTimes; // IP地址 -> 上次请求时间
  std::mutex rateLimitMutex;
  int requestsPerMinute = 60; // 每分钟最大请求数

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
  void handleHeartbeatMessage(std::shared_ptr<Message> msg,
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
  void broadcastToDevices(const Message &msg);

  // 创建错误消息
  void sendErrorResponse(crow::websocket::connection &conn,
                         const std::string &code, const std::string &message,
                         const std::string &originalMsgId = "");

  // 设置REST API路由
  void setupRestApi();

  // 心跳检测线程函数
  void heartbeatThreadFunction();

  // 开始心跳检测
  void startHeartbeatCheck();

  // 停止心跳检测
  void stopHeartbeatCheck();

  // 执行命令队列
  void processCommandQueue();

  // 添加命令到队列
  void addCommandToQueue(const CommandMessage &cmd);

  // 检查用户是否有权限访问设备
  bool hasDeviceAccess(const std::string &clientId,
                       const std::string &deviceId);

  // 速率限制检查
  bool checkRateLimit(const std::string &ipAddress);

  int port;
  std::string configDirectory;
  int autosaveInterval;
  bool running;
};

} // namespace astrocomm
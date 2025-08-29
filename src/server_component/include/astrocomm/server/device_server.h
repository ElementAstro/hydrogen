#pragma once
#include <astrocomm/core/message.h>
#include <astrocomm/core/error_recovery.h>
#include "auth_manager.h"
#include "device_manager.h"
#include <chrono>
#include "crow.h"
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

namespace astrocomm {
namespace server {

using namespace astrocomm::core;

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
   * @brief 获取服务器统计信息
   * 
   * @return JSON格式的统计信息
   */
  nlohmann::json getServerStats() const;

  /**
   * @brief 设置速率限制
   * 
   * @param requestsPerMinute 每分钟最大请求数
   */
  void setRateLimit(int requestsPerMinute) { this->requestsPerMinute = requestsPerMinute; }

  /**
   * @brief 添加用户设备权限
   * 
   * @param clientId 客户端ID
   * @param deviceId 设备ID
   */
  void addUserDevicePermission(const std::string &clientId, const std::string &deviceId);

  /**
   * @brief 移除用户设备权限
   * 
   * @param clientId 客户端ID
   * @param deviceId 设备ID
   */
  void removeUserDevicePermission(const std::string &clientId, const std::string &deviceId);

  /**
   * @brief 获取设备管理器
   * 
   * @return 设备管理器的引用
   */
  DeviceManager& getDeviceManager() { return *deviceManager; }

  /**
   * @brief 获取认证管理器
   * 
   * @return 认证管理器的引用
   */
  AuthManager& getAuthManager() { return *authManager; }

  /**
   * @brief 获取错误恢复管理器
   * 
   * @return 错误恢复管理器的引用
   */
  ErrorRecoveryManager& getErrorManager() { return *errorManager; }

private:
  // 内部方法声明
  void setupWebSocketHandlers();
  void setupRestApi();
  void handleWebSocketOpen(crow::websocket::connection &conn);
  void handleWebSocketClose(crow::websocket::connection &conn);
  void handleWebSocketMessage(crow::websocket::connection &conn,
                              const std::string &data, bool is_binary);
  void heartbeatThreadFunction();
  void startHeartbeatCheck();
  void stopHeartbeatCheck();
  void processCommandQueue();
  void addCommandToQueue(const CommandMessage &cmd);
  bool hasDeviceAccess(const std::string &clientId, const std::string &deviceId);
  bool checkRateLimit(const std::string &ipAddress);

  // Crow app实例
  crow::SimpleApp app;
  uint16_t serverPort;
  std::string configPath;

  // 设备和认证管理器
  std::unique_ptr<DeviceManager> deviceManager;
  std::unique_ptr<AuthManager> authManager;
  std::unique_ptr<ErrorRecoveryManager> errorManager;

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

  // 分布式模式设置
  bool distributedModeEnabled = false;
  std::string serverId;
  uint16_t discoveryPort = 8001;
  std::string multicastGroup = "239.255.0.1";
  std::thread discoveryThread;
  bool discoveryRunning = false;

  int port;
  std::string configDirectory;
  int autosaveInterval;
  bool running;
};

} // namespace server
} // namespace astrocomm

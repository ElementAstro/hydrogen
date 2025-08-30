#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>

namespace astrocomm {
namespace device {
namespace core {

using json = nlohmann::json;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

/**
 * @brief 通信状态枚举
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    ERROR
};

/**
 * @brief 消息处理回调函数类型
 */
using MessageHandler = std::function<void(const std::string& message)>;
using ConnectionStateHandler = std::function<void(ConnectionState state, const std::string& error)>;

/**
 * @brief 通信管理器 - 统一管理WebSocket连接和消息处理
 * 
 * 该类提供了设备与服务器之间的通信抽象层，支持：
 * - 自动重连机制
 * - 消息队列管理
 * - 连接状态监控
 * - 线程安全的消息发送
 */
class CommunicationManager {
public:
    /**
     * @brief 构造函数
     * @param deviceId 设备ID
     */
    explicit CommunicationManager(const std::string& deviceId);
    
    /**
     * @brief 析构函数
     */
    ~CommunicationManager();

    /**
     * @brief 连接到服务器
     * @param host 服务器地址
     * @param port 服务器端口
     * @return 连接是否成功
     */
    bool connect(const std::string& host, uint16_t port);

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief 发送消息
     * @param message 要发送的消息
     * @return 发送是否成功
     */
    bool sendMessage(const std::string& message);

    /**
     * @brief 发送JSON消息
     * @param jsonMessage JSON消息对象
     * @return 发送是否成功
     */
    bool sendMessage(const json& jsonMessage);

    /**
     * @brief 设置消息处理器
     * @param handler 消息处理回调函数
     */
    void setMessageHandler(MessageHandler handler);

    /**
     * @brief 设置连接状态处理器
     * @param handler 连接状态变化回调函数
     */
    void setConnectionStateHandler(ConnectionStateHandler handler);

    /**
     * @brief 获取当前连接状态
     * @return 当前连接状态
     */
    ConnectionState getConnectionState() const;

    /**
     * @brief 是否已连接
     * @return 连接状态
     */
    bool isConnected() const;

    /**
     * @brief 启用自动重连
     * @param enable 是否启用
     * @param retryInterval 重连间隔（秒）
     * @param maxRetries 最大重连次数（0表示无限重连）
     */
    void setAutoReconnect(bool enable, int retryInterval = 5, int maxRetries = 0);

    /**
     * @brief 启动消息循环
     */
    void startMessageLoop();

    /**
     * @brief 停止消息循环
     */
    void stopMessageLoop();

private:
    /**
     * @brief 消息循环线程函数
     */
    void messageLoop();

    /**
     * @brief 重连线程函数
     */
    void reconnectLoop();

    /**
     * @brief 更新连接状态
     * @param newState 新状态
     * @param error 错误信息（可选）
     */
    void updateConnectionState(ConnectionState newState, const std::string& error = "");

    /**
     * @brief 处理接收到的消息
     * @param message 消息内容
     */
    void handleReceivedMessage(const std::string& message);

private:
    std::string deviceId_;
    std::string host_;
    uint16_t port_;

    // WebSocket连接
    net::io_context ioc_;
    std::unique_ptr<websocket::stream<tcp::socket>> ws_;
    
    // 连接状态
    mutable std::mutex stateMutex_;
    std::atomic<ConnectionState> connectionState_;
    
    // 消息处理
    MessageHandler messageHandler_;
    ConnectionStateHandler connectionStateHandler_;
    
    // 线程管理
    std::thread messageThread_;
    std::thread reconnectThread_;
    std::atomic<bool> running_;
    std::atomic<bool> shouldStop_;
    
    // 自动重连配置
    std::atomic<bool> autoReconnectEnabled_;
    std::atomic<int> retryInterval_;
    std::atomic<int> maxRetries_;
    std::atomic<int> currentRetries_;
    
    // 消息发送互斥锁
    mutable std::mutex sendMutex_;
};

} // namespace core
} // namespace device
} // namespace astrocomm

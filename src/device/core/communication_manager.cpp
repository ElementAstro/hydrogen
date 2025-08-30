#include "communication_manager.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace astrocomm {
namespace device {
namespace core {

CommunicationManager::CommunicationManager(const std::string& deviceId)
    : deviceId_(deviceId)
    , port_(0)
    , connectionState_(ConnectionState::DISCONNECTED)
    , running_(false)
    , shouldStop_(false)
    , autoReconnectEnabled_(false)
    , retryInterval_(5)
    , maxRetries_(0)
    , currentRetries_(0) {
    
    SPDLOG_DEBUG("CommunicationManager created for device: {}", deviceId_);
}

CommunicationManager::~CommunicationManager() {
    disconnect();
}

bool CommunicationManager::connect(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (connectionState_ == ConnectionState::CONNECTED) {
        SPDLOG_WARN("Device {} already connected", deviceId_);
        return true;
    }

    host_ = host;
    port_ = port;
    
    updateConnectionState(ConnectionState::CONNECTING);
    
    try {
        // 创建WebSocket连接
        ws_ = std::make_unique<websocket::stream<tcp::socket>>(ioc_);
        
        // 解析主机地址
        tcp::resolver resolver(ioc_);
        auto const results = resolver.resolve(host, std::to_string(port));
        
        // 连接到服务器
        auto ep = net::connect(ws_->next_layer(), results);
        
        // 设置WebSocket选项
        ws_->set_option(websocket::stream_base::timeout::suggested(
            beast::role_type::client));
        
        ws_->set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(beast::http::field::user_agent, "AstroComm-Device/1.0");
            }));
        
        // 执行WebSocket握手
        ws_->handshake(host + ':' + std::to_string(port), "/");
        
        updateConnectionState(ConnectionState::CONNECTED);
        currentRetries_ = 0;
        
        SPDLOG_INFO("Device {} connected to {}:{}", deviceId_, host, port);
        return true;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to connect device {}: {}", deviceId_, e.what());
        updateConnectionState(ConnectionState::ERROR, e.what());
        ws_.reset();
        return false;
    }
}

void CommunicationManager::disconnect() {
    shouldStop_ = true;
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (connectionState_ != ConnectionState::DISCONNECTED) {
            updateConnectionState(ConnectionState::DISCONNECTED);
        }
    }
    
    // 停止消息循环
    stopMessageLoop();
    
    // 关闭WebSocket连接
    if (ws_) {
        try {
            ws_->close(websocket::close_code::normal);
        } catch (const std::exception& e) {
            SPDLOG_WARN("Error closing WebSocket for device {}: {}", deviceId_, e.what());
        }
        ws_.reset();
    }
    
    // 等待线程结束
    if (messageThread_.joinable()) {
        messageThread_.join();
    }
    if (reconnectThread_.joinable()) {
        reconnectThread_.join();
    }
    
    SPDLOG_INFO("Device {} disconnected", deviceId_);
}

bool CommunicationManager::sendMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    if (!isConnected() || !ws_) {
        SPDLOG_WARN("Cannot send message for device {}: not connected", deviceId_);
        return false;
    }
    
    try {
        ws_->write(net::buffer(message));
        SPDLOG_DEBUG("Sent message for device {}: {}", deviceId_, message);
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to send message for device {}: {}", deviceId_, e.what());
        updateConnectionState(ConnectionState::ERROR, e.what());
        return false;
    }
}

bool CommunicationManager::sendMessage(const json& jsonMessage) {
    return sendMessage(jsonMessage.dump());
}

void CommunicationManager::setMessageHandler(MessageHandler handler) {
    messageHandler_ = handler;
}

void CommunicationManager::setConnectionStateHandler(ConnectionStateHandler handler) {
    connectionStateHandler_ = handler;
}

ConnectionState CommunicationManager::getConnectionState() const {
    return connectionState_.load();
}

bool CommunicationManager::isConnected() const {
    return connectionState_.load() == ConnectionState::CONNECTED;
}

void CommunicationManager::setAutoReconnect(bool enable, int retryInterval, int maxRetries) {
    autoReconnectEnabled_ = enable;
    retryInterval_ = retryInterval;
    maxRetries_ = maxRetries;
    
    if (enable && !reconnectThread_.joinable()) {
        reconnectThread_ = std::thread(&CommunicationManager::reconnectLoop, this);
    }
}

void CommunicationManager::startMessageLoop() {
    if (running_) {
        return;
    }
    
    running_ = true;
    messageThread_ = std::thread(&CommunicationManager::messageLoop, this);
    SPDLOG_DEBUG("Message loop started for device {}", deviceId_);
}

void CommunicationManager::stopMessageLoop() {
    running_ = false;
    if (messageThread_.joinable()) {
        messageThread_.join();
    }
    SPDLOG_DEBUG("Message loop stopped for device {}", deviceId_);
}

void CommunicationManager::messageLoop() {
    while (running_ && !shouldStop_) {
        if (!isConnected() || !ws_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        try {
            beast::flat_buffer buffer;
            ws_->read(buffer);
            std::string message = beast::buffers_to_string(buffer.data());
            
            handleReceivedMessage(message);
            
        } catch (beast::system_error const& se) {
            if (se.code() == websocket::error::closed) {
                SPDLOG_INFO("WebSocket connection closed for device {}", deviceId_);
                updateConnectionState(ConnectionState::DISCONNECTED);
                break;
            } else {
                SPDLOG_ERROR("WebSocket error for device {}: {}", deviceId_, se.code().message());
                updateConnectionState(ConnectionState::ERROR, se.code().message());
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in message loop for device {}: {}", deviceId_, e.what());
            updateConnectionState(ConnectionState::ERROR, e.what());
        }
    }
}

void CommunicationManager::reconnectLoop() {
    while (!shouldStop_) {
        if (autoReconnectEnabled_ && 
            connectionState_ == ConnectionState::ERROR &&
            (maxRetries_ == 0 || currentRetries_ < maxRetries_)) {
            
            SPDLOG_INFO("Attempting to reconnect device {} (attempt {})", 
                       deviceId_, currentRetries_ + 1);
            
            updateConnectionState(ConnectionState::RECONNECTING);
            
            if (connect(host_, port_)) {
                startMessageLoop();
            } else {
                currentRetries_++;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(retryInterval_));
    }
}

void CommunicationManager::updateConnectionState(ConnectionState newState, const std::string& error) {
    ConnectionState oldState = connectionState_.exchange(newState);
    
    if (oldState != newState) {
        SPDLOG_DEBUG("Device {} connection state changed: {} -> {}", 
                    deviceId_, static_cast<int>(oldState), static_cast<int>(newState));
        
        if (connectionStateHandler_) {
            connectionStateHandler_(newState, error);
        }
    }
}

void CommunicationManager::handleReceivedMessage(const std::string& message) {
    SPDLOG_DEBUG("Received message for device {}: {}", deviceId_, message);
    
    if (messageHandler_) {
        try {
            messageHandler_(message);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in message handler for device {}: {}", deviceId_, e.what());
        }
    }
}

} // namespace core
} // namespace device
} // namespace astrocomm

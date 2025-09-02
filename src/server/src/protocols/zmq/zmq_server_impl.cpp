#include "hydrogen/server/protocols/zmq/zmq_server.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <random>
#include <sstream>

namespace hydrogen {
namespace server {
namespace protocols {
namespace zmq {

/**
 * @brief Concrete implementation of the ZeroMQ Server
 */
class ZmqServerImpl : public IZmqServer {
public:
    explicit ZmqServerImpl(const ZmqServerConfig& config)
        : config_(config), running_(false), initialized_(false), context_(nullptr) {
        spdlog::info("ZeroMQ server created with address: {}", config_.bindAddress);
    }

    ~ZmqServerImpl() {
        stop();
    }

    // Server lifecycle
    bool initialize() override {
        if (initialized_) {
            spdlog::warn("ZeroMQ server already initialized");
            return true;
        }

        try {
            // Create ZeroMQ context
            // Note: In a real implementation, you would use actual ZeroMQ library
            // For now, we'll simulate the initialization
            context_ = reinterpret_cast<void*>(0x12345678); // Simulated context
            
            // Initialize internal data structures
            clients_.clear();
            messageQueue_.clear();
            
            // Initialize statistics
            statistics_ = ZmqServerStatistics{};
            
            initialized_ = true;
            spdlog::info("ZeroMQ server initialized successfully on {}", config_.bindAddress);
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Failed to initialize ZeroMQ server: {}", e.what());
            return false;
        }
    }

    bool start() override {
        if (!initialized_) {
            spdlog::error("ZeroMQ server not initialized");
            return false;
        }

        if (running_) {
            spdlog::warn("ZeroMQ server already running");
            return true;
        }

        try {
            running_ = true;
            
            // Start server threads based on socket type
            switch (config_.socketType) {
                case ZmqSocketType::REP:
                    serverThread_ = std::thread(&ZmqServerImpl::repServerLoop, this);
                    break;
                case ZmqSocketType::PUB:
                    serverThread_ = std::thread(&ZmqServerImpl::pubServerLoop, this);
                    break;
                case ZmqSocketType::PUSH:
                    serverThread_ = std::thread(&ZmqServerImpl::pushServerLoop, this);
                    break;
                case ZmqSocketType::ROUTER:
                    serverThread_ = std::thread(&ZmqServerImpl::routerServerLoop, this);
                    break;
                default:
                    spdlog::error("Unsupported ZeroMQ socket type: {}", static_cast<int>(config_.socketType));
                    running_ = false;
                    return false;
            }
            
            // Start message processing thread
            messageProcessorThread_ = std::thread(&ZmqServerImpl::messageProcessorLoop, this);
            
            spdlog::info("ZeroMQ server started and listening on {}", config_.bindAddress);
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Failed to start ZeroMQ server: {}", e.what());
            running_ = false;
            return false;
        }
    }

    bool stop() override {
        if (!running_) {
            return true;
        }

        try {
            spdlog::info("Stopping ZeroMQ server...");
            
            running_ = false;
            
            // Wait for threads to finish
            if (serverThread_.joinable()) {
                serverThread_.join();
            }
            
            if (messageProcessorThread_.joinable()) {
                messageProcessorThread_.join();
            }
            
            // Cleanup ZeroMQ context
            if (context_) {
                // In a real implementation: zmq_ctx_destroy(context_);
                context_ = nullptr;
            }
            
            spdlog::info("ZeroMQ server stopped");
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Error stopping ZeroMQ server: {}", e.what());
            return false;
        }
    }

    bool restart() override {
        return stop() && start();
    }

    bool isRunning() const override {
        return running_;
    }

    bool isInitialized() const override {
        return initialized_;
    }

    // Configuration
    ZmqServerConfig getConfig() const override {
        return config_;
    }

    bool updateConfig(const ZmqServerConfig& config) override {
        if (running_) {
            spdlog::warn("Cannot update ZeroMQ server config while running");
            return false;
        }

        config_ = config;
        spdlog::info("ZeroMQ server configuration updated");
        return true;
    }

    // Message handling
    bool sendMessage(const std::string& message, const std::string& clientId) override {
        if (!running_) {
            spdlog::error("ZeroMQ server not running");
            return false;
        }

        try {
            ZmqMessage zmqMsg;
            zmqMsg.id = generateMessageId();
            zmqMsg.content = message;
            zmqMsg.clientId = clientId;
            zmqMsg.timestamp = std::chrono::system_clock::now();
            zmqMsg.type = ZmqMessageType::DATA;
            
            // Add to message queue
            {
                std::lock_guard<std::mutex> lock(messageQueueMutex_);
                messageQueue_.push_back(zmqMsg);
            }
            
            // Update statistics
            statistics_.totalMessagesSent++;
            
            spdlog::debug("ZeroMQ message queued for sending: {} to {}", zmqMsg.id, clientId);
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Failed to send ZeroMQ message: {}", e.what());
            return false;
        }
    }

    bool broadcastMessage(const std::string& message) override {
        if (!running_) {
            spdlog::error("ZeroMQ server not running");
            return false;
        }

        if (config_.socketType != ZmqSocketType::PUB) {
            spdlog::error("Broadcast only supported for PUB socket type");
            return false;
        }

        try {
            ZmqMessage zmqMsg;
            zmqMsg.id = generateMessageId();
            zmqMsg.content = message;
            zmqMsg.clientId = ""; // Broadcast to all
            zmqMsg.timestamp = std::chrono::system_clock::now();
            zmqMsg.type = ZmqMessageType::BROADCAST;
            
            // Add to message queue
            {
                std::lock_guard<std::mutex> lock(messageQueueMutex_);
                messageQueue_.push_back(zmqMsg);
            }
            
            // Update statistics
            statistics_.totalMessagesSent++;
            
            spdlog::debug("ZeroMQ broadcast message queued: {}", zmqMsg.id);
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Failed to broadcast ZeroMQ message: {}", e.what());
            return false;
        }
    }

    std::vector<ZmqMessage> getReceivedMessages() const override {
        std::lock_guard<std::mutex> lock(receivedMessagesMutex_);
        return receivedMessages_;
    }

    void clearReceivedMessages() override {
        std::lock_guard<std::mutex> lock(receivedMessagesMutex_);
        receivedMessages_.clear();
        spdlog::debug("ZeroMQ received messages cleared");
    }

    // Client management
    std::vector<std::string> getConnectedClients() const override {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        
        std::vector<std::string> clientIds;
        for (const auto& pair : clients_) {
            clientIds.push_back(pair.first);
        }
        return clientIds;
    }

    size_t getClientCount() const override {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        return clients_.size();
    }

    bool disconnectClient(const std::string& clientId) override {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        
        auto it = clients_.find(clientId);
        if (it != clients_.end()) {
            clients_.erase(it);
            spdlog::info("ZeroMQ client disconnected: {}", clientId);
            return true;
        }
        
        spdlog::warn("ZeroMQ client not found for disconnection: {}", clientId);
        return false;
    }

    // Statistics and monitoring
    ZmqServerStatistics getStatistics() const override {
        return statistics_;
    }

    void resetStatistics() override {
        statistics_ = ZmqServerStatistics{};
        statistics_.connectedClients = getClientCount();
        spdlog::debug("ZeroMQ server statistics reset");
    }

    // Health checking
    bool isHealthy() const override {
        return initialized_ && running_ && context_ != nullptr;
    }

    std::string getHealthStatus() const override {
        if (!initialized_) {
            return "Not initialized";
        }
        if (!running_) {
            return "Not running";
        }
        if (!context_) {
            return "No ZeroMQ context";
        }
        return "Healthy";
    }

    // Socket configuration
    bool setSocketOption(ZmqSocketOption option, int value) override {
        if (running_) {
            spdlog::error("Cannot set socket option while server is running");
            return false;
        }

        try {
            // Store socket options for when socket is created
            socketOptions_[option] = value;
            spdlog::debug("ZeroMQ socket option set: {} = {}", static_cast<int>(option), value);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to set ZeroMQ socket option: {}", e.what());
            return false;
        }
    }

    int getSocketOption(ZmqSocketOption option) const override {
        auto it = socketOptions_.find(option);
        if (it != socketOptions_.end()) {
            return it->second;
        }
        return -1; // Option not set
    }

    // Message handlers
    void setMessageHandler(ZmqMessageHandler handler) override {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        messageHandler_ = handler;
        spdlog::debug("ZeroMQ message handler set");
    }

    void removeMessageHandler() override {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        messageHandler_ = nullptr;
        spdlog::debug("ZeroMQ message handler removed");
    }

    // Connection handlers
    void setConnectionHandler(ZmqConnectionHandler handler) override {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        connectionHandler_ = handler;
        spdlog::debug("ZeroMQ connection handler set");
    }

    void removeConnectionHandler() override {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        connectionHandler_ = nullptr;
        spdlog::debug("ZeroMQ connection handler removed");
    }

private:
    ZmqServerConfig config_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    void* context_; // ZeroMQ context (simulated)
    std::thread serverThread_;
    std::thread messageProcessorThread_;
    std::chrono::steady_clock::time_point startTime_;
    
    mutable std::mutex clientsMutex_;
    mutable std::mutex messageQueueMutex_;
    mutable std::mutex receivedMessagesMutex_;
    mutable std::mutex handlerMutex_;
    
    std::unordered_map<std::string, ZmqClientInfo> clients_;
    std::vector<ZmqMessage> messageQueue_;
    std::vector<ZmqMessage> receivedMessages_;
    std::unordered_map<ZmqSocketOption, int> socketOptions_;
    
    ZmqServerStatistics statistics_;
    ZmqMessageHandler messageHandler_;
    ZmqConnectionHandler connectionHandler_;

    void repServerLoop() {
        spdlog::debug("ZeroMQ REP server loop started");
        startTime_ = std::chrono::steady_clock::now();
        
        while (running_) {
            try {
                // Simulate REP (Request-Reply) server behavior
                processRepMessages();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (const std::exception& e) {
                spdlog::error("Error in ZeroMQ REP server loop: {}", e.what());
            }
        }
        
        spdlog::debug("ZeroMQ REP server loop finished");
    }

    void pubServerLoop() {
        spdlog::debug("ZeroMQ PUB server loop started");
        startTime_ = std::chrono::steady_clock::now();
        
        while (running_) {
            try {
                // Simulate PUB (Publisher) server behavior
                processPubMessages();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (const std::exception& e) {
                spdlog::error("Error in ZeroMQ PUB server loop: {}", e.what());
            }
        }
        
        spdlog::debug("ZeroMQ PUB server loop finished");
    }

    void pushServerLoop() {
        spdlog::debug("ZeroMQ PUSH server loop started");
        startTime_ = std::chrono::steady_clock::now();
        
        while (running_) {
            try {
                // Simulate PUSH server behavior
                processPushMessages();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (const std::exception& e) {
                spdlog::error("Error in ZeroMQ PUSH server loop: {}", e.what());
            }
        }
        
        spdlog::debug("ZeroMQ PUSH server loop finished");
    }

    void routerServerLoop() {
        spdlog::debug("ZeroMQ ROUTER server loop started");
        startTime_ = std::chrono::steady_clock::now();
        
        while (running_) {
            try {
                // Simulate ROUTER server behavior
                processRouterMessages();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (const std::exception& e) {
                spdlog::error("Error in ZeroMQ ROUTER server loop: {}", e.what());
            }
        }
        
        spdlog::debug("ZeroMQ ROUTER server loop finished");
    }

    void messageProcessorLoop() {
        while (running_) {
            try {
                processMessageQueue();
                updateStatistics();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } catch (const std::exception& e) {
                spdlog::error("Error in ZeroMQ message processor loop: {}", e.what());
            }
        }
        
        spdlog::debug("ZeroMQ message processor loop finished");
    }

    void processRepMessages() {
        // Simulate processing REP messages
        // In a real implementation, this would handle ZeroMQ REP socket operations
    }

    void processPubMessages() {
        // Simulate processing PUB messages
        // In a real implementation, this would handle ZeroMQ PUB socket operations
    }

    void processPushMessages() {
        // Simulate processing PUSH messages
        // In a real implementation, this would handle ZeroMQ PUSH socket operations
    }

    void processRouterMessages() {
        // Simulate processing ROUTER messages
        // In a real implementation, this would handle ZeroMQ ROUTER socket operations
    }

    void processMessageQueue() {
        std::lock_guard<std::mutex> lock(messageQueueMutex_);
        
        if (!messageQueue_.empty()) {
            // Process queued messages
            for (const auto& message : messageQueue_) {
                // Simulate sending message
                spdlog::debug("Processing ZeroMQ message: {}", message.id);
                
                // Call message handler if set
                {
                    std::lock_guard<std::mutex> handlerLock(handlerMutex_);
                    if (messageHandler_) {
                        try {
                            messageHandler_(message);
                        } catch (const std::exception& e) {
                            spdlog::error("Error in ZeroMQ message handler: {}", e.what());
                        }
                    }
                }
            }
            
            messageQueue_.clear();
        }
    }

    void updateStatistics() {
        statistics_.uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime_).count();
        statistics_.connectedClients = getClientCount();
        statistics_.messagesPerSecond = calculateMessageRate();
    }

    double calculateMessageRate() const {
        // Calculate messages per second over the last minute
        // In a real implementation, this would track actual message timestamps
        return static_cast<double>(statistics_.totalMessagesSent) / 60.0;
    }

    std::string generateMessageId() const {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        ss << "zmq_";
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << dis(gen);
        }
        return ss.str();
    }
};

// Factory function implementation
std::unique_ptr<IZmqServer> ZmqServerFactory::createServer(const ZmqServerConfig& config) {
    return std::make_unique<ZmqServerImpl>(config);
}

std::unique_ptr<IZmqServer> ZmqServerFactory::createServer(const std::string& bindAddress, ZmqSocketType socketType) {
    ZmqServerConfig config;
    config.bindAddress = bindAddress;
    config.socketType = socketType;
    config.ioThreads = 1;
    config.maxSockets = 1024;
    config.sendTimeout = 1000;
    config.receiveTimeout = 1000;
    
    return std::make_unique<ZmqServerImpl>(config);
}

} // namespace zmq
} // namespace protocols
} // namespace server
} // namespace hydrogen

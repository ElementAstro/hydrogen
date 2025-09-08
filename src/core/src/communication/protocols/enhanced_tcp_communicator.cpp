#include "hydrogen/core/communication/protocols/tcp_communicator.h"

#include <algorithm>
#include <random>
#include <sstream>

// Platform-specific includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace hydrogen {
namespace core {
namespace communication {
namespace protocols {

// TcpConnectionConfig implementation
json TcpConnectionConfig::toJson() const {
    json j;
    j["serverAddress"] = serverAddress;
    j["serverPort"] = serverPort;
    j["isServer"] = isServer;
    j["connectTimeout"] = connectTimeout.count();
    j["readTimeout"] = readTimeout.count();
    j["writeTimeout"] = writeTimeout.count();
    j["bufferSize"] = bufferSize;
    j["enableKeepAlive"] = enableKeepAlive;
    j["keepAliveInterval"] = keepAliveInterval.count();
    j["keepAliveProbes"] = keepAliveProbes;
    j["keepAliveTimeout"] = keepAliveTimeout.count();
    j["enableNagle"] = enableNagle;
    j["maxConnections"] = maxConnections;
    j["reuseAddress"] = reuseAddress;
    j["bindInterface"] = bindInterface;
    j["enableSSL"] = enableSSL;
    j["sslCertPath"] = sslCertPath;
    j["sslKeyPath"] = sslKeyPath;
    j["sslCaPath"] = sslCaPath;
    j["enableCompression"] = enableCompression;
    j["enableMessageBatching"] = enableMessageBatching;
    j["maxBatchSize"] = maxBatchSize;
    j["batchTimeout"] = batchTimeout.count();
    return j;
}

TcpConnectionConfig TcpConnectionConfig::fromJson(const json& j) {
    TcpConnectionConfig config;
    if (j.contains("serverAddress")) config.serverAddress = j["serverAddress"];
    if (j.contains("serverPort")) config.serverPort = j["serverPort"];
    if (j.contains("isServer")) config.isServer = j["isServer"];
    if (j.contains("connectTimeout")) config.connectTimeout = std::chrono::milliseconds(j["connectTimeout"]);
    if (j.contains("readTimeout")) config.readTimeout = std::chrono::milliseconds(j["readTimeout"]);
    if (j.contains("writeTimeout")) config.writeTimeout = std::chrono::milliseconds(j["writeTimeout"]);
    if (j.contains("bufferSize")) config.bufferSize = j["bufferSize"];
    if (j.contains("enableKeepAlive")) config.enableKeepAlive = j["enableKeepAlive"];
    if (j.contains("keepAliveInterval")) config.keepAliveInterval = std::chrono::seconds(j["keepAliveInterval"]);
    if (j.contains("keepAliveProbes")) config.keepAliveProbes = j["keepAliveProbes"];
    if (j.contains("keepAliveTimeout")) config.keepAliveTimeout = std::chrono::seconds(j["keepAliveTimeout"]);
    if (j.contains("enableNagle")) config.enableNagle = j["enableNagle"];
    if (j.contains("maxConnections")) config.maxConnections = j["maxConnections"];
    if (j.contains("reuseAddress")) config.reuseAddress = j["reuseAddress"];
    if (j.contains("bindInterface")) config.bindInterface = j["bindInterface"];
    if (j.contains("enableSSL")) config.enableSSL = j["enableSSL"];
    if (j.contains("sslCertPath")) config.sslCertPath = j["sslCertPath"];
    if (j.contains("sslKeyPath")) config.sslKeyPath = j["sslKeyPath"];
    if (j.contains("sslCaPath")) config.sslCaPath = j["sslCaPath"];
    if (j.contains("enableCompression")) config.enableCompression = j["enableCompression"];
    if (j.contains("enableMessageBatching")) config.enableMessageBatching = j["enableMessageBatching"];
    if (j.contains("maxBatchSize")) config.maxBatchSize = j["maxBatchSize"];
    if (j.contains("batchTimeout")) config.batchTimeout = std::chrono::milliseconds(j["batchTimeout"]);
    return config;
}

// TcpConnectionMetrics implementation
json TcpConnectionMetrics::toJson() const {
    json j;
    j["connectionsEstablished"] = connectionsEstablished.load();
    j["connectionsDropped"] = connectionsDropped.load();
    j["messagesSent"] = messagesSent.load();
    j["messagesReceived"] = messagesReceived.load();
    j["bytesSent"] = bytesSent.load();
    j["bytesReceived"] = bytesReceived.load();
    j["averageLatency"] = averageLatency.load();
    j["errorCount"] = errorCount.load();
    j["timeoutCount"] = timeoutCount.load();
    j["lastActivity"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        lastActivity.time_since_epoch()).count();
    return j;
}

// TcpClientSession implementation
TcpClientSession::TcpClientSession(const std::string& clientId)
    : clientId_(clientId) {
    spdlog::debug("TcpClientSession: Created session for client: {}", clientId_);
}

TcpClientSession::~TcpClientSession() {
    spdlog::debug("TcpClientSession: Destroying session for client: {}", clientId_);
    disconnect();
}

bool TcpClientSession::connect(const TcpConnectionConfig& config) {
    if (state_.load() == TcpConnectionState::CONNECTED) {
        spdlog::warn("TcpClientSession: Already connected");
        return true;
    }

    config_ = config;
    state_.store(TcpConnectionState::CONNECTING);

    spdlog::info("TcpClientSession: Connecting to {}:{}", config_.serverAddress, config_.serverPort);

    if (!performConnect()) {
        state_.store(TcpConnectionState::TCP_ERROR);
        return false;
    }

    // Start background threads
    running_.store(true);
    receiveThread_ = std::make_unique<std::thread>(&TcpClientSession::receiveThreadFunction, this);
    sendThread_ = std::make_unique<std::thread>(&TcpClientSession::sendThreadFunction, this);

    state_.store(TcpConnectionState::CONNECTED);
    metrics_.connectionsEstablished.fetch_add(1);

    // Notify connection status
    {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        if (connectionStatusCallback_) {
            connectionStatusCallback_(true);
        }
    }

    spdlog::info("TcpClientSession: Successfully connected client: {}", clientId_);
    return true;
}

void TcpClientSession::disconnect() {
    if (state_.load() == TcpConnectionState::DISCONNECTED) {
        return;
    }

    spdlog::info("TcpClientSession: Disconnecting client: {}", clientId_);

    state_.store(TcpConnectionState::DISCONNECTING);
    running_.store(false);

    // Wake up send thread
    sendCondition_.notify_all();

    // Wait for threads to finish
    if (receiveThread_ && receiveThread_->joinable()) {
        receiveThread_->join();
    }

    if (sendThread_ && sendThread_->joinable()) {
        sendThread_->join();
    }

    performDisconnect();

    state_.store(TcpConnectionState::DISCONNECTED);
    metrics_.connectionsDropped.fetch_add(1);

    // Notify connection status
    {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        if (connectionStatusCallback_) {
            connectionStatusCallback_(false);
        }
    }

    spdlog::info("TcpClientSession: Disconnected client: {}", clientId_);
}

bool TcpClientSession::isConnected() const {
    return state_.load() == TcpConnectionState::CONNECTED;
}

std::future<bool> TcpClientSession::sendMessage(const std::string& message) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();

    if (!isConnected()) {
        promise->set_value(false);
        return future;
    }

    // Add message to send queue
    {
        std::lock_guard<std::mutex> lock(sendQueueMutex_);
        sendQueue_.push(message);
    }

    sendCondition_.notify_one();
    promise->set_value(true);

    return future;
}

bool TcpClientSession::sendMessageSync(const std::string& message) {
    if (!isConnected()) {
        return false;
    }

    try {
        auto startTime = std::chrono::high_resolution_clock::now();

#ifdef _WIN32
        int result = ::send(static_cast<SOCKET>(socket_), message.c_str(),
                           static_cast<int>(message.length()), 0);
        bool success = (result != SOCKET_ERROR);
#else
        ssize_t result = ::send(socket_, message.c_str(), message.length(), MSG_NOSIGNAL);
        bool success = (result >= 0);
#endif

        if (success) {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

            metrics_.messagesSent.fetch_add(1);
            metrics_.bytesSent.fetch_add(message.length());
            metrics_.lastActivity = std::chrono::system_clock::now();

            // Update average latency
            double currentLatency = duration.count() / 1000.0; // Convert to milliseconds
            double currentAvg = metrics_.averageLatency.load();
            double newAvg = (currentAvg + currentLatency) / 2.0;
            metrics_.averageLatency.store(newAvg);

            spdlog::debug("TcpClientSession: Sent message ({} bytes) in {:.2f}ms",
                         message.length(), currentLatency);
            return true;
        } else {
            metrics_.errorCount.fetch_add(1);
            handleError("Failed to send message");
            return false;
        }

    } catch (const std::exception& e) {
        metrics_.errorCount.fetch_add(1);
        handleError("Exception during send: " + std::string(e.what()));
        return false;
    }
}

void TcpClientSession::setMessageCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    messageCallback_ = callback;
}

void TcpClientSession::setConnectionStatusCallback(std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    connectionStatusCallback_ = callback;
}

void TcpClientSession::setErrorCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    errorCallback_ = callback;
}

bool TcpClientSession::performConnect() {
    try {
#ifdef _WIN32
        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            handleError("WSAStartup failed: " + std::to_string(result));
            return false;
        }

        // Create socket
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            handleError("Socket creation failed: " + std::to_string(WSAGetLastError()));
            WSACleanup();
            return false;
        }

        socket_ = static_cast<uintptr_t>(sock);
#else
        // Create socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            handleError("Socket creation failed: " + std::string(strerror(errno)));
            return false;
        }

        socket_ = sock;
#endif

        // Configure socket options
        if (config_.enableKeepAlive) {
            int keepAlive = 1;
#ifdef _WIN32
            setsockopt(static_cast<SOCKET>(socket_), SOL_SOCKET, SO_KEEPALIVE,
                      reinterpret_cast<const char*>(&keepAlive), sizeof(keepAlive));
#else
            setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));

            int keepIdle = static_cast<int>(config_.keepAliveInterval.count());
            int keepIntvl = static_cast<int>(config_.keepAliveTimeout.count());
            int keepCnt = config_.keepAliveProbes;

            setsockopt(socket_, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
            setsockopt(socket_, IPPROTO_TCP, TCP_KEEPINTVL, &keepIntvl, sizeof(keepIntvl));
            setsockopt(socket_, IPPROTO_TCP, TCP_KEEPCNT, &keepCnt, sizeof(keepCnt));
#endif
        }

        // Disable Nagle's algorithm if requested
        if (!config_.enableNagle) {
            int noDelay = 1;
#ifdef _WIN32
            setsockopt(static_cast<SOCKET>(socket_), IPPROTO_TCP, TCP_NODELAY,
                      reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
#else
            setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
#endif
        }

        // Set up server address
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(config_.serverPort);

        // Resolve hostname
        struct hostent* host = gethostbyname(config_.serverAddress.c_str());
        if (!host) {
            handleError("Failed to resolve hostname: " + config_.serverAddress);
            performDisconnect();
            return false;
        }

        memcpy(&serverAddr.sin_addr, host->h_addr_list[0], host->h_length);

        // Connect to server
#ifdef _WIN32
        int connectResult = ::connect(static_cast<SOCKET>(socket_),
                                     reinterpret_cast<struct sockaddr*>(&serverAddr),
                                     sizeof(serverAddr));
        if (connectResult == SOCKET_ERROR) {
            handleError("Connection failed: " + std::to_string(WSAGetLastError()));
            performDisconnect();
            return false;
        }
#else
        int connectResult = ::connect(socket_,
                                     reinterpret_cast<struct sockaddr*>(&serverAddr),
                                     sizeof(serverAddr));
        if (connectResult < 0) {
            handleError("Connection failed: " + std::string(strerror(errno)));
            performDisconnect();
            return false;
        }
#endif

        return true;

    } catch (const std::exception& e) {
        handleError("Exception during connect: " + std::string(e.what()));
        return false;
    }
}

void TcpClientSession::performDisconnect() {
    if (socket_ != 0) {
#ifdef _WIN32
        closesocket(static_cast<SOCKET>(socket_));
        WSACleanup();
        socket_ = 0;
#else
        if (socket_ >= 0) {
            close(socket_);
            socket_ = -1;
        }
#endif
    }
}

void TcpClientSession::receiveThreadFunction() {
    spdlog::debug("TcpClientSession: Receive thread started for client: {}", clientId_);

    std::vector<char> buffer(config_.bufferSize);

    while (running_.load() && isConnected()) {
        try {
#ifdef _WIN32
            int bytesReceived = recv(static_cast<SOCKET>(socket_), buffer.data(),
                                   static_cast<int>(buffer.size()), 0);

            if (bytesReceived == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    handleError("Receive error: " + std::to_string(error));
                    break;
                }
                continue;
            } else if (bytesReceived == 0) {
                spdlog::info("TcpClientSession: Connection closed by peer");
                break;
            }
#else
            ssize_t bytesReceived = recv(socket_, buffer.data(), buffer.size(), 0);

            if (bytesReceived < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    handleError("Receive error: " + std::string(strerror(errno)));
                    break;
                }
                continue;
            } else if (bytesReceived == 0) {
                spdlog::info("TcpClientSession: Connection closed by peer");
                break;
            }
#endif

            // Process received data
            std::string message(buffer.data(), bytesReceived);

            metrics_.messagesReceived.fetch_add(1);
            metrics_.bytesReceived.fetch_add(bytesReceived);
            metrics_.lastActivity = std::chrono::system_clock::now();

            // Call message callback
            {
                std::lock_guard<std::mutex> lock(callbacksMutex_);
                if (messageCallback_) {
                    messageCallback_(message);
                }
            }

            spdlog::debug("TcpClientSession: Received message ({} bytes)", bytesReceived);

        } catch (const std::exception& e) {
            handleError("Exception in receive thread: " + std::string(e.what()));
            break;
        }
    }

    spdlog::debug("TcpClientSession: Receive thread stopped for client: {}", clientId_);
}

void TcpClientSession::sendThreadFunction() {
    spdlog::debug("TcpClientSession: Send thread started for client: {}", clientId_);

    while (running_.load()) {
        std::unique_lock<std::mutex> lock(sendQueueMutex_);

        // Wait for messages or shutdown
        sendCondition_.wait(lock, [this] {
            return !sendQueue_.empty() || !running_.load();
        });

        if (!running_.load()) {
            break;
        }

        // Process all queued messages
        while (!sendQueue_.empty() && isConnected()) {
            std::string message = sendQueue_.front();
            sendQueue_.pop();
            lock.unlock();

            // Send message synchronously
            sendMessageSync(message);

            lock.lock();
        }
    }

    spdlog::debug("TcpClientSession: Send thread stopped for client: {}", clientId_);
}

void TcpClientSession::handleError(const std::string& error) {
    spdlog::error("TcpClientSession: Error in client {}: {}", clientId_, error);

    metrics_.errorCount.fetch_add(1);

    {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        if (errorCallback_) {
            errorCallback_(error);
        }
    }
}

// TcpCommunicator implementation
TcpCommunicator::TcpCommunicator(const TcpConnectionConfig& config)
    : config_(config) {
    spdlog::debug("TcpCommunicator: Created with server mode: {}", config_.isServer);
    
    // Initialize performance optimization components
    initializePerformanceComponents();
}

TcpCommunicator::~TcpCommunicator() {
    spdlog::debug("TcpCommunicator: Destructor called");
    disconnect();
    shutdownPerformanceComponents();
}

bool TcpCommunicator::connect(const ConnectionConfig& config) {
    spdlog::info("TcpCommunicator: Connecting with configuration");

    // Convert generic config to TCP config if needed
    // For now, use existing TCP config
    (void)config; // Suppress unused parameter warning
    
    if (config_.isServer) {
        return startServer();
    } else {
        // Create client session
        clientSession_ = std::make_shared<TcpClientSession>("main_client");
        
        // Set up callbacks
        clientSession_->setMessageCallback([this](const std::string& message) {
            handleIncomingMessage(message);
        });
        
        clientSession_->setConnectionStatusCallback([this](bool connected) {
            handleConnectionStatusChange(connected);
        });
        
        clientSession_->setErrorCallback([this](const std::string& error) {
            handleError(error);
        });
        
        bool success = clientSession_->connect(config_);
        connected_.store(success);
        
        return success;
    }
}

void TcpCommunicator::disconnect() {
    spdlog::info("TcpCommunicator: Disconnecting");
    
    if (server_) {
        server_->stop();
        server_.reset();
    }
    
    if (clientSession_) {
        clientSession_->disconnect();
        clientSession_.reset();
    }
    
    connected_.store(false);
}

bool TcpCommunicator::isConnected() const {
    if (config_.isServer) {
        return server_ && server_->isRunning();
    } else {
        return clientSession_ && clientSession_->isConnected();
    }
}

std::future<CommunicationResponse> TcpCommunicator::sendMessage(const CommunicationMessage& message) {
    auto promise = std::make_shared<std::promise<CommunicationResponse>>();
    auto future = promise->get_future();
    
    if (!isConnected()) {
        CommunicationResponse response;
        response.success = false;
        response.errorMessage = "Not connected";
        response.timestamp = std::chrono::system_clock::now();
        promise->set_value(response);
        return future;
    }
    
    try {
        std::string serializedMessage = serializeMessage(message);
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        bool sendSuccess = false;
        if (config_.isServer) {
            // Send to all clients or specific client
            if (message.deviceId.empty()) {
                sendSuccess = server_->sendToAllClients(serializedMessage);
            } else {
                sendSuccess = server_->sendToClient(message.deviceId, serializedMessage);
            }
        } else {
            auto sendFuture = clientSession_->sendMessage(serializedMessage);
            sendSuccess = sendFuture.get(); // Wait for completion
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        CommunicationResponse response;
        response.messageId = message.messageId;
        response.success = sendSuccess;
        response.timestamp = std::chrono::system_clock::now();
        response.responseTime = duration;
        
        if (!sendSuccess) {
            response.errorMessage = "Failed to send message";
        }
        
        updateStatistics(response);
        promise->set_value(response);
        
    } catch (const std::exception& e) {
        CommunicationResponse response;
        response.messageId = message.messageId;
        response.success = false;
        response.errorMessage = e.what();
        response.timestamp = std::chrono::system_clock::now();
        promise->set_value(response);
    }
    
    return future;
}

CommunicationResponse TcpCommunicator::sendMessageSync(const CommunicationMessage& message) {
    auto future = sendMessage(message);
    return future.get();
}

void TcpCommunicator::setMessageCallback(std::function<void(const CommunicationMessage&)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    messageCallback_ = callback;
}

void TcpCommunicator::setConnectionStatusCallback(std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    connectionStatusCallback_ = callback;
}

CommunicationStats TcpCommunicator::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void TcpCommunicator::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = CommunicationStats{};
}

std::vector<CommunicationProtocol> TcpCommunicator::getSupportedProtocols() const {
    return {CommunicationProtocol::TCP};
}

void TcpCommunicator::setQoSParameters(const json& qosParams) {
    qosParameters_ = qosParams;
    spdlog::debug("TcpCommunicator: QoS parameters updated");
}

void TcpCommunicator::setCompressionEnabled(bool enabled) {
    compressionEnabled_.store(enabled);
    spdlog::debug("TcpCommunicator: Compression {}", enabled ? "enabled" : "disabled");
}

void TcpCommunicator::setEncryptionEnabled(bool enabled, const std::string& encryptionKey) {
    encryptionEnabled_.store(enabled);
    encryptionKey_ = encryptionKey;
    spdlog::debug("TcpCommunicator: Encryption {}", enabled ? "enabled" : "disabled");
}

bool TcpCommunicator::startServer() {
    spdlog::info("TcpCommunicator: Starting TCP server on port {}", config_.serverPort);
    
    server_ = std::make_shared<TcpServer>(config_);
    
    // Set up server callbacks
    server_->setClientConnectedCallback([this](const std::string& clientId) {
        spdlog::info("TcpCommunicator: Client connected: {}", clientId);
        handleConnectionStatusChange(true);
    });
    
    server_->setClientDisconnectedCallback([this](const std::string& clientId) {
        spdlog::info("TcpCommunicator: Client disconnected: {}", clientId);
    });
    
    server_->setMessageReceivedCallback([this](const std::string& clientId, const std::string& message) {
        spdlog::debug("TcpCommunicator: Message received from client {}", clientId);
        handleIncomingMessage(message);
    });
    
    server_->setErrorCallback([this](const std::string& error) {
        handleError(error);
    });
    
    bool success = server_->start();
    connected_.store(success);
    
    return success;
}

void TcpCommunicator::stopServer() {
    if (server_) {
        server_->stop();
        connected_.store(false);
    }
}

std::vector<std::string> TcpCommunicator::getConnectedClients() const {
    if (server_) {
        return server_->getConnectedClients();
    }
    return {};
}

bool TcpCommunicator::sendToClient(const std::string& clientId, const CommunicationMessage& message) {
    if (!server_) {
        return false;
    }
    
    std::string serializedMessage = serializeMessage(message);
    return server_->sendToClient(clientId, serializedMessage);
}

bool TcpCommunicator::sendToAllClients(const CommunicationMessage& message) {
    if (!server_) {
        return false;
    }
    
    std::string serializedMessage = serializeMessage(message);
    return server_->sendToAllClients(serializedMessage);
}

bool TcpCommunicator::initializePerformanceComponents() {
    spdlog::debug("TcpCommunicator: Initializing performance components");
    
    try {
        // Initialize memory pools
        if (memoryPoolingEnabled_.load()) {
            auto& poolManager = MemoryPoolManager::getInstance();
            stringPool_ = poolManager.getStringPool();
            if (!stringPool_) {
                spdlog::error("TcpCommunicator: Failed to get string pool");
                return false;
            }
        }
        
        // Initialize serialization optimizer
        if (serializationOptimizationEnabled_.load()) {
            auto& optimizerManager = SerializationOptimizerManager::getInstance();
            serializationOptimizer_ = optimizerManager.getDefaultOptimizer();
            if (!serializationOptimizer_) {
                spdlog::error("TcpCommunicator: Failed to get serialization optimizer");
                return false;
            }
        }
        
        // Initialize message batcher
        if (messageBatchingEnabled_.load() && config_.enableMessageBatching) {
            MessageBatcherConfig batchConfig;
            batchConfig.maxBatchSize = config_.maxBatchSize;
            batchConfig.batchTimeout = config_.batchTimeout;
            batchConfig.enableDestinationBatching = true;
            
            messageBatcher_ = std::make_shared<MessageBatcher>(batchConfig);
            
            messageBatcher_->setBatchReadyCallback([this](const MessageBatch& batch) {
                // Process batch - send all messages in the batch
                for (const auto& message : batch.messages) {
                    // Convert back to CommunicationMessage and send
                    // This is a simplified implementation
                    spdlog::debug("TcpCommunicator: Processing batched message: {}", message.id);
                }
            });
            
            if (!messageBatcher_->start()) {
                spdlog::error("TcpCommunicator: Failed to start message batcher");
                return false;
            }
        }
        
        spdlog::info("TcpCommunicator: Performance components initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("TcpCommunicator: Exception initializing performance components: {}", e.what());
        return false;
    }
}

void TcpCommunicator::shutdownPerformanceComponents() {
    spdlog::debug("TcpCommunicator: Shutting down performance components");
    
    if (messageBatcher_) {
        messageBatcher_->stop();
        messageBatcher_.reset();
    }
    
    serializationOptimizer_.reset();
    stringPool_.reset();
    connectionPool_.reset();
}

std::string TcpCommunicator::serializeMessage(const CommunicationMessage& message) {
    try {
        json messageJson;
        messageJson["messageId"] = message.messageId;
        messageJson["deviceId"] = message.deviceId;
        messageJson["command"] = message.command;
        messageJson["payload"] = message.payload;
        messageJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.timestamp.time_since_epoch()).count();
        messageJson["priority"] = message.priority;
        // messageJson["metadata"] = message.metadata; // Not available in current CommunicationMessage
        
        if (serializationOptimizer_) {
            return serializationOptimizer_->serialize(messageJson);
        } else {
            return messageJson.dump();
        }
        
    } catch (const std::exception& e) {
        spdlog::error("TcpCommunicator: Failed to serialize message: {}", e.what());
        return "";
    }
}

CommunicationMessage TcpCommunicator::deserializeMessage(const std::string& data) {
    CommunicationMessage message;
    
    try {
        json messageJson;
        
        if (serializationOptimizer_) {
            messageJson = serializationOptimizer_->deserialize(data);
        } else {
            messageJson = json::parse(data);
        }
        
        if (messageJson.contains("messageId")) message.messageId = messageJson["messageId"];
        if (messageJson.contains("deviceId")) message.deviceId = messageJson["deviceId"];
        if (messageJson.contains("command")) message.command = messageJson["command"];
        if (messageJson.contains("payload")) message.payload = messageJson["payload"];
        if (messageJson.contains("timestamp")) {
            message.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(messageJson["timestamp"]));
        }
        if (messageJson.contains("priority")) message.priority = messageJson["priority"];
        // if (messageJson.contains("metadata")) message.metadata = messageJson["metadata"]; // Not available
        
    } catch (const std::exception& e) {
        spdlog::error("TcpCommunicator: Failed to deserialize message: {}", e.what());
        message.messageId = generateMessageId();
        message.command = "error";
        message.payload = json{{"error", "Failed to deserialize message"}, {"raw_data", data}};
        message.timestamp = std::chrono::system_clock::now();
    }
    
    return message;
}

void TcpCommunicator::updateStatistics(const CommunicationResponse& response) {
    std::lock_guard<std::mutex> lock(statsMutex_);

    if (response.success) {
        stats_.messagesSent++;
    } else {
        stats_.messagesError++;
    }

    // Update response time statistics
    if (response.responseTime.count() > 0) {
        double currentResponseTime = response.responseTime.count();

        // Update average response time
        stats_.averageResponseTime = (stats_.averageResponseTime + currentResponseTime) / 2.0;

        // Update min/max response times
        if (stats_.minResponseTime == 0.0 || currentResponseTime < stats_.minResponseTime) {
            stats_.minResponseTime = currentResponseTime;
        }
        if (currentResponseTime > stats_.maxResponseTime) {
            stats_.maxResponseTime = currentResponseTime;
        }
    }

    stats_.lastActivity = response.timestamp;
}

std::string TcpCommunicator::generateMessageId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "tcp_";
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

void TcpCommunicator::handleIncomingMessage(const std::string& rawMessage) {
    try {
        CommunicationMessage message = deserializeMessage(rawMessage);
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.messagesReceived++;
            stats_.lastActivity = std::chrono::system_clock::now();
        }
        
        // Call message callback
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            if (messageCallback_) {
                messageCallback_(message);
            }
        }
        
        spdlog::debug("TcpCommunicator: Processed incoming message: {}", message.messageId);
        
    } catch (const std::exception& e) {
        spdlog::error("TcpCommunicator: Error processing incoming message: {}", e.what());
        handleError("Failed to process incoming message: " + std::string(e.what()));
    }
}

void TcpCommunicator::handleConnectionStatusChange(bool connected) {
    connected_.store(connected);
    
    {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        if (connectionStatusCallback_) {
            connectionStatusCallback_(connected);
        }
    }
    
    spdlog::info("TcpCommunicator: Connection status changed: {}", connected ? "connected" : "disconnected");
}

void TcpCommunicator::handleError(const std::string& error) {
    spdlog::error("TcpCommunicator: Error: {}", error);

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.messagesError++;
    }
}

// TCP-specific method implementations
void TcpCommunicator::setTcpConfiguration(const TcpConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
    spdlog::info("TcpCommunicator: TCP configuration updated");
}

TcpConnectionConfig TcpCommunicator::getTcpConfiguration() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

TcpConnectionMetrics TcpCommunicator::getTcpMetrics() const {
    TcpConnectionMetrics metrics;

    if (clientSession_) {
        metrics = clientSession_->getMetrics();
    } else if (server_) {
        metrics = server_->getAggregatedMetrics();
    }

    return metrics;
}

json TcpCommunicator::getDetailedTcpMetrics() const {
    if (server_) {
        return server_->getDetailedMetrics();
    } else if (clientSession_) {
        json metrics = json::object();
        metrics["client"] = clientSession_->getMetrics().toJson();
        return metrics;
    }

    return json::object();
}

// Performance optimization method implementations
void TcpCommunicator::enableConnectionPooling(bool enabled) {
    connectionPoolingEnabled_.store(enabled);
    spdlog::debug("TcpCommunicator: Connection pooling {}", enabled ? "enabled" : "disabled");
}

void TcpCommunicator::enableMessageBatching(bool enabled) {
    messageBatchingEnabled_.store(enabled);
    spdlog::debug("TcpCommunicator: Message batching {}", enabled ? "enabled" : "disabled");
}

void TcpCommunicator::enableMemoryPooling(bool enabled) {
    memoryPoolingEnabled_.store(enabled);
    spdlog::debug("TcpCommunicator: Memory pooling {}", enabled ? "enabled" : "disabled");
}

void TcpCommunicator::enableSerializationOptimization(bool enabled) {
    serializationOptimizationEnabled_.store(enabled);
    spdlog::debug("TcpCommunicator: Serialization optimization {}", enabled ? "enabled" : "disabled");
}

// TcpServer implementation
TcpServer::TcpServer(const TcpConnectionConfig& config)
    : config_(config) {
    spdlog::debug("TcpServer: Created server for port {}", config_.serverPort);
}

TcpServer::~TcpServer() {
    spdlog::debug("TcpServer: Destructor called");
    stop();
}

bool TcpServer::start() {
    if (running_.load()) {
        spdlog::warn("TcpServer: Already running");
        return true;
    }

    spdlog::info("TcpServer: Starting server on {}:{}", config_.bindInterface, config_.serverPort);

    if (!setupServerSocket()) {
        return false;
    }

    running_.store(true);

    // Start accept thread
    acceptThread_ = std::make_unique<std::thread>(&TcpServer::acceptThreadFunction, this);

    spdlog::info("TcpServer: Server started successfully");
    return true;
}

void TcpServer::stop() {
    if (!running_.load()) {
        return;
    }

    spdlog::info("TcpServer: Stopping server");

    running_.store(false);

    // Close server socket to break accept loop
    if (serverSocket_ != 0) {
#ifdef _WIN32
        closesocket(static_cast<SOCKET>(serverSocket_));
        serverSocket_ = 0;
#else
        if (serverSocket_ >= 0) {
            close(serverSocket_);
            serverSocket_ = -1;
        }
#endif
    }

    // Wait for accept thread
    if (acceptThread_ && acceptThread_->joinable()) {
        acceptThread_->join();
    }

    // Disconnect all clients
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& [clientId, client] : clients_) {
            client->disconnect();
        }
        clients_.clear();
    }

    spdlog::info("TcpServer: Server stopped");
}

std::vector<std::string> TcpServer::getConnectedClients() const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::vector<std::string> clientIds;

    for (const auto& [clientId, client] : clients_) {
        if (client->isConnected()) {
            clientIds.push_back(clientId);
        }
    }

    return clientIds;
}

bool TcpServer::sendToClient(const std::string& clientId, const std::string& message) {
    std::lock_guard<std::mutex> lock(clientsMutex_);

    auto it = clients_.find(clientId);
    if (it != clients_.end() && it->second->isConnected()) {
        return it->second->sendMessageSync(message);
    }

    return false;
}

bool TcpServer::sendToAllClients(const std::string& message) {
    std::lock_guard<std::mutex> lock(clientsMutex_);

    bool allSuccess = true;
    for (auto& [clientId, client] : clients_) {
        if (client->isConnected()) {
            if (!client->sendMessageSync(message)) {
                allSuccess = false;
            }
        }
    }

    return allSuccess;
}

void TcpServer::disconnectClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);

    auto it = clients_.find(clientId);
    if (it != clients_.end()) {
        it->second->disconnect();
        clients_.erase(it);
    }
}

void TcpServer::setClientConnectedCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    clientConnectedCallback_ = callback;
}

void TcpServer::setClientDisconnectedCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    clientDisconnectedCallback_ = callback;
}

void TcpServer::setMessageReceivedCallback(std::function<void(const std::string&, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    messageReceivedCallback_ = callback;
}

void TcpServer::setErrorCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    errorCallback_ = callback;
}

size_t TcpServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return clients_.size();
}

TcpConnectionMetrics TcpServer::getAggregatedMetrics() const {
    std::lock_guard<std::mutex> lock(clientsMutex_);

    TcpConnectionMetrics aggregated;
    size_t clientCount = 0;
    double totalLatency = 0.0;

    for (const auto& [clientId, client] : clients_) {
        auto clientMetrics = client->getMetrics();
        aggregated.connectionsEstablished.store(
            aggregated.connectionsEstablished.load() + clientMetrics.connectionsEstablished.load());
        aggregated.connectionsDropped.store(
            aggregated.connectionsDropped.load() + clientMetrics.connectionsDropped.load());
        aggregated.messagesSent.store(
            aggregated.messagesSent.load() + clientMetrics.messagesSent.load());
        aggregated.messagesReceived.store(
            aggregated.messagesReceived.load() + clientMetrics.messagesReceived.load());
        aggregated.bytesSent.store(
            aggregated.bytesSent.load() + clientMetrics.bytesSent.load());
        aggregated.bytesReceived.store(
            aggregated.bytesReceived.load() + clientMetrics.bytesReceived.load());
        aggregated.errorCount.store(
            aggregated.errorCount.load() + clientMetrics.errorCount.load());
        aggregated.timeoutCount.store(
            aggregated.timeoutCount.load() + clientMetrics.timeoutCount.load());

        // Accumulate latency for averaging
        totalLatency += clientMetrics.averageLatency.load();
        clientCount++;
    }

    // Calculate average latency
    if (clientCount > 0) {
        aggregated.averageLatency.store(totalLatency / clientCount);
    }

    aggregated.lastActivity = std::chrono::system_clock::now();

    return aggregated;
}

json TcpServer::getDetailedMetrics() const {
    json metrics = json::object();

    auto aggregated = getAggregatedMetrics();
    metrics["aggregated"] = aggregated.toJson();

    std::lock_guard<std::mutex> lock(clientsMutex_);
    metrics["clients"] = json::object();

    for (const auto& [clientId, client] : clients_) {
        metrics["clients"][clientId] = client->getMetrics().toJson();
    }

    metrics["serverInfo"] = json::object();
    metrics["serverInfo"]["port"] = config_.serverPort;
    metrics["serverInfo"]["bindInterface"] = config_.bindInterface;
    metrics["serverInfo"]["clientCount"] = clients_.size();
    metrics["serverInfo"]["running"] = running_.load();

    return metrics;
}

bool TcpServer::setupServerSocket() {
    try {
#ifdef _WIN32
        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            spdlog::error("TcpServer: WSAStartup failed: {}", result);
            return false;
        }

        // Create socket
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            spdlog::error("TcpServer: Socket creation failed: {}", WSAGetLastError());
            WSACleanup();
            return false;
        }

        serverSocket_ = static_cast<uintptr_t>(sock);
#else
        // Create socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            spdlog::error("TcpServer: Socket creation failed: {}", strerror(errno));
            return false;
        }

        serverSocket_ = sock;
#endif

        // Set socket options
        if (config_.reuseAddress) {
            int reuseAddr = 1;
#ifdef _WIN32
            setsockopt(static_cast<SOCKET>(serverSocket_), SOL_SOCKET, SO_REUSEADDR,
                      reinterpret_cast<const char*>(&reuseAddr), sizeof(reuseAddr));
#else
            setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));
#endif
        }

        // Bind socket
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(config_.serverPort);

        if (config_.bindInterface == "0.0.0.0") {
            serverAddr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, config_.bindInterface.c_str(), &serverAddr.sin_addr);
        }

#ifdef _WIN32
        int bindResult = bind(static_cast<SOCKET>(serverSocket_),
                             reinterpret_cast<struct sockaddr*>(&serverAddr),
                             sizeof(serverAddr));
        if (bindResult == SOCKET_ERROR) {
            spdlog::error("TcpServer: Bind failed: {}", WSAGetLastError());
            closesocket(static_cast<SOCKET>(serverSocket_));
            WSACleanup();
            return false;
        }
#else
        int bindResult = bind(serverSocket_,
                             reinterpret_cast<struct sockaddr*>(&serverAddr),
                             sizeof(serverAddr));
        if (bindResult < 0) {
            spdlog::error("TcpServer: Bind failed: {}", strerror(errno));
            close(serverSocket_);
            return false;
        }
#endif

        // Listen for connections
#ifdef _WIN32
        int listenResult = listen(static_cast<SOCKET>(serverSocket_), static_cast<int>(config_.maxConnections));
        if (listenResult == SOCKET_ERROR) {
            spdlog::error("TcpServer: Listen failed: {}", WSAGetLastError());
            closesocket(static_cast<SOCKET>(serverSocket_));
            WSACleanup();
            return false;
        }
#else
        int listenResult = listen(serverSocket_, static_cast<int>(config_.maxConnections));
        if (listenResult < 0) {
            spdlog::error("TcpServer: Listen failed: {}", strerror(errno));
            close(serverSocket_);
            return false;
        }
#endif

        return true;

    } catch (const std::exception& e) {
        spdlog::error("TcpServer: Exception setting up server socket: {}", e.what());
        return false;
    }
}

void TcpServer::acceptThreadFunction() {
    spdlog::debug("TcpServer: Accept thread started");

    while (running_.load()) {
        try {
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);

#ifdef _WIN32
            SOCKET clientSocket = accept(static_cast<SOCKET>(serverSocket_),
                                        reinterpret_cast<struct sockaddr*>(&clientAddr),
                                        &clientAddrLen);

            if (clientSocket == INVALID_SOCKET) {
                if (running_.load()) {
                    spdlog::error("TcpServer: Accept failed: {}", WSAGetLastError());
                }
                continue;
            }

            handleNewConnection(static_cast<int>(clientSocket));
#else
            int clientSocket = accept(serverSocket_,
                                     reinterpret_cast<struct sockaddr*>(&clientAddr),
                                     &clientAddrLen);

            if (clientSocket < 0) {
                if (running_.load()) {
                    spdlog::error("TcpServer: Accept failed: {}", strerror(errno));
                }
                continue;
            }

            handleNewConnection(clientSocket);
#endif

        } catch (const std::exception& e) {
            if (running_.load()) {
                spdlog::error("TcpServer: Exception in accept thread: {}", e.what());
            }
        }
    }

    spdlog::debug("TcpServer: Accept thread stopped");
}

void TcpServer::handleNewConnection(int clientSocket) {
    std::string clientId = generateClientId();

    spdlog::info("TcpServer: New client connection: {}", clientId);

    // Create client session
    auto clientSession = std::make_shared<TcpClientSession>(clientId);

    // Set up callbacks
    clientSession->setMessageCallback([this, clientId](const std::string& message) {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        if (messageReceivedCallback_) {
            messageReceivedCallback_(clientId, message);
        }
    });

    clientSession->setConnectionStatusCallback([this, clientId](bool connected) {
        if (!connected) {
            cleanupClient(clientId);
        }
    });

    clientSession->setErrorCallback([this, clientId](const std::string& error) {
        spdlog::error("TcpServer: Client {} error: {}", clientId, error);
        cleanupClient(clientId);
    });

    // Store client session
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_[clientId] = clientSession;
    }

    // Notify client connected
    {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        if (clientConnectedCallback_) {
            clientConnectedCallback_(clientId);
        }
    }

    // TODO: Initialize client session with existing socket
    // This would require modifying TcpClientSession to accept an existing socket
    // For now, we'll close the socket as the session will create its own
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(clientSocket));
#else
    close(clientSocket);
#endif
}

std::string TcpServer::generateClientId() {
    static std::atomic<size_t> counter{0};
    return "client_" + std::to_string(counter.fetch_add(1));
}

void TcpServer::cleanupClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);

    auto it = clients_.find(clientId);
    if (it != clients_.end()) {
        it->second->disconnect();
        clients_.erase(it);

        // Notify client disconnected
        {
            std::lock_guard<std::mutex> callbackLock(callbacksMutex_);
            if (clientDisconnectedCallback_) {
                clientDisconnectedCallback_(clientId);
            }
        }

        spdlog::info("TcpServer: Cleaned up client: {}", clientId);
    }
}

// TcpCommunicatorFactory implementation
std::shared_ptr<TcpCommunicator> TcpCommunicatorFactory::createClient(const TcpConnectionConfig& config) {
    TcpConnectionConfig clientConfig = config;
    clientConfig.isServer = false;

    auto communicator = std::make_shared<TcpCommunicator>(clientConfig);
    spdlog::info("TcpCommunicatorFactory: Created TCP client for {}:{}",
                 config.serverAddress, config.serverPort);

    return communicator;
}

std::shared_ptr<TcpCommunicator> TcpCommunicatorFactory::createServer(const TcpConnectionConfig& config) {
    TcpConnectionConfig serverConfig = config;
    serverConfig.isServer = true;

    auto communicator = std::make_shared<TcpCommunicator>(serverConfig);
    spdlog::info("TcpCommunicatorFactory: Created TCP server on port {}", config.serverPort);

    return communicator;
}

TcpConnectionConfig TcpCommunicatorFactory::createDefaultClientConfig(const std::string& host, uint16_t port) {
    TcpConnectionConfig config;
    config.serverAddress = host;
    config.serverPort = port;
    config.isServer = false;
    config.connectTimeout = std::chrono::milliseconds{5000};
    config.readTimeout = std::chrono::milliseconds{30000};
    config.writeTimeout = std::chrono::milliseconds{5000};
    config.enableKeepAlive = true;
    config.enableNagle = false;
    config.enableMessageBatching = true;

    return config;
}

TcpConnectionConfig TcpCommunicatorFactory::createDefaultServerConfig(uint16_t port, const std::string& bindInterface) {
    TcpConnectionConfig config;
    config.serverPort = port;
    config.bindInterface = bindInterface;
    config.isServer = true;
    config.maxConnections = 100;
    config.reuseAddress = true;
    config.enableKeepAlive = true;
    config.enableNagle = false;
    config.enableMessageBatching = true;

    return config;
}

std::shared_ptr<TcpCommunicator> TcpCommunicatorFactory::createWithPerformanceOptimization(
    const TcpConnectionConfig& config,
    bool enableConnectionPooling,
    bool enableMessageBatching,
    bool enableMemoryPooling,
    bool enableSerializationOptimization) {

    auto communicator = std::make_shared<TcpCommunicator>(config);

    communicator->enableConnectionPooling(enableConnectionPooling);
    communicator->enableMessageBatching(enableMessageBatching);
    communicator->enableMemoryPooling(enableMemoryPooling);
    communicator->enableSerializationOptimization(enableSerializationOptimization);

    spdlog::info("TcpCommunicatorFactory: Created TCP communicator with performance optimizations");

    return communicator;
}

TcpConnectionConfig TcpCommunicatorFactory::createHighPerformanceConfig() {
    TcpConnectionConfig config;
    config.bufferSize = 65536; // 64KB buffer
    config.enableKeepAlive = true;
    config.keepAliveInterval = std::chrono::seconds{10};
    config.keepAliveProbes = 5;
    config.keepAliveTimeout = std::chrono::seconds{5};
    config.enableNagle = false; // Disable for low latency
    config.enableMessageBatching = true;
    config.maxBatchSize = 100;
    config.batchTimeout = std::chrono::milliseconds{50};
    config.connectTimeout = std::chrono::milliseconds{2000};
    config.readTimeout = std::chrono::milliseconds{10000};
    config.writeTimeout = std::chrono::milliseconds{2000};

    return config;
}

TcpConnectionConfig TcpCommunicatorFactory::createSecureConfig(const std::string& certPath, const std::string& keyPath) {
    TcpConnectionConfig config;
    config.enableSSL = true;
    config.sslCertPath = certPath;
    config.sslKeyPath = keyPath;
    config.enableKeepAlive = true;
    config.enableNagle = false;
    config.connectTimeout = std::chrono::milliseconds{10000}; // Longer timeout for SSL handshake

    return config;
}

// TcpConnectionManager implementation
TcpConnectionManager& TcpConnectionManager::getInstance() {
    static TcpConnectionManager instance;
    return instance;
}

void TcpConnectionManager::registerConnection(const std::string& name, std::shared_ptr<TcpCommunicator> communicator) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_[name] = communicator;
    spdlog::info("TcpConnectionManager: Registered connection: {}", name);
}

void TcpConnectionManager::unregisterConnection(const std::string& name) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto it = connections_.find(name);
    if (it != connections_.end()) {
        it->second->disconnect();
        connections_.erase(it);
        spdlog::info("TcpConnectionManager: Unregistered connection: {}", name);
    }
}

std::shared_ptr<TcpCommunicator> TcpConnectionManager::getConnection(const std::string& name) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto it = connections_.find(name);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

json TcpConnectionManager::getAllConnectionMetrics() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    json metrics = json::object();

    for (const auto& [name, connection] : connections_) {
        metrics[name] = connection->getTcpMetrics().toJson();
    }

    return metrics;
}

} // namespace protocols
} // namespace communication
} // namespace core
} // namespace hydrogen

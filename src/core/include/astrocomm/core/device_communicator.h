#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <future>
#include <nlohmann/json.hpp>

namespace astrocomm {
namespace core {

using json = nlohmann::json;

/**
 * @brief Communication protocol types
 */
enum class CommunicationProtocol {
    WEBSOCKET,          // WebSocket protocol
    TCP,                // Raw TCP socket
    UDP,                // UDP socket
    SERIAL,             // Serial communication
    USB,                // USB communication
    BLUETOOTH,          // Bluetooth communication
    HTTP,               // HTTP REST API
    MQTT,               // MQTT messaging
    CUSTOM              // Custom protocol
};

/**
 * @brief Communication message structure
 */
struct CommunicationMessage {
    std::string messageId;
    std::string deviceId;
    std::string command;
    json payload;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds timeout{30000};
    int priority = 0; // Higher values = higher priority
    
    json toJson() const;
    static CommunicationMessage fromJson(const json& j);
};

/**
 * @brief Communication response structure
 */
struct CommunicationResponse {
    std::string messageId;
    std::string deviceId;
    bool success = false;
    std::string errorCode;
    std::string errorMessage;
    json payload;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds responseTime{0};
    
    json toJson() const;
    static CommunicationResponse fromJson(const json& j);
};

/**
 * @brief Communication statistics
 */
struct CommunicationStats {
    uint64_t messagesSent = 0;
    uint64_t messagesReceived = 0;
    uint64_t messagesTimeout = 0;
    uint64_t messagesError = 0;
    double averageResponseTime = 0.0;
    double minResponseTime = 0.0;
    double maxResponseTime = 0.0;
    std::chrono::system_clock::time_point lastActivity;
    
    json toJson() const;
    static CommunicationStats fromJson(const json& j);
};

/**
 * @brief Connection configuration
 */
struct ConnectionConfig {
    CommunicationProtocol protocol;
    std::string address;
    int port = 0;
    std::string path; // For WebSocket/HTTP paths
    json parameters; // Protocol-specific parameters
    std::chrono::milliseconds connectTimeout{10000};
    std::chrono::milliseconds readTimeout{5000};
    std::chrono::milliseconds writeTimeout{5000};
    int maxRetries = 3;
    bool autoReconnect = true;
    std::chrono::milliseconds reconnectDelay{5000};
    
    json toJson() const;
    static ConnectionConfig fromJson(const json& j);
};

/**
 * @brief Abstract device communicator interface
 */
class IDeviceCommunicator {
public:
    virtual ~IDeviceCommunicator() = default;
    
    /**
     * @brief Connect to device
     * @param config Connection configuration
     * @return True if connection successful
     */
    virtual bool connect(const ConnectionConfig& config) = 0;
    
    /**
     * @brief Disconnect from device
     */
    virtual void disconnect() = 0;
    
    /**
     * @brief Check if connected
     * @return True if connected
     */
    virtual bool isConnected() const = 0;
    
    /**
     * @brief Send message to device
     * @param message Message to send
     * @return Future for the response
     */
    virtual std::future<CommunicationResponse> sendMessage(const CommunicationMessage& message) = 0;
    
    /**
     * @brief Send message synchronously
     * @param message Message to send
     * @return Response from device
     */
    virtual CommunicationResponse sendMessageSync(const CommunicationMessage& message) = 0;
    
    /**
     * @brief Register callback for incoming messages
     * @param callback Function to call for incoming messages
     */
    virtual void setMessageCallback(std::function<void(const CommunicationMessage&)> callback) = 0;
    
    /**
     * @brief Register callback for connection status changes
     * @param callback Function to call when connection status changes
     */
    virtual void setConnectionStatusCallback(std::function<void(bool)> callback) = 0;
    
    /**
     * @brief Get communication statistics
     * @return Communication statistics
     */
    virtual CommunicationStats getStatistics() const = 0;
    
    /**
     * @brief Reset communication statistics
     */
    virtual void resetStatistics() = 0;
    
    /**
     * @brief Get supported protocols
     * @return Vector of supported protocols
     */
    virtual std::vector<CommunicationProtocol> getSupportedProtocols() const = 0;
    
    /**
     * @brief Set quality of service parameters
     * @param qosParams QoS parameters as JSON
     */
    virtual void setQoSParameters(const json& qosParams) = 0;
    
    /**
     * @brief Enable/disable message compression
     * @param enabled Whether to enable compression
     */
    virtual void setCompressionEnabled(bool enabled) = 0;
    
    /**
     * @brief Enable/disable message encryption
     * @param enabled Whether to enable encryption
     * @param encryptionKey Encryption key (if applicable)
     */
    virtual void setEncryptionEnabled(bool enabled, const std::string& encryptionKey = "") = 0;
};

/**
 * @brief Device communicator factory
 */
class DeviceCommunicatorFactory {
public:
    /**
     * @brief Create communicator for protocol
     * @param protocol Communication protocol
     * @return Unique pointer to communicator
     */
    static std::unique_ptr<IDeviceCommunicator> createCommunicator(CommunicationProtocol protocol);
    
    /**
     * @brief Register custom communicator factory
     * @param protocol Protocol type
     * @param factory Factory function
     */
    static void registerCommunicatorFactory(CommunicationProtocol protocol,
        std::function<std::unique_ptr<IDeviceCommunicator>()> factory);
    
    /**
     * @brief Get supported protocols
     * @return Vector of supported protocols
     */
    static std::vector<CommunicationProtocol> getSupportedProtocols();

private:
    static std::unordered_map<CommunicationProtocol, 
        std::function<std::unique_ptr<IDeviceCommunicator>()>> factories_;
    static std::mutex factoriesMutex_;
};

/**
 * @brief WebSocket communicator implementation
 */
class WebSocketCommunicator : public IDeviceCommunicator {
public:
    WebSocketCommunicator();
    virtual ~WebSocketCommunicator();
    
    // IDeviceCommunicator implementation
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    std::future<CommunicationResponse> sendMessage(const CommunicationMessage& message) override;
    CommunicationResponse sendMessageSync(const CommunicationMessage& message) override;
    void setMessageCallback(std::function<void(const CommunicationMessage&)> callback) override;
    void setConnectionStatusCallback(std::function<void(bool)> callback) override;
    CommunicationStats getStatistics() const override;
    void resetStatistics() override;
    std::vector<CommunicationProtocol> getSupportedProtocols() const override;
    void setQoSParameters(const json& qosParams) override;
    void setCompressionEnabled(bool enabled) override;
    void setEncryptionEnabled(bool enabled, const std::string& encryptionKey = "") override;

private:
    mutable std::mutex statsMutex_;
    CommunicationStats stats_;
    
    std::atomic<bool> connected_{false};
    ConnectionConfig config_;
    
    // Callbacks
    std::function<void(const CommunicationMessage&)> messageCallback_;
    std::function<void(bool)> connectionStatusCallback_;
    
    // QoS and optimization settings
    json qosParameters_;
    std::atomic<bool> compressionEnabled_{false};
    std::atomic<bool> encryptionEnabled_{false};
    std::string encryptionKey_;
    
    // Helper methods
    void updateStats(const CommunicationResponse& response);
    std::string generateMessageId() const;
};

/**
 * @brief TCP communicator implementation
 */
class TcpCommunicator : public IDeviceCommunicator {
public:
    TcpCommunicator();
    virtual ~TcpCommunicator();
    
    // IDeviceCommunicator implementation
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    std::future<CommunicationResponse> sendMessage(const CommunicationMessage& message) override;
    CommunicationResponse sendMessageSync(const CommunicationMessage& message) override;
    void setMessageCallback(std::function<void(const CommunicationMessage&)> callback) override;
    void setConnectionStatusCallback(std::function<void(bool)> callback) override;
    CommunicationStats getStatistics() const override;
    void resetStatistics() override;
    std::vector<CommunicationProtocol> getSupportedProtocols() const override;
    void setQoSParameters(const json& qosParams) override;
    void setCompressionEnabled(bool enabled) override;
    void setEncryptionEnabled(bool enabled, const std::string& encryptionKey = "") override;

private:
    mutable std::mutex statsMutex_;
    CommunicationStats stats_;
    
    std::atomic<bool> connected_{false};
    ConnectionConfig config_;
    
    // Callbacks
    std::function<void(const CommunicationMessage&)> messageCallback_;
    std::function<void(bool)> connectionStatusCallback_;
    
    // Helper methods
    void updateStats(const CommunicationResponse& response);
    std::string generateMessageId() const;
};

/**
 * @brief Serial communicator implementation
 */
class SerialCommunicator : public IDeviceCommunicator {
public:
    SerialCommunicator();
    virtual ~SerialCommunicator();
    
    // IDeviceCommunicator implementation
    bool connect(const ConnectionConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    std::future<CommunicationResponse> sendMessage(const CommunicationMessage& message) override;
    CommunicationResponse sendMessageSync(const CommunicationMessage& message) override;
    void setMessageCallback(std::function<void(const CommunicationMessage&)> callback) override;
    void setConnectionStatusCallback(std::function<void(bool)> callback) override;
    CommunicationStats getStatistics() const override;
    void resetStatistics() override;
    std::vector<CommunicationProtocol> getSupportedProtocols() const override;
    void setQoSParameters(const json& qosParams) override;
    void setCompressionEnabled(bool enabled) override;
    void setEncryptionEnabled(bool enabled, const std::string& encryptionKey = "") override;

private:
    mutable std::mutex statsMutex_;
    CommunicationStats stats_;
    
    std::atomic<bool> connected_{false};
    ConnectionConfig config_;
    
    // Callbacks
    std::function<void(const CommunicationMessage&)> messageCallback_;
    std::function<void(bool)> connectionStatusCallback_;
    
    // Helper methods
    void updateStats(const CommunicationResponse& response);
    std::string generateMessageId() const;
};

/**
 * @brief Helper functions for communication protocols
 */
std::string communicationProtocolToString(CommunicationProtocol protocol);
CommunicationProtocol stringToCommunicationProtocol(const std::string& protocol);

} // namespace core
} // namespace astrocomm

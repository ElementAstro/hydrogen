#pragma once

#include <hydrogen/core/protocol_communicators.h>
#include <hydrogen/core/device_communicator.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <vector>

namespace hydrogen {
namespace device {
namespace core {

using json = nlohmann::json;
using hydrogen::core::CommunicationProtocol;
using hydrogen::core::CommunicationMessage;
using hydrogen::core::MultiProtocolDeviceCommunicator;

/**
 * @brief Enhanced communication state enumeration
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    ERROR,
    PARTIAL_CONNECTION  // Some protocols connected, others not
};

/**
 * @brief Protocol-aware message handler
 */
using ProtocolMessageHandler = std::function<void(const std::string& message, CommunicationProtocol protocol)>;
using ProtocolConnectionStateHandler = std::function<void(ConnectionState state, CommunicationProtocol protocol, const std::string& error)>;

/**
 * @brief Multi-protocol communication manager
 * 
 * Enhanced communication manager that supports multiple protocols simultaneously:
 * - WebSocket (legacy compatibility)
 * - TCP (raw socket communication)
 * - Stdio (stdin/stdout communication)
 * - MQTT, gRPC, ZeroMQ (via protocol communicators)
 * 
 * Features:
 * - Protocol selection and switching
 * - Automatic fallback mechanisms
 * - Concurrent protocol support
 * - Message routing and broadcasting
 * - Protocol-specific configuration
 */
class MultiProtocolCommunicationManager {
public:
    /**
     * @brief Protocol configuration structure
     */
    struct ProtocolConfiguration {
        CommunicationProtocol protocol;
        json config;
        bool enabled = true;
        bool autoConnect = true;
        int priority = 0;  // Higher priority protocols are preferred
    };

    explicit MultiProtocolCommunicationManager(const std::string& deviceId);
    ~MultiProtocolCommunicationManager();

    // Protocol management
    bool addProtocol(const ProtocolConfiguration& protocolConfig);
    bool removeProtocol(CommunicationProtocol protocol);
    bool enableProtocol(CommunicationProtocol protocol, bool enable = true);
    std::vector<CommunicationProtocol> getActiveProtocols() const;
    std::vector<CommunicationProtocol> getConnectedProtocols() const;

    // Connection management
    bool connect();  // Connect all enabled protocols
    bool connect(CommunicationProtocol protocol);  // Connect specific protocol
    void disconnect();  // Disconnect all protocols
    void disconnect(CommunicationProtocol protocol);  // Disconnect specific protocol
    
    bool isConnected() const;  // Any protocol connected
    bool isConnected(CommunicationProtocol protocol) const;  // Specific protocol connected
    ConnectionState getConnectionState() const;
    ConnectionState getConnectionState(CommunicationProtocol protocol) const;

    // Message handling
    bool sendMessage(const std::string& message);  // Send via primary protocol
    bool sendMessage(const std::string& message, CommunicationProtocol protocol);  // Send via specific protocol
    bool sendMessage(const json& jsonMessage);
    bool sendMessage(const json& jsonMessage, CommunicationProtocol protocol);
    bool broadcastMessage(const std::string& message);  // Send via all connected protocols
    bool broadcastMessage(const json& jsonMessage);

    // Protocol selection and fallback
    void setPrimaryProtocol(CommunicationProtocol protocol);
    CommunicationProtocol getPrimaryProtocol() const;
    void setFallbackProtocols(const std::vector<CommunicationProtocol>& protocols);
    std::vector<CommunicationProtocol> getFallbackProtocols() const;

    // Event handlers
    void setMessageHandler(ProtocolMessageHandler handler);
    void setConnectionStateHandler(ProtocolConnectionStateHandler handler);

    // Auto-reconnection
    void enableAutoReconnect(bool enable = true);
    void setReconnectInterval(int intervalMs);
    void setMaxRetries(int maxRetries);

    // Statistics and monitoring
    json getStatistics() const;
    json getProtocolStatus() const;
    void resetStatistics();

    // Legacy compatibility methods (for existing WebSocket-only code)
    bool connectWebSocket(const std::string& host, uint16_t port);
    void startMessageLoop();
    void stopMessageLoop();
    void setMessageHandler(std::function<void(const std::string&)> handler);  // Legacy handler

    // Configuration helpers (public for factory use)
    json createWebSocketConfig(const std::string& host, uint16_t port) const;
    json createTcpConfig(const std::string& host, uint16_t port, bool isServer = false) const;
    json createStdioConfig() const;

private:
    std::string deviceId_;
    std::unique_ptr<MultiProtocolDeviceCommunicator> communicator_;
    
    // Protocol configurations
    mutable std::mutex configMutex_;
    std::unordered_map<CommunicationProtocol, ProtocolConfiguration> protocolConfigs_;
    
    // Connection state tracking
    mutable std::mutex stateMutex_;
    std::unordered_map<CommunicationProtocol, ConnectionState> protocolStates_;
    
    // Protocol selection
    CommunicationProtocol primaryProtocol_;
    std::vector<CommunicationProtocol> fallbackProtocols_;
    
    // Event handlers
    ProtocolMessageHandler messageHandler_;
    ProtocolConnectionStateHandler connectionStateHandler_;
    std::function<void(const std::string&)> legacyMessageHandler_;  // For compatibility
    
    // Auto-reconnection
    std::atomic<bool> autoReconnectEnabled_;
    std::atomic<int> reconnectInterval_;
    std::atomic<int> maxRetries_;
    std::unordered_map<CommunicationProtocol, int> currentRetries_;
    
    // Threading
    std::atomic<bool> running_;
    std::thread reconnectThread_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    std::unordered_map<CommunicationProtocol, uint64_t> messagesSent_;
    std::unordered_map<CommunicationProtocol, uint64_t> messagesReceived_;
    std::unordered_map<CommunicationProtocol, std::chrono::system_clock::time_point> lastActivity_;

    // Internal methods
    void initializeCommunicator();
    void setupProtocolHandlers();
    void updateConnectionState(CommunicationProtocol protocol, ConnectionState state, const std::string& error = "");
    void handleMessage(const CommunicationMessage& message, CommunicationProtocol protocol);
    void handleConnectionChange(CommunicationProtocol protocol, bool connected);
    void reconnectLoop();
    CommunicationProtocol selectBestProtocol() const;
    bool tryFallbackProtocols(const std::string& message);
};

/**
 * @brief Factory for creating communication managers with common configurations
 */
class CommunicationManagerFactory {
public:
    // Create manager with WebSocket only (legacy compatibility)
    static std::unique_ptr<MultiProtocolCommunicationManager> createWebSocketOnly(
        const std::string& deviceId, const std::string& host, uint16_t port);
    
    // Create manager with TCP only
    static std::unique_ptr<MultiProtocolCommunicationManager> createTcpOnly(
        const std::string& deviceId, const std::string& host, uint16_t port, bool isServer = false);
    
    // Create manager with stdio only
    static std::unique_ptr<MultiProtocolCommunicationManager> createStdioOnly(
        const std::string& deviceId);
    
    // Create manager with multiple protocols
    static std::unique_ptr<MultiProtocolCommunicationManager> createMultiProtocol(
        const std::string& deviceId, const std::vector<MultiProtocolCommunicationManager::ProtocolConfiguration>& configs);
    
    // Create manager with default protocols (WebSocket + TCP fallback)
    static std::unique_ptr<MultiProtocolCommunicationManager> createWithDefaults(
        const std::string& deviceId, const std::string& host, uint16_t port);
};

} // namespace core
} // namespace device
} // namespace hydrogen

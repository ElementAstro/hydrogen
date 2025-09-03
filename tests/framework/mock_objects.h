#pragma once

#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <thread>

namespace hydrogen {
namespace testing {

using json = nlohmann::json;

/**
 * @brief Mock Device for testing device interactions
 */
class MockDevice {
public:
    MOCK_METHOD(std::string, getDeviceId, (), (const));
    MOCK_METHOD(std::string, getDeviceType, (), (const));
    MOCK_METHOD(std::string, getName, (), (const));
    MOCK_METHOD(std::string, getVersion, (), (const));
    MOCK_METHOD(json, getDeviceInfo, (), (const));
    MOCK_METHOD(json, getProperties, (), (const));
    MOCK_METHOD(json, getProperty, (const std::string&), (const));
    MOCK_METHOD(bool, setProperty, (const std::string&, const json&));
    MOCK_METHOD(std::vector<std::string>, getCapabilities, (), (const));
    MOCK_METHOD(bool, hasCapability, (const std::string&), (const));
    MOCK_METHOD(json, executeCommand, (const std::string&, const json&));
    MOCK_METHOD(bool, isOnline, (), (const));
    MOCK_METHOD(bool, connect, ());
    MOCK_METHOD(void, disconnect, ());
    
    // Test helpers
    void setupDefaultBehavior();
    void simulateOffline();
    void simulateError(const std::string& error);
};

/**
 * @brief Mock Device Manager for testing device management
 */
class MockDeviceManager {
public:
    MOCK_METHOD(std::vector<std::string>, getDeviceIds, (), (const));
    MOCK_METHOD(std::shared_ptr<MockDevice>, getDevice, (const std::string&), (const));
    MOCK_METHOD(bool, addDevice, (std::shared_ptr<MockDevice>));
    MOCK_METHOD(bool, removeDevice, (const std::string&));
    MOCK_METHOD(json, discoverDevices, (const std::vector<std::string>&));
    MOCK_METHOD(size_t, getDeviceCount, (), (const));
    MOCK_METHOD(std::vector<std::shared_ptr<MockDevice>>, getDevicesByType, (const std::string&), (const));
    MOCK_METHOD(bool, isDeviceOnline, (const std::string&), (const));
    MOCK_METHOD(json, getDeviceStatistics, (), (const));
    
    void setupDefaultBehavior();
    void addMockDevice(const std::string& deviceId, const std::string& deviceType = "test");
};

/**
 * @brief Mock WebSocket Client for testing WebSocket communications
 */
class MockWebSocketClient {
public:
    MOCK_METHOD(bool, connect, (const std::string&, uint16_t));
    MOCK_METHOD(void, disconnect, ());
    MOCK_METHOD(bool, isConnected, (), (const));
    MOCK_METHOD(bool, send, (const std::string&));
    MOCK_METHOD(bool, send, (const json&));
    MOCK_METHOD(void, setMessageHandler, (std::function<void(const std::string&)>));
    MOCK_METHOD(void, setConnectionHandler, (std::function<void(bool)>));
    MOCK_METHOD(void, setErrorHandler, (std::function<void(const std::string&)>));
    MOCK_METHOD(json, getConnectionInfo, (), (const));
    MOCK_METHOD(json, getStatistics, (), (const));
    
    void setupDefaultBehavior();
    void simulateConnectionFailure();
    void simulateMessage(const std::string& message);
    void simulateMessage(const json& message);
    void simulateDisconnection();
};

/**
 * @brief Mock WebSocket Server for testing server-side WebSocket functionality
 */
class MockWebSocketServer {
public:
    MOCK_METHOD(bool, start, (uint16_t));
    MOCK_METHOD(void, stop, ());
    MOCK_METHOD(bool, isRunning, (), (const));
    MOCK_METHOD(size_t, getClientCount, (), (const));
    MOCK_METHOD(std::vector<std::string>, getClientIds, (), (const));
    MOCK_METHOD(bool, sendToClient, (const std::string&, const std::string&));
    MOCK_METHOD(bool, sendToClient, (const std::string&, const json&));
    MOCK_METHOD(bool, broadcast, (const std::string&));
    MOCK_METHOD(bool, broadcast, (const json&));
    MOCK_METHOD(void, setClientConnectedHandler, (std::function<void(const std::string&)>));
    MOCK_METHOD(void, setClientDisconnectedHandler, (std::function<void(const std::string&)>));
    MOCK_METHOD(void, setMessageHandler, (std::function<void(const std::string&, const std::string&)>));
    
    void setupDefaultBehavior();
    void simulateClientConnection(const std::string& clientId);
    void simulateClientDisconnection(const std::string& clientId);
    void simulateClientMessage(const std::string& clientId, const std::string& message);
};

/**
 * @brief Mock Message Processor for testing message processing
 */
class MockMessageProcessor {
public:
    MOCK_METHOD(bool, processMessage, (const json&));
    MOCK_METHOD(void, setMessageHandler, (const std::string&, std::function<json(const json&)>));
    MOCK_METHOD(void, removeMessageHandler, (const std::string&));
    MOCK_METHOD(std::vector<std::string>, getSupportedMessageTypes, (), (const));
    MOCK_METHOD(json, getProcessingStatistics, (), (const));
    MOCK_METHOD(void, startProcessing, ());
    MOCK_METHOD(void, stopProcessing, ());
    MOCK_METHOD(bool, isProcessing, (), (const));
    MOCK_METHOD(size_t, getQueueSize, (), (const));
    MOCK_METHOD(void, clearQueue, ());
    
    void setupDefaultBehavior();
    void simulateProcessingDelay(std::chrono::milliseconds delay);
    void simulateProcessingError(const std::string& error);
};

/**
 * @brief Mock gRPC Client for testing gRPC communications
 */
class MockGrpcClient {
public:
    MOCK_METHOD(bool, connect, (const std::string&, uint16_t));
    MOCK_METHOD(void, disconnect, ());
    MOCK_METHOD(bool, isConnected, (), (const));
    MOCK_METHOD(json, call, (const std::string&, const json&));
    MOCK_METHOD(void, callAsync, (const std::string&, const json&, std::function<void(const json&)>));
    MOCK_METHOD(bool, startStream, (const std::string&));
    MOCK_METHOD(bool, sendStreamMessage, (const json&));
    MOCK_METHOD(void, stopStream, ());
    MOCK_METHOD(void, setStreamHandler, (std::function<void(const json&)>));
    
    void setupDefaultBehavior();
    void simulateCallResponse(const std::string& method, const json& response);
    void simulateStreamMessage(const json& message);
    void simulateConnectionError();
};

/**
 * @brief Mock MQTT Client for testing MQTT communications
 */
class MockMqttClient {
public:
    MOCK_METHOD(bool, connect, (const std::string&, uint16_t, const std::string&));
    MOCK_METHOD(void, disconnect, ());
    MOCK_METHOD(bool, isConnected, (), (const));
    MOCK_METHOD(bool, publish, (const std::string&, const std::string&, int));
    MOCK_METHOD(bool, publish, (const std::string&, const json&, int));
    MOCK_METHOD(bool, subscribe, (const std::string&, int));
    MOCK_METHOD(bool, unsubscribe, (const std::string&));
    MOCK_METHOD(void, setMessageHandler, (std::function<void(const std::string&, const std::string&)>));
    MOCK_METHOD(void, setConnectionHandler, (std::function<void(bool)>));
    MOCK_METHOD(std::vector<std::string>, getSubscriptions, (), (const));
    
    void setupDefaultBehavior();
    void simulateMessage(const std::string& topic, const std::string& payload);
    void simulateMessage(const std::string& topic, const json& payload);
    void simulateConnectionLoss();
};

/**
 * @brief Mock ZeroMQ Client for testing ZeroMQ communications
 */
class MockZmqClient {
public:
    MOCK_METHOD(bool, connect, (const std::string&));
    MOCK_METHOD(void, disconnect, ());
    MOCK_METHOD(bool, isConnected, (), (const));
    MOCK_METHOD(bool, send, (const std::string&));
    MOCK_METHOD(bool, send, (const json&));
    MOCK_METHOD(bool, receive, (std::string&));
    MOCK_METHOD(bool, receive, (json&));
    MOCK_METHOD(void, setSocketType, (int));
    MOCK_METHOD(void, setReceiveTimeout, (int));
    MOCK_METHOD(void, setSendTimeout, (int));
    MOCK_METHOD(json, getSocketInfo, (), (const));
    
    void setupDefaultBehavior();
    void simulateMessage(const std::string& message);
    void simulateMessage(const json& message);
    void simulateTimeout();
};

/**
 * @brief Mock Connection Manager for testing connection management
 */
class MockConnectionManager {
public:
    MOCK_METHOD(bool, connect, (const std::string&, uint16_t));
    MOCK_METHOD(void, disconnect, ());
    MOCK_METHOD(bool, isConnected, (), (const));
    MOCK_METHOD(bool, reconnect, ());
    MOCK_METHOD(void, setAutoReconnect, (bool));
    MOCK_METHOD(void, setReconnectInterval, (std::chrono::milliseconds));
    MOCK_METHOD(void, setConnectionTimeout, (std::chrono::milliseconds));
    MOCK_METHOD(void, setConnectionHandler, (std::function<void(bool)>));
    MOCK_METHOD(void, setErrorHandler, (std::function<void(const std::string&)>));
    MOCK_METHOD(json, getConnectionStatus, (), (const));
    MOCK_METHOD(json, getConnectionStatistics, (), (const));
    
    void setupDefaultBehavior();
    void simulateConnectionSuccess();
    void simulateConnectionFailure(const std::string& reason);
    void simulateReconnection();
};

/**
 * @brief Mock Configuration Manager for testing configuration management
 */
class MockConfigurationManager {
public:
    MOCK_METHOD(json, getConfiguration, (const std::string&), (const));
    MOCK_METHOD(bool, setConfiguration, (const std::string&, const json&));
    MOCK_METHOD(bool, loadConfiguration, (const std::string&));
    MOCK_METHOD(bool, saveConfiguration, (const std::string&));
    MOCK_METHOD(std::vector<std::string>, getConfigurationKeys, (), (const));
    MOCK_METHOD(bool, hasConfiguration, (const std::string&), (const));
    MOCK_METHOD(void, setConfigurationHandler, (std::function<void(const std::string&, const json&)>));
    MOCK_METHOD(json, getDefaultConfiguration, (), (const));
    MOCK_METHOD(bool, validateConfiguration, (const json&), (const));
    
    void setupDefaultBehavior();
    void addConfiguration(const std::string& key, const json& value);
    void simulateConfigurationChange(const std::string& key, const json& newValue);
};

/**
 * @brief Test Server for integration testing
 */
class TestServer {
public:
    TestServer(uint16_t port = 8080);
    ~TestServer();
    
    bool start();
    void stop();
    bool isRunning() const;
    
    uint16_t getPort() const { return port_; }
    size_t getClientCount() const;
    
    void setMessageHandler(std::function<void(const std::string&, const json&)> handler);
    void setConnectionHandler(std::function<void(const std::string&, bool)> handler);
    
    bool sendToClient(const std::string& clientId, const json& message);
    bool broadcast(const json& message);
    
    json getServerStatistics() const;

private:
    uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread serverThread_;
    
    std::function<void(const std::string&, const json&)> messageHandler_;
    std::function<void(const std::string&, bool)> connectionHandler_;
    
    void serverLoop();
};

/**
 * @brief Test utilities for mock setup and management
 */
class MockTestUtils {
public:
    // Mock lifecycle management
    static void setupAllMocks();
    static void resetAllMocks();
    static void verifyAllMocks();
    
    // Mock behavior configuration
    static void configureMockDefaults();
    static void configureStrictMocks();
    static void configureNiceMocks();
    
    // Mock interaction helpers
    static void expectCall(const std::string& mockName, const std::string& method, int times = 1);
    static void expectNoCall(const std::string& mockName, const std::string& method);
    static void expectAnyCall(const std::string& mockName);
    
    // Mock state verification
    static bool verifyMockState(const std::string& mockName);
    static json getMockStatistics();
    static void printMockReport();
};

} // namespace testing
} // namespace hydrogen

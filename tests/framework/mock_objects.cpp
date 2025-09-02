#include "mock_objects.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

namespace hydrogen {
namespace testing {

// MockDevice implementation
void MockDevice::setupDefaultBehavior() {
    ON_CALL(*this, getDeviceId()).WillByDefault(::testing::Return("mock_device_001"));
    ON_CALL(*this, getDeviceType()).WillByDefault(::testing::Return("mock"));
    ON_CALL(*this, getName()).WillByDefault(::testing::Return("Mock Device"));
    ON_CALL(*this, getVersion()).WillByDefault(::testing::Return("1.0.0"));
    ON_CALL(*this, isOnline()).WillByDefault(::testing::Return(true));
    ON_CALL(*this, connect()).WillByDefault(::testing::Return(true));
    
    json defaultInfo;
    defaultInfo["deviceId"] = "mock_device_001";
    defaultInfo["deviceType"] = "mock";
    defaultInfo["name"] = "Mock Device";
    defaultInfo["version"] = "1.0.0";
    defaultInfo["status"] = "online";
    ON_CALL(*this, getDeviceInfo()).WillByDefault(::testing::Return(defaultInfo));
    
    json defaultProperties;
    defaultProperties["temperature"] = 25.0;
    defaultProperties["humidity"] = 60.0;
    defaultProperties["status"] = "active";
    ON_CALL(*this, getProperties()).WillByDefault(::testing::Return(defaultProperties));
    
    std::vector<std::string> defaultCapabilities = {"read", "write", "monitor"};
    ON_CALL(*this, getCapabilities()).WillByDefault(::testing::Return(defaultCapabilities));
    
    ON_CALL(*this, hasCapability(::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, setProperty(::testing::_, ::testing::_)).WillByDefault(::testing::Return(true));
}

void MockDevice::simulateOffline() {
    ON_CALL(*this, isOnline()).WillByDefault(::testing::Return(false));
    ON_CALL(*this, connect()).WillByDefault(::testing::Return(false));
}

void MockDevice::simulateError(const std::string& error) {
    ON_CALL(*this, executeCommand(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([error](const std::string&, const json&) -> json {
            json response;
            response["error"] = error;
            response["success"] = false;
            return response;
        }));
}

// MockDeviceManager implementation
void MockDeviceManager::setupDefaultBehavior() {
    ON_CALL(*this, getDeviceCount()).WillByDefault(::testing::Return(0));
    ON_CALL(*this, getDeviceIds()).WillByDefault(::testing::Return(std::vector<std::string>{}));
    ON_CALL(*this, addDevice(::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, removeDevice(::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, isDeviceOnline(::testing::_)).WillByDefault(::testing::Return(true));
    
    json defaultStats;
    defaultStats["totalDevices"] = 0;
    defaultStats["onlineDevices"] = 0;
    defaultStats["offlineDevices"] = 0;
    ON_CALL(*this, getDeviceStatistics()).WillByDefault(::testing::Return(defaultStats));
}

void MockDeviceManager::addMockDevice(const std::string& deviceId, const std::string& deviceType) {
    auto mockDevice = std::make_shared<MockDevice>();
    mockDevice->setupDefaultBehavior();
    
    ON_CALL(*mockDevice, getDeviceId()).WillByDefault(::testing::Return(deviceId));
    ON_CALL(*mockDevice, getDeviceType()).WillByDefault(::testing::Return(deviceType));
    
    ON_CALL(*this, getDevice(deviceId)).WillByDefault(::testing::Return(mockDevice));
}

// MockWebSocketClient implementation
void MockWebSocketClient::setupDefaultBehavior() {
    ON_CALL(*this, connect(::testing::_, ::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(true));
    ON_CALL(*this, send(::testing::A<const std::string&>())).WillByDefault(::testing::Return(true));
    ON_CALL(*this, send(::testing::A<const json&>())).WillByDefault(::testing::Return(true));
    
    json defaultConnectionInfo;
    defaultConnectionInfo["host"] = "localhost";
    defaultConnectionInfo["port"] = 8080;
    defaultConnectionInfo["connected"] = true;
    ON_CALL(*this, getConnectionInfo()).WillByDefault(::testing::Return(defaultConnectionInfo));
    
    json defaultStats;
    defaultStats["messagesSent"] = 0;
    defaultStats["messagesReceived"] = 0;
    defaultStats["connectionTime"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ON_CALL(*this, getStatistics()).WillByDefault(::testing::Return(defaultStats));
}

void MockWebSocketClient::simulateConnectionFailure() {
    ON_CALL(*this, connect(::testing::_, ::testing::_)).WillByDefault(::testing::Return(false));
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(false));
}

void MockWebSocketClient::simulateMessage(const std::string& message) {
    // This would trigger the message handler if it was set
    // In a real implementation, this would call the registered message handler
}

void MockWebSocketClient::simulateMessage(const json& message) {
    simulateMessage(message.dump());
}

void MockWebSocketClient::simulateDisconnection() {
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(false));
    // This would trigger the connection handler with false
}

// MockWebSocketServer implementation
void MockWebSocketServer::setupDefaultBehavior() {
    ON_CALL(*this, start(::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, isRunning()).WillByDefault(::testing::Return(true));
    ON_CALL(*this, getClientCount()).WillByDefault(::testing::Return(0));
    ON_CALL(*this, getClientIds()).WillByDefault(::testing::Return(std::vector<std::string>{}));
    ON_CALL(*this, sendToClient(::testing::_, ::testing::A<const std::string&>())).WillByDefault(::testing::Return(true));
    ON_CALL(*this, sendToClient(::testing::_, ::testing::A<const json&>())).WillByDefault(::testing::Return(true));
    ON_CALL(*this, broadcast(::testing::A<const std::string&>())).WillByDefault(::testing::Return(true));
    ON_CALL(*this, broadcast(::testing::A<const json&>())).WillByDefault(::testing::Return(true));
}

void MockWebSocketServer::simulateClientConnection(const std::string& clientId) {
    // This would trigger the client connected handler
}

void MockWebSocketServer::simulateClientDisconnection(const std::string& clientId) {
    // This would trigger the client disconnected handler
}

void MockWebSocketServer::simulateClientMessage(const std::string& clientId, const std::string& message) {
    // This would trigger the message handler with clientId and message
}

// MockMessageProcessor implementation
void MockMessageProcessor::setupDefaultBehavior() {
    ON_CALL(*this, processMessage(::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, getSupportedMessageTypes()).WillByDefault(::testing::Return(std::vector<std::string>{"command", "query", "response"}));
    ON_CALL(*this, isProcessing()).WillByDefault(::testing::Return(true));
    ON_CALL(*this, getQueueSize()).WillByDefault(::testing::Return(0));
    
    json defaultStats;
    defaultStats["messagesProcessed"] = 0;
    defaultStats["processingErrors"] = 0;
    defaultStats["averageProcessingTime"] = 0.0;
    ON_CALL(*this, getProcessingStatistics()).WillByDefault(::testing::Return(defaultStats));
}

void MockMessageProcessor::simulateProcessingDelay(std::chrono::milliseconds delay) {
    ON_CALL(*this, processMessage(::testing::_))
        .WillByDefault(::testing::Invoke([delay](const json&) -> bool {
            std::this_thread::sleep_for(delay);
            return true;
        }));
}

void MockMessageProcessor::simulateProcessingError(const std::string& error) {
    ON_CALL(*this, processMessage(::testing::_)).WillByDefault(::testing::Return(false));
}

// MockGrpcClient implementation
void MockGrpcClient::setupDefaultBehavior() {
    ON_CALL(*this, connect(::testing::_, ::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(true));
    ON_CALL(*this, startStream(::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, sendStreamMessage(::testing::_)).WillByDefault(::testing::Return(true));
    
    ON_CALL(*this, call(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](const std::string& method, const json& request) -> json {
            json response;
            response["method"] = method;
            response["success"] = true;
            response["result"] = "mock_result";
            return response;
        }));
}

void MockGrpcClient::simulateCallResponse(const std::string& method, const json& response) {
    ON_CALL(*this, call(method, ::testing::_)).WillByDefault(::testing::Return(response));
}

void MockGrpcClient::simulateStreamMessage(const json& message) {
    // This would trigger the stream handler
}

void MockGrpcClient::simulateConnectionError() {
    ON_CALL(*this, connect(::testing::_, ::testing::_)).WillByDefault(::testing::Return(false));
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(false));
}

// MockMqttClient implementation
void MockMqttClient::setupDefaultBehavior() {
    ON_CALL(*this, connect(::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(true));
    ON_CALL(*this, publish(::testing::_, ::testing::A<const std::string&>(), ::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, publish(::testing::_, ::testing::A<const json&>(), ::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, subscribe(::testing::_, ::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, unsubscribe(::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, getSubscriptions()).WillByDefault(::testing::Return(std::vector<std::string>{}));
}

void MockMqttClient::simulateMessage(const std::string& topic, const std::string& payload) {
    // This would trigger the message handler
}

void MockMqttClient::simulateMessage(const std::string& topic, const json& payload) {
    simulateMessage(topic, payload.dump());
}

void MockMqttClient::simulateConnectionLoss() {
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(false));
    // This would trigger the connection handler with false
}

// MockZmqClient implementation
void MockZmqClient::setupDefaultBehavior() {
    ON_CALL(*this, connect(::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(true));
    ON_CALL(*this, send(::testing::A<const std::string&>())).WillByDefault(::testing::Return(true));
    ON_CALL(*this, send(::testing::A<const json&>())).WillByDefault(::testing::Return(true));
    ON_CALL(*this, receive(::testing::A<std::string&>())).WillByDefault(::testing::Return(false));
    ON_CALL(*this, receive(::testing::A<json&>())).WillByDefault(::testing::Return(false));
    
    json defaultSocketInfo;
    defaultSocketInfo["type"] = "REQ";
    defaultSocketInfo["endpoint"] = "tcp://localhost:5555";
    defaultSocketInfo["connected"] = true;
    ON_CALL(*this, getSocketInfo()).WillByDefault(::testing::Return(defaultSocketInfo));
}

void MockZmqClient::simulateMessage(const std::string& message) {
    ON_CALL(*this, receive(::testing::A<std::string&>()))
        .WillByDefault(::testing::Invoke([message](std::string& msg) -> bool {
            msg = message;
            return true;
        }));
}

void MockZmqClient::simulateMessage(const json& message) {
    ON_CALL(*this, receive(::testing::A<json&>()))
        .WillByDefault(::testing::Invoke([message](json& msg) -> bool {
            msg = message;
            return true;
        }));
}

void MockZmqClient::simulateTimeout() {
    ON_CALL(*this, receive(::testing::A<std::string&>())).WillByDefault(::testing::Return(false));
    ON_CALL(*this, receive(::testing::A<json&>())).WillByDefault(::testing::Return(false));
}

// MockConnectionManager implementation
void MockConnectionManager::setupDefaultBehavior() {
    ON_CALL(*this, connect(::testing::_, ::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(true));
    ON_CALL(*this, reconnect()).WillByDefault(::testing::Return(true));
    
    json defaultStatus;
    defaultStatus["connected"] = true;
    defaultStatus["host"] = "localhost";
    defaultStatus["port"] = 8080;
    defaultStatus["connectionTime"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ON_CALL(*this, getConnectionStatus()).WillByDefault(::testing::Return(defaultStatus));
    
    json defaultStats;
    defaultStats["connectionAttempts"] = 1;
    defaultStats["successfulConnections"] = 1;
    defaultStats["reconnections"] = 0;
    defaultStats["totalUptime"] = 0;
    ON_CALL(*this, getConnectionStatistics()).WillByDefault(::testing::Return(defaultStats));
}

void MockConnectionManager::simulateConnectionSuccess() {
    ON_CALL(*this, connect(::testing::_, ::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(true));
}

void MockConnectionManager::simulateConnectionFailure(const std::string& reason) {
    ON_CALL(*this, connect(::testing::_, ::testing::_)).WillByDefault(::testing::Return(false));
    ON_CALL(*this, isConnected()).WillByDefault(::testing::Return(false));
}

void MockConnectionManager::simulateReconnection() {
    // This would trigger the connection handler with true after a delay
}

// MockConfigurationManager implementation
void MockConfigurationManager::setupDefaultBehavior() {
    ON_CALL(*this, hasConfiguration(::testing::_)).WillByDefault(::testing::Return(false));
    ON_CALL(*this, setConfiguration(::testing::_, ::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, loadConfiguration(::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, saveConfiguration(::testing::_)).WillByDefault(::testing::Return(true));
    ON_CALL(*this, getConfigurationKeys()).WillByDefault(::testing::Return(std::vector<std::string>{}));
    ON_CALL(*this, validateConfiguration(::testing::_)).WillByDefault(::testing::Return(true));
    
    json defaultConfig;
    defaultConfig["version"] = "1.0.0";
    defaultConfig["debug"] = false;
    ON_CALL(*this, getDefaultConfiguration()).WillByDefault(::testing::Return(defaultConfig));
}

void MockConfigurationManager::addConfiguration(const std::string& key, const json& value) {
    ON_CALL(*this, getConfiguration(key)).WillByDefault(::testing::Return(value));
    ON_CALL(*this, hasConfiguration(key)).WillByDefault(::testing::Return(true));
}

void MockConfigurationManager::simulateConfigurationChange(const std::string& key, const json& newValue) {
    // This would trigger the configuration handler
}

} // namespace testing
} // namespace hydrogen

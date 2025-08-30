# DeviceClient Refactoring Plan

## Overview
This document outlines the refactoring plan for breaking down the large `DeviceClient` class into smaller, more maintainable components following SOLID principles.

## Current Issues
- **DeviceClient class**: 1080+ lines, multiple responsibilities
- **Violates SRP**: Connection, messaging, device management, commands, subscriptions, auth
- **Hard to test**: Tightly coupled functionality
- **Code duplication**: Similar code in `src/client/` and `src/client_component/`

## New Architecture

### 1. ConnectionManager
**Responsibility**: WebSocket connection lifecycle and reconnection logic
**Interface**:
```cpp
class ConnectionManager {
public:
    bool connect(const std::string& host, uint16_t port);
    void disconnect();
    bool isConnected() const;
    void setAutoReconnect(bool enable, int intervalMs = 5000, int maxAttempts = 0);
    json getConnectionStatus() const;
    
    // Callback for connection state changes
    void setConnectionCallback(std::function<void(bool)> callback);
    
private:
    std::unique_ptr<websocket::stream<tcp::socket>> ws;
    net::io_context ioc;
    bool connected;
    // Auto-reconnect logic
    bool enableAutoReconnect;
    int reconnectIntervalMs;
    int maxReconnectAttempts;
    std::thread reconnectThread;
};
```

### 2. MessageProcessor
**Responsibility**: Message sending, receiving, parsing, and processing loop
**Interface**:
```cpp
class MessageProcessor {
public:
    bool sendMessage(const Message& msg);
    void startMessageLoop();
    void stopMessageLoop();
    void registerMessageHandler(MessageType type, std::function<void(const Message&)> handler);
    
    // For synchronous requests
    json sendAndWaitForResponse(const Message& msg, int timeoutSeconds = 10);
    
private:
    ConnectionManager* connectionManager;
    std::thread messageThread;
    std::map<MessageType, std::function<void(const Message&)>> messageHandlers;
    // Response handling for sync requests
    std::mutex responseMutex;
    std::condition_variable responseCV;
    std::map<std::string, json> responses;
};
```

### 3. DeviceManager
**Responsibility**: Device discovery, caching, and property management
**Interface**:
```cpp
class DeviceManager {
public:
    json discoverDevices(const std::vector<std::string>& deviceTypes = {});
    json getDevices() const;
    json getDeviceProperties(const std::string& deviceId, const std::vector<std::string>& properties);
    json setDeviceProperties(const std::string& deviceId, const json& properties);
    
private:
    MessageProcessor* messageProcessor;
    mutable std::mutex devicesMutex;
    json devices; // Device cache
};
```

### 4. CommandExecutor
**Responsibility**: Command execution (sync/async) and batch operations
**Interface**:
```cpp
class CommandExecutor {
public:
    json executeCommand(const std::string& deviceId, const std::string& command,
                       const json& parameters = json::object(),
                       Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE);
    
    void executeCommandAsync(const std::string& deviceId, const std::string& command,
                            const json& parameters = json::object(),
                            Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE,
                            std::function<void(const json&)> callback = nullptr);
    
    json executeBatchCommands(const std::string& deviceId,
                             const std::vector<std::pair<std::string, json>>& commands,
                             bool sequential = true,
                             Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE);
    
private:
    MessageProcessor* messageProcessor;
    std::unique_ptr<MessageQueueManager> messageQueueManager;
    // Async callback handling
    std::mutex callbacksMutex;
    std::map<std::string, std::function<void(const json&)>> asyncCallbacks;
};
```

### 5. SubscriptionManager
**Responsibility**: Property and event subscriptions and callback management
**Interface**:
```cpp
class SubscriptionManager {
public:
    using PropertyCallback = std::function<void(const std::string& deviceId,
                                               const std::string& property, 
                                               const json& value)>;
    using EventCallback = std::function<void(const std::string& deviceId, 
                                            const std::string& event,
                                            const json& details)>;
    
    void subscribeToProperty(const std::string& deviceId, const std::string& property,
                            PropertyCallback callback);
    void unsubscribeFromProperty(const std::string& deviceId, const std::string& property);
    
    void subscribeToEvent(const std::string& deviceId, const std::string& event,
                         EventCallback callback);
    void unsubscribeFromEvent(const std::string& deviceId, const std::string& event);
    
    // Called by MessageProcessor when property/event messages arrive
    void handlePropertyChange(const EventMessage& msg);
    void handleEvent(const EventMessage& msg);
    
private:
    std::mutex subscriptionsMutex;
    std::map<std::string, PropertyCallback> propertySubscriptions;
    std::map<std::string, EventCallback> eventSubscriptions;
};
```

### 6. AuthenticationManager
**Responsibility**: User authentication
**Interface**:
```cpp
class AuthenticationManager {
public:
    bool authenticate(const std::string& method, const std::string& credentials);
    bool isAuthenticated() const;
    json getAuthStatus() const;
    
private:
    MessageProcessor* messageProcessor;
    bool authenticated;
    std::string currentMethod;
};
```

### 7. EventPublisher
**Responsibility**: Publishing events
**Interface**:
```cpp
class EventPublisher {
public:
    void publishEvent(const std::string& eventName,
                     const json& details = json::object(),
                     Message::Priority priority = Message::Priority::NORMAL);
    
private:
    MessageProcessor* messageProcessor;
};
```

## Refactored DeviceClient

The main `DeviceClient` class becomes a facade that coordinates these components:

```cpp
class DeviceClient {
public:
    DeviceClient();
    ~DeviceClient();
    
    // Delegate to ConnectionManager
    bool connect(const std::string& host, uint16_t port);
    void disconnect();
    bool isConnected() const;
    void setAutoReconnect(bool enable, int intervalMs = 5000, int maxAttempts = 0);
    
    // Delegate to DeviceManager
    json discoverDevices(const std::vector<std::string>& deviceTypes = {});
    json getDevices() const;
    json getDeviceProperties(const std::string& deviceId, const std::vector<std::string>& properties);
    json setDeviceProperties(const std::string& deviceId, const json& properties);
    
    // Delegate to CommandExecutor
    json executeCommand(const std::string& deviceId, const std::string& command,
                       const json& parameters = json::object(),
                       Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE);
    void executeCommandAsync(/* ... */);
    json executeBatchCommands(/* ... */);
    
    // Delegate to SubscriptionManager
    void subscribeToProperty(/* ... */);
    void subscribeToEvent(/* ... */);
    
    // Delegate to AuthenticationManager
    bool authenticate(const std::string& method, const std::string& credentials);
    
    // Delegate to EventPublisher
    void publishEvent(/* ... */);
    
    // Control methods
    void run();
    void startMessageProcessing();
    void stopMessageProcessing();
    json getStatusInfo() const;
    
private:
    std::unique_ptr<ConnectionManager> connectionManager;
    std::unique_ptr<MessageProcessor> messageProcessor;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<CommandExecutor> commandExecutor;
    std::unique_ptr<SubscriptionManager> subscriptionManager;
    std::unique_ptr<AuthenticationManager> authManager;
    std::unique_ptr<EventPublisher> eventPublisher;
};
```

## Benefits

1. **Single Responsibility**: Each component has one clear purpose
2. **Easier Testing**: Components can be unit tested in isolation
3. **Better Maintainability**: Changes to one area don't affect others
4. **Reusability**: Components can be reused in different contexts
5. **Clearer Dependencies**: Component relationships are explicit
6. **Easier to Extend**: New functionality can be added without modifying existing components

## Implementation Plan

1. Create base interfaces and component headers
2. Extract ConnectionManager from DeviceClient
3. Extract MessageProcessor from DeviceClient
4. Extract DeviceManager from DeviceClient
5. Extract CommandExecutor from DeviceClient
6. Extract SubscriptionManager from DeviceClient
7. Extract AuthenticationManager from DeviceClient
8. Extract EventPublisher from DeviceClient
9. Refactor main DeviceClient to use components
10. Apply same refactoring to client_component version
11. Create comprehensive unit tests
12. Run integration tests to ensure functionality is preserved

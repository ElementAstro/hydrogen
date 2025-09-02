#pragma once

#include "automatic_adapter.h"
#include "indi_compatibility.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <chrono>
#include <sstream>

namespace hydrogen {
namespace device {
namespace interfaces {
namespace indi {

/**
 * @brief INDI message types for protocol handling
 */
enum class INDIMessageType {
    GET_PROPERTIES,
    NEW_TEXT_VECTOR,
    NEW_NUMBER_VECTOR,
    NEW_SWITCH_VECTOR,
    NEW_BLOB_VECTOR,
    DEF_TEXT_VECTOR,
    DEF_NUMBER_VECTOR,
    DEF_SWITCH_VECTOR,
    DEF_LIGHT_VECTOR,
    DEF_BLOB_VECTOR,
    SET_TEXT_VECTOR,
    SET_NUMBER_VECTOR,
    SET_SWITCH_VECTOR,
    SET_LIGHT_VECTOR,
    SET_BLOB_VECTOR,
    DEL_PROPERTY,
    MESSAGE,
    ENABLE_BLOB
};

/**
 * @brief INDI message structure for protocol communication
 */
struct INDIMessage {
    INDIMessageType type;
    std::string device;
    std::string property;
    std::string element;
    std::string value;
    std::string timestamp;
    PropertyState state;
    std::unordered_map<std::string, std::string> attributes;
    
    std::string toXML() const;
    static INDIMessage fromXML(const std::string& xml);
};

/**
 * @brief INDI client connection handler
 */
class INDIClientConnection {
public:
    INDIClientConnection(int socketFd, const std::string& clientId)
        : socketFd_(socketFd), clientId_(clientId), connected_(true) {}
    
    ~INDIClientConnection() {
        disconnect();
    }
    
    // Send message to client
    bool sendMessage(const INDIMessage& message);
    bool sendXML(const std::string& xml);
    
    // Receive message from client
    std::optional<INDIMessage> receiveMessage();
    
    // Connection management
    void disconnect();
    bool isConnected() const { return connected_.load(); }
    std::string getClientId() const { return clientId_; }

private:
    int socketFd_;
    std::string clientId_;
    std::atomic<bool> connected_;
    std::mutex sendMutex_;
    std::mutex receiveMutex_;
};

/**
 * @brief Automatic INDI protocol bridge for seamless device integration
 */
template<typename DeviceType>
class INDIProtocolBridge {
public:
    explicit INDIProtocolBridge(std::shared_ptr<automatic::INDIAutomaticAdapter<DeviceType>> adapter)
        : adapter_(adapter), running_(false) {
        initializePropertyMappings();
    }
    
    virtual ~INDIProtocolBridge() {
        stop();
    }
    
    // Bridge lifecycle
    void start(int port = 7624) {
        if (running_.load()) return;
        
        running_ = true;
        serverPort_ = port;
        
        // Start server thread
        serverThread_ = std::thread(&INDIProtocolBridge::serverLoop, this);
        
        // Start property synchronization thread
        syncThread_ = std::thread(&INDIProtocolBridge::propertySyncLoop, this);
        
        SPDLOG_INFO("INDI bridge started on port {}", port);
    }
    
    void stop() {
        if (!running_.load()) return;
        
        running_ = false;
        
        // Disconnect all clients
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            for (auto& [clientId, client] : clients_) {
                client->disconnect();
            }
            clients_.clear();
        }
        
        // Stop threads
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        if (syncThread_.joinable()) {
            syncThread_.join();
        }
        
        SPDLOG_INFO("INDI bridge stopped");
    }
    
    // Client management
    void addClient(std::shared_ptr<INDIClientConnection> client) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_[client->getClientId()] = client;
        
        // Send initial property definitions
        sendInitialProperties(client);
        
        SPDLOG_INFO("INDI client connected: {}", client->getClientId());
    }
    
    void removeClient(const std::string& clientId) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clients_.find(clientId);
        if (it != clients_.end()) {
            it->second->disconnect();
            clients_.erase(it);
            SPDLOG_INFO("INDI client disconnected: {}", clientId);
        }
    }
    
    // Property management
    void defineProperty(const PropertyVector& property) {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        properties_[property.name] = property;
        
        // Send to all connected clients
        INDIMessage message;
        message.type = getDefineMessageType(property.type);
        message.device = property.device;
        message.property = property.name;
        message.state = property.state;
        
        broadcastMessage(message);
    }
    
    void updateProperty(const PropertyVector& property) {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        properties_[property.name] = property;
        
        // Send update to all connected clients
        INDIMessage message;
        message.type = getSetMessageType(property.type);
        message.device = property.device;
        message.property = property.name;
        message.state = property.state;
        message.timestamp = getCurrentTimestamp();
        
        broadcastMessage(message);
    }
    
    void deleteProperty(const std::string& propertyName) {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        auto it = properties_.find(propertyName);
        if (it != properties_.end()) {
            INDIMessage message;
            message.type = INDIMessageType::DEL_PROPERTY;
            message.device = it->second.device;
            message.property = propertyName;
            
            broadcastMessage(message);
            properties_.erase(it);
        }
    }
    
    // Message handling
    void processClientMessage(const std::string& clientId, const INDIMessage& message) {
        try {
            switch (message.type) {
                case INDIMessageType::GET_PROPERTIES:
                    handleGetProperties(clientId, message);
                    break;
                case INDIMessageType::NEW_TEXT_VECTOR:
                case INDIMessageType::NEW_NUMBER_VECTOR:
                case INDIMessageType::NEW_SWITCH_VECTOR:
                case INDIMessageType::NEW_BLOB_VECTOR:
                    handleNewProperty(clientId, message);
                    break;
                case INDIMessageType::ENABLE_BLOB:
                    handleEnableBlob(clientId, message);
                    break;
                default:
                    SPDLOG_WARN("Unhandled INDI message type: {}", static_cast<int>(message.type));
                    break;
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error processing INDI message from {}: {}", clientId, e.what());
            sendErrorMessage(clientId, "Error processing message: " + std::string(e.what()));
        }
    }

private:
    std::shared_ptr<automatic::INDIAutomaticAdapter<DeviceType>> adapter_;
    std::atomic<bool> running_;
    int serverPort_;
    
    // Threading
    std::thread serverThread_;
    std::thread syncThread_;
    
    // Client management
    std::mutex clientsMutex_;
    std::unordered_map<std::string, std::shared_ptr<INDIClientConnection>> clients_;
    
    // Property management
    std::mutex propertiesMutex_;
    std::unordered_map<std::string, PropertyVector> properties_;
    
    // Property mappings
    std::unordered_map<std::string, std::string> internalToINDI_;
    std::unordered_map<std::string, std::string> indiToInternal_;
    
    void initializePropertyMappings() {
        // Initialize standard INDI property mappings based on device type
        if constexpr (std::is_same_v<DeviceType, ICamera>) {
            internalToINDI_["exposureDuration"] = "CCD_EXPOSURE";
            internalToINDI_["temperature"] = "CCD_TEMPERATURE";
            internalToINDI_["coolerOn"] = "CCD_COOLER";
            internalToINDI_["binX"] = "CCD_BINNING";
            internalToINDI_["binY"] = "CCD_BINNING";
            internalToINDI_["startX"] = "CCD_FRAME";
            internalToINDI_["startY"] = "CCD_FRAME";
            internalToINDI_["numX"] = "CCD_FRAME";
            internalToINDI_["numY"] = "CCD_FRAME";
        } else if constexpr (std::is_same_v<DeviceType, ITelescope>) {
            internalToINDI_["rightAscension"] = "EQUATORIAL_EOD_COORD";
            internalToINDI_["declination"] = "EQUATORIAL_EOD_COORD";
            internalToINDI_["altitude"] = "HORIZONTAL_COORD";
            internalToINDI_["azimuth"] = "HORIZONTAL_COORD";
            internalToINDI_["tracking"] = "TELESCOPE_TRACK_STATE";
            internalToINDI_["slewing"] = "TELESCOPE_MOTION_NS";
            internalToINDI_["parked"] = "TELESCOPE_PARK";
        } else if constexpr (std::is_same_v<DeviceType, IFocuser>) {
            internalToINDI_["position"] = "ABS_FOCUS_POSITION";
            internalToINDI_["temperature"] = "FOCUS_TEMPERATURE";
            internalToINDI_["isMoving"] = "FOCUS_MOTION";
        }
        
        // Build reverse mapping
        for (const auto& [internal, indi] : internalToINDI_) {
            indiToInternal_[indi] = internal;
        }
    }
    
    void serverLoop() {
        // In real implementation, would create TCP server socket
        // and accept client connections
        SPDLOG_DEBUG("INDI server loop started");
        
        while (running_.load()) {
            // Simulate client connection handling
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Process messages from connected clients
            std::lock_guard<std::mutex> lock(clientsMutex_);
            for (auto it = clients_.begin(); it != clients_.end();) {
                if (!it->second->isConnected()) {
                    SPDLOG_INFO("INDI client {} disconnected", it->first);
                    it = clients_.erase(it);
                } else {
                    // Check for incoming messages
                    auto message = it->second->receiveMessage();
                    if (message) {
                        processClientMessage(it->first, *message);
                    }
                    ++it;
                }
            }
        }
        
        SPDLOG_DEBUG("INDI server loop stopped");
    }
    
    void propertySyncLoop() {
        SPDLOG_DEBUG("INDI property sync loop started");
        
        while (running_.load()) {
            try {
                // Synchronize properties with internal device
                synchronizeProperties();
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Error in property sync loop: {}", e.what());
            }
        }
        
        SPDLOG_DEBUG("INDI property sync loop stopped");
    }
    
    void synchronizeProperties() {
        // Get current property values from internal device
        for (const auto& [internal, indi] : internalToINDI_) {
            try {
                auto indiProperty = adapter_->getINDIProperty(indi);
                updateProperty(indiProperty);
            } catch (const std::exception& e) {
                SPDLOG_DEBUG("Failed to sync property {}: {}", internal, e.what());
            }
        }
    }
    
    void sendInitialProperties(std::shared_ptr<INDIClientConnection> client) {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        for (const auto& [name, property] : properties_) {
            INDIMessage message;
            message.type = getDefineMessageType(property.type);
            message.device = property.device;
            message.property = property.name;
            message.state = property.state;
            
            client->sendMessage(message);
        }
    }
    
    void broadcastMessage(const INDIMessage& message) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const auto& [clientId, client] : clients_) {
            if (client->isConnected()) {
                client->sendMessage(message);
            }
        }
    }
    
    void sendErrorMessage(const std::string& clientId, const std::string& error) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clients_.find(clientId);
        if (it != clients_.end() && it->second->isConnected()) {
            INDIMessage message;
            message.type = INDIMessageType::MESSAGE;
            message.device = adapter_->getDeviceName();
            message.value = error;
            message.timestamp = getCurrentTimestamp();
            
            it->second->sendMessage(message);
        }
    }
    
    void handleGetProperties(const std::string& clientId, const INDIMessage& message) {
        // Send all properties to requesting client
        auto client = clients_[clientId];
        if (client && client->isConnected()) {
            sendInitialProperties(client);
        }
    }
    
    void handleNewProperty(const std::string& clientId, const INDIMessage& message) {
        // Update internal device property
        auto it = indiToInternal_.find(message.property);
        if (it != indiToInternal_.end()) {
            json value = json::parse(message.value);
            adapter_->setProperty(message.property, value, "INDI");
        }
    }
    
    void handleEnableBlob(const std::string& clientId, const INDIMessage& message) {
        // Handle BLOB enable/disable for client
        SPDLOG_DEBUG("BLOB {} for client {}", message.value, clientId);
    }
    
    INDIMessageType getDefineMessageType(PropertyType type) const {
        switch (type) {
            case PropertyType::TEXT: return INDIMessageType::DEF_TEXT_VECTOR;
            case PropertyType::NUMBER: return INDIMessageType::DEF_NUMBER_VECTOR;
            case PropertyType::SWITCH: return INDIMessageType::DEF_SWITCH_VECTOR;
            case PropertyType::LIGHT: return INDIMessageType::DEF_LIGHT_VECTOR;
            case PropertyType::BLOB: return INDIMessageType::DEF_BLOB_VECTOR;
            default: return INDIMessageType::DEF_TEXT_VECTOR;
        }
    }
    
    INDIMessageType getSetMessageType(PropertyType type) const {
        switch (type) {
            case PropertyType::TEXT: return INDIMessageType::SET_TEXT_VECTOR;
            case PropertyType::NUMBER: return INDIMessageType::SET_NUMBER_VECTOR;
            case PropertyType::SWITCH: return INDIMessageType::SET_SWITCH_VECTOR;
            case PropertyType::LIGHT: return INDIMessageType::SET_LIGHT_VECTOR;
            case PropertyType::BLOB: return INDIMessageType::SET_BLOB_VECTOR;
            default: return INDIMessageType::SET_TEXT_VECTOR;
        }
    }
    
    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
        return ss.str();
    }
};

/**
 * @brief INDI device registry for automatic device discovery
 */
class INDIDeviceRegistry {
public:
    static INDIDeviceRegistry& getInstance() {
        static INDIDeviceRegistry instance;
        return instance;
    }
    
    // Register device with INDI
    template<typename DeviceType>
    void registerDevice(const std::string& deviceId, std::shared_ptr<DeviceType> device) {
        auto adapter = std::make_shared<automatic::INDIAutomaticAdapter<DeviceType>>(device);
        auto bridge = std::make_shared<INDIProtocolBridge<DeviceType>>(adapter);
        
        std::lock_guard<std::mutex> lock(registryMutex_);
        registeredDevices_[deviceId] = {adapter, bridge};
        
        // Start bridge
        bridge->start();
        
        SPDLOG_INFO("Registered INDI device: {}", deviceId);
    }
    
    // Unregister device from INDI
    void unregisterDevice(const std::string& deviceId) {
        std::lock_guard<std::mutex> lock(registryMutex_);
        auto it = registeredDevices_.find(deviceId);
        if (it != registeredDevices_.end()) {
            // Stop bridge
            std::static_pointer_cast<INDIProtocolBridge<void>>(it->second.bridge)->stop();
            registeredDevices_.erase(it);
            SPDLOG_INFO("Unregistered INDI device: {}", deviceId);
        }
    }
    
    // Get registered devices
    std::vector<std::string> getRegisteredDevices() const {
        std::lock_guard<std::mutex> lock(registryMutex_);
        std::vector<std::string> devices;
        for (const auto& [deviceId, info] : registeredDevices_) {
            devices.push_back(deviceId);
        }
        return devices;
    }

private:
    struct DeviceInfo {
        std::shared_ptr<void> adapter;
        std::shared_ptr<void> bridge;
    };
    
    mutable std::mutex registryMutex_;
    std::unordered_map<std::string, DeviceInfo> registeredDevices_;
};

} // namespace indi
} // namespace interfaces
} // namespace device
} // namespace hydrogen

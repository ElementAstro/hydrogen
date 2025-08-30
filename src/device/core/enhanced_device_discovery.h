#pragma once

#include "multi_protocol_communication_manager.h"
#include <astrocomm/core/device_health.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

namespace astrocomm {
namespace device {
namespace core {

using json = nlohmann::json;
using astrocomm::core::CommunicationProtocol;
using astrocomm::core::DeviceHealthStatus;

/**
 * @brief Device discovery information
 */
struct DeviceDiscoveryInfo {
    std::string deviceId;
    std::string deviceType;
    std::string manufacturer;
    std::string model;
    std::string firmwareVersion;
    std::string serialNumber;
    
    // Network information
    std::string ipAddress;
    uint16_t port = 0;
    std::vector<CommunicationProtocol> supportedProtocols;
    CommunicationProtocol preferredProtocol = CommunicationProtocol::WEBSOCKET;
    
    // Device capabilities
    std::vector<std::string> capabilities;
    json deviceSpecificInfo;
    
    // Discovery metadata
    std::chrono::system_clock::time_point discoveredAt;
    std::chrono::system_clock::time_point lastSeen;
    std::string discoveryMethod;  // "broadcast", "mdns", "manual", etc.
    bool isOnline = true;
    DeviceHealthStatus healthStatus = DeviceHealthStatus::UNKNOWN;
    
    // Connection information
    bool isConnected = false;
    std::string connectionError;
    
    json toJson() const {
        json j;
        j["deviceId"] = deviceId;
        j["deviceType"] = deviceType;
        j["manufacturer"] = manufacturer;
        j["model"] = model;
        j["firmwareVersion"] = firmwareVersion;
        j["serialNumber"] = serialNumber;
        j["ipAddress"] = ipAddress;
        j["port"] = port;
        j["supportedProtocols"] = supportedProtocols;
        j["preferredProtocol"] = static_cast<int>(preferredProtocol);
        j["capabilities"] = capabilities;
        j["deviceSpecificInfo"] = deviceSpecificInfo;
        j["discoveredAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            discoveredAt.time_since_epoch()).count();
        j["lastSeen"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            lastSeen.time_since_epoch()).count();
        j["discoveryMethod"] = discoveryMethod;
        j["isOnline"] = isOnline;
        j["healthStatus"] = static_cast<int>(healthStatus);
        j["isConnected"] = isConnected;
        j["connectionError"] = connectionError;
        return j;
    }
    
    static DeviceDiscoveryInfo fromJson(const json& j) {
        DeviceDiscoveryInfo info;
        if (j.contains("deviceId")) info.deviceId = j["deviceId"];
        if (j.contains("deviceType")) info.deviceType = j["deviceType"];
        if (j.contains("manufacturer")) info.manufacturer = j["manufacturer"];
        if (j.contains("model")) info.model = j["model"];
        if (j.contains("firmwareVersion")) info.firmwareVersion = j["firmwareVersion"];
        if (j.contains("serialNumber")) info.serialNumber = j["serialNumber"];
        if (j.contains("ipAddress")) info.ipAddress = j["ipAddress"];
        if (j.contains("port")) info.port = j["port"];
        if (j.contains("supportedProtocols")) {
            for (const auto& protocol : j["supportedProtocols"]) {
                info.supportedProtocols.push_back(static_cast<CommunicationProtocol>(protocol.get<int>()));
            }
        }
        if (j.contains("preferredProtocol")) {
            info.preferredProtocol = static_cast<CommunicationProtocol>(j["preferredProtocol"].get<int>());
        }
        if (j.contains("capabilities")) info.capabilities = j["capabilities"];
        if (j.contains("deviceSpecificInfo")) info.deviceSpecificInfo = j["deviceSpecificInfo"];
        if (j.contains("discoveredAt")) {
            auto ms = std::chrono::milliseconds(j["discoveredAt"].get<int64_t>());
            info.discoveredAt = std::chrono::system_clock::time_point(ms);
        }
        if (j.contains("lastSeen")) {
            auto ms = std::chrono::milliseconds(j["lastSeen"].get<int64_t>());
            info.lastSeen = std::chrono::system_clock::time_point(ms);
        }
        if (j.contains("discoveryMethod")) info.discoveryMethod = j["discoveryMethod"];
        if (j.contains("isOnline")) info.isOnline = j["isOnline"];
        if (j.contains("healthStatus")) {
            info.healthStatus = static_cast<DeviceHealthStatus>(j["healthStatus"].get<int>());
        }
        if (j.contains("isConnected")) info.isConnected = j["isConnected"];
        if (j.contains("connectionError")) info.connectionError = j["connectionError"];
        return info;
    }
};

/**
 * @brief Device discovery filter criteria
 */
struct DeviceDiscoveryFilter {
    std::unordered_set<std::string> deviceTypes;
    std::unordered_set<std::string> manufacturers;
    std::unordered_set<CommunicationProtocol> requiredProtocols;
    std::unordered_set<std::string> requiredCapabilities;
    bool onlineOnly = false;
    bool connectedOnly = false;
    DeviceHealthStatus minHealthStatus = DeviceHealthStatus::UNKNOWN;
    std::chrono::milliseconds maxAge{0};  // 0 = no age limit
    
    bool matches(const DeviceDiscoveryInfo& info) const {
        // Check device type filter
        if (!deviceTypes.empty() && deviceTypes.find(info.deviceType) == deviceTypes.end()) {
            return false;
        }
        
        // Check manufacturer filter
        if (!manufacturers.empty() && manufacturers.find(info.manufacturer) == manufacturers.end()) {
            return false;
        }
        
        // Check protocol requirements
        if (!requiredProtocols.empty()) {
            bool hasRequiredProtocol = false;
            for (const auto& protocol : info.supportedProtocols) {
                if (requiredProtocols.find(protocol) != requiredProtocols.end()) {
                    hasRequiredProtocol = true;
                    break;
                }
            }
            if (!hasRequiredProtocol) {
                return false;
            }
        }
        
        // Check capability requirements
        if (!requiredCapabilities.empty()) {
            for (const auto& capability : requiredCapabilities) {
                if (std::find(info.capabilities.begin(), info.capabilities.end(), capability) == info.capabilities.end()) {
                    return false;
                }
            }
        }
        
        // Check online status
        if (onlineOnly && !info.isOnline) {
            return false;
        }
        
        // Check connection status
        if (connectedOnly && !info.isConnected) {
            return false;
        }
        
        // Check health status
        if (static_cast<int>(info.healthStatus) < static_cast<int>(minHealthStatus)) {
            return false;
        }
        
        // Check age
        if (maxAge.count() > 0) {
            auto age = std::chrono::system_clock::now() - info.lastSeen;
            if (age > maxAge) {
                return false;
            }
        }
        
        return true;
    }
};

/**
 * @brief Enhanced device discovery manager
 * 
 * Features:
 * - Multi-protocol device discovery (WebSocket, TCP, mDNS, broadcast)
 * - Real-time device status monitoring
 * - Intelligent caching with TTL
 * - Advanced filtering and search
 * - Automatic device health monitoring
 * - Event-driven notifications
 * - Persistent device registry
 */
class EnhancedDeviceDiscoveryManager {
public:
    /**
     * @brief Discovery event types
     */
    enum class DiscoveryEvent {
        DEVICE_DISCOVERED,
        DEVICE_UPDATED,
        DEVICE_LOST,
        DEVICE_CONNECTED,
        DEVICE_DISCONNECTED,
        HEALTH_STATUS_CHANGED
    };
    
    using DiscoveryEventHandler = std::function<void(DiscoveryEvent event, const DeviceDiscoveryInfo& info)>;
    
    explicit EnhancedDeviceDiscoveryManager();
    ~EnhancedDeviceDiscoveryManager();
    
    // Discovery control
    bool startDiscovery();
    void stopDiscovery();
    bool isDiscoveryActive() const;
    
    // Manual device registration
    bool registerDevice(const DeviceDiscoveryInfo& info);
    bool unregisterDevice(const std::string& deviceId);
    bool updateDeviceInfo(const std::string& deviceId, const DeviceDiscoveryInfo& info);
    
    // Device queries
    std::vector<DeviceDiscoveryInfo> getDiscoveredDevices() const;
    std::vector<DeviceDiscoveryInfo> getDiscoveredDevices(const DeviceDiscoveryFilter& filter) const;
    std::optional<DeviceDiscoveryInfo> getDeviceInfo(const std::string& deviceId) const;
    bool isDeviceDiscovered(const std::string& deviceId) const;
    
    // Device connection management
    bool connectToDevice(const std::string& deviceId);
    bool disconnectFromDevice(const std::string& deviceId);
    bool isDeviceConnected(const std::string& deviceId) const;
    
    // Health monitoring
    void updateDeviceHealth(const std::string& deviceId, DeviceHealthStatus status);
    DeviceHealthStatus getDeviceHealth(const std::string& deviceId) const;
    
    // Event handling
    void setDiscoveryEventHandler(DiscoveryEventHandler handler);
    
    // Configuration
    void setDiscoveryInterval(std::chrono::milliseconds interval);
    void setDeviceTimeout(std::chrono::milliseconds timeout);
    void setCacheSize(size_t maxDevices);
    void enablePersistentRegistry(bool enable, const std::string& filename = "device_registry.json");
    
    // Discovery methods
    void enableBroadcastDiscovery(bool enable = true);
    void enableMdnsDiscovery(bool enable = true);
    void enableTcpScanDiscovery(bool enable = true, const std::vector<std::string>& ipRanges = {});
    void addManualDiscoveryEndpoint(const std::string& host, uint16_t port, CommunicationProtocol protocol);
    
    // Statistics and monitoring
    json getDiscoveryStatistics() const;
    void clearDiscoveryCache();
    
    // Persistence
    bool saveDeviceRegistry(const std::string& filename = "") const;
    bool loadDeviceRegistry(const std::string& filename = "");

private:
    // Device storage
    mutable std::shared_mutex devicesMutex_;
    std::unordered_map<std::string, DeviceDiscoveryInfo> discoveredDevices_;
    
    // Discovery configuration
    std::atomic<bool> discoveryActive_;
    std::chrono::milliseconds discoveryInterval_{5000};
    std::chrono::milliseconds deviceTimeout_{30000};
    size_t maxCacheSize_ = 1000;
    
    // Discovery methods
    std::atomic<bool> broadcastDiscoveryEnabled_{true};
    std::atomic<bool> mdnsDiscoveryEnabled_{true};
    std::atomic<bool> tcpScanDiscoveryEnabled_{false};
    std::vector<std::string> tcpScanRanges_;
    std::vector<std::tuple<std::string, uint16_t, CommunicationProtocol>> manualEndpoints_;
    
    // Threading
    std::thread discoveryThread_;
    std::thread healthMonitorThread_;
    std::atomic<bool> running_;
    
    // Event handling
    DiscoveryEventHandler eventHandler_;
    
    // Persistence
    std::atomic<bool> persistentRegistryEnabled_{false};
    std::string registryFilename_ = "device_registry.json";
    
    // Statistics
    mutable std::mutex statsMutex_;
    struct DiscoveryStats {
        uint64_t totalDiscoveries = 0;
        uint64_t activeDevices = 0;
        uint64_t connectedDevices = 0;
        uint64_t healthyDevices = 0;
        std::unordered_map<std::string, uint32_t> deviceTypeCounts;
        std::unordered_map<std::string, uint32_t> discoveryMethodCounts;
        std::chrono::system_clock::time_point lastDiscoveryTime;
    } stats_;
    
    // Internal methods
    void discoveryLoop();
    void healthMonitorLoop();
    void performBroadcastDiscovery();
    void performMdnsDiscovery();
    void performTcpScanDiscovery();
    void performManualEndpointDiscovery();
    void checkDeviceTimeouts();
    void updateDeviceStatus(const std::string& deviceId, bool online);
    void notifyDiscoveryEvent(DiscoveryEvent event, const DeviceDiscoveryInfo& info);
    void updateStatistics();
    bool validateDeviceInfo(const DeviceDiscoveryInfo& info) const;
    void cleanupExpiredDevices();
    
    // Discovery protocol implementations
    std::vector<DeviceDiscoveryInfo> discoverWebSocketDevices();
    std::vector<DeviceDiscoveryInfo> discoverTcpDevices(const std::string& ipRange);
    std::vector<DeviceDiscoveryInfo> discoverMdnsDevices();
    DeviceDiscoveryInfo queryDeviceInfo(const std::string& host, uint16_t port, CommunicationProtocol protocol);
    
    // Utility methods
    std::vector<std::string> generateIpRange(const std::string& cidr) const;
    bool isValidIpAddress(const std::string& ip) const;
    std::string getLocalIpAddress() const;
};

/**
 * @brief Device discovery factory for common configurations
 */
class DeviceDiscoveryFactory {
public:
    static std::unique_ptr<EnhancedDeviceDiscoveryManager> createDefaultDiscovery();
    static std::unique_ptr<EnhancedDeviceDiscoveryManager> createLocalNetworkDiscovery();
    static std::unique_ptr<EnhancedDeviceDiscoveryManager> createManualDiscovery(
        const std::vector<std::tuple<std::string, uint16_t, CommunicationProtocol>>& endpoints);
};

} // namespace core
} // namespace device
} // namespace astrocomm

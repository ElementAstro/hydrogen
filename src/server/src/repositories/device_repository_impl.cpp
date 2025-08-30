#include "astrocomm/server/repositories/device_repository.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>

namespace astrocomm {
namespace server {
namespace repositories {

/**
 * @brief Concrete implementation of the Device Repository
 */
class DeviceRepositoryImpl : public IDeviceRepository {
public:
    explicit DeviceRepositoryImpl(const std::string& dataPath = "./data/devices.json")
        : dataPath_(dataPath), inTransaction_(false) {
        spdlog::info("Device repository initialized with data path: {}", dataPath_);
    }

    ~DeviceRepositoryImpl() {
        if (inTransaction_) {
            rollbackTransaction();
        }
    }

    // Basic CRUD operations
    bool create(const services::DeviceInfo& device) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        if (devices_.find(device.deviceId) != devices_.end()) {
            spdlog::warn("Device already exists: {}", device.deviceId);
            return false;
        }
        
        devices_[device.deviceId] = device;
        spdlog::info("Device created: {}", device.deviceId);
        return true;
    }

    std::optional<services::DeviceInfo> read(const std::string& deviceId) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it != devices_.end()) {
            return it->second;
        }
        
        return std::nullopt;
    }

    bool update(const services::DeviceInfo& device) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(device.deviceId);
        if (it == devices_.end()) {
            spdlog::warn("Device not found for update: {}", device.deviceId);
            return false;
        }
        
        it->second = device;
        spdlog::info("Device updated: {}", device.deviceId);
        return true;
    }

    bool remove(const std::string& deviceId) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = devices_.find(deviceId);
        if (it == devices_.end()) {
            spdlog::warn("Device not found for removal: {}", deviceId);
            return false;
        }
        
        devices_.erase(it);
        spdlog::info("Device removed: {}", deviceId);
        return true;
    }

    bool exists(const std::string& deviceId) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        return devices_.find(deviceId) != devices_.end();
    }

    // Bulk operations
    std::vector<services::DeviceInfo> getAll() const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<services::DeviceInfo> result;
        for (const auto& pair : devices_) {
            result.push_back(pair.second);
        }
        
        return result;
    }

    std::vector<services::DeviceInfo> getByType(const std::string& deviceType) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<services::DeviceInfo> result;
        for (const auto& pair : devices_) {
            if (pair.second.deviceType == deviceType) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    std::vector<services::DeviceInfo> getByStatus(services::DeviceConnectionStatus status) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<services::DeviceInfo> result;
        for (const auto& pair : devices_) {
            if (pair.second.connectionStatus == status) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    bool createBulk(const std::vector<services::DeviceInfo>& devices) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        for (const auto& device : devices) {
            if (devices_.find(device.deviceId) != devices_.end()) {
                spdlog::warn("Device already exists in bulk create: {}", device.deviceId);
                continue;
            }
            devices_[device.deviceId] = device;
        }
        
        spdlog::info("Bulk created {} devices", devices.size());
        return true;
    }

    bool updateBulk(const std::vector<services::DeviceInfo>& devices) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        size_t updated = 0;
        for (const auto& device : devices) {
            auto it = devices_.find(device.deviceId);
            if (it != devices_.end()) {
                it->second = device;
                updated++;
            }
        }
        
        spdlog::info("Bulk updated {} devices", updated);
        return true;
    }

    bool removeBulk(const std::vector<std::string>& deviceIds) override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        size_t removed = 0;
        for (const auto& deviceId : deviceIds) {
            auto it = devices_.find(deviceId);
            if (it != devices_.end()) {
                devices_.erase(it);
                removed++;
            }
        }
        
        spdlog::info("Bulk removed {} devices", removed);
        return true;
    }

    // Query operations
    std::vector<services::DeviceInfo> findByProperty(const std::string& propertyName, 
                                                   const std::string& propertyValue) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<services::DeviceInfo> result;
        for (const auto& pair : devices_) {
            const auto& device = pair.second;
            auto it = device.properties.find(propertyName);
            if (it != device.properties.end() && it->second == propertyValue) {
                result.push_back(device);
            }
        }
        
        return result;
    }

    std::vector<services::DeviceInfo> findByCapability(const std::string& capability) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<services::DeviceInfo> result;
        for (const auto& pair : devices_) {
            const auto& device = pair.second;
            if (std::find(device.capabilities.begin(), device.capabilities.end(), capability) 
                != device.capabilities.end()) {
                result.push_back(device);
            }
        }
        
        return result;
    }

    std::vector<services::DeviceInfo> findByManufacturer(const std::string& manufacturer) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<services::DeviceInfo> result;
        for (const auto& pair : devices_) {
            if (pair.second.manufacturer == manufacturer) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    size_t count() const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        return devices_.size();
    }

    size_t countByType(const std::string& deviceType) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        size_t count = 0;
        for (const auto& pair : devices_) {
            if (pair.second.deviceType == deviceType) {
                count++;
            }
        }
        
        return count;
    }

    size_t countByStatus(services::DeviceConnectionStatus status) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        size_t count = 0;
        for (const auto& pair : devices_) {
            if (pair.second.connectionStatus == status) {
                count++;
            }
        }
        
        return count;
    }

    // Search operations
    std::vector<services::DeviceInfo> search(const std::string& searchTerm) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<services::DeviceInfo> result;
        std::string lowerSearchTerm = searchTerm;
        std::transform(lowerSearchTerm.begin(), lowerSearchTerm.end(), lowerSearchTerm.begin(), ::tolower);
        
        for (const auto& pair : devices_) {
            const auto& device = pair.second;
            
            // Search in device name, type, manufacturer, model
            std::string searchableText = device.deviceName + " " + device.deviceType + " " + 
                                       device.manufacturer + " " + device.model;
            std::transform(searchableText.begin(), searchableText.end(), searchableText.begin(), ::tolower);
            
            if (searchableText.find(lowerSearchTerm) != std::string::npos) {
                result.push_back(device);
            }
        }
        
        return result;
    }

    std::vector<services::DeviceInfo> findSimilar(const services::DeviceInfo& device, 
                                                double threshold) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<services::DeviceInfo> result;
        for (const auto& pair : devices_) {
            const auto& candidate = pair.second;
            if (candidate.deviceId == device.deviceId) continue;
            
            double similarity = calculateSimilarity(device, candidate);
            if (similarity >= threshold) {
                result.push_back(candidate);
            }
        }
        
        return result;
    }

    // Persistence management
    bool save() override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        try {
            nlohmann::json jsonData;
            for (const auto& pair : devices_) {
                jsonData[pair.first] = deviceToJson(pair.second);
            }
            
            std::ofstream file(dataPath_);
            file << jsonData.dump(2);
            file.close();
            
            spdlog::info("Device repository saved to: {}", dataPath_);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to save device repository: {}", e.what());
            return false;
        }
    }

    bool load() override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        try {
            std::ifstream file(dataPath_);
            if (!file.is_open()) {
                spdlog::warn("Device repository file not found, starting with empty repository");
                return true;
            }
            
            nlohmann::json jsonData;
            file >> jsonData;
            file.close();
            
            devices_.clear();
            for (const auto& item : jsonData.items()) {
                devices_[item.key()] = jsonFromDevice(item.value());
            }
            
            spdlog::info("Device repository loaded from: {} ({} devices)", dataPath_, devices_.size());
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to load device repository: {}", e.what());
            return false;
        }
    }

    bool backup(const std::string& backupPath) const override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        try {
            nlohmann::json jsonData;
            for (const auto& pair : devices_) {
                jsonData[pair.first] = deviceToJson(pair.second);
            }
            
            std::ofstream file(backupPath);
            file << jsonData.dump(2);
            file.close();
            
            spdlog::info("Device repository backed up to: {}", backupPath);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to backup device repository: {}", e.what());
            return false;
        }
    }

    bool restore(const std::string& backupPath) override {
        std::string originalPath = dataPath_;
        dataPath_ = backupPath;
        bool result = load();
        dataPath_ = originalPath;
        return result;
    }

    bool clear() override {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        devices_.clear();
        spdlog::info("Device repository cleared");
        return true;
    }

    // Transaction support
    bool beginTransaction() override {
        std::lock_guard<std::mutex> lock(transactionMutex_);
        
        if (inTransaction_) {
            spdlog::warn("Transaction already in progress");
            return false;
        }
        
        transactionBackup_ = devices_;
        inTransaction_ = true;
        spdlog::debug("Transaction started");
        return true;
    }

    bool commitTransaction() override {
        std::lock_guard<std::mutex> lock(transactionMutex_);
        
        if (!inTransaction_) {
            spdlog::warn("No transaction in progress");
            return false;
        }
        
        transactionBackup_.clear();
        inTransaction_ = false;
        spdlog::debug("Transaction committed");
        return true;
    }

    bool rollbackTransaction() override {
        std::lock_guard<std::mutex> lock(transactionMutex_);
        
        if (!inTransaction_) {
            spdlog::warn("No transaction in progress");
            return false;
        }
        
        {
            std::lock_guard<std::mutex> devicesLock(devicesMutex_);
            devices_ = transactionBackup_;
        }
        
        transactionBackup_.clear();
        inTransaction_ = false;
        spdlog::debug("Transaction rolled back");
        return true;
    }

    bool isInTransaction() const override {
        std::lock_guard<std::mutex> lock(transactionMutex_);
        return inTransaction_;
    }

private:
    mutable std::mutex devicesMutex_;
    mutable std::mutex transactionMutex_;
    
    std::unordered_map<std::string, services::DeviceInfo> devices_;
    std::unordered_map<std::string, services::DeviceInfo> transactionBackup_;
    
    std::string dataPath_;
    bool inTransaction_;

    double calculateSimilarity(const services::DeviceInfo& device1, 
                             const services::DeviceInfo& device2) const {
        double score = 0.0;
        double maxScore = 4.0;
        
        if (device1.deviceType == device2.deviceType) score += 1.0;
        if (device1.manufacturer == device2.manufacturer) score += 1.0;
        if (device1.model == device2.model) score += 1.0;
        
        // Compare capabilities
        size_t commonCapabilities = 0;
        for (const auto& cap : device1.capabilities) {
            if (std::find(device2.capabilities.begin(), device2.capabilities.end(), cap) 
                != device2.capabilities.end()) {
                commonCapabilities++;
            }
        }
        
        if (!device1.capabilities.empty() && !device2.capabilities.empty()) {
            score += (double)commonCapabilities / std::max(device1.capabilities.size(), device2.capabilities.size());
        }
        
        return score / maxScore;
    }

    nlohmann::json deviceToJson(const services::DeviceInfo& device) const {
        nlohmann::json json;
        json["deviceId"] = device.deviceId;
        json["deviceName"] = device.deviceName;
        json["deviceType"] = device.deviceType;
        json["manufacturer"] = device.manufacturer;
        json["model"] = device.model;
        json["capabilities"] = device.capabilities;
        json["properties"] = device.properties;
        return json;
    }

    services::DeviceInfo jsonFromDevice(const nlohmann::json& json) const {
        services::DeviceInfo device;
        device.deviceId = json.value("deviceId", "");
        device.deviceName = json.value("deviceName", "");
        device.deviceType = json.value("deviceType", "");
        device.manufacturer = json.value("manufacturer", "");
        device.model = json.value("model", "");
        device.capabilities = json.value("capabilities", std::vector<std::string>{});
        device.properties = json.value("properties", std::unordered_map<std::string, std::string>{});
        return device;
    }
};

// Factory function implementation
std::unique_ptr<IDeviceRepository> DeviceRepositoryFactory::createRepository(const std::string& dataPath) {
    return std::make_unique<DeviceRepositoryImpl>(dataPath);
}

} // namespace repositories
} // namespace server
} // namespace astrocomm

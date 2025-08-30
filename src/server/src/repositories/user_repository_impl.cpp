#include "astrocomm/server/repositories/user_repository.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace astrocomm {
namespace server {
namespace repositories {

/**
 * @brief Concrete implementation of the User Repository
 */
class UserRepositoryImpl : public IUserRepository {
public:
    explicit UserRepositoryImpl(const std::string& dataPath = "./data/users.json")
        : dataPath_(dataPath), inTransaction_(false) {
        spdlog::info("User repository initialized with data path: {}", dataPath_);
    }

    ~UserRepositoryImpl() {
        if (inTransaction_) {
            rollbackTransaction();
        }
    }

    // Basic CRUD operations
    bool create(const services::UserInfo& user) override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        if (users_.find(user.userId) != users_.end()) {
            spdlog::warn("User already exists: {}", user.userId);
            return false;
        }
        
        // Check for duplicate username or email
        for (const auto& pair : users_) {
            const auto& existingUser = pair.second;
            if (existingUser.username == user.username) {
                spdlog::warn("Username already exists: {}", user.username);
                return false;
            }
            if (existingUser.email == user.email) {
                spdlog::warn("Email already exists: {}", user.email);
                return false;
            }
        }
        
        users_[user.userId] = user;
        spdlog::info("User created: {} ({})", user.userId, user.username);
        return true;
    }

    std::optional<services::UserInfo> read(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(userId);
        if (it != users_.end()) {
            return it->second;
        }
        
        return std::nullopt;
    }

    bool update(const services::UserInfo& user) override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(user.userId);
        if (it == users_.end()) {
            spdlog::warn("User not found for update: {}", user.userId);
            return false;
        }
        
        // Check for duplicate username or email (excluding current user)
        for (const auto& pair : users_) {
            const auto& existingUser = pair.second;
            if (existingUser.userId != user.userId) {
                if (existingUser.username == user.username) {
                    spdlog::warn("Username already exists: {}", user.username);
                    return false;
                }
                if (existingUser.email == user.email) {
                    spdlog::warn("Email already exists: {}", user.email);
                    return false;
                }
            }
        }
        
        it->second = user;
        spdlog::info("User updated: {} ({})", user.userId, user.username);
        return true;
    }

    bool remove(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(userId);
        if (it == users_.end()) {
            spdlog::warn("User not found for removal: {}", userId);
            return false;
        }
        
        std::string username = it->second.username;
        users_.erase(it);
        spdlog::info("User removed: {} ({})", userId, username);
        return true;
    }

    bool exists(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        return users_.find(userId) != users_.end();
    }

    // Authentication operations
    std::optional<services::UserInfo> findByUsername(const std::string& username) const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        for (const auto& pair : users_) {
            if (pair.second.username == username) {
                return pair.second;
            }
        }
        
        return std::nullopt;
    }

    std::optional<services::UserInfo> findByEmail(const std::string& email) const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        for (const auto& pair : users_) {
            if (pair.second.email == email) {
                return pair.second;
            }
        }
        
        return std::nullopt;
    }

    bool validateCredentials(const std::string& username, const std::string& password) const override {
        auto user = findByUsername(username);
        if (!user) {
            return false;
        }
        
        // In a real implementation, you would hash the password and compare
        // For now, we'll do a simple comparison (NOT SECURE - for demo only)
        return user->passwordHash == password;
    }

    bool updatePassword(const std::string& userId, const std::string& newPasswordHash) override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(userId);
        if (it == users_.end()) {
            spdlog::warn("User not found for password update: {}", userId);
            return false;
        }
        
        it->second.passwordHash = newPasswordHash;
        it->second.lastPasswordChange = std::chrono::system_clock::now();
        spdlog::info("Password updated for user: {}", userId);
        return true;
    }

    bool updateLastLogin(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(userId);
        if (it == users_.end()) {
            return false;
        }
        
        it->second.lastLogin = std::chrono::system_clock::now();
        return true;
    }

    // Role and permission operations
    std::vector<services::UserInfo> getUsersByRole(const std::string& role) const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        std::vector<services::UserInfo> result;
        for (const auto& pair : users_) {
            const auto& user = pair.second;
            if (std::find(user.roles.begin(), user.roles.end(), role) != user.roles.end()) {
                result.push_back(user);
            }
        }
        
        return result;
    }

    bool addRole(const std::string& userId, const std::string& role) override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(userId);
        if (it == users_.end()) {
            return false;
        }
        
        auto& roles = it->second.roles;
        if (std::find(roles.begin(), roles.end(), role) == roles.end()) {
            roles.push_back(role);
            spdlog::info("Role '{}' added to user: {}", role, userId);
            return true;
        }
        
        return false; // Role already exists
    }

    bool removeRole(const std::string& userId, const std::string& role) override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(userId);
        if (it == users_.end()) {
            return false;
        }
        
        auto& roles = it->second.roles;
        auto roleIt = std::find(roles.begin(), roles.end(), role);
        if (roleIt != roles.end()) {
            roles.erase(roleIt);
            spdlog::info("Role '{}' removed from user: {}", role, userId);
            return true;
        }
        
        return false; // Role not found
    }

    bool hasRole(const std::string& userId, const std::string& role) const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(userId);
        if (it == users_.end()) {
            return false;
        }
        
        const auto& roles = it->second.roles;
        return std::find(roles.begin(), roles.end(), role) != roles.end();
    }

    bool addPermission(const std::string& userId, const std::string& permission) override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(userId);
        if (it == users_.end()) {
            return false;
        }
        
        auto& permissions = it->second.permissions;
        if (std::find(permissions.begin(), permissions.end(), permission) == permissions.end()) {
            permissions.push_back(permission);
            spdlog::info("Permission '{}' added to user: {}", permission, userId);
            return true;
        }
        
        return false; // Permission already exists
    }

    bool removePermission(const std::string& userId, const std::string& permission) override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(userId);
        if (it == users_.end()) {
            return false;
        }
        
        auto& permissions = it->second.permissions;
        auto permIt = std::find(permissions.begin(), permissions.end(), permission);
        if (permIt != permissions.end()) {
            permissions.erase(permIt);
            spdlog::info("Permission '{}' removed from user: {}", permission, userId);
            return true;
        }
        
        return false; // Permission not found
    }

    bool hasPermission(const std::string& userId, const std::string& permission) const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        auto it = users_.find(userId);
        if (it == users_.end()) {
            return false;
        }
        
        const auto& permissions = it->second.permissions;
        return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
    }

    // Bulk operations
    std::vector<services::UserInfo> getAll() const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        std::vector<services::UserInfo> result;
        for (const auto& pair : users_) {
            result.push_back(pair.second);
        }
        
        return result;
    }

    std::vector<services::UserInfo> getActiveUsers() const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        std::vector<services::UserInfo> result;
        for (const auto& pair : users_) {
            if (pair.second.isActive) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    size_t count() const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        return users_.size();
    }

    size_t countActiveUsers() const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        size_t count = 0;
        for (const auto& pair : users_) {
            if (pair.second.isActive) {
                count++;
            }
        }
        
        return count;
    }

    // Search operations
    std::vector<services::UserInfo> search(const std::string& searchTerm) const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        std::vector<services::UserInfo> result;
        std::string lowerSearchTerm = searchTerm;
        std::transform(lowerSearchTerm.begin(), lowerSearchTerm.end(), lowerSearchTerm.begin(), ::tolower);
        
        for (const auto& pair : users_) {
            const auto& user = pair.second;
            
            // Search in username, email, first name, last name
            std::string searchableText = user.username + " " + user.email + " " + 
                                       user.firstName + " " + user.lastName;
            std::transform(searchableText.begin(), searchableText.end(), searchableText.begin(), ::tolower);
            
            if (searchableText.find(lowerSearchTerm) != std::string::npos) {
                result.push_back(user);
            }
        }
        
        return result;
    }

    // Persistence management
    bool save() override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        try {
            nlohmann::json jsonData;
            for (const auto& pair : users_) {
                jsonData[pair.first] = userToJson(pair.second);
            }
            
            std::ofstream file(dataPath_);
            file << jsonData.dump(2);
            file.close();
            
            spdlog::info("User repository saved to: {}", dataPath_);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to save user repository: {}", e.what());
            return false;
        }
    }

    bool load() override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        try {
            std::ifstream file(dataPath_);
            if (!file.is_open()) {
                spdlog::warn("User repository file not found, starting with empty repository");
                return true;
            }
            
            nlohmann::json jsonData;
            file >> jsonData;
            file.close();
            
            users_.clear();
            for (const auto& item : jsonData.items()) {
                users_[item.key()] = jsonFromUser(item.value());
            }
            
            spdlog::info("User repository loaded from: {} ({} users)", dataPath_, users_.size());
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to load user repository: {}", e.what());
            return false;
        }
    }

    bool backup(const std::string& backupPath) const override {
        std::lock_guard<std::mutex> lock(usersMutex_);
        
        try {
            nlohmann::json jsonData;
            for (const auto& pair : users_) {
                jsonData[pair.first] = userToJson(pair.second);
            }
            
            std::ofstream file(backupPath);
            file << jsonData.dump(2);
            file.close();
            
            spdlog::info("User repository backed up to: {}", backupPath);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to backup user repository: {}", e.what());
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
        std::lock_guard<std::mutex> lock(usersMutex_);
        users_.clear();
        spdlog::info("User repository cleared");
        return true;
    }

    // Transaction support
    bool beginTransaction() override {
        std::lock_guard<std::mutex> lock(transactionMutex_);
        
        if (inTransaction_) {
            spdlog::warn("Transaction already in progress");
            return false;
        }
        
        transactionBackup_ = users_;
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
            std::lock_guard<std::mutex> usersLock(usersMutex_);
            users_ = transactionBackup_;
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
    mutable std::mutex usersMutex_;
    mutable std::mutex transactionMutex_;
    
    std::unordered_map<std::string, services::UserInfo> users_;
    std::unordered_map<std::string, services::UserInfo> transactionBackup_;
    
    std::string dataPath_;
    bool inTransaction_;

    nlohmann::json userToJson(const services::UserInfo& user) const {
        nlohmann::json json;
        json["userId"] = user.userId;
        json["username"] = user.username;
        json["email"] = user.email;
        json["firstName"] = user.firstName;
        json["lastName"] = user.lastName;
        json["passwordHash"] = user.passwordHash;
        json["roles"] = user.roles;
        json["permissions"] = user.permissions;
        json["isActive"] = user.isActive;
        json["preferences"] = user.preferences;
        return json;
    }

    services::UserInfo jsonFromUser(const nlohmann::json& json) const {
        services::UserInfo user;
        user.userId = json.value("userId", "");
        user.username = json.value("username", "");
        user.email = json.value("email", "");
        user.firstName = json.value("firstName", "");
        user.lastName = json.value("lastName", "");
        user.passwordHash = json.value("passwordHash", "");
        user.roles = json.value("roles", std::vector<std::string>{});
        user.permissions = json.value("permissions", std::vector<std::string>{});
        user.isActive = json.value("isActive", true);
        user.preferences = json.value("preferences", std::unordered_map<std::string, std::string>{});
        return user;
    }
};

// Factory function implementation
std::unique_ptr<IUserRepository> UserRepositoryFactory::createRepository(const std::string& dataPath) {
    return std::make_unique<UserRepositoryImpl>(dataPath);
}

} // namespace repositories
} // namespace server
} // namespace astrocomm

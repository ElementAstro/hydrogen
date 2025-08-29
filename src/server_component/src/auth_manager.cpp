#include "astrocomm/server/auth_manager.h"
#include <astrocomm/core/utils.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <shared_mutex>
#include <nlohmann/json.hpp>

namespace astrocomm {
namespace server {

AuthManager::AuthManager() : jwtSecret_(astrocomm::core::generateUuid()) {
    // Add a default admin user for initial setup
    addUser("admin", "admin", {"admin", "device_control", "user_management"});
}

AuthManager::AuthManager(const std::string& configPath) 
    : configPath_(configPath), jwtSecret_(astrocomm::core::generateUuid()) {
    loadConfiguration(configPath);
}

AuthManager::~AuthManager() {
    if (autoSave_) {
        saveConfiguration();
    }
}

std::string AuthManager::authenticate(const std::string& username, 
                                    const std::string& password,
                                    const std::string& ipAddress) {
    recordLoginAttempt(ipAddress, false); // Assume failure initially
    
    if (isRateLimited(ipAddress)) {
        return "";
    }
    
    std::shared_lock<std::shared_mutex> lock(usersMutex_);
    auto it = users_.find(username);
    if (it == users_.end() || !it->second.enabled) {
        return "";
    }
    
    if (!verifyPassword(password, it->second.passwordHash)) {
        return "";
    }
    
    lock.unlock();
    
    // Authentication successful
    recordLoginAttempt(ipAddress, true);
    
    std::string token = generateToken(username);
    
    // Store session
    Session session;
    session.token = token;
    session.username = username;
    session.expiryTime = std::chrono::system_clock::now() + 
                        std::chrono::minutes(sessionTimeoutMinutes_);
    session.ipAddress = ipAddress;
    
    {
        std::lock_guard<std::shared_mutex> sessionLock(sessionsMutex_);
        sessions_[token] = session;
    }
    
    return token;
}

bool AuthManager::validateToken(const std::string& token) {
    std::shared_lock<std::shared_mutex> lock(sessionsMutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return false;
    }
    
    return std::chrono::system_clock::now() < it->second.expiryTime;
}

std::string AuthManager::getUsernameFromToken(const std::string& token) {
    std::shared_lock<std::shared_mutex> lock(sessionsMutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end() || std::chrono::system_clock::now() >= it->second.expiryTime) {
        return "";
    }
    
    return it->second.username;
}

bool AuthManager::revokeToken(const std::string& token) {
    std::lock_guard<std::shared_mutex> lock(sessionsMutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return false;
    }
    
    sessions_.erase(it);
    return true;
}

bool AuthManager::addUser(const std::string& username, 
                         const std::string& password,
                         const std::vector<std::string>& permissions) {
    std::lock_guard<std::shared_mutex> lock(usersMutex_);
    
    if (users_.find(username) != users_.end()) {
        return false; // User already exists
    }
    
    UserInfo userInfo;
    userInfo.passwordHash = hashPassword(password);
    userInfo.permissions = permissions;
    userInfo.enabled = true;
    
    users_[username] = userInfo;
    
    if (autoSave_) {
        saveConfiguration();
    }
    
    return true;
}

bool AuthManager::removeUser(const std::string& username) {
    std::lock_guard<std::shared_mutex> lock(usersMutex_);
    
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }
    
    users_.erase(it);
    
    if (autoSave_) {
        saveConfiguration();
    }
    
    return true;
}

bool AuthManager::changePassword(const std::string& username,
                               const std::string& oldPassword,
                               const std::string& newPassword) {
    std::lock_guard<std::shared_mutex> lock(usersMutex_);
    
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }
    
    if (!verifyPassword(oldPassword, it->second.passwordHash)) {
        return false;
    }
    
    it->second.passwordHash = hashPassword(newPassword);
    
    if (autoSave_) {
        saveConfiguration();
    }
    
    return true;
}

bool AuthManager::hasPermission(const std::string& username, const std::string& permission) {
    std::shared_lock<std::shared_mutex> lock(usersMutex_);
    
    auto it = users_.find(username);
    if (it == users_.end() || !it->second.enabled) {
        return false;
    }
    
    const auto& permissions = it->second.permissions;
    return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
}

bool AuthManager::addPermission(const std::string& username, const std::string& permission) {
    std::lock_guard<std::shared_mutex> lock(usersMutex_);
    
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }
    
    auto& permissions = it->second.permissions;
    if (std::find(permissions.begin(), permissions.end(), permission) == permissions.end()) {
        permissions.push_back(permission);
        
        if (autoSave_) {
            saveConfiguration();
        }
    }
    
    return true;
}

bool AuthManager::removePermission(const std::string& username, const std::string& permission) {
    std::lock_guard<std::shared_mutex> lock(usersMutex_);
    
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }
    
    auto& permissions = it->second.permissions;
    permissions.erase(std::remove(permissions.begin(), permissions.end(), permission), 
                     permissions.end());
    
    if (autoSave_) {
        saveConfiguration();
    }
    
    return true;
}

std::vector<AuthManager::Session> AuthManager::getActiveSessions() const {
    std::shared_lock<std::shared_mutex> lock(sessionsMutex_);
    
    std::vector<Session> activeSessions;
    auto now = std::chrono::system_clock::now();
    
    for (const auto& pair : sessions_) {
        if (now < pair.second.expiryTime) {
            activeSessions.push_back(pair.second);
        }
    }
    
    return activeSessions;
}

std::vector<AuthManager::LoginAttempt> AuthManager::getLoginAttempts(const std::string& ipAddress, 
                                                                    int timeWindow) const {
    std::lock_guard<std::mutex> lock(attemptsMutex_);
    
    std::vector<LoginAttempt> attempts;
    auto cutoff = std::chrono::system_clock::now() - std::chrono::minutes(timeWindow);
    
    for (const auto& attempt : loginAttempts_) {
        if (attempt.ipAddress == ipAddress && attempt.timestamp > cutoff) {
            attempts.push_back(attempt);
        }
    }
    
    return attempts;
}

bool AuthManager::isRateLimited(const std::string& ipAddress) const {
    auto attempts = getLoginAttempts(ipAddress, rateLimitDurationMinutes_);
    
    int failedAttempts = 0;
    for (const auto& attempt : attempts) {
        if (!attempt.successful) {
            failedAttempts++;
        }
    }
    
    return failedAttempts >= maxFailedAttempts_;
}

void AuthManager::setMaxFailedAttempts(int maxAttempts) {
    maxFailedAttempts_ = maxAttempts;
}

void AuthManager::setRateLimitDuration(int minutes) {
    rateLimitDurationMinutes_ = minutes;
}

void AuthManager::setSessionTimeout(int minutes) {
    sessionTimeoutMinutes_ = minutes;
}

void AuthManager::setAutoSave(bool enabled) {
    autoSave_ = enabled;
}

bool AuthManager::loadConfiguration(const std::string& configPath) {
    std::string path = configPath.empty() ? configPath_ : configPath;
    if (path.empty()) {
        return false;
    }
    
    try {
        if (!std::filesystem::exists(path)) {
            return false;
        }
        
        std::ifstream file(path);
        nlohmann::json config;
        file >> config;
        
        // Load users
        if (config.contains("users")) {
            std::lock_guard<std::shared_mutex> lock(usersMutex_);
            users_.clear();
            
            for (const auto& userJson : config["users"]) {
                UserInfo userInfo;
                userInfo.passwordHash = userJson["passwordHash"];
                userInfo.permissions = userJson["permissions"];
                userInfo.enabled = userJson.value("enabled", true);
                
                users_[userJson["username"]] = userInfo;
            }
        }
        
        // Load settings
        if (config.contains("settings")) {
            const auto& settings = config["settings"];
            maxFailedAttempts_ = settings.value("maxFailedAttempts", 5);
            rateLimitDurationMinutes_ = settings.value("rateLimitDurationMinutes", 15);
            sessionTimeoutMinutes_ = settings.value("sessionTimeoutMinutes", 60);
            jwtSecret_ = settings.value("jwtSecret", astrocomm::core::generateUuid());
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool AuthManager::saveConfiguration(const std::string& configPath) const {
    std::string path = configPath.empty() ? configPath_ : configPath;
    if (path.empty()) {
        return false;
    }
    
    try {
        // Ensure directory exists
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        
        nlohmann::json config;
        
        // Save users
        {
            std::shared_lock<std::shared_mutex> lock(usersMutex_);
            config["users"] = nlohmann::json::array();
            
            for (const auto& pair : users_) {
                nlohmann::json userJson = {
                    {"username", pair.first},
                    {"passwordHash", pair.second.passwordHash},
                    {"permissions", pair.second.permissions},
                    {"enabled", pair.second.enabled}
                };
                config["users"].push_back(userJson);
            }
        }
        
        // Save settings
        config["settings"] = {
            {"maxFailedAttempts", maxFailedAttempts_},
            {"rateLimitDurationMinutes", rateLimitDurationMinutes_},
            {"sessionTimeoutMinutes", sessionTimeoutMinutes_},
            {"jwtSecret", jwtSecret_}
        };
        
        std::ofstream file(path);
        file << config.dump(4);
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void AuthManager::cleanup() {
    cleanupExpiredSessions();
    cleanupOldLoginAttempts();
}

// Private methods
std::string AuthManager::hashPassword(const std::string& password) const {
    // Simple hash for demonstration - in production, use bcrypt or similar
    std::hash<std::string> hasher;
    return std::to_string(hasher(password + jwtSecret_));
}

bool AuthManager::verifyPassword(const std::string& password, const std::string& hash) const {
    return hashPassword(password) == hash;
}

std::string AuthManager::generateToken(const std::string& username) const {
    // Simple token generation - in production, use proper JWT
    return astrocomm::core::generateUuid() + "_" + username;
}

void AuthManager::recordLoginAttempt(const std::string& ipAddress, bool successful) {
    std::lock_guard<std::mutex> lock(attemptsMutex_);
    
    LoginAttempt attempt;
    attempt.ipAddress = ipAddress;
    attempt.timestamp = std::chrono::system_clock::now();
    attempt.successful = successful;
    
    loginAttempts_.push_back(attempt);
    
    // Keep only recent attempts to prevent memory growth
    if (loginAttempts_.size() > 10000) {
        loginAttempts_.erase(loginAttempts_.begin(), loginAttempts_.begin() + 5000);
    }
}

void AuthManager::cleanupExpiredSessions() {
    std::lock_guard<std::shared_mutex> lock(sessionsMutex_);
    
    auto now = std::chrono::system_clock::now();
    auto it = sessions_.begin();
    
    while (it != sessions_.end()) {
        if (now >= it->second.expiryTime) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

void AuthManager::cleanupOldLoginAttempts() {
    std::lock_guard<std::mutex> lock(attemptsMutex_);
    
    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24);
    
    loginAttempts_.erase(
        std::remove_if(loginAttempts_.begin(), loginAttempts_.end(),
                      [cutoff](const LoginAttempt& attempt) {
                          return attempt.timestamp < cutoff;
                      }),
        loginAttempts_.end()
    );
}

} // namespace server
} // namespace astrocomm

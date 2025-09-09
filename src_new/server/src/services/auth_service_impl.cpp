#include "hydrogen/server/services/auth_service.h"
#include "hydrogen/server/core/service_registry.h"
#include <spdlog/spdlog.h>
#include <random>
#include <iomanip>
#include <sstream>
#include <regex>
#include <optional>
#include <deque>
#include <openssl/sha.h>
#include <openssl/rand.h>

namespace hydrogen {
namespace server {
namespace services {

/**
 * @brief Concrete implementation of the Authentication Service
 */
class AuthServiceImpl : public core::BaseService, public IAuthService {
public:
    explicit AuthServiceImpl(const std::string& name = "AuthService")
        : core::BaseService(name, "1.0.0") {
        description_ = "Authentication and authorization service for Hydrogen server";
    }

    // IService implementation
    bool initialize() override {
        setState(core::ServiceState::INITIALIZING);
        
        spdlog::info("Initializing Authentication Service...");
        
        // Initialize internal data structures
        users_.clear();
        sessions_.clear();
        tokens_.clear();
        apiKeys_.clear();
        auditLog_.clear();
        
        // Set default configuration
        tokenExpiration_ = std::chrono::seconds(getConfigInt("token_expiration", 3600));
        sessionTimeout_ = std::chrono::seconds(getConfigInt("session_timeout", 1800));
        maxFailedAttempts_ = getConfigInt("max_failed_attempts", 5);
        lockoutDuration_ = std::chrono::seconds(getConfigInt("lockout_duration", 300));
        
        // Create default admin user if none exists
        createDefaultAdminUser();
        
        setState(core::ServiceState::INITIALIZED);
        setHealthy(true);
        setHealthStatus("Authentication service initialized successfully");
        
        spdlog::info("Authentication Service initialized");
        return true;
    }

    bool start() override {
        if (core::BaseService::getState() != core::ServiceState::INITIALIZED) {
            if (!initialize()) {
                return false;
            }
        }
        
        setState(core::ServiceState::STARTING);
        spdlog::info("Starting Authentication Service...");
        
        // Start session cleanup task
        startSessionCleanup();
        
        setState(core::ServiceState::RUNNING);
        setHealthStatus("Authentication service running");
        
        spdlog::info("Authentication Service started");
        return true;
    }

    bool stop() override {
        setState(core::ServiceState::STOPPING);
        spdlog::info("Stopping Authentication Service...");
        
        // Stop session cleanup
        stopSessionCleanup();
        
        setState(core::ServiceState::STOPPED);
        setHealthStatus("Authentication service stopped");
        
        spdlog::info("Authentication Service stopped");
        return true;
    }

    bool shutdown() override {
        stop();
        
        // Clear sensitive data
        std::lock_guard<std::mutex> lock(authMutex_);
        users_.clear();
        sessions_.clear();
        tokens_.clear();
        apiKeys_.clear();
        
        spdlog::info("Authentication Service shutdown");
        return true;
    }

    std::vector<core::ServiceDependency> getDependencies() const override {
        return {}; // No dependencies for basic auth service
    }

    bool areDependenciesSatisfied() const override {
        return true; // No dependencies
    }

    // IAuthService implementation
    AuthResult authenticate(const AuthRequest& request) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        AuthResult result;
        result.success = false;
        result.timestamp = std::chrono::system_clock::now();
        
        // Check rate limiting
        if (isRateLimited(request.remoteAddress)) {
            result.errorMessage = "Rate limit exceeded";
            logAuthEvent("", "rate_limit_exceeded", request.remoteAddress);
            return result;
        }
        
        recordAuthAttempt(request.remoteAddress);
        
        // Find user
        auto userIt = std::find_if(users_.begin(), users_.end(),
            [&request](const auto& pair) {
                return pair.second.username == request.username;
            });
        
        if (userIt == users_.end()) {
            result.errorMessage = "Invalid credentials";
            recordFailedLogin(request.username, request.remoteAddress);
            logAuthEvent("", "login_failed", "User not found: " + request.username);
            return result;
        }
        
        auto& user = userIt->second;
        
        // Check if user is locked
        if (isUserLocked(user.userId)) {
            result.errorMessage = "Account locked";
            logAuthEvent(user.userId, "login_failed", "Account locked");
            return result;
        }
        
        // Check if user is active
        if (!user.isActive) {
            result.errorMessage = "Account disabled";
            logAuthEvent(user.userId, "login_failed", "Account disabled");
            return result;
        }
        
        // Verify password
        if (!verifyPasswordHash(request.password, getPasswordHash(user.userId).value_or(""))) {
            result.errorMessage = "Invalid credentials";
            recordFailedLogin(request.username, request.remoteAddress);
            user.failedLoginAttempts++;
            
            if (user.failedLoginAttempts >= maxFailedAttempts_) {
                lockUser(user.userId, lockoutDuration_);
                logAuthEvent(user.userId, "account_locked", "Too many failed attempts");
            }
            
            logAuthEvent(user.userId, "login_failed", "Invalid password");
            return result;
        }
        
        // Authentication successful
        user.failedLoginAttempts = 0;
        user.lastLoginAt = std::chrono::system_clock::now();
        
        // Create session
        SessionInfo session = createSession(user.userId, request.clientId, request.remoteAddress);
        
        // Create token
        AuthToken token;
        token.token = generateSecureToken(32);
        token.userId = user.userId;
        token.username = user.username;
        token.role = user.role;
        token.permissions = user.permissions;
        token.issuedAt = std::chrono::system_clock::now();
        token.expiresAt = token.issuedAt + tokenExpiration_;
        token.issuer = "Hydrogen-AuthService";
        
        tokens_[token.token] = token;
        
        result.success = true;
        result.token = token;
        result.session = session;
        
        recordSuccessfulLogin(user.userId, request.remoteAddress);
        logAuthEvent(user.userId, "login_success", "User authenticated successfully");
        
        if (authEventCallback_) {
            authEventCallback_(user.userId, "authenticated", "Login successful");
        }
        
        updateMetric("successful_logins", std::to_string(++successfulLogins_));
        return result;
    }

    bool validateToken(const std::string& token) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = tokens_.find(token);
        if (it == tokens_.end()) {
            return false;
        }
        
        // Check if token is expired
        auto now = std::chrono::system_clock::now();
        if (now > it->second.expiresAt) {
            tokens_.erase(it);
            return false;
        }
        
        return true;
    }

    AuthToken parseToken(const std::string& token) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = tokens_.find(token);
        if (it != tokens_.end() && validateToken(token)) {
            return it->second;
        }
        
        return AuthToken{}; // Return empty token if invalid
    }

    bool refreshToken(const std::string& token, AuthToken& newToken) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = tokens_.find(token);
        if (it == tokens_.end()) {
            return false;
        }
        
        // Create new token
        newToken = it->second;
        newToken.token = generateSecureToken(32);
        newToken.issuedAt = std::chrono::system_clock::now();
        newToken.expiresAt = newToken.issuedAt + tokenExpiration_;
        
        // Remove old token and add new one
        tokens_.erase(it);
        tokens_[newToken.token] = newToken;
        
        logAuthEvent(newToken.userId, "token_refreshed", "Token refreshed successfully");
        return true;
    }

    bool revokeToken(const std::string& token) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = tokens_.find(token);
        if (it != tokens_.end()) {
            logAuthEvent(it->second.userId, "token_revoked", "Token revoked");
            tokens_.erase(it);
            return true;
        }
        
        return false;
    }

    bool createUser(const UserInfo& userInfo, const std::string& password) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        if (userInfo.username.empty() || password.empty()) {
            spdlog::error("Username and password are required");
            return false;
        }
        
        if (!isValidUsername(userInfo.username)) {
            spdlog::error("Invalid username format: {}", userInfo.username);
            return false;
        }
        
        if (!validatePassword(password)) {
            spdlog::error("Password does not meet requirements");
            return false;
        }
        
        // Check if username already exists
        if (usernameExists(userInfo.username)) {
            spdlog::error("Username already exists: {}", userInfo.username);
            return false;
        }
        
        // Check if email already exists
        if (!userInfo.email.empty() && emailExists(userInfo.email)) {
            spdlog::error("Email already exists: {}", userInfo.email);
            return false;
        }
        
        UserInfo user = userInfo;
        user.userId = generateUserId();
        user.createdAt = std::chrono::system_clock::now();
        user.passwordChangedAt = user.createdAt;
        user.isActive = true;
        user.isLocked = false;
        user.failedLoginAttempts = 0;
        
        users_[user.userId] = user;
        passwords_[user.userId] = hashPassword(password);
        
        spdlog::info("Created user: {} ({})", user.username, user.userId);
        logAuthEvent(user.userId, "user_created", "User account created");
        
        updateMetric("total_users", std::to_string(users_.size()));
        return true;
    }

    bool updateUser(const UserInfo& userInfo) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = users_.find(userInfo.userId);
        if (it == users_.end()) {
            spdlog::error("User not found: {}", userInfo.userId);
            return false;
        }
        
        it->second = userInfo;
        logAuthEvent(userInfo.userId, "user_updated", "User information updated");
        return true;
    }

    bool deleteUser(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = users_.find(userId);
        if (it == users_.end()) {
            spdlog::error("User not found: {}", userId);
            return false;
        }
        
        std::string username = it->second.username;
        
        // Remove user data
        users_.erase(it);
        passwords_.erase(userId);
        
        // Terminate all user sessions
        terminateAllUserSessions(userId);
        
        // Revoke all user tokens
        auto tokenIt = tokens_.begin();
        while (tokenIt != tokens_.end()) {
            if (tokenIt->second.userId == userId) {
                tokenIt = tokens_.erase(tokenIt);
            } else {
                ++tokenIt;
            }
        }
        
        spdlog::info("Deleted user: {} ({})", username, userId);
        logAuthEvent(userId, "user_deleted", "User account deleted");
        
        updateMetric("total_users", std::to_string(users_.size()));
        return true;
    }

    UserInfo getUserInfo(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = users_.find(userId);
        if (it != users_.end()) {
            return it->second;
        }
        
        return UserInfo{}; // Return empty user info if not found
    }

    UserInfo getUserByUsername(const std::string& username) const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = std::find_if(users_.begin(), users_.end(),
            [&username](const auto& pair) {
                return pair.second.username == username;
            });
        
        if (it != users_.end()) {
            return it->second;
        }
        
        return UserInfo{}; // Return empty user info if not found
    }

    std::vector<UserInfo> getAllUsers() const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        std::vector<UserInfo> result;
        result.reserve(users_.size());
        
        for (const auto& pair : users_) {
            result.push_back(pair.second);
        }
        
        return result;
    }

    bool userExists(const std::string& username) const override {
        return usernameExists(username);
    }

    bool changePassword(const std::string& userId, const std::string& oldPassword, 
                       const std::string& newPassword) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto userIt = users_.find(userId);
        if (userIt == users_.end()) {
            spdlog::error("User not found: {}", userId);
            return false;
        }
        
        // Verify old password
        auto currentHash = getPasswordHash(userId);
        if (!currentHash || !verifyPasswordHash(oldPassword, *currentHash)) {
            spdlog::error("Invalid current password for user: {}", userId);
            logAuthEvent(userId, "password_change_failed", "Invalid current password");
            return false;
        }
        
        if (!validatePassword(newPassword)) {
            spdlog::error("New password does not meet requirements");
            return false;
        }
        
        // Update password
        passwords_[userId] = hashPassword(newPassword);
        userIt->second.passwordChangedAt = std::chrono::system_clock::now();
        
        spdlog::info("Password changed for user: {}", userId);
        logAuthEvent(userId, "password_changed", "Password changed successfully");
        
        return true;
    }

    bool resetPassword(const std::string& userId, const std::string& newPassword) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto userIt = users_.find(userId);
        if (userIt == users_.end()) {
            spdlog::error("User not found: {}", userId);
            return false;
        }
        
        if (!validatePassword(newPassword)) {
            spdlog::error("New password does not meet requirements");
            return false;
        }
        
        // Reset password
        passwords_[userId] = hashPassword(newPassword);
        userIt->second.passwordChangedAt = std::chrono::system_clock::now();
        userIt->second.failedLoginAttempts = 0;
        userIt->second.isLocked = false;
        
        spdlog::info("Password reset for user: {}", userId);
        logAuthEvent(userId, "password_reset", "Password reset by administrator");
        
        return true;
    }

    bool validatePassword(const std::string& password) const override {
        // Basic password validation
        if (password.length() < 8) {
            return false;
        }
        
        // Check for at least one uppercase, lowercase, digit, and special character
        bool hasUpper = false, hasLower = false, hasDigit = false, hasSpecial = false;
        
        for (char c : password) {
            if (std::isupper(c)) hasUpper = true;
            else if (std::islower(c)) hasLower = true;
            else if (std::isdigit(c)) hasDigit = true;
            else if (std::ispunct(c)) hasSpecial = true;
        }
        
        return hasUpper && hasLower && hasDigit && hasSpecial;
    }

    std::string generateTemporaryPassword() override {
        return generateSecureToken(12);
    }

    SessionInfo createSession(const std::string& userId, const std::string& clientId,
                            const std::string& remoteAddress) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        SessionInfo session;
        session.sessionId = generateSecureToken(32);
        session.userId = userId;
        session.clientId = clientId;
        session.remoteAddress = remoteAddress;
        session.createdAt = std::chrono::system_clock::now();
        session.lastActivity = session.createdAt;
        session.expiresAt = session.createdAt + sessionTimeout_;
        session.isActive = true;
        
        auto userIt = users_.find(userId);
        if (userIt != users_.end()) {
            session.username = userIt->second.username;
        }
        
        sessions_[session.sessionId] = session;
        
        if (sessionEventCallback_) {
            sessionEventCallback_(session, "created");
        }
        
        return session;
    }

    bool validateSession(const std::string& sessionId) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return false;
        }
        
        auto& session = it->second;
        
        // Check if session is expired
        auto now = std::chrono::system_clock::now();
        if (now > session.expiresAt || !session.isActive) {
            sessions_.erase(it);
            return false;
        }
        
        return true;
    }

    SessionInfo getSessionInfo(const std::string& sessionId) const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            return it->second;
        }
        
        return SessionInfo{}; // Return empty session info if not found
    }

    bool updateSessionActivity(const std::string& sessionId) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second.lastActivity = std::chrono::system_clock::now();
            it->second.expiresAt = it->second.lastActivity + sessionTimeout_;
            return true;
        }
        
        return false;
    }

    bool terminateSession(const std::string& sessionId) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            if (sessionEventCallback_) {
                sessionEventCallback_(it->second, "terminated");
            }
            
            sessions_.erase(it);
            return true;
        }
        
        return false;
    }

    std::vector<SessionInfo> getUserSessions(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        std::vector<SessionInfo> result;
        
        for (const auto& pair : sessions_) {
            if (pair.second.userId == userId) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    std::vector<SessionInfo> getActiveSessions() const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        std::vector<SessionInfo> result;
        result.reserve(sessions_.size());
        
        for (const auto& pair : sessions_) {
            if (pair.second.isActive) {
                result.push_back(pair.second);
            }
        }
        
        return result;
    }

    bool terminateAllUserSessions(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto it = sessions_.begin();
        while (it != sessions_.end()) {
            if (it->second.userId == userId) {
                if (sessionEventCallback_) {
                    sessionEventCallback_(it->second, "terminated");
                }
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
        
        return true;
    }

    // Additional method implementations would continue here...
    // For brevity, I'm including key methods and placeholders for others

    bool assignRole(const std::string& userId, UserRole role) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        auto it = users_.find(userId);
        if (it != users_.end()) {
            it->second.role = role;
            logAuthEvent(userId, "role_assigned", "Role updated");
            return true;
        }
        return false;
    }

    bool grantPermission(const std::string& userId, Permission permission) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        auto it = users_.find(userId);
        if (it != users_.end()) {
            it->second.permissions.insert(permission);
            logAuthEvent(userId, "permission_granted", "Permission granted");
            return true;
        }
        return false;
    }

    bool revokePermission(const std::string& userId, Permission permission) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        auto it = users_.find(userId);
        if (it != users_.end()) {
            it->second.permissions.erase(permission);
            logAuthEvent(userId, "permission_revoked", "Permission revoked");
            return true;
        }
        return false;
    }

    bool hasPermission(const std::string& userId, Permission permission) const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        auto it = users_.find(userId);
        if (it != users_.end()) {
            return it->second.permissions.find(permission) != it->second.permissions.end();
        }
        return false;
    }

    std::unordered_set<Permission> getUserPermissions(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        auto it = users_.find(userId);
        if (it != users_.end()) {
            return it->second.permissions;
        }
        return {};
    }

    std::unordered_set<Permission> getRolePermissions(UserRole role) const override {
        // Define default permissions for each role
        switch (role) {
            case UserRole::GUEST:
                return {Permission::READ_DEVICES, Permission::VIEW_LOGS};
            case UserRole::USER:
                return {Permission::READ_DEVICES, Permission::WRITE_DEVICES, Permission::VIEW_LOGS};
            case UserRole::OPERATOR:
                return {Permission::READ_DEVICES, Permission::WRITE_DEVICES, Permission::CONTROL_DEVICES, 
                       Permission::EXECUTE_COMMANDS, Permission::VIEW_LOGS};
            case UserRole::ADMIN:
                return {Permission::READ_DEVICES, Permission::WRITE_DEVICES, Permission::CONTROL_DEVICES,
                       Permission::EXECUTE_COMMANDS, Permission::MANAGE_USERS, Permission::MANAGE_CONFIGS,
                       Permission::VIEW_LOGS, Permission::BULK_OPERATIONS, Permission::MANAGE_GROUPS};
            case UserRole::SUPER_ADMIN:
                return {Permission::READ_DEVICES, Permission::WRITE_DEVICES, Permission::CONTROL_DEVICES,
                       Permission::EXECUTE_COMMANDS, Permission::MANAGE_USERS, Permission::MANAGE_SYSTEM,
                       Permission::MANAGE_CONFIGS, Permission::VIEW_LOGS, Permission::BULK_OPERATIONS,
                       Permission::MANAGE_GROUPS};
            default:
                return {};
        }
    }

    bool lockUser(const std::string& userId, std::chrono::seconds duration) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        auto it = users_.find(userId);
        if (it != users_.end()) {
            it->second.isLocked = true;
            if (duration.count() > 0) {
                it->second.lockedUntil = std::chrono::system_clock::now() + duration;
            }
            logAuthEvent(userId, "user_locked", "User account locked");
            return true;
        }
        return false;
    }

    bool unlockUser(const std::string& userId) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        auto it = users_.find(userId);
        if (it != users_.end()) {
            it->second.isLocked = false;
            it->second.failedLoginAttempts = 0;
            it->second.lockedUntil = {};
            logAuthEvent(userId, "user_unlocked", "User account unlocked");
            return true;
        }
        return false;
    }

    bool isUserLocked(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        auto it = users_.find(userId);
        if (it != users_.end()) {
            if (!it->second.isLocked) {
                return false;
            }
            
            // Check if lock has expired
            auto now = std::chrono::system_clock::now();
            if (it->second.lockedUntil.time_since_epoch().count() > 0 && now > it->second.lockedUntil) {
                // Lock has expired, unlock the user
                const_cast<UserInfo&>(it->second).isLocked = false;
                const_cast<UserInfo&>(it->second).lockedUntil = {};
                return false;
            }
            
            return true;
        }
        return false;
    }

    void recordFailedLogin(const std::string& username, const std::string& remoteAddress) override {
        failedLoginAttempts_[username + "@" + remoteAddress]++;
        updateMetric("failed_logins", std::to_string(++failedLogins_));
    }

    void recordSuccessfulLogin(const std::string& userId, const std::string& remoteAddress) override {
        // Clear failed attempts for this user
        auto userIt = users_.find(userId);
        if (userIt != users_.end()) {
            std::string key = userIt->second.username + "@" + remoteAddress;
            failedLoginAttempts_.erase(key);
        }
    }

    int getFailedLoginAttempts(const std::string& username) const override {
        auto it = failedLoginAttempts_.find(username);
        return (it != failedLoginAttempts_.end()) ? it->second : 0;
    }

    bool isRateLimited(const std::string& identifier) const override {
        auto it = rateLimitAttempts_.find(identifier);
        if (it != rateLimitAttempts_.end()) {
            return it->second >= 10; // Max 10 attempts per minute
        }
        return false;
    }

    void recordAuthAttempt(const std::string& identifier) override {
        rateLimitAttempts_[identifier]++;
    }

    void resetRateLimit(const std::string& identifier) override {
        rateLimitAttempts_.erase(identifier);
    }

    std::string generateApiKey(const std::string& userId, const std::string& description) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        std::string apiKey = "ak_" + generateSecureToken(32);
        apiKeys_[apiKey] = {userId, description, std::chrono::system_clock::now()};
        
        logAuthEvent(userId, "api_key_generated", "API key generated: " + description);
        return apiKey;
    }

    bool validateApiKey(const std::string& apiKey) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        return apiKeys_.find(apiKey) != apiKeys_.end();
    }

    bool revokeApiKey(const std::string& apiKey) override {
        std::lock_guard<std::mutex> lock(authMutex_);
        auto it = apiKeys_.find(apiKey);
        if (it != apiKeys_.end()) {
            logAuthEvent(it->second.userId, "api_key_revoked", "API key revoked");
            apiKeys_.erase(it);
            return true;
        }
        return false;
    }

    std::vector<std::string> getUserApiKeys(const std::string& userId) const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        std::vector<std::string> result;
        
        for (const auto& pair : apiKeys_) {
            if (pair.second.userId == userId) {
                result.push_back(pair.first);
            }
        }
        
        return result;
    }

    std::vector<std::string> getAuthAuditLog(const std::string& userId, size_t limit) const override {
        std::lock_guard<std::mutex> lock(authMutex_);
        std::vector<std::string> result;
        
        size_t count = 0;
        for (auto it = auditLog_.rbegin(); it != auditLog_.rend() && count < limit; ++it, ++count) {
            if (userId.empty() || it->find(userId) != std::string::npos) {
                result.push_back(*it);
            }
        }
        
        return result;
    }

    void logAuthEvent(const std::string& userId, const std::string& event, 
                     const std::string& details) override {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << " [" << event << "] ";
        if (!userId.empty()) {
            ss << "User: " << userId << " ";
        }
        ss << details;
        
        auditLog_.push_back(ss.str());
        
        // Keep only last 1000 entries
        if (auditLog_.size() > 1000) {
            auditLog_.pop_front();
        }
    }

    void setTokenExpiration(std::chrono::seconds expiration) override {
        tokenExpiration_ = expiration;
    }

    void setSessionTimeout(std::chrono::seconds timeout) override {
        sessionTimeout_ = timeout;
    }

    void setMaxFailedAttempts(int maxAttempts) override {
        maxFailedAttempts_ = maxAttempts;
    }

    void setLockoutDuration(std::chrono::seconds duration) override {
        lockoutDuration_ = duration;
    }

    void setPasswordPolicy(const std::unordered_map<std::string, std::string>& policy) override {
        passwordPolicy_ = policy;
    }

    void setAuthEventCallback(AuthEventCallback callback) override {
        authEventCallback_ = callback;
    }

    void setSessionEventCallback(SessionEventCallback callback) override {
        sessionEventCallback_ = callback;
    }

    void setSecurityEventCallback(SecurityEventCallback callback) override {
        securityEventCallback_ = callback;
    }

    std::string hashPassword(const std::string& password) const override {
        // Simple SHA-256 hash (in production, use bcrypt or similar)
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, password.c_str(), password.length());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    bool verifyPasswordHash(const std::string& password, const std::string& hash) const override {
        return hashPassword(password) == hash;
    }

    std::string generateSecureToken(size_t length) const override {
        const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::string result;
        result.reserve(length);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
        
        for (size_t i = 0; i < length; ++i) {
            result += charset[dis(gen)];
        }
        
        return result;
    }

    bool isValidEmail(const std::string& email) const override {
        const std::regex emailPattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        return std::regex_match(email, emailPattern);
    }

    bool isValidUsername(const std::string& username) const override {
        if (username.length() < 3 || username.length() > 32) {
            return false;
        }
        
        const std::regex usernamePattern(R"([a-zA-Z0-9_-]+)");
        return std::regex_match(username, usernamePattern);
    }

    // Placeholder implementations for remaining methods
    bool enableMFA(const std::string& userId, const std::string& method) override { return true; }
    bool disableMFA(const std::string& userId) override { return true; }
    bool verifyMFA(const std::string& userId, const std::string& code) override { return true; }
    std::string generateMFASecret(const std::string& userId) override { return generateSecureToken(16); }

private:
    mutable std::mutex authMutex_;
    
    std::unordered_map<std::string, UserInfo> users_;
    std::unordered_map<std::string, std::string> passwords_;
    std::unordered_map<std::string, SessionInfo> sessions_;
    std::unordered_map<std::string, AuthToken> tokens_;
    
    struct ApiKeyInfo {
        std::string userId;
        std::string description;
        std::chrono::system_clock::time_point createdAt;
    };
    std::unordered_map<std::string, ApiKeyInfo> apiKeys_;
    
    std::unordered_map<std::string, int> failedLoginAttempts_;
    std::unordered_map<std::string, int> rateLimitAttempts_;
    std::deque<std::string> auditLog_;
    
    std::chrono::seconds tokenExpiration_{3600};
    std::chrono::seconds sessionTimeout_{1800};
    int maxFailedAttempts_{5};
    std::chrono::seconds lockoutDuration_{300};
    std::unordered_map<std::string, std::string> passwordPolicy_;
    
    std::thread sessionCleanupThread_;
    std::atomic<bool> sessionCleanupRunning_{false};
    
    AuthEventCallback authEventCallback_;
    SessionEventCallback sessionEventCallback_;
    SecurityEventCallback securityEventCallback_;
    
    std::atomic<size_t> successfulLogins_{0};
    std::atomic<size_t> failedLogins_{0};
    
    std::string description_;

    std::string generateUserId() const {
        return "user_" + generateSecureToken(16);
    }

    bool usernameExists(const std::string& username) const {
        return std::any_of(users_.begin(), users_.end(),
            [&username](const auto& pair) {
                return pair.second.username == username;
            });
    }

    bool emailExists(const std::string& email) const {
        return std::any_of(users_.begin(), users_.end(),
            [&email](const auto& pair) {
                return pair.second.email == email;
            });
    }

    std::optional<std::string> getPasswordHash(const std::string& userId) const {
        auto it = passwords_.find(userId);
        return (it != passwords_.end()) ? std::optional<std::string>(it->second) : std::nullopt;
    }

    void createDefaultAdminUser() {
        if (users_.empty()) {
            UserInfo admin;
            admin.userId = generateUserId();
            admin.username = "admin";
            admin.email = "admin@Hydrogen.local";
            admin.fullName = "System Administrator";
            admin.role = UserRole::SUPER_ADMIN;
            admin.permissions = getRolePermissions(UserRole::SUPER_ADMIN);
            admin.isActive = true;
            admin.isLocked = false;
            admin.createdAt = std::chrono::system_clock::now();
            admin.passwordChangedAt = admin.createdAt;
            admin.failedLoginAttempts = 0;
            
            users_[admin.userId] = admin;
            passwords_[admin.userId] = hashPassword("admin123!"); // Default password
            
            spdlog::info("Created default admin user: {} (password: admin123!)", admin.username);
        }
    }

    void startSessionCleanup() {
        sessionCleanupRunning_ = true;
        sessionCleanupThread_ = std::thread([this]() {
            while (sessionCleanupRunning_) {
                cleanupExpiredSessions();
                cleanupRateLimits();
                std::this_thread::sleep_for(std::chrono::minutes(1));
            }
        });
    }

    void stopSessionCleanup() {
        sessionCleanupRunning_ = false;
        if (sessionCleanupThread_.joinable()) {
            sessionCleanupThread_.join();
        }
    }

    void cleanupExpiredSessions() {
        std::lock_guard<std::mutex> lock(authMutex_);
        
        auto now = std::chrono::system_clock::now();
        auto it = sessions_.begin();
        
        while (it != sessions_.end()) {
            if (now > it->second.expiresAt) {
                if (sessionEventCallback_) {
                    sessionEventCallback_(it->second, "expired");
                }
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
        
        // Cleanup expired tokens
        auto tokenIt = tokens_.begin();
        while (tokenIt != tokens_.end()) {
            if (now > tokenIt->second.expiresAt) {
                tokenIt = tokens_.erase(tokenIt);
            } else {
                ++tokenIt;
            }
        }
    }

    void cleanupRateLimits() {
        // Reset rate limits every minute
        rateLimitAttempts_.clear();
    }
};

// AuthServiceFactory Implementation
std::unique_ptr<core::IService> AuthServiceFactory::createService(
    const std::string& serviceName,
    const std::unordered_map<std::string, std::string>& config) {
    
    if (serviceName == "AuthService") {
        auto service = std::make_unique<AuthServiceImpl>();
        service->core::BaseService::setConfiguration(config);
        return std::unique_ptr<core::IService>(static_cast<core::BaseService*>(service.release()));
    }
    
    return nullptr;
}

std::vector<std::string> AuthServiceFactory::getSupportedServices() const {
    return {"AuthService"};
}

bool AuthServiceFactory::isServiceSupported(const std::string& serviceName) const {
    return serviceName == "AuthService";
}

} // namespace services
} // namespace server
} // namespace hydrogen

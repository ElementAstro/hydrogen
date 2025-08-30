#pragma once

#include "../core/service_registry.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <memory>

namespace astrocomm {
namespace server {
namespace services {

/**
 * @brief Authentication method enumeration
 */
enum class AuthMethod {
    BASIC,          // Username/password
    JWT,            // JSON Web Token
    API_KEY,        // API key authentication
    OAUTH2,         // OAuth 2.0
    CERTIFICATE,    // Client certificate
    LDAP,           // LDAP authentication
    CUSTOM          // Custom authentication
};

/**
 * @brief User role enumeration
 */
enum class UserRole {
    GUEST,          // Read-only access
    USER,           // Standard user access
    OPERATOR,       // Device operation access
    ADMIN,          // Administrative access
    SUPER_ADMIN     // Full system access
};

/**
 * @brief Permission enumeration
 */
enum class Permission {
    READ_DEVICES,
    WRITE_DEVICES,
    CONTROL_DEVICES,
    MANAGE_USERS,
    MANAGE_SYSTEM,
    VIEW_LOGS,
    MANAGE_CONFIGS,
    EXECUTE_COMMANDS,
    BULK_OPERATIONS,
    MANAGE_GROUPS
};

/**
 * @brief User information structure
 */
struct UserInfo {
    std::string userId;
    std::string username;
    std::string email;
    std::string fullName;
    UserRole role;
    std::unordered_set<Permission> permissions;
    bool isActive;
    bool isLocked;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastLoginAt;
    std::chrono::system_clock::time_point passwordChangedAt;
    std::unordered_map<std::string, std::string> metadata;
    int failedLoginAttempts;
    std::chrono::system_clock::time_point lockedUntil;
};

/**
 * @brief Authentication token structure
 */
struct AuthToken {
    std::string token;
    std::string userId;
    std::string username;
    UserRole role;
    std::unordered_set<Permission> permissions;
    std::chrono::system_clock::time_point issuedAt;
    std::chrono::system_clock::time_point expiresAt;
    std::string issuer;
    std::unordered_map<std::string, std::string> claims;
};

/**
 * @brief Session information structure
 */
struct SessionInfo {
    std::string sessionId;
    std::string userId;
    std::string username;
    std::string clientId;
    std::string remoteAddress;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastActivity;
    std::chrono::system_clock::time_point expiresAt;
    bool isActive;
    std::unordered_map<std::string, std::string> sessionData;
};

/**
 * @brief Authentication request structure
 */
struct AuthRequest {
    std::string username;
    std::string password;
    std::string clientId;
    std::string remoteAddress;
    AuthMethod method;
    std::unordered_map<std::string, std::string> additionalData;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Authentication result structure
 */
struct AuthResult {
    bool success;
    std::string errorMessage;
    AuthToken token;
    SessionInfo session;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Authentication service interface
 * 
 * Provides comprehensive authentication and authorization functionality
 * including user management, session handling, token validation,
 * and permission checking.
 */
class IAuthService : public virtual core::IService {
public:
    virtual ~IAuthService() = default;

    // Authentication
    virtual AuthResult authenticate(const AuthRequest& request) = 0;
    virtual bool validateToken(const std::string& token) = 0;
    virtual AuthToken parseToken(const std::string& token) = 0;
    virtual bool refreshToken(const std::string& token, AuthToken& newToken) = 0;
    virtual bool revokeToken(const std::string& token) = 0;

    // User management
    virtual bool createUser(const UserInfo& userInfo, const std::string& password) = 0;
    virtual bool updateUser(const UserInfo& userInfo) = 0;
    virtual bool deleteUser(const std::string& userId) = 0;
    virtual UserInfo getUserInfo(const std::string& userId) const = 0;
    virtual UserInfo getUserByUsername(const std::string& username) const = 0;
    virtual std::vector<UserInfo> getAllUsers() const = 0;
    virtual bool userExists(const std::string& username) const = 0;

    // Password management
    virtual bool changePassword(const std::string& userId, const std::string& oldPassword, 
                              const std::string& newPassword) = 0;
    virtual bool resetPassword(const std::string& userId, const std::string& newPassword) = 0;
    virtual bool validatePassword(const std::string& password) const = 0;
    virtual std::string generateTemporaryPassword() = 0;

    // Role and permission management
    virtual bool assignRole(const std::string& userId, UserRole role) = 0;
    virtual bool grantPermission(const std::string& userId, Permission permission) = 0;
    virtual bool revokePermission(const std::string& userId, Permission permission) = 0;
    virtual bool hasPermission(const std::string& userId, Permission permission) const = 0;
    virtual std::unordered_set<Permission> getUserPermissions(const std::string& userId) const = 0;
    virtual std::unordered_set<Permission> getRolePermissions(UserRole role) const = 0;

    // Session management
    virtual SessionInfo createSession(const std::string& userId, const std::string& clientId,
                                    const std::string& remoteAddress) = 0;
    virtual bool validateSession(const std::string& sessionId) = 0;
    virtual SessionInfo getSessionInfo(const std::string& sessionId) const = 0;
    virtual bool updateSessionActivity(const std::string& sessionId) = 0;
    virtual bool terminateSession(const std::string& sessionId) = 0;
    virtual std::vector<SessionInfo> getUserSessions(const std::string& userId) const = 0;
    virtual std::vector<SessionInfo> getActiveSessions() const = 0;
    virtual bool terminateAllUserSessions(const std::string& userId) = 0;

    // Account security
    virtual bool lockUser(const std::string& userId, std::chrono::seconds duration = std::chrono::seconds::zero()) = 0;
    virtual bool unlockUser(const std::string& userId) = 0;
    virtual bool isUserLocked(const std::string& userId) const = 0;
    virtual void recordFailedLogin(const std::string& username, const std::string& remoteAddress) = 0;
    virtual void recordSuccessfulLogin(const std::string& userId, const std::string& remoteAddress) = 0;
    virtual int getFailedLoginAttempts(const std::string& username) const = 0;

    // Rate limiting
    virtual bool isRateLimited(const std::string& identifier) const = 0;
    virtual void recordAuthAttempt(const std::string& identifier) = 0;
    virtual void resetRateLimit(const std::string& identifier) = 0;

    // API key management
    virtual std::string generateApiKey(const std::string& userId, const std::string& description = "") = 0;
    virtual bool validateApiKey(const std::string& apiKey) = 0;
    virtual bool revokeApiKey(const std::string& apiKey) = 0;
    virtual std::vector<std::string> getUserApiKeys(const std::string& userId) const = 0;

    // Multi-factor authentication
    virtual bool enableMFA(const std::string& userId, const std::string& method) = 0;
    virtual bool disableMFA(const std::string& userId) = 0;
    virtual bool verifyMFA(const std::string& userId, const std::string& code) = 0;
    virtual std::string generateMFASecret(const std::string& userId) = 0;

    // Audit and logging
    virtual std::vector<std::string> getAuthAuditLog(const std::string& userId = "", 
                                                    size_t limit = 100) const = 0;
    virtual void logAuthEvent(const std::string& userId, const std::string& event, 
                            const std::string& details = "") = 0;

    // Configuration
    virtual void setTokenExpiration(std::chrono::seconds expiration) = 0;
    virtual void setSessionTimeout(std::chrono::seconds timeout) = 0;
    virtual void setMaxFailedAttempts(int maxAttempts) = 0;
    virtual void setLockoutDuration(std::chrono::seconds duration) = 0;
    virtual void setPasswordPolicy(const std::unordered_map<std::string, std::string>& policy) = 0;

    // Event callbacks
    using AuthEventCallback = std::function<void(const std::string& userId, const std::string& event, const std::string& details)>;
    using SessionEventCallback = std::function<void(const SessionInfo& session, const std::string& event)>;
    using SecurityEventCallback = std::function<void(const std::string& userId, const std::string& event, const std::string& remoteAddress)>;

    virtual void setAuthEventCallback(AuthEventCallback callback) = 0;
    virtual void setSessionEventCallback(SessionEventCallback callback) = 0;
    virtual void setSecurityEventCallback(SecurityEventCallback callback) = 0;

    // Utility methods
    virtual std::string hashPassword(const std::string& password) const = 0;
    virtual bool verifyPasswordHash(const std::string& password, const std::string& hash) const = 0;
    virtual std::string generateSecureToken(size_t length = 32) const = 0;
    virtual bool isValidEmail(const std::string& email) const = 0;
    virtual bool isValidUsername(const std::string& username) const = 0;
};

/**
 * @brief Authentication service factory
 */
class AuthServiceFactory : public core::IServiceFactory {
public:
    std::unique_ptr<core::IService> createService(
        const std::string& serviceName,
        const std::unordered_map<std::string, std::string>& config = {}
    ) override;
    
    std::vector<std::string> getSupportedServices() const override;
    bool isServiceSupported(const std::string& serviceName) const override;
};

} // namespace services
} // namespace server
} // namespace astrocomm

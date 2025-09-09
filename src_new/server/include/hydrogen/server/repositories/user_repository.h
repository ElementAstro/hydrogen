#pragma once

#include "../services/auth_service.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>

namespace hydrogen {
namespace server {
namespace repositories {

/**
 * @brief User query criteria
 */
struct UserQuery {
    std::optional<services::UserRole> role;
    std::optional<bool> isActive;
    std::optional<bool> isLocked;
    std::optional<std::chrono::system_clock::time_point> createdAfter;
    std::optional<std::chrono::system_clock::time_point> createdBefore;
    std::optional<std::chrono::system_clock::time_point> lastLoginAfter;
    std::optional<std::chrono::system_clock::time_point> lastLoginBefore;
    std::string emailDomain;
    size_t limit = 0;
    size_t offset = 0;
    std::string sortBy = "username";
    bool sortAscending = true;
};

/**
 * @brief User repository interface
 * 
 * Provides data access layer for user information, authentication data,
 * and user management operations.
 */
class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    // Basic CRUD operations
    virtual bool create(const services::UserInfo& user, const std::string& passwordHash) = 0;
    virtual std::optional<services::UserInfo> read(const std::string& userId) const = 0;
    virtual bool update(const services::UserInfo& user) = 0;
    virtual bool remove(const std::string& userId) = 0;
    virtual bool exists(const std::string& userId) const = 0;

    // User lookup operations
    virtual std::optional<services::UserInfo> findByUsername(const std::string& username) const = 0;
    virtual std::optional<services::UserInfo> findByEmail(const std::string& email) const = 0;
    virtual bool usernameExists(const std::string& username) const = 0;
    virtual bool emailExists(const std::string& email) const = 0;

    // Password operations
    virtual bool updatePassword(const std::string& userId, const std::string& passwordHash) = 0;
    virtual std::optional<std::string> getPasswordHash(const std::string& userId) const = 0;
    virtual bool updatePasswordChangedAt(const std::string& userId, std::chrono::system_clock::time_point timestamp) = 0;

    // Query operations
    virtual std::vector<services::UserInfo> findAll() const = 0;
    virtual std::vector<services::UserInfo> findByQuery(const UserQuery& query) const = 0;
    virtual std::vector<services::UserInfo> findByRole(services::UserRole role) const = 0;
    virtual std::vector<services::UserInfo> findActiveUsers() const = 0;
    virtual std::vector<services::UserInfo> findLockedUsers() const = 0;

    // Status operations
    virtual bool updateActiveStatus(const std::string& userId, bool isActive) = 0;
    virtual bool updateLockStatus(const std::string& userId, bool isLocked, 
                                std::chrono::system_clock::time_point lockedUntil = {}) = 0;
    virtual bool updateLastLogin(const std::string& userId, std::chrono::system_clock::time_point timestamp) = 0;
    virtual bool updateFailedLoginAttempts(const std::string& userId, int attempts) = 0;

    // Role and permission operations
    virtual bool updateRole(const std::string& userId, services::UserRole role) = 0;
    virtual bool grantPermission(const std::string& userId, services::Permission permission) = 0;
    virtual bool revokePermission(const std::string& userId, services::Permission permission) = 0;
    virtual std::unordered_set<services::Permission> getUserPermissions(const std::string& userId) const = 0;
    virtual bool hasPermission(const std::string& userId, services::Permission permission) const = 0;

    // Metadata operations
    virtual bool updateMetadata(const std::string& userId, const std::unordered_map<std::string, std::string>& metadata) = 0;
    virtual bool setMetadataValue(const std::string& userId, const std::string& key, const std::string& value) = 0;
    virtual std::optional<std::string> getMetadataValue(const std::string& userId, const std::string& key) const = 0;
    virtual std::unordered_map<std::string, std::string> getMetadata(const std::string& userId) const = 0;

    // Statistics
    virtual size_t count() const = 0;
    virtual size_t countByRole(services::UserRole role) const = 0;
    virtual size_t countActiveUsers() const = 0;
    virtual size_t countLockedUsers() const = 0;
    virtual std::unordered_map<services::UserRole, size_t> getRoleStatistics() const = 0;

    // Search operations
    virtual std::vector<services::UserInfo> search(const std::string& searchTerm) const = 0;

    // Persistence management
    virtual bool save() = 0;
    virtual bool load() = 0;
    virtual bool backup(const std::string& backupPath) const = 0;
    virtual bool restore(const std::string& backupPath) = 0;

    // Event callbacks
    using UserChangeCallback = std::function<void(const std::string& userId, const std::string& operation, const services::UserInfo& user)>;
    virtual void setChangeCallback(UserChangeCallback callback) = 0;
};

/**
 * @brief Session repository interface
 * 
 * Manages persistence of user sessions and authentication tokens.
 */
class ISessionRepository {
public:
    virtual ~ISessionRepository() = default;

    // Session operations
    virtual bool createSession(const services::SessionInfo& session) = 0;
    virtual std::optional<services::SessionInfo> getSession(const std::string& sessionId) const = 0;
    virtual bool updateSession(const services::SessionInfo& session) = 0;
    virtual bool removeSession(const std::string& sessionId) = 0;
    virtual bool sessionExists(const std::string& sessionId) const = 0;

    // Session queries
    virtual std::vector<services::SessionInfo> getUserSessions(const std::string& userId) const = 0;
    virtual std::vector<services::SessionInfo> getActiveSessions() const = 0;
    virtual std::vector<services::SessionInfo> getExpiredSessions() const = 0;

    // Session management
    virtual bool updateLastActivity(const std::string& sessionId, std::chrono::system_clock::time_point timestamp) = 0;
    virtual bool extendSession(const std::string& sessionId, std::chrono::system_clock::time_point newExpiry) = 0;
    virtual bool terminateSession(const std::string& sessionId) = 0;
    virtual bool terminateUserSessions(const std::string& userId) = 0;
    virtual bool terminateExpiredSessions() = 0;

    // Session data operations
    virtual bool setSessionData(const std::string& sessionId, const std::string& key, const std::string& value) = 0;
    virtual std::optional<std::string> getSessionData(const std::string& sessionId, const std::string& key) const = 0;
    virtual std::unordered_map<std::string, std::string> getAllSessionData(const std::string& sessionId) const = 0;
    virtual bool removeSessionData(const std::string& sessionId, const std::string& key) = 0;

    // Statistics
    virtual size_t getActiveSessionCount() const = 0;
    virtual size_t getUserSessionCount(const std::string& userId) const = 0;
    virtual std::chrono::milliseconds getAverageSessionDuration() const = 0;

    // Cleanup operations
    virtual bool cleanupExpiredSessions() = 0;
    virtual bool cleanupOldSessions(std::chrono::hours maxAge) = 0;
};

/**
 * @brief Authentication token repository interface
 * 
 * Manages persistence of authentication tokens (JWT, API keys, etc.).
 */
class ITokenRepository {
public:
    virtual ~ITokenRepository() = default;

    // Token operations
    virtual bool storeToken(const services::AuthToken& token) = 0;
    virtual std::optional<services::AuthToken> getToken(const std::string& token) const = 0;
    virtual bool updateToken(const services::AuthToken& token) = 0;
    virtual bool revokeToken(const std::string& token) = 0;
    virtual bool isTokenRevoked(const std::string& token) const = 0;

    // Token queries
    virtual std::vector<services::AuthToken> getUserTokens(const std::string& userId) const = 0;
    virtual std::vector<services::AuthToken> getExpiredTokens() const = 0;
    virtual std::vector<services::AuthToken> getActiveTokens() const = 0;

    // Token validation
    virtual bool validateToken(const std::string& token) const = 0;
    virtual bool isTokenExpired(const std::string& token) const = 0;

    // API key operations
    virtual bool storeApiKey(const std::string& apiKey, const std::string& userId, const std::string& description) = 0;
    virtual std::optional<std::string> getApiKeyOwner(const std::string& apiKey) const = 0;
    virtual bool revokeApiKey(const std::string& apiKey) = 0;
    virtual std::vector<std::string> getUserApiKeys(const std::string& userId) const = 0;

    // Cleanup operations
    virtual bool cleanupExpiredTokens() = 0;
    virtual bool cleanupRevokedTokens(std::chrono::hours maxAge) = 0;

    // Statistics
    virtual size_t getActiveTokenCount() const = 0;
    virtual size_t getUserTokenCount(const std::string& userId) const = 0;
    virtual size_t getRevokedTokenCount() const = 0;
};

/**
 * @brief Authentication audit repository interface
 * 
 * Manages persistence of authentication events and audit logs.
 */
class IAuditRepository {
public:
    virtual ~IAuditRepository() = default;

    // Audit log operations
    virtual bool logAuthEvent(const std::string& userId, const std::string& event, 
                            const std::string& details, const std::string& remoteAddress) = 0;
    virtual bool logSecurityEvent(const std::string& event, const std::string& details, 
                                const std::string& remoteAddress) = 0;
    virtual bool logLoginAttempt(const std::string& username, bool success, 
                               const std::string& remoteAddress, const std::string& userAgent) = 0;

    // Query operations
    virtual std::vector<std::string> getAuthAuditLog(const std::string& userId = "", size_t limit = 100) const = 0;
    virtual std::vector<std::string> getSecurityAuditLog(size_t limit = 100) const = 0;
    virtual std::vector<std::string> getLoginAttempts(const std::string& username = "", size_t limit = 100) const = 0;

    // Failed login tracking
    virtual bool recordFailedLogin(const std::string& username, const std::string& remoteAddress) = 0;
    virtual int getFailedLoginCount(const std::string& username, std::chrono::minutes timeWindow) const = 0;
    virtual bool clearFailedLogins(const std::string& username) = 0;

    // Rate limiting tracking
    virtual bool recordAuthAttempt(const std::string& identifier) = 0;
    virtual int getAuthAttemptCount(const std::string& identifier, std::chrono::minutes timeWindow) const = 0;
    virtual bool clearAuthAttempts(const std::string& identifier) = 0;

    // Cleanup operations
    virtual bool cleanupOldAuditLogs(std::chrono::hours maxAge) = 0;
    virtual bool cleanupOldLoginAttempts(std::chrono::hours maxAge) = 0;

    // Statistics
    virtual size_t getAuditLogCount() const = 0;
    virtual size_t getFailedLoginCount(std::chrono::hours timeWindow) const = 0;
    virtual size_t getSuccessfulLoginCount(std::chrono::hours timeWindow) const = 0;
};

} // namespace repositories
} // namespace server
} // namespace hydrogen

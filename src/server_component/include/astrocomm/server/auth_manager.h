#pragma once
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace astrocomm {
namespace server {

/**
 * @class AuthManager
 * @brief Manages authentication and user credentials for the astrocomm server.
 *
 * The AuthManager provides functionality for user management, authentication,
 * session tracking, and persistent credential storage. It supports both JWT and
 * Basic authentication methods.
 */
class AuthManager {
public:
  /**
   * @brief Structure representing a user session
   */
  struct Session {
    std::string token;                 ///< Session token or JWT
    std::string username;              ///< Associated username
    std::chrono::system_clock::time_point expiryTime; ///< When the session expires
    std::string ipAddress;             ///< IP address associated with the session
  };

  /**
   * @brief Structure to track login attempts
   */
  struct LoginAttempt {
    std::string ipAddress;             ///< IP address of the attempt
    std::chrono::system_clock::time_point timestamp; ///< When the attempt occurred
    bool successful;                   ///< Whether the login was successful
  };

  /**
   * @brief Default constructor
   * 
   * Initializes the authentication manager and generates a random JWT secret.
   */
  AuthManager();
  
  /**
   * @brief Constructor with configuration file path
   * 
   * Initializes the authentication manager and loads configuration from the specified file.
   * 
   * @param configPath Path to the authentication configuration file
   */
  explicit AuthManager(const std::string &configPath);

  /**
   * @brief Destructor
   * 
   * Saves the current configuration to file if auto-save is enabled.
   */
  ~AuthManager();

  /**
   * @brief Authenticate a user with username and password
   * 
   * @param username The username to authenticate
   * @param password The password to verify
   * @param ipAddress The IP address of the client making the request
   * @return Authentication token if successful, empty string if failed
   */
  std::string authenticate(const std::string &username, 
                          const std::string &password,
                          const std::string &ipAddress = "");

  /**
   * @brief Validate an authentication token
   * 
   * @param token The token to validate
   * @return True if the token is valid and not expired, false otherwise
   */
  bool validateToken(const std::string &token);

  /**
   * @brief Get the username associated with a token
   * 
   * @param token The authentication token
   * @return Username if token is valid, empty string otherwise
   */
  std::string getUsernameFromToken(const std::string &token);

  /**
   * @brief Revoke a session token
   * 
   * @param token The token to revoke
   * @return True if the token was successfully revoked, false if not found
   */
  bool revokeToken(const std::string &token);

  /**
   * @brief Add a new user
   * 
   * @param username The username for the new user
   * @param password The password for the new user
   * @param permissions List of permissions for the user
   * @return True if user was added successfully, false if username already exists
   */
  bool addUser(const std::string &username, 
               const std::string &password,
               const std::vector<std::string> &permissions = {});

  /**
   * @brief Remove a user
   * 
   * @param username The username to remove
   * @return True if user was removed successfully, false if not found
   */
  bool removeUser(const std::string &username);

  /**
   * @brief Change a user's password
   * 
   * @param username The username
   * @param oldPassword The current password
   * @param newPassword The new password
   * @return True if password was changed successfully, false otherwise
   */
  bool changePassword(const std::string &username,
                     const std::string &oldPassword,
                     const std::string &newPassword);

  /**
   * @brief Check if a user has a specific permission
   * 
   * @param username The username to check
   * @param permission The permission to verify
   * @return True if user has the permission, false otherwise
   */
  bool hasPermission(const std::string &username, const std::string &permission);

  /**
   * @brief Add a permission to a user
   * 
   * @param username The username
   * @param permission The permission to add
   * @return True if permission was added, false if user not found
   */
  bool addPermission(const std::string &username, const std::string &permission);

  /**
   * @brief Remove a permission from a user
   * 
   * @param username The username
   * @param permission The permission to remove
   * @return True if permission was removed, false if user not found
   */
  bool removePermission(const std::string &username, const std::string &permission);

  /**
   * @brief Get all active sessions
   * 
   * @return Vector of all active sessions
   */
  std::vector<Session> getActiveSessions() const;

  /**
   * @brief Get login attempts for a specific IP address
   * 
   * @param ipAddress The IP address to check
   * @param timeWindow Time window in minutes to check (default: 60)
   * @return Vector of login attempts within the time window
   */
  std::vector<LoginAttempt> getLoginAttempts(const std::string &ipAddress, 
                                           int timeWindow = 60) const;

  /**
   * @brief Check if an IP address is rate limited
   * 
   * @param ipAddress The IP address to check
   * @return True if the IP is rate limited, false otherwise
   */
  bool isRateLimited(const std::string &ipAddress) const;

  /**
   * @brief Set the maximum number of failed login attempts before rate limiting
   * 
   * @param maxAttempts Maximum failed attempts (default: 5)
   */
  void setMaxFailedAttempts(int maxAttempts);

  /**
   * @brief Set the rate limit duration in minutes
   * 
   * @param minutes Duration in minutes (default: 15)
   */
  void setRateLimitDuration(int minutes);

  /**
   * @brief Set the session timeout in minutes
   * 
   * @param minutes Session timeout (default: 60)
   */
  void setSessionTimeout(int minutes);

  /**
   * @brief Enable or disable automatic configuration saving
   * 
   * @param enabled True to enable auto-save, false to disable
   */
  void setAutoSave(bool enabled);

  /**
   * @brief Load configuration from file
   * 
   * @param configPath Path to the configuration file
   * @return True if loaded successfully, false otherwise
   */
  bool loadConfiguration(const std::string &configPath = "");

  /**
   * @brief Save configuration to file
   * 
   * @param configPath Path to save the configuration (optional)
   * @return True if saved successfully, false otherwise
   */
  bool saveConfiguration(const std::string &configPath = "") const;

  /**
   * @brief Clean up expired sessions and old login attempts
   */
  void cleanup();

private:
  struct UserInfo {
    std::string passwordHash;
    std::vector<std::string> permissions;
    bool enabled = true;
  };

  // Internal methods
  std::string hashPassword(const std::string &password) const;
  bool verifyPassword(const std::string &password, const std::string &hash) const;
  std::string generateToken(const std::string &username) const;
  void recordLoginAttempt(const std::string &ipAddress, bool successful);
  void cleanupExpiredSessions();
  void cleanupOldLoginAttempts();

  // Configuration
  std::string configPath_;
  bool autoSave_ = true;
  int maxFailedAttempts_ = 5;
  int rateLimitDurationMinutes_ = 15;
  int sessionTimeoutMinutes_ = 60;
  std::string jwtSecret_;

  // Data storage
  mutable std::shared_mutex usersMutex_;
  std::unordered_map<std::string, UserInfo> users_;

  mutable std::shared_mutex sessionsMutex_;
  std::unordered_map<std::string, Session> sessions_;
  
  mutable std::mutex attemptsMutex_;
  std::vector<LoginAttempt> loginAttempts_;
};

} // namespace server
} // namespace astrocomm

#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace astrocomm {

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
   * @param configFilePath Path to the configuration file
   * @param autoSave Whether to automatically save changes to the configuration file
   */
  AuthManager(const std::string &configFilePath, bool autoSave = true);
  
  /**
   * @brief Destructor
   * 
   * Saves configuration if auto-save is enabled.
   */
  ~AuthManager();

  /**
   * @brief Authenticate a user with given method and credentials
   * 
   * @param method Authentication method ("jwt" or "basic")
   * @param credentials Authentication credentials
   * @param ipAddress IP address of the client (optional)
   * @return true if authentication is successful
   * @return false if authentication fails
   */
  bool authenticate(const std::string &method, const std::string &credentials, 
                   const std::string &ipAddress = "");

  /**
   * @brief Create a login session for a user
   * 
   * @param username Username to create session for
   * @param ipAddress IP address of the client
   * @param expirationMinutes Minutes until session expiration (default: 60)
   * @return std::string JWT token for the session
   */
  std::string createSession(const std::string &username, 
                           const std::string &ipAddress,
                           int expirationMinutes = 60);
                           
  /**
   * @brief Invalidate a user session
   * 
   * @param token Session token to invalidate
   * @return true if session was found and invalidated
   * @return false if session was not found
   */
  bool invalidateSession(const std::string &token);
  
  /**
   * @brief Invalidate all sessions for a specific user
   * 
   * @param username Username whose sessions should be invalidated
   * @return int Number of sessions invalidated
   */
  int invalidateUserSessions(const std::string &username);

  /**
   * @brief Add a new user
   * 
   * @param username Username to add
   * @param password User's password
   * @param saveChanges Whether to save changes to config file (if auto-save enabled)
   * @return true if user was added successfully
   * @return false if user already exists or other error occurred
   */
  bool addUser(const std::string &username, const std::string &password, bool saveChanges = true);

  /**
   * @brief Remove an existing user
   * 
   * @param username Username to remove
   * @param saveChanges Whether to save changes to config file (if auto-save enabled)
   * @return true if user was removed successfully
   * @return false if user doesn't exist or other error occurred
   */
  bool removeUser(const std::string &username, bool saveChanges = true);

  /**
   * @brief Change a user's password
   * 
   * @param username Username whose password should be changed
   * @param newPassword New password
   * @param saveChanges Whether to save changes to config file (if auto-save enabled)
   * @return true if password was changed successfully
   * @return false if user doesn't exist or other error occurred
   */
  bool changePassword(const std::string &username, const std::string &newPassword, 
                     bool saveChanges = true);

  /**
   * @brief Check if a user exists
   * 
   * @param username Username to check
   * @return true if user exists
   * @return false if user doesn't exist
   */
  bool userExists(const std::string &username) const;

  /**
   * @brief Save user configuration to file
   * 
   * @param filePath Path to save configuration to (uses default if empty)
   * @return true if configuration was saved successfully
   * @return false if an error occurred
   */
  bool saveUserConfiguration(const std::string &filePath = "") const;

  /**
   * @brief Load user configuration from file
   * 
   * @param filePath Path to load configuration from
   * @return true if configuration was loaded successfully
   * @return false if an error occurred
   */
  bool loadUserConfiguration(const std::string &filePath);
  
  /**
   * @brief Set the default configuration file path
   * 
   * @param path New default path
   */
  void setConfigFilePath(const std::string &path);
  
  /**
   * @brief Enable or disable auto-saving of configuration
   * 
   * @param enable Whether to enable auto-saving
   */
  void enableAutoSave(bool enable);

private:
  /**
   * @brief Validate a JWT token
   * 
   * @param token JWT token to validate
   * @return true if token is valid
   * @return false if token is invalid
   */
  bool validateJwt(const std::string &token);

  /**
   * @brief Validate basic authentication credentials
   * 
   * @param credentials Base64 encoded credentials
   * @param ipAddress IP address of client
   * @return true if credentials are valid
   * @return false if credentials are invalid
   */
  bool validateBasicAuth(const std::string &credentials, const std::string &ipAddress = "");

  /**
   * @brief Generate a JWT token for a user
   * 
   * @param username Username to generate token for
   * @param expirationMinutes Minutes until token expiration
   * @return std::string Generated JWT token
   */
  std::string generateJwt(const std::string &username, int expirationMinutes = 60);

  /**
   * @brief Hash a password with optional salt
   * 
   * @param password Password to hash
   * @param salt Salt to use (random if empty)
   * @return std::string Hashed password
   */
  std::string hashPassword(const std::string &password,
                           const std::string &salt = "") const;
                           
  /**
   * @brief Record a login attempt
   * 
   * @param username Username that attempted login
   * @param ipAddress IP address of client
   * @param successful Whether the login was successful
   */
  void recordLoginAttempt(const std::string &username, 
                         const std::string &ipAddress, 
                         bool successful);
                         
  /**
   * @brief Check if login should be blocked due to too many failed attempts
   * 
   * @param username Username to check
   * @param ipAddress IP address to check
   * @return true if login should be blocked
   * @return false if login is allowed
   */
  bool isLoginBlocked(const std::string &username, const std::string &ipAddress);
  
  /**
   * @brief Clean up expired sessions
   */
  void cleanExpiredSessions();

  mutable std::mutex usersMutex;       ///< Mutex for users map
  mutable std::mutex sessionsMutex;    ///< Mutex for sessions map
  mutable std::mutex attemptsMutex;    ///< Mutex for login attempts map
  
  std::unordered_map<std::string, std::string> users; ///< username -> hashed_password
  std::vector<Session> activeSessions;               ///< Active login sessions
  std::vector<LoginAttempt> loginAttempts;           ///< Login attempt history
  
  std::string jwtSecret;               ///< Secret for JWT token generation
  std::string configFilePath;          ///< Default configuration file path
  bool autoSaveEnabled;                ///< Whether to auto-save configuration
  
  int maxLoginAttempts;                ///< Maximum failed login attempts
  int loginBlockDurationMinutes;       ///< Duration to block login after too many failed attempts
  int loginHistorySize;                ///< Maximum number of login attempts to store
};

} // namespace astrocomm
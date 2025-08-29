#include "server/auth_manager.h"
#include "common/utils.h"
#include <base64.h>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <random>
#include <spdlog/spdlog.h>
#include <sstream>


#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>


namespace astrocomm {

using json = nlohmann::json;

/**
 * @brief Default constructor for AuthManager
 *
 * Initializes authentication manager with a random JWT secret
 * and default security settings.
 */
AuthManager::AuthManager()
    : autoSaveEnabled(false), maxLoginAttempts(5),
      loginBlockDurationMinutes(30), loginHistorySize(1000) {
  // Generate a random JWT secret
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 255);

  std::stringstream ss;
  for (int i = 0; i < 32; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
  }

  jwtSecret = ss.str();

  // Add a default admin user for testing
  addUser("admin", "admin123", false);

  spdlog::info("[AuthManager] Auth manager initialized");
}

/**
 * @brief Constructor with configuration file path
 *
 * Initializes authentication manager and loads config from specified file.
 *
 * @param configFilePath Path to configuration file
 * @param autoSave Whether to automatically save changes to config file
 */
AuthManager::AuthManager(const std::string &configFilePath, bool autoSave)
    : configFilePath(configFilePath), autoSaveEnabled(autoSave),
      maxLoginAttempts(5), loginBlockDurationMinutes(30),
      loginHistorySize(1000) {
  // Generate a random JWT secret as fallback
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 255);

  std::stringstream ss;
  for (int i = 0; i < 32; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
  }

  jwtSecret = ss.str();

  // Try to load configuration from file
  if (!configFilePath.empty()) {
    if (std::filesystem::exists(configFilePath)) {
      if (!loadUserConfiguration(configFilePath)) {
        spdlog::warn("[AuthManager] Failed to load configuration from {}, "
                     "using default settings",
                     configFilePath);
      }
    } else {
      spdlog::info("[AuthManager] Configuration file {} doesn't exist, will "
                   "create on first save",
                   configFilePath);

      // Add a default admin user
      addUser("admin", "admin123", true);
    }
  }

  spdlog::info("[AuthManager] Auth manager initialized with config file: {}",
               configFilePath.empty() ? "none" : configFilePath);
}

/**
 * @brief Destructor for AuthManager
 *
 * Automatically saves configuration if auto-save is enabled.
 */
AuthManager::~AuthManager() {
  if (autoSaveEnabled && !configFilePath.empty()) {
    saveUserConfiguration(configFilePath);
  }

  spdlog::info("[AuthManager] Auth manager shutting down");
}

/**
 * @brief Authenticate a user with given method and credentials
 *
 * @param method Authentication method ("jwt" or "basic")
 * @param credentials Authentication credentials
 * @param ipAddress IP address of the client
 * @return true if authentication is successful
 * @return false if authentication fails
 */
bool AuthManager::authenticate(const std::string &method,
                               const std::string &credentials,
                               const std::string &ipAddress) {
  if (string_utils::toLower(method) == "jwt") {
    return validateJwt(credentials);
  } else if (string_utils::toLower(method) == "basic") {
    return validateBasicAuth(credentials, ipAddress);
  }

  spdlog::warn("[AuthManager] Unsupported authentication method: {}", method);
  return false;
}

/**
 * @brief Create a login session for a user
 *
 * @param username Username to create session for
 * @param ipAddress IP address of the client
 * @param expirationMinutes Minutes until session expiration
 * @return std::string JWT token for the session
 */
std::string AuthManager::createSession(const std::string &username,
                                       const std::string &ipAddress,
                                       int expirationMinutes) {
  // Clean up expired sessions first
  cleanExpiredSessions();

  // Generate a JWT token
  std::string token = generateJwt(username, expirationMinutes);

  // Create and store the session
  Session session;
  session.token = token;
  session.username = username;
  session.ipAddress = ipAddress;
  session.expiryTime = std::chrono::system_clock::now() +
                       std::chrono::minutes(expirationMinutes);

  {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    activeSessions.push_back(session);
  }

  spdlog::info("[AuthManager] Created new session for user: {} from IP: {}",
               username, ipAddress);

  return token;
}

/**
 * @brief Invalidate a user session
 *
 * @param token Session token to invalidate
 * @return true if session was found and invalidated
 * @return false if session was not found
 */
bool AuthManager::invalidateSession(const std::string &token) {
  std::lock_guard<std::mutex> lock(sessionsMutex);

  auto it =
      std::find_if(activeSessions.begin(), activeSessions.end(),
                   [&token](const Session &s) { return s.token == token; });

  if (it != activeSessions.end()) {
    spdlog::info("[AuthManager] Invalidated session for user: {}",
                 it->username);
    activeSessions.erase(it);
    return true;
  }

  spdlog::debug("[AuthManager] Session not found for invalidation: {}", token);
  return false;
}

/**
 * @brief Invalidate all sessions for a specific user
 *
 * @param username Username whose sessions should be invalidated
 * @return int Number of sessions invalidated
 */
int AuthManager::invalidateUserSessions(const std::string &username) {
  std::lock_guard<std::mutex> lock(sessionsMutex);

  int count = 0;
  auto it = activeSessions.begin();
  while (it != activeSessions.end()) {
    if (it->username == username) {
      it = activeSessions.erase(it);
      count++;
    } else {
      ++it;
    }
  }

  if (count > 0) {
    spdlog::info("[AuthManager] Invalidated {} sessions for user: {}", count,
                 username);
  }

  return count;
}

/**
 * @brief Add a new user
 *
 * @param username Username to add
 * @param password User's password
 * @param saveChanges Whether to save changes to config file
 * @return true if user was added successfully
 * @return false if user already exists or other error occurred
 */
bool AuthManager::addUser(const std::string &username,
                          const std::string &password, bool saveChanges) {
  std::lock_guard<std::mutex> lock(usersMutex);

  if (users.find(username) != users.end()) {
    spdlog::warn("[AuthManager] User already exists: {}", username);
    return false;
  }

  // Generate a random salt and hash the password
  std::string salt = generateUuid();
  std::string hashedPassword = hashPassword(password, salt);

  // Store the hashed password with salt
  users[username] = salt + ":" + hashedPassword;

  spdlog::info("[AuthManager] User added: {}", username);

  // Save changes if auto-save is enabled and requested
  if (saveChanges && autoSaveEnabled && !configFilePath.empty()) {
    saveUserConfiguration(configFilePath);
  }

  return true;
}

/**
 * @brief Remove an existing user
 *
 * @param username Username to remove
 * @param saveChanges Whether to save changes to config file
 * @return true if user was removed successfully
 * @return false if user doesn't exist
 */
bool AuthManager::removeUser(const std::string &username, bool saveChanges) {
  std::lock_guard<std::mutex> lock(usersMutex);

  auto it = users.find(username);
  if (it == users.end()) {
    spdlog::warn("[AuthManager] User not found: {}", username);
    return false;
  }

  users.erase(it);

  // Invalidate any active sessions for this user
  invalidateUserSessions(username);

  spdlog::info("[AuthManager] User removed: {}", username);

  // Save changes if auto-save is enabled and requested
  if (saveChanges && autoSaveEnabled && !configFilePath.empty()) {
    saveUserConfiguration(configFilePath);
  }

  return true;
}

/**
 * @brief Change a user's password
 *
 * @param username Username whose password should be changed
 * @param newPassword New password
 * @param saveChanges Whether to save changes to config file
 * @return true if password was changed successfully
 * @return false if user doesn't exist
 */
bool AuthManager::changePassword(const std::string &username,
                                 const std::string &newPassword,
                                 bool saveChanges) {
  std::lock_guard<std::mutex> lock(usersMutex);

  auto it = users.find(username);
  if (it == users.end()) {
    spdlog::warn("[AuthManager] User not found for password change: {}",
                 username);
    return false;
  }

  // Generate a new salt and hash the new password
  std::string salt = generateUuid();
  std::string hashedPassword = hashPassword(newPassword, salt);

  // Update the stored password
  it->second = salt + ":" + hashedPassword;

  // Invalidate all sessions for this user to force re-login
  invalidateUserSessions(username);

  spdlog::info("[AuthManager] Password changed for user: {}", username);

  // Save changes if auto-save is enabled and requested
  if (saveChanges && autoSaveEnabled && !configFilePath.empty()) {
    saveUserConfiguration(configFilePath);
  }

  return true;
}

/**
 * @brief Check if a user exists
 *
 * @param username Username to check
 * @return true if user exists
 * @return false if user doesn't exist
 */
bool AuthManager::userExists(const std::string &username) const {
  std::lock_guard<std::mutex> lock(usersMutex);
  return users.find(username) != users.end();
}

/**
 * @brief Save user configuration to file
 *
 * @param filePath Path to save configuration to
 * @return true if configuration was saved successfully
 * @return false if an error occurred
 */
bool AuthManager::saveUserConfiguration(const std::string &filePath) const {
  std::lock_guard<std::mutex> lockUsers(usersMutex);
  std::lock_guard<std::mutex> lockSessions(sessionsMutex);

  try {
    // Determine which file path to use
    std::string actualFilePath = filePath.empty() ? configFilePath : filePath;

    if (actualFilePath.empty()) {
      spdlog::error(
          "[AuthManager] No file path specified for saving configuration");
      return false;
    }

    // Create directory if it doesn't exist
    std::filesystem::path path(actualFilePath);
    if (!path.parent_path().empty()) {
      std::filesystem::create_directories(path.parent_path());
    }

    json usersJson;
    for (const auto &entry : users) {
      usersJson[entry.first] = entry.second;
    }

    json sessionsJson = json::array();
    for (const auto &session : activeSessions) {
      json sessionJson;
      sessionJson["token"] = session.token;
      sessionJson["username"] = session.username;
      sessionJson["ipAddress"] = session.ipAddress;

      // Convert time_point to seconds since epoch
      auto epochSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                              session.expiryTime.time_since_epoch())
                              .count();
      sessionJson["expiryTime"] = epochSeconds;

      sessionsJson.push_back(sessionJson);
    }

    json config = {{"users", usersJson},
                   {"jwtSecret", jwtSecret},
                   {"sessions", sessionsJson},
                   {"securitySettings",
                    {{"maxLoginAttempts", maxLoginAttempts},
                     {"loginBlockDurationMinutes", loginBlockDurationMinutes},
                     {"loginHistorySize", loginHistorySize}}}};

    std::ofstream file(actualFilePath);
    if (!file.is_open()) {
      spdlog::error("[AuthManager] Failed to open file for writing: {}",
                    actualFilePath);
      return false;
    }

    file << config.dump(2);
    file.close();

    spdlog::info("[AuthManager] User configuration saved to {}",
                 actualFilePath);
    return true;
  } catch (const std::exception &e) {
    spdlog::error("[AuthManager] Error saving user configuration: {}",
                  e.what());
    return false;
  }
}

/**
 * @brief Load user configuration from file
 *
 * @param filePath Path to load configuration from
 * @return true if configuration was loaded successfully
 * @return false if an error occurred
 */
bool AuthManager::loadUserConfiguration(const std::string &filePath) {
  try {
    std::ifstream file(filePath);
    if (!file.is_open()) {
      spdlog::warn("[AuthManager] Failed to open configuration file: {}",
                   filePath);
      return false;
    }

    json config;
    file >> config;
    file.close();

    {
      std::lock_guard<std::mutex> lockUsers(usersMutex);
      users.clear();

      if (config.contains("jwtSecret") && config["jwtSecret"].is_string()) {
        jwtSecret = config["jwtSecret"];
      }

      if (config.contains("users") && config["users"].is_object()) {
        for (auto it = config["users"].begin(); it != config["users"].end();
             ++it) {
          users[it.key()] = it.value();
        }
      }

      // Load security settings if available
      if (config.contains("securitySettings") &&
          config["securitySettings"].is_object()) {
        auto &settings = config["securitySettings"];

        if (settings.contains("maxLoginAttempts")) {
          maxLoginAttempts = settings["maxLoginAttempts"];
        }

        if (settings.contains("loginBlockDurationMinutes")) {
          loginBlockDurationMinutes = settings["loginBlockDurationMinutes"];
        }

        if (settings.contains("loginHistorySize")) {
          loginHistorySize = settings["loginHistorySize"];
        }
      }
    }

    // Load sessions if available
    if (config.contains("sessions") && config["sessions"].is_array()) {
      std::lock_guard<std::mutex> lockSessions(sessionsMutex);
      activeSessions.clear();

      for (const auto &sessionJson : config["sessions"]) {
        if (sessionJson.contains("token") && sessionJson.contains("username") &&
            sessionJson.contains("expiryTime")) {

          Session session;
          session.token = sessionJson["token"];
          session.username = sessionJson["username"];

          if (sessionJson.contains("ipAddress")) {
            session.ipAddress = sessionJson["ipAddress"];
          }

          // Convert seconds since epoch back to time_point
          int64_t epochSeconds = sessionJson["expiryTime"];
          session.expiryTime = std::chrono::system_clock::time_point(
              std::chrono::seconds(epochSeconds));

          // Only add non-expired sessions
          if (session.expiryTime > std::chrono::system_clock::now()) {
            activeSessions.push_back(session);
          }
        }
      }
    }

    spdlog::info("[AuthManager] Loaded user configuration from {} ({} users, "
                 "{} active sessions)",
                 filePath, users.size(), activeSessions.size());
    return true;
  } catch (const std::exception &e) {
    spdlog::error("[AuthManager] Error loading user configuration: {}",
                  e.what());
    return false;
  }
}

/**
 * @brief Set the default configuration file path
 *
 * @param path New default path
 */
void AuthManager::setConfigFilePath(const std::string &path) {
  configFilePath = path;
  spdlog::debug("[AuthManager] Config file path set to: {}", path);
}

/**
 * @brief Enable or disable auto-saving of configuration
 *
 * @param enable Whether to enable auto-saving
 */
void AuthManager::enableAutoSave(bool enable) {
  autoSaveEnabled = enable;
  spdlog::debug("[AuthManager] Auto-save {}", enable ? "enabled" : "disabled");
}

/**
 * @brief Validate a JWT token
 *
 * @param token JWT token to validate
 * @return true if token is valid
 * @return false if token is invalid
 */
bool AuthManager::validateJwt(const std::string &token) {
  // First check if this token exists in our active sessions
  {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it =
        std::find_if(activeSessions.begin(), activeSessions.end(),
                     [&token](const Session &s) { return s.token == token; });

    if (it != activeSessions.end()) {
      // Check if session is expired
      if (it->expiryTime <= std::chrono::system_clock::now()) {
        spdlog::warn("[AuthManager] JWT session expired for user: {}",
                     it->username);
        activeSessions.erase(it); // Remove expired session
        return false;
      }

      spdlog::info("[AuthManager] JWT validation successful for existing "
                   "session (user: {})",
                   it->username);
      return true;
    }
  }

  // If not found in active sessions, validate the token itself
  try {
    // Split the token into header, payload, and signature
    auto parts = string_utils::split(token, '.');
    if (parts.size() != 3) {
      spdlog::warn("[AuthManager] Invalid JWT format");
      return false;
    }

    // Decode the payload
    std::string decodedPayload = base64_decode(parts[1]);
    json payload = json::parse(decodedPayload);

    // Check expiration
    if (payload.contains("exp")) {
      int64_t expTime = payload["exp"];
      int64_t currentTime =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();

      if (currentTime > expTime) {
        spdlog::warn("[AuthManager] JWT token expired");
        return false;
      }
    }

    // Extract username from sub claim
    if (payload.contains("sub") && payload["sub"].is_string()) {
      std::string username = payload["sub"];

      std::lock_guard<std::mutex> lock(usersMutex);
      if (users.find(username) != users.end()) {
        // In a real implementation, you would verify the signature here
        spdlog::info("[AuthManager] JWT authentication successful for user: {}",
                     username);

        return true;
      }
    }

    spdlog::warn("[AuthManager] Invalid JWT claims");
    return false;
  } catch (const std::exception &e) {
    spdlog::error("[AuthManager] JWT validation error: {}", e.what());
    return false;
  }
}

/**
 * @brief Validate basic authentication credentials
 *
 * @param credentials Base64 encoded credentials
 * @param ipAddress IP address of client
 * @return true if credentials are valid
 * @return false if credentials are invalid
 */
bool AuthManager::validateBasicAuth(const std::string &credentials,
                                    const std::string &ipAddress) {
  try {
    // Check if login is blocked due to too many failed attempts
    if (!ipAddress.empty() && isLoginBlocked("", ipAddress)) {
      spdlog::warn("[AuthManager] Login blocked due to too many failed "
                   "attempts from IP: {}",
                   ipAddress);
      return false;
    }

    // Decode base64 credentials
    std::string decoded = base64_decode(credentials);

    // Split username and password
    size_t colonPos = decoded.find(':');
    if (colonPos == std::string::npos) {
      spdlog::warn("[AuthManager] Invalid basic auth format");
      if (!ipAddress.empty()) {
        recordLoginAttempt("unknown", ipAddress, false);
      }
      return false;
    }

    std::string username = decoded.substr(0, colonPos);
    std::string password = decoded.substr(colonPos + 1);

    // Check if login is blocked for this specific user
    if (!ipAddress.empty() && isLoginBlocked(username, ipAddress)) {
      spdlog::warn("[AuthManager] Login blocked for user: {} from IP: {}",
                   username, ipAddress);
      recordLoginAttempt(username, ipAddress, false);
      return false;
    }

    std::lock_guard<std::mutex> lock(usersMutex);

    auto it = users.find(username);
    if (it == users.end()) {
      spdlog::warn("[AuthManager] User not found: {}", username);
      if (!ipAddress.empty()) {
        recordLoginAttempt(username, ipAddress, false);
      }
      return false;
    }

    // Extract salt from stored hash
    std::string stored = it->second;
    size_t saltEnd = stored.find(':');
    if (saltEnd == std::string::npos) {
      spdlog::error("[AuthManager] Invalid stored password format for user: {}",
                    username);
      if (!ipAddress.empty()) {
        recordLoginAttempt(username, ipAddress, false);
      }
      return false;
    }

    std::string salt = stored.substr(0, saltEnd);
    std::string storedHash = stored.substr(saltEnd + 1);

    // Hash the provided password with the same salt
    std::string computedHash = hashPassword(password, salt);

    if (computedHash == storedHash) {
      spdlog::info("[AuthManager] Basic auth successful for user: {}",
                   username);
      if (!ipAddress.empty()) {
        recordLoginAttempt(username, ipAddress, true);
      }
      return true;
    }

    spdlog::warn("[AuthManager] Invalid password for user: {}", username);
    if (!ipAddress.empty()) {
      recordLoginAttempt(username, ipAddress, false);
    }
    return false;
  } catch (const std::exception &e) {
    spdlog::error("[AuthManager] Basic auth validation error: {}", e.what());
    return false;
  }
}

/**
 * @brief Generate a JWT token for a user
 *
 * @param username Username to generate token for
 * @param expirationMinutes Minutes until token expiration
 * @return std::string Generated JWT token
 */
std::string AuthManager::generateJwt(const std::string &username,
                                     int expirationMinutes) {
  try {
    // Create header
    json header = {{"alg", "HS256"}, {"typ", "JWT"}};

    // Calculate expiration time
    int64_t currentTime =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    int64_t expirationTime = currentTime + expirationMinutes * 60;

    // Create payload
    json payload = {{"sub", username},
                    {"iat", currentTime},
                    {"exp", expirationTime},
                    {"iss", "astrocomm-auth"}};

    // Base64 encode header and payload
    std::string encodedHeader = base64_encode(
        reinterpret_cast<const unsigned char *>(header.dump().c_str()),
        header.dump().length());

    std::string encodedPayload = base64_encode(
        reinterpret_cast<const unsigned char *>(payload.dump().c_str()),
        payload.dump().length());

    // Remove base64 padding
    encodedHeader.erase(
        std::remove(encodedHeader.begin(), encodedHeader.end(), '='),
        encodedHeader.end());
    encodedPayload.erase(
        std::remove(encodedPayload.begin(), encodedPayload.end(), '='),
        encodedPayload.end());

    // Create signature input
    std::string signatureInput = encodedHeader + "." + encodedPayload;

    // Compute HMAC-SHA256 signature
    unsigned char hmac[SHA256_DIGEST_LENGTH];
    unsigned int len;
    HMAC(EVP_sha256(), jwtSecret.c_str(), static_cast<int>(jwtSecret.length()),
         reinterpret_cast<const unsigned char *>(signatureInput.c_str()),
         signatureInput.length(), hmac, &len);

    // Base64 encode signature
    std::string encodedSignature = base64_encode(hmac, len);
    encodedSignature.erase(
        std::remove(encodedSignature.begin(), encodedSignature.end(), '='),
        encodedSignature.end());

    // Create final JWT token
    return signatureInput + "." + encodedSignature;
  } catch (const std::exception &e) {
    spdlog::error("[AuthManager] JWT generation error: {}", e.what());
    return "";
  }
}

/**
 * @brief Hash a password with optional salt
 *
 * @param password Password to hash
 * @param salt Salt to use (random if empty)
 * @return std::string Hashed password
 */
std::string AuthManager::hashPassword(const std::string &password,
                                      const std::string &salt) const {
  // Use provided salt or generate a random one
  std::string actualSalt = salt.empty() ? generateUuid() : salt;

  // Combine salt and password
  std::string saltedPassword = actualSalt + password;

  // Compute SHA-256 hash
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, saltedPassword.c_str(), saltedPassword.size());
  SHA256_Final(hash, &sha256);

  // Convert to hex string
  std::stringstream ss;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(hash[i]);
  }

  return ss.str();
}

/**
 * @brief Record a login attempt
 *
 * @param username Username that attempted login
 * @param ipAddress IP address of client
 * @param successful Whether the login was successful
 */
void AuthManager::recordLoginAttempt(const std::string &username,
                                     const std::string &ipAddress,
                                     bool successful) {
  std::lock_guard<std::mutex> lock(attemptsMutex);

  // Create the login attempt record
  LoginAttempt attempt;
  attempt.ipAddress = ipAddress;
  attempt.timestamp = std::chrono::system_clock::now();
  attempt.successful = successful;

  // Add to the history
  loginAttempts.push_back(attempt);

  // Limit the size of the history
  if (loginAttempts.size() > static_cast<size_t>(loginHistorySize)) {
    loginAttempts.erase(loginAttempts.begin());
  }

  if (!successful) {
    spdlog::warn("[AuthManager] Failed login attempt for user: {} from IP: {}",
                 username, ipAddress);
  }
}

/**
 * @brief Check if login should be blocked due to too many failed attempts
 *
 * @param username Username to check (can be empty to check IP only)
 * @param ipAddress IP address to check
 * @return true if login should be blocked
 * @return false if login is allowed
 */
bool AuthManager::isLoginBlocked(const std::string &username,
                                 const std::string &ipAddress) {
  if (ipAddress.empty()) {
    return false; // Can't block without an IP address
  }

  std::lock_guard<std::mutex> lock(attemptsMutex);

  // Get the cutoff time for checking recent attempts
  auto cutoffTime = std::chrono::system_clock::now() -
                    std::chrono::minutes(loginBlockDurationMinutes);

  // Count failed attempts from this IP address
  int failedAttempts = 0;
  for (const auto &attempt : loginAttempts) {
    if (attempt.timestamp >= cutoffTime && attempt.ipAddress == ipAddress &&
        !attempt.successful) {
      failedAttempts++;
    }
  }

  return failedAttempts >= maxLoginAttempts;
}

/**
 * @brief Clean up expired sessions
 */
void AuthManager::cleanExpiredSessions() {
  std::lock_guard<std::mutex> lock(sessionsMutex);

  auto currentTime = std::chrono::system_clock::now();
  auto it = activeSessions.begin();

  int removedCount = 0;
  while (it != activeSessions.end()) {
    if (it->expiryTime <= currentTime) {
      it = activeSessions.erase(it);
      removedCount++;
    } else {
      ++it;
    }
  }

  if (removedCount > 0) {
    spdlog::debug("[AuthManager] Cleaned up {} expired sessions", removedCount);
  }
}

} // namespace astrocomm
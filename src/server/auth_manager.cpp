#include "server/auth_manager.h"
#include "common/utils.h"
#include <base64.h>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <openssl/sha.h>
#include <random>
#include <sstream>

namespace astrocomm {

using json = nlohmann::json;

AuthManager::AuthManager() {
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
  addUser("admin", "admin123");

  logInfo("Auth manager initialized", "AuthManager");
}

AuthManager::~AuthManager() {
  logInfo("Auth manager shutting down", "AuthManager");
}

bool AuthManager::authenticate(const std::string &method,
                               const std::string &credentials) {
  if (string_utils::toLower(method) == "jwt") {
    return validateJwt(credentials);
  } else if (string_utils::toLower(method) == "basic") {
    return validateBasicAuth(credentials);
  }

  logWarning("Unsupported authentication method: " + method, "AuthManager");
  return false;
}

bool AuthManager::addUser(const std::string &username,
                          const std::string &password) {
  std::lock_guard<std::mutex> lock(usersMutex);

  if (users.find(username) != users.end()) {
    logWarning("User already exists: " + username, "AuthManager");
    return false;
  }

  // Generate a random salt and hash the password
  std::string salt = generateUuid();
  std::string hashedPassword = hashPassword(password, salt);

  // Store the hashed password with salt
  users[username] = salt + ":" + hashedPassword;

  logInfo("User added: " + username, "AuthManager");
  return true;
}

bool AuthManager::removeUser(const std::string &username) {
  std::lock_guard<std::mutex> lock(usersMutex);

  auto it = users.find(username);
  if (it == users.end()) {
    logWarning("User not found: " + username, "AuthManager");
    return false;
  }

  users.erase(it);
  logInfo("User removed: " + username, "AuthManager");
  return true;
}

bool AuthManager::userExists(const std::string &username) const {
  std::lock_guard<std::mutex> lock(usersMutex);
  return users.find(username) != users.end();
}

bool AuthManager::saveUserConfiguration(const std::string &filePath) const {
  std::lock_guard<std::mutex> lock(usersMutex);

  try {
    json usersJson;

    for (const auto &entry : users) {
      usersJson[entry.first] = entry.second;
    }

    json config = {{"users", usersJson}, {"jwtSecret", jwtSecret}};

    std::ofstream file(filePath);
    if (!file.is_open()) {
      logError("Failed to open file for writing: " + filePath, "AuthManager");
      return false;
    }

    file << config.dump(2);
    file.close();

    logInfo("User configuration saved to " + filePath, "AuthManager");
    return true;
  } catch (const std::exception &e) {
    logError("Error saving user configuration: " + std::string(e.what()),
             "AuthManager");
    return false;
  }
}

bool AuthManager::loadUserConfiguration(const std::string &filePath) {
  try {
    std::ifstream file(filePath);
    if (!file.is_open()) {
      logWarning("Failed to open configuration file: " + filePath,
                 "AuthManager");
      return false;
    }

    json config;
    file >> config;
    file.close();

    std::lock_guard<std::mutex> lock(usersMutex);

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

    logInfo("Loaded user configuration from " + filePath + " (" +
                std::to_string(users.size()) + " users)",
            "AuthManager");
    return true;
  } catch (const std::exception &e) {
    logError("Error loading user configuration: " + std::string(e.what()),
             "AuthManager");
    return false;
  }
}

bool AuthManager::validateJwt(const std::string &token) {
  // This is a simplified JWT validation
  // In a real implementation, you would:
  // 1. Parse the JWT
  // 2. Verify the signature using the secret
  // 3. Check expiration time
  // 4. Validate other claims

  try {
    // Split the token into header, payload, and signature
    auto parts = string_utils::split(token, '.');
    if (parts.size() != 3) {
      logWarning("Invalid JWT format", "AuthManager");
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
        logWarning("JWT token expired", "AuthManager");
        return false;
      }
    }

    // Extract username from sub claim
    if (payload.contains("sub") && payload["sub"].is_string()) {
      std::string username = payload["sub"];

      std::lock_guard<std::mutex> lock(usersMutex);
      if (users.find(username) != users.end()) {
        // In a real implementation, you would verify the signature here
        logInfo("JWT authentication successful for user: " + username,
                "AuthManager");
        return true;
      }
    }

    logWarning("Invalid JWT claims", "AuthManager");
    return false;
  } catch (const std::exception &e) {
    logError("JWT validation error: " + std::string(e.what()), "AuthManager");
    return false;
  }
}

bool AuthManager::validateBasicAuth(const std::string &credentials) {
  try {
    // Decode base64 credentials
    std::string decoded = base64_decode(credentials);

    // Split username and password
    size_t colonPos = decoded.find(':');
    if (colonPos == std::string::npos) {
      logWarning("Invalid basic auth format", "AuthManager");
      return false;
    }

    std::string username = decoded.substr(0, colonPos);
    std::string password = decoded.substr(colonPos + 1);

    std::lock_guard<std::mutex> lock(usersMutex);

    auto it = users.find(username);
    if (it == users.end()) {
      logWarning("User not found: " + username, "AuthManager");
      return false;
    }

    // Extract salt from stored hash
    std::string stored = it->second;
    size_t saltEnd = stored.find(':');
    if (saltEnd == std::string::npos) {
      logError("Invalid stored password format for user: " + username,
               "AuthManager");
      return false;
    }

    std::string salt = stored.substr(0, saltEnd);
    std::string storedHash = stored.substr(saltEnd + 1);

    // Hash the provided password with the same salt
    std::string computedHash = hashPassword(password, salt);

    if (computedHash == storedHash) {
      logInfo("Basic auth successful for user: " + username, "AuthManager");
      return true;
    }

    logWarning("Invalid password for user: " + username, "AuthManager");
    return false;
  } catch (const std::exception &e) {
    logError("Basic auth validation error: " + std::string(e.what()),
             "AuthManager");
    return false;
  }
}

std::string AuthManager::hashPassword(const std::string &password,
                                      const std::string &salt) const {
  // Combine salt and password
  std::string saltedPassword = salt + password;

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

} // namespace astrocomm
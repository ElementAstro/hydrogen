#pragma once
#include <mutex>
#include <string>
#include <unordered_map>

namespace astrocomm {

class AuthManager {
public:
  AuthManager();
  ~AuthManager();

  // 用户认证
  bool authenticate(const std::string &method, const std::string &credentials);

  // 添加用户
  bool addUser(const std::string &username, const std::string &password);

  // 移除用户
  bool removeUser(const std::string &username);

  // 检查用户是否存在
  bool userExists(const std::string &username) const;

  // 保存用户配置到文件
  bool saveUserConfiguration(const std::string &filePath) const;

  // 从文件加载用户配置
  bool loadUserConfiguration(const std::string &filePath);

private:
  // 验证JWT令牌
  bool validateJwt(const std::string &token);

  // 验证基本认证
  bool validateBasicAuth(const std::string &credentials);

  // 计算密码哈希
  std::string hashPassword(const std::string &password,
                           const std::string &salt = "") const;

  std::mutex usersMutex;
  std::unordered_map<std::string, std::string>
      users; // username -> hashed_password
  std::string jwtSecret;
};

} // namespace astrocomm
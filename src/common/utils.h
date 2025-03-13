
#pragma once
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace astrocomm {

// 生成UUID
std::string generateUuid();

// 获取ISO 8601格式的当前时间戳
std::string getIsoTimestamp();

// 将整数解析为布尔值
bool parseBoolean(const std::string &value);

// 字符串处理工具
namespace string_utils {
// 去除字符串两端的空白字符
std::string trim(const std::string &str);

// 转换为小写
std::string toLower(const std::string &str);

// 转换为大写
std::string toUpper(const std::string &str);

// 分割字符串
std::vector<std::string> split(const std::string &str, char delimiter);
} // namespace string_utils

} // namespace astrocomm
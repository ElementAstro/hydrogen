#include "utils.h"

#include <algorithm>
#include <cctype>

namespace astrocomm {

std::string generateUuid() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 15);
  static const char *hex_chars = "0123456789abcdef";

  std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
  for (auto &c : uuid) {
    if (c == 'x')
      c = hex_chars[dis(gen)];
    else if (c == 'y')
      c = hex_chars[(dis(gen) & 0x3) | 0x8];
  }
  return uuid;
}

std::string getIsoTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto itt = std::chrono::system_clock::to_time_t(now);
  std::ostringstream ss;
  ss << std::put_time(gmtime(&itt), "%FT%T.") << std::setfill('0')
     << std::setw(3)
     << std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch())
                .count() %
            1000
     << "Z";
  return ss.str();
}

bool parseBoolean(const std::string &value) {
  std::string lowerValue = string_utils::toLower(value);
  return lowerValue == "true" || lowerValue == "yes" || lowerValue == "1" ||
         lowerValue == "on";
}

namespace string_utils {

std::string trim(const std::string &str) {
  auto start = std::find_if_not(str.begin(), str.end(),
                                [](int c) { return std::isspace(c); });

  auto end = std::find_if_not(str.rbegin(), str.rend(), [](int c) {
               return std::isspace(c);
             }).base();

  return (start < end) ? std::string(start, end) : std::string();
}

std::string toLower(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

std::string toUpper(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return result;
}

std::vector<std::string> split(const std::string &str, char delimiter) {
  std::vector<std::string> tokens;
  std::stringstream ss(str);
  std::string item;

  while (std::getline(ss, item, delimiter)) {
    tokens.push_back(item);
  }

  return tokens;
}

} // namespace string_utils

} // namespace astrocomm
#include "utils.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>

#ifdef USE_BOOST_REGEX
#include <boost/regex.hpp>
#else
#include <regex>
#endif

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

std::chrono::time_point<std::chrono::system_clock>
parseIsoTimestamp(const std::string &timestamp) {
#ifdef USE_BOOST_REGEX
  boost::regex iso8601_pattern(
      R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.(\d{1,3}))?Z)");
  boost::smatch matches;
  if (!boost::regex_match(timestamp, matches, iso8601_pattern)) {
    throw std::invalid_argument("Invalid ISO 8601 timestamp format");
  }
#else
  std::regex iso8601_pattern(
      R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.(\d{1,3}))?Z)");
  std::smatch matches;
  if (!std::regex_match(timestamp, matches, iso8601_pattern)) {
    throw std::invalid_argument("Invalid ISO 8601 timestamp format");
  }
#endif

  std::tm tm = {};
  std::stringstream ss(timestamp.substr(0, 19));
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

  if (ss.fail()) {
    throw std::invalid_argument("Failed to parse timestamp");
  }

  auto timepoint = std::chrono::system_clock::from_time_t(std::mktime(&tm));

  if (matches.size() > 1 && matches[1].matched) {
    std::string ms_str = matches[1].str();
    ms_str.resize(3, '0');

    int milliseconds = std::stoi(ms_str);
    timepoint += std::chrono::milliseconds(milliseconds);
  }

  std::time_t local_time = std::mktime(&tm);
  std::time_t gm_time = std::mktime(std::gmtime(&local_time));
  auto time_difference = std::difftime(local_time, gm_time);

  timepoint -= std::chrono::seconds(static_cast<long>(time_difference));

  return timepoint;
}

} // namespace string_utils

} // namespace astrocomm
#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace hydrogen {
namespace core {

/**
 * @brief Generates a universally unique identifier (UUID)
 *
 * @return std::string The generated UUID string
 */
std::string generateUuid();

/**
 * @brief Gets the current timestamp in ISO 8601 format
 *
 * @return std::string Current time as an ISO 8601 formatted string
 * (YYYY-MM-DDThh:mm:ss.sssZ)
 */
std::string getIsoTimestamp();

/**
 * @brief Parses a string value into a boolean
 *
 * @param value The string to parse
 * @return bool The parsed boolean value
 */
bool parseBoolean(const std::string &value);

/**
 * @brief Namespace containing string manipulation utilities
 */
namespace string_utils {

/**
 * @brief Removes whitespace characters from both ends of a string
 *
 * @param str The input string to trim
 * @return std::string The trimmed string
 */
std::string trim(const std::string &str);

/**
 * @brief Converts a string to lowercase
 *
 * @param str The input string to convert
 * @return std::string The lowercase version of the input string
 */
std::string toLower(const std::string &str);

/**
 * @brief Converts a string to uppercase
 *
 * @param str The input string to convert
 * @return std::string The uppercase version of the input string
 */
std::string toUpper(const std::string &str);

/**
 * @brief Splits a string into substrings based on a delimiter
 *
 * @param str The input string to split
 * @param delimiter The character used as separator
 * @return std::vector<std::string> Vector containing the substrings
 */
std::vector<std::string> split(const std::string &str, char delimiter);

/**
 * @brief Parses an ISO 8601 formatted timestamp string into a time_point
 *
 * @param timestamp The ISO 8601 formatted string to parse
 * @return std::chrono::time_point<std::chrono::system_clock> The parsed
 * timestamp as a time_point
 */
std::chrono::time_point<std::chrono::system_clock>
parseIsoTimestamp(const std::string &timestamp);

} // namespace string_utils

} // namespace core
} // namespace hydrogen

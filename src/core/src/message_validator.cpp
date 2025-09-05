#include "hydrogen/core/message_validator.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace hydrogen {
namespace core {

// BaseMessageValidator implementation
BaseMessageValidator::BaseMessageValidator(const SecurityRules &rules)
    : securityRules_(rules) {}

ValidationResult BaseMessageValidator::validate(const Message &message) const {
  ValidationResult result;
  result.isValid = true;

  // Validate basic message fields
  validateMessageId(message.getMessageId(), result);
  validateDeviceId(message.getDeviceId(), result);
  validateTimestamp(message.getTimestamp(), result);
  validateMessageType(message.getMessageType(), result);
  validatePriority(message.getPriority(), result);
  validateQoSLevel(message.getQoSLevel(), result);

  // Validate message content for security issues
  json messageJson = message.toJson();
  checkForMaliciousContent(messageJson.dump(), result);
  validateJsonStructure(messageJson, 0, result);

  // Sanitize the message data
  result.sanitizedData = sanitize(messageJson);

  return result;
}

ValidationResult
BaseMessageValidator::validateJson(const json &messageJson) const {
  ValidationResult result;
  result.isValid = true;

  // Check required fields
  if (!messageJson.contains("messageType")) {
    result.addError("Missing required field: messageType");
  }
  if (!messageJson.contains("messageId")) {
    result.addError("Missing required field: messageId");
  }
  if (!messageJson.contains("timestamp")) {
    result.addError("Missing required field: timestamp");
  }

  // Validate field values if present
  if (messageJson.contains("messageId")) {
    validateMessageId(messageJson["messageId"], result);
  }
  if (messageJson.contains("deviceId")) {
    validateDeviceId(messageJson["deviceId"], result);
  }
  if (messageJson.contains("timestamp")) {
    validateTimestamp(messageJson["timestamp"], result);
  }

  // Security validation
  checkForMaliciousContent(messageJson.dump(), result);
  validateJsonStructure(messageJson, 0, result);

  // Sanitize the data
  result.sanitizedData = sanitize(messageJson);

  return result;
}

json BaseMessageValidator::sanitize(const json &input) const {
  return sanitizeJsonValue(input, 0);
}

bool BaseMessageValidator::validateMessageId(const std::string &messageId,
                                             ValidationResult &result) const {
  if (messageId.empty()) {
    result.addError("Message ID cannot be empty");
    return false;
  }

  if (messageId.length() > securityRules_.maxStringLength) {
    result.addError("Message ID exceeds maximum length");
    return false;
  }

  // Check for valid UUID format or acceptable ID pattern
  std::regex uuidRegex(
      R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
  std::regex msgIdRegex(R"(msg_[0-9]+)");

  if (!std::regex_match(messageId, uuidRegex) &&
      !std::regex_match(messageId, msgIdRegex)) {
    result.addWarning("Message ID format may not be standard");
  }

  return true;
}

bool BaseMessageValidator::validateDeviceId(const std::string &deviceId,
                                            ValidationResult &result) const {
  if (deviceId.empty()) {
    // Device ID can be empty for some message types
    return true;
  }

  if (deviceId.length() > securityRules_.maxStringLength) {
    result.addError("Device ID exceeds maximum length");
    return false;
  }

  // Check for malicious patterns
  if (checkForPathTraversal(deviceId, result) ||
      checkForSqlInjection(deviceId, result) ||
      checkForXssAttempts(deviceId, result)) {
    result.addError("Device ID contains potentially malicious content");
    return false;
  }

  return true;
}

bool BaseMessageValidator::validateTimestamp(const std::string &timestamp,
                                             ValidationResult &result) const {
  if (timestamp.empty()) {
    result.addError("Timestamp cannot be empty");
    return false;
  }

  // Try to parse ISO 8601 format
  std::regex isoRegex(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d{3})?Z?)");
  if (!std::regex_match(timestamp, isoRegex)) {
    result.addWarning("Timestamp format may not be ISO 8601 compliant");
  }

  return true;
}

bool BaseMessageValidator::validateMessageType(MessageType type,
                                               ValidationResult &result) const {
  // All defined message types are valid
  return true;
}

bool BaseMessageValidator::validatePriority(Message::Priority priority,
                                            ValidationResult &result) const {
  // All defined priority levels are valid
  return true;
}

bool BaseMessageValidator::validateQoSLevel(Message::QoSLevel qos,
                                            ValidationResult &result) const {
  // All defined QoS levels are valid
  return true;
}

bool BaseMessageValidator::checkForMaliciousContent(
    const std::string &content, ValidationResult &result) const {
  bool hasMaliciousContent = false;

  if (!securityRules_.allowScriptTags && checkForXssAttempts(content, result)) {
    hasMaliciousContent = true;
  }

  if (!securityRules_.allowSqlKeywords &&
      checkForSqlInjection(content, result)) {
    hasMaliciousContent = true;
  }

  if (!securityRules_.allowPathTraversal &&
      checkForPathTraversal(content, result)) {
    hasMaliciousContent = true;
  }

  // Check for blocked patterns
  for (const auto &pattern : securityRules_.blockedPatterns) {
    if (content.find(pattern) != std::string::npos) {
      result.addError("Content contains blocked pattern: " + pattern);
      hasMaliciousContent = true;
    }
  }

  return hasMaliciousContent;
}

bool BaseMessageValidator::checkForSqlInjection(
    const std::string &content, ValidationResult &result) const {
  std::vector<std::string> sqlKeywords = {
      "SELECT", "INSERT", "UPDATE", "DELETE", "DROP",
      "CREATE", "ALTER",  "UNION",  "OR",     "AND",
      "WHERE",  "FROM",   "JOIN",   "EXEC",   "EXECUTE"};

  std::string upperContent = content;
  std::transform(upperContent.begin(), upperContent.end(), upperContent.begin(),
                 ::toupper);

  for (const auto &keyword : sqlKeywords) {
    if (upperContent.find(keyword) != std::string::npos) {
      result.addWarning("Content contains SQL keyword: " + keyword);
      return true;
    }
  }

  return false;
}

bool BaseMessageValidator::checkForXssAttempts(const std::string &content,
                                               ValidationResult &result) const {
  std::vector<std::string> xssPatterns = {
      "<script",  "</script>",    "javascript:", "onload=", "onerror=",
      "onclick=", "onmouseover=", "eval(",       "alert(",  "document.cookie"};

  std::string lowerContent = content;
  std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(),
                 ::tolower);

  for (const auto &pattern : xssPatterns) {
    if (lowerContent.find(pattern) != std::string::npos) {
      result.addWarning("Content contains potential XSS pattern: " + pattern);
      return true;
    }
  }

  return false;
}

bool BaseMessageValidator::checkForPathTraversal(
    const std::string &content, ValidationResult &result) const {
  std::vector<std::string> traversalPatterns = {
      "../", "..\\", "..%2f", "..%5c", "%2e%2e%2f", "%2e%2e%5c"};

  std::string lowerContent = content;
  std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(),
                 ::tolower);

  for (const auto &pattern : traversalPatterns) {
    if (lowerContent.find(pattern) != std::string::npos) {
      result.addWarning("Content contains path traversal pattern: " + pattern);
      return true;
    }
  }

  return false;
}

bool BaseMessageValidator::validateJsonStructure(
    const json &data, size_t currentDepth, ValidationResult &result) const {
  if (currentDepth > securityRules_.maxObjectDepth) {
    result.addError("JSON structure exceeds maximum depth limit");
    return false;
  }

  if (data.is_object()) {
    if (data.size() > securityRules_.maxArraySize) {
      result.addError("JSON object exceeds maximum size limit");
      return false;
    }

    for (const auto &[key, value] : data.items()) {
      if (key.length() > securityRules_.maxStringLength) {
        result.addError("JSON key exceeds maximum length");
        return false;
      }

      if (!validateJsonStructure(value, currentDepth + 1, result)) {
        return false;
      }
    }
  } else if (data.is_array()) {
    if (data.size() > securityRules_.maxArraySize) {
      result.addError("JSON array exceeds maximum size limit");
      return false;
    }

    for (const auto &item : data) {
      if (!validateJsonStructure(item, currentDepth + 1, result)) {
        return false;
      }
    }
  } else if (data.is_string()) {
    std::string str = data.get<std::string>();
    if (str.length() > securityRules_.maxStringLength) {
      result.addError("JSON string exceeds maximum length");
      return false;
    }
  }

  return true;
}

std::string
BaseMessageValidator::sanitizeString(const std::string &input) const {
  std::string result = input;

  // Remove HTML tags if not allowed
  if (!securityRules_.allowScriptTags) {
    result = removeHtmlTags(result);
  }

  // Escape SQL characters if not allowed
  if (!securityRules_.allowSqlKeywords) {
    result = escapeSqlCharacters(result);
  }

  // Truncate if too long
  if (result.length() > securityRules_.maxStringLength) {
    result = result.substr(0, securityRules_.maxStringLength);
  }

  return result;
}

json BaseMessageValidator::sanitizeJsonValue(const json &value,
                                             size_t currentDepth) const {
  if (currentDepth > securityRules_.maxObjectDepth) {
    return json{}; // Return empty object if depth exceeded
  }

  if (value.is_string()) {
    return sanitizeString(value.get<std::string>());
  } else if (value.is_object()) {
    json sanitized = json::object();
    size_t count = 0;
    for (const auto &[key, val] : value.items()) {
      if (count >= securityRules_.maxArraySize)
        break;
      std::string sanitizedKey = sanitizeString(key);
      sanitized[sanitizedKey] = sanitizeJsonValue(val, currentDepth + 1);
      count++;
    }
    return sanitized;
  } else if (value.is_array()) {
    json sanitized = json::array();
    size_t count = 0;
    for (const auto &item : value) {
      if (count >= securityRules_.maxArraySize)
        break;
      sanitized.push_back(sanitizeJsonValue(item, currentDepth + 1));
      count++;
    }
    return sanitized;
  }

  return value; // Return as-is for numbers, booleans, null
}

std::string
BaseMessageValidator::removeHtmlTags(const std::string &input) const {
  std::regex htmlRegex("<[^>]*>");
  return std::regex_replace(input, htmlRegex, "");
}

std::string
BaseMessageValidator::escapeSqlCharacters(const std::string &input) const {
  std::string result = input;

  // Replace single quotes with double single quotes
  size_t pos = 0;
  while ((pos = result.find("'", pos)) != std::string::npos) {
    result.replace(pos, 1, "''");
    pos += 2;
  }

  return result;
}

// CommandMessageValidator implementation
CommandMessageValidator::CommandMessageValidator(const SecurityRules &rules)
    : BaseMessageValidator(rules) {
  // Initialize allowed commands (can be configured)
  allowedCommands_ = {"connect",        "disconnect",     "get_properties",
                      "set_properties", "execute",        "abort",
                      "park",           "unpark",         "home",
                      "calibrate",      "start_exposure", "stop_exposure",
                      "get_status",     "reset"};

  // Initialize dangerous commands that require extra validation
  dangerousCommands_ = {"reset", "calibrate", "execute", "abort"};
}

ValidationResult
CommandMessageValidator::validate(const Message &message) const {
  ValidationResult result = BaseMessageValidator::validate(message);

  if (message.getMessageType() != MessageType::COMMAND) {
    result.addError("Message type must be COMMAND for CommandMessageValidator");
    return result;
  }

  const CommandMessage &cmd = static_cast<const CommandMessage &>(message);

  validateCommand(cmd.getCommand(), result);
  validateParameters(cmd.getParameters(), result);
  validateProperties(cmd.getProperties(), result);

  return result;
}

ValidationResult
CommandMessageValidator::validateJson(const json &messageJson) const {
  ValidationResult result = BaseMessageValidator::validateJson(messageJson);

  if (messageJson.contains("command")) {
    validateCommand(messageJson["command"], result);
  }
  if (messageJson.contains("parameters")) {
    validateParameters(messageJson["parameters"], result);
  }
  if (messageJson.contains("properties")) {
    validateProperties(messageJson["properties"], result);
  }

  return result;
}

bool CommandMessageValidator::validateCommand(const std::string &command,
                                              ValidationResult &result) const {
  if (command.empty()) {
    result.addError("Command cannot be empty");
    return false;
  }

  if (command.length() > 100) { // Reasonable command length limit
    result.addError("Command name too long");
    return false;
  }

  // Check if command is in allowed list
  if (allowedCommands_.find(command) == allowedCommands_.end()) {
    result.addWarning("Command not in allowed list: " + command);
  }

  // Extra validation for dangerous commands
  if (dangerousCommands_.find(command) != dangerousCommands_.end()) {
    result.addWarning("Dangerous command detected: " + command);
  }

  return true;
}

bool CommandMessageValidator::validateParameters(
    const json &parameters, ValidationResult &result) const {
  if (parameters.is_null()) {
    return true; // Null parameters are allowed
  }

  return validateJsonStructure(parameters, 0, result);
}

bool CommandMessageValidator::validateProperties(
    const json &properties, ValidationResult &result) const {
  if (properties.is_null()) {
    return true; // Null properties are allowed
  }

  return validateJsonStructure(properties, 0, result);
}

// ResponseMessageValidator implementation
ResponseMessageValidator::ResponseMessageValidator(const SecurityRules &rules)
    : BaseMessageValidator(rules) {
  validStatuses_ = {"success", "error",     "pending",
                    "timeout", "cancelled", "partial"};
}

ValidationResult
ResponseMessageValidator::validate(const Message &message) const {
  ValidationResult result = BaseMessageValidator::validate(message);

  if (message.getMessageType() != MessageType::RESPONSE) {
    result.addError(
        "Message type must be RESPONSE for ResponseMessageValidator");
    return result;
  }

  const ResponseMessage &resp = static_cast<const ResponseMessage &>(message);

  validateStatus(resp.getStatus(), result);
  validateResponseData(resp.getDetails(), result);

  return result;
}

ValidationResult
ResponseMessageValidator::validateJson(const json &messageJson) const {
  ValidationResult result = BaseMessageValidator::validateJson(messageJson);

  if (messageJson.contains("status")) {
    validateStatus(messageJson["status"], result);
  }
  if (messageJson.contains("details")) {
    validateResponseData(messageJson["details"], result);
  }

  return result;
}

bool ResponseMessageValidator::validateStatus(const std::string &status,
                                              ValidationResult &result) const {
  if (status.empty()) {
    result.addError("Response status cannot be empty");
    return false;
  }

  if (validStatuses_.find(status) == validStatuses_.end()) {
    result.addWarning("Unknown response status: " + status);
  }

  return true;
}

bool ResponseMessageValidator::validateResponseData(
    const json &data, ValidationResult &result) const {
  if (data.is_null()) {
    return true; // Null data is allowed
  }

  return validateJsonStructure(data, 0, result);
}

// MessageValidatorFactory implementation
std::unique_ptr<MessageValidatorInterface>
MessageValidatorFactory::createValidator(MessageType type,
                                         const SecurityRules &rules) {
  switch (type) {
  case MessageType::COMMAND:
    return createCommandValidator(rules);
  case MessageType::RESPONSE:
    return createResponseValidator(rules);
  case MessageType::EVENT:
    return createEventValidator(rules);
  case MessageType::ERR:
    return createErrorValidator(rules);
  default:
    return createBaseValidator(rules);
  }
}

std::unique_ptr<BaseMessageValidator>
MessageValidatorFactory::createBaseValidator(const SecurityRules &rules) {
  return std::make_unique<BaseMessageValidator>(rules);
}

std::unique_ptr<CommandMessageValidator>
MessageValidatorFactory::createCommandValidator(const SecurityRules &rules) {
  return std::make_unique<CommandMessageValidator>(rules);
}

std::unique_ptr<ResponseMessageValidator>
MessageValidatorFactory::createResponseValidator(const SecurityRules &rules) {
  return std::make_unique<ResponseMessageValidator>(rules);
}

std::unique_ptr<EventMessageValidator>
MessageValidatorFactory::createEventValidator(const SecurityRules &rules) {
  return std::make_unique<EventMessageValidator>(rules);
}

std::unique_ptr<ErrorMessageValidator>
MessageValidatorFactory::createErrorValidator(const SecurityRules &rules) {
  return std::make_unique<ErrorMessageValidator>(rules);
}

// MessageSanitizer implementation
MessageSanitizer::MessageSanitizer(const SecurityRules &rules)
    : securityRules_(rules) {
  initializeRegexPatterns();
}

std::unique_ptr<Message>
MessageSanitizer::sanitizeMessage(const Message &message) const {
  json sanitizedJson = sanitizeJson(message.toJson());
  return createMessageFromJson(sanitizedJson);
}

json MessageSanitizer::sanitizeJson(const json &input) const {
  BaseMessageValidator validator(securityRules_);
  return validator.sanitize(input);
}

void MessageSanitizer::initializeRegexPatterns() {
  htmlTagRegex_ = std::regex("<[^>]*>");
  sqlKeywordRegex_ = std::regex(
      R"(\b(SELECT|INSERT|UPDATE|DELETE|DROP|CREATE|ALTER|UNION|OR|AND|WHERE|FROM|JOIN|EXEC|EXECUTE)\b)",
      std::regex_constants::icase);
  pathTraversalRegex_ = std::regex(R"(\.\.[/\\])");
  scriptTagRegex_ =
      std::regex(R"(<script[^>]*>.*?</script>)", std::regex_constants::icase);
}

// Global instance
MessageSanitizer &getGlobalMessageSanitizer() {
  static MessageSanitizer instance;
  return instance;
}

} // namespace core
} // namespace hydrogen

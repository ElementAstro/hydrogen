#pragma once

#include "message.h"
#include <functional>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Validation result structure
 */
struct ValidationResult {
  bool isValid = false;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  json sanitizedData;

  void addError(const std::string &error) {
    errors.push_back(error);
    isValid = false;
  }

  void addWarning(const std::string &warning) { warnings.push_back(warning); }

  bool hasErrors() const { return !errors.empty(); }
  bool hasWarnings() const { return !warnings.empty(); }

  std::string getErrorSummary() const {
    if (errors.empty())
      return "";
    std::string summary = "Validation errors: ";
    for (size_t i = 0; i < errors.size(); ++i) {
      if (i > 0)
        summary += "; ";
      summary += errors[i];
    }
    return summary;
  }
};

/**
 * @brief Security validation rules
 */
struct SecurityRules {
  size_t maxMessageSize = 1024 * 1024; // 1MB default
  size_t maxStringLength = 10000;
  size_t maxArraySize = 1000;
  size_t maxObjectDepth = 10;
  bool allowScriptTags = false;
  bool allowSqlKeywords = false;
  bool allowPathTraversal = false;
  std::unordered_set<std::string> blockedPatterns;
  std::unordered_set<std::string> allowedDeviceIdPatterns;
};

/**
 * @brief Message validation interface
 */
class MessageValidatorInterface {
public:
  virtual ~MessageValidatorInterface() = default;
  virtual ValidationResult validate(const Message &message) const = 0;
  virtual ValidationResult validateJson(const json &messageJson) const = 0;
  virtual json sanitize(const json &input) const = 0;
};

/**
 * @brief Base message validator with common validation logic
 */
class BaseMessageValidator : public MessageValidatorInterface {
public:
  explicit BaseMessageValidator(const SecurityRules &rules = SecurityRules{});

  ValidationResult validate(const Message &message) const override;
  ValidationResult validateJson(const json &messageJson) const override;
  json sanitize(const json &input) const override;

protected:
  SecurityRules securityRules_;

  // Common validation methods
  bool validateMessageId(const std::string &messageId,
                         ValidationResult &result) const;
  bool validateDeviceId(const std::string &deviceId,
                        ValidationResult &result) const;
  bool validateTimestamp(const std::string &timestamp,
                         ValidationResult &result) const;
  bool validateMessageType(MessageType type, ValidationResult &result) const;
  bool validatePriority(Message::Priority priority,
                        ValidationResult &result) const;
  bool validateQoSLevel(Message::QoSLevel qos, ValidationResult &result) const;

  // Security validation methods
  bool checkForMaliciousContent(const std::string &content,
                                ValidationResult &result) const;
  bool checkForSqlInjection(const std::string &content,
                            ValidationResult &result) const;
  bool checkForXssAttempts(const std::string &content,
                           ValidationResult &result) const;
  bool checkForPathTraversal(const std::string &content,
                             ValidationResult &result) const;
  bool validateJsonStructure(const json &data, size_t currentDepth,
                             ValidationResult &result) const;

  // Sanitization methods
  std::string sanitizeString(const std::string &input) const;
  json sanitizeJsonValue(const json &value, size_t currentDepth = 0) const;
  std::string removeHtmlTags(const std::string &input) const;
  std::string escapeSqlCharacters(const std::string &input) const;
};

/**
 * @brief Command message validator
 */
class CommandMessageValidator : public BaseMessageValidator {
public:
  explicit CommandMessageValidator(
      const SecurityRules &rules = SecurityRules{});

  ValidationResult validate(const Message &message) const override;
  ValidationResult validateJson(const json &messageJson) const override;

private:
  bool validateCommand(const std::string &command,
                       ValidationResult &result) const;
  bool validateParameters(const json &parameters,
                          ValidationResult &result) const;
  bool validateProperties(const json &properties,
                          ValidationResult &result) const;

  std::unordered_set<std::string> allowedCommands_;
  std::unordered_set<std::string> dangerousCommands_;
};

/**
 * @brief Response message validator
 */
class ResponseMessageValidator : public BaseMessageValidator {
public:
  explicit ResponseMessageValidator(
      const SecurityRules &rules = SecurityRules{});

  ValidationResult validate(const Message &message) const override;
  ValidationResult validateJson(const json &messageJson) const override;

private:
  bool validateStatus(const std::string &status,
                      ValidationResult &result) const;
  bool validateResponseData(const json &data, ValidationResult &result) const;

  std::unordered_set<std::string> validStatuses_;
};

/**
 * @brief Event message validator
 */
class EventMessageValidator : public BaseMessageValidator {
public:
  explicit EventMessageValidator(const SecurityRules &rules = SecurityRules{});

  ValidationResult validate(const Message &message) const override;
  ValidationResult validateJson(const json &messageJson) const override;

private:
  bool validateEventName(const std::string &eventName,
                         ValidationResult &result) const;
  bool validateEventData(const json &data, ValidationResult &result) const;

  std::unordered_set<std::string> allowedEventTypes_;
};

/**
 * @brief Error message validator
 */
class ErrorMessageValidator : public BaseMessageValidator {
public:
  explicit ErrorMessageValidator(const SecurityRules &rules = SecurityRules{});

  ValidationResult validate(const Message &message) const override;
  ValidationResult validateJson(const json &messageJson) const override;

private:
  bool validateErrorCode(int errorCode, ValidationResult &result) const;
  bool validateErrorMessage(const std::string &errorMessage,
                            ValidationResult &result) const;
};

/**
 * @brief Protocol-specific validator factory
 */
class MessageValidatorFactory {
public:
  static std::unique_ptr<MessageValidatorInterface>
  createValidator(MessageType type,
                  const SecurityRules &rules = SecurityRules{});
  static std::unique_ptr<BaseMessageValidator>
  createBaseValidator(const SecurityRules &rules = SecurityRules{});
  static std::unique_ptr<CommandMessageValidator>
  createCommandValidator(const SecurityRules &rules = SecurityRules{});
  static std::unique_ptr<ResponseMessageValidator>
  createResponseValidator(const SecurityRules &rules = SecurityRules{});
  static std::unique_ptr<EventMessageValidator>
  createEventValidator(const SecurityRules &rules = SecurityRules{});
  static std::unique_ptr<ErrorMessageValidator>
  createErrorValidator(const SecurityRules &rules = SecurityRules{});
};

/**
 * @brief Comprehensive message sanitizer
 */
class MessageSanitizer {
public:
  explicit MessageSanitizer(const SecurityRules &rules = SecurityRules{});

  // Sanitize complete message
  std::unique_ptr<Message> sanitizeMessage(const Message &message) const;

  // Sanitize JSON data
  json sanitizeJson(const json &input) const;

  // Sanitize specific data types
  std::string sanitizeDeviceId(const std::string &deviceId) const;
  std::string sanitizeCommand(const std::string &command) const;
  json sanitizeParameters(const json &parameters) const;

  // Update security rules
  void updateSecurityRules(const SecurityRules &rules);
  const SecurityRules &getSecurityRules() const;

private:
  SecurityRules securityRules_;
  std::regex htmlTagRegex_;
  std::regex sqlKeywordRegex_;
  std::regex pathTraversalRegex_;
  std::regex scriptTagRegex_;

  void initializeRegexPatterns();
};

/**
 * @brief Global message validator instance
 */
MessageSanitizer &getGlobalMessageSanitizer();

} // namespace core
} // namespace hydrogen

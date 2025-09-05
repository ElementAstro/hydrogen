#pragma once

#include "message.h"
#include "message_transformer.h"
#include <exception>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief Protocol-specific error codes and formats
 */
enum class ProtocolErrorCode {
  // Generic errors
  SUCCESS = 0,
  UNKNOWN_ERROR = 1000,
  INTERNAL_ERROR = 1001,
  INVALID_REQUEST = 1002,
  INVALID_PARAMETERS = 1003,
  OPERATION_FAILED = 1004,

  // Connection errors
  CONNECTION_FAILED = 2000,
  CONNECTION_LOST = 2001,
  CONNECTION_TIMEOUT = 2002,
  AUTHENTICATION_FAILED = 2003,
  AUTHORIZATION_FAILED = 2004,

  // Protocol errors
  PROTOCOL_ERROR = 3000,
  UNSUPPORTED_OPERATION = 3001,
  MESSAGE_FORMAT_ERROR = 3002,
  PROTOCOL_VERSION_MISMATCH = 3003,

  // Device errors
  DEVICE_NOT_FOUND = 4000,
  DEVICE_BUSY = 4001,
  DEVICE_ERROR = 4002,
  DEVICE_DISCONNECTED = 4003,
  DEVICE_TIMEOUT = 4004,

  // Resource errors
  RESOURCE_UNAVAILABLE = 5000,
  RESOURCE_EXHAUSTED = 5001,
  QUOTA_EXCEEDED = 5002,

  // Validation errors
  VALIDATION_ERROR = 6000,
  MISSING_REQUIRED_FIELD = 6001,
  INVALID_FIELD_VALUE = 6002,
  FIELD_OUT_OF_RANGE = 6003
};

/**
 * @brief Protocol error information
 */
struct ProtocolError {
  ProtocolErrorCode code;
  std::string message;
  std::string details;
  std::string component;
  std::string operation;
  json metadata;
  std::chrono::system_clock::time_point timestamp;

  // Protocol-specific formatting
  json toJson() const;
  std::string toString() const;
};

/**
 * @brief Protocol-specific error formatter interface
 */
class ProtocolErrorFormatter {
public:
  virtual ~ProtocolErrorFormatter() = default;

  // Format error for specific protocol
  virtual json formatError(const ProtocolError &error) const = 0;
  virtual std::string getErrorMessage(const ProtocolError &error) const = 0;
  virtual int getProtocolErrorCode(ProtocolErrorCode code) const = 0;
  virtual std::string getProtocolName() const = 0;
};

/**
 * @brief HTTP/WebSocket error formatter
 */
class HttpErrorFormatter : public ProtocolErrorFormatter {
public:
  json formatError(const ProtocolError &error) const override;
  std::string getErrorMessage(const ProtocolError &error) const override;
  int getProtocolErrorCode(ProtocolErrorCode code) const override;
  std::string getProtocolName() const override { return "HTTP/WebSocket"; }

private:
  int mapToHttpStatusCode(ProtocolErrorCode code) const;
};

/**
 * @brief gRPC error formatter
 */
class GrpcErrorFormatter : public ProtocolErrorFormatter {
public:
  json formatError(const ProtocolError &error) const override;
  std::string getErrorMessage(const ProtocolError &error) const override;
  int getProtocolErrorCode(ProtocolErrorCode code) const override;
  std::string getProtocolName() const override { return "gRPC"; }

private:
  int mapToGrpcStatusCode(ProtocolErrorCode code) const;
};

/**
 * @brief MQTT error formatter
 */
class MqttErrorFormatter : public ProtocolErrorFormatter {
public:
  json formatError(const ProtocolError &error) const override;
  std::string getErrorMessage(const ProtocolError &error) const override;
  int getProtocolErrorCode(ProtocolErrorCode code) const override;
  std::string getProtocolName() const override { return "MQTT"; }

private:
  int mapToMqttReasonCode(ProtocolErrorCode code) const;
};

/**
 * @brief ZeroMQ error formatter
 */
class ZeroMqErrorFormatter : public ProtocolErrorFormatter {
public:
  json formatError(const ProtocolError &error) const override;
  std::string getErrorMessage(const ProtocolError &error) const override;
  int getProtocolErrorCode(ProtocolErrorCode code) const override;
  std::string getProtocolName() const override { return "ZeroMQ"; }
};

/**
 * @brief ASCOM error formatter
 */
class AscomErrorFormatter : public ProtocolErrorFormatter {
public:
  json formatError(const ProtocolError &error) const override;
  std::string getErrorMessage(const ProtocolError &error) const override;
  int getProtocolErrorCode(ProtocolErrorCode code) const override;
  std::string getProtocolName() const override { return "ASCOM"; }

private:
  int mapToAscomErrorCode(ProtocolErrorCode code) const;
  std::string getAscomErrorSource(const ProtocolError &error) const;
};

/**
 * @brief INDI error formatter
 */
class IndiErrorFormatter : public ProtocolErrorFormatter {
public:
  json formatError(const ProtocolError &error) const override;
  std::string getErrorMessage(const ProtocolError &error) const override;
  int getProtocolErrorCode(ProtocolErrorCode code) const override;
  std::string getProtocolName() const override { return "INDI"; }

private:
  std::string mapToIndiState(ProtocolErrorCode code) const;
};

/**
 * @brief Protocol error mapper - converts exceptions to protocol-specific
 * errors
 */
class ProtocolErrorMapper {
public:
  using ExceptionHandler = std::function<ProtocolError(const std::exception &)>;

  ProtocolErrorMapper();
  ~ProtocolErrorMapper() = default;

  // Register formatters
  void registerFormatter(MessageFormat format,
                         std::unique_ptr<ProtocolErrorFormatter> formatter);

  // Register exception handlers
  void registerExceptionHandler(const std::string &exceptionType,
                                ExceptionHandler handler);

  // Error mapping
  ProtocolError mapException(const std::exception &ex,
                             const std::string &component = "",
                             const std::string &operation = "") const;
  ProtocolError createError(ProtocolErrorCode code, const std::string &message,
                            const std::string &details = "") const;

  // Protocol-specific formatting
  json formatErrorForProtocol(const ProtocolError &error,
                              MessageFormat format) const;
  std::unique_ptr<ErrorMessage>
  createErrorMessage(const ProtocolError &error,
                     const std::string &originalMessageId = "") const;

  // Utility methods
  bool hasFormatter(MessageFormat format) const;
  std::vector<MessageFormat> getSupportedFormats() const;

  // Error code utilities
  static std::string getErrorCodeName(ProtocolErrorCode code);
  static ProtocolErrorCode parseErrorCode(const std::string &codeName);
  static bool isRecoverableError(ProtocolErrorCode code);

private:
  std::unordered_map<MessageFormat, std::unique_ptr<ProtocolErrorFormatter>>
      formatters_;
  std::unordered_map<std::string, ExceptionHandler> exceptionHandlers_;

  void initializeDefaultFormatters();
  void initializeDefaultExceptionHandlers();

  ProtocolError mapStandardException(const std::exception &ex) const;
  std::string getExceptionTypeName(const std::exception &ex) const;
};

/**
 * @brief Exception-to-error mapping utilities
 */
class ErrorMappingUtils {
public:
  // Standard exception mappers
  static ProtocolError mapStdException(const std::exception &ex);
  static ProtocolError mapRuntimeError(const std::runtime_error &ex);
  static ProtocolError mapLogicError(const std::logic_error &ex);
  static ProtocolError mapInvalidArgument(const std::invalid_argument &ex);
  static ProtocolError mapOutOfRange(const std::out_of_range &ex);
  static ProtocolError mapSystemError(const std::system_error &ex);

  // Custom exception mappers
  static ProtocolError mapDeviceException(const std::exception &ex);
  static ProtocolError mapConnectionException(const std::exception &ex);
  static ProtocolError mapProtocolException(const std::exception &ex);
  static ProtocolError mapValidationException(const std::exception &ex);

  // Error severity assessment
  static bool isCriticalError(ProtocolErrorCode code);
  static bool requiresReconnection(ProtocolErrorCode code);
  static bool shouldRetry(ProtocolErrorCode code);
};

/**
 * @brief Global protocol error mapper instance
 */
ProtocolErrorMapper &getGlobalProtocolErrorMapper();

/**
 * @brief Convenience macros for error handling
 */
#define HANDLE_PROTOCOL_ERROR(ex, component, operation, format)                \
  do {                                                                         \
    auto error =                                                               \
        getGlobalProtocolErrorMapper().mapException(ex, component, operation); \
    auto formattedError =                                                      \
        getGlobalProtocolErrorMapper().formatErrorForProtocol(error, format);  \
    spdlog::error("Protocol error in {}: {}", component, error.toString());    \
  } while (0)

#define CREATE_PROTOCOL_ERROR_MESSAGE(code, message, details, originalId)      \
  getGlobalProtocolErrorMapper().createErrorMessage(                           \
      getGlobalProtocolErrorMapper().createError(code, message, details),      \
      originalId)

} // namespace core
} // namespace hydrogen

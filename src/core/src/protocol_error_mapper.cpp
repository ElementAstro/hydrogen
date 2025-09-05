#include "hydrogen/core/protocol_error_mapper.h"
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif
#include <typeinfo>
#ifdef __GNUC__
#include <cxxabi.h>
#endif
#include <memory>

namespace hydrogen {
namespace core {

// ProtocolError implementation
json ProtocolError::toJson() const {
  json j;
  j["code"] = static_cast<int>(code);
  j["message"] = message;
  j["details"] = details;
  j["component"] = component;
  j["operation"] = operation;
  j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                       timestamp.time_since_epoch())
                       .count();
  j["metadata"] = metadata;
  return j;
}

std::string ProtocolError::toString() const {
  return "ProtocolError[" + std::to_string(static_cast<int>(code)) +
         "]: " + message + (details.empty() ? "" : " (" + details + ")");
}

// HttpErrorFormatter implementation
json HttpErrorFormatter::formatError(const ProtocolError &error) const {
  json result;
  result["error"] = {
      {"code", getProtocolErrorCode(error.code)},
      {"message", getErrorMessage(error)},
      {"details", error.details},
      {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        error.timestamp.time_since_epoch())
                        .count()}};

  if (!error.metadata.empty()) {
    result["error"]["metadata"] = error.metadata;
  }

  return result;
}

std::string
HttpErrorFormatter::getErrorMessage(const ProtocolError &error) const {
  return error.message;
}

int HttpErrorFormatter::getProtocolErrorCode(ProtocolErrorCode code) const {
  return mapToHttpStatusCode(code);
}

int HttpErrorFormatter::mapToHttpStatusCode(ProtocolErrorCode code) const {
  switch (code) {
  case ProtocolErrorCode::SUCCESS:
    return 200; // OK
  case ProtocolErrorCode::INVALID_REQUEST:
  case ProtocolErrorCode::INVALID_PARAMETERS:
  case ProtocolErrorCode::MESSAGE_FORMAT_ERROR:
    return 400; // Bad Request
  case ProtocolErrorCode::AUTHENTICATION_FAILED:
    return 401; // Unauthorized
  case ProtocolErrorCode::AUTHORIZATION_FAILED:
    return 403; // Forbidden
  case ProtocolErrorCode::DEVICE_NOT_FOUND:
    return 404; // Not Found
  case ProtocolErrorCode::UNSUPPORTED_OPERATION:
    return 405; // Method Not Allowed
  case ProtocolErrorCode::CONNECTION_TIMEOUT:
  case ProtocolErrorCode::DEVICE_TIMEOUT:
    return 408; // Request Timeout
  case ProtocolErrorCode::DEVICE_BUSY:
    return 409; // Conflict
  case ProtocolErrorCode::VALIDATION_ERROR:
  case ProtocolErrorCode::MISSING_REQUIRED_FIELD:
  case ProtocolErrorCode::INVALID_FIELD_VALUE:
    return 422; // Unprocessable Entity
  case ProtocolErrorCode::QUOTA_EXCEEDED:
    return 429; // Too Many Requests
  case ProtocolErrorCode::INTERNAL_ERROR:
  case ProtocolErrorCode::OPERATION_FAILED:
  case ProtocolErrorCode::DEVICE_ERROR:
    return 500; // Internal Server Error
  case ProtocolErrorCode::DEVICE_DISCONNECTED:
  case ProtocolErrorCode::CONNECTION_FAILED:
    return 502; // Bad Gateway
  case ProtocolErrorCode::RESOURCE_UNAVAILABLE:
  case ProtocolErrorCode::RESOURCE_EXHAUSTED:
    return 503; // Service Unavailable
  default:
    return 500; // Internal Server Error
  }
}

// GrpcErrorFormatter implementation
json GrpcErrorFormatter::formatError(const ProtocolError &error) const {
  json result;
  result["code"] = getProtocolErrorCode(error.code);
  result["message"] = getErrorMessage(error);
  result["details"] = error.details;

  if (!error.metadata.empty()) {
    result["metadata"] = error.metadata;
  }

  return result;
}

std::string
GrpcErrorFormatter::getErrorMessage(const ProtocolError &error) const {
  return error.message;
}

int GrpcErrorFormatter::getProtocolErrorCode(ProtocolErrorCode code) const {
  return mapToGrpcStatusCode(code);
}

int GrpcErrorFormatter::mapToGrpcStatusCode(ProtocolErrorCode code) const {
  switch (code) {
  case ProtocolErrorCode::SUCCESS:
    return 0; // OK
  case ProtocolErrorCode::OPERATION_FAILED:
    return 1; // CANCELLED
  case ProtocolErrorCode::UNKNOWN_ERROR:
    return 2; // UNKNOWN
  case ProtocolErrorCode::INVALID_PARAMETERS:
  case ProtocolErrorCode::INVALID_REQUEST:
  case ProtocolErrorCode::MESSAGE_FORMAT_ERROR:
    return 3; // INVALID_ARGUMENT
  case ProtocolErrorCode::CONNECTION_TIMEOUT:
  case ProtocolErrorCode::DEVICE_TIMEOUT:
    return 4; // DEADLINE_EXCEEDED
  case ProtocolErrorCode::DEVICE_NOT_FOUND:
    return 5; // NOT_FOUND
  case ProtocolErrorCode::DEVICE_BUSY:
    return 6; // ALREADY_EXISTS
  case ProtocolErrorCode::AUTHORIZATION_FAILED:
    return 7; // PERMISSION_DENIED
  case ProtocolErrorCode::RESOURCE_EXHAUSTED:
  case ProtocolErrorCode::QUOTA_EXCEEDED:
    return 8; // RESOURCE_EXHAUSTED
  case ProtocolErrorCode::VALIDATION_ERROR:
  case ProtocolErrorCode::MISSING_REQUIRED_FIELD:
    return 9; // FAILED_PRECONDITION
  case ProtocolErrorCode::FIELD_OUT_OF_RANGE:
    return 11; // OUT_OF_RANGE
  case ProtocolErrorCode::UNSUPPORTED_OPERATION:
    return 12; // UNIMPLEMENTED
  case ProtocolErrorCode::INTERNAL_ERROR:
  case ProtocolErrorCode::DEVICE_ERROR:
    return 13; // INTERNAL
  case ProtocolErrorCode::RESOURCE_UNAVAILABLE:
  case ProtocolErrorCode::DEVICE_DISCONNECTED:
    return 14; // UNAVAILABLE
  case ProtocolErrorCode::CONNECTION_LOST:
    return 15; // DATA_LOSS
  case ProtocolErrorCode::AUTHENTICATION_FAILED:
    return 16; // UNAUTHENTICATED
  default:
    return 2; // UNKNOWN
  }
}

// MqttErrorFormatter implementation
json MqttErrorFormatter::formatError(const ProtocolError &error) const {
  json result;
  result["reasonCode"] = getProtocolErrorCode(error.code);
  result["reasonString"] = getErrorMessage(error);
  result["userProperties"] = {{"component", error.component},
                              {"operation", error.operation},
                              {"details", error.details}};

  if (!error.metadata.empty()) {
    for (const auto &[key, value] : error.metadata.items()) {
      result["userProperties"][key] = value;
    }
  }

  return result;
}

std::string
MqttErrorFormatter::getErrorMessage(const ProtocolError &error) const {
  return error.message;
}

int MqttErrorFormatter::getProtocolErrorCode(ProtocolErrorCode code) const {
  return mapToMqttReasonCode(code);
}

int MqttErrorFormatter::mapToMqttReasonCode(ProtocolErrorCode code) const {
  switch (code) {
  case ProtocolErrorCode::SUCCESS:
    return 0x00; // Success
  case ProtocolErrorCode::PROTOCOL_ERROR:
  case ProtocolErrorCode::MESSAGE_FORMAT_ERROR:
    return 0x81; // Malformed Packet
  case ProtocolErrorCode::PROTOCOL_VERSION_MISMATCH:
    return 0x84; // Unsupported Protocol Version
  case ProtocolErrorCode::AUTHENTICATION_FAILED:
    return 0x86; // Bad User Name or Password
  case ProtocolErrorCode::AUTHORIZATION_FAILED:
    return 0x87; // Not authorized
  case ProtocolErrorCode::RESOURCE_UNAVAILABLE:
    return 0x88; // Server unavailable
  case ProtocolErrorCode::DEVICE_BUSY:
    return 0x89; // Server busy
  case ProtocolErrorCode::QUOTA_EXCEEDED:
    return 0x97; // Quota exceeded
  case ProtocolErrorCode::INVALID_PARAMETERS:
  case ProtocolErrorCode::VALIDATION_ERROR:
    return 0x9C; // Bad subscription identifier
  case ProtocolErrorCode::UNSUPPORTED_OPERATION:
    return 0x9E; // Shared Subscriptions not supported
  case ProtocolErrorCode::CONNECTION_TIMEOUT:
    return 0xA0; // Connection rate exceeded
  case ProtocolErrorCode::INTERNAL_ERROR:
  case ProtocolErrorCode::UNKNOWN_ERROR:
  default:
    return 0x80; // Unspecified error
  }
}

// ZeroMqErrorFormatter implementation
json ZeroMqErrorFormatter::formatError(const ProtocolError &error) const {
  json result;
  result["error"] = {
      {"code", getProtocolErrorCode(error.code)},
      {"message", getErrorMessage(error)},
      {"type", "ZMQ_ERROR"},
      {"component", error.component},
      {"operation", error.operation},
      {"details", error.details},
      {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        error.timestamp.time_since_epoch())
                        .count()}};

  if (!error.metadata.empty()) {
    result["error"]["metadata"] = error.metadata;
  }

  return result;
}

std::string
ZeroMqErrorFormatter::getErrorMessage(const ProtocolError &error) const {
  return "[ZMQ] " + error.message;
}

int ZeroMqErrorFormatter::getProtocolErrorCode(ProtocolErrorCode code) const {
  // ZeroMQ doesn't have standard error codes, so we use our internal codes
  return static_cast<int>(code);
}

// AscomErrorFormatter implementation
json AscomErrorFormatter::formatError(const ProtocolError &error) const {
  json result;
  result["ErrorNumber"] = getProtocolErrorCode(error.code);
  result["ErrorMessage"] = getErrorMessage(error);
  result["Source"] = getAscomErrorSource(error);
  result["HelpLink"] = "";
  result["InnerException"] = nullptr;

  if (!error.details.empty()) {
    result["Details"] = error.details;
  }

  return result;
}

std::string
AscomErrorFormatter::getErrorMessage(const ProtocolError &error) const {
  return error.message;
}

int AscomErrorFormatter::getProtocolErrorCode(ProtocolErrorCode code) const {
  return mapToAscomErrorCode(code);
}

int AscomErrorFormatter::mapToAscomErrorCode(ProtocolErrorCode code) const {
  switch (code) {
  case ProtocolErrorCode::SUCCESS:
    return 0x00000000; // Success
  case ProtocolErrorCode::INVALID_PARAMETERS:
  case ProtocolErrorCode::INVALID_REQUEST:
    return 0x80040005; // E_INVALIDARG
  case ProtocolErrorCode::UNSUPPORTED_OPERATION:
    return 0x80004001; // E_NOTIMPL
  case ProtocolErrorCode::DEVICE_NOT_FOUND:
    return 0x80040400; // ASCOM_ERROR_BASE + 0x400 (Not connected)
  case ProtocolErrorCode::DEVICE_BUSY:
    return 0x80040401; // ASCOM_ERROR_BASE + 0x401 (Invalid while parked)
  case ProtocolErrorCode::DEVICE_ERROR:
    return 0x80040402; // ASCOM_ERROR_BASE + 0x402 (Invalid operation)
  case ProtocolErrorCode::CONNECTION_FAILED:
  case ProtocolErrorCode::DEVICE_DISCONNECTED:
    return 0x80040403; // ASCOM_ERROR_BASE + 0x403 (Not connected)
  case ProtocolErrorCode::VALIDATION_ERROR:
    return 0x80040404; // ASCOM_ERROR_BASE + 0x404 (Invalid value)
  case ProtocolErrorCode::FIELD_OUT_OF_RANGE:
    return 0x80040405; // ASCOM_ERROR_BASE + 0x405 (Value not set)
  case ProtocolErrorCode::INTERNAL_ERROR:
  case ProtocolErrorCode::UNKNOWN_ERROR:
  default:
    return 0x80004005; // E_FAIL
  }
}

std::string
AscomErrorFormatter::getAscomErrorSource(const ProtocolError &error) const {
  if (!error.component.empty()) {
    return "Hydrogen." + error.component;
  }
  return "Hydrogen.Core";
}

// IndiErrorFormatter implementation
json IndiErrorFormatter::formatError(const ProtocolError &error) const {
  json result;
  result["device"] = error.component;
  result["name"] = error.operation;
  result["state"] = mapToIndiState(error.code);
  result["message"] = getErrorMessage(error);
  result["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                            error.timestamp.time_since_epoch())
                            .count();

  if (!error.details.empty()) {
    result["details"] = error.details;
  }

  return result;
}

std::string
IndiErrorFormatter::getErrorMessage(const ProtocolError &error) const {
  return error.message;
}

int IndiErrorFormatter::getProtocolErrorCode(ProtocolErrorCode code) const {
  // INDI doesn't use numeric error codes, return 0 for compatibility
  return 0;
}

std::string IndiErrorFormatter::mapToIndiState(ProtocolErrorCode code) const {
  switch (code) {
  case ProtocolErrorCode::SUCCESS:
    return "Ok";
  case ProtocolErrorCode::DEVICE_BUSY:
    return "Busy";
  case ProtocolErrorCode::CONNECTION_TIMEOUT:
  case ProtocolErrorCode::DEVICE_TIMEOUT:
    return "Alert";
  case ProtocolErrorCode::DEVICE_ERROR:
  case ProtocolErrorCode::OPERATION_FAILED:
  case ProtocolErrorCode::INTERNAL_ERROR:
  case ProtocolErrorCode::UNKNOWN_ERROR:
  default:
    return "Alert";
  }
}

// ProtocolErrorMapper implementation
ProtocolErrorMapper::ProtocolErrorMapper() {
  initializeDefaultFormatters();
  initializeDefaultExceptionHandlers();
}

void ProtocolErrorMapper::registerFormatter(
    MessageFormat format, std::unique_ptr<ProtocolErrorFormatter> formatter) {
  formatters_[format] = std::move(formatter);
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug("ProtocolErrorMapper: Registered formatter for format: {}",
                static_cast<int>(format));
#endif
}

void ProtocolErrorMapper::registerExceptionHandler(
    const std::string &exceptionType, ExceptionHandler handler) {
  exceptionHandlers_[exceptionType] = handler;
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::debug(
      "ProtocolErrorMapper: Registered exception handler for type: {}",
      exceptionType);
#endif
}

ProtocolError
ProtocolErrorMapper::mapException(const std::exception &ex,
                                  const std::string &component,
                                  const std::string &operation) const {
  std::string exceptionType = getExceptionTypeName(ex);

  // Try specific exception handler first
  auto it = exceptionHandlers_.find(exceptionType);
  if (it != exceptionHandlers_.end()) {
    try {
      ProtocolError error = it->second(ex);
      error.component = component;
      error.operation = operation;
      error.timestamp = std::chrono::system_clock::now();
      return error;
    } catch (const std::exception &handlerEx) {
#ifdef HYDROGEN_HAS_SPDLOG
      spdlog::error("Exception handler failed for {}: {}", exceptionType,
                    handlerEx.what());
#endif
    }
  }

  // Fall back to standard mapping
  ProtocolError error = mapStandardException(ex);
  error.component = component;
  error.operation = operation;
  error.timestamp = std::chrono::system_clock::now();

  return error;
}

ProtocolError
ProtocolErrorMapper::createError(ProtocolErrorCode code,
                                 const std::string &message,
                                 const std::string &details) const {
  ProtocolError error;
  error.code = code;
  error.message = message;
  error.details = details;
  error.timestamp = std::chrono::system_clock::now();

  return error;
}

json ProtocolErrorMapper::formatErrorForProtocol(const ProtocolError &error,
                                                 MessageFormat format) const {
  auto it = formatters_.find(format);
  if (it != formatters_.end()) {
    return it->second->formatError(error);
  }

  // Default JSON format if no specific formatter
  return error.toJson();
}

std::unique_ptr<ErrorMessage> ProtocolErrorMapper::createErrorMessage(
    const ProtocolError &error, const std::string &originalMessageId) const {
  auto errorMsg = std::make_unique<ErrorMessage>(
      std::to_string(static_cast<int>(error.code)), error.message);

  json details;
  details["component"] = error.component;
  details["operation"] = error.operation;
  details["details"] = error.details;
  details["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                             error.timestamp.time_since_epoch())
                             .count();

  if (!error.metadata.empty()) {
    details["metadata"] = error.metadata;
  }

  errorMsg->setDetails(details);

  if (!originalMessageId.empty()) {
    errorMsg->setOriginalMessageId(originalMessageId);
  }

  return errorMsg;
}

bool ProtocolErrorMapper::hasFormatter(MessageFormat format) const {
  return formatters_.find(format) != formatters_.end();
}

std::vector<MessageFormat> ProtocolErrorMapper::getSupportedFormats() const {
  std::vector<MessageFormat> formats;
  for (const auto &[format, formatter] : formatters_) {
    formats.push_back(format);
  }
  return formats;
}

void ProtocolErrorMapper::initializeDefaultFormatters() {
  registerFormatter(MessageFormat::HTTP_JSON,
                    std::make_unique<HttpErrorFormatter>());
  registerFormatter(MessageFormat::PROTOBUF,
                    std::make_unique<GrpcErrorFormatter>());
  registerFormatter(MessageFormat::MQTT,
                    std::make_unique<MqttErrorFormatter>());
  registerFormatter(MessageFormat::ZEROMQ,
                    std::make_unique<ZeroMqErrorFormatter>());

  // Note: ASCOM and INDI formatters would be registered when those protocols
  // are enabled
#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("ProtocolErrorMapper: Initialized default error formatters");
#endif
}

void ProtocolErrorMapper::initializeDefaultExceptionHandlers() {
  // Register standard exception handlers with wrapper functions
  registerExceptionHandler("std::runtime_error", [](const std::exception &ex) {
    return ErrorMappingUtils::mapRuntimeError(
        dynamic_cast<const std::runtime_error &>(ex));
  });

  registerExceptionHandler("std::logic_error", [](const std::exception &ex) {
    return ErrorMappingUtils::mapLogicError(
        dynamic_cast<const std::logic_error &>(ex));
  });

  registerExceptionHandler(
      "std::invalid_argument", [](const std::exception &ex) {
        return ErrorMappingUtils::mapInvalidArgument(
            dynamic_cast<const std::invalid_argument &>(ex));
      });

  registerExceptionHandler("std::out_of_range", [](const std::exception &ex) {
    return ErrorMappingUtils::mapOutOfRange(
        dynamic_cast<const std::out_of_range &>(ex));
  });

  registerExceptionHandler("std::system_error", [](const std::exception &ex) {
    return ErrorMappingUtils::mapSystemError(
        dynamic_cast<const std::system_error &>(ex));
  });

#ifdef HYDROGEN_HAS_SPDLOG
  spdlog::info("ProtocolErrorMapper: Initialized default exception handlers");
#endif
}

ProtocolError
ProtocolErrorMapper::mapStandardException(const std::exception &ex) const {
  return ErrorMappingUtils::mapStdException(ex);
}

std::string
ProtocolErrorMapper::getExceptionTypeName(const std::exception &ex) const {
  const std::type_info &ti = typeid(ex);

#ifdef __GNUC__
  int status;
  char *demangled = abi::__cxa_demangle(ti.name(), nullptr, nullptr, &status);

  std::string result;
  if (status == 0 && demangled) {
    result = demangled;
    free(demangled);
  } else {
    result = ti.name();
  }
#else
  // For MSVC and other compilers, type_info::name() is already readable
  std::string result = ti.name();
#endif

  return result;
}

// Static utility methods for ProtocolErrorMapper
std::string ProtocolErrorMapper::getErrorCodeName(ProtocolErrorCode code) {
  switch (code) {
  case ProtocolErrorCode::SUCCESS:
    return "SUCCESS";
  case ProtocolErrorCode::UNKNOWN_ERROR:
    return "UNKNOWN_ERROR";
  case ProtocolErrorCode::INTERNAL_ERROR:
    return "INTERNAL_ERROR";
  case ProtocolErrorCode::INVALID_REQUEST:
    return "INVALID_REQUEST";
  case ProtocolErrorCode::INVALID_PARAMETERS:
    return "INVALID_PARAMETERS";
  case ProtocolErrorCode::OPERATION_FAILED:
    return "OPERATION_FAILED";
  case ProtocolErrorCode::CONNECTION_FAILED:
    return "CONNECTION_FAILED";
  case ProtocolErrorCode::CONNECTION_LOST:
    return "CONNECTION_LOST";
  case ProtocolErrorCode::CONNECTION_TIMEOUT:
    return "CONNECTION_TIMEOUT";
  case ProtocolErrorCode::AUTHENTICATION_FAILED:
    return "AUTHENTICATION_FAILED";
  case ProtocolErrorCode::AUTHORIZATION_FAILED:
    return "AUTHORIZATION_FAILED";
  case ProtocolErrorCode::PROTOCOL_ERROR:
    return "PROTOCOL_ERROR";
  case ProtocolErrorCode::UNSUPPORTED_OPERATION:
    return "UNSUPPORTED_OPERATION";
  case ProtocolErrorCode::MESSAGE_FORMAT_ERROR:
    return "MESSAGE_FORMAT_ERROR";
  case ProtocolErrorCode::PROTOCOL_VERSION_MISMATCH:
    return "PROTOCOL_VERSION_MISMATCH";
  case ProtocolErrorCode::DEVICE_NOT_FOUND:
    return "DEVICE_NOT_FOUND";
  case ProtocolErrorCode::DEVICE_BUSY:
    return "DEVICE_BUSY";
  case ProtocolErrorCode::DEVICE_ERROR:
    return "DEVICE_ERROR";
  case ProtocolErrorCode::DEVICE_DISCONNECTED:
    return "DEVICE_DISCONNECTED";
  case ProtocolErrorCode::DEVICE_TIMEOUT:
    return "DEVICE_TIMEOUT";
  case ProtocolErrorCode::RESOURCE_UNAVAILABLE:
    return "RESOURCE_UNAVAILABLE";
  case ProtocolErrorCode::RESOURCE_EXHAUSTED:
    return "RESOURCE_EXHAUSTED";
  case ProtocolErrorCode::QUOTA_EXCEEDED:
    return "QUOTA_EXCEEDED";
  case ProtocolErrorCode::VALIDATION_ERROR:
    return "VALIDATION_ERROR";
  case ProtocolErrorCode::MISSING_REQUIRED_FIELD:
    return "MISSING_REQUIRED_FIELD";
  case ProtocolErrorCode::INVALID_FIELD_VALUE:
    return "INVALID_FIELD_VALUE";
  case ProtocolErrorCode::FIELD_OUT_OF_RANGE:
    return "FIELD_OUT_OF_RANGE";
  default:
    return "UNKNOWN";
  }
}

ProtocolErrorCode
ProtocolErrorMapper::parseErrorCode(const std::string &codeName) {
  static std::unordered_map<std::string, ProtocolErrorCode> codeMap = {
      {"SUCCESS", ProtocolErrorCode::SUCCESS},
      {"UNKNOWN_ERROR", ProtocolErrorCode::UNKNOWN_ERROR},
      {"INTERNAL_ERROR", ProtocolErrorCode::INTERNAL_ERROR},
      {"INVALID_REQUEST", ProtocolErrorCode::INVALID_REQUEST},
      {"INVALID_PARAMETERS", ProtocolErrorCode::INVALID_PARAMETERS},
      {"OPERATION_FAILED", ProtocolErrorCode::OPERATION_FAILED},
      {"CONNECTION_FAILED", ProtocolErrorCode::CONNECTION_FAILED},
      {"CONNECTION_LOST", ProtocolErrorCode::CONNECTION_LOST},
      {"CONNECTION_TIMEOUT", ProtocolErrorCode::CONNECTION_TIMEOUT},
      {"AUTHENTICATION_FAILED", ProtocolErrorCode::AUTHENTICATION_FAILED},
      {"AUTHORIZATION_FAILED", ProtocolErrorCode::AUTHORIZATION_FAILED},
      {"PROTOCOL_ERROR", ProtocolErrorCode::PROTOCOL_ERROR},
      {"UNSUPPORTED_OPERATION", ProtocolErrorCode::UNSUPPORTED_OPERATION},
      {"MESSAGE_FORMAT_ERROR", ProtocolErrorCode::MESSAGE_FORMAT_ERROR},
      {"PROTOCOL_VERSION_MISMATCH",
       ProtocolErrorCode::PROTOCOL_VERSION_MISMATCH},
      {"DEVICE_NOT_FOUND", ProtocolErrorCode::DEVICE_NOT_FOUND},
      {"DEVICE_BUSY", ProtocolErrorCode::DEVICE_BUSY},
      {"DEVICE_ERROR", ProtocolErrorCode::DEVICE_ERROR},
      {"DEVICE_DISCONNECTED", ProtocolErrorCode::DEVICE_DISCONNECTED},
      {"DEVICE_TIMEOUT", ProtocolErrorCode::DEVICE_TIMEOUT},
      {"RESOURCE_UNAVAILABLE", ProtocolErrorCode::RESOURCE_UNAVAILABLE},
      {"RESOURCE_EXHAUSTED", ProtocolErrorCode::RESOURCE_EXHAUSTED},
      {"QUOTA_EXCEEDED", ProtocolErrorCode::QUOTA_EXCEEDED},
      {"VALIDATION_ERROR", ProtocolErrorCode::VALIDATION_ERROR},
      {"MISSING_REQUIRED_FIELD", ProtocolErrorCode::MISSING_REQUIRED_FIELD},
      {"INVALID_FIELD_VALUE", ProtocolErrorCode::INVALID_FIELD_VALUE},
      {"FIELD_OUT_OF_RANGE", ProtocolErrorCode::FIELD_OUT_OF_RANGE}};

  auto it = codeMap.find(codeName);
  return (it != codeMap.end()) ? it->second : ProtocolErrorCode::UNKNOWN_ERROR;
}

bool ProtocolErrorMapper::isRecoverableError(ProtocolErrorCode code) {
  switch (code) {
  case ProtocolErrorCode::CONNECTION_TIMEOUT:
  case ProtocolErrorCode::DEVICE_TIMEOUT:
  case ProtocolErrorCode::CONNECTION_LOST:
  case ProtocolErrorCode::DEVICE_BUSY:
  case ProtocolErrorCode::RESOURCE_UNAVAILABLE:
    return true;
  case ProtocolErrorCode::AUTHENTICATION_FAILED:
  case ProtocolErrorCode::AUTHORIZATION_FAILED:
  case ProtocolErrorCode::UNSUPPORTED_OPERATION:
  case ProtocolErrorCode::DEVICE_NOT_FOUND:
  case ProtocolErrorCode::VALIDATION_ERROR:
  case ProtocolErrorCode::INVALID_PARAMETERS:
    return false;
  default:
    return false;
  }
}

// ErrorMappingUtils implementation
ProtocolError ErrorMappingUtils::mapStdException(const std::exception &ex) {
  ProtocolError error;
  error.code = ProtocolErrorCode::UNKNOWN_ERROR;
  error.message = ex.what();
  error.details = "Standard exception occurred";
  error.timestamp = std::chrono::system_clock::now();
  return error;
}

ProtocolError ErrorMappingUtils::mapRuntimeError(const std::runtime_error &ex) {
  ProtocolError error;
  error.code = ProtocolErrorCode::OPERATION_FAILED;
  error.message = ex.what();
  error.details = "Runtime error occurred during operation";
  error.timestamp = std::chrono::system_clock::now();
  return error;
}

ProtocolError ErrorMappingUtils::mapLogicError(const std::logic_error &ex) {
  ProtocolError error;
  error.code = ProtocolErrorCode::INTERNAL_ERROR;
  error.message = ex.what();
  error.details = "Logic error in program execution";
  error.timestamp = std::chrono::system_clock::now();
  return error;
}

ProtocolError
ErrorMappingUtils::mapInvalidArgument(const std::invalid_argument &ex) {
  ProtocolError error;
  error.code = ProtocolErrorCode::INVALID_PARAMETERS;
  error.message = ex.what();
  error.details = "Invalid argument provided to function";
  error.timestamp = std::chrono::system_clock::now();
  return error;
}

ProtocolError ErrorMappingUtils::mapOutOfRange(const std::out_of_range &ex) {
  ProtocolError error;
  error.code = ProtocolErrorCode::FIELD_OUT_OF_RANGE;
  error.message = ex.what();
  error.details = "Value is out of valid range";
  error.timestamp = std::chrono::system_clock::now();
  return error;
}

ProtocolError ErrorMappingUtils::mapSystemError(const std::system_error &ex) {
  ProtocolError error;
  error.code = ProtocolErrorCode::INTERNAL_ERROR;
  error.message = ex.what();
  error.details = "System error: " + std::to_string(ex.code().value()) + " (" +
                  ex.code().message() + ")";
  error.timestamp = std::chrono::system_clock::now();
  return error;
}

ProtocolError ErrorMappingUtils::mapDeviceException(const std::exception &ex) {
  ProtocolError error;
  error.code = ProtocolErrorCode::DEVICE_ERROR;
  error.message = ex.what();
  error.details = "Device-specific error occurred";
  error.timestamp = std::chrono::system_clock::now();
  return error;
}

ProtocolError
ErrorMappingUtils::mapConnectionException(const std::exception &ex) {
  ProtocolError error;
  error.code = ProtocolErrorCode::CONNECTION_FAILED;
  error.message = ex.what();
  error.details = "Connection-related error occurred";
  error.timestamp = std::chrono::system_clock::now();
  return error;
}

ProtocolError
ErrorMappingUtils::mapProtocolException(const std::exception &ex) {
  ProtocolError error;
  error.code = ProtocolErrorCode::PROTOCOL_ERROR;
  error.message = ex.what();
  error.details = "Protocol-specific error occurred";
  error.timestamp = std::chrono::system_clock::now();
  return error;
}

ProtocolError
ErrorMappingUtils::mapValidationException(const std::exception &ex) {
  ProtocolError error;
  error.code = ProtocolErrorCode::VALIDATION_ERROR;
  error.message = ex.what();
  error.details = "Validation error occurred";
  error.timestamp = std::chrono::system_clock::now();
  return error;
}

bool ErrorMappingUtils::isCriticalError(ProtocolErrorCode code) {
  switch (code) {
  case ProtocolErrorCode::INTERNAL_ERROR:
  case ProtocolErrorCode::RESOURCE_EXHAUSTED:
  case ProtocolErrorCode::DEVICE_ERROR:
    return true;
  default:
    return false;
  }
}

bool ErrorMappingUtils::requiresReconnection(ProtocolErrorCode code) {
  switch (code) {
  case ProtocolErrorCode::CONNECTION_FAILED:
  case ProtocolErrorCode::CONNECTION_LOST:
  case ProtocolErrorCode::DEVICE_DISCONNECTED:
  case ProtocolErrorCode::PROTOCOL_ERROR:
    return true;
  default:
    return false;
  }
}

bool ErrorMappingUtils::shouldRetry(ProtocolErrorCode code) {
  switch (code) {
  case ProtocolErrorCode::CONNECTION_TIMEOUT:
  case ProtocolErrorCode::DEVICE_TIMEOUT:
  case ProtocolErrorCode::DEVICE_BUSY:
  case ProtocolErrorCode::RESOURCE_UNAVAILABLE:
    return true;
  case ProtocolErrorCode::AUTHENTICATION_FAILED:
  case ProtocolErrorCode::AUTHORIZATION_FAILED:
  case ProtocolErrorCode::VALIDATION_ERROR:
  case ProtocolErrorCode::INVALID_PARAMETERS:
  case ProtocolErrorCode::UNSUPPORTED_OPERATION:
    return false;
  default:
    return false;
  }
}

// Global instance
ProtocolErrorMapper &getGlobalProtocolErrorMapper() {
  static ProtocolErrorMapper instance;
  return instance;
}

} // namespace core
} // namespace hydrogen

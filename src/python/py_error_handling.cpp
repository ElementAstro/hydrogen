#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>

#include "../device/interfaces/device_interface.h"
#include "../device/interfaces/automatic_compatibility.h"
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace py = pybind11;
using namespace hydrogen::device;
using namespace hydrogen::device::interfaces;

/**
 * @brief Custom exception classes for Python bindings with ASCOM/INDI compliance
 */

// Base device exception
class DeviceException : public std::runtime_error {
public:
    explicit DeviceException(const std::string& message) : std::runtime_error(message) {}
};

// ASCOM-specific exceptions following ASCOM standard error codes
class ASCOMException : public DeviceException {
public:
    uint32_t errorCode;
    
    ASCOMException(uint32_t code, const std::string& message) 
        : DeviceException(message), errorCode(code) {}
};

class ASCOMNotConnectedException : public ASCOMException {
public:
    ASCOMNotConnectedException() : ASCOMException(0x80040007, "Device not connected") {}
};

class ASCOMInvalidValueException : public ASCOMException {
public:
    ASCOMInvalidValueException(const std::string& parameter) 
        : ASCOMException(0x80040002, "Invalid value for parameter: " + parameter) {}
};

class ASCOMInvalidOperationException : public ASCOMException {
public:
    ASCOMInvalidOperationException(const std::string& operation) 
        : ASCOMException(0x8004000B, "Invalid operation: " + operation) {}
};

class ASCOMNotImplementedException : public ASCOMException {
public:
    ASCOMNotImplementedException(const std::string& method) 
        : ASCOMException(0x8004000C, "Method not implemented: " + method) {}
};

// INDI-specific exceptions
class INDIException : public DeviceException {
public:
    explicit INDIException(const std::string& message) : DeviceException("INDI Error: " + message) {}
};

class INDIPropertyException : public INDIException {
public:
    INDIPropertyException(const std::string& property) 
        : INDIException("Property error: " + property) {}
};

class INDITimeoutException : public INDIException {
public:
    INDITimeoutException() : INDIException("Operation timed out") {}
};

/**
 * @brief Type conversion utilities with validation
 */
class TypeValidator {
public:
    // Validate numeric ranges
    template<typename T>
    static T validateRange(const T& value, const T& min, const T& max, const std::string& paramName) {
        if (value < min || value > max) {
            throw ASCOMInvalidValueException(paramName + " must be between " + 
                                           std::to_string(min) + " and " + std::to_string(max));
        }
        return value;
    }
    
    // Validate positive values
    template<typename T>
    static T validatePositive(const T& value, const std::string& paramName) {
        if (value <= 0) {
            throw ASCOMInvalidValueException(paramName + " must be positive");
        }
        return value;
    }
    
    // Validate non-negative values
    template<typename T>
    static T validateNonNegative(const T& value, const std::string& paramName) {
        if (value < 0) {
            throw ASCOMInvalidValueException(paramName + " must be non-negative");
        }
        return value;
    }
    
    // Validate string not empty
    static std::string validateNotEmpty(const std::string& value, const std::string& paramName) {
        if (value.empty()) {
            throw ASCOMInvalidValueException(paramName + " cannot be empty");
        }
        return value;
    }
    
    // Validate array dimensions
    template<typename T>
    static void validateArrayDimensions(const py::array_t<T>& array, size_t expectedWidth, size_t expectedHeight) {
        if (array.ndim() != 2) {
            throw ASCOMInvalidValueException("Array must be 2-dimensional");
        }
        if (array.shape(0) != expectedHeight || array.shape(1) != expectedWidth) {
            throw ASCOMInvalidValueException("Array dimensions do not match expected size");
        }
    }
};

/**
 * @brief Safe property access wrappers
 */
template<typename DeviceType>
class SafePropertyAccess {
public:
    // Safe property getter with error handling
    template<typename T>
    static T safeGetProperty(std::shared_ptr<DeviceType> device, const std::string& propertyName) {
        try {
            if (!device) {
                throw ASCOMNotConnectedException();
            }
            
            json value = device->getProperty(propertyName);
            return automatic::TypeConverter::fromJson<T>(value);
            
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error getting property {}: {}", propertyName, e.what());
            throw DeviceException("Failed to get property " + propertyName + ": " + e.what());
        }
    }
    
    // Safe property setter with validation
    template<typename T>
    static void safeSetProperty(std::shared_ptr<DeviceType> device, const std::string& propertyName, const T& value) {
        try {
            if (!device) {
                throw ASCOMNotConnectedException();
            }
            
            json jsonValue = automatic::TypeConverter::toJson(value);
            if (!device->setProperty(propertyName, jsonValue)) {
                throw ASCOMInvalidOperationException("Failed to set property " + propertyName);
            }
            
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error setting property {} to {}: {}", propertyName, value, e.what());
            throw DeviceException("Failed to set property " + propertyName + ": " + e.what());
        }
    }
    
    // Safe method invocation with error handling
    template<typename ReturnType, typename... Args>
    static ReturnType safeInvokeMethod(std::shared_ptr<DeviceType> device, const std::string& methodName, Args... args) {
        try {
            if (!device) {
                throw ASCOMNotConnectedException();
            }
            
            json params = json::object();
            size_t i = 0;
            (params["param" + std::to_string(i++)] = automatic::TypeConverter::toJson(args), ...);
            
            json result;
            if (!device->handleDeviceCommand(methodName, params, result)) {
                throw ASCOMInvalidOperationException("Method " + methodName + " failed");
            }
            
            if constexpr (std::is_void_v<ReturnType>) {
                return;
            } else {
                return automatic::TypeConverter::fromJson<ReturnType>(result);
            }
            
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error invoking method {}: {}", methodName, e.what());
            throw DeviceException("Failed to invoke method " + methodName + ": " + e.what());
        }
    }
};

/**
 * @brief Connection state validator
 */
class ConnectionValidator {
public:
    template<typename DeviceType>
    static void validateConnected(std::shared_ptr<DeviceType> device, const std::string& operation) {
        if (!device) {
            throw ASCOMNotConnectedException();
        }
        
        try {
            bool connected = device->getProperty("connected").get<bool>();
            if (!connected) {
                throw ASCOMNotConnectedException();
            }
        } catch (const json::exception&) {
            // If connected property doesn't exist, assume device is connected if it exists
            if (!device) {
                throw ASCOMNotConnectedException();
            }
        }
    }
    
    template<typename DeviceType>
    static void validateCapability(std::shared_ptr<DeviceType> device, const std::string& capability, const std::string& operation) {
        validateConnected(device, operation);
        
        try {
            bool canPerform = device->getProperty(capability).get<bool>();
            if (!canPerform) {
                throw ASCOMNotImplementedException(operation + " (capability " + capability + " not supported)");
            }
        } catch (const json::exception&) {
            // If capability property doesn't exist, assume it's supported
            SPDLOG_DEBUG("Capability {} not found, assuming supported", capability);
        }
    }
};

/**
 * @brief Async operation wrapper for Python
 */
class AsyncOperationWrapper {
public:
    template<typename DeviceType, typename ReturnType>
    static py::object wrapAsyncOperation(std::shared_ptr<DeviceType> device, 
                                       const std::string& methodName,
                                       const std::function<ReturnType()>& operation) {
        try {
            ConnectionValidator::validateConnected(device, methodName);
            
            // For now, execute synchronously but could be extended for true async
            if constexpr (std::is_void_v<ReturnType>) {
                operation();
                return py::none();
            } else {
                ReturnType result = operation();
                return py::cast(result);
            }
            
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Async operation {} failed: {}", methodName, e.what());
            throw;
        }
    }
};

/**
 * @brief Bind error handling utilities to Python
 */
void bind_error_handling(py::module& m) {
    // Exception classes
    py::register_exception<DeviceException>(m, "DeviceException");
    py::register_exception<ASCOMException>(m, "ASCOMException");
    py::register_exception<ASCOMNotConnectedException>(m, "ASCOMNotConnectedException");
    py::register_exception<ASCOMInvalidValueException>(m, "ASCOMInvalidValueException");
    py::register_exception<ASCOMInvalidOperationException>(m, "ASCOMInvalidOperationException");
    py::register_exception<ASCOMNotImplementedException>(m, "ASCOMNotImplementedException");
    py::register_exception<INDIException>(m, "INDIException");
    py::register_exception<INDIPropertyException>(m, "INDIPropertyException");
    py::register_exception<INDITimeoutException>(m, "INDITimeoutException");
    
    // Type validator utilities
    py::class_<TypeValidator>(m, "TypeValidator")
        .def_static("validate_range", [](double value, double min, double max, const std::string& name) {
            return TypeValidator::validateRange(value, min, max, name);
        }, py::arg("value"), py::arg("min"), py::arg("max"), py::arg("param_name"))
        .def_static("validate_positive", [](double value, const std::string& name) {
            return TypeValidator::validatePositive(value, name);
        }, py::arg("value"), py::arg("param_name"))
        .def_static("validate_non_negative", [](double value, const std::string& name) {
            return TypeValidator::validateNonNegative(value, name);
        }, py::arg("value"), py::arg("param_name"))
        .def_static("validate_not_empty", &TypeValidator::validateNotEmpty,
                   py::arg("value"), py::arg("param_name"));
    
    // Error code constants
    m.attr("ASCOM_OK") = py::int_(0x00000000);
    m.attr("ASCOM_UNSPECIFIED_ERROR") = py::int_(0x80040001);
    m.attr("ASCOM_INVALID_VALUE") = py::int_(0x80040002);
    m.attr("ASCOM_VALUE_NOT_SET") = py::int_(0x80040003);
    m.attr("ASCOM_NOT_CONNECTED") = py::int_(0x80040007);
    m.attr("ASCOM_INVALID_WHILE_PARKED") = py::int_(0x80040008);
    m.attr("ASCOM_INVALID_WHILE_SLAVED") = py::int_(0x80040009);
    m.attr("ASCOM_SETTINGS_PROVIDER_ERROR") = py::int_(0x8004000A);
    m.attr("ASCOM_INVALID_OPERATION") = py::int_(0x8004000B);
    m.attr("ASCOM_ACTION_NOT_IMPLEMENTED") = py::int_(0x8004000C);
}

/**
 * @brief Exception translator for automatic exception conversion
 */
void setupExceptionTranslator() {
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p) std::rethrow_exception(p);
        } catch (const ASCOMException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const INDIException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const DeviceException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::out_of_range& e) {
            PyErr_SetString(PyExc_IndexError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    });
}

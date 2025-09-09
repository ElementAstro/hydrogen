#pragma once

#include "device_interface.h"
#include "ascom_compatibility.h"
#include "indi_compatibility.h"
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>
#include <typeinfo>
#include <any>
#include <variant>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace device {
namespace interfaces {
namespace automatic {

using json = nlohmann::json;

/**
 * @brief Type conversion utilities for automatic parameter handling
 */
class TypeConverter {
public:
    // Convert between different numeric types with validation
    template<typename To, typename From>
    static To convert(const From& value) {
        if constexpr (std::is_same_v<To, From>) {
            return value;
        } else if constexpr (std::is_arithmetic_v<To> && std::is_arithmetic_v<From>) {
            return static_cast<To>(value);
        } else {
            throw std::invalid_argument("Unsupported type conversion");
        }
    }
    
    // Convert JSON to specific types
    template<typename T>
    static T fromJson(const json& j) {
        if constexpr (std::is_same_v<T, std::string>) {
            return j.get<std::string>();
        } else if constexpr (std::is_arithmetic_v<T>) {
            return j.get<T>();
        } else if constexpr (std::is_same_v<T, bool>) {
            return j.get<bool>();
        } else {
            return T::fromJson(j);
        }
    }
    
    // Convert specific types to JSON
    template<typename T>
    static json toJson(const T& value) {
        if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, std::string> || std::is_same_v<T, bool>) {
            return json(value);
        } else {
            return value.toJson();
        }
    }
    
    // Validate parameter ranges and constraints
    template<typename T>
    static bool validate(const T& value, const T& min = T{}, const T& max = T{}) {
        if constexpr (std::is_arithmetic_v<T>) {
            return value >= min && value <= max;
        }
        return true;
    }
};

/**
 * @brief Property mapping descriptor for automatic property handling
 */
struct PropertyMapping {
    std::string internalName;
    std::string ascomName;
    std::string indiName;
    std::string dataType;
    bool readOnly;
    std::any minValue;
    std::any maxValue;
    std::function<json(const json&)> validator;
    std::function<json(const json&)> transformer;
    
    PropertyMapping(const std::string& internal, const std::string& ascom, const std::string& indi,
                   const std::string& type, bool ro = false)
        : internalName(internal), ascomName(ascom), indiName(indi), dataType(type), readOnly(ro) {}
};

/**
 * @brief Method mapping descriptor for automatic method delegation
 */
struct MethodMapping {
    std::string internalMethod;
    std::string ascomMethod;
    std::string indiMethod;
    std::vector<std::string> parameterTypes;
    std::string returnType;
    std::function<json(const std::vector<json>&)> parameterTransformer;
    std::function<json(const json&)> resultTransformer;
    bool isAsync;
    
    MethodMapping(const std::string& internal, const std::string& ascom, const std::string& indi,
                 const std::vector<std::string>& params = {}, const std::string& ret = "void")
        : internalMethod(internal), ascomMethod(ascom), indiMethod(indi), 
          parameterTypes(params), returnType(ret), isAsync(false) {}
};

/**
 * @brief Base automatic adapter class for seamless protocol translation
 */
class AutomaticAdapterBase {
public:
    virtual ~AutomaticAdapterBase() = default;
    
    // Property management
    void registerPropertyMapping(const PropertyMapping& mapping);
    void registerMethodMapping(const MethodMapping& mapping);
    
    // Automatic property access
    json getProperty(const std::string& protocolName, const std::string& protocol) const;
    bool setProperty(const std::string& protocolName, const json& value, const std::string& protocol);
    
    // Automatic method invocation
    json invokeMethod(const std::string& protocolMethod, const std::vector<json>& parameters, const std::string& protocol);
    
    // Error translation
    std::string translateError(const std::exception& e, const std::string& protocol) const;
    
    // Device discovery and registration
    virtual void registerWithProtocol(const std::string& protocol) = 0;
    virtual void unregisterFromProtocol(const std::string& protocol) = 0;

protected:
    // Internal device access
    virtual json getInternalProperty(const std::string& name) const = 0;
    virtual bool setInternalProperty(const std::string& name, const json& value) = 0;
    virtual json invokeInternalMethod(const std::string& method, const std::vector<json>& parameters) = 0;
    
    // Property and method mappings
    std::unordered_map<std::string, PropertyMapping> propertyMappings_;
    std::unordered_map<std::string, MethodMapping> methodMappings_;
    
    // Protocol-specific name mappings
    std::unordered_map<std::string, std::string> ascomToInternal_;
    std::unordered_map<std::string, std::string> indiToInternal_;
    std::unordered_map<std::string, std::string> internalToAscom_;
    std::unordered_map<std::string, std::string> internalToIndi_;

private:
    PropertyMapping* findPropertyMapping(const std::string& protocolName, const std::string& protocol);
    MethodMapping* findMethodMapping(const std::string& protocolMethod, const std::string& protocol);
};

/**
 * @brief Automatic device adapter template for specific device types
 */
template<typename DeviceType>
class AutomaticDeviceAdapter : public AutomaticAdapterBase {
public:
    explicit AutomaticDeviceAdapter(std::shared_ptr<DeviceType> device)
        : device_(device) {
        initializeStandardMappings();
    }
    
    // Protocol registration
    void registerWithProtocol(const std::string& protocol) override {
        if (protocol == "ASCOM") {
            registerWithASCOM();
        } else if (protocol == "INDI") {
            registerWithINDI();
        }
    }
    
    void unregisterFromProtocol(const std::string& protocol) override {
        if (protocol == "ASCOM") {
            unregisterFromASCOM();
        } else if (protocol == "INDI") {
            unregisterFromINDI();
        }
    }

protected:
    // Internal device access implementation
    json getInternalProperty(const std::string& name) const override {
        if (device_) {
            return device_->getProperty(name);
        }
        return json{};
    }
    
    bool setInternalProperty(const std::string& name, const json& value) override {
        if (device_) {
            return device_->setProperty(name, value);
        }
        return false;
    }
    
    json invokeInternalMethod(const std::string& method, const std::vector<json>& parameters) override {
        if (device_) {
            json result;
            json params = json::object();
            for (size_t i = 0; i < parameters.size(); ++i) {
                params["param" + std::to_string(i)] = parameters[i];
            }
            
            if (device_->handleDeviceCommand(method, params, result)) {
                return result;
            }
        }
        return json{};
    }

private:
    std::shared_ptr<DeviceType> device_;
    
    // Initialize standard property and method mappings
    void initializeStandardMappings();
    
    // Protocol-specific registration
    void registerWithASCOM();
    void unregisterFromASCOM();
    void registerWithINDI();
    void unregisterFromINDI();
};

/**
 * @brief ASCOM automatic adapter with COM interface support
 */
template<typename DeviceType>
class ASCOMAutomaticAdapter : public AutomaticDeviceAdapter<DeviceType> {
public:
    explicit ASCOMAutomaticAdapter(std::shared_ptr<DeviceType> device)
        : AutomaticDeviceAdapter<DeviceType>(device) {}
    
    // ASCOM-specific property access
    template<typename T>
    T getASCOMProperty(const std::string& propertyName) const {
        json value = this->getProperty(propertyName, "ASCOM");
        return TypeConverter::fromJson<T>(value);
    }
    
    template<typename T>
    void setASCOMProperty(const std::string& propertyName, const T& value) {
        json jsonValue = TypeConverter::toJson(value);
        this->setProperty(propertyName, jsonValue, "ASCOM");
    }
    
    // ASCOM-specific method invocation
    template<typename ReturnType, typename... Args>
    ReturnType invokeASCOMMethod(const std::string& methodName, Args... args) {
        std::vector<json> parameters;
        (parameters.push_back(TypeConverter::toJson(args)), ...);
        
        json result = this->invokeMethod(methodName, parameters, "ASCOM");
        
        if constexpr (std::is_void_v<ReturnType>) {
            return;
        } else {
            return TypeConverter::fromJson<ReturnType>(result);
        }
    }
    
    // ASCOM error handling
    void handleASCOMException(const std::exception& e) const {
        std::string ascomError = this->translateError(e, "ASCOM");
        // In real implementation, would throw ASCOM-specific exception
        throw std::runtime_error("ASCOM Error: " + ascomError);
    }
};

/**
 * @brief INDI automatic adapter with XML property support
 */
template<typename DeviceType>
class INDIAutomaticAdapter : public AutomaticDeviceAdapter<DeviceType> {
public:
    explicit INDIAutomaticAdapter(std::shared_ptr<DeviceType> device)
        : AutomaticDeviceAdapter<DeviceType>(device) {}
    
    // INDI-specific property management
    indi::PropertyVector getINDIProperty(const std::string& propertyName) const {
        json value = this->getProperty(propertyName, "INDI");
        return createINDIProperty(propertyName, value);
    }
    
    void setINDIProperty(const std::string& propertyName, const indi::PropertyVector& property) {
        json value = propertyToJson(property);
        this->setProperty(propertyName, value, "INDI");
    }
    
    // INDI message handling
    void processINDIMessage(const std::string& message) {
        // Parse INDI XML message and route to appropriate handler
        auto property = indi::PropertyVector::fromXML(message);
        setINDIProperty(property.name, property);
    }
    
    std::string generateINDIMessage(const std::string& propertyName) const {
        auto property = getINDIProperty(propertyName);
        return property.toXML().dump();
    }

private:
    indi::PropertyVector createINDIProperty(const std::string& name, const json& value) const;
    json propertyToJson(const indi::PropertyVector& property) const;
};

} // namespace automatic
} // namespace interfaces
} // namespace device
} // namespace hydrogen

#include "automatic_adapter.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <stdexcept>

namespace hydrogen {
namespace device {
namespace interfaces {
namespace automatic {

// AutomaticAdapterBase Implementation
void AutomaticAdapterBase::registerPropertyMapping(const PropertyMapping& mapping) {
    propertyMappings_[mapping.internalName] = mapping;
    
    // Build reverse lookup maps
    if (!mapping.ascomName.empty()) {
        ascomToInternal_[mapping.ascomName] = mapping.internalName;
        internalToAscom_[mapping.internalName] = mapping.ascomName;
    }
    
    if (!mapping.indiName.empty()) {
        indiToInternal_[mapping.indiName] = mapping.internalName;
        internalToIndi_[mapping.internalName] = mapping.indiName;
    }
    
    SPDLOG_DEBUG("Registered property mapping: {} -> ASCOM: {}, INDI: {}", 
                mapping.internalName, mapping.ascomName, mapping.indiName);
}

void AutomaticAdapterBase::registerMethodMapping(const MethodMapping& mapping) {
    methodMappings_[mapping.internalMethod] = mapping;
    
    SPDLOG_DEBUG("Registered method mapping: {} -> ASCOM: {}, INDI: {}", 
                mapping.internalMethod, mapping.ascomMethod, mapping.indiMethod);
}

json AutomaticAdapterBase::getProperty(const std::string& protocolName, const std::string& protocol) const {
    auto* mapping = const_cast<AutomaticAdapterBase*>(this)->findPropertyMapping(protocolName, protocol);
    if (!mapping) {
        throw std::invalid_argument("Property not found: " + protocolName);
    }
    
    try {
        json internalValue = getInternalProperty(mapping->internalName);
        
        // Apply transformation if specified
        if (mapping->transformer) {
            return mapping->transformer(internalValue);
        }
        
        return internalValue;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to get property {}: {}", protocolName, e.what());
        throw;
    }
}

bool AutomaticAdapterBase::setProperty(const std::string& protocolName, const json& value, const std::string& protocol) {
    auto* mapping = findPropertyMapping(protocolName, protocol);
    if (!mapping) {
        SPDLOG_ERROR("Property not found: {}", protocolName);
        return false;
    }
    
    if (mapping->readOnly) {
        SPDLOG_ERROR("Attempt to set read-only property: {}", protocolName);
        return false;
    }
    
    try {
        // Validate value if validator is specified
        if (mapping->validator && !mapping->validator(value).get<bool>()) {
            SPDLOG_ERROR("Property validation failed for {}: {}", protocolName, value.dump());
            return false;
        }
        
        // Apply transformation if specified
        json transformedValue = value;
        if (mapping->transformer) {
            transformedValue = mapping->transformer(value);
        }
        
        return setInternalProperty(mapping->internalName, transformedValue);
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to set property {}: {}", protocolName, e.what());
        return false;
    }
}

json AutomaticAdapterBase::invokeMethod(const std::string& protocolMethod, const std::vector<json>& parameters, const std::string& protocol) {
    auto* mapping = findMethodMapping(protocolMethod, protocol);
    if (!mapping) {
        throw std::invalid_argument("Method not found: " + protocolMethod);
    }
    
    try {
        // Transform parameters if transformer is specified
        std::vector<json> transformedParams = parameters;
        if (mapping->parameterTransformer) {
            json paramArray = json::array();
            for (const auto& param : parameters) {
                paramArray.push_back(param);
            }
            json transformed = mapping->parameterTransformer(paramArray);
            transformedParams.clear();
            for (const auto& param : transformed) {
                transformedParams.push_back(param);
            }
        }
        
        // Invoke internal method
        json result = invokeInternalMethod(mapping->internalMethod, transformedParams);
        
        // Transform result if transformer is specified
        if (mapping->resultTransformer) {
            return mapping->resultTransformer(result);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to invoke method {}: {}", protocolMethod, e.what());
        throw;
    }
}

std::string AutomaticAdapterBase::translateError(const std::exception& e, const std::string& protocol) const {
    std::string errorMessage = e.what();
    
    if (protocol == "ASCOM") {
        // Translate to ASCOM error codes and messages
        if (errorMessage.find("invalid_argument") != std::string::npos) {
            return "0x80040005: Invalid parameter value";
        } else if (errorMessage.find("runtime_error") != std::string::npos) {
            return "0x80040004: Operation failed";
        } else if (errorMessage.find("not_connected") != std::string::npos) {
            return "0x80040007: Device not connected";
        } else {
            return "0x80040001: Unspecified error - " + errorMessage;
        }
    } else if (protocol == "INDI") {
        // Translate to INDI alert messages
        return "Alert: " + errorMessage;
    }
    
    return errorMessage;
}

PropertyMapping* AutomaticAdapterBase::findPropertyMapping(const std::string& protocolName, const std::string& protocol) {
    std::string internalName;
    
    if (protocol == "ASCOM") {
        auto it = ascomToInternal_.find(protocolName);
        if (it != ascomToInternal_.end()) {
            internalName = it->second;
        }
    } else if (protocol == "INDI") {
        auto it = indiToInternal_.find(protocolName);
        if (it != indiToInternal_.end()) {
            internalName = it->second;
        }
    } else {
        internalName = protocolName; // Assume internal name
    }
    
    auto it = propertyMappings_.find(internalName);
    return (it != propertyMappings_.end()) ? &it->second : nullptr;
}

MethodMapping* AutomaticAdapterBase::findMethodMapping(const std::string& protocolMethod, const std::string& protocol) {
    // Find method mapping by protocol-specific method name
    for (auto& [internalMethod, mapping] : methodMappings_) {
        if (protocol == "ASCOM" && mapping.ascomMethod == protocolMethod) {
            return &mapping;
        } else if (protocol == "INDI" && mapping.indiMethod == protocolMethod) {
            return &mapping;
        } else if (mapping.internalMethod == protocolMethod) {
            return &mapping;
        }
    }
    
    return nullptr;
}

// Template specializations for common device types
template<>
void AutomaticDeviceAdapter<ICamera>::initializeStandardMappings() {
    // Camera property mappings
    registerPropertyMapping(PropertyMapping("currentBrightness", "Gain", "CCD_GAIN", "int"));
    registerPropertyMapping(PropertyMapping("exposureDuration", "ExposureDuration", "CCD_EXPOSURE", "double"));
    registerPropertyMapping(PropertyMapping("temperature", "CCDTemperature", "CCD_TEMPERATURE", "double", true));
    registerPropertyMapping(PropertyMapping("coolerOn", "CoolerOn", "CCD_COOLER", "bool"));
    registerPropertyMapping(PropertyMapping("binX", "BinX", "CCD_BINNING", "int"));
    registerPropertyMapping(PropertyMapping("binY", "BinY", "CCD_BINNING", "int"));
    registerPropertyMapping(PropertyMapping("startX", "StartX", "CCD_FRAME", "int"));
    registerPropertyMapping(PropertyMapping("startY", "StartY", "CCD_FRAME", "int"));
    registerPropertyMapping(PropertyMapping("numX", "NumX", "CCD_FRAME", "int"));
    registerPropertyMapping(PropertyMapping("numY", "NumY", "CCD_FRAME", "int"));
    
    // Camera method mappings
    registerMethodMapping(MethodMapping("START_EXPOSURE", "StartExposure", "startExposure", {"double", "bool"}, "void"));
    registerMethodMapping(MethodMapping("ABORT_EXPOSURE", "AbortExposure", "abortExposure", {}, "void"));
    registerMethodMapping(MethodMapping("STOP_EXPOSURE", "StopExposure", "stopExposure", {}, "void"));
}

template<>
void AutomaticDeviceAdapter<ITelescope>::initializeStandardMappings() {
    // Telescope property mappings
    registerPropertyMapping(PropertyMapping("rightAscension", "RightAscension", "EQUATORIAL_EOD_COORD", "double", true));
    registerPropertyMapping(PropertyMapping("declination", "Declination", "EQUATORIAL_EOD_COORD", "double", true));
    registerPropertyMapping(PropertyMapping("altitude", "Altitude", "HORIZONTAL_COORD", "double", true));
    registerPropertyMapping(PropertyMapping("azimuth", "Azimuth", "HORIZONTAL_COORD", "double", true));
    registerPropertyMapping(PropertyMapping("tracking", "Tracking", "TELESCOPE_TRACK_STATE", "bool"));
    registerPropertyMapping(PropertyMapping("slewing", "Slewing", "TELESCOPE_MOTION_NS", "bool", true));
    registerPropertyMapping(PropertyMapping("parked", "AtPark", "TELESCOPE_PARK", "bool", true));
    registerPropertyMapping(PropertyMapping("targetRA", "TargetRightAscension", "EQUATORIAL_EOD_COORD", "double"));
    registerPropertyMapping(PropertyMapping("targetDec", "TargetDeclination", "EQUATORIAL_EOD_COORD", "double"));
    
    // Telescope method mappings
    registerMethodMapping(MethodMapping("SLEW_TO_COORDINATES", "SlewToCoordinates", "slewToCoordinates", {"double", "double"}, "void"));
    registerMethodMapping(MethodMapping("SYNC_TO_COORDINATES", "SyncToCoordinates", "syncToCoordinates", {"double", "double"}, "void"));
    registerMethodMapping(MethodMapping("ABORT_SLEW", "AbortSlew", "abortSlew", {}, "void"));
    registerMethodMapping(MethodMapping("PARK", "Park", "park", {}, "void"));
    registerMethodMapping(MethodMapping("UNPARK", "Unpark", "unpark", {}, "void"));
    registerMethodMapping(MethodMapping("FIND_HOME", "FindHome", "findHome", {}, "void"));
}

template<>
void AutomaticDeviceAdapter<IFocuser>::initializeStandardMappings() {
    // Focuser property mappings
    registerPropertyMapping(PropertyMapping("position", "Position", "ABS_FOCUS_POSITION", "int", true));
    registerPropertyMapping(PropertyMapping("temperature", "Temperature", "FOCUS_TEMPERATURE", "double", true));
    registerPropertyMapping(PropertyMapping("isMoving", "IsMoving", "FOCUS_MOTION", "bool", true));
    registerPropertyMapping(PropertyMapping("tempComp", "TempComp", "FOCUS_TEMPERATURE", "bool"));
    
    // Focuser method mappings
    registerMethodMapping(MethodMapping("MOVE_TO_POSITION", "Move", "moveToPosition", {"int"}, "void"));
    registerMethodMapping(MethodMapping("MOVE_RELATIVE", "MoveRelative", "moveRelative", {"int"}, "void"));
    registerMethodMapping(MethodMapping("HALT", "Halt", "halt", {}, "void"));
}

// Explicit template instantiations
template class AutomaticDeviceAdapter<ICamera>;
template class AutomaticDeviceAdapter<ITelescope>;
template class AutomaticDeviceAdapter<IFocuser>;
template class ASCOMAutomaticAdapter<ICamera>;
template class ASCOMAutomaticAdapter<ITelescope>;
template class ASCOMAutomaticAdapter<IFocuser>;
template class INDIAutomaticAdapter<ICamera>;
template class INDIAutomaticAdapter<ITelescope>;
template class INDIAutomaticAdapter<IFocuser>;

} // namespace automatic
} // namespace interfaces
} // namespace device
} // namespace hydrogen

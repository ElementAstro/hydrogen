#pragma once

#include "automatic_adapter.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#endif

namespace hydrogen {
namespace device {
namespace interfaces {
namespace ascom {

/**
 * @brief ASCOM error codes following ASCOM standard
 */
enum class ASCOMErrorCode : uint32_t {
    OK = 0x00000000,
    UNSPECIFIED_ERROR = 0x80040001,
    INVALID_VALUE = 0x80040002,
    VALUE_NOT_SET = 0x80040003,
    NOT_CONNECTED = 0x80040007,
    INVALID_WHILE_PARKED = 0x80040008,
    INVALID_WHILE_SLAVED = 0x80040009,
    SETTINGS_PROVIDER_ERROR = 0x8004000A,
    INVALID_OPERATION = 0x8004000B,
    ACTION_NOT_IMPLEMENTED = 0x8004000C
};

/**
 * @brief ASCOM device types following ASCOM standard
 */
enum class ASCOMDeviceType {
    TELESCOPE = 0,
    CAMERA = 1,
    FOCUSER = 2,
    ROTATOR = 3,
    FILTER_WHEEL = 4,
    DOME = 5,
    SWITCH = 6,
    SAFETY_MONITOR = 7,
    COVER_CALIBRATOR = 8,
    OBSERVING_CONDITIONS = 9
};

/**
 * @brief ASCOM COM interface wrapper for automatic device registration
 */
class ASCOMCOMInterface {
public:
    virtual ~ASCOMCOMInterface() = default;
    
    // IDispatch interface methods
    virtual HRESULT GetTypeInfoCount(UINT* pctinfo) = 0;
    virtual HRESULT GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) = 0;
    virtual HRESULT GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) = 0;
    virtual HRESULT Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, 
                          DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr) = 0;
    
    // ASCOM standard properties and methods
    virtual BSTR get_Name() = 0;
    virtual BSTR get_Description() = 0;
    virtual BSTR get_DriverInfo() = 0;
    virtual BSTR get_DriverVersion() = 0;
    virtual short get_InterfaceVersion() = 0;
    virtual VARIANT_BOOL get_Connected() = 0;
    virtual void put_Connected(VARIANT_BOOL value) = 0;
    virtual BSTR Action(BSTR ActionName, BSTR ActionParameters) = 0;
    virtual void CommandBlind(BSTR Command, VARIANT_BOOL Raw) = 0;
    virtual VARIANT_BOOL CommandBool(BSTR Command, VARIANT_BOOL Raw) = 0;
    virtual BSTR CommandString(BSTR Command, VARIANT_BOOL Raw) = 0;
    virtual void SetupDialog() = 0;
};

/**
 * @brief Automatic ASCOM COM bridge for seamless device integration
 */
template<typename DeviceType>
class ASCOMCOMBridge : public ASCOMCOMInterface {
public:
    explicit ASCOMCOMBridge(std::shared_ptr<automatic::ASCOMAutomaticAdapter<DeviceType>> adapter)
        : adapter_(adapter), refCount_(1) {
        initializeCOMInterface();
    }
    
    virtual ~ASCOMCOMBridge() = default;
    
    // IUnknown interface
    HRESULT QueryInterface(REFIID riid, void** ppvObject) {
        if (riid == IID_IUnknown || riid == IID_IDispatch) {
            *ppvObject = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    
    ULONG AddRef() {
        return InterlockedIncrement(&refCount_);
    }
    
    ULONG Release() {
        ULONG count = InterlockedDecrement(&refCount_);
        if (count == 0) {
            delete this;
        }
        return count;
    }
    
    // IDispatch interface implementation
    HRESULT GetTypeInfoCount(UINT* pctinfo) override {
        *pctinfo = 1;
        return S_OK;
    }
    
    HRESULT GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override {
        // In real implementation, would load type library
        *ppTInfo = nullptr;
        return E_NOTIMPL;
    }
    
    HRESULT GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) override {
        // Map method names to dispatch IDs
        for (UINT i = 0; i < cNames; i++) {
            std::wstring name(rgszNames[i]);
            auto it = dispIdMap_.find(name);
            if (it != dispIdMap_.end()) {
                rgDispId[i] = it->second;
            } else {
                rgDispId[i] = DISPID_UNKNOWN;
                return DISP_E_UNKNOWNNAME;
            }
        }
        return S_OK;
    }
    
    HRESULT Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, 
                  DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr) override {
        try {
            return invokeMethod(dispIdMember, wFlags, pDispParams, pVarResult);
        } catch (const std::exception& e) {
            if (pExcepInfo) {
                pExcepInfo->wCode = static_cast<WORD>(ASCOMErrorCode::UNSPECIFIED_ERROR);
                pExcepInfo->bstrDescription = SysAllocString(L"Internal error occurred");
            }
            return DISP_E_EXCEPTION;
        }
    }
    
    // ASCOM standard interface implementation
    BSTR get_Name() override {
        try {
            std::string name = adapter_->getASCOMProperty<std::string>("Name");
            return SysAllocString(std::wstring(name.begin(), name.end()).c_str());
        } catch (const std::exception& e) {
            adapter_->handleASCOMException(e);
            return SysAllocString(L"Unknown Device");
        }
    }
    
    BSTR get_Description() override {
        try {
            std::string desc = adapter_->getASCOMProperty<std::string>("Description");
            return SysAllocString(std::wstring(desc.begin(), desc.end()).c_str());
        } catch (const std::exception& e) {
            adapter_->handleASCOMException(e);
            return SysAllocString(L"ASCOM Device");
        }
    }
    
    BSTR get_DriverInfo() override {
        try {
            std::string info = adapter_->getASCOMProperty<std::string>("DriverInfo");
            return SysAllocString(std::wstring(info.begin(), info.end()).c_str());
        } catch (const std::exception& e) {
            adapter_->handleASCOMException(e);
            return SysAllocString(L"Hydrogen ASCOM Driver v1.0");
        }
    }
    
    BSTR get_DriverVersion() override {
        return SysAllocString(L"1.0.0");
    }
    
    short get_InterfaceVersion() override {
        return 4; // ASCOM Interface Version 4
    }
    
    VARIANT_BOOL get_Connected() override {
        try {
            bool connected = adapter_->getASCOMProperty<bool>("Connected");
            return connected ? VARIANT_TRUE : VARIANT_FALSE;
        } catch (const std::exception& e) {
            adapter_->handleASCOMException(e);
            return VARIANT_FALSE;
        }
    }
    
    void put_Connected(VARIANT_BOOL value) override {
        try {
            adapter_->setASCOMProperty("Connected", value == VARIANT_TRUE);
        } catch (const std::exception& e) {
            adapter_->handleASCOMException(e);
        }
    }
    
    BSTR Action(BSTR ActionName, BSTR ActionParameters) override {
        try {
            std::wstring actionName(ActionName);
            std::wstring actionParams(ActionParameters);
            
            std::string result = adapter_->invokeASCOMMethod<std::string>(
                "Action", 
                std::string(actionName.begin(), actionName.end()),
                std::string(actionParams.begin(), actionParams.end())
            );
            
            return SysAllocString(std::wstring(result.begin(), result.end()).c_str());
        } catch (const std::exception& e) {
            adapter_->handleASCOMException(e);
            return SysAllocString(L"");
        }
    }
    
    void CommandBlind(BSTR Command, VARIANT_BOOL Raw) override {
        try {
            std::wstring command(Command);
            adapter_->invokeASCOMMethod<void>(
                "CommandBlind",
                std::string(command.begin(), command.end()),
                Raw == VARIANT_TRUE
            );
        } catch (const std::exception& e) {
            adapter_->handleASCOMException(e);
        }
    }
    
    VARIANT_BOOL CommandBool(BSTR Command, VARIANT_BOOL Raw) override {
        try {
            std::wstring command(Command);
            bool result = adapter_->invokeASCOMMethod<bool>(
                "CommandBool",
                std::string(command.begin(), command.end()),
                Raw == VARIANT_TRUE
            );
            return result ? VARIANT_TRUE : VARIANT_FALSE;
        } catch (const std::exception& e) {
            adapter_->handleASCOMException(e);
            return VARIANT_FALSE;
        }
    }
    
    BSTR CommandString(BSTR Command, VARIANT_BOOL Raw) override {
        try {
            std::wstring command(Command);
            std::string result = adapter_->invokeASCOMMethod<std::string>(
                "CommandString",
                std::string(command.begin(), command.end()),
                Raw == VARIANT_TRUE
            );
            return SysAllocString(std::wstring(result.begin(), result.end()).c_str());
        } catch (const std::exception& e) {
            adapter_->handleASCOMException(e);
            return SysAllocString(L"");
        }
    }
    
    void SetupDialog() override {
        try {
            adapter_->invokeASCOMMethod<void>("SetupDialog");
        } catch (const std::exception& e) {
            adapter_->handleASCOMException(e);
        }
    }

private:
    std::shared_ptr<automatic::ASCOMAutomaticAdapter<DeviceType>> adapter_;
    LONG refCount_;
    std::unordered_map<std::wstring, DISPID> dispIdMap_;
    
    void initializeCOMInterface() {
        // Initialize dispatch ID mappings
        dispIdMap_[L"Name"] = 1;
        dispIdMap_[L"Description"] = 2;
        dispIdMap_[L"DriverInfo"] = 3;
        dispIdMap_[L"DriverVersion"] = 4;
        dispIdMap_[L"InterfaceVersion"] = 5;
        dispIdMap_[L"Connected"] = 6;
        dispIdMap_[L"Action"] = 7;
        dispIdMap_[L"CommandBlind"] = 8;
        dispIdMap_[L"CommandBool"] = 9;
        dispIdMap_[L"CommandString"] = 10;
        dispIdMap_[L"SetupDialog"] = 11;
    }
    
    HRESULT invokeMethod(DISPID dispId, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult) {
        // Route method calls based on dispatch ID
        switch (dispId) {
            case 1: // Name
                if (wFlags & DISPATCH_PROPERTYGET) {
                    V_VT(pVarResult) = VT_BSTR;
                    V_BSTR(pVarResult) = get_Name();
                }
                break;
            case 6: // Connected
                if (wFlags & DISPATCH_PROPERTYGET) {
                    V_VT(pVarResult) = VT_BOOL;
                    V_BOOL(pVarResult) = get_Connected();
                } else if (wFlags & DISPATCH_PROPERTYPUT) {
                    put_Connected(pDispParams->rgvarg[0].boolVal);
                }
                break;
            // Add more cases for other methods...
            default:
                return DISP_E_MEMBERNOTFOUND;
        }
        return S_OK;
    }
};

/**
 * @brief ASCOM device registry for automatic device discovery
 */
class ASCOMDeviceRegistry {
public:
    static ASCOMDeviceRegistry& getInstance() {
        static ASCOMDeviceRegistry instance;
        return instance;
    }
    
    // Register device with ASCOM
    template<typename DeviceType>
    void registerDevice(const std::string& deviceId, std::shared_ptr<DeviceType> device, ASCOMDeviceType deviceType) {
        auto adapter = std::make_shared<automatic::ASCOMAutomaticAdapter<DeviceType>>(device);
        auto bridge = std::make_shared<ASCOMCOMBridge<DeviceType>>(adapter);
        
        std::lock_guard<std::mutex> lock(registryMutex_);
        registeredDevices_[deviceId] = {adapter, bridge, deviceType};
        
        // Register with Windows COM system
        registerWithCOM(deviceId, deviceType);
        
        SPDLOG_INFO("Registered ASCOM device: {} (type: {})", deviceId, static_cast<int>(deviceType));
    }
    
    // Unregister device from ASCOM
    void unregisterDevice(const std::string& deviceId) {
        std::lock_guard<std::mutex> lock(registryMutex_);
        auto it = registeredDevices_.find(deviceId);
        if (it != registeredDevices_.end()) {
            unregisterFromCOM(deviceId);
            registeredDevices_.erase(it);
            SPDLOG_INFO("Unregistered ASCOM device: {}", deviceId);
        }
    }
    
    // Get registered devices
    std::vector<std::string> getRegisteredDevices() const {
        std::lock_guard<std::mutex> lock(registryMutex_);
        std::vector<std::string> devices;
        for (const auto& [deviceId, info] : registeredDevices_) {
            devices.push_back(deviceId);
        }
        return devices;
    }

private:
    struct DeviceInfo {
        std::shared_ptr<void> adapter;
        std::shared_ptr<void> bridge;
        ASCOMDeviceType deviceType;
    };
    
    mutable std::mutex registryMutex_;
    std::unordered_map<std::string, DeviceInfo> registeredDevices_;
    
    void registerWithCOM(const std::string& deviceId, ASCOMDeviceType deviceType) {
        // In real implementation, would register with Windows COM system
        // This involves creating registry entries and class factories
        SPDLOG_DEBUG("Registering {} with COM system", deviceId);
    }
    
    void unregisterFromCOM(const std::string& deviceId) {
        // In real implementation, would unregister from Windows COM system
        SPDLOG_DEBUG("Unregistering {} from COM system", deviceId);
    }
};

} // namespace ascom
} // namespace interfaces
} // namespace device
} // namespace hydrogen

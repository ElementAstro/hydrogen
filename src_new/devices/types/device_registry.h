#pragma once

#include "core/modern_device_base.h"
#include "focuser.h"
#include "camera.h"
#include "telescope.h"
#include "filter_wheel.h"
#include "rotator.h"
#include "guider.h"
#include "switch.h"
#include "dome.h"
#include "cover_calibrator.h"
#include "observing_conditions.h"
#include "safety_monitor.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>

namespace hydrogen {
namespace device {

/**
 * @brief 设备注册�?- 管理所有设备工厂和实例
 * 
 * 提供统一的设备创建、管理和发现机制
 */
class DeviceRegistry {
public:
    /**
     * @brief 获取单例实例
     */
    static DeviceRegistry& getInstance();

    /**
     * @brief 析构函数
     */
    ~DeviceRegistry();

    /**
     * @brief 注册设备工厂
     * @param deviceType 设备类型
     * @param factory 设备工厂
     * @return 注册是否成功
     */
    bool registerDeviceFactory(const std::string& deviceType, 
                              std::unique_ptr<core::DeviceFactory> factory);

    /**
     * @brief 注销设备工厂
     * @param deviceType 设备类型
     * @return 注销是否成功
     */
    bool unregisterDeviceFactory(const std::string& deviceType);

    /**
     * @brief 创建设备实例
     * @param deviceType 设备类型
     * @param deviceId 设备ID
     * @param manufacturer 制造商（可选）
     * @param model 型号（可选）
     * @return 设备实例，失败返回nullptr
     */
    std::unique_ptr<core::ModernDeviceBase> createDevice(const std::string& deviceType,
                                                        const std::string& deviceId,
                                                        const std::string& manufacturer = "",
                                                        const std::string& model = "");

    /**
     * @brief 获取支持的设备类型列�?     * @return 设备类型列表
     */
    std::vector<std::string> getSupportedDeviceTypes() const;

    /**
     * @brief 获取指定设备类型支持的制造商列表
     * @param deviceType 设备类型
     * @return 制造商列表
     */
    std::vector<std::string> getSupportedManufacturers(const std::string& deviceType) const;

    /**
     * @brief 获取指定制造商支持的型号列�?     * @param deviceType 设备类型
     * @param manufacturer 制造商
     * @return 型号列表
     */
    std::vector<std::string> getSupportedModels(const std::string& deviceType, 
                                               const std::string& manufacturer) const;

    /**
     * @brief 注册设备实例
     * @param device 设备实例
     * @return 注册是否成功
     */
    bool registerDeviceInstance(std::shared_ptr<core::ModernDeviceBase> device);

    /**
     * @brief 注销设备实例
     * @param deviceId 设备ID
     * @return 注销是否成功
     */
    bool unregisterDeviceInstance(const std::string& deviceId);

    /**
     * @brief 获取设备实例
     * @param deviceId 设备ID
     * @return 设备实例，不存在返回nullptr
     */
    std::shared_ptr<core::ModernDeviceBase> getDeviceInstance(const std::string& deviceId);

    /**
     * @brief 获取所有设备实�?     * @return 设备实例映射
     */
    std::unordered_map<std::string, std::shared_ptr<core::ModernDeviceBase>> getAllDeviceInstances() const;

    /**
     * @brief 获取指定类型的设备实�?     * @param deviceType 设备类型
     * @return 设备实例列表
     */
    std::vector<std::shared_ptr<core::ModernDeviceBase>> getDeviceInstancesByType(const std::string& deviceType) const;

    /**
     * @brief 初始化所有设�?     * @return 成功初始化的设备数量
     */
    size_t initializeAllDevices();

    /**
     * @brief 启动所有设�?     * @return 成功启动的设备数�?     */
    size_t startAllDevices();

    /**
     * @brief 停止所有设�?     */
    void stopAllDevices();

    /**
     * @brief 断开所有设备连�?     */
    void disconnectAllDevices();

    /**
     * @brief 获取设备统计信息
     * @return 统计信息JSON
     */
    json getDeviceStatistics() const;

    /**
     * @brief 导出设备配置
     * @param filename 文件�?     * @return 导出是否成功
     */
    bool exportDeviceConfigurations(const std::string& filename) const;

    /**
     * @brief 导入设备配置
     * @param filename 文件�?     * @return 导入是否成功
     */
    bool importDeviceConfigurations(const std::string& filename);

    /**
     * @brief 注册默认设备工厂
     */
    void registerDefaultFactories();

private:
    /**
     * @brief 私有构造函数（单例模式�?     */
    DeviceRegistry();

    /**
     * @brief 禁用拷贝构造和赋�?     */
    DeviceRegistry(const DeviceRegistry&) = delete;
    DeviceRegistry& operator=(const DeviceRegistry&) = delete;

private:
    // 设备工厂管理
    mutable std::mutex factoriesMutex_;
    std::unordered_map<std::string, std::unique_ptr<core::DeviceFactory>> deviceFactories_;

    // 设备实例管理
    mutable std::mutex instancesMutex_;
    std::unordered_map<std::string, std::shared_ptr<core::ModernDeviceBase>> deviceInstances_;
};

/**
 * @brief 设备创建辅助函数
 */
namespace DeviceCreator {

/**
 * @brief 创建调焦�? * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 调焦器实�? */
inline std::unique_ptr<Focuser> createFocuser(const std::string& deviceId,
                                              const std::string& manufacturer = "ZWO",
                                              const std::string& model = "EAF") {
    return std::make_unique<Focuser>(deviceId, manufacturer, model);
}

/**
 * @brief 创建圆顶
 * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 圆顶实例
 */
inline std::unique_ptr<Dome> createDome(const std::string& deviceId,
                                        const std::string& manufacturer = "Generic",
                                        const std::string& model = "Dome") {
    return std::make_unique<Dome>(deviceId, manufacturer, model);
}

/**
 * @brief 创建遮光罩校准器
 * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 遮光罩校准器实例
 */
inline std::unique_ptr<CoverCalibrator> createCoverCalibrator(const std::string& deviceId,
                                                              const std::string& manufacturer = "Generic",
                                                              const std::string& model = "CoverCalibrator") {
    return std::make_unique<CoverCalibrator>(deviceId, manufacturer, model);
}

/**
 * @brief 创建观测条件监测�? * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 观测条件监测器实�? */
inline std::unique_ptr<ObservingConditions> createObservingConditions(const std::string& deviceId,
                                                                       const std::string& manufacturer = "Generic",
                                                                       const std::string& model = "WeatherStation") {
    return std::make_unique<ObservingConditions>(deviceId, manufacturer, model);
}

/**
 * @brief 创建安全监控�? * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 安全监控器实�? */
inline std::unique_ptr<SafetyMonitor> createSafetyMonitor(const std::string& deviceId,
                                                          const std::string& manufacturer = "Generic",
                                                          const std::string& model = "SafetyMonitor") {
    return std::make_unique<SafetyMonitor>(deviceId, manufacturer, model);
}

/**
 * @brief 创建相机
 * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 相机实例
 */
inline std::unique_ptr<Camera> createCamera(const std::string& deviceId,
                                           const std::string& manufacturer = "ZWO",
                                           const std::string& model = "ASI294MC") {
    return std::make_unique<Camera>(deviceId, manufacturer, model);
}

/**
 * @brief 创建望远�? * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 望远镜实�? */
inline std::unique_ptr<Telescope> createTelescope(const std::string& deviceId,
                                                  const std::string& manufacturer = "Celestron",
                                                  const std::string& model = "NexStar Evolution") {
    return std::make_unique<Telescope>(deviceId, manufacturer, model);
}

/**
 * @brief 创建滤镜�? * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 滤镜轮实�? */
inline std::unique_ptr<FilterWheel> createFilterWheel(const std::string& deviceId,
                                                      const std::string& manufacturer = "ZWO",
                                                      const std::string& model = "EFW") {
    return std::make_unique<FilterWheel>(deviceId, manufacturer, model);
}

/**
 * @brief 创建旋转�? * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 旋转器实�? */
inline std::unique_ptr<Rotator> createRotator(const std::string& deviceId,
                                              const std::string& manufacturer = "Pegasus",
                                              const std::string& model = "FocusCube") {
    return std::make_unique<Rotator>(deviceId, manufacturer, model);
}

/**
 * @brief 创建导星�? * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 导星器实�? */
inline std::unique_ptr<Guider> createGuider(const std::string& deviceId,
                                            const std::string& manufacturer = "ZWO",
                                            const std::string& model = "ASI120MM-Mini") {
    return std::make_unique<Guider>(deviceId, manufacturer, model);
}

/**
 * @brief 创建开�? * @param deviceId 设备ID
 * @param manufacturer 制造商
 * @param model 型号
 * @return 开关实�? */
inline std::unique_ptr<Switch> createSwitch(const std::string& deviceId,
                                            const std::string& manufacturer = "Pegasus",
                                            const std::string& model = "Ultimate Powerbox") {
    return std::make_unique<Switch>(deviceId, manufacturer, model);
}

/**
 * @brief 从配置创建设�? * @param config 设备配置JSON
 * @return 设备实例
 */
std::unique_ptr<core::ModernDeviceBase> createDeviceFromConfig(const json& config);

/**
 * @brief 批量创建设备
 * @param configs 设备配置列表
 * @return 设备实例列表
 */
std::vector<std::unique_ptr<core::ModernDeviceBase>> createDevicesFromConfigs(const std::vector<json>& configs);

} // namespace DeviceCreator

/**
 * @brief 设备管理器宏定义
 */
#define REGISTER_DEVICE_FACTORY(deviceType, factoryClass) \
    DeviceRegistry::getInstance().registerDeviceFactory(deviceType, std::make_unique<factoryClass>())

#define CREATE_DEVICE(deviceType, deviceId, manufacturer, model) \
    DeviceRegistry::getInstance().createDevice(deviceType, deviceId, manufacturer, model)

#define GET_DEVICE(deviceId) \
    DeviceRegistry::getInstance().getDeviceInstance(deviceId)

#define REGISTER_DEVICE(device) \
    DeviceRegistry::getInstance().registerDeviceInstance(device)

// 便捷�?#define CREATE_FOCUSER(deviceId, manufacturer, model) \
    DeviceCreator::createFocuser(deviceId, manufacturer, model)

#define CREATE_CAMERA(deviceId, manufacturer, model) \
    DeviceCreator::createCamera(deviceId, manufacturer, model)

#define CREATE_TELESCOPE(deviceId, manufacturer, model) \
    DeviceCreator::createTelescope(deviceId, manufacturer, model)

#define CREATE_FILTER_WHEEL(deviceId, manufacturer, model) \
    DeviceCreator::createFilterWheel(deviceId, manufacturer, model)

#define CREATE_DOME(deviceId, manufacturer, model) \
    DeviceCreator::createDome(deviceId, manufacturer, model)

#define CREATE_COVER_CALIBRATOR(deviceId, manufacturer, model) \
    DeviceCreator::createCoverCalibrator(deviceId, manufacturer, model)

#define CREATE_OBSERVING_CONDITIONS(deviceId, manufacturer, model) \
    DeviceCreator::createObservingConditions(deviceId, manufacturer, model)

#define CREATE_SAFETY_MONITOR(deviceId, manufacturer, model) \
    DeviceCreator::createSafetyMonitor(deviceId, manufacturer, model)

#define CREATE_ROTATOR(deviceId, manufacturer, model) \
    DeviceCreator::createRotator(deviceId, manufacturer, model)

#define CREATE_GUIDER(deviceId, manufacturer, model) \
    DeviceCreator::createGuider(deviceId, manufacturer, model)

#define CREATE_SWITCH(deviceId, manufacturer, model) \
    DeviceCreator::createSwitch(deviceId, manufacturer, model)

} // namespace device
} // namespace hydrogen

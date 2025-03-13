#pragma once
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace astrocomm {

using json = nlohmann::json;

class DeviceManager {
public:
  DeviceManager();
  ~DeviceManager();

  // 添加设备
  void addDevice(const std::string &deviceId, const json &deviceInfo);

  // 更新设备信息
  void updateDevice(const std::string &deviceId, const json &deviceInfo);

  // 移除设备
  void removeDevice(const std::string &deviceId);

  // 获取设备信息
  json getDeviceInfo(const std::string &deviceId) const;

  // 获取设备列表（可按类型过滤）
  json getDevices(const std::vector<std::string> &deviceTypes = {}) const;

  // 检查设备是否存在
  bool deviceExists(const std::string &deviceId) const;

  // 获取设备属性值
  json getDeviceProperty(const std::string &deviceId,
                         const std::string &property) const;

  // 更新设备属性
  void updateDeviceProperty(const std::string &deviceId,
                            const std::string &property, const json &value);

  // 将设备设置持久化到文件
  bool saveDeviceConfiguration(const std::string &filePath) const;

  // 从文件加载设备设置
  bool loadDeviceConfiguration(const std::string &filePath);

private:
  mutable std::mutex devicesMutex;
  std::unordered_map<std::string, json> devices;
};

} // namespace astrocomm
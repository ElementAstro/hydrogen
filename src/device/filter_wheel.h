#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"
#include "behaviors/movable_behavior.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace astrocomm {
namespace device {

using json = nlohmann::json;

/**
 * @brief Filter information structure
 */
struct FilterInfo {
  int position;                    // Filter position (0-based)
  std::string name;               // Filter name
  std::string type;               // Filter type (e.g., "Luminance", "Red", "Green", "Blue")
  double wavelength;              // Central wavelength (nm)
  double bandwidth;               // Bandwidth (nm)
  double exposureFactor;          // Exposure factor relative to luminance
  std::string description;        // Description
};

/**
 * @brief 滤镜轮设备实现
 *
 * 基于新架构的滤镜轮实现，使用MovableBehavior提供移动控制功能。
 * 支持多种制造商的滤镜轮设备，提供统一的控制接口。
 */
class FilterWheel : public core::ModernDeviceBase, 
                    public interfaces::IFilterWheel {
public:
  /**
   * @brief Constructor
   * @param deviceId Device identifier
   * @param manufacturer Manufacturer name
   * @param model Model name
   */
  FilterWheel(const std::string &deviceId, 
              const std::string &manufacturer = "ZWO",
              const std::string &model = "EFW");

  /**
   * @brief Virtual destructor
   */
  virtual ~FilterWheel();

  /**
   * @brief 获取设备类型名称
   */
  static std::string getDeviceTypeName() { return "FILTER_WHEEL"; }

  /**
   * @brief 获取支持的制造商列表
   */
  static std::vector<std::string> getSupportedManufacturers() {
    return {"ZWO", "QHY", "SBIG", "Atik", "Moravian", "Generic"};
  }

  /**
   * @brief 获取支持的型号列表
   */
  static std::vector<std::string> getSupportedModels(const std::string& manufacturer) {
    if (manufacturer == "ZWO") return {"EFW", "EFW-Mini", "EFW-7x36"};
    if (manufacturer == "QHY") return {"CFW2-US", "CFW3-US", "CFW3-L"};
    if (manufacturer == "SBIG") return {"CFW-8", "CFW-10", "CFW-L"};
    if (manufacturer == "Atik") return {"EFW2", "EFW3"};
    if (manufacturer == "Moravian") return {"G2-1600", "G3-16200"};
    return {"Generic Filter Wheel"};
  }

  // 实现IMovable接口（委托给MovableBehavior）
  bool moveToPosition(int position) override;
  bool moveRelative(int steps) override;
  bool stopMovement() override;
  bool home() override;
  int getCurrentPosition() const override;
  bool isMoving() const override;

  // 实现IFilterWheel特定接口
  int getFilterCount() const override;
  int getCurrentFilter() const override;
  bool setFilter(int position) override;
  std::string getFilterName(int position) const override;
  bool setFilterName(int position, const std::string& name) override;

  // ==== 向后兼容接口 ====

  /**
   * @brief Get number of filters (向后兼容)
   */
  virtual int getNumFilters() const;

  /**
   * @brief Set filter position (向后兼容)
   */
  virtual void setFilterPosition(int position);

  /**
   * @brief Get filter position (向后兼容)
   */
  virtual int getFilterPosition() const;

  // ==== 扩展功能接口 ====

  /**
   * @brief Set filter count
   */
  virtual bool setFilterCount(int count);

  /**
   * @brief Get filter information
   */
  virtual FilterInfo getFilterInfo(int position) const;

  /**
   * @brief Set filter information
   */
  virtual bool setFilterInfo(int position, const FilterInfo& info);

  /**
   * @brief Get all filter information
   */
  virtual std::vector<FilterInfo> getAllFilterInfo() const;

  /**
   * @brief Load filter configuration from file
   */
  virtual bool loadFilterConfiguration(const std::string& filename);

  /**
   * @brief Save filter configuration to file
   */
  virtual bool saveFilterConfiguration(const std::string& filename) const;

  /**
   * @brief Get filter by name
   */
  virtual int getFilterByName(const std::string& name) const;

  /**
   * @brief Set filter by name
   */
  virtual bool setFilterByName(const std::string& name);

  /**
   * @brief Get filter names
   */
  virtual std::vector<std::string> getFilterNames() const;

  /**
   * @brief Set default filter configuration
   */
  virtual bool setDefaultFilterConfiguration();

  /**
   * @brief Get filter wheel diameter
   */
  virtual double getWheelDiameter() const;

  /**
   * @brief Set filter wheel diameter
   */
  virtual bool setWheelDiameter(double diameter);

  /**
   * @brief Get filter thickness
   */
  virtual double getFilterThickness(int position) const;

  /**
   * @brief Set filter thickness
   */
  virtual bool setFilterThickness(int position, double thickness);

  /**
   * @brief Calculate focus offset for filter
   */
  virtual double calculateFocusOffset(int fromFilter, int toFilter) const;

  /**
   * @brief Wait for filter change to complete
   */
  bool waitForFilterChange(int timeoutMs = 0);

protected:
  // 重写基类方法
  bool initializeDevice() override;
  bool startDevice() override;
  void stopDevice() override;
  bool handleDeviceCommand(const std::string& command,
                          const json& parameters,
                          json& result) override;
  void updateDevice() override;

private:
  /**
   * @brief 初始化滤镜轮行为组件
   */
  void initializeFilterWheelBehaviors();

  /**
   * @brief 滤镜轮移动行为实现
   */
  class FilterWheelMovableBehavior : public behaviors::MovableBehavior {
  public:
    explicit FilterWheelMovableBehavior(FilterWheel* filterWheel);

  protected:
    bool executeMovement(int targetPosition) override;
    bool executeStop() override;
    bool executeHome() override;

  private:
    FilterWheel* filterWheel_;
  };

  // 硬件抽象接口
  virtual bool executeFilterChange(int position);
  virtual bool executeStop();
  virtual bool executeHome();
  virtual int readCurrentPosition();

  // 配置管理
  void initializeDefaultFilters();
  bool validateFilterPosition(int position) const;

private:
  // 行为组件指针
  FilterWheelMovableBehavior* movableBehavior_;

  // 滤镜轮参数
  std::atomic<int> filterCount_;
  std::atomic<double> wheelDiameter_;

  // 滤镜信息
  mutable std::mutex filterInfoMutex_;
  std::unordered_map<int, FilterInfo> filterInfo_;

  // 滤镜厚度信息（用于焦点偏移计算）
  mutable std::mutex thicknessMutex_;
  std::unordered_map<int, double> filterThickness_;

  // 移动完成条件变量
  mutable std::mutex filterChangeMutex_;
  std::condition_variable filterChangeCV_;
};

/**
 * @brief 滤镜轮工厂
 */
class FilterWheelFactory : public core::TypedDeviceFactory<FilterWheel> {
public:
  FilterWheelFactory(const std::string& manufacturer = "Generic", 
                     const std::string& model = "Filter Wheel")
      : TypedDeviceFactory<FilterWheel>(manufacturer, model) {}
};

} // namespace device
} // namespace astrocomm

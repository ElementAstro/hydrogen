#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"
#include "behaviors/movable_behavior.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

namespace astrocomm {
namespace device {

using json = nlohmann::json;

/**
 * @brief Rotation direction enumeration
 */
enum class RotationDirection {
  CLOCKWISE,        // Clockwise rotation
  COUNTERCLOCKWISE  // Counter-clockwise rotation
};

/**
 * @brief 旋转器设备实现
 *
 * 基于新架构的旋转器实现，使用MovableBehavior提供移动控制功能。
 * 支持多种制造商的旋转器设备，提供统一的控制接口。
 */
class Rotator : public core::ModernDeviceBase, 
                public interfaces::IRotator {
public:
  /**
   * @brief Constructor
   * @param deviceId Device identifier
   * @param manufacturer Manufacturer name
   * @param model Model name
   */
  Rotator(const std::string &deviceId, 
          const std::string &manufacturer = "Pegasus",
          const std::string &model = "FocusCube");

  /**
   * @brief Virtual destructor
   */
  virtual ~Rotator();

  /**
   * @brief 获取设备类型名称
   */
  static std::string getDeviceTypeName() { return "ROTATOR"; }

  /**
   * @brief 获取支持的制造商列表
   */
  static std::vector<std::string> getSupportedManufacturers() {
    return {"Pegasus", "Optec", "Moonlite", "Lakeside", "Generic"};
  }

  /**
   * @brief 获取支持的型号列表
   */
  static std::vector<std::string> getSupportedModels(const std::string& manufacturer) {
    if (manufacturer == "Pegasus") return {"FocusCube", "Falcon Rotator"};
    if (manufacturer == "Optec") return {"Gemini", "IFW"};
    if (manufacturer == "Moonlite") return {"NightCrawler Rotator"};
    if (manufacturer == "Lakeside") return {"Rotator"};
    return {"Generic Rotator"};
  }

  // 实现IMovable接口（委托给MovableBehavior）
  bool moveToPosition(int position) override;
  bool moveRelative(int steps) override;
  bool stopMovement() override;
  bool home() override;
  int getCurrentPosition() const override;
  bool isMoving() const override;

  // 实现IRotator特定接口
  double getCurrentAngle() const override;
  bool rotateToAngle(double angle) override;
  bool rotateRelative(double angle) override;
  bool supportsReverse() const override;
  bool setReverse(bool reversed) override;

  // ==== 向后兼容接口 ====

  /**
   * @brief Move to position (向后兼容)
   */
  virtual void moveToPosition(double position);

  /**
   * @brief Get position (向后兼容)
   */
  virtual double getPosition() const;

  /**
   * @brief Set position (向后兼容)
   */
  virtual void setPosition(double position);

  /**
   * @brief Sync position (向后兼容)
   */
  virtual void syncPosition(double position);

  /**
   * @brief Halt movement (向后兼容)
   */
  virtual void halt();

  /**
   * @brief Set reverse (向后兼容)
   */
  virtual void setReverse(bool reverse);

  /**
   * @brief Get reverse (向后兼容)
   */
  virtual bool getReverse() const;

  // ==== 扩展功能接口 ====

  /**
   * @brief Set rotation speed
   */
  virtual bool setRotationSpeed(double speed);

  /**
   * @brief Get rotation speed
   */
  virtual double getRotationSpeed() const;

  /**
   * @brief Set step size (degrees per step)
   */
  virtual bool setStepSize(double stepSize);

  /**
   * @brief Get step size
   */
  virtual double getStepSize() const;

  /**
   * @brief Set rotation limits
   */
  virtual bool setRotationLimits(double minAngle, double maxAngle);

  /**
   * @brief Get rotation limits
   */
  virtual void getRotationLimits(double& minAngle, double& maxAngle) const;

  /**
   * @brief Enable/disable rotation limits
   */
  virtual bool setLimitsEnabled(bool enabled);

  /**
   * @brief Check if rotation limits are enabled
   */
  virtual bool areLimitsEnabled() const;

  /**
   * @brief Calibrate rotator
   */
  virtual bool calibrate();

  /**
   * @brief Set zero position
   */
  virtual bool setZeroPosition();

  /**
   * @brief Get mechanical angle (raw position)
   */
  virtual double getMechanicalAngle() const;

  /**
   * @brief Set mechanical angle offset
   */
  virtual bool setMechanicalOffset(double offset);

  /**
   * @brief Get mechanical angle offset
   */
  virtual double getMechanicalOffset() const;

  /**
   * @brief Convert position to angle
   */
  virtual double positionToAngle(int position) const;

  /**
   * @brief Convert angle to position
   */
  virtual int angleToPosition(double angle) const;

  /**
   * @brief Get rotation direction for angle change
   */
  virtual RotationDirection getRotationDirection(double fromAngle, double toAngle) const;

  /**
   * @brief Wait for rotation to complete
   */
  bool waitForRotationComplete(int timeoutMs = 0);

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
   * @brief 初始化旋转器行为组件
   */
  void initializeRotatorBehaviors();

  /**
   * @brief 旋转器移动行为实现
   */
  class RotatorMovableBehavior : public behaviors::MovableBehavior {
  public:
    explicit RotatorMovableBehavior(Rotator* rotator);

  protected:
    bool executeMovement(int targetPosition) override;
    bool executeStop() override;
    bool executeHome() override;

  private:
    Rotator* rotator_;
  };

  // 硬件抽象接口
  virtual bool executeRotation(double targetAngle);
  virtual bool executeStop();
  virtual bool executeHome();
  virtual double readCurrentAngle();

  // 角度处理函数
  double normalizeAngle(double angle) const;
  bool validateAngle(double angle) const;
  double calculateShortestPath(double fromAngle, double toAngle) const;

private:
  // 行为组件指针
  RotatorMovableBehavior* movableBehavior_;

  // 旋转器参数
  std::atomic<double> rotationSpeed_;      // 旋转速度 (degrees/second)
  std::atomic<double> stepSize_;           // 步长 (degrees/step)
  std::atomic<double> mechanicalOffset_;   // 机械偏移角度
  std::atomic<bool> reversed_;             // 是否反向
  
  // 角度限制
  std::atomic<bool> limitsEnabled_;
  std::atomic<double> minAngle_;
  std::atomic<double> maxAngle_;
  
  // 当前状态
  std::atomic<double> currentAngle_;
  std::atomic<double> targetAngle_;
  
  // 旋转完成条件变量
  mutable std::mutex rotationCompleteMutex_;
  std::condition_variable rotationCompleteCV_;
};

/**
 * @brief 旋转器工厂
 */
class RotatorFactory : public core::TypedDeviceFactory<Rotator> {
public:
  RotatorFactory(const std::string& manufacturer = "Generic", 
                 const std::string& model = "Rotator")
      : TypedDeviceFactory<Rotator>(manufacturer, model) {}
};

} // namespace device
} // namespace astrocomm

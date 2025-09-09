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

namespace hydrogen {
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
 * @brief 旋转器设备实�? *
 * 基于新架构的旋转器实现，使用MovableBehavior提供移动控制功能�? * 支持多种制造商的旋转器设备，提供统一的控制接口�? */
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
   * @brief 获取支持的型号列�?   */
  static std::vector<std::string> getSupportedModels(const std::string& manufacturer) {
    if (manufacturer == "Pegasus") return {"FocusCube", "Falcon Rotator"};
    if (manufacturer == "Optec") return {"Gemini", "IFW"};
    if (manufacturer == "Moonlite") return {"NightCrawler Rotator"};
    if (manufacturer == "Lakeside") return {"Rotator"};
    return {"Generic Rotator"};
  }

  // 实现IMovable接口（委托给MovableBehavior�?  bool moveToPosition(int position) override;
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
  void setReverse(bool value) override;  // ASCOM standard (void return)
  bool setReverseMode(bool reversed) override;  // Legacy method (bool return)

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

  // Note: setReverse is already implemented above with bool return type

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

  // Additional methods needed by implementation
  bool moveToPosition(int position) override;
  bool setReversed(bool reversed);
  bool isReversed() const;
  std::vector<std::string> getCapabilities() const override;

  // Hardware abstraction interface (public for behavior classes)
  virtual bool executeRotation(double targetAngle);
  virtual bool executeStop();
  virtual bool executeHome();
  virtual double readCurrentAngle();

  // Missing IRotator interface methods
  double getMechanicalPosition() const override;
  double getTargetPosition() const override;
  void move(double position) override;
  void moveAbsolute(double position) override;
  void moveMechanical(double position) override;
  bool getIsMoving() const override;
  bool getCanReverse() const override;
  void sync(double position) override;

  // Missing IDevice interface methods
  std::string getName() const override;
  std::string getDescription() const override;
  std::string getDriverInfo() const override;
  std::string getDriverVersion() const override;
  int getInterfaceVersion() const override;
  std::vector<std::string> getSupportedActions() const override;
  bool isConnecting() const override;
  interfaces::DeviceState getDeviceState() const override;
  std::string action(const std::string& actionName, const std::string& actionParameters) override;
  void commandBlind(const std::string& command, bool raw) override;
  bool commandBool(const std::string& command, bool raw) override;
  std::string commandString(const std::string& command, bool raw) override;
  void setupDialog() override;

  // Device lifecycle methods
  virtual void run();  // Main device loop

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
  // Forward declaration and friend class
  class RotatorMovableBehavior;
  friend class RotatorMovableBehavior;
  /**
   * @brief 初始化旋转器行为组件
   */
  void initializeRotatorBehaviors();

  /**
   * @brief 旋转器移动行为实�?   */
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

  // Angle processing functions
  double normalizeAngle(double angle) const;
  bool validateAngle(double angle) const;
  double calculateShortestPath(double fromAngle, double toAngle) const;

private:
  // 行为组件指针
  RotatorMovableBehavior* movableBehavior_;

  // Rotator parameters
  std::atomic<double> rotationSpeed_;      // Rotation speed (degrees/second)
  std::atomic<double> stepSize_;           // Step size (degrees/step)
  std::atomic<double> mechanicalOffset_;   // Mechanical offset angle
  std::atomic<bool> reversed_;             // Whether reversed

  // Additional parameters needed by implementation
  std::atomic<double> maxSpeed_;           // Maximum speed
  std::atomic<double> currentSpeed_;       // Current speed
  std::atomic<bool> isReversed_;           // Reverse state
  std::atomic<bool> canReverse_;           // Can reverse capability
  
  // Angle limits
  std::atomic<bool> limitsEnabled_;
  std::atomic<double> minAngle_;
  std::atomic<double> maxAngle_;

  // Current state
  std::atomic<double> currentAngle_;
  std::atomic<double> targetAngle_;
  
  // 旋转完成条件变量
  mutable std::mutex rotationCompleteMutex_;
  std::condition_variable rotationCompleteCV_;
};

/**
 * @brief 旋转器工�? */
class RotatorFactory : public core::TypedDeviceFactory<Rotator> {
public:
  RotatorFactory(const std::string& manufacturer = "Generic", 
                 const std::string& model = "Rotator")
      : TypedDeviceFactory<Rotator>(manufacturer, model) {}
};

} // namespace device
} // namespace hydrogen

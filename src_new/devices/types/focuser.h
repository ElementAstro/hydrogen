#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"
#include "behaviors/movable_behavior.h"
#include "behaviors/temperature_control_behavior.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace device {

using json = nlohmann::json;

/**
 * @brief Stepping mode enumeration for motor control
 */
enum class StepMode {
  FULL_STEP = 1,         // Full step
  HALF_STEP = 2,         // Half step
  QUARTER_STEP = 4,      // Quarter step
  EIGHTH_STEP = 8,       // Eighth step
  SIXTEENTH_STEP = 16,   // Sixteenth step
  THIRTYSECOND_STEP = 32 // Thirty-second step
};

/**
 * @brief Focus curve data point structure
 */
struct FocusPoint {
  int position;          // Position
  double metric;         // Focus quality metric (higher = better focus)
  double temperature;    // Temperature at time of measurement
  std::string timestamp; // Timestamp of measurement
};

/**
 * @brief 调焦器设备实�?
 *
 * 基于新架构的调焦器实现，使用行为组件提供移动控制和温度管理功能�?
 * 支持多种制造商的调焦器设备，提供统一的控制接口�?
 */
class Focuser : public core::ModernDeviceBase,
                public interfaces::IFocuser,
                public interfaces::ITemperatureControlled {
public:
  /**
   * @brief Constructor
   * @param deviceId Device identifier
   * @param manufacturer Manufacturer name
   * @param model Model name
   */
  Focuser(const std::string &deviceId,
          const std::string &manufacturer = "ZWO",
          const std::string &model = "EAF");

  /**
   * @brief Virtual destructor
   */
  virtual ~Focuser();

  /**
   * @brief 获取设备类型名称
   */
  static std::string getDeviceTypeName() { return "FOCUSER"; }

  /**
   * @brief 获取支持的制造商列表
   */
  static std::vector<std::string> getSupportedManufacturers() {
    return {"ZWO", "Celestron", "QHY", "Moonlite", "Generic"};
  }

  /**
   * @brief 获取支持的型号列�?
   */
  static std::vector<std::string> getSupportedModels(const std::string& manufacturer) {
    if (manufacturer == "ZWO") return {"EAF", "EAF-S"};
    if (manufacturer == "Celestron") return {"Focus Motor"};
    if (manufacturer == "QHY") return {"CFW3-US"};
    if (manufacturer == "Moonlite") return {"CSL", "DRO"};
    return {"Generic Focuser"};
  }

  // 实现IMovable接口
  bool moveToPosition(int position) override;
  bool moveRelative(int steps) override;
  bool stopMovement() override;
  bool home() override;
  int getCurrentPosition() const override;
  bool isMoving() const override;

  // 实现IFocuser特定接口
  double getTemperature() const override;
  bool supportsTemperatureCompensation() const override;
  bool setTemperatureCompensation(bool enabled) override;

  // 实现ITemperatureControlled接口
  bool setTargetTemperature(double temperature) override;
  double getCurrentTemperature() const override;
  double getTargetTemperature() const override;
  bool stopTemperatureControl() override;
  bool isTemperatureStable() const override;

  // ==== 扩展功能接口 ====

  /**
   * @brief Move to absolute position (向后兼容)
   */
  virtual bool moveAbsolute(int position, bool synchronous = false);

  /**
   * @brief Abort current movement (向后兼容)
   */
  virtual bool abort();

  // ==== 扩展功能方法 ====

  /**
   * @brief Set maximum position
   * @param maxPos Maximum position value
   * @return True if successful
   */
  virtual bool setMaxPosition(int maxPos);

  /**
   * @brief Set movement speed
   * @param speedValue Speed value (1-10)
   * @return True if successful
   */
  virtual bool setSpeed(int speedValue);

  /**
   * @brief Set backlash compensation value
   * @param backlashValue Backlash steps
   * @return True if successful
   */
  virtual bool setBacklash(int backlashValue);

  /**
   * @brief Set stepping mode
   * @param mode Stepping mode enum value
   * @return True if successful
   */
  virtual bool setStepMode(StepMode mode);

  /**
   * @brief Save current position as a named focus point
   * @param name Focus point name
   * @param description Optional description
   * @return True if successful
   */
  virtual bool saveFocusPoint(const std::string &name,
                              const std::string &description = "");

  /**
   * @brief Move to saved focus point
   * @param name Focus point name
   * @param synchronous If true, wait for movement to complete
   * @return True if movement started successfully
   */
  virtual bool moveToSavedPoint(const std::string &name,
                                bool synchronous = false);

  /**
   * @brief Get all saved focus points
   * @return JSON object with focus point names and positions
   */
  virtual json getSavedFocusPoints() const;

  /**
   * @brief Start auto focus process
   * @param startPos Starting position
   * @param endPos Ending position
   * @param steps Number of steps
   * @param useExistingCurve Whether to use existing curve data
   * @return True if auto focus started successfully
   */
  virtual bool startAutoFocus(int startPos, int endPos, int steps,
                              bool useExistingCurve = false);

  /**
   * @brief Get focus curve data
   * @return JSON array of focus curve data points
   */
  virtual json getFocusCurveData() const;

  /**
   * @brief Save device configuration
   * @param filePath Configuration file path
   * @return True if successful
   */
  virtual bool saveConfiguration(const std::string &filePath) const;

  /**
   * @brief Load device configuration
   * @param filePath Configuration file path
   * @return True if successful
   */
  virtual bool loadConfiguration(const std::string &filePath);

  /**
   * @brief Set focus quality metric callback
   * This callback is called during auto focus to evaluate focus quality at
   * current position
   * @param callback Callback function that accepts position and returns quality
   * metric (higher=better)
   */
  using FocusMetricCallback = std::function<double(int position)>;
  void setFocusMetricCallback(FocusMetricCallback callback);

  // ==== Additional Interface Methods ====

  /**
   * @brief Get maximum position
   */
  int getMaxPosition() const;

  /**
   * @brief Get backlash compensation value
   */
  int getBacklash() const;

  /**
   * @brief Get temperature compensation coefficient
   */
  double getTempCompCoefficient() const;

  /**
   * @brief Set temperature compensation coefficient
   */
  bool setTempCompCoefficient(double coefficient);

  /**
   * @brief Handle device-specific commands
   */
  bool handleDeviceCommand(const std::string& command, const json& parameters, json& result);

  /**
   * @brief Update device state
   */
  void updateDevice();

  /**
   * @brief Get device capabilities
   */
  std::vector<std::string> getCapabilities() const override;

  // Hardware abstraction methods (moved from protected to public for behavior access)
  virtual bool executeMovement(int targetPosition);
  virtual bool executeStop();
  virtual bool executeHome();
  virtual double readTemperature();
  virtual double readAmbientTemperature();
  virtual bool setTemperatureControl(double power);

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
  // ==== Base Class Method Overrides ====

  /**
   * @brief Initialize device-specific functionality
   */
  bool initializeDevice();

  /**
   * @brief Start device-specific functionality
   */
  bool startDevice();

  /**
   * @brief Stop device-specific functionality
   */
  void stopDevice();

  // ==== Protected Update Methods ====

  /**
   * @brief Focuser status update thread
   * Derived classes can override this method to implement custom update logic
   */
  virtual void updateLoop();

  /**
   * @brief Send move completed event
   * @param relatedMessageId Related command message ID
   */
  virtual void sendMoveCompletedEvent(const std::string &relatedMessageId);

  /**
   * @brief Apply temperature compensation algorithm
   * Derived classes can override this method to implement custom temperature
   * compensation
   * @param currentPosition Current position
   * @return Temperature-compensated position
   */
  virtual int applyTemperatureCompensation(int currentPosition);

  /**
   * @brief Calculate focus quality metric
   * Derived classes should override this method to provide actual focus quality
   * evaluation
   * @param position Current position
   * @return Focus quality value (higher=better)
   */
  virtual double calculateFocusMetric(int position);

  /**
   * @brief Perform auto focus algorithm
   * Derived classes can override this method to provide custom auto focus
   * algorithm
   */
  virtual void performAutoFocus();

  /**
   * @brief Wait for movement to complete
   * @param timeoutMs Timeout in milliseconds, 0 means wait indefinitely
   * @return True if movement completed successfully, false if timeout
   */
  bool waitForMoveComplete(int timeoutMs = 0);

  // Note: Hardware abstraction methods moved to public section above

  // Enhanced methods from modern_focuser
  void initializeHardware();
  void initializeFocuserBehaviors();
  bool validatePosition(int position) const;
  int calculateMovementTime(int distance) const;

private:
  // Behavior components
  behaviors::MovableBehavior* movableBehavior_;
  behaviors::TemperatureControlBehavior* temperatureBehavior_;

  // Hardware-specific parameters
  std::atomic<int> maxPosition_;
  std::atomic<int> stepSize_;
  std::atomic<int> backlash_;
  std::atomic<bool> temperatureCompensation_;
  std::atomic<double> tempCompCoefficient_;
  std::atomic<double> currentTemperature_;
  std::atomic<double> ambientTemperature_;

  // Enhanced hardware parameters from modern_focuser
  int hardwareMaxPosition_;
  int hardwareMinPosition_;
  double hardwareStepSize_;
  bool hasTemperatureSensor_;
  bool hasTemperatureControl_;

  // Communication parameters
  std::string serialPort_;
  int baudRate_;

  // Movement parameters
  int maxSpeed_;
  int acceleration_;

  // Temperature parameters
  double temperatureOffset_;
  double temperatureScale_;

  // Auto focus parameters (preserved from original)
  std::vector<FocusPoint> focusCurve_;
  std::atomic<bool> cancelAutoFocus_;
  FocusMetricCallback focusMetricCallback_;

  // Saved focus points
  std::unordered_map<std::string, std::pair<int, std::string>> savedFocusPoints_;

  // Thread-safe state access
  mutable std::mutex focusCurveMutex_;
  mutable std::mutex focusPointsMutex_;
};

/**
 * @brief Factory function for creating focuser instances
 * @param deviceId Device identifier
 * @param manufacturer Manufacturer name
 * @param model Model name
 * @return Unique pointer to focuser instance
 */
std::unique_ptr<Focuser> createModernFocuser(const std::string& deviceId,
                                             const std::string& manufacturer = "ZWO",
                                             const std::string& model = "EAF");

} // namespace device
} // namespace hydrogen

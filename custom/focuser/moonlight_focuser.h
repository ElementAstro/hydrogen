#pragma once
#include "device/focuser.h"

namespace astrocomm {

/**
 * @brief Moonlight电动调焦器特定的工作模式
 */
enum class MoonlightDriveMode {
  NORMAL = 0,      // 正常驱动模式
  HIGH_TORQUE = 1, // 高扭矩模式（用于重负载）
  SILENT = 2,    // 静音模式（牺牲部分扭矩换取更安静的操作）
  POWER_SAVE = 3 // 省电模式（空闲时降低功耗）
};

/**
 * @brief Moonlight电动调焦器的实现类
 *
 * 这个类实现了Moonlight电动调焦器特有的功能和参数
 */
class MoonlightFocuser : public Focuser {
public:
  /**
   * @brief 构造函数
   * @param deviceId 设备ID
   * @param model 型号（默认为"Nitecrawler V3"）
   */
  MoonlightFocuser(const std::string &deviceId,
                   const std::string &model = "Nitecrawler V3");

  /**
   * @brief 析构函数
   */
  virtual ~MoonlightFocuser();

  // ==== Moonlight特有方法 ====

  /**
   * @brief 设置电机电流限制
   * @param currentLimit 电流限制值（百分比，0-100）
   * @return 设置是否成功
   */
  bool setCurrentLimit(int currentLimit);

  /**
   * @brief 设置驱动模式
   * @param mode 驱动模式
   * @return 设置是否成功
   */
  bool setDriveMode(MoonlightDriveMode mode);

  /**
   * @brief 启用/禁用旋钮手动控制
   * @param enabled 是否启用
   * @return 设置是否成功
   */
  bool setKnobControl(bool enabled);

  /**
   * @brief 启用/禁用电机空闲时断电
   * @param enabled 是否启用
   * @return 设置是否成功
   */
  bool setMotorPowerDown(bool enabled);

  /**
   * @brief 获取电机温度
   * @return 电机温度（摄氏度）
   */
  double getMotorTemperature() const;

  /**
   * @brief 执行校准过程
   * @return 校准是否成功
   */
  bool calibrate();

  /**
   * @brief 重置设备到出厂设置
   * @return 重置是否成功
   */
  bool resetToFactoryDefaults();

protected:
  // ==== 重写基类方法 ====

  /**
   * @brief 重写状态更新循环，添加Moonlight特有的状态更新逻辑
   */
  virtual void updateLoop() override;

  /**
   * @brief 重写温度补偿算法，使用Moonlight特有的温度补偿公式
   */
  virtual int applyTemperatureCompensation(int currentPosition) override;

  // ==== Moonlight特有命令处理器 ====
  void handleSetCurrentLimitCommand(const CommandMessage &cmd,
                                    ResponseMessage &response);
  void handleSetDriveModeCommand(const CommandMessage &cmd,
                                 ResponseMessage &response);
  void handleSetKnobControlCommand(const CommandMessage &cmd,
                                   ResponseMessage &response);
  void handleSetMotorPowerDownCommand(const CommandMessage &cmd,
                                      ResponseMessage &response);
  void handleCalibrateCommand(const CommandMessage &cmd,
                              ResponseMessage &response);
  void handleResetCommand(const CommandMessage &cmd, ResponseMessage &response);

private:
  // ==== Moonlight特有状态变量 ====
  int currentLimit;             // 电机电流限制（百分比）
  MoonlightDriveMode driveMode; // 驱动模式
  bool knobControlEnabled;      // 旋钮控制启用状态
  bool motorPowerDownEnabled;   // 电机空闲时断电功能状态
  double motorTemperature;      // 电机温度
  double controllerVoltage;     // 控制器电压
  double motorCurrent;          // 电机电流
  bool isCalibrated;            // 校准状态

  // 内部方法
  void checkMotorParameters(); // 检查电机参数（过热、过载等）
  void simulateKnobControl();  // 模拟旋钮控制
};

} // namespace astrocomm
#pragma once

#include "device/focuser.h"

#include <array>
#include <random>
#include <string>

namespace astrocomm {

/**
 * @brief ASI EAF调焦器状态枚举
 */
enum class EafStatus {
  IDLE = 0,   // 空闲
  MOVING = 1, // 移动中
  ERROR = 2,  // 错误
  STOPPED = 3 // 已停止
};

/**
 * @brief ASI EAF电机类型枚举
 */
enum class EafMotorType {
  DC_MOTOR = 0, // 直流电机
  STEPPER = 1   // 步进电机
};

/**
 * @brief ASI EAF调焦器实现类
 *
 * 该类模拟ZWO ASI EAF调焦器设备的特性和行为
 */
class AsiEafFocuser : public Focuser {
public:
  /**
   * @brief 构造函数
   * @param deviceId 设备ID
   * @param hwVersion 硬件版本
   * @param fwVersion 固件版本
   */
  AsiEafFocuser(const std::string &deviceId,
                const std::string &hwVersion = "2.0",
                const std::string &fwVersion = "1.5.1");

  /**
   * @brief 析构函数
   */
  virtual ~AsiEafFocuser();

  // 重写启动方法
  virtual bool start() override;

  // 重写停止方法
  virtual void stop() override;

  // ==== ASI EAF特定方法 ====

  /**
   * @brief 设置反向
   * @param reverse 是否反向
   * @return 设置是否成功
   */
  bool setReverse(bool reverse);

  /**
   * @brief 设置手动移动模式
   * @param enable 是否启用手动移动
   * @return 设置是否成功
   */
  bool setHandController(bool enable);

  /**
   * @brief 设置高精度模式
   * @param enable 是否启用高精度模式
   * @return 设置是否成功
   */
  bool setHighPrecision(bool enable);

  /**
   * @brief 设置保持开启状态
   * @param enable 是否保持开启
   * @return 设置是否成功
   */
  bool setHoldOnBoot(bool enable);

  /**
   * @brief 启动寻找零位过程
   * @return 是否成功启动寻找零位
   */
  bool findHome();

  /**
   * @brief 获取电机参数
   * @return 电机参数JSON对象
   */
  json getMotorParams() const;

  /**
   * @brief 设置电机参数
   * @param params 电机参数JSON对象
   * @return 设置是否成功
   */
  bool setMotorParams(const json &params);

  /**
   * @brief 执行设备复位
   * @param hardReset 是否为硬复位
   * @return 复位是否成功
   */
  bool resetDevice(bool hardReset = false);

protected:
  // ==== 重写基类方法 ====

  /**
   * @brief 重写更新循环以模拟ASI EAF行为
   */
  virtual void updateLoop() override;

  /**
   * @brief 重写ASI EAF的温度补偿
   * @param currentPosition 当前位置
   * @return 温度补偿后的位置
   */
  virtual int applyTemperatureCompensation(int currentPosition) override;

  /**
   * @brief 重写处理命令
   */
  virtual void handleSetReverse(const CommandMessage &cmd,
                                ResponseMessage &response);
  virtual void handleSetHandController(const CommandMessage &cmd,
                                       ResponseMessage &response);
  virtual void handleSetHighPrecision(const CommandMessage &cmd,
                                      ResponseMessage &response);
  virtual void handleSetHoldOnBoot(const CommandMessage &cmd,
                                   ResponseMessage &response);
  virtual void handleFindHome(const CommandMessage &cmd,
                              ResponseMessage &response);
  virtual void handleGetMotorParams(const CommandMessage &cmd,
                                    ResponseMessage &response);
  virtual void handleSetMotorParams(const CommandMessage &cmd,
                                    ResponseMessage &response);
  virtual void handleResetDevice(const CommandMessage &cmd,
                                 ResponseMessage &response);

private:
  // ==== ASI EAF特定状态 ====

  // 设备信息
  std::string hardwareVersion;
  std::string firmwareVersion;
  std::string serialNumber;

  // 设备状态
  EafStatus eafStatus;
  EafMotorType motorType;

  // 设备设置
  bool reverseDirection;
  bool handControllerEnabled;
  bool highPrecisionEnabled;
  bool holdOnBootEnabled;

  // 电机参数
  uint16_t motorSpeed;        // 电机速度 (0-255)
  uint8_t motorAcceleration;  // 电机加速度 (0-255)
  uint16_t motorCurrentLimit; // 电机电流限制 (mA)

  // 零位寻找
  bool isHoming;
  int homePosition;

  // 健康监控
  float voltage;        // 电源电压
  float motorCurrent;   // 电机电流
  float controllerTemp; // 控制器温度

  // 高精度模式状态跟踪
  std::array<int, 10> recentPositions;
  size_t positionIndex;

  // 随机数生成器
  std::mt19937 rng;
  std::uniform_real_distribution<> voltageNoise;
  std::uniform_real_distribution<> currentNoise;
  std::uniform_real_distribution<> tempNoise;

  // 私有辅助方法
  void initializeDevice();
  void updateDeviceStatus();
  void updateHealthMetrics();
  std::string generateSerialNumber();
  void setStatus(EafStatus status);
  int getAveragedPosition() const;
};

} // namespace astrocomm

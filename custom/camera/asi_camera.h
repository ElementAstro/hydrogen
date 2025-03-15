#pragma once

#include "device/camera.h"
#include <string>

namespace astrocomm {

/**
 * @brief ASI相机控制模式枚举
 */
enum class ASIControlType {
  GAIN,                   // 增益
  EXPOSURE,               // 曝光
  GAMMA,                  // 伽马
  WB_R,                   // 白平衡 R
  WB_B,                   // 白平衡 B
  OFFSET,                 // 偏移
  BANDWIDTHOVERLOAD,      // 带宽
  OVERCLOCK,              // 超频
  TEMPERATURE,            // 温度 (只读)
  FLIP,                   // 翻转
  AUTO_MAX_GAIN,          // 自动曝光最大增益
  AUTO_MAX_EXP,           // 自动曝光最大曝光时间
  AUTO_TARGET_BRIGHTNESS, // 自动曝光目标亮度
  HARDWARE_BIN,           // 硬件像素合并
  HIGH_SPEED_MODE,        // 高速模式
  COOLER_POWER,           // 制冷功率 (只读)
  TARGET_TEMP,            // 目标温度
  COOLER_ON,              // 制冷开关
  MONO_BIN,               // 单色模式像素合并
  FAN_ON,                 // 风扇开关
  PATTERN_ADJUST,         // 模式调整
  ANTI_DEW_HEATER,        // 防露加热器
  HUMIDITY,               // 湿度 (只读)
  PRESSURE                // 气压 (只读)
};

/**
 * @brief ASI相机控制参数结构
 */
struct ASIControlCaps {
  ASIControlType type;
  std::string name;
  long minValue;
  long maxValue;
  long defaultValue;
  bool isAutoSupported;
  bool isWritable;
};

/**
 * @brief ASI相机专用类，实现ZWO ASI相机的特定功能
 */
class ASICamera : public Camera {
public:
  /**
   * @brief 构造函数
   * @param deviceId 设备ID
   * @param model 型号（如"ASI294MM Pro"）
   * @param params 相机参数
   */
  ASICamera(const std::string &deviceId,
            const std::string &model = "ASI294MM Pro",
            const CameraParameters &params = ASICamera::getDefaultParams());

  /**
   * @brief 析构函数
   */
  virtual ~ASICamera();

  /**
   * @brief 获取默认的ASI相机参数
   * @return 默认ASI相机参数
   */
  static CameraParameters getDefaultParams();

  /**
   * @brief 指示这是一个特定的实现而非基类
   * @return 始终返回false
   */
  virtual bool isBaseImplementation() const override;

  // ---- ASI特有功能 ----

  /**
   * @brief 设置伽马值
   * @param value 伽马值
   * @return 设置成功返回true，否则返回false
   */
  bool setGamma(int value);

  /**
   * @brief 获取伽马值
   * @return 当前伽马值
   */
  int getGamma() const;

  /**
   * @brief 设置白平衡R值
   * @param value 白平衡R值
   * @return 设置成功返回true，否则返回false
   */
  bool setWhiteBalanceR(int value);

  /**
   * @brief 获取白平衡R值
   * @return 当前白平衡R值
   */
  int getWhiteBalanceR() const;

  /**
   * @brief 设置白平衡B值
   * @param value 白平衡B值
   * @return 设置成功返回true，否则返回false
   */
  bool setWhiteBalanceB(int value);

  /**
   * @brief 获取白平衡B值
   * @return 当前白平衡B值
   */
  int getWhiteBalanceB() const;

  /**
   * @brief 设置硬件像素合并
   * @param enabled 是否启用
   * @return 设置成功返回true，否则返回false
   */
  bool setHardwareBin(bool enabled);

  /**
   * @brief 获取硬件像素合并状态
   * @return 硬件像素合并是否启用
   */
  bool getHardwareBin() const;

  /**
   * @brief 设置高速模式
   * @param enabled 是否启用
   * @return 设置成功返回true，否则返回false
   */
  bool setHighSpeedMode(bool enabled);

  /**
   * @brief 获取高速模式状态
   * @return 高速模式是否启用
   */
  bool getHighSpeedMode() const;

  /**
   * @brief 设置风扇开关
   * @param enabled 是否启用
   * @return 设置成功返回true，否则返回false
   */
  bool setFanEnabled(bool enabled);

  /**
   * @brief 获取风扇状态
   * @return 风扇是否启用
   */
  bool getFanEnabled() const;

  /**
   * @brief 设置防露加热器
   * @param value 加热器强度(0-100)
   * @return 设置成功返回true，否则返回false
   */
  bool setAntiDewHeater(int value);

  /**
   * @brief 获取防露加热器强度
   * @return 加热器强度(0-100)
   */
  int getAntiDewHeater() const;

  /**
   * @brief 启用或禁用自动曝光功能
   * @param enabled 是否启用
   * @param maxGain 自动曝光最大增益(可选)
   * @param maxExposure 自动曝光最大曝光时间(可选)
   * @param targetBrightness 自动曝光目标亮度(可选)
   * @return 设置成功返回true，否则返回false
   */
  bool setAutoExposure(bool enabled, int maxGain = -1,
                       double maxExposure = -1.0, int targetBrightness = -1);

  /**
   * @brief 获取自动曝光参数
   * @return 包含自动曝光参数的JSON对象
   */
  json getAutoExposureParameters() const;

  /**
   * @brief 获取支持的控制功能列表
   * @return 包含支持控制功能的JSON数组
   */
  json getSupportedControls() const;

  /**
   * @brief 获取相机湿度
   * @return 相机内部湿度(%)，如不支持则返回-1
   */
  float getHumidity() const;

  /**
   * @brief 获取相机内部气压
   * @return 相机内部气压(hPa)，如不支持则返回-1
   */
  float getPressure() const;

  /**
   * @brief 设置翻转模式
   * @param horizontal 是否水平翻转
   * @param vertical 是否垂直翻转
   * @return 设置成功返回true，否则返回false
   */
  bool setFlipMode(bool horizontal, bool vertical);

  /**
   * @brief 获取翻转模式
   * @return 包含翻转模式的JSON对象
   */
  json getFlipMode() const;

protected:
  /**
   * @brief 重写生成图像数据方法，提供ASI相机特有特征
   */
  virtual void generateImageData() override;

  /**
   * @brief 应用ASI特有的图像效果
   * @param imageData 图像数据
   */
  virtual void applyImageEffects(std::vector<uint8_t> &imageData) override;

  /**
   * @brief 重载更新循环，增加ASI特有状态更新
   */
  virtual void updateLoop() override;

private:
  // ASI特有参数
  int gamma;          // 伽马值
  int whiteBalanceR;  // 白平衡R值
  int whiteBalanceB;  // 白平衡B值
  bool hardwareBin;   // 硬件像素合并
  bool highSpeedMode; // 高速模式
  bool fanEnabled;    // 风扇是否开启
  int antiDewHeater;  // 防露加热器
  float humidity;     // 湿度
  float pressure;     // 气压
  struct {
    bool horizontal; // 水平翻转
    bool vertical;   // 垂直翻转
  } flipMode;

  // 自动曝光参数
  struct {
    int maxGain;        // 自动曝光最大增益
    double maxExposure; // 自动曝光最大曝光时间
  } autoExposureParams;

  // 支持的控制功能
  std::vector<ASIControlCaps> supportedControls;

  // 初始化支持的控制功能
  void initSupportedControls();

  // ASI特有命令处理器
  void handleASISpecificCommand(const CommandMessage &cmd,
                                ResponseMessage &response);

  // 辅助方法 - 控制类型转换
  std::string controlTypeToString(ASIControlType type) const;
  ASIControlType stringToControlType(const std::string &typeStr) const;
};

} // namespace astrocomm

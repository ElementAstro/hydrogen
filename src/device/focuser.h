#pragma once

#include "device/device_base.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace astrocomm {

/**
 * @brief 步进模式枚举，定义电机的步进模式
 */
enum class StepMode {
  FULL_STEP = 1,         // 全步
  HALF_STEP = 2,         // 半步
  QUARTER_STEP = 4,      // 1/4步
  EIGHTH_STEP = 8,       // 1/8步
  SIXTEENTH_STEP = 16,   // 1/16步
  THIRTYSECOND_STEP = 32 // 1/32步
};

/**
 * @brief 对焦曲线数据点结构
 */
struct FocusPoint {
  int position;          // 位置
  double metric;         // 对焦质量度量 (更高 = 更好的焦点)
  double temperature;    // 记录时的温度
  std::string timestamp; // 记录时间戳
};

/**
 * @brief 调焦器基类 - 提供调焦器设备的基本功能实现
 *
 * 这个类为可继承的基类，实现了调焦器的核心功能，可以被派生类扩展
 */
class Focuser : public DeviceBase {
public:
  /**
   * @brief 构造函数
   * @param deviceId 设备ID
   * @param manufacturer 制造商
   * @param model 型号
   */
  Focuser(const std::string &deviceId, const std::string &manufacturer = "ZWO",
          const std::string &model = "EAF");

  /**
   * @brief 虚析构函数，确保正确析构派生类
   */
  virtual ~Focuser();

  // 重写启动和停止方法
  virtual bool start() override;
  virtual void stop() override;

  // ==== 调焦器基本操作 ====

  /**
   * @brief 移动到绝对位置
   * @param position 目标位置
   * @param synchronous 如果为true，会等待移动完成
   * @return 如果操作成功启动则返回true
   */
  virtual bool moveAbsolute(int position, bool synchronous = false);

  /**
   * @brief 相对移动指定的步数
   * @param steps 移动步数(正值向外，负值向内)
   * @param synchronous 如果为true，会等待移动完成
   * @return 如果操作成功启动则返回true
   */
  virtual bool moveRelative(int steps, bool synchronous = false);

  /**
   * @brief 中止当前移动
   * @return 如果成功中止则返回true
   */
  virtual bool abort();

  // ==== 调焦器参数设置 ====

  /**
   * @brief 设置最大位置
   * @param maxPos 最大位置值
   * @return 设置是否成功
   */
  virtual bool setMaxPosition(int maxPos);

  /**
   * @brief 设置移动速度
   * @param speedValue 速度值(1-10)
   * @return 设置是否成功
   */
  virtual bool setSpeed(int speedValue);

  /**
   * @brief 设置反向间隙补偿值
   * @param backlashValue 反向间隙步数
   * @return 设置是否成功
   */
  virtual bool setBacklash(int backlashValue);

  /**
   * @brief 设置步进模式
   * @param mode 步进模式枚举值
   * @return 设置是否成功
   */
  virtual bool setStepMode(StepMode mode);

  /**
   * @brief 设置温度补偿
   * @param enabled 是否启用
   * @param coefficient 补偿系数
   * @return 设置是否成功
   */
  virtual bool setTemperatureCompensation(bool enabled,
                                          double coefficient = 0.0);

  // ==== 高级功能 ====

  /**
   * @brief 保存当前位置为命名焦点
   * @param name 焦点名称
   * @param description 可选描述
   * @return 保存是否成功
   */
  virtual bool saveFocusPoint(const std::string &name,
                              const std::string &description = "");

  /**
   * @brief 移动到已保存的焦点
   * @param name 焦点名称
   * @param synchronous 如果为true，会等待移动完成
   * @return 移动是否成功启动
   */
  virtual bool moveToSavedPoint(const std::string &name,
                                bool synchronous = false);

  /**
   * @brief 获取所有保存的焦点
   * @return 焦点名称和位置的JSON对象
   */
  virtual json getSavedFocusPoints() const;

  /**
   * @brief 启动自动对焦过程
   * @param startPos 起始位置
   * @param endPos 结束位置
   * @param steps 步进次数
   * @param useExistingCurve 是否使用现有曲线数据
   * @return 是否成功启动自动对焦
   */
  virtual bool startAutoFocus(int startPos, int endPos, int steps,
                              bool useExistingCurve = false);

  /**
   * @brief 获取对焦曲线数据
   * @return 对焦曲线数据点的JSON数组
   */
  virtual json getFocusCurveData() const;

  /**
   * @brief 保存设备配置
   * @param filePath 配置文件路径
   * @return 保存是否成功
   */
  virtual bool saveConfiguration(const std::string &filePath) const;

  /**
   * @brief 加载设备配置
   * @param filePath 配置文件路径
   * @return 加载是否成功
   */
  virtual bool loadConfiguration(const std::string &filePath);

  /**
   * @brief 设置对焦质量度量回调
   * 这个回调会在自动对焦过程中被调用来评估当前位置的对焦质量
   * @param callback 回调函数，接收位置参数，返回对焦质量(更高=更好)
   */
  using FocusMetricCallback = std::function<double(int position)>;
  void setFocusMetricCallback(FocusMetricCallback callback);

protected:
  // ==== 保护的更新方法 ====

  /**
   * @brief 调焦器状态更新线程
   * 子类可以重写此方法实现自定义更新逻辑
   */
  virtual void updateLoop();

  /**
   * @brief 发送移动完成事件
   * @param relatedMessageId 相关命令消息ID
   */
  virtual void sendMoveCompletedEvent(const std::string &relatedMessageId);

  /**
   * @brief 应用温度补偿算法
   * 子类可以重写此方法实现自定义温度补偿算法
   * @param currentPosition 当前位置
   * @return 温度补偿后的位置
   */
  virtual int applyTemperatureCompensation(int currentPosition);

  /**
   * @brief 计算对焦质量度量
   * 子类应该重写这个方法来提供实际的对焦质量评估
   * @param position 当前位置
   * @return 对焦质量值(更高=更好)
   */
  virtual double calculateFocusMetric(int position);

  /**
   * @brief 执行自动对焦算法
   * 子类可以重写此方法提供自定义自动对焦算法
   */
  virtual void performAutoFocus();

  /**
   * @brief 等待移动完成
   * @param timeoutMs 超时毫秒数，0表示一直等待
   * @return 如果移动成功完成返回true，如果超时返回false
   */
  bool waitForMoveComplete(int timeoutMs = 0);

  // ==== 命令处理器 ====
  virtual void handleMoveAbsoluteCommand(const CommandMessage &cmd,
                                         ResponseMessage &response);
  virtual void handleMoveRelativeCommand(const CommandMessage &cmd,
                                         ResponseMessage &response);
  virtual void handleAbortCommand(const CommandMessage &cmd,
                                  ResponseMessage &response);
  virtual void handleSetMaxPositionCommand(const CommandMessage &cmd,
                                           ResponseMessage &response);
  virtual void handleSetSpeedCommand(const CommandMessage &cmd,
                                     ResponseMessage &response);
  virtual void handleSetBacklashCommand(const CommandMessage &cmd,
                                        ResponseMessage &response);
  virtual void handleSetTempCompCommand(const CommandMessage &cmd,
                                        ResponseMessage &response);
  virtual void handleSetStepModeCommand(const CommandMessage &cmd,
                                        ResponseMessage &response);
  virtual void handleSaveFocusPointCommand(const CommandMessage &cmd,
                                           ResponseMessage &response);
  virtual void handleMoveToSavedPointCommand(const CommandMessage &cmd,
                                             ResponseMessage &response);
  virtual void handleAutoFocusCommand(const CommandMessage &cmd,
                                      ResponseMessage &response);

  // ==== 保护的状态变量 ====
  int position;                           // 当前位置
  int targetPosition;                     // 目标位置
  int maxPosition;                        // 最大位置
  int speed;                              // 移动速度 (1-10)
  int backlash;                           // 反向间隙补偿
  bool tempCompEnabled;                   // 温度补偿是否启用
  double tempCompCoefficient;             // 温度补偿系数
  double temperature;                     // 当前温度
  StepMode stepMode;                      // 步进模式
  std::atomic<bool> isMoving;             // 是否正在移动
  std::atomic<bool> isAutoFocusing;       // 是否正在自动对焦
  std::string currentMoveMessageId;       // 当前移动命令的消息ID
  bool movingDirection;                   // true = 向外, false = 向内
  std::condition_variable moveCompleteCv; // 移动完成条件变量

  // 温度模拟参数
  double ambientTemperature; // 环境温度
  double temperatureDrift;   // 温度变化趋势

  // 自动对焦参数
  std::vector<FocusPoint> focusCurve;      // 对焦曲线数据
  std::atomic<bool> cancelAutoFocus;       // 取消自动对焦标志
  FocusMetricCallback focusMetricCallback; // 对焦质量评估回调

  // 保存的焦点
  std::unordered_map<std::string, std::pair<int, std::string>>
      savedFocusPoints; // 名称 -> (位置,描述)

  // 更新线程
  std::thread updateThread;
  std::atomic<bool> updateRunning;

  // 自动对焦线程
  std::thread autoFocusThread;

  // 线程安全的状态访问
  mutable std::mutex statusMutex;
  mutable std::mutex focusCurveMutex;
  mutable std::mutex focusPointsMutex;
};

} // namespace astrocomm

#pragma once

#include "device/device_base.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace astrocomm {

/**
 * @brief 相机参数结构体
 */
struct CameraParameters {
  int width = 1920;                // 图像宽度(像素)
  int height = 1080;               // 图像高度(像素)
  int bitDepth = 16;               // 像素位深度
  bool hasColorSensor = true;      // 是否为彩色传感器
  bool hasCooler = true;           // 是否有制冷功能
  bool hasFilterWheel = false;     // 是否有滤镜轮
  int maxBinningX = 4;             // 最大X方向像素合并
  int maxBinningY = 4;             // 最大Y方向像素合并
  double pixelSizeX = 3.76;        // X方向像素尺寸(微米)
  double pixelSizeY = 3.76;        // Y方向像素尺寸(微米)
  int maxGain = 100;               // 最大增益值
  int maxOffset = 100;             // 最大偏移量
  double minExposureTime = 0.001;  // 最短曝光时间(秒)
  double maxExposureTime = 3600.0; // 最长曝光时间(秒)
  double minCoolerTemp = -40.0;    // 最低冷却温度(摄氏度)
  int numFilters = 0;              // 滤镜数量
};

/**
 * @brief 相机状态枚举
 */
enum class CameraState {
  IDLE,            // 空闲
  EXPOSING,        // 正在曝光
  READING_OUT,     // 正在读出数据
  DOWNLOADING,     // 正在下载数据
  PROCESSING,      // 正在处理
  ERROR,           // 错误状态
  WAITING_TRIGGER, // 等待触发
  COOLING,         // 正在冷却
  WARMING_UP       // 正在预热
};

/**
 * @brief 图像类型枚举
 */
enum class ImageType {
  LIGHT, // 光照图像
  DARK,  // 暗场
  BIAS,  // 偏置帧
  FLAT,  // 平场
  TEST   // 测试图像
};

/**
 * @brief 相机基类，提供所有相机通用功能和接口
 *
 * 此类是一个功能丰富的基类，允许子类实现特定相机型号的功能。
 * 它支持曝光控制、冷却、ROI设置、增益控制和各种相机特性配置。
 */
class Camera : public DeviceBase {
public:
  /**
   * @brief 构造函数
   * @param deviceId 设备ID
   * @param manufacturer 制造商
   * @param model 型号
   * @param params 相机参数
   */
  Camera(const std::string &deviceId, const std::string &manufacturer = "ZWO",
         const std::string &model = "ASI294MM Pro",
         const CameraParameters &params = CameraParameters());

  /**
   * @brief 析构函数
   */
  virtual ~Camera();

  /**
   * @brief 重写启动方法
   * @return 启动成功返回true，否则返回false
   */
  virtual bool start() override;

  /**
   * @brief 重写停止方法
   */
  virtual void stop() override;

  // ---- 曝光控制方法 ----
  /**
   * @brief 开始曝光
   * @param duration 曝光时间(秒)
   * @param type 图像类型
   * @param autoSave 是否自动保存
   * @return 曝光开始成功返回true，否则返回false
   */
  virtual bool startExposure(double duration, ImageType type = ImageType::LIGHT,
                             bool autoSave = false);

  /**
   * @brief 中止当前曝光
   * @return 中止成功返回true，否则返回false
   */
  virtual bool abortExposure();

  /**
   * @brief 获取图像数据
   * @return 图像数据字节数组
   */
  virtual std::vector<uint8_t> getImageData() const;

  /**
   * @brief 保存当前图像到文件
   * @param filename 文件名，为空则使用自动生成的名称
   * @param format 图像格式(如"FITS", "JPEG", "PNG", "RAW")
   * @return 保存成功返回true，否则返回false
   */
  virtual bool saveImage(const std::string &filename = "",
                         const std::string &format = "FITS");

  // ---- 相机参数控制 ----
  /**
   * @brief 设置增益
   * @param gain 增益值
   * @return 设置成功返回true，否则返回false
   */
  virtual bool setGain(int gain);

  /**
   * @brief 设置偏置/偏移量
   * @param offset 偏移值
   * @return 设置成功返回true，否则返回false
   */
  virtual bool setOffset(int offset);

  /**
   * @brief 设置ROI(感兴趣区域)
   * @param x 起始X坐标
   * @param y 起始Y坐标
   * @param width 宽度
   * @param height 高度
   * @return 设置成功返回true，否则返回false
   */
  virtual bool setROI(int x, int y, int width, int height);

  /**
   * @brief 设置像素合并(Binning)
   * @param binX X方向合并数
   * @param binY Y方向合并数
   * @return 设置成功返回true，否则返回false
   */
  virtual bool setBinning(int binX, int binY);

  // ---- 冷却控制 ----
  /**
   * @brief 设置目标冷却温度
   * @param temperature 目标温度(摄氏度)
   * @return 设置成功返回true，否则返回false
   */
  virtual bool setCoolerTemperature(double temperature);

  /**
   * @brief 启用或禁用冷却器
   * @param enabled 启用为true，禁用为false
   * @return 设置成功返回true，否则返回false
   */
  virtual bool setCoolerEnabled(bool enabled);

  /**
   * @brief 获取当前温度
   * @return 传感器当前温度(摄氏度)
   */
  virtual double getCurrentTemperature() const;

  /**
   * @brief 获取冷却器功率
   * @return 冷却器功率(百分比，0-100)
   */
  virtual int getCoolerPower() const;

  // ---- 滤镜轮控制 ----
  /**
   * @brief 设置滤镜位置
   * @param position 滤镜位置(从0开始)
   * @return 设置成功返回true，否则返回false
   */
  virtual bool setFilterPosition(int position);

  /**
   * @brief 获取当前滤镜位置
   * @return 当前滤镜位置
   */
  virtual int getFilterPosition() const;

  /**
   * @brief 设置滤镜名称
   * @param position 滤镜位置
   * @param name 滤镜名称
   * @return 设置成功返回true，否则返回false
   */
  virtual bool setFilterName(int position, const std::string &name);

  /**
   * @brief 获取滤镜名称
   * @param position 滤镜位置
   * @return 滤镜名称
   */
  virtual std::string getFilterName(int position) const;

  // ---- 状态查询 ----
  /**
   * @brief 获取当前相机状态
   * @return 相机状态枚举值
   */
  virtual CameraState getState() const;

  /**
   * @brief 获取曝光进度
   * @return 曝光进度(0.0-1.0)
   */
  virtual double getExposureProgress() const;

  /**
   * @brief 获取相机参数
   * @return 相机参数结构体
   */
  virtual const CameraParameters &getCameraParameters() const;

  /**
   * @brief 相机是否正在曝光
   * @return 正在曝光返回true，否则返回false
   */
  virtual bool isExposing() const;

  // ---- 高级功能 ----
  /**
   * @brief 设置自动曝光参数
   * @param targetBrightness 目标亮度(0-255)
   * @param tolerance 容差
   * @return 设置成功返回true，否则返回false
   */
  virtual bool setAutoExposure(int targetBrightness, int tolerance = 5);

  /**
   * @brief 设置曝光延迟(用于长时间曝光的延迟启动)
   * @param delaySeconds 延迟秒数
   * @return 设置成功返回true，否则返回false
   */
  virtual bool setExposureDelay(double delaySeconds);

  /**
   * @brief 设置曝光完成回调函数
   * @param callback 回调函数
   */
  virtual void setExposureCallback(
      std::function<void(bool success, const std::string &message)> callback);

  /**
   * @brief 标记此设备为可继承基类
   * 子类应设置此参数以支持派生控制
   */
  virtual bool isBaseImplementation() const;

protected:
  // ---- 模拟器实现 ----
  /**
   * @brief 模拟相机状态更新线程
   * 子类可覆盖此方法以提供其自己的实现
   */
  virtual void updateLoop();

  /**
   * @brief 生成模拟图像数据
   * 子类应覆盖此方法以提供其特定类型的图像
   */
  virtual void generateImageData();

  /**
   * @brief 应用图像效果(如噪声、热点等)
   * @param imageData 图像数据
   */
  virtual void applyImageEffects(std::vector<uint8_t> &imageData);

  // ---- 命令处理器 ----
  /**
   * @brief 处理开始曝光命令
   * @param cmd 命令消息
   * @param response 响应消息
   */
  virtual void handleStartExposureCommand(const CommandMessage &cmd,
                                          ResponseMessage &response);

  /**
   * @brief 处理中止曝光命令
   * @param cmd 命令消息
   * @param response 响应消息
   */
  virtual void handleAbortExposureCommand(const CommandMessage &cmd,
                                          ResponseMessage &response);

  /**
   * @brief 处理获取图像命令
   * @param cmd 命令消息
   * @param response 响应消息
   */
  virtual void handleGetImageCommand(const CommandMessage &cmd,
                                     ResponseMessage &response);

  /**
   * @brief 处理设置冷却器命令
   * @param cmd 命令消息
   * @param response 响应消息
   */
  virtual void handleSetCoolerCommand(const CommandMessage &cmd,
                                      ResponseMessage &response);

  /**
   * @brief 处理设置ROI命令
   * @param cmd 命令消息
   * @param response 响应消息
   */
  virtual void handleSetROICommand(const CommandMessage &cmd,
                                   ResponseMessage &response);

  /**
   * @brief 处理设置增益/偏移命令
   * @param cmd 命令消息
   * @param response 响应消息
   */
  virtual void handleSetGainOffsetCommand(const CommandMessage &cmd,
                                          ResponseMessage &response);

  /**
   * @brief 处理设置滤镜轮命令
   * @param cmd 命令消息
   * @param response 响应消息
   */
  virtual void handleSetFilterCommand(const CommandMessage &cmd,
                                      ResponseMessage &response);

  // ---- 辅助方法 ----
  /**
   * @brief 发送曝光进度事件
   * @param progress 进度值(0.0-1.0)
   */
  virtual void sendExposureProgressEvent(double progress);

  /**
   * @brief 发送曝光完成事件
   * @param relatedMessageId 相关消息ID
   * @param success 是否成功
   * @param errorMessage 错误信息(如果失败)
   */
  virtual void sendExposureCompleteEvent(const std::string &relatedMessageId,
                                         bool success = true,
                                         const std::string &errorMessage = "");

  /**
   * @brief 字符串转换为ImageType枚举
   * @param typeStr 类型字符串
   * @return 图像类型枚举
   */
  virtual ImageType stringToImageType(const std::string &typeStr);

  /**
   * @brief ImageType枚举转换为字符串
   * @param type 图像类型枚举
   * @return 类型字符串
   */
  virtual std::string imageTypeToString(ImageType type);

  /**
   * @brief 字符串转换为CameraState枚举
   * @param stateStr 状态字符串
   * @return 相机状态枚举
   */
  virtual CameraState stringToCameraState(const std::string &stateStr);

  /**
   * @brief CameraState枚举转换为字符串
   * @param state 相机状态枚举
   * @return 状态字符串
   */
  virtual std::string cameraStateToString(CameraState state);

  // ---- 模拟相机状态 ----
  std::atomic<CameraState> cameraState;    // 相机状态
  CameraParameters cameraParams;           // 相机参数
  std::atomic<double> exposureDuration;    // 曝光持续时间(秒)
  std::atomic<double> exposureProgress;    // 曝光进度(0-1)
  std::atomic<ImageType> currentImageType; // 当前图像类型
  std::atomic<bool> autoSave;              // 自动保存图像
  std::atomic<int> gain;                   // 增益
  std::atomic<int> offset;                 // 偏移量
  std::atomic<double> sensorTemperature;   // 传感器温度
  std::atomic<double> targetTemperature;   // 目标温度
  std::atomic<bool> coolerEnabled;         // 制冷器启用
  std::atomic<int> coolerPower;            // 制冷器功率
  std::atomic<int> currentFilterPosition;  // 当前滤镜位置

  // ROI和合并设置
  struct {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int binX = 1;
    int binY = 1;
  } roi;

  // 自动曝光参数
  struct {
    bool enabled = false;
    int targetBrightness = 128;
    int tolerance = 5;
  } autoExposure;

  // 图像数据
  mutable std::mutex imageDataMutex;
  std::vector<uint8_t> imageData;

  // 滤镜名称映射
  std::mutex filterMutex;
  std::map<int, std::string> filterNames;

  // 曝光控制
  std::string currentExposureMessageId;
  std::atomic<double> exposureDelay;
  std::mutex callbackMutex;
  std::function<void(bool, const std::string &)> exposureCallback;

  // 更新线程
  std::thread updateThread;
  std::atomic<bool> updateRunning;
  std::mutex stateMutex;
  std::condition_variable exposureCV; // 用于等待曝光完成

  // 噪声模拟
  std::mt19937 rng; // 随机数生成器

  // 基类标志
  bool baseImplementation;
};

} // namespace astrocomm

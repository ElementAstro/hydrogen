#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"
#include "behaviors/temperature_control_behavior.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

namespace astrocomm {
namespace device {

using json = nlohmann::json;

/**
 * @brief Camera parameters structure
 */
struct CameraParameters {
  int width = 1920;                // Image width (pixels)
  int height = 1080;               // Image height (pixels)
  int bitDepth = 16;               // Pixel bit depth
  bool hasColorSensor = true;      // Whether it's a color sensor
  bool hasCooler = true;           // Whether it has cooling capability
  bool hasFilterWheel = false;     // Whether it has a filter wheel
  int maxBinningX = 4;             // Maximum X-direction pixel binning
  int maxBinningY = 4;             // Maximum Y-direction pixel binning
  double pixelSizeX = 3.76;        // X-direction pixel size (microns)
  double pixelSizeY = 3.76;        // Y-direction pixel size (microns)
  int maxGain = 100;               // Maximum gain value
  int maxOffset = 100;             // Maximum offset value
  double minExposureTime = 0.001;  // Minimum exposure time (seconds)
  double maxExposureTime = 3600.0; // Maximum exposure time (seconds)
  double minCoolerTemp = -40.0;    // Minimum cooling temperature (Celsius)
  int numFilters = 0;              // Number of filters
};

/**
 * @brief Camera state enumeration
 */
enum class CameraState {
  IDLE,            // Idle
  EXPOSING,        // Currently exposing
  READING_OUT,     // Reading out data
  DOWNLOADING,     // Downloading data
  PROCESSING,      // Processing data
  ERROR,           // Error state
  COOLING,         // Cooling
  WARMING_UP       // Warming up
};

/**
 * @brief 相机设备实现
 *
 * 基于新架构的相机实现，使用行为组件提供温度控制功能。
 * 支持多种制造商的相机设备，提供统一的控制接口。
 */
class Camera : public core::ModernDeviceBase, 
               public interfaces::ICamera,
               public interfaces::ITemperatureControlled {
public:
  /**
   * @brief Constructor
   * @param deviceId Device identifier
   * @param manufacturer Manufacturer name
   * @param model Model name
   */
  Camera(const std::string &deviceId, 
         const std::string &manufacturer = "ZWO",
         const std::string &model = "ASI294MC");

  /**
   * @brief Virtual destructor
   */
  virtual ~Camera();

  /**
   * @brief 获取设备类型名称
   */
  static std::string getDeviceTypeName() { return "CAMERA"; }

  /**
   * @brief 获取支持的制造商列表
   */
  static std::vector<std::string> getSupportedManufacturers() {
    return {"ZWO", "QHY", "SBIG", "Atik", "Canon", "Nikon", "Generic"};
  }

  /**
   * @brief 获取支持的型号列表
   */
  static std::vector<std::string> getSupportedModels(const std::string& manufacturer) {
    if (manufacturer == "ZWO") return {"ASI294MC", "ASI183MC", "ASI1600MM", "ASI533MC"};
    if (manufacturer == "QHY") return {"QHY268C", "QHY183C", "QHY294C", "QHY600M"};
    if (manufacturer == "SBIG") return {"STF-8300M", "STX-16803", "STXL-6303E"};
    if (manufacturer == "Atik") return {"460EX", "383L+", "One 6.0"};
    if (manufacturer == "Canon") return {"EOS R5", "EOS 6D Mark II"};
    if (manufacturer == "Nikon") return {"D850", "Z7 II"};
    return {"Generic Camera"};
  }

  // 实现ICamera接口
  bool startExposure(double duration) override;
  bool stopExposure() override;
  bool isExposing() const override;
  std::vector<uint8_t> getImageData() const override;
  bool setGain(int gain) override;
  int getGain() const override;
  bool setROI(int x, int y, int width, int height) override;

  // 实现ITemperatureControlled接口
  bool setTargetTemperature(double temperature) override;
  double getCurrentTemperature() const override;
  double getTargetTemperature() const override;
  bool stopTemperatureControl() override;
  bool isTemperatureStable() const override;

  // ==== 扩展功能接口 ====

  /**
   * @brief Start exposure (向后兼容)
   */
  virtual bool expose(double duration, bool synchronous = false);

  /**
   * @brief Abort current exposure (向后兼容)
   */
  virtual bool abort();

  /**
   * @brief Set camera parameters
   */
  virtual bool setCameraParameters(const CameraParameters& params);

  /**
   * @brief Get camera parameters
   */
  virtual CameraParameters getCameraParameters() const;

  /**
   * @brief Set binning
   */
  virtual bool setBinning(int binX, int binY);

  /**
   * @brief Get current binning
   */
  virtual void getBinning(int& binX, int& binY) const;

  /**
   * @brief Set offset
   */
  virtual bool setOffset(int offset);

  /**
   * @brief Get offset
   */
  virtual int getOffset() const;

  /**
   * @brief Get current camera state
   */
  virtual CameraState getCameraState() const;

  /**
   * @brief Get exposure progress (0.0 to 1.0)
   */
  virtual double getExposureProgress() const;

  /**
   * @brief Get remaining exposure time
   */
  virtual double getRemainingExposureTime() const;

  /**
   * @brief Enable/disable cooler
   */
  virtual bool setCoolerEnabled(bool enabled);

  /**
   * @brief Check if cooler is enabled
   */
  virtual bool isCoolerEnabled() const;

  /**
   * @brief Get cooler power percentage
   */
  virtual double getCoolerPower() const;

  /**
   * @brief Set image format
   */
  virtual bool setImageFormat(const std::string& format);

  /**
   * @brief Get supported image formats
   */
  virtual std::vector<std::string> getSupportedImageFormats() const;

  /**
   * @brief Save image to file
   */
  virtual bool saveImage(const std::string& filename, const std::string& format = "FITS");

  /**
   * @brief Get image statistics
   */
  virtual json getImageStatistics() const;

  /**
   * @brief Wait for exposure to complete
   */
  bool waitForExposureComplete(int timeoutMs = 0);

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
   * @brief 初始化相机行为组件
   */
  void initializeCameraBehaviors();

  /**
   * @brief 相机温度控制行为实现
   */
  class CameraTemperatureBehavior : public behaviors::TemperatureControlBehavior {
  public:
    explicit CameraTemperatureBehavior(Camera* camera);

  protected:
    double readCurrentTemperature() override;
    double readAmbientTemperature() override;
    bool setControlPower(double power) override;

  private:
    Camera* camera_;
  };

  // 硬件抽象接口
  virtual bool executeExposure(double duration);
  virtual bool executeStopExposure();
  virtual std::vector<uint8_t> executeImageDownload();
  virtual double readTemperature();
  virtual bool setTemperatureControl(double power);

  // 曝光线程函数
  void exposureThreadFunction();

private:
  // 行为组件指针
  CameraTemperatureBehavior* temperatureBehavior_;

  // 相机参数
  CameraParameters cameraParams_;

  // 曝光状态
  std::atomic<CameraState> cameraState_;
  std::atomic<double> exposureDuration_;
  std::atomic<double> exposureStartTime_;
  std::atomic<bool> exposureInProgress_;

  // 图像参数
  std::atomic<int> currentGain_;
  std::atomic<int> currentOffset_;
  std::atomic<int> binningX_;
  std::atomic<int> binningY_;
  
  // ROI设置
  std::atomic<int> roiX_;
  std::atomic<int> roiY_;
  std::atomic<int> roiWidth_;
  std::atomic<int> roiHeight_;

  // 制冷参数
  std::atomic<bool> coolerEnabled_;
  std::atomic<double> coolerPower_;

  // 图像数据
  mutable std::mutex imageDataMutex_;
  std::vector<uint8_t> imageData_;

  // 曝光线程
  std::thread exposureThread_;
  std::atomic<bool> exposureThreadRunning_;

  // 曝光完成条件变量
  mutable std::mutex exposureCompleteMutex_;
  std::condition_variable exposureCompleteCV_;

  // 随机数生成器（用于模拟）
  mutable std::mt19937 randomGenerator_;
};

/**
 * @brief 相机工厂
 */
class CameraFactory : public core::TypedDeviceFactory<Camera> {
public:
  CameraFactory(const std::string& manufacturer = "Generic", 
                const std::string& model = "Camera")
      : TypedDeviceFactory<Camera>(manufacturer, model) {}
};

} // namespace device
} // namespace astrocomm

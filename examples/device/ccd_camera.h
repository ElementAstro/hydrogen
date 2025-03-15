// include/device/ccd_camera.h
#pragma once
#include "device/camera.h"

namespace astrocomm {

/**
 * @brief CCD相机类，专门用于处理CCD传感器的特性
 */
class CCDCamera : public Camera {
public:
  /**
   * @brief 构造函数
   * @param deviceId 设备ID
   * @param manufacturer 制造商
   * @param model 型号
   * @param params 相机参数
   */
  CCDCamera(const std::string &deviceId,
            const std::string &manufacturer = "SBIG",
            const std::string &model = "ST-8300M",
            const CameraParameters &params = CCDCamera::getDefaultParams());

  /**
   * @brief 析构函数
   */
  virtual ~CCDCamera();

  /**
   * @brief 获取默认的CCD相机参数
   * @return 默认CCD相机参数
   */
  static CameraParameters getDefaultParams();

  /**
   * @brief 指示这是一个特定的实现而非基类
   * @return 始终返回false
   */
  virtual bool isBaseImplementation() const override;

  /**
   * @brief 设置反转读出模式
   * @param enabled 启用或禁用
   * @return 设置成功返回true，否则返回false
   */
  bool setInvertReadout(bool enabled);

  /**
   * @brief 设置抗反冲洗
   * @param enabled 启用或禁用
   * @return 设置成功返回true，否则返回false
   */
  bool setAntiBlooming(bool enabled);

  /**
   * @brief 设置预曝光冲洗
   * @param enabled 启用或禁用
   * @return 设置成功返回true，否则返回false
   */
  bool setPreExposureFlush(bool enabled);

protected:
  /**
   * @brief 重写生成图像数据方法，提供CCD特有的图像特征
   */
  virtual void generateImageData() override;

  /**
   * @brief 应用CCD特有的图像效果
   * @param imageData 图像数据
   */
  virtual void applyImageEffects(std::vector<uint8_t> &imageData) override;

private:
  bool invertReadout;    // 反转读出模式
  bool antiBlooming;     // 抗反冲洗功能
  bool preExposureFlush; // 预曝光冲洗

  // CCD相机特有的命令处理函数
  void handleCCDSpecificCommand(const CommandMessage &cmd,
                                ResponseMessage &response);
};

} // namespace astrocomm
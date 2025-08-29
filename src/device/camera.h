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

#undef ERROR

namespace astrocomm {

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
  WAITING_TRIGGER, // Waiting for trigger
  COOLING,         // Currently cooling
  WARMING_UP       // Currently warming up
};

/**
 * @brief Image type enumeration
 */
enum class ImageType {
  LIGHT, // Light frame
  DARK,  // Dark frame
  BIAS,  // Bias frame
  FLAT,  // Flat field
  TEST   // Test image
};

/**
 * @brief Base camera class that provides common functionality and interfaces for all cameras
 *
 * This feature-rich base class allows subclasses to implement camera-specific functionality.
 * It supports exposure control, cooling, ROI settings, gain control, and various camera configurations.
 */
class Camera : public DeviceBase {
public:
  /**
   * @brief Constructor
   * @param deviceId Device ID
   * @param manufacturer Manufacturer
   * @param model Model
   * @param params Camera parameters
   */
  Camera(const std::string &deviceId, const std::string &manufacturer = "ZWO",
         const std::string &model = "ASI294MM Pro",
         const CameraParameters &params = CameraParameters());

  /**
   * @brief Destructor
   */
  virtual ~Camera();

  /**
   * @brief Override start method
   * @return true if successful, false otherwise
   */
  virtual bool start() override;

  /**
   * @brief Override stop method
   */
  virtual void stop() override;

  // ---- Exposure Control Methods ----
  /**
   * @brief Start exposure
   * @param duration Exposure time (seconds)
   * @param type Image type
   * @param autoSave Whether to auto-save
   * @return true if exposure started successfully, false otherwise
   */
  virtual bool startExposure(double duration, ImageType type = ImageType::LIGHT,
                             bool autoSave = false);

  /**
   * @brief Abort current exposure
   * @return true if abortion successful, false otherwise
   */
  virtual bool abortExposure();

  /**
   * @brief Get image data
   * @return Image data byte array
   */
  virtual std::vector<uint8_t> getImageData() const;

  /**
   * @brief Save current image to file
   * @param filename Filename, uses auto-generated name if empty
   * @param format Image format (e.g., "FITS", "JPEG", "PNG", "RAW")
   * @return true if saved successfully, false otherwise
   */
  virtual bool saveImage(const std::string &filename = "",
                         const std::string &format = "FITS");

  // ---- Camera Parameter Controls ----
  /**
   * @brief Set gain
   * @param gain Gain value
   * @return true if successful, false otherwise
   */
  virtual bool setGain(int gain);

  /**
   * @brief Set offset/bias
   * @param offset Offset value
   * @return true if successful, false otherwise
   */
  virtual bool setOffset(int offset);

  /**
   * @brief Set ROI (region of interest)
   * @param x Starting X coordinate
   * @param y Starting Y coordinate
   * @param width Width
   * @param height Height
   * @return true if successful, false otherwise
   */
  virtual bool setROI(int x, int y, int width, int height);

  /**
   * @brief Set pixel binning
   * @param binX X-direction binning
   * @param binY Y-direction binning
   * @return true if successful, false otherwise
   */
  virtual bool setBinning(int binX, int binY);

  // ---- Cooling Controls ----
  /**
   * @brief Set target cooling temperature
   * @param temperature Target temperature (Celsius)
   * @return true if successful, false otherwise
   */
  virtual bool setCoolerTemperature(double temperature);

  /**
   * @brief Enable or disable cooler
   * @param enabled true to enable, false to disable
   * @return true if successful, false otherwise
   */
  virtual bool setCoolerEnabled(bool enabled);

  /**
   * @brief Get current temperature
   * @return Sensor's current temperature (Celsius)
   */
  virtual double getCurrentTemperature() const;

  /**
   * @brief Get cooler power
   * @return Cooler power (percentage, 0-100)
   */
  virtual int getCoolerPower() const;

  // ---- Filter Wheel Controls ----
  /**
   * @brief Set filter position
   * @param position Filter position (starting from 0)
   * @return true if successful, false otherwise
   */
  virtual bool setFilterPosition(int position);

  /**
   * @brief Get current filter position
   * @return Current filter position
   */
  virtual int getFilterPosition() const;

  /**
   * @brief Set filter name
   * @param position Filter position
   * @param name Filter name
   * @return true if successful, false otherwise
   */
  virtual bool setFilterName(int position, const std::string &name);

  /**
   * @brief Get filter name
   * @param position Filter position
   * @return Filter name
   */
  virtual std::string getFilterName(int position) const;

  // ---- Status Queries ----
  /**
   * @brief Get current camera state
   * @return Camera state enum value
   */
  virtual CameraState getState() const;

  /**
   * @brief Get exposure progress
   * @return Exposure progress (0.0-1.0)
   */
  virtual double getExposureProgress() const;

  /**
   * @brief Get camera parameters
   * @return Camera parameters struct
   */
  virtual const CameraParameters &getCameraParameters() const;

  /**
   * @brief Check if camera is currently exposing
   * @return true if exposing, false otherwise
   */
  virtual bool isExposing() const;

  // ---- Advanced Features ----
  /**
   * @brief Set auto exposure parameters
   * @param targetBrightness Target brightness (0-255)
   * @param tolerance Tolerance
   * @return true if successful, false otherwise
   */
  virtual bool setAutoExposure(int targetBrightness, int tolerance = 5);

  /**
   * @brief Set exposure delay (for delayed start of long exposures)
   * @param delaySeconds Delay in seconds
   * @return true if successful, false otherwise
   */
  virtual bool setExposureDelay(double delaySeconds);

  /**
   * @brief Set exposure completion callback function
   * @param callback Callback function
   */
  virtual void setExposureCallback(
      std::function<void(bool success, const std::string &message)> callback);

  /**
   * @brief Indicates if this device is inheritable base class
   * Derived classes should set this to support derived control
   */
  virtual bool isBaseImplementation() const;

protected:
  // ---- Simulator Implementation ----
  /**
   * @brief Camera state update thread
   * Derived classes can override this to provide their own implementation
   */
  virtual void updateLoop();

  /**
   * @brief Generate simulated image data
   * Derived classes should override this to provide their specific image types
   */
  virtual void generateImageData();

  /**
   * @brief Apply image effects (like noise, hot spots, etc.)
   * @param imageData Image data
   */
  virtual void applyImageEffects(std::vector<uint8_t> &imageData);

  // ---- Command Handlers ----
  /**
   * @brief Handle start exposure command
   * @param cmd Command message
   * @param response Response message
   */
  virtual void handleStartExposureCommand(const CommandMessage &cmd,
                                          ResponseMessage &response);

  /**
   * @brief Handle abort exposure command
   * @param cmd Command message
   * @param response Response message
   */
  virtual void handleAbortExposureCommand(const CommandMessage &cmd,
                                          ResponseMessage &response);

  /**
   * @brief Handle get image command
   * @param cmd Command message
   * @param response Response message
   */
  virtual void handleGetImageCommand(const CommandMessage &cmd,
                                     ResponseMessage &response);

  /**
   * @brief Handle set cooler command
   * @param cmd Command message
   * @param response Response message
   */
  virtual void handleSetCoolerCommand(const CommandMessage &cmd,
                                      ResponseMessage &response);

  /**
   * @brief Handle set ROI command
   * @param cmd Command message
   * @param response Response message
   */
  virtual void handleSetROICommand(const CommandMessage &cmd,
                                   ResponseMessage &response);

  /**
   * @brief Handle set gain/offset command
   * @param cmd Command message
   * @param response Response message
   */
  virtual void handleSetGainOffsetCommand(const CommandMessage &cmd,
                                          ResponseMessage &response);

  /**
   * @brief Handle set filter wheel command
   * @param cmd Command message
   * @param response Response message
   */
  virtual void handleSetFilterCommand(const CommandMessage &cmd,
                                      ResponseMessage &response);

  // ---- Helper Methods ----
  /**
   * @brief Send exposure progress event
   * @param progress Progress value (0.0-1.0)
   */
  virtual void sendExposureProgressEvent(double progress);

  /**
   * @brief Send exposure completion event
   * @param relatedMessageId Related message ID
   * @param success Whether successful
   * @param errorMessage Error message (if failed)
   */
  virtual void sendExposureCompleteEvent(const std::string &relatedMessageId,
                                         bool success = true,
                                         const std::string &errorMessage = "");

  /**
   * @brief Convert string to ImageType enum
   * @param typeStr Type string
   * @return Image type enum
   */
  virtual ImageType stringToImageType(const std::string &typeStr);

  /**
   * @brief Convert ImageType enum to string
   * @param type Image type enum
   * @return Type string
   */
  virtual std::string imageTypeToString(ImageType type);

  /**
   * @brief Convert string to CameraState enum
   * @param stateStr State string
   * @return Camera state enum
   */
  virtual CameraState stringToCameraState(const std::string &stateStr);

  /**
   * @brief Convert CameraState enum to string
   * @param state Camera state enum
   * @return State string
   */
  virtual std::string cameraStateToString(CameraState state);

  // ---- Simulated Camera State ----
  std::atomic<CameraState> cameraState;    // Camera state
  CameraParameters cameraParams;           // Camera parameters
  std::atomic<double> exposureDuration;    // Exposure duration (seconds)
  std::atomic<double> exposureProgress;    // Exposure progress (0-1)
  std::atomic<ImageType> currentImageType; // Current image type
  std::atomic<bool> autoSave;              // Auto-save image
  std::atomic<int> gain;                   // Gain
  std::atomic<int> offset;                 // Offset
  std::atomic<double> sensorTemperature;   // Sensor temperature
  std::atomic<double> targetTemperature;   // Target temperature
  std::atomic<bool> coolerEnabled;         // Cooler enabled
  std::atomic<int> coolerPower;            // Cooler power
  std::atomic<int> currentFilterPosition;  // Current filter position

  // ROI and binning settings
  struct {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int binX = 1;
    int binY = 1;
  } roi;

  // Auto exposure parameters
  struct {
    bool enabled = false;
    int targetBrightness = 128;
    int tolerance = 5;
  } autoExposure;

  // Image data
  mutable std::mutex imageDataMutex;
  std::vector<uint8_t> imageData;

  // Filter name mapping
  mutable std::mutex filterMutex;
  std::map<int, std::string> filterNames;

  // Exposure control
  std::string currentExposureMessageId;
  std::atomic<double> exposureDelay;
  std::mutex callbackMutex;
  std::function<void(bool, const std::string &)> exposureCallback;

  // Update thread
  std::thread updateThread;
  std::atomic<bool> updateRunning;
  std::mutex stateMutex;
  std::condition_variable exposureCV; // For waiting for exposure completion

  // Noise simulation
  std::mt19937 rng; // Random number generator

  // Base class flag
  bool baseImplementation;
};

} // namespace astrocomm

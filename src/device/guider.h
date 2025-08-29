#pragma once

#include "device/device_base.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#ifdef _WIN32
#undef ERROR
#endif

namespace astrocomm {

// Guider state enumeration
enum class GuiderState {
  DISCONNECTED, // Not connected
  CONNECTED,    // Connected but not guiding
  CALIBRATING,  // Calibration in progress
  GUIDING,      // Guiding in progress
  PAUSED,       // Guiding paused
  SETTLING,     // Settling after dither
  ERROR         // Error state
};

// Calibration state enumeration
enum class CalibrationState {
  IDLE,
  NORTH_MOVING,
  NORTH_COMPLETE,
  SOUTH_MOVING,
  SOUTH_COMPLETE,
  EAST_MOVING,
  EAST_COMPLETE,
  WEST_MOVING,
  WEST_COMPLETE,
  COMPLETED,
  FAILED
};

// Guider interface type enumeration
enum class GuiderInterfaceType {
  PHD2,                  // PHD2
  LINGUIDER,             // Lin-guider
  METAGUIDE,             // MetaGuide
  DIREKTGUIDER,          // DirectGuide
  ASTROPHOTOGRAPHY_TOOL, // APT
  KSTARS_EKOS,           // KStars/EKOS
  MAXIM_DL,              // MaxIm DL
  ASTROART,              // AstroArt
  ASTAP,                 // ASTAP
  VOYAGER,               // Voyager
  NINA,                  // N.I.N.A
  CUSTOM                 // Custom interface
};

// Guiding correction structure
struct GuidingCorrection {
  double raCorrection;  // RA correction (milliseconds)
  double decCorrection; // DEC correction (milliseconds)
  double raRaw;         // Raw RA error (pixels)
  double decRaw;        // Raw DEC error (pixels)

  GuidingCorrection()
      : raCorrection(0), decCorrection(0), raRaw(0), decRaw(0) {}
};

// Calibration data structure
struct CalibrationData {
  double raAngle;  // RA axis angle
  double decAngle; // DEC axis angle
  double raRate;   // RA rate (pixels/second)
  double decRate;  // DEC rate (pixels/second)
  bool flipped;    // Pier flip state
  bool calibrated; // Calibration status

  CalibrationData()
      : raAngle(0), decAngle(90), raRate(0), decRate(0), flipped(false),
        calibrated(false) {}
};

// Guide star information
struct StarInfo {
  double x;    // X coordinate
  double y;    // Y coordinate
  double flux; // Brightness
  double snr;  // Signal-to-noise ratio
  bool locked; // Lock status

  StarInfo() : x(0), y(0), flux(0), snr(0), locked(false) {}
  StarInfo(double x, double y) : x(x), y(y), flux(0), snr(0), locked(false) {}
};

// Guiding statistics data
struct GuiderStats {
  double rms;         // Overall RMS error (pixels)
  double rmsRa;       // RA RMS error (pixels)
  double rmsDec;      // DEC RMS error (pixels)
  double peak;        // Peak error (pixels)
  int totalFrames;    // Total frame count
  double snr;         // Signal-to-noise ratio
  double elapsedTime; // Guiding duration (seconds)

  GuiderStats()
      : rms(0), rmsRa(0), rmsDec(0), peak(0), totalFrames(0), snr(0),
        elapsedTime(0) {}
};

/**
 * @brief Base interface for guider software communication
 *
 * This pure virtual interface defines the contract for communication with
 * external guiding software. Implementations should handle the specific
 * protocols required by different guiding software.
 */
class GuiderInterface {
public:
  virtual ~GuiderInterface() = default;

  // Connection management
  virtual bool connect(const std::string &host, int port) = 0;
  virtual void disconnect() = 0;
  virtual bool isConnected() const = 0;

  // Guiding control
  virtual bool startGuiding() = 0;
  virtual bool stopGuiding() = 0;
  virtual bool pauseGuiding() = 0;
  virtual bool resumeGuiding() = 0;

  // Calibration control
  virtual bool startCalibration() = 0;
  virtual bool cancelCalibration() = 0;

  // Dithering
  virtual bool dither(double amount, double settleTime = 5.0,
                      double settlePixels = 1.5) = 0;

  // Status queries
  virtual GuiderState getGuiderState() const = 0;
  virtual CalibrationState getCalibrationState() const = 0;
  virtual GuiderStats getStats() const = 0;
  virtual StarInfo getGuideStar() const = 0;
  virtual CalibrationData getCalibrationData() const = 0;

  // Configuration
  virtual void setPixelScale(double scaleArcsecPerPixel) = 0;
  virtual void setGuideRate(double raRateMultiplier,
                            double decRateMultiplier) = 0;

  // Data access
  virtual GuidingCorrection getCurrentCorrection() const = 0;

  // Identification
  virtual GuiderInterfaceType getInterfaceType() const = 0;
  virtual std::string getInterfaceName() const = 0;

  // Update (non-blocking polling)
  virtual void update() = 0;
};

/**
 * @brief Base device class for guider functionality
 *
 * This class provides the foundation for guider device implementations,
 * handling communication with guiding software and exposing a consistent
 * interface for control and status monitoring.
 */
class GuiderDevice : public DeviceBase {
public:
  GuiderDevice(const std::string &deviceId,
               const std::string &manufacturer = "Generic",
               const std::string &model = "Guider");
  virtual ~GuiderDevice();

  // DeviceBase overrides
  virtual bool start() override;
  virtual void stop() override;

  // Core guider functionality
  virtual bool connectToGuider(GuiderInterfaceType type,
                               const std::string &host, int port);
  virtual void disconnectFromGuider();

  // Status access
  GuiderInterfaceType getInterfaceType() const;

  /**
   * @brief Gets the current guider interface instance
   *
   * @return std::shared_ptr<GuiderInterface> The current interface or nullptr
   * if not connected
   */
  virtual std::shared_ptr<GuiderInterface> getInterface() const;

  // Static utility methods
  static std::string interfaceTypeToString(GuiderInterfaceType type);
  static GuiderInterfaceType stringToInterfaceType(const std::string &typeStr);
  static std::string guiderStateToString(GuiderState state);
  static std::string calibrationStateToString(CalibrationState state);

protected:
  // Status update mechanism
  virtual void statusUpdateLoop();

  /**
   * @brief Processes guider state changes
   *
   * Override this method to implement custom behavior when the guider state
   * changes
   *
   * @param newState The new guider state
   */
  virtual void handleStateChanged(GuiderState newState);

  /**
   * @brief Processes guiding corrections
   *
   * Override this method to implement custom behavior when new guiding
   * corrections are received
   *
   * @param correction The new guiding correction values
   */
  virtual void handleCorrectionReceived(const GuidingCorrection &correction);

  /**
   * @brief Processes calibration state changes
   *
   * Override this method to implement custom behavior when the calibration
   * state changes
   *
   * @param newState The new calibration state
   * @param data The calibration data
   */
  virtual void handleCalibrationChanged(CalibrationState newState,
                                        const CalibrationData &data);

  /**
   * @brief Processes guider statistics updates
   *
   * Override this method to implement custom behavior when guider statistics
   * are updated
   *
   * @param newStats The updated guider statistics
   */
  virtual void handleStatsUpdated(const GuiderStats &newStats);

  // Command handlers - override these to customize command behavior
  virtual void handleConnectCommand(const CommandMessage &cmd,
                                    ResponseMessage &response);
  virtual void handleDisconnectCommand(const CommandMessage &cmd,
                                       ResponseMessage &response);
  virtual void handleStartGuidingCommand(const CommandMessage &cmd,
                                         ResponseMessage &response);
  virtual void handleStopGuidingCommand(const CommandMessage &cmd,
                                        ResponseMessage &response);
  virtual void handlePauseGuidingCommand(const CommandMessage &cmd,
                                         ResponseMessage &response);
  virtual void handleResumeGuidingCommand(const CommandMessage &cmd,
                                          ResponseMessage &response);
  virtual void handleStartCalibrationCommand(const CommandMessage &cmd,
                                             ResponseMessage &response);
  virtual void handleCancelCalibrationCommand(const CommandMessage &cmd,
                                              ResponseMessage &response);
  virtual void handleDitherCommand(const CommandMessage &cmd,
                                   ResponseMessage &response);
  virtual void handleSetParametersCommand(const CommandMessage &cmd,
                                          ResponseMessage &response);
  virtual void handleGetStatusCommand(const CommandMessage &cmd,
                                      ResponseMessage &response);

  /**
   * @brief Validates the guider interface is connected
   *
   * Helper method to check if the guider interface is valid and connected
   *
   * @param response Response message to populate on failure
   * @return bool True if interface is valid and connected, false otherwise
   */
  bool validateInterfaceConnection(ResponseMessage &response) const;

  /**
   * @brief Registers all command handlers with the device
   *
   * Call this in derived class constructors after any custom initialization
   */
  void registerCommandHandlers();

  // Interface instance
  std::shared_ptr<GuiderInterface> guiderInterface;
  GuiderInterfaceType interfaceType;

  // State variables
  GuiderState lastState;
  CalibrationState lastCalState;

  // Status update thread
  std::thread statusThread;
  std::atomic<bool> running;

  // Thread safety
  mutable std::mutex interfaceMutex;

  // Status update interval
  int statusUpdateInterval; // milliseconds
};

/**
 * @brief Factory function to create a guider interface of specific type
 *
 * @param type The type of guider interface to create
 * @return std::shared_ptr<GuiderInterface> The created interface or nullptr on
 * failure
 */
std::shared_ptr<GuiderInterface>
createGuiderInterface(GuiderInterfaceType type);

} // namespace astrocomm
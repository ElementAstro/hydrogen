#pragma once

#include "device/device_base.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace astrocomm {

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
 * @brief Focuser base class - provides core functionality for focuser devices
 *
 * This is an inheritable base class that implements core focuser functionality
 * which can be extended by derived classes for specific devices.
 */
class Focuser : public DeviceBase {
public:
  /**
   * @brief Constructor
   * @param deviceId Device identifier
   * @param manufacturer Manufacturer name
   * @param model Model name
   */
  Focuser(const std::string &deviceId, const std::string &manufacturer = "ZWO",
          const std::string &model = "EAF");

  /**
   * @brief Virtual destructor to ensure proper destruction of derived classes
   */
  virtual ~Focuser();

  // Override start and stop methods
  virtual bool start() override;
  virtual void stop() override;

  // ==== Basic Focuser Operations ====

  /**
   * @brief Move to absolute position
   * @param position Target position
   * @param synchronous If true, wait for movement to complete
   * @return True if operation started successfully
   */
  virtual bool moveAbsolute(int position, bool synchronous = false);

  /**
   * @brief Move relative number of steps
   * @param steps Steps to move (positive outward, negative inward)
   * @param synchronous If true, wait for movement to complete
   * @return True if operation started successfully
   */
  virtual bool moveRelative(int steps, bool synchronous = false);

  /**
   * @brief Abort current movement
   * @return True if successful
   */
  virtual bool abort();

  // ==== Focuser Parameter Settings ====

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
   * @brief Set temperature compensation
   * @param enabled Whether enabled
   * @param coefficient Compensation coefficient
   * @return True if successful
   */
  virtual bool setTemperatureCompensation(bool enabled,
                                          double coefficient = 0.0);

  // ==== Advanced Features ====

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

protected:
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

  // ==== Command Handlers ====
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

  // ==== Protected State Variables ====
  int position;                     // Current position
  int targetPosition;               // Target position
  int maxPosition;                  // Maximum position
  int speed;                        // Movement speed (1-10)
  int backlash;                     // Backlash compensation
  bool tempCompEnabled;             // Temperature compensation enabled
  double tempCompCoefficient;       // Temperature compensation coefficient
  double temperature;               // Current temperature
  StepMode stepMode;                // Step mode
  std::atomic<bool> isMoving;       // Whether currently moving
  std::atomic<bool> isAutoFocusing; // Whether auto focusing
  std::string currentMoveMessageId; // Current move command message ID
  bool movingDirection;             // true = outward, false = inward
  std::condition_variable
      moveCompleteCv; // Movement completion condition variable

  // Temperature simulation parameters
  double ambientTemperature; // Ambient temperature
  double temperatureDrift;   // Temperature drift trend

  // Auto focus parameters
  std::vector<FocusPoint> focusCurve;      // Focus curve data
  std::atomic<bool> cancelAutoFocus;       // Cancel auto focus flag
  FocusMetricCallback focusMetricCallback; // Focus quality evaluation callback

  // Saved focus points
  std::unordered_map<std::string, std::pair<int, std::string>>
      savedFocusPoints; // name -> (position,description)

  // Update thread
  std::thread updateThread;
  std::atomic<bool> updateRunning;

  // Auto focus thread
  std::thread autoFocusThread;

  // Thread-safe state access
  mutable std::mutex statusMutex;
  mutable std::mutex focusCurveMutex;
  mutable std::mutex focusPointsMutex;
};

} // namespace astrocomm

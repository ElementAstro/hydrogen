#pragma once
#include "device/device_base.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace astrocomm {

/**
 * @class Rotator
 * @brief Base class for rotator devices
 * 
 * This class provides the basic functionality for a rotator device,
 * including position control, movement, synchronization and reversal.
 * It is designed to be inherited by specific rotator implementations.
 */
class Rotator : public DeviceBase {
public:
  /**
   * @brief Constructor for Rotator class
   * @param deviceId Unique identifier for the device
   * @param manufacturer Manufacturer name
   * @param model Model name
   */
  Rotator(const std::string &deviceId,
          const std::string &manufacturer = "Generic",
          const std::string &model = "Field Rotator");
  
  /**
   * @brief Virtual destructor
   */
  virtual ~Rotator();

  // Device lifecycle methods
  virtual bool start() override;
  virtual void stop() override;

  // Core rotator methods - all virtual to allow overriding by derived classes
  /**
   * @brief Move to an absolute position
   * @param position Target position in degrees
   * @throws std::runtime_error If the device is not connected or the position is invalid
   */
  virtual void moveTo(double position);
  
  /**
   * @brief Move by a relative offset
   * @param offset Position offset in degrees
   * @throws std::runtime_error If the device is not connected
   */
  virtual void moveBy(double offset);
  
  /**
   * @brief Halt the current movement immediately
   * @throws std::runtime_error If the device is not connected
   */
  virtual void halt();
  
  /**
   * @brief Set reverse direction
   * @param reversed True to reverse direction, false for normal direction
   * @throws std::runtime_error If the device is not connected
   */
  virtual void setReverse(bool reversed);
  
  /**
   * @brief Synchronize the rotator position
   * @param position Position to sync to in degrees
   * @throws std::runtime_error If the device is not connected
   */
  virtual void sync(double position);
  
  /**
   * @brief Set the step size for movements
   * @param stepSize Step size in degrees
   * @throws std::invalid_argument If step size is less than or equal to zero
   */
  virtual void setStepSize(double stepSize);
  
  /**
   * @brief Set the rotator speed
   * @param speed Speed in degrees per second
   * @throws std::invalid_argument If speed is less than or equal to zero or greater than max speed
   */
  virtual void setSpeed(double speed);

  // State query methods
  /**
   * @brief Get the current position
   * @return Current position in degrees (0-359.99)
   */
  virtual double getPosition() const;
  
  /**
   * @brief Get the target position
   * @return Target position in degrees (0-359.99)
   */
  virtual double getTargetPosition() const;
  
  /**
   * @brief Check if the rotator is moving
   * @return True if the rotator is moving
   */
  virtual bool isMoving() const;
  
  /**
   * @brief Check if the rotator direction is reversed
   * @return True if the rotator direction is reversed
   */
  virtual bool isReversed() const;
  
  /**
   * @brief Get the current speed
   * @return Current speed in degrees per second
   */
  virtual double getSpeed() const;
  
  /**
   * @brief Get the maximum speed
   * @return Maximum speed in degrees per second
   */
  virtual double getMaxSpeed() const;

protected:
  // Extension points for derived classes
  /**
   * @brief Hook called before movement starts
   * @param targetPos Target position in degrees
   * @return True if movement can proceed, false to cancel
   */
  virtual bool onBeforeMove(double targetPos);
  
  /**
   * @brief Hook called after movement completes
   * @param finalPos Final position in degrees
   */
  virtual void onAfterMove(double finalPos);
  
  /**
   * @brief Hook called when position is updated
   * @param newPos New position in degrees
   */
  virtual void onPositionUpdate(double newPos);
  
  /**
   * @brief Hook called when halt is requested
   */
  virtual void onHalt();
  
  /**
   * @brief Hook called when reverse is changed
   * @param reversed New reversed state
   */
  virtual void onReverseChanged(bool reversed);

  // Update thread implementation
  /**
   * @brief Update loop running in a separate thread
   */
  virtual void updateLoop();
  
  /**
   * @brief Calculate the actual position change for one update cycle
   * @param currentPos Current position
   * @param targetPos Target position
   * @param elapsed Time elapsed since last update
   * @return New position
   */
  virtual double calculateNewPosition(double currentPos, double targetPos, 
                                     std::chrono::duration<double> elapsed);

  // Command handlers
  void handleMoveToCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleMoveByCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleHaltCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleReverseCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleSyncCommand(const CommandMessage &cmd, ResponseMessage &response);
  void handleSetSpeedCommand(const CommandMessage &cmd, ResponseMessage &response);

  // Utility methods
  void sendMoveCompletedEvent(const std::string &relatedMessageId);
  
  /**
   * @brief Normalize angle to 0-360 degrees range
   * @param angle Angle to normalize
   * @return Normalized angle
   */
  double normalizeAngle(double angle) const;
  
  /**
   * @brief Calculate rotation path
   * @param current Current position
   * @param target Target position
   * @return Pair of (distance, clockwise)
   */
  std::pair<double, bool> calculateRotationPath(double current, double target) const;
  
  /**
   * @brief Validate that device is connected before operations
   * @throws std::runtime_error If device is not connected
   */
  void validateConnected() const;

  // Protected member variables that derived classes might need to access
  std::atomic<double> position;       // Current position (degrees, 0-359.99)
  std::atomic<double> targetPosition; // Target position (degrees)
  double stepSize;                    // Step size (degrees)
  double maxSpeed;                    // Maximum rotation speed (degrees/sec)
  std::atomic<double> currentSpeed;   // Current rotation speed (degrees/sec)

  std::atomic<bool> isMovingFlag;     // Whether movement is in progress
  std::atomic<bool> isReversedFlag;   // Whether reversed direction is active

  // Mutex for thread safety in derived classes
  mutable std::mutex stateMutex;

private:
  // Update thread
  std::thread update_thread;
  std::atomic<bool> update_running;

  // Current command message ID (for completion events)
  std::string current_move_message_id;
  
  // Performance optimization - cached update interval
  std::chrono::milliseconds updateInterval;
};

} // namespace astrocomm
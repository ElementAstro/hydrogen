#pragma once

#include "device/device_base.h"

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace astrocomm {

// Filter wheel device exception class
class FilterWheelException : public std::runtime_error {
public:
  explicit FilterWheelException(const std::string &message)
      : std::runtime_error(message) {}
};

// Filter wheel device class
class FilterWheel : public DeviceBase {
public:
  FilterWheel(const std::string &deviceId,
              const std::string &manufacturer = "QHY",
              const std::string &model = "CFW3");
  virtual ~FilterWheel();

  // Override start and stop methods
  virtual bool start() override;
  virtual void stop() override;

  // Filter wheel specific methods
  virtual void setPosition(int position);
  virtual void setFilterNames(const std::vector<std::string> &names);
  virtual void setFilterOffsets(const std::vector<int> &offsets);
  virtual void abort();

  // Additional virtual methods for subclasses to override
  virtual bool isMovementComplete() const;
  virtual int getMaxFilterCount() const;
  virtual void setFilterCount(int count);

  // Public accessors for current state
  virtual std::string getCurrentFilterName() const;
  virtual int getCurrentFilterOffset() const;

protected:
  // Filter wheel state
  int position;                         // Current position
  int targetPosition;                   // Target position
  int filterCount;                      // Filter count
  std::vector<std::string> filterNames; // Filter names
  std::vector<int> filterOffsets;       // Focus offsets

  // Movement related state
  std::atomic<bool> isMoving;
  std::string currentMoveMessageId;
  int moveDirection; // 1 = clockwise, -1 = counterclockwise

  // Update thread
  std::thread updateThread;
  std::atomic<bool> updateRunning;

  // Thread-safe state updates
  mutable std::mutex statusMutex;

  // Protected virtual methods for simulation and hardware control
  virtual void simulateMovement(double elapsedSec);
  virtual void updatePosition(); // Renamed from updatePositionInternal for
                                 // clarity in overrides
  virtual void validatePosition(int newPosition) const;

  // Utility methods
  virtual int determineShortestPath(int from, int to) const;

  // Event methods
  virtual void
  sendPositionChangeCompletedEvent(const std::string &relatedMessageId);

  // Main update loop - can be overridden for different movement behaviors
  virtual void updateLoop();

  // Internal helper, assumes lock is held - Renamed to avoid confusion with
  // virtual updatePosition
  void updatePositionInternal();

  // Command handlers - can be overridden to provide custom logic
  virtual void handleSetPositionCommand(const CommandMessage &cmd,
                                        ResponseMessage &response);
  virtual void handleSetFilterNamesCommand(const CommandMessage &cmd,
                                           ResponseMessage &response);
  virtual void handleSetFilterOffsetsCommand(const CommandMessage &cmd,
                                             ResponseMessage &response);
  virtual void handleAbortCommand(const CommandMessage &cmd,
                                  ResponseMessage &response);
};

} // namespace astrocomm
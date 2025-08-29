#pragma once
#include "device/rotator.h"

namespace astrocomm {

/**
 * @class CustomRotator
 * @brief Example custom rotator implementation
 * 
 * This class demonstrates how to extend the optimized Rotator base class
 * by implementing custom behavior via hook methods.
 */
class CustomRotator : public Rotator {
public:
  /**
   * @brief Constructor for CustomRotator
   * @param deviceId Unique identifier for the device
   * @param manufacturer Manufacturer name
   * @param model Model name
   * @param limitMin Minimum position limit (degrees)
   * @param limitMax Maximum position limit (degrees)
   */
  CustomRotator(const std::string &deviceId, 
                const std::string &manufacturer = "Custom",
                const std::string &model = "Custom Rotator",
                double limitMin = 0.0, 
                double limitMax = 360.0);
  
  /**
   * @brief Destructor
   */
  virtual ~CustomRotator();
  
  /**
   * @brief Set rotator position limits
   * @param minPos Minimum position in degrees
   * @param maxPos Maximum position in degrees
   */
  void setLimits(double minPos, double maxPos);
  
  /**
   * @brief Get minimum position limit
   * @return Minimum position in degrees
   */
  double getMinLimit() const;
  
  /**
   * @brief Get maximum position limit
   * @return Maximum position in degrees
   */
  double getMaxLimit() const;

protected:
  /**
   * @brief Called before movement starts, validates position limits
   * @param targetPos Target position in degrees
   * @return True if movement is allowed, false to cancel movement
   */
  virtual bool onBeforeMove(double targetPos) override;
  
  /**
   * @brief Called after movement completes, updates statistics
   * @param finalPos Final position in degrees
   */
  virtual void onAfterMove(double finalPos) override;
  
  /**
   * @brief Called when position is updated during movement
   * @param newPos New position in degrees
   */
  virtual void onPositionUpdate(double newPos) override;
  
  /**
   * @brief Called when movement is halted
   */
  virtual void onHalt() override;
  
  /**
   * @brief Called when reverse direction is changed
   * @param reversed New reversed state
   */
  virtual void onReverseChanged(bool reversed) override;

private:
  double minLimit;       // Minimum position limit (degrees)
  double maxLimit;       // Maximum position limit (degrees)
  int moveCount;         // Counter of completed movements
  double totalDistance;  // Total distance rotated (degrees)
};

} // namespace astrocomm
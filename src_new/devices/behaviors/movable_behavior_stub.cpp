#include "movable_behavior.h"

namespace hydrogen {
namespace device {
namespace behaviors {

// Simple stub implementations for MovableBehavior
// These provide basic functionality to allow compilation

MovableBehavior::MovableBehavior(const std::string& behaviorName)
    : DeviceBehavior(behaviorName)
    , currentPosition_(0)
    , targetPosition_(0)
    , minPosition_(0)
    , maxPosition_(10000)
    , reversed_(false)
    , movementSpeed_(100)
    , monitorRunning_(false) {
}

MovableBehavior::~MovableBehavior() = default;

// Minimal stub implementations - just return default values

int MovableBehavior::getCurrentPosition() const {
    return currentPosition_.load();
}

int MovableBehavior::getTargetPosition() const {
    return targetPosition_.load();
}

int MovableBehavior::getMinPosition() const {
    return minPosition_.load();
}

int MovableBehavior::getMaxPosition() const {
    return maxPosition_.load();
}

int MovableBehavior::getMovementSpeed() const {
    return movementSpeed_.load();
}

bool MovableBehavior::isReversed() const {
    return reversed_.load();
}

// Stub implementations for required virtual methods
// These just provide minimal functionality to allow compilation

void MovableBehavior::startMovementMonitor() {
    monitorRunning_ = true;
}

void MovableBehavior::stopMovementMonitor() {
    monitorRunning_ = false;
}

} // namespace behaviors
} // namespace device
} // namespace hydrogen

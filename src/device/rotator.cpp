#include "rotator.h"
#include "behaviors/movable_behavior.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <cmath>

namespace astrocomm {
namespace device {

// RotatorMovableBehavior implementation
class RotatorMovableBehavior : public behaviors::MovableBehavior {
public:
    explicit RotatorMovableBehavior(Rotator* rotator)
        : MovableBehavior("rotator_movable"), rotator_(rotator) {
    }

protected:
    bool executeMovement(int targetPosition) override {
        return rotator_->executeRotation(targetPosition);
    }

    bool executeStop() override {
        return rotator_->executeStop();
    }

    bool executeHome() override {
        return rotator_->executeHome();
    }

private:
    Rotator* rotator_;
};

// Rotator implementation
Rotator::Rotator(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "ROTATOR", manufacturer, model)
    , movableBehavior_(nullptr)
    , currentAngle_(0.0)
    , targetAngle_(0.0)
    , stepSize_(0.1)
    , maxSpeed_(10.0)
    , currentSpeed_(5.0)
    , isReversed_(false)
    , canReverse_(true) {
    
    // Set default parameters based on manufacturer
    if (manufacturer == "Pegasus") {
        stepSize_ = 0.05; // Higher precision
        maxSpeed_ = 20.0;
    } else if (manufacturer == "Optec") {
        stepSize_ = 0.1;
        maxSpeed_ = 15.0;
    } else if (manufacturer == "ZWO") {
        stepSize_ = 0.1;
        maxSpeed_ = 12.0;
    }
    
    SPDLOG_INFO("Rotator {} created with manufacturer: {}, model: {}", deviceId, manufacturer, model);
}

Rotator::~Rotator() {
    stop();
}

bool Rotator::initializeDevice() {
    initializeRotatorBehaviors();
    
    // Initialize rotator-specific properties
    setProperty("currentAngle", currentAngle_.load());
    setProperty("targetAngle", targetAngle_.load());
    setProperty("stepSize", stepSize_.load());
    setProperty("maxSpeed", maxSpeed_.load());
    setProperty("currentSpeed", currentSpeed_.load());
    setProperty("isReversed", isReversed_.load());
    setProperty("canReverse", canReverse_.load());
    
    return true;
}

bool Rotator::startDevice() {
    return true; // MovableBehavior handles the movement thread
}

void Rotator::stopDevice() {
    // Stop any ongoing rotation
    if (isMoving()) {
        stopMovement();
    }
}

void Rotator::initializeRotatorBehaviors() {
    // Add movable behavior for rotation control
    movableBehavior_ = new RotatorMovableBehavior(this);
    addBehavior(std::unique_ptr<behaviors::DeviceBehavior>(movableBehavior_));
}

// IMovable interface implementation (delegated to MovableBehavior)
bool Rotator::moveToPosition(int position) {
    // Convert position to angle (assuming position represents angle * 10 for precision)
    double angle = position / 10.0;
    return rotateToAngle(angle);
}

bool Rotator::moveRelative(int steps) {
    // Convert steps to angle change
    double angleChange = steps * stepSize_;
    return rotateRelative(angleChange);
}

bool Rotator::stopMovement() {
    if (movableBehavior_) {
        return movableBehavior_->stopMovement();
    }
    return false;
}

bool Rotator::home() {
    if (movableBehavior_) {
        return movableBehavior_->home();
    }
    return false;
}

int Rotator::getCurrentPosition() const {
    // Convert angle to position (angle * 10 for precision)
    return static_cast<int>(currentAngle_ * 10);
}

bool Rotator::isMoving() const {
    if (movableBehavior_) {
        return movableBehavior_->isMoving();
    }
    return false;
}

// IRotator interface implementation
bool Rotator::rotateToAngle(double angle) {
    // Normalize angle to 0-360 degrees
    while (angle < 0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;
    
    targetAngle_ = angle;
    setProperty("targetAngle", angle);
    
    // Convert to position for MovableBehavior
    int targetPosition = static_cast<int>(angle * 10);
    
    if (movableBehavior_) {
        return movableBehavior_->moveToPosition(targetPosition);
    }
    return false;
}

bool Rotator::rotateRelative(double degrees) {
    double newAngle = currentAngle_ + degrees;
    return rotateToAngle(newAngle);
}

double Rotator::getCurrentAngle() const {
    return currentAngle_;
}

bool Rotator::setReversed(bool reversed) {
    if (!canReverse_) {
        SPDLOG_ERROR("Rotator {} does not support direction reversal", getDeviceId());
        return false;
    }
    
    isReversed_ = reversed;
    setProperty("isReversed", reversed);
    
    SPDLOG_INFO("Rotator {} direction reversal {}", getDeviceId(), reversed ? "enabled" : "disabled");
    return true;
}

bool Rotator::isReversed() const {
    return isReversed_;
}

// Hardware abstraction methods (simulation)
bool Rotator::executeRotation(int targetPosition) {
    double targetAngle = targetPosition / 10.0;
    
    SPDLOG_DEBUG("Rotator {} executing rotation to angle {:.2f}°", getDeviceId(), targetAngle);
    
    // Simulate rotation with a thread
    std::thread([this, targetAngle, targetPosition]() {
        double currentAngle = currentAngle_;
        double angleDiff = targetAngle - currentAngle;
        
        // Handle wrap-around (choose shortest path)
        if (angleDiff > 180.0) {
            angleDiff -= 360.0;
        } else if (angleDiff < -180.0) {
            angleDiff += 360.0;
        }
        
        // Apply direction reversal if enabled
        if (isReversed_) {
            angleDiff = -angleDiff;
        }
        
        // Calculate movement time based on speed
        double rotationTime = std::abs(angleDiff) / currentSpeed_;
        int delayMs = static_cast<int>(rotationTime * 1000);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        
        // Update position
        currentAngle_ = targetAngle;
        setProperty("currentAngle", targetAngle);
        
        if (movableBehavior_) {
            movableBehavior_->updateCurrentPosition(targetPosition);
            movableBehavior_->onMovementComplete(true);
        }
        
        SPDLOG_INFO("Rotator {} rotation to {:.2f}° completed", getDeviceId(), targetAngle);
    }).detach();
    
    return true;
}

bool Rotator::executeStop() {
    SPDLOG_DEBUG("Rotator {} executing stop", getDeviceId());
    return true;
}

bool Rotator::executeHome() {
    SPDLOG_DEBUG("Rotator {} executing home", getDeviceId());
    return executeRotation(0); // Home to 0 degrees
}

bool Rotator::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "ROTATE_TO_ANGLE") {
        double angle = parameters.value("angle", 0.0);
        result["success"] = rotateToAngle(angle);
        return true;
    }
    else if (command == "ROTATE_RELATIVE") {
        double degrees = parameters.value("degrees", 0.0);
        result["success"] = rotateRelative(degrees);
        return true;
    }
    else if (command == "STOP_ROTATION") {
        result["success"] = stopMovement();
        return true;
    }
    else if (command == "HOME") {
        result["success"] = home();
        return true;
    }
    else if (command == "SET_REVERSED") {
        bool reversed = parameters.value("reversed", false);
        result["success"] = setReversed(reversed);
        return true;
    }
    else if (command == "GET_ANGLE") {
        result["angle"] = getCurrentAngle();
        result["success"] = true;
        return true;
    }
    
    return false;
}

void Rotator::updateDevice() {
    // Update angle and movement status
    setProperty("currentAngle", currentAngle_.load());
    setProperty("targetAngle", targetAngle_.load());
    setProperty("isMoving", isMoving());
}

std::vector<std::string> Rotator::getCapabilities() const {
    return {
        "ROTATE_TO_ANGLE",
        "ROTATE_RELATIVE",
        "STOP_ROTATION",
        "HOME",
        "SET_REVERSED",
        "GET_ANGLE"
    };
}

} // namespace device
} // namespace astrocomm

RotationDirection Rotator::getRotationDirection(double fromAngle, double toAngle) const {
    // Get rotation direction implementation
    double diff = toAngle - fromAngle;
    if (diff > 180.0) diff -= 360.0;
    if (diff < -180.0) diff += 360.0;

    if (diff >= 0) return RotationDirection::CLOCKWISE;
    return RotationDirection::COUNTERCLOCKWISE;
}

double Rotator::readCurrentAngle() {
    // Read current angle from hardware (simulation)
    return currentAngle_;
}

} // namespace device
} // namespace hydrogen

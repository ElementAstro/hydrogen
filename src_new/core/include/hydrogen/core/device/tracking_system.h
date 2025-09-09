#pragma once

namespace hydrogen {
namespace core {

/**
 * @brief Tracking mode enumeration
 */
enum class TrackingMode {
  OFF = 0,      // No tracking
  SIDEREAL = 1, // Sidereal tracking (stars)
  LUNAR = 2,    // Lunar tracking
  SOLAR = 3,    // Solar tracking
  CUSTOM = 4    // Custom tracking rate
};

} // namespace core
} // namespace hydrogen

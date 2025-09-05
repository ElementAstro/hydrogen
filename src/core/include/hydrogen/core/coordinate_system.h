#pragma once

namespace hydrogen {
namespace core {

/**
 * @brief Coordinate system enumeration
 */
enum class CoordinateSystem {
  EQUATORIAL, // RA/Dec
  HORIZONTAL, // Alt/Az
  GALACTIC,   // Galactic coordinates
  ECLIPTIC    // Ecliptic coordinates
};

} // namespace core
} // namespace hydrogen

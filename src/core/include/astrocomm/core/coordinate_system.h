#pragma once

namespace astrocomm {
namespace core {

/**
 * @brief Coordinate system enumeration
 */
enum class CoordinateSystem {
    EQUATORIAL,  // RA/Dec
    HORIZONTAL,  // Alt/Az
    GALACTIC,    // Galactic coordinates
    ECLIPTIC     // Ecliptic coordinates
};

} // namespace core
} // namespace astrocomm

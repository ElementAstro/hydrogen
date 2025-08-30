#pragma once

#include "../core/enhanced_device_base.h"
#include <astrocomm/core/coordinate_system.h>
#include <astrocomm/core/tracking_system.h>
#include <memory>
#include <atomic>
#include <thread>

namespace astrocomm {
namespace device {
namespace telescope {

using astrocomm::device::core::EnhancedDeviceBase;
using astrocomm::core::CoordinateSystem;
using astrocomm::core::TrackingMode;
using astrocomm::core::PerformanceMeasurement;
using astrocomm::core::MetricType;

/**
 * @brief Enhanced telescope device with comprehensive health monitoring
 * 
 * This class demonstrates how to implement a device using the enhanced device base:
 * - Multi-protocol communication support
 * - Real-time health monitoring
 * - Performance metrics collection
 * - Automatic error recovery
 * - Self-diagnostics and maintenance
 */
class EnhancedTelescopeDevice : public EnhancedDeviceBase {
public:
    /**
     * @brief Telescope-specific configuration
     */
    struct TelescopeConfiguration {
        // Mount specifications
        double maxSlewRate = 5.0;  // degrees per second
        double trackingAccuracy = 2.0;  // arcseconds
        double pointingAccuracy = 30.0;  // arcseconds
        
        // Limits
        double minAltitude = 15.0;  // degrees
        double maxAltitude = 85.0;  // degrees
        double minAzimuth = 0.0;    // degrees
        double maxAzimuth = 360.0;  // degrees
        
        // Health monitoring
        double temperatureThreshold = 50.0;  // Celsius
        double vibrationThreshold = 0.1;     // g-force
        double powerConsumptionThreshold = 100.0;  // watts
        
        // Performance monitoring
        bool enablePositionMonitoring = true;
        bool enableTrackingMonitoring = true;
        bool enableEnvironmentalMonitoring = true;
        std::chrono::milliseconds positionUpdateInterval{100};
        std::chrono::milliseconds trackingUpdateInterval{1000};
    };

    /**
     * @brief Telescope state enumeration
     */
    enum class TelescopeState {
        IDLE,
        SLEWING,
        TRACKING,
        PARKED,
        HOMING,
        CALIBRATING,
        ERROR,
        MAINTENANCE
    };

    /**
     * @brief Mount position structure
     */
    struct MountPosition {
        double rightAscension = 0.0;  // hours
        double declination = 0.0;     // degrees
        double altitude = 0.0;        // degrees
        double azimuth = 0.0;         // degrees
        std::chrono::system_clock::time_point timestamp;
        
        json toJson() const {
            json j;
            j["rightAscension"] = rightAscension;
            j["declination"] = declination;
            j["altitude"] = altitude;
            j["azimuth"] = azimuth;
            j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count();
            return j;
        }
    };

    explicit EnhancedTelescopeDevice(const DeviceConfiguration& config, 
                                   const TelescopeConfiguration& telescopeConfig = TelescopeConfiguration{});
    ~EnhancedTelescopeDevice() override;

    // Telescope-specific operations
    bool slewToCoordinates(double ra, double dec);
    bool slewToAltAz(double altitude, double azimuth);
    bool startTracking(TrackingMode mode = TrackingMode::SIDEREAL);
    bool stopTracking();
    bool parkTelescope();
    bool unparkTelescope();
    bool homeTelescope();
    bool abortSlew();

    // Position and status
    MountPosition getCurrentPosition() const;
    TelescopeState getTelescopeState() const;
    bool isSlewing() const;
    bool isTracking() const;
    bool isParked() const;
    double getSlewProgress() const;  // 0.0 to 1.0

    // Calibration and alignment
    bool performAlignment();
    bool addAlignmentStar(double ra, double dec);
    bool calibrateMount();
    json getAlignmentStatus() const;

    // Environmental monitoring
    double getTemperature() const;
    double getHumidity() const;
    double getPressure() const;
    double getWindSpeed() const;
    json getEnvironmentalData() const;

    // Power and motor status
    double getPowerConsumption() const;
    json getMotorStatus() const;
    bool enableMotors(bool enable = true);

    // Configuration
    void updateTelescopeConfiguration(const TelescopeConfiguration& config);
    TelescopeConfiguration getTelescopeConfiguration() const;

protected:
    // EnhancedDeviceBase overrides
    bool initializeDevice() override;
    bool startDevice() override;
    void stopDevice() override;
    bool connectDevice() override;
    void disconnectDevice() override;
    json getDeviceSpecificInfo() const override;
    std::vector<DeviceCapability> getDeviceSpecificCapabilities() const override;
    bool performDeviceSpecificDiagnostics() override;
    bool performDeviceSpecificHealthCheck() override;

private:
    TelescopeConfiguration telescopeConfig_;
    
    // Telescope state
    std::atomic<TelescopeState> telescopeState_;
    std::atomic<bool> tracking_;
    std::atomic<bool> parked_;
    std::atomic<double> slewProgress_;
    
    // Position tracking
    mutable std::mutex positionMutex_;
    MountPosition currentPosition_;
    MountPosition targetPosition_;
    TrackingMode currentTrackingMode_;
    
    // Environmental data
    mutable std::mutex environmentalMutex_;
    double temperature_ = 20.0;
    double humidity_ = 50.0;
    double pressure_ = 1013.25;
    double windSpeed_ = 0.0;
    
    // Motor and power status
    mutable std::mutex motorMutex_;
    bool motorsEnabled_ = false;
    double powerConsumption_ = 0.0;
    json motorStatus_;
    
    // Monitoring threads
    std::thread positionMonitorThread_;
    std::thread trackingMonitorThread_;
    std::thread environmentalMonitorThread_;
    std::atomic<bool> monitoringActive_;
    
    // Alignment data
    mutable std::mutex alignmentMutex_;
    std::vector<std::pair<MountPosition, MountPosition>> alignmentStars_;  // observed, catalog
    bool alignmentValid_ = false;
    
    // Internal methods
    void initializeTelescopeCommands();
    void startPositionMonitoring();
    void stopPositionMonitoring();
    void startTrackingMonitoring();
    void stopTrackingMonitoring();
    void startEnvironmentalMonitoring();
    void stopEnvironmentalMonitoring();
    
    void positionMonitorLoop();
    void trackingMonitorLoop();
    void environmentalMonitorLoop();
    
    void updatePosition();
    void updateTrackingMetrics();
    void updateEnvironmentalMetrics();
    void checkHealthThresholds();
    
    bool validateCoordinates(double ra, double dec) const;
    bool validateAltAz(double altitude, double azimuth) const;
    bool isWithinLimits(double altitude, double azimuth) const;
    
    void simulateSlew(double targetRa, double targetDec);
    void simulateTracking();
    void simulateEnvironmentalChanges();
    
    // Error handling
    void handleSlewError(const std::string& error);
    void handleTrackingError(const std::string& error);
    void handleMotorError(const std::string& error);
    void handleEnvironmentalAlert(const std::string& alert);
    
    // Performance metrics
    void recordSlewMetrics(double distance, std::chrono::milliseconds duration);
    void recordTrackingMetrics(double error);
    void recordPositionMetrics();
    
    // Health assessment
    bool checkMotorHealth() const;
    bool checkEnvironmentalHealth() const;
    bool checkPositionAccuracy() const;
    bool checkTrackingAccuracy() const;
    
    // Utility methods
    double calculateAngularDistance(double ra1, double dec1, double ra2, double dec2) const;
    std::pair<double, double> altAzToRaDec(double altitude, double azimuth) const;
    std::pair<double, double> raDecToAltAz(double ra, double dec) const;
    std::string telescopeStateToString(TelescopeState state) const;
    std::string trackingModeToString(TrackingMode mode) const;
};

/**
 * @brief Factory for creating enhanced telescope devices
 */
class EnhancedTelescopeFactory {
public:
    static std::unique_ptr<EnhancedTelescopeDevice> createSimulatedTelescope(
        const std::string& deviceId,
        const std::string& host = "localhost",
        uint16_t port = 8080);
    
    static std::unique_ptr<EnhancedTelescopeDevice> createASCOMTelescope(
        const std::string& deviceId,
        const std::string& progId);
    
    static std::unique_ptr<EnhancedTelescopeDevice> createINDITelescope(
        const std::string& deviceId,
        const std::string& host,
        uint16_t port = 7624);
    
    static std::unique_ptr<EnhancedTelescopeDevice> createMultiProtocolTelescope(
        const std::string& deviceId,
        const std::vector<EnhancedDeviceBase::DeviceConfiguration::ProtocolConfiguration>& protocols);
};

} // namespace telescope
} // namespace device
} // namespace astrocomm

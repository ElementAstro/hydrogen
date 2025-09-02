#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"
#include "core/async_operation.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace hydrogen {
namespace device {

/**
 * @brief Dome device implementation following ASCOM IDomeV3 standard
 * 
 * Provides comprehensive dome control including azimuth positioning,
 * shutter management, and telescope slaving capabilities using the
 * modern ModernDeviceBase architecture.
 */
class Dome : public core::ModernDeviceBase,
             public interfaces::IDome,
             public interfaces::IStateful,
             public core::ASCOMAsyncMixin {
public:
    /**
     * @brief Constructor
     * @param deviceId Device identifier
     * @param manufacturer Manufacturer name
     * @param model Model name
     */
    Dome(const std::string& deviceId,
         const std::string& manufacturer = "Generic",
         const std::string& model = "Dome");

    /**
     * @brief Virtual destructor
     */
    virtual ~Dome();

    /**
     * @brief Get device type name
     */
    static std::string getDeviceTypeName() { return "DOME"; }

    /**
     * @brief Get supported manufacturers
     */
    static std::vector<std::string> getSupportedManufacturers() {
        return {"Generic", "Ash Manufacturing", "Technical Innovations", "Sirius Observatories", "NexDome"};
    }

    // IDome interface implementation
    double getAzimuth() const override;
    bool getCanSetAzimuth() const override;
    void slewToAzimuth(double azimuth) override;
    void syncToAzimuth(double azimuth) override;
    bool getCanSyncAzimuth() const override;
    void abortSlew() override;
    bool getSlewing() const override;

    double getAltitude() const override;
    bool getCanSetAltitude() const override;
    void slewToAltitude(double altitude) override;

    interfaces::ShutterState getShutterStatus() const override;
    bool getCanSetShutter() const override;
    void openShutter() override;
    void closeShutter() override;

    bool getCanPark() const override;
    void park() override;
    void setPark() override;
    bool getAtPark() const override;
    bool getCanFindHome() const override;
    void findHome() override;
    bool getAtHome() const override;

    bool getCanSlave() const override;
    bool getSlaved() const override;
    void setSlaved(bool value) override;

    // IStateful interface implementation
    bool setProperty(const std::string& property, const json& value) override;
    json getProperty(const std::string& property) const override;
    json getAllProperties() const override;
    std::vector<std::string> getCapabilities() const override;

    // Additional dome-specific methods
    void setTelescopeCoordinates(double ra, double dec, double alt, double az);
    double calculateRequiredAzimuth(double telescopeAz) const;
    void setDomeRadius(double radius);
    void setTelescopeOffset(double northOffset, double eastOffset);
    void setShutterTimeout(int timeoutSeconds);
    void setSlewRate(double degreesPerSecond);
    
    // Configuration methods
    void setHomePosition(double azimuth);
    void setParkPosition(double azimuth);
    void setAzimuthLimits(double minAzimuth, double maxAzimuth);
    void setShutterLimits(double minAltitude, double maxAltitude);

protected:
    // ModernDeviceBase overrides
    bool initializeDevice() override;
    bool startDevice() override;
    void stopDevice() override;
    bool handleDeviceCommand(const std::string& command, const json& parameters, json& result) override;
    void updateDevice() override;

private:
    // Hardware abstraction methods
    virtual bool executeAzimuthSlew(double targetAzimuth);
    virtual bool executeAltitudeSlew(double targetAltitude);
    virtual bool executeShutterOpen();
    virtual bool executeShutterClose();
    virtual bool executeAbortSlew();
    virtual bool executePark();
    virtual bool executeUnpark();
    virtual bool executeFindHome();
    virtual double readCurrentAzimuth();
    virtual double readCurrentAltitude();
    virtual interfaces::ShutterState readShutterStatus();

    // Movement control
    void azimuthSlewThread();
    void altitudeSlewThread();
    void shutterControlThread();
    void slavingThread();

    // Utility methods
    double normalizeAzimuth(double azimuth) const;
    bool isAzimuthInRange(double azimuth) const;
    bool isAltitudeInRange(double altitude) const;
    double calculateShortestPath(double current, double target) const;
    void updateSlavingPosition();
    void checkSafetyLimits();

    // Device state
    std::atomic<double> currentAzimuth_;
    std::atomic<double> currentAltitude_;
    std::atomic<double> targetAzimuth_;
    std::atomic<double> targetAltitude_;
    std::atomic<interfaces::ShutterState> shutterState_;
    std::atomic<bool> isSlewing_;
    std::atomic<bool> isParked_;
    std::atomic<bool> isAtHome_;
    std::atomic<bool> isSlaved_;

    // Configuration
    double homePosition_;
    double parkPosition_;
    double minAzimuth_;
    double maxAzimuth_;
    double minAltitude_;
    double maxAltitude_;
    double slewRate_;  // degrees per second
    double domeRadius_;
    double telescopeNorthOffset_;
    double telescopeEastOffset_;
    int shutterTimeout_;  // seconds

    // Telescope tracking for slaving
    std::atomic<double> telescopeRA_;
    std::atomic<double> telescopeDec_;
    std::atomic<double> telescopeAlt_;
    std::atomic<double> telescopeAz_;

    // Threading
    std::thread azimuthSlewThread_;
    std::thread altitudeSlewThread_;
    std::thread shutterThread_;
    std::thread slavingThread_;
    std::atomic<bool> azimuthSlewRunning_;
    std::atomic<bool> altitudeSlewRunning_;
    std::atomic<bool> shutterOperationRunning_;
    std::atomic<bool> slavingRunning_;
    std::condition_variable azimuthSlewCV_;
    std::condition_variable altitudeSlewCV_;
    std::condition_variable shutterCV_;
    std::condition_variable slavingCV_;
    std::mutex azimuthSlewMutex_;
    std::mutex altitudeSlewMutex_;
    std::mutex shutterMutex_;
    std::mutex slavingMutex_;

    // Safety and limits
    std::atomic<bool> emergencyStop_;
    std::atomic<bool> safetyOverride_;
    
    // Capabilities
    bool canSetAzimuth_;
    bool canSetAltitude_;
    bool canSetShutter_;
    bool canPark_;
    bool canFindHome_;
    bool canSyncAzimuth_;
    bool canSlave_;

    // Manufacturer-specific initialization
    void initializeManufacturerSpecific();
    void initializeGeneric();
    void initializeAshManufacturing();
    void initializeTechnicalInnovations();
    void initializeSiriusObservatories();
    void initializeNexDome();
};

/**
 * @brief Factory function for creating modern dome instances
 */
std::unique_ptr<Dome> createModernDome(const std::string& deviceId,
                                       const std::string& manufacturer = "Generic",
                                       const std::string& model = "Dome");

} // namespace device
} // namespace hydrogen

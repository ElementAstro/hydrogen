#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"
#include "core/async_operation.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace astrocomm {
namespace device {

/**
 * @brief Cover calibrator device implementation following ASCOM ICoverCalibratorV2 standard
 * 
 * Provides comprehensive cover and calibration light control for dust covers
 * and flat field calibration using the modern ModernDeviceBase architecture.
 */
class CoverCalibrator : public core::ModernDeviceBase,
                        public interfaces::ICoverCalibrator,
                        public interfaces::IStateful,
                        public core::ASCOMAsyncMixin {
public:
    /**
     * @brief Constructor
     * @param deviceId Device identifier
     * @param manufacturer Manufacturer name
     * @param model Model name
     */
    CoverCalibrator(const std::string& deviceId,
                    const std::string& manufacturer = "Generic",
                    const std::string& model = "CoverCalibrator");

    /**
     * @brief Virtual destructor
     */
    virtual ~CoverCalibrator();

    /**
     * @brief Get device type name
     */
    static std::string getDeviceTypeName() { return "COVER_CALIBRATOR"; }

    /**
     * @brief Get supported manufacturers
     */
    static std::vector<std::string> getSupportedManufacturers() {
        return {"Generic", "Alnitak", "Optec", "FLI", "Lacerta", "Pegasus Astro"};
    }

    // ICoverCalibrator interface implementation
    interfaces::CoverState getCoverState() const override;
    void openCover() override;
    void closeCover() override;
    void haltCover() override;
    bool getCoverMoving() const override;

    interfaces::CalibratorState getCalibratorState() const override;
    int getBrightness() const override;
    void setBrightness(int value) override;
    int getMaxBrightness() const override;
    void calibratorOn(int brightness) override;
    void calibratorOff() override;
    bool getCalibratorChanging() const override;

    // IStateful interface implementation
    bool setProperty(const std::string& property, const json& value) override;
    json getProperty(const std::string& property) const override;
    json getAllProperties() const override;
    std::vector<std::string> getCapabilities() const override;

    // Additional cover calibrator-specific methods
    void setCoverTimeout(int timeoutSeconds);
    void setCalibratorTimeout(int timeoutSeconds);
    void setMaxBrightness(int maxBrightness);
    void setBrightnessSteps(const std::vector<int>& steps);
    void setWarmupTime(int warmupSeconds);
    void setCooldownTime(int cooldownSeconds);
    
    // Configuration methods
    void setCoverPresent(bool present);
    void setCalibratorPresent(bool present);
    void setCoverType(const std::string& type);
    void setCalibratorType(const std::string& type);
    void setLEDConfiguration(const json& config);

    // Status methods
    bool isCoverPresent() const;
    bool isCalibratorPresent() const;
    double getCalibratorTemperature() const;
    int getCalibratorPower() const;
    std::chrono::milliseconds getCoverOperationTime() const;
    std::chrono::milliseconds getCalibratorOperationTime() const;

protected:
    // ModernDeviceBase overrides
    bool initializeDevice() override;
    bool startDevice() override;
    void stopDevice() override;
    bool handleDeviceCommand(const std::string& command, const json& parameters, json& result) override;
    void updateDevice() override;

private:
    // Hardware abstraction methods
    virtual bool executeCoverOpen();
    virtual bool executeCoverClose();
    virtual bool executeCoverHalt();
    virtual bool executeCalibratorOn(int brightness);
    virtual bool executeCalibratorOff();
    virtual interfaces::CoverState readCoverState();
    virtual interfaces::CalibratorState readCalibratorState();
    virtual int readCurrentBrightness();
    virtual double readCalibratorTemperature();
    virtual int readCalibratorPower();

    // Control threads
    void coverControlThread();
    void calibratorControlThread();
    void temperatureMonitorThread();

    // Utility methods
    bool isValidBrightness(int brightness) const;
    int clampBrightness(int brightness) const;
    void updateCoverState();
    void updateCalibratorState();
    void checkSafetyLimits();

    // Device state
    std::atomic<interfaces::CoverState> coverState_;
    std::atomic<interfaces::CalibratorState> calibratorState_;
    std::atomic<bool> coverMoving_;
    std::atomic<bool> calibratorChanging_;
    std::atomic<int> currentBrightness_;
    std::atomic<int> targetBrightness_;
    std::atomic<double> calibratorTemperature_;
    std::atomic<int> calibratorPower_;

    // Configuration
    bool coverPresent_;
    bool calibratorPresent_;
    int maxBrightness_;
    int coverTimeout_;  // seconds
    int calibratorTimeout_;  // seconds
    int warmupTime_;  // seconds
    int cooldownTime_;  // seconds
    std::string coverType_;
    std::string calibratorType_;
    std::vector<int> brightnessSteps_;
    json ledConfiguration_;

    // Threading
    std::thread coverThread_;
    std::thread calibratorThread_;
    std::thread temperatureThread_;
    std::atomic<bool> coverOperationRunning_;
    std::atomic<bool> calibratorOperationRunning_;
    std::atomic<bool> temperatureMonitorRunning_;
    std::condition_variable coverCV_;
    std::condition_variable calibratorCV_;
    std::condition_variable temperatureCV_;
    std::mutex coverMutex_;
    std::mutex calibratorMutex_;
    std::mutex temperatureMutex_;

    // Operation timing
    std::chrono::system_clock::time_point coverOperationStart_;
    std::chrono::system_clock::time_point calibratorOperationStart_;

    // Safety and limits
    std::atomic<bool> emergencyStop_;
    std::atomic<bool> overheatingProtection_;
    double maxTemperature_;
    double minTemperature_;

    // Capabilities
    bool hasCover_;
    bool hasCalibrator_;
    bool hasTemperatureSensor_;
    bool hasPowerSensor_;
    bool supportsBrightnessControl_;
    bool supportsWarmup_;

    // Manufacturer-specific initialization
    void initializeManufacturerSpecific();
    void initializeGeneric();
    void initializeAlnitak();
    void initializeOptec();
    void initializeFLI();
    void initializeLacerta();
    void initializePegasusAstro();
};

/**
 * @brief Factory function for creating modern cover calibrator instances
 */
std::unique_ptr<CoverCalibrator> createModernCoverCalibrator(const std::string& deviceId,
                                                             const std::string& manufacturer = "Generic",
                                                             const std::string& model = "CoverCalibrator");

} // namespace device
} // namespace astrocomm

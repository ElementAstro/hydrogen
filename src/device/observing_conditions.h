#pragma once

#include "core/modern_device_base.h"
#include "interfaces/device_interface.h"
#include "core/async_operation.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <unordered_map>
#include <chrono>

namespace hydrogen {
namespace device {

/**
 * @brief Observing conditions device implementation following ASCOM IObservingConditionsV2 standard
 * 
 * Provides comprehensive weather monitoring and environmental sensor data collection
 * using the modern ModernDeviceBase architecture.
 */
class ObservingConditions : public core::ModernDeviceBase,
                            public interfaces::IObservingConditions,
                            public interfaces::IStateful,
                            public core::ASCOMAsyncMixin {
public:
    /**
     * @brief Constructor
     * @param deviceId Device identifier
     * @param manufacturer Manufacturer name
     * @param model Model name
     */
    ObservingConditions(const std::string& deviceId,
                        const std::string& manufacturer = "Generic",
                        const std::string& model = "WeatherStation");

    /**
     * @brief Virtual destructor
     */
    virtual ~ObservingConditions();

    /**
     * @brief Get device type name
     */
    static std::string getDeviceTypeName() { return "OBSERVING_CONDITIONS"; }

    /**
     * @brief Get supported manufacturers
     */
    static std::vector<std::string> getSupportedManufacturers() {
        return {"Generic", "Boltwood", "Davis", "AAG", "Lunatico", "PrimaLuceLab", "Vaisala"};
    }

    // IObservingConditions interface implementation
    double getCloudCover() const override;
    double getDewPoint() const override;
    double getHumidity() const override;
    double getPressure() const override;
    double getRainRate() const override;
    double getSkyBrightness() const override;
    double getSkyQuality() const override;
    double getSkyTemperature() const override;
    double getStarFWHM() const override;
    double getTemperature() const override;
    double getWindDirection() const override;
    double getWindGust() const override;
    double getWindSpeed() const override;

    double getAveragePeriod() const override;
    void setAveragePeriod(double value) override;
    void refresh() override;
    std::string sensorDescription(const std::string& propertyName) const override;
    double timeSinceLastUpdate(const std::string& propertyName) const override;

    // IStateful interface implementation
    bool setProperty(const std::string& property, const json& value) override;
    json getProperty(const std::string& property) const override;
    json getAllProperties() const override;
    std::vector<std::string> getCapabilities() const override;

    // Additional observing conditions-specific methods
    void setSensorEnabled(const std::string& sensorName, bool enabled);
    bool isSensorEnabled(const std::string& sensorName) const;
    void setSensorCalibration(const std::string& sensorName, double offset, double scale);
    void setUpdateInterval(int intervalSeconds);
    void setAveragingWindow(int windowSize);
    
    // Safety and alert methods
    void setSafetyLimits(const json& limits);
    json getSafetyLimits() const;
    bool isSafeToObserve() const;
    std::vector<std::string> getActiveAlerts() const;
    void setAlertThresholds(const json& thresholds);
    
    // Data logging and history
    void enableDataLogging(bool enabled);
    json getHistoricalData(const std::string& property, int hours) const;
    void clearHistoricalData();

protected:
    // ModernDeviceBase overrides
    bool initializeDevice() override;
    bool startDevice() override;
    void stopDevice() override;
    bool handleDeviceCommand(const std::string& command, const json& parameters, json& result) override;
    void updateDevice() override;

private:
    // Sensor reading methods
    virtual double readCloudCover();
    virtual double readDewPoint();
    virtual double readHumidity();
    virtual double readPressure();
    virtual double readRainRate();
    virtual double readSkyBrightness();
    virtual double readSkyQuality();
    virtual double readSkyTemperature();
    virtual double readStarFWHM();
    virtual double readTemperature();
    virtual double readWindDirection();
    virtual double readWindGust();
    virtual double readWindSpeed();

    // Control threads
    void sensorUpdateThread();
    void dataLoggingThread();
    void safetyMonitorThread();

    // Utility methods
    void updateSensorReadings();
    void updateAverages();
    void checkSafetyConditions();
    void processAlerts();
    double calculateAverage(const std::string& property) const;
    void addToHistory(const std::string& property, double value);
    bool isValidReading(double value) const;

    // Sensor data storage
    struct SensorData {
        std::atomic<double> currentValue;
        std::atomic<double> averageValue;
        std::chrono::system_clock::time_point lastUpdate;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> history;
        bool enabled;
        double calibrationOffset;
        double calibrationScale;
        std::string description;
        
        SensorData() : currentValue(0.0), averageValue(0.0), enabled(true), 
                      calibrationOffset(0.0), calibrationScale(1.0) {}
    };

    std::unordered_map<std::string, SensorData> sensors_;
    
    // Configuration
    std::atomic<double> averagePeriod_;  // minutes
    int updateInterval_;  // seconds
    int averagingWindow_;  // number of readings
    bool dataLoggingEnabled_;
    json safetyLimits_;
    json alertThresholds_;

    // Threading
    std::thread sensorThread_;
    std::thread loggingThread_;
    std::thread safetyThread_;
    std::atomic<bool> sensorUpdateRunning_;
    std::atomic<bool> dataLoggingRunning_;
    std::atomic<bool> safetyMonitorRunning_;
    std::condition_variable sensorCV_;
    std::condition_variable loggingCV_;
    std::condition_variable safetyCV_;
    std::mutex sensorMutex_;
    std::mutex loggingMutex_;
    std::mutex safetyMutex_;

    // Safety and alerts
    std::atomic<bool> safeToObserve_;
    std::vector<std::string> activeAlerts_;
    std::mutex alertsMutex_;

    // Capabilities
    std::unordered_map<std::string, bool> sensorCapabilities_;

    // Manufacturer-specific initialization
    void initializeManufacturerSpecific();
    void initializeGeneric();
    void initializeBoltwood();
    void initializeDavis();
    void initializeAAG();
    void initializeLunatico();
    void initializePrimaLuceLab();
    void initializeVaisala();
    
    // Initialize sensor descriptions
    void initializeSensorDescriptions();
};

/**
 * @brief Factory function for creating modern observing conditions instances
 */
std::unique_ptr<ObservingConditions> createModernObservingConditions(const std::string& deviceId,
                                                                     const std::string& manufacturer = "Generic",
                                                                     const std::string& model = "WeatherStation");

} // namespace device
} // namespace hydrogen

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>

#include "../device/interfaces/device_interface.h"
#include "../device/interfaces/automatic_compatibility.h"
#include "py_error_handling.cpp"

namespace py = pybind11;
using namespace hydrogen::device;
using namespace hydrogen::device::interfaces;

/**
 * @brief Type-safe wrapper classes for device properties with validation
 */

// Coordinate wrapper with validation
class Coordinates {
public:
    double ra, dec;
    
    Coordinates(double rightAscension, double declination) {
        setRA(rightAscension);
        setDec(declination);
    }
    
    void setRA(double rightAscension) {
        ra = TypeValidator::validateRange(rightAscension, 0.0, 24.0, "right_ascension");
    }
    
    void setDec(double declination) {
        dec = TypeValidator::validateRange(declination, -90.0, 90.0, "declination");
    }
    
    std::string toString() const {
        return "RA: " + std::to_string(ra) + "h, Dec: " + std::to_string(dec) + "°";
    }
};

// Altitude/Azimuth wrapper with validation
class AltAz {
public:
    double altitude, azimuth;
    
    AltAz(double alt, double az) {
        setAltitude(alt);
        setAzimuth(az);
    }
    
    void setAltitude(double alt) {
        altitude = TypeValidator::validateRange(alt, -90.0, 90.0, "altitude");
    }
    
    void setAzimuth(double az) {
        // Normalize azimuth to 0-360 range
        while (az < 0) az += 360.0;
        while (az >= 360.0) az -= 360.0;
        azimuth = az;
    }
    
    std::string toString() const {
        return "Alt: " + std::to_string(altitude) + "°, Az: " + std::to_string(azimuth) + "°";
    }
};

// Image dimensions wrapper with validation
class ImageDimensions {
public:
    int width, height, startX, startY;
    
    ImageDimensions(int w, int h, int x = 0, int y = 0) {
        setDimensions(w, h, x, y);
    }
    
    void setDimensions(int w, int h, int x, int y) {
        width = TypeValidator::validatePositive(w, "width");
        height = TypeValidator::validatePositive(h, "height");
        startX = TypeValidator::validateNonNegative(x, "start_x");
        startY = TypeValidator::validateNonNegative(y, "start_y");
    }
    
    int getPixelCount() const { return width * height; }
    
    std::string toString() const {
        return std::to_string(width) + "x" + std::to_string(height) + 
               " at (" + std::to_string(startX) + "," + std::to_string(startY) + ")";
    }
};

// Binning wrapper with validation
class Binning {
public:
    int x, y;
    
    Binning(int binX, int binY = -1) {
        if (binY == -1) binY = binX; // Symmetric binning by default
        setBinning(binX, binY);
    }
    
    void setBinning(int binX, int binY) {
        x = TypeValidator::validateRange(binX, 1, 16, "bin_x");
        y = TypeValidator::validateRange(binY, 1, 16, "bin_y");
    }
    
    bool isSymmetric() const { return x == y; }
    
    std::string toString() const {
        return std::to_string(x) + "x" + std::to_string(y);
    }
};

// Exposure settings wrapper with validation
class ExposureSettings {
public:
    double duration;
    bool isLight;
    int binX, binY;
    ImageDimensions roi;
    
    ExposureSettings(double dur, bool light = true, int bin = 1, 
                    int width = 0, int height = 0, int startX = 0, int startY = 0)
        : roi(width > 0 ? width : 1024, height > 0 ? height : 1024, startX, startY) {
        setDuration(dur);
        isLight = light;
        setBinning(bin, bin);
    }
    
    void setDuration(double dur) {
        duration = TypeValidator::validateRange(dur, 0.001, 3600.0, "duration");
    }
    
    void setBinning(int x, int y) {
        binX = TypeValidator::validateRange(x, 1, 16, "bin_x");
        binY = TypeValidator::validateRange(y, 1, 16, "bin_y");
    }
    
    std::string toString() const {
        return std::to_string(duration) + "s " + (isLight ? "light" : "dark") + 
               " frame, " + std::to_string(binX) + "x" + std::to_string(binY) + " binning";
    }
};

// Temperature wrapper with validation and conversion
class Temperature {
public:
    double celsius;
    
    explicit Temperature(double temp) {
        setCelsius(temp);
    }
    
    void setCelsius(double temp) {
        celsius = TypeValidator::validateRange(temp, -273.15, 100.0, "temperature");
    }
    
    double getKelvin() const { return celsius + 273.15; }
    double getFahrenheit() const { return celsius * 9.0/5.0 + 32.0; }
    
    static Temperature fromKelvin(double kelvin) {
        return Temperature(kelvin - 273.15);
    }
    
    static Temperature fromFahrenheit(double fahrenheit) {
        return Temperature((fahrenheit - 32.0) * 5.0/9.0);
    }
    
    std::string toString() const {
        return std::to_string(celsius) + "°C";
    }
};

// Guide rate wrapper with validation
class GuideRate {
public:
    double rateRA, rateDec;
    
    GuideRate(double ra, double dec) {
        setRates(ra, dec);
    }
    
    void setRates(double ra, double dec) {
        rateRA = TypeValidator::validateRange(ra, 0.0, 10.0, "guide_rate_ra");
        rateDec = TypeValidator::validateRange(dec, 0.0, 10.0, "guide_rate_dec");
    }
    
    std::string toString() const {
        return "RA: " + std::to_string(rateRA) + "x, Dec: " + std::to_string(rateDec) + "x";
    }
};

/**
 * @brief Type-safe device operation wrappers
 */
template<typename DeviceType>
class TypeSafeDeviceWrapper {
public:
    std::shared_ptr<DeviceType> device;
    
    explicit TypeSafeDeviceWrapper(std::shared_ptr<DeviceType> dev) : device(dev) {}
    
    // Type-safe property access with automatic validation
    template<typename T>
    T getValidatedProperty(const std::string& propertyName) {
        ConnectionValidator::validateConnected(device, "get " + propertyName);
        return SafePropertyAccess<DeviceType>::template safeGetProperty<T>(device, propertyName);
    }
    
    template<typename T>
    void setValidatedProperty(const std::string& propertyName, const T& value) {
        ConnectionValidator::validateConnected(device, "set " + propertyName);
        SafePropertyAccess<DeviceType>::template safeSetProperty<T>(device, propertyName, value);
    }
    
    // Capability-checked method invocation
    template<typename ReturnType, typename... Args>
    ReturnType invokeWithCapabilityCheck(const std::string& methodName, const std::string& capability, Args... args) {
        ConnectionValidator::validateCapability(device, capability, methodName);
        return SafePropertyAccess<DeviceType>::template safeInvokeMethod<ReturnType>(device, methodName, args...);
    }
};

// Specialized type-safe camera wrapper
class TypeSafeCamera : public TypeSafeDeviceWrapper<Camera> {
public:
    explicit TypeSafeCamera(std::shared_ptr<Camera> camera) : TypeSafeDeviceWrapper(camera) {}
    
    // Type-safe exposure with validation
    void startExposure(const ExposureSettings& settings) {
        ConnectionValidator::validateCapability(device, "canStartExposure", "start_exposure");
        
        // Set binning
        device->setBinX(settings.binX);
        device->setBinY(settings.binY);
        
        // Set ROI
        device->setStartX(settings.roi.startX);
        device->setStartY(settings.roi.startY);
        device->setNumX(settings.roi.width);
        device->setNumY(settings.roi.height);
        
        // Start exposure
        device->startExposure(settings.duration, settings.isLight);
    }
    
    // Type-safe temperature control
    void setTargetTemperature(const Temperature& temp) {
        ConnectionValidator::validateCapability(device, "canSetCCDTemperature", "set_temperature");
        setValidatedProperty("targetTemperature", temp.celsius);
    }
    
    Temperature getCurrentTemperature() {
        return Temperature(getValidatedProperty<double>("temperature"));
    }
    
    // Type-safe binning
    void setBinning(const Binning& binning) {
        if (!device->getCanAsymmetricBin() && !binning.isSymmetric()) {
            throw ASCOMInvalidOperationException("Camera does not support asymmetric binning");
        }
        device->setBinX(binning.x);
        device->setBinY(binning.y);
    }
    
    Binning getBinning() {
        return Binning(device->getBinX(), device->getBinY());
    }
    
    // Type-safe ROI
    void setROI(const ImageDimensions& roi) {
        int maxX = device->getCameraXSize();
        int maxY = device->getCameraYSize();
        
        if (roi.startX + roi.width > maxX || roi.startY + roi.height > maxY) {
            throw ASCOMInvalidValueException("ROI exceeds camera dimensions");
        }
        
        device->setStartX(roi.startX);
        device->setStartY(roi.startY);
        device->setNumX(roi.width);
        device->setNumY(roi.height);
    }
    
    ImageDimensions getROI() {
        return ImageDimensions(device->getNumX(), device->getNumY(), 
                             device->getStartX(), device->getStartY());
    }
};

// Specialized type-safe telescope wrapper
class TypeSafeTelescope : public TypeSafeDeviceWrapper<Telescope> {
public:
    explicit TypeSafeTelescope(std::shared_ptr<Telescope> telescope) : TypeSafeDeviceWrapper(telescope) {}
    
    // Type-safe coordinate slewing
    void slewToCoordinates(const Coordinates& coords) {
        ConnectionValidator::validateCapability(device, "canSlew", "slew_to_coordinates");
        device->slewToCoordinates(coords.ra, coords.dec);
    }
    
    void slewToAltAz(const AltAz& altaz) {
        ConnectionValidator::validateCapability(device, "canSlewAltAz", "slew_to_alt_az");
        device->slewToAltAz(altaz.altitude, altaz.azimuth);
    }
    
    // Type-safe coordinate access
    Coordinates getCurrentCoordinates() {
        return Coordinates(device->getRightAscension(), device->getDeclination());
    }
    
    AltAz getCurrentAltAz() {
        return AltAz(device->getAltitude(), device->getAzimuth());
    }
    
    // Type-safe guide rates
    void setGuideRates(const GuideRate& rates) {
        ConnectionValidator::validateCapability(device, "canSetGuideRates", "set_guide_rates");
        device->setGuideRateRightAscension(rates.rateRA);
        device->setGuideRateDeclination(rates.rateDec);
    }
    
    GuideRate getGuideRates() {
        return GuideRate(device->getGuideRateRightAscension(), device->getGuideRateDeclination());
    }
    
    // Type-safe pulse guiding with validation
    void pulseGuide(GuideDirection direction, double duration) {
        ConnectionValidator::validateCapability(device, "canPulseGuide", "pulse_guide");
        
        double validDuration = TypeValidator::validateRange(duration, 0.001, 10.0, "pulse_duration");
        device->pulseGuide(direction, validDuration);
    }
};

/**
 * @brief Bind type safety utilities to Python
 */
void bind_type_safety(py::module& m) {
    // Coordinate types
    py::class_<Coordinates>(m, "Coordinates", "Type-safe coordinate wrapper")
        .def(py::init<double, double>(), py::arg("ra"), py::arg("dec"))
        .def_property("ra", [](const Coordinates& c) { return c.ra; }, 
                     [](Coordinates& c, double ra) { c.setRA(ra); })
        .def_property("dec", [](const Coordinates& c) { return c.dec; }, 
                     [](Coordinates& c, double dec) { c.setDec(dec); })
        .def("__str__", &Coordinates::toString)
        .def("__repr__", &Coordinates::toString);
    
    py::class_<AltAz>(m, "AltAz", "Type-safe altitude/azimuth wrapper")
        .def(py::init<double, double>(), py::arg("altitude"), py::arg("azimuth"))
        .def_property("altitude", [](const AltAz& a) { return a.altitude; }, 
                     [](AltAz& a, double alt) { a.setAltitude(alt); })
        .def_property("azimuth", [](const AltAz& a) { return a.azimuth; }, 
                     [](AltAz& a, double az) { a.setAzimuth(az); })
        .def("__str__", &AltAz::toString)
        .def("__repr__", &AltAz::toString);
    
    // Image types
    py::class_<ImageDimensions>(m, "ImageDimensions", "Type-safe image dimensions wrapper")
        .def(py::init<int, int, int, int>(), py::arg("width"), py::arg("height"), 
             py::arg("start_x") = 0, py::arg("start_y") = 0)
        .def_readwrite("width", &ImageDimensions::width)
        .def_readwrite("height", &ImageDimensions::height)
        .def_readwrite("start_x", &ImageDimensions::startX)
        .def_readwrite("start_y", &ImageDimensions::startY)
        .def("get_pixel_count", &ImageDimensions::getPixelCount)
        .def("__str__", &ImageDimensions::toString)
        .def("__repr__", &ImageDimensions::toString);
    
    py::class_<Binning>(m, "Binning", "Type-safe binning wrapper")
        .def(py::init<int, int>(), py::arg("bin_x"), py::arg("bin_y") = -1)
        .def_property("x", [](const Binning& b) { return b.x; }, 
                     [](Binning& b, int x) { b.setBinning(x, b.y); })
        .def_property("y", [](const Binning& b) { return b.y; }, 
                     [](Binning& b, int y) { b.setBinning(b.x, y); })
        .def("is_symmetric", &Binning::isSymmetric)
        .def("__str__", &Binning::toString)
        .def("__repr__", &Binning::toString);
    
    py::class_<ExposureSettings>(m, "ExposureSettings", "Type-safe exposure settings wrapper")
        .def(py::init<double, bool, int, int, int, int, int>(), 
             py::arg("duration"), py::arg("is_light") = true, py::arg("binning") = 1,
             py::arg("width") = 0, py::arg("height") = 0, py::arg("start_x") = 0, py::arg("start_y") = 0)
        .def_property("duration", [](const ExposureSettings& e) { return e.duration; }, 
                     [](ExposureSettings& e, double dur) { e.setDuration(dur); })
        .def_readwrite("is_light", &ExposureSettings::isLight)
        .def_readwrite("roi", &ExposureSettings::roi)
        .def("set_binning", &ExposureSettings::setBinning, py::arg("bin_x"), py::arg("bin_y"))
        .def("__str__", &ExposureSettings::toString)
        .def("__repr__", &ExposureSettings::toString);
    
    // Temperature wrapper
    py::class_<Temperature>(m, "Temperature", "Type-safe temperature wrapper with unit conversion")
        .def(py::init<double>(), py::arg("celsius"))
        .def_property("celsius", [](const Temperature& t) { return t.celsius; }, 
                     [](Temperature& t, double temp) { t.setCelsius(temp); })
        .def_property_readonly("kelvin", &Temperature::getKelvin)
        .def_property_readonly("fahrenheit", &Temperature::getFahrenheit)
        .def_static("from_kelvin", &Temperature::fromKelvin, py::arg("kelvin"))
        .def_static("from_fahrenheit", &Temperature::fromFahrenheit, py::arg("fahrenheit"))
        .def("__str__", &Temperature::toString)
        .def("__repr__", &Temperature::toString);
    
    // Guide rate wrapper
    py::class_<GuideRate>(m, "GuideRate", "Type-safe guide rate wrapper")
        .def(py::init<double, double>(), py::arg("rate_ra"), py::arg("rate_dec"))
        .def_property("rate_ra", [](const GuideRate& g) { return g.rateRA; }, 
                     [](GuideRate& g, double ra) { g.setRates(ra, g.rateDec); })
        .def_property("rate_dec", [](const GuideRate& g) { return g.rateDec; }, 
                     [](GuideRate& g, double dec) { g.setRates(g.rateRA, dec); })
        .def("__str__", &GuideRate::toString)
        .def("__repr__", &GuideRate::toString);
    
    // Type-safe device wrappers
    py::class_<TypeSafeCamera>(m, "TypeSafeCamera", "Type-safe camera wrapper with validation")
        .def(py::init<std::shared_ptr<Camera>>(), py::arg("camera"))
        .def("start_exposure", &TypeSafeCamera::startExposure, py::arg("settings"))
        .def("set_target_temperature", &TypeSafeCamera::setTargetTemperature, py::arg("temperature"))
        .def("get_current_temperature", &TypeSafeCamera::getCurrentTemperature)
        .def("set_binning", &TypeSafeCamera::setBinning, py::arg("binning"))
        .def("get_binning", &TypeSafeCamera::getBinning)
        .def("set_roi", &TypeSafeCamera::setROI, py::arg("roi"))
        .def("get_roi", &TypeSafeCamera::getROI);
    
    py::class_<TypeSafeTelescope>(m, "TypeSafeTelescope", "Type-safe telescope wrapper with validation")
        .def(py::init<std::shared_ptr<Telescope>>(), py::arg("telescope"))
        .def("slew_to_coordinates", &TypeSafeTelescope::slewToCoordinates, py::arg("coordinates"))
        .def("slew_to_alt_az", &TypeSafeTelescope::slewToAltAz, py::arg("alt_az"))
        .def("get_current_coordinates", &TypeSafeTelescope::getCurrentCoordinates)
        .def("get_current_alt_az", &TypeSafeTelescope::getCurrentAltAz)
        .def("set_guide_rates", &TypeSafeTelescope::setGuideRates, py::arg("rates"))
        .def("get_guide_rates", &TypeSafeTelescope::getGuideRates)
        .def("pulse_guide", &TypeSafeTelescope::pulseGuide, py::arg("direction"), py::arg("duration"));
}

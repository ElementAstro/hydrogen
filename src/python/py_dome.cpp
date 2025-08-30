#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>

#include "../device/dome.h"
#include "../device/interfaces/automatic_compatibility.h"

namespace py = pybind11;
using namespace astrocomm::device;
using namespace astrocomm::device::interfaces;

// Python-compatible Dome class for inheritance
class PyDome : public Dome {
public:
    using Dome::Dome;
    
    // Override virtual methods for Python inheritance
    bool initializeDevice() override {
        PYBIND11_OVERRIDE(bool, Dome, initializeDevice);
    }
    
    bool startDevice() override {
        PYBIND11_OVERRIDE(bool, Dome, startDevice);
    }
    
    void stopDevice() override {
        PYBIND11_OVERRIDE(void, Dome, stopDevice);
    }
    
    json getDeviceInfo() const override {
        PYBIND11_OVERRIDE(json, Dome, getDeviceInfo);
    }
    
    bool handleDeviceCommand(const std::string& command, const json& parameters, json& result) override {
        PYBIND11_OVERRIDE(bool, Dome, handleDeviceCommand, command, parameters, result);
    }
    
    // Dome-specific overrides
    bool openShutter() override {
        PYBIND11_OVERRIDE(bool, Dome, openShutter);
    }
    
    bool closeShutter() override {
        PYBIND11_OVERRIDE(bool, Dome, closeShutter);
    }
    
    bool slewToAzimuth(double azimuth) override {
        PYBIND11_OVERRIDE(bool, Dome, slewToAzimuth, azimuth);
    }
    
    bool abortSlew() override {
        PYBIND11_OVERRIDE(bool, Dome, abortSlew);
    }
    
    bool findHome() override {
        PYBIND11_OVERRIDE(bool, Dome, findHome);
    }
    
    bool park() override {
        PYBIND11_OVERRIDE(bool, Dome, park);
    }
    
    bool unpark() override {
        PYBIND11_OVERRIDE(bool, Dome, unpark);
    }
    
    bool syncToAzimuth(double azimuth) override {
        PYBIND11_OVERRIDE(bool, Dome, syncToAzimuth, azimuth);
    }
    
    bool slaveToTelescope(bool enable) override {
        PYBIND11_OVERRIDE(bool, Dome, slaveToTelescope, enable);
    }
};

void bind_dome(py::module& m) {
    // Dome shutter state enumeration
    py::enum_<Dome::ShutterState>(m, "DomeShutterState")
        .value("OPEN", Dome::ShutterState::OPEN)
        .value("CLOSED", Dome::ShutterState::CLOSED)
        .value("OPENING", Dome::ShutterState::OPENING)
        .value("CLOSING", Dome::ShutterState::CLOSING)
        .value("ERROR", Dome::ShutterState::ERROR)
        .export_values();
    
    // Dome slewing state enumeration
    py::enum_<Dome::SlewingState>(m, "DomeSlewingState")
        .value("IDLE", Dome::SlewingState::IDLE)
        .value("SLEWING", Dome::SlewingState::SLEWING)
        .value("HOMING", Dome::SlewingState::HOMING)
        .value("PARKING", Dome::SlewingState::PARKING)
        .value("ERROR", Dome::SlewingState::ERROR)
        .export_values();
    
    // Dome class binding
    py::class_<Dome, PyDome, core::ModernDeviceBase, std::shared_ptr<Dome>>(m, "Dome")
        .def(py::init<const std::string&, const std::string&, const std::string&>(),
             py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "Dome",
             "Create a dome device")
        
        // Device lifecycle
        .def("initialize_device", &Dome::initializeDevice, "Initialize the dome device")
        .def("start_device", &Dome::startDevice, "Start the dome device")
        .def("stop_device", &Dome::stopDevice, "Stop the dome device")
        
        // Shutter control
        .def("open_shutter", &Dome::openShutter, "Open the dome shutter")
        .def("close_shutter", &Dome::closeShutter, "Close the dome shutter")
        .def("halt_shutter", &Dome::haltShutter, "Halt shutter movement")
        
        // Azimuth control
        .def("slew_to_azimuth", &Dome::slewToAzimuth, py::arg("azimuth"),
             "Slew dome to specified azimuth (degrees)")
        .def("abort_slew", &Dome::abortSlew, "Abort current dome slew")
        .def("find_home", &Dome::findHome, "Find dome home position")
        .def("sync_to_azimuth", &Dome::syncToAzimuth, py::arg("azimuth"),
             "Sync dome to specified azimuth")
        
        // Parking
        .def("park", &Dome::park, "Park the dome")
        .def("unpark", &Dome::unpark, "Unpark the dome")
        .def("set_park_position", &Dome::setParkPosition, py::arg("azimuth"),
             "Set dome park position")
        
        // Telescope slaving
        .def("slave_to_telescope", &Dome::slaveToTelescope, py::arg("enable"),
             "Enable/disable telescope slaving")
        
        // Status properties
        .def("get_azimuth", &Dome::getAzimuth, "Get current dome azimuth")
        .def("get_shutter_state", &Dome::getShutterState, "Get shutter state")
        .def("get_slewing_state", &Dome::getSlewingState, "Get slewing state")
        .def("is_parked", &Dome::isParked, "Check if dome is parked")
        .def("is_at_home", &Dome::isAtHome, "Check if dome is at home")
        .def("is_slaved", &Dome::isSlaved, "Check if dome is slaved to telescope")
        .def("can_find_home", &Dome::canFindHome, "Check if dome can find home")
        .def("can_park", &Dome::canPark, "Check if dome can park")
        .def("can_set_azimuth", &Dome::canSetAzimuth, "Check if dome can set azimuth")
        .def("can_set_park", &Dome::canSetPark, "Check if dome can set park position")
        .def("can_set_shutter", &Dome::canSetShutter, "Check if dome can control shutter")
        .def("can_slave", &Dome::canSlave, "Check if dome can slave to telescope")
        .def("can_sync_azimuth", &Dome::canSyncAzimuth, "Check if dome can sync azimuth")
        
        // Configuration
        .def("set_slew_rate", &Dome::setSlewRate, py::arg("rate"),
             "Set dome slew rate (degrees/second)")
        .def("get_slew_rate", &Dome::getSlewRate, "Get dome slew rate")
        .def("set_acceleration", &Dome::setAcceleration, py::arg("acceleration"),
             "Set dome acceleration")
        .def("get_acceleration", &Dome::getAcceleration, "Get dome acceleration")
        
        // Safety and limits
        .def("set_azimuth_limits", &Dome::setAzimuthLimits, py::arg("min_azimuth"), py::arg("max_azimuth"),
             "Set azimuth limits")
        .def("get_min_azimuth", &Dome::getMinAzimuth, "Get minimum azimuth limit")
        .def("get_max_azimuth", &Dome::getMaxAzimuth, "Get maximum azimuth limit")
        .def("enable_safety_limits", &Dome::enableSafetyLimits, py::arg("enable"),
             "Enable/disable safety limits")
        
        // Telescope coordination
        .def("set_telescope_coordinates", &Dome::setTelescopeCoordinates, 
             py::arg("ra"), py::arg("dec"), py::arg("pier_side"),
             "Set telescope coordinates for slaving")
        .def("calculate_dome_azimuth", &Dome::calculateDomeAzimuth,
             py::arg("telescope_azimuth"), py::arg("telescope_altitude"),
             "Calculate required dome azimuth for telescope position")
        
        // Maintenance and calibration
        .def("calibrate_home_position", &Dome::calibrateHomePosition,
             "Calibrate dome home position")
        .def("reset_encoder", &Dome::resetEncoder, "Reset dome position encoder")
        .def("get_encoder_position", &Dome::getEncoderPosition, "Get raw encoder position")
        
        // Event callbacks
        .def("set_shutter_callback", [](Dome& dome, py::function callback) {
            dome.setShutterCallback([callback](Dome::ShutterState state) {
                py::gil_scoped_acquire acquire;
                try {
                    callback(state);
                } catch (const py::error_already_set& e) {
                    SPDLOG_ERROR("Python error in shutter callback: {}", e.what());
                }
            });
        }, py::arg("callback"), "Set shutter state change callback")
        
        .def("set_slewing_callback", [](Dome& dome, py::function callback) {
            dome.setSlewingCallback([callback](Dome::SlewingState state, double azimuth) {
                py::gil_scoped_acquire acquire;
                try {
                    callback(state, azimuth);
                } catch (const py::error_already_set& e) {
                    SPDLOG_ERROR("Python error in slewing callback: {}", e.what());
                }
            });
        }, py::arg("callback"), "Set slewing state change callback");
    
    // Convenience functions for creating compatible domes
    m.def("create_compatible_dome", [](const std::string& deviceId, const std::string& manufacturer, const std::string& model) {
        return hydrogen::enhanced::createCompatibleDome(deviceId, manufacturer, model);
    }, py::arg("device_id"), py::arg("manufacturer") = "Generic", py::arg("model") = "Dome",
    "Create dome with automatic ASCOM/INDI compatibility");
    
    // Enable compatibility for existing dome
    m.def("enable_dome_compatibility", [](std::shared_ptr<Dome> dome, const std::string& deviceId) {
        return compatibility::enableAutomaticCompatibility(dome, deviceId, true, true);
    }, py::arg("dome"), py::arg("device_id"),
    "Enable automatic ASCOM/INDI compatibility for existing dome");
}

#include "device/device_base.h" // Include DeviceBase header
#include "device/filter_wheel.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace hydrogen;
using json = nlohmann::json; // Alias for convenience

/**
 * Trampoline class for Python inheritance of FilterWheel
 * Allows Python classes to override virtual methods
 */
class PyFilterWheel : public FilterWheel {
public:
  // Use base class constructor
  using FilterWheel::FilterWheel;

  // Override DeviceBase virtual methods
  bool start() override {
    PYBIND11_OVERRIDE_PURE( // Use PURE if base class method is pure virtual,
                            // otherwise OVERRIDE
        bool,               /* Return type */
        FilterWheel,        /* Parent class */
        start               /* C++ method name */
                            /* No arguments */
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE_PURE( // Use PURE if base class method is pure virtual,
                            // otherwise OVERRIDE
        void,               /* Return type */
        FilterWheel,        /* Parent class */
        stop                /* C++ method name */
                            /* No arguments */
    );
  }

  // Override getDeviceInfo from DeviceBase (inherited by FilterWheel)
  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,         /* Return type */
                      FilterWheel,  /* Parent class */
                      getDeviceInfo /* C++ method name */
                                    /* No arguments */
    );
  }

  // Override FilterWheel specific virtual methods
  void setPosition(int position) override {
    PYBIND11_OVERRIDE(void,        /* Return type */
                      FilterWheel, /* Parent class */
                      setPosition, /* C++ method name */
                      position     /* Argument(s) */
    );
  }

  void setFilterNames(const std::vector<std::string> &names) override {
    PYBIND11_OVERRIDE(void,           /* Return type */
                      FilterWheel,    /* Parent class */
                      setFilterNames, /* C++ method name */
                      names           /* Argument(s) */
    );
  }

  void setFilterOffsets(const std::vector<int> &offsets) override {
    PYBIND11_OVERRIDE(void,             /* Return type */
                      FilterWheel,      /* Parent class */
                      setFilterOffsets, /* C++ method name */
                      offsets           /* Argument(s) */
    );
  }

  void abort() override {
    PYBIND11_OVERRIDE(void,        /* Return type */
                      FilterWheel, /* Parent class */
                      abort        /* C++ method name */
                                   /* No arguments */
    );
  }

  bool isMovementComplete() const override {
    PYBIND11_OVERRIDE(bool,              /* Return type */
                      FilterWheel,       /* Parent class */
                      isMovementComplete /* C++ method name */
                                         /* No arguments */
    );
  }

  int getMaxFilterCount() const override {
    PYBIND11_OVERRIDE(int,              /* Return type */
                      FilterWheel,      /* Parent class */
                      getMaxFilterCount /* C++ method name */
                                        /* No arguments */
    );
  }

  void setFilterCount(int count) override {
    PYBIND11_OVERRIDE(void,           /* Return type */
                      FilterWheel,    /* Parent class */
                      setFilterCount, /* C++ method name */
                      count           /* Argument(s) */
    );
  }

  // Advanced overrides for internal methods (optional, ensure they are virtual
  // in base class)
  void simulateMovement(double elapsedSec) override {
    PYBIND11_OVERRIDE(void,             /* Return type */
                      FilterWheel,      /* Parent class */
                      simulateMovement, /* C++ method name */
                      elapsedSec        /* Argument(s) */
    );
  }

  void updatePosition() override {
    PYBIND11_OVERRIDE(void,          /* Return type */
                      FilterWheel,   /* Parent class */
                      updatePosition /* C++ method name */
                                     /* No arguments */
    );
  }
};

void register_filter_wheel_bindings(py::module &m) {
  // Register FilterWheel base class (make it non-inheritable directly if
  // desired) Note: We bind FilterWheel first, then PyFilterWheel which inherits
  // from it.
  py::class_<FilterWheel, DeviceBase, std::shared_ptr<FilterWheel>>
      filterWheelBinding(m,
                         "FilterWheelBase"); // Renamed to avoid conflict if
                                             // PyFilterWheel is the main export

  filterWheelBinding
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "QHY",
           py::arg("model") = "CFW3")
      // Bind public methods needed for direct use (if any)
      .def("set_position", &FilterWheel::setPosition, py::arg("position"))
      .def("set_filter_names", &FilterWheel::setFilterNames, py::arg("names"))
      .def("set_filter_offsets", &FilterWheel::setFilterOffsets,
           py::arg("offsets"))
      .def("abort", &FilterWheel::abort)
      .def("is_movement_complete", &FilterWheel::isMovementComplete)
      .def("get_max_filter_count", &FilterWheel::getMaxFilterCount)
      .def("set_filter_count", &FilterWheel::setFilterCount, py::arg("count"))
      // Use the now public methods
      .def_property_readonly("current_filter",
                             &FilterWheel::getCurrentFilterName)
      .def_property_readonly("current_offset",
                             &FilterWheel::getCurrentFilterOffset)
      // Bind other necessary public/protected methods from DeviceBase if needed
      .def("start", &FilterWheel::start)
      .def("stop", &FilterWheel::stop)
      .def("get_device_info", &FilterWheel::getDeviceInfo);

  // Register Python-inheritable class using the trampoline
  py::class_<PyFilterWheel, FilterWheel, std::shared_ptr<PyFilterWheel>>(
      m, "FilterWheel") // Use the desired Python name "FilterWheel" here
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "PythonFilterWheel",
           py::arg("model") = "v1.0");
  // No need to re-bind methods here, inheritance handles it.
  // The PYBIND11_OVERRIDE macros in PyFilterWheel enable Python overrides.
}
#include "device/filter_wheel.h"
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace astrocomm;

void register_filter_wheel_bindings(py::module &m) {
  py::class_<FilterWheel, DeviceBase, std::shared_ptr<FilterWheel>>(
      m, "FilterWheel")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "QHY",
           py::arg("model") = "CFW3")
      .def("set_position", &FilterWheel::setPosition, py::arg("position"))
      .def("set_filter_names", &FilterWheel::setFilterNames, py::arg("names"))
      .def("set_filter_offsets", &FilterWheel::setFilterOffsets,
           py::arg("offsets"))
      .def("abort", &FilterWheel::abort);

  // Python扩展类
  py::class_<PyFilterWheel, FilterWheel, std::shared_ptr<PyFilterWheel>>(
      m, "PyFilterWheel")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "PythonFilterWheel",
           py::arg("model") = "v1.0");
}

// 创建一个用于Python继承的派生类
class PyFilterWheel : public FilterWheel {
public:
  // 使用与基类相同的构造函数
  using FilterWheel::FilterWheel;

  // 为Python覆盖提供虚函数
  bool start() override {
    PYBIND11_OVERRIDE(bool,        // 返回类型
                      FilterWheel, // 父类
                      start,       // 调用的函数
                                   /* 参数列表为空 */
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,        // 返回类型
                      FilterWheel, // 父类
                      stop,        // 调用的函数
                                   /* 参数列表为空 */
    );
  }

  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,          // 返回类型
                      FilterWheel,   // 父类
                      getDeviceInfo, // 调用的函数
                                     /* 参数列表为空 */
    );
  }
};
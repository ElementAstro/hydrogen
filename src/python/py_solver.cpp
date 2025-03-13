#include "device/solver.h"
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace astrocomm;

void register_solver_bindings(py::module &m) {
  // SolverState 枚举
  py::enum_<SolverState>(m, "SolverState")
      .value("IDLE", SolverState::IDLE)
      .value("SOLVING", SolverState::SOLVING)
      .value("COMPLETE", SolverState::COMPLETE)
      .value("FAILED", SolverState::FAILED)
      .export_values();

  py::class_<Solver, DeviceBase, std::shared_ptr<Solver>>(m, "Solver")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "AstroCode",
           py::arg("model") = "AstroSolver")
      .def(
          "solve",
          [](Solver &self, py::array_t<uint8_t> imageData, int width,
             int height) {
            py::buffer_info buf = imageData.request();
            if (buf.ndim != 1 && buf.ndim != 2)
              throw std::runtime_error("Image data must be 1D or 2D array");

            std::vector<uint8_t> data;
            if (buf.ndim == 1) {
              data.assign(static_cast<uint8_t *>(buf.ptr),
                          static_cast<uint8_t *>(buf.ptr) + buf.size);
            } else {
              // 2D array to 1D vector
              uint8_t *ptr = static_cast<uint8_t *>(buf.ptr);
              for (py::ssize_t i = 0; i < buf.shape[0]; i++) {
                for (py::ssize_t j = 0; j < buf.shape[1]; j++) {
                  data.push_back(ptr[i * buf.shape[1] + j]);
                }
              }
            }

            self.solve(data, width, height);
          },
          py::arg("image_data"), py::arg("width"), py::arg("height"))
      .def("solve_from_file", &Solver::solveFromFile, py::arg("file_path"))
      .def("abort", &Solver::abort)
      .def("set_parameters", &Solver::setParameters, py::arg("params"))
      .def("set_solver_path", &Solver::setSolverPath, py::arg("path"))
      .def("set_solver_options", &Solver::setSolverOptions, py::arg("options"))
      .def("get_last_solution", &Solver::getLastSolution);

  // Python扩展类
  py::class_<PySolver, Solver, std::shared_ptr<PySolver>>(m, "PySolver")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "PythonSolver",
           py::arg("model") = "v1.0");
}

// 创建一个用于Python继承的派生类
class PySolver : public Solver {
public:
  // 使用与基类相同的构造函数
  using Solver::Solver;

  // 为Python覆盖提供虚函数
  bool start() override {
    PYBIND11_OVERRIDE(bool,   // 返回类型
                      Solver, // 父类
                      start,  // 调用的函数
                              /* 参数列表为空 */
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,   // 返回类型
                      Solver, // 父类
                      stop,   // 调用的函数
                              /* 参数列表为空 */
    );
  }

  json getDeviceInfo() const override {
    PYBIND11_OVERRIDE(json,          // 返回类型
                      Solver,        // 父类
                      getDeviceInfo, // 调用的函数
                                     /* 参数列表为空 */
    );
  }
};
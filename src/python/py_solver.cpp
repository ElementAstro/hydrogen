#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


#include "device/solver.h"

namespace py = pybind11;
using namespace hydrogen;

// Define the Python-inheritable class, so Python code can override virtual
// methods
class PySolver : public Solver {
public:
  // Use the same constructor as the base class
  using Solver::Solver;

  // Trampoline methods for Python to override virtual methods
  bool start() override {
    PYBIND11_OVERRIDE(bool,   // Return type
                      Solver, // Parent class
                      start   // Function name
    );
  }

  void stop() override {
    PYBIND11_OVERRIDE(void,   // Return type
                      Solver, // Parent class
                      stop    // Function name
    );
  }

  bool performSolve(const std::vector<uint8_t> &imageData, int width,
                    int height) override {
    PYBIND11_OVERRIDE(bool,                    // Return type
                      Solver,                  // Parent class
                      performSolve,            // Function name
                      imageData, width, height // Arguments
    );
  }

  json extractStars(const std::vector<uint8_t> &imageData, int width,
                    int height) override {
    PYBIND11_OVERRIDE(json,                    // Return type
                      Solver,                  // Parent class
                      extractStars,            // Function name
                      imageData, width, height // Arguments
    );
  }

  bool matchStarPattern(const json &stars) override {
    PYBIND11_OVERRIDE(bool,             // Return type
                      Solver,           // Parent class
                      matchStarPattern, // Function name
                      stars             // Arguments
    );
  }

  json calculateDistortion(const json &stars,
                           const json &matchedStars) override {
    PYBIND11_OVERRIDE(json,                // Return type
                      Solver,              // Parent class
                      calculateDistortion, // Function name
                      stars, matchedStars  // Arguments
    );
  }

  json generateSolution(bool success) override {
    PYBIND11_OVERRIDE(json,             // Return type
                      Solver,           // Parent class
                      generateSolution, // Function name
                      success           // Arguments
    );
  }

  // 添加公共方法来访问受保护的方�?
  bool py_perform_solve(const std::vector<uint8_t> &imageData, int width,
                        int height) {
    return performSolve(imageData, width, height);
  }

  json py_extract_stars(const std::vector<uint8_t> &imageData, int width,
                        int height) {
    return extractStars(imageData, width, height);
  }

  bool py_match_star_pattern(const json &stars) {
    return matchStarPattern(stars);
  }

  json py_calculate_distortion(const json &stars, const json &matchedStars) {
    return calculateDistortion(stars, matchedStars);
  }

  json py_generate_solution(bool success) { return generateSolution(success); }
};

void init_solver(py::module_ &m) {
  // SolverState enumeration
  py::enum_<SolverState>(m, "SolverState")
      .value("IDLE", SolverState::IDLE)
      .value("SOLVING", SolverState::SOLVING)
      .value("COMPLETE", SolverState::COMPLETE)
      .value("FAILED", SolverState::FAILED)
      .export_values();

  // SolverException class
  py::register_exception<SolverException>(m, "SolverException");

  // Solver base class
  py::class_<Solver, DeviceBase, PySolver, std::shared_ptr<Solver>>(m, "Solver")
      .def(py::init<const std::string &, const std::string &,
                    const std::string &>(),
           py::arg("device_id"), py::arg("manufacturer") = "AstroCode",
           py::arg("model") = "AstroSolver")
      .def("start", &Solver::start, "Start the solver device")
      .def("stop", &Solver::stop, "Stop the solver device")
      .def(
          "solve",
          [](Solver &self, py::array_t<uint8_t> imageData, int width,
             int height) {
            py::buffer_info buf = imageData.request();
            if (buf.ndim != 1 && buf.ndim != 2)
              throw SolverException("Image data must be 1D or 2D array");

            std::vector<uint8_t> data;
            if (buf.ndim == 1) {
              // Direct copy for 1D arrays
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
          py::arg("image_data"), py::arg("width"), py::arg("height"),
          "Solve an image from raw data")
      .def("solve_from_file", &Solver::solveFromFile, py::arg("file_path"),
           "Solve an image from a file")
      .def("abort", &Solver::abort, "Abort a running solve operation")
      .def("set_parameters", &Solver::setParameters, py::arg("params"),
           "Update solver parameters")
      .def("set_solver_path", &Solver::setSolverPath, py::arg("path"),
           "Set path to external solver executable")
      .def("set_solver_options", &Solver::setSolverOptions, py::arg("options"),
           "Set options for the external solver")
      .def("get_last_solution", &Solver::getLastSolution,
           "Get the last successful solution")
      .def("get_state", &Solver::getState, "Get current solver state")
      .def("get_progress", &Solver::getProgress,
           "Get current solving progress (0-100)")
      // 使用PySolver中新添加的公共方法进行绑�?
      .def("perform_solve", &PySolver::py_perform_solve, py::arg("image_data"),
           py::arg("width"), py::arg("height"),
           "Core implementation of plate solving algorithm")
      .def("extract_stars", &PySolver::py_extract_stars, py::arg("image_data"),
           py::arg("width"), py::arg("height"), "Extract stars from image data")
      .def("match_star_pattern", &PySolver::py_match_star_pattern,
           py::arg("stars"), "Match star pattern against catalog")
      .def("calculate_distortion", &PySolver::py_calculate_distortion,
           py::arg("stars"), py::arg("matched_stars"),
           "Calculate image distortion parameters")
      .def("generate_solution", &PySolver::py_generate_solution,
           py::arg("success"), "Generate solution data from solve results");

  // Add utility functions
  m.def("format_ra_to_hms", &formatRAToHMS,
        "Format RA coordinate to HH:MM:SS.SS string", py::arg("ra"));
  m.def("format_dec_to_dms", &formatDecToDMS,
        "Format DEC coordinate to DD:MM:SS.SS string", py::arg("dec"));
  m.def("base64_decode", &base64_decode, "Decode base64 string to binary data",
        py::arg("encoded_string"));
}
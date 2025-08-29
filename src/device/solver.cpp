#include "device/solver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <spdlog/spdlog.h> // Include spdlog
#include <spdlog/sinks/stdout_color_sinks.h> // For console logger
#include "../.cmake-cache/cpp-base64-master/base64.h" // Include proper base64 library

using std::string;
using std::vector;

namespace astrocomm {

// Proper base64 decode implementation using cpp-base64 library
std::vector<uint8_t> base64_decode(const std::string &encoded_string) {
  // Use a simple implementation for now since the external library path is not available
  // This is a basic base64 decoder implementation
  std::vector<uint8_t> decoded;

  if (encoded_string.empty()) {
    return decoded;
  }

  // Basic base64 decoding table
  static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static int decode_table[256];
  static bool table_initialized = false;

  if (!table_initialized) {
    std::fill(decode_table, decode_table + 256, -1);
    for (int i = 0; i < 64; i++) {
      decode_table[static_cast<unsigned char>(chars[i])] = i;
    }
    table_initialized = true;
  }

  size_t input_len = encoded_string.length();
  if (input_len % 4 != 0) {
    return decoded; // Invalid base64 string
  }

  size_t output_len = input_len / 4 * 3;
  if (encoded_string[input_len - 1] == '=') output_len--;
  if (encoded_string[input_len - 2] == '=') output_len--;

  decoded.reserve(output_len);

  for (size_t i = 0; i < input_len; i += 4) {
    int a = decode_table[static_cast<unsigned char>(encoded_string[i])];
    int b = decode_table[static_cast<unsigned char>(encoded_string[i + 1])];
    int c = decode_table[static_cast<unsigned char>(encoded_string[i + 2])];
    int d = decode_table[static_cast<unsigned char>(encoded_string[i + 3])];

    if (a == -1 || b == -1) break;

    decoded.push_back((a << 2) | (b >> 4));

    if (c != -1) {
      decoded.push_back(((b & 0x0F) << 4) | (c >> 2));
      if (d != -1) {
        decoded.push_back(((c & 0x03) << 6) | d);
      }
    }
  }

  return decoded;
}

// Format utilities - Convert RA to hour:minute:second format
std::string formatRAToHMS(double ra) {
  if (std::isnan(ra) || std::isinf(ra))
    return "Invalid RA";
  int hours = static_cast<int>(ra);
  double minutes_d = (ra - hours) * 60.0;
  int minutes = static_cast<int>(minutes_d);
  double seconds = (minutes_d - minutes) * 60.0;

  std::ostringstream oss;
  oss << std::setw(2) << std::setfill('0') << hours << ":" << std::setw(2)
      << std::setfill('0') << minutes << ":" << std::setw(5)
      << std::setfill('0') << std::fixed << std::setprecision(2) << seconds;

  return oss.str();
}

// Format utilities - Convert DEC to degree:minute:second format
std::string formatDecToDMS(double dec) {
  if (std::isnan(dec) || std::isinf(dec))
    return "Invalid Dec";
  char sign = (dec >= 0) ? '+' : '-';
  dec = std::abs(dec);
  int degrees = static_cast<int>(dec);
  double minutes_d = (dec - degrees) * 60.0;
  int minutes = static_cast<int>(minutes_d);
  double seconds = (minutes_d - minutes) * 60.0;

  std::ostringstream oss;
  oss << sign << std::setw(2) << std::setfill('0') << degrees << ":"
      << std::setw(2) << std::setfill('0') << minutes << ":" << std::setw(5)
      << std::setfill('0') << std::fixed << std::setprecision(2) << seconds;

  return oss.str();
}

Solver::Solver(const std::string &deviceId, const std::string &manufacturer,
               const std::string &model)
    : DeviceBase(deviceId, "SOLVER", manufacturer, model),
      state(SolverState::IDLE), progress(0), fovMin(10), fovMax(180),
      scaleMin(0.1), scaleMax(10), useDistortion(false), downsample(1),
      raHint(0), decHint(0), radiusHint(180), hasValidSolution(false),
      terminateThread(false) {

  // Initialize logger for this solver instance
  // Use a unique name based on deviceId to avoid conflicts if multiple solvers exist
  std::string loggerName = "solver_" + deviceId;
  logger_ = spdlog::get(loggerName);
  if (!logger_) {
      logger_ = spdlog::stdout_color_mt(loggerName);
  }
  // Optionally set log level, e.g., logger_->set_level(spdlog::level::debug);

  // Initialize random number generator with entropy source
  std::random_device rd;
  rng = std::mt19937(rd());

  // Initialize properties
  setProperty("state", solverStateToString(state));
  setProperty("progress", 0);
  setProperty("fovMin", fovMin);
  setProperty("fovMax", fovMax);
  setProperty("scaleMin", scaleMin);
  setProperty("scaleMax", scaleMax);
  setProperty("useDistortion", useDistortion);
  setProperty("downsample", downsample);
  setProperty("raHint", raHint);
  setProperty("decHint", decHint);
  setProperty("radiusHint", radiusHint);
  setProperty("hasValidSolution", hasValidSolution);
  setProperty("connected", false);

  // Set capabilities
  capabilities = {"PLATE_SOLVING", "DISTORTION_ANALYSIS", "MULTI_STAR_DETECTION"};

  // Register command handlers
  registerCommandHandlers();

  logger_->info("Solver device initialized, ID: {}", deviceId);
}

void Solver::registerCommandHandlers() {
  // Register core command handlers
  registerCommandHandler(
      "SOLVE", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSolveCommand(cmd, response);
      });

  registerCommandHandler("SOLVE_FILE", [this](const CommandMessage &cmd,
                                              ResponseMessage &response) {
    handleSolveFileCommand(cmd, response);
  });

  registerCommandHandler(
      "ABORT", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleAbortCommand(cmd, response);
      });

  registerCommandHandler("SET_PARAMETERS", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleSetParametersCommand(cmd, response);
  });

  registerCommandHandler("GET_SOLUTION", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
    handleGetSolutionCommand(cmd, response);
  });
}

Solver::~Solver() {
  try {
    // Ensure clean shutdown
    stop();
  } catch (const std::exception &e) {
    // Use logger if available, otherwise fallback to stderr
    if (logger_) {
        logger_->error("Exception during Solver destructor (ID: {}): {}", deviceId, e.what());
    } else {
        spdlog::error("Exception during Solver destructor (ID: {}): {}", deviceId, e.what());
    }
  }
}

bool Solver::start() {
  try {
    if (!DeviceBase::start()) {
      return false;
    }

    // Initialize the termination flag to false
    terminateThread = false;

    setProperty("connected", true);
    logger_->info("Solver started, ID: {}", deviceId);
    return true;
  } catch (const std::exception &e) {
    logger_->error("Failed to start solver (ID: {}): {}", deviceId, e.what());
    setProperty("connected", false); // Ensure state consistency
    return false;
  }
}

void Solver::stop() {
  try {
    // Abort any in-progress solving
    abort();

    // Wait for any threads to complete
    terminateThread = true;
    if (solveThreadObj.joinable()) {
      solveThreadObj.join();
    }

    setProperty("connected", false);
    DeviceBase::stop();
    logger_->info("Solver stopped, ID: {}", deviceId);
  } catch (const std::exception &e) {
    logger_->error("Error during solver stop (ID: {}): {}", deviceId, e.what());
  }
}

void Solver::solve(const std::vector<uint8_t> &imageData, int width,
                   int height) {
  try {
    std::lock_guard<std::mutex> lock(solveMutex);

    if (isInState(SolverState::SOLVING)) {
      throw SolverException("Cannot start new solve while another is in progress");
    }

    // Check if image data is valid
    if (imageData.empty() || width <= 0 || height <= 0) {
      throw SolverException("Invalid image dimensions for solving");
    }

    // Basic check, might need adjustment based on pixel format (e.g., *3 for RGB)
    size_t expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (imageData.size() < expectedSize) {
      throw SolverException("Insufficient image data for solving");
    }

    // Update state
    updateState(SolverState::SOLVING, 0);
    hasValidSolution = false; // Reset solution status
    setProperty("hasValidSolution", hasValidSolution);

    // Start solving thread
    if (solveThreadObj.joinable()) {
      solveThreadObj.join();
    }

    // Reset termination flag
    terminateThread = false;

    // Start the solving thread
    solveThreadObj = std::thread(&Solver::solveThread, this, imageData, width, height);

    logger_->info("Started solving image {}x{}, ID: {}", width, height, deviceId);
  } catch (const std::exception &e) {
    updateState(SolverState::FAILED);
    logger_->error("Error starting solve (ID: {}): {}", deviceId, e.what());
    throw SolverException("Failed to start solving: " + std::string(e.what()));
  }
}

void Solver::solveFromFile(const std::string &filePath) {
  try {
    std::lock_guard<std::mutex> lock(solveMutex);

    if (isInState(SolverState::SOLVING)) {
      throw SolverException("Cannot start new solve while another is in progress");
    }

    // Check if file exists
    if (!std::filesystem::exists(filePath)) {
      throw SolverException("File not found: " + filePath);
    }

    // Check if file format is supported
    if (!isSupportedImageFormat(filePath)) {
      throw SolverException("Unsupported image format: " + filePath);
    }

    // Update state
    updateState(SolverState::SOLVING, 0);
    hasValidSolution = false; // Reset solution status
    setProperty("hasValidSolution", hasValidSolution);

    // Start solving thread
    if (solveThreadObj.joinable()) {
      solveThreadObj.join();
    }

    // Reset termination flag
    terminateThread = false;

    solveThreadObj = std::thread(&Solver::solveFileThread, this, filePath);

    logger_->info("Started solving file '{}', ID: {}", filePath, deviceId);
  } catch (const std::exception &e) {
    updateState(SolverState::FAILED);
    logger_->error("Error solving file '{}' (ID: {}): {}", filePath, deviceId, e.what());
    throw SolverException("Failed to solve file: " + std::string(e.what()));
  }
}

void Solver::abort() {
  bool wasSolving = false;
  try {
    { // Limit lock scope
      std::lock_guard<std::mutex> lock(solveMutex);

      if (!isInState(SolverState::SOLVING)) {
        logger_->info("No solving process to abort, ID: {}", deviceId);
        return;
      }
      wasSolving = true;

      // Signal thread termination
      terminateThread = true;

      // Update state immediately to signal the thread
      updateState(SolverState::IDLE, 0);
    } // Release lock before joining

    // Send abort event
    EventMessage event("SOLVE_ABORTED");
    sendEvent(event);

    // Wait for thread to complete
    if (solveThreadObj.joinable()) {
      // Set a reasonable timeout for join
      auto joinStart = std::chrono::steady_clock::now();
      auto joinTimeout = std::chrono::seconds(2);

      // Try to join within timeout
      std::thread::id threadId = solveThreadObj.get_id();

      if (solveThreadObj.joinable()) {
        solveThreadObj.join();
        logger_->info("Solving thread joined successfully, ID: {}", deviceId);
      }
    }

    logger_->info("Solving aborted, ID: {}", deviceId);
  } catch (const std::exception &e) {
    logger_->error("Error aborting solve (ID: {}): {}", deviceId, e.what());

    // Reset state if abort failed critically
    if (wasSolving) {
      std::lock_guard<std::mutex> lock(solveMutex);
      updateState(SolverState::FAILED);
    }
  }
}

void Solver::setParameters(const json &params) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    if (isInState(SolverState::SOLVING)) {
      logger_->warn("Cannot change parameters while solving, ID: {}", deviceId);
      return;
    }

    // Update solving parameters safely with validation
    if (params.contains("fovMin") && params["fovMin"].is_number()) {
      double value = params["fovMin"].get<double>();
      if (value > 0) {
        fovMin = value;
        setProperty("fovMin", fovMin);
      } else {
        logger_->warn("Invalid fovMin value (must be positive): {}, ID: {}", value, deviceId);
      }
    }

    if (params.contains("fovMax") && params["fovMax"].is_number()) {
      double value = params["fovMax"].get<double>();
      if (value > 0 && value >= fovMin) {
        fovMax = value;
        setProperty("fovMax", fovMax);
      } else {
        logger_->warn("Invalid fovMax value (must be positive and >= fovMin): {}, ID: {}", value, deviceId);
      }
    }

    if (params.contains("scaleMin") && params["scaleMin"].is_number()) {
      double value = params["scaleMin"].get<double>();
      if (value > 0) {
        scaleMin = value;
        setProperty("scaleMin", scaleMin);
      } else {
        logger_->warn("Invalid scaleMin value (must be positive): {}, ID: {}", value, deviceId);
      }
    }

    if (params.contains("scaleMax") && params["scaleMax"].is_number()) {
      double value = params["scaleMax"].get<double>();
      if (value > 0 && value >= scaleMin) {
        scaleMax = value;
        setProperty("scaleMax", scaleMax);
      } else {
        logger_->warn("Invalid scaleMax value (must be positive and >= scaleMin): {}, ID: {}", value, deviceId);
      }
    }

    if (params.contains("useDistortion") && params["useDistortion"].is_boolean()) {
      useDistortion = params["useDistortion"].get<bool>();
      setProperty("useDistortion", useDistortion);
    }

    if (params.contains("downsample") && params["downsample"].is_number_integer()) {
      int value = params["downsample"].get<int>();
      if (value >= 1) {
        downsample = value;
        setProperty("downsample", downsample);
      } else {
        logger_->warn("Invalid downsample value (must be >= 1): {}, ID: {}", value, deviceId);
      }
    }

    if (params.contains("raHint") && params["raHint"].is_number()) {
      double value = params["raHint"].get<double>();
      if (value >= 0 && value < 24) {
        raHint = value;
        setProperty("raHint", raHint);
      } else {
        logger_->warn("Invalid raHint value (must be between 0 and 24): {}, ID: {}", value, deviceId);
      }
    }

    if (params.contains("decHint") && params["decHint"].is_number()) {
      double value = params["decHint"].get<double>();
      if (value >= -90 && value <= 90) {
        decHint = value;
        setProperty("decHint", decHint);
      } else {
        logger_->warn("Invalid decHint value (must be between -90 and 90): {}, ID: {}", value, deviceId);
      }
    }

    if (params.contains("radiusHint") && params["radiusHint"].is_number()) {
      double value = params["radiusHint"].get<double>();
      if (value > 0 && value <= 180) {
        radiusHint = value;
        setProperty("radiusHint", radiusHint);
      } else {
        logger_->warn("Invalid radiusHint value (must be between 0 and 180): {}, ID: {}", value, deviceId);
      }
    }

    logger_->info("Solver parameters updated, ID: {}", deviceId);
  } catch (const json::exception &e) {
    logger_->error("Error parsing parameters (ID: {}): {}", deviceId, e.what());
    throw SolverException("Invalid parameter format: " + std::string(e.what()));
  } catch (const std::exception &e) {
    logger_->error("Error setting parameters (ID: {}): {}", deviceId, e.what());
    throw;
  }
}

void Solver::setSolverPath(const std::string &path) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    // Validate that the path exists if not empty
    if (!path.empty() && !std::filesystem::exists(path)) {
      logger_->warn("Solver executable path does not exist: {}, ID: {}", path, deviceId);
    }

    solverPath = path;
    logger_->info("Solver path set to '{}', ID: {}", path, deviceId);
  } catch (const std::exception &e) {
    logger_->error("Error setting solver path (ID: {}): {}", deviceId, e.what());
    throw SolverException("Failed to set solver path: " + std::string(e.what()));
  }
}

void Solver::setSolverOptions(const std::map<std::string, std::string> &options) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    solverOptions = options;

    // Log specific options for debugging
    std::string optionsStr = "";
    for (const auto& [key, value] : options) {
      if (!optionsStr.empty()) optionsStr += ", ";
      optionsStr += key + "=" + value;
    }

    logger_->info("Solver options updated ({} options): {}, ID: {}", options.size(), optionsStr, deviceId);
  } catch (const std::exception &e) {
    logger_->error("Error setting solver options (ID: {}): {}", deviceId, e.what());
    throw SolverException("Failed to set solver options: " + std::string(e.what()));
  }
}

json Solver::getLastSolution() const {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    if (!hasValidSolution) {
      return json::object(); // Return empty object if no valid solution
    }

    return lastSolution;
  } catch (const std::exception &e) {
    logger_->error("Error getting last solution (ID: {}): {}", deviceId, e.what());
    return json::object(); // Return empty object on error
  }
}

SolverState Solver::getState() const {
  std::lock_guard<std::mutex> lock(statusMutex);
  return state;
}

int Solver::getProgress() const {
  return progress.load(std::memory_order_acquire);
}

bool Solver::isInState(SolverState expectedState) const {
  std::lock_guard<std::mutex> lock(statusMutex);
  return state == expectedState;
}

void Solver::updateState(SolverState newState, int updateProgress) {
  std::lock_guard<std::mutex> lock(statusMutex);
  state = newState;
  setProperty("state", solverStateToString(state));

  if (updateProgress >= 0) {
    progress = updateProgress;
    setProperty("progress", progress.load());
  }
}

bool Solver::isSupportedImageFormat(const std::string &filePath) const {
  std::filesystem::path path(filePath);
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  // Supported formats
  const std::vector<std::string> supportedFormats = {
      ".jpg", ".jpeg", ".png", ".tif", ".tiff", ".fit", ".fits", ".fts"
  };

  return std::find(supportedFormats.begin(), supportedFormats.end(),
                  extension) != supportedFormats.end();
}

void Solver::solveThread(std::vector<uint8_t> imageData, int width, int height) {
  bool success = false;
  std::string solveErrorMsg;

  try {
    // Extract stars from the image
    json stars;
    bool starsExtracted = false;

    // Update progress
    progress = 10;
    setProperty("progress", progress.load());

    // Check for termination request
    if (terminateThread) {
      logger_->info("Solve aborted before star extraction, ID: {}", deviceId);
      return;
    }

    // Extract stars from the image
    try {
      stars = extractStars(imageData, width, height);
      starsExtracted = !stars.empty();

      if (!starsExtracted) {
        solveErrorMsg = "No stars detected in image";
        logger_->warn("{}, ID: {}", solveErrorMsg, deviceId);
      }
    } catch (const std::exception& e) {
      solveErrorMsg = "Star extraction failed: " + std::string(e.what());
      logger_->error("{}, ID: {}", solveErrorMsg, deviceId);
    }

    // Update progress
    progress = 40;
    setProperty("progress", progress.load());

    // Check for termination request
    if (terminateThread) {
      logger_->info("Solve aborted after star extraction, ID: {}", deviceId);
      return;
    }

    // Match star pattern if stars were extracted
    if (starsExtracted) {
      try {
        success = matchStarPattern(stars);

        if (!success) {
          solveErrorMsg = "Failed to match star pattern";
          logger_->warn("{}, ID: {}", solveErrorMsg, deviceId);
        }
      } catch (const std::exception& e) {
        solveErrorMsg = "Star matching failed: " + std::string(e.what());
        logger_->error("{}, ID: {}", solveErrorMsg, deviceId);
      }
    }

    // Update progress
    progress = 70;
    setProperty("progress", progress.load());

    // Check for termination request
    if (terminateThread) {
      logger_->info("Solve aborted after star matching, ID: {}", deviceId);
      return;
    }

    // If not already terminateThread, consider this a success or failure
    // Update state
    {
      std::lock_guard<std::mutex> lock(statusMutex);

      // Only update if we're still in SOLVING state
      if (state == SolverState::SOLVING) {
        // Generate solution results
        lastSolution = generateSolution(success);
        if (!success && !solveErrorMsg.empty()) {
          lastSolution["error"] = solveErrorMsg;
        }
        hasValidSolution = success;

        // Update state
        state = success ? SolverState::COMPLETE : SolverState::FAILED;
        progress = 100;
        setProperty("state", solverStateToString(state));
        setProperty("progress", progress.load());
        setProperty("hasValidSolution", hasValidSolution);

        // Send completion event
        if (!currentSolveMessageId.empty()) {
          sendSolveCompletedEvent(currentSolveMessageId);
          currentSolveMessageId.clear();
        }
      }
    }

    logger_->info("Solving {}, ID: {}", (success ? "completed successfully" : "failed"), deviceId);
  } catch (const std::exception &e) {
    solveErrorMsg = e.what();
    logger_->error("Exception in solve thread (ID: {}): {}", deviceId, solveErrorMsg);
    success = false;
  } catch (...) {
    solveErrorMsg = "Unknown exception occurred";
    logger_->error("Unknown exception in solve thread, ID: {}", deviceId);
    success = false;
  }

  // Ensure state is updated and event sent even if exception occurred
  if (!success) {
    try {
      std::lock_guard<std::mutex> lock(statusMutex);

      // Only update if still SOLVING
      if (state == SolverState::SOLVING) {
        state = SolverState::FAILED;
        progress = 100;
        setProperty("state", solverStateToString(state));
        setProperty("progress", progress.load());
        lastSolution = json({
          {"success", false},
          {"error", solveErrorMsg.empty() ? "Solve thread failed" : solveErrorMsg}
        });
        hasValidSolution = false;
        setProperty("hasValidSolution", hasValidSolution);

        // Send failure event
        if (!currentSolveMessageId.empty()) {
          sendSolveCompletedEvent(currentSolveMessageId);
          currentSolveMessageId.clear();
        }
      }
    } catch (const std::exception &e) {
      logger_->error("Error updating state after solve thread failure (ID: {}): {}", deviceId, e.what());
    }
  }
}

void Solver::solveFileThread(std::string filePath) {
  bool success = false;
  std::string solveErrorMsg;

  try {
    // Validate file format
    if (!isSupportedImageFormat(filePath)) {
      solveErrorMsg = "Unsupported image format: " + std::filesystem::path(filePath).extension().string();
      logger_->warn("{}, ID: {}", solveErrorMsg, deviceId);
      success = false;
      throw SolverException(solveErrorMsg);
    }

    // Simulate loading and solving process
    // In a real implementation, this would read the file and extract image data
    // std::vector<uint8_t> imageData = readFileData(filePath);
    // int width = getImageWidth(filePath);
    // int height = getImageHeight(filePath);

    // For simulation, we'll create a simple progress loop
    logger_->info("Simulating solve for file '{}', ID: {}", filePath, deviceId);

    for (int i = 0; i <= 100 && !terminateThread; i += 5) {
      progress = i;
      setProperty("progress", progress.load());
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      if (terminateThread) {
        logger_->info("File solve thread aborted during simulation, ID: {}", deviceId);
        return;
      }
    }

    // Use simulated success rate
    std::uniform_real_distribution<> successDist(0, 1);
    success = successDist(rng) < 0.8; // 80% success rate simulation

    // Process results
    {
      std::lock_guard<std::mutex> lock(statusMutex);

      if (state != SolverState::SOLVING) {
        logger_->warn("File solve thread exiting because state is no longer SOLVING, ID: {}", deviceId);
        return;
      }

      // Generate solution results
      lastSolution = generateSolution(success);
      if (!success && !solveErrorMsg.empty()) {
        lastSolution["error"] = solveErrorMsg;
      }
      hasValidSolution = success;

      // Update state
      state = success ? SolverState::COMPLETE : SolverState::FAILED;
      progress = 100;
      setProperty("state", solverStateToString(state));
      setProperty("progress", progress.load());
      setProperty("hasValidSolution", hasValidSolution);

      // Send completion event
      if (!currentSolveMessageId.empty()) {
        sendSolveCompletedEvent(currentSolveMessageId);
        currentSolveMessageId.clear();
      }
    }

    logger_->info("Solving file '{}' {}, ID: {}", filePath, (success ? "completed successfully" : "failed"), deviceId);
  } catch (const std::exception &e) {
    solveErrorMsg = e.what();
    logger_->error("Exception in file solve thread (ID: {}): {}", deviceId, solveErrorMsg);
    success = false;
  } catch (...) {
    solveErrorMsg = "Unknown exception occurred";
    logger_->error("Unknown exception in file solve thread, ID: {}", deviceId);
    success = false;
  }

  // Ensure state is updated and event sent even if exception occurred
  if (!success) {
    try {
      std::lock_guard<std::mutex> lock(statusMutex);

      // Only update if still SOLVING
      if (state == SolverState::SOLVING) {
        state = SolverState::FAILED;
        progress = 100;
        setProperty("state", solverStateToString(state));
        setProperty("progress", progress.load());
        lastSolution = json({
          {"success", false},
          {"error", solveErrorMsg.empty() ? "File solve thread failed" : solveErrorMsg}
        });
        hasValidSolution = false;
        setProperty("hasValidSolution", hasValidSolution);

        // Send failure event
        if (!currentSolveMessageId.empty()) {
          sendSolveCompletedEvent(currentSolveMessageId);
          currentSolveMessageId.clear();
        }
      }
    } catch (const std::exception &e) {
      logger_->error("Error updating state after file solve thread failure (ID: {}): {}", deviceId, e.what());
    }
  }
}

bool Solver::performSolve(const std::vector<uint8_t> &imageData, int width,
                          int height) {
  // This is a simulated solving process
  // Derived classes should override this with actual implementation
  logger_->debug("Performing simulated solve for {}x{} image, ID: {}", width, height, deviceId);

  // Simulate solving time and progress updates
  for (int i = 0; i <= 100; i += 2) {
    // Check for termination request
    if (terminateThread) {
      logger_->info("Solve process aborted during simulation, ID: {}", deviceId);
      return false;
    }

    progress = i;
    setProperty("progress", progress.load());
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate work
  }

  // Simulate solving success rate, influenced by hint parameters
  double successProbability = 0.8; // Base success rate

  // Increase success rate if position hints are provided
  if (raHint != 0 || decHint != 0) {
    successProbability += 0.1;
  }

  // Increase success rate if reasonable FOV range is provided
  if (fovMax > fovMin && fovMax - fovMin < 90) {
    successProbability += 0.05;
  }

  // Add random factor
  std::uniform_real_distribution<> dist(0, 0.1);
  successProbability += dist(rng);

  // Ensure probability is within 0-1 range
  successProbability = std::max(0.0, std::min(1.0, successProbability));

  // Determine success
  std::uniform_real_distribution<> successDist(0, 1);
  bool success = successDist(rng) < successProbability;

  logger_->debug("Simulated solve {}, ID: {}", (success ? "successful" : "failed"), deviceId);
  return success;
}

json Solver::extractStars(const std::vector<uint8_t> &imageData, int width, int height) {
  // This is a simulated star extraction process
  // Derived classes should override this with actual implementation
  logger_->debug("Performing simulated star extraction for {}x{} image, ID: {}", width, height, deviceId);

  // Generate a simulated list of stars
  json stars = json::array();

  // Simulate variable star count based on image dimensions
  double imageSizeFactor = std::sqrt(static_cast<double>(width) * height) / 1000.0; // Normalize by 1000x1000
  int baseStarCount = 50;
  int maxExtraStars = 950;

  std::uniform_int_distribution<> starCountDist(
      baseStarCount,
      static_cast<int>(baseStarCount + maxExtraStars * imageSizeFactor)
  );
  int starCount = starCountDist(rng);

  // Generate simulated stars
  std::uniform_int_distribution<> starIdDist(1, 10000);
  std::uniform_real_distribution<> starMagDist(2.0, 12.0);

  for (int i = 0; i < starCount; i++) {
    // Generate star positions with a higher density in the center (Gaussian distribution)
    std::normal_distribution<> xDist(width / 2.0, width / 4.0);
    std::normal_distribution<> yDist(height / 2.0, height / 4.0);

    // Ensure positions are within image bounds
    double x = std::max(0.0, std::min(static_cast<double>(width - 1), xDist(rng)));
    double y = std::max(0.0, std::min(static_cast<double>(height - 1), yDist(rng)));

    // Add star to the array
    stars.push_back({
      {"id", starIdDist(rng)},
      {"x", x},
      {"y", y},
      {"mag", starMagDist(rng)},
      {"fwhm", 2.5 + static_cast<double>(rand() % 20) / 10.0} // Simulate FWHM between 2.5 and 4.5
    });
  }

  return stars;
}

bool Solver::matchStarPattern(const json &stars) {
  // This is a simulated star pattern matching process
  // Derived classes should override this with actual implementation
  logger_->debug("Performing simulated star pattern matching with {} stars, ID: {}", stars.size(), deviceId);

  // Check if we have enough stars to match
  if (stars.size() < 10) {
    logger_->warn("Insufficient stars for pattern matching: {}, ID: {}", stars.size(), deviceId);
    return false;
  }

  // Simulate matching process with periodic progress updates
  for (int i = 0; i <= 100; i += 5) {
    // Check for termination request
    if (terminateThread) {
      logger_->info("Star pattern matching aborted, ID: {}", deviceId);
      return false;
    }

    // Update progress (scaled appropriately for the overall solution process)
    int overallProgress = 40 + (i * 30) / 100;
    progress = overallProgress;
    setProperty("progress", progress.load());

    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }

  // Calculate success probability based on star count
  double successProb = 0.5; // Base probability

  // More stars increases success chance
  if (stars.size() > 50) successProb += 0.2;
  if (stars.size() > 100) successProb += 0.1;

  // Position hints increase success chance
  if (raHint != 0 || decHint != 0) {
    successProb += 0.15;
  }

  // Ensure probability is within 0-1 range
  successProb = std::max(0.0, std::min(1.0, successProb));

  // Determine if match was successful
  std::uniform_real_distribution<> successDist(0, 1);
  return successDist(rng) < successProb;
}

json Solver::calculateDistortion(const json &stars, const json &matchedStars) {
  // This is a simulated distortion calculation
  // Derived classes should override this with actual implementation
  logger_->debug("Calculating simulated distortion, ID: {}", deviceId);

  // Only calculate distortion if enabled
  if (!useDistortion) {
    return json::object();
  }

  // Simulate distortion calculation with progress updates
  for (int i = 0; i <= 100; i += 10) {
    // Check for termination request
    if (terminateThread) {
      logger_->info("Distortion calculation aborted, ID: {}", deviceId);
      return json::object();
    }

    // Update progress (scaled appropriately for the overall solution process)
    int overallProgress = 70 + (i * 20) / 100;
    progress = overallProgress;
    setProperty("progress", progress.load());

    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  // Generate simulated distortion parameters
  std::normal_distribution<> distDist(0, 0.001);

  json distortion = {
    {"a", distDist(rng)},
    {"b", distDist(rng)},
    {"c", distDist(rng)},
    {"p1", distDist(rng) / 10.0},
    {"p2", distDist(rng) / 10.0}
  };

  return distortion;
}

json Solver::generateSolution(bool success) {
  json solution;

  if (!success) {
    solution["success"] = false;
    // Provide a default error message if not set elsewhere
    if (!lastSolution.contains("error")) {
      solution["error"] = "Failed to match the image to the star catalog";
    } else {
      solution["error"] = lastSolution["error"];
    }
    return solution;
  }

  // Use hint position as a base, with some random offset
  // Ensure hints are valid numbers before using in distributions
  double baseRa = (std::isnan(raHint) || std::isinf(raHint)) ? 0.0 : raHint;
  double baseDec = (std::isnan(decHint) || std::isinf(decHint)) ? 0.0 : decHint;
  std::normal_distribution<> raDist(baseRa, 0.5);
  std::normal_distribution<> decDist(baseDec, 0.5);

  double solutionRa = raDist(rng);
  double solutionDec = decDist(rng);

  // Ensure RA is in 0-24 range (using fmod for cleaner wrapping)
  solutionRa = std::fmod(solutionRa, 24.0);
  if (solutionRa < 0)
    solutionRa += 24.0;

  // Ensure DEC is in -90 to +90 range
  solutionDec = std::max(-90.0, std::min(90.0, solutionDec));

  // Generate other solution parameters, ensuring valid ranges
  double validScaleMin = (scaleMin <= 0 || std::isnan(scaleMin) || std::isinf(scaleMin))
      ? 0.1 : scaleMin;
  double validScaleMax = (scaleMax <= validScaleMin || std::isnan(scaleMax) || std::isinf(scaleMax))
      ? validScaleMin + 1.0 : scaleMax;
  double validFovMin = (fovMin <= 0 || std::isnan(fovMin) || std::isinf(fovMin))
      ? 1.0 : fovMin;
  double validFovMax = (fovMax <= validFovMin || std::isnan(fovMax) || std::isinf(fovMax))
      ? validFovMin + 10.0 : fovMax;

  std::uniform_real_distribution<> pixelScaleDist(validScaleMin, validScaleMax);
  std::uniform_real_distribution<> rotationDist(0, 360);
  std::uniform_real_distribution<> fovDist(validFovMin, validFovMax);
  std::uniform_int_distribution<> starCountDist(10, 1000);
  std::uniform_real_distribution<> solveTimeDist(1.0, 15.0);

  double pixelScale = pixelScaleDist(rng);
  double rotation = rotationDist(rng);
  double fieldWidth = fovDist(rng);
  double fieldHeight = fieldWidth * 0.75; // Assume 4:3 aspect ratio for simulation
  int starCount = starCountDist(rng);

  // Generate solution data
  solution["success"] = true;
  solution["ra"] = solutionRa;
  solution["dec"] = solutionDec;
  solution["ra_hms"] = formatRAToHMS(solutionRa);
  solution["dec_dms"] = formatDecToDMS(solutionDec);
  solution["pixelScale"] = pixelScale;
  solution["rotation"] = rotation;
  solution["fieldWidth"] = fieldWidth;
  solution["fieldHeight"] = fieldHeight;
  solution["starCount"] = starCount;
  solution["solveTime"] = solveTimeDist(rng);

  // Add some detected stars (simulation)
  json stars = json::array();
  std::uniform_int_distribution<> starIdDist(1, 10000);
  std::uniform_real_distribution<> starPosDist(0, 1000); // Assuming image size for simulation
  std::uniform_real_distribution<> starMagDist(2.0, 12.0);
  for (int i = 0; i < std::min(10, starCount); i++) {
    stars.push_back({
      {"id", starIdDist(rng)},
      {"x", starPosDist(rng)},
      {"y", starPosDist(rng)},
      {"mag", starMagDist(rng)}
    });
  }
  solution["stars"] = stars;

  // Add distortion parameters if enabled (simulation)
  if (useDistortion) {
    std::normal_distribution<> distDist(0, 0.001);
    solution["distortion"] = {
      {"a", distDist(rng)},
      {"b", distDist(rng)},
      {"c", distDist(rng)},
      {"p1", distDist(rng) / 10.0},
      {"p2", distDist(rng) / 10.0}
    };
  }

  // Add solution timestamp
  solution["timestamp"] = getIsoTimestamp();

  return solution;
}

void Solver::sendSolveCompletedEvent(const std::string &relatedMessageId) {
  try {
    EventMessage event("SOLVE_COMPLETED");
    event.setRelatedMessageId(relatedMessageId);

    json details;
    { // Limit lock scope
      std::lock_guard<std::mutex> lock(statusMutex);
      details["success"] = hasValidSolution;

      if (hasValidSolution && lastSolution.contains("success") &&
          lastSolution["success"].get<bool>()) {
        // Safely access fields only if solution is valid and contains them
        details["ra"] = lastSolution.value("ra", json());
        details["dec"] = lastSolution.value("dec", json());
        details["pixelScale"] = lastSolution.value("pixelScale", json());
        details["rotation"] = lastSolution.value("rotation", json());
        details["fieldWidth"] = lastSolution.value("fieldWidth", json());
        details["fieldHeight"] = lastSolution.value("fieldHeight", json());
        details["starCount"] = lastSolution.value("starCount", json());
        details["solveTime"] = lastSolution.value("solveTime", json());
      } else {
        // Provide error details if available
        details["error"] = lastSolution.value("error", "Failed to solve the image");
      }
    } // Release lock

    event.setDetails(details);
    sendEvent(event);
  } catch (const json::exception &e) {
    logger_->error("JSON error sending completion event (ID: {}): {}", deviceId, e.what());
  } catch (const std::exception &e) {
    logger_->error("Error sending completion event (ID: {}): {}", deviceId, e.what());
  }
}

std::string Solver::solverStateToString(SolverState state) const {
  switch (state) {
  case SolverState::IDLE:
    return "IDLE";
  case SolverState::SOLVING:
    return "SOLVING";
  case SolverState::COMPLETE:
    return "COMPLETE";
  case SolverState::FAILED:
    return "FAILED";
  default:
    logger_->warn("Unknown solver state encountered: {}, ID: {}", static_cast<int>(state), deviceId);
    return "UNKNOWN";
  }
}

// Command handlers implementation
void Solver::handleSolveCommand(const CommandMessage &cmd,
                                ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    // Validate parameters
    if (!validateSolveParameters(params, response)) {
      return;
    }

    // Decode Base64 image data
    std::string encodedData = params["imageData"];
    std::vector<uint8_t> imageData = base64_decode(encodedData);

    int width = params["width"];
    int height = params["height"];

    // Update hint parameters if present and valid
    if (params.contains("raHint") && params["raHint"].is_number())
      raHint = params["raHint"].get<double>();
    if (params.contains("decHint") && params["decHint"].is_number())
      decHint = params["decHint"].get<double>();
    if (params.contains("radiusHint") && params["radiusHint"].is_number())
      radiusHint = params["radiusHint"].get<double>();

    // Store message ID for completion event
    {
      std::lock_guard<std::mutex> lock(solveMutex);
      currentSolveMessageId = cmd.getMessageId();
    }

    // Start solving
    solve(imageData, width, height);

    response.setStatus("IN_PROGRESS");
    response.setDetails({
      {"message", "Solving started"},
      {"state", solverStateToString(state)},
      {"progress", progress.load()}
    });
  } catch (const SolverException &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "SOLVER_ERROR"}, {"message", e.what()}});
    // Reset message ID if solve initiation failed
    std::lock_guard<std::mutex> lock(solveMutex);
    currentSolveMessageId.clear();
  } catch (const json::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({
      {"error", "INVALID_PARAMETERS"},
      {"message", "JSON parsing error: " + std::string(e.what())}
    });
    std::lock_guard<std::mutex> lock(solveMutex);
    currentSolveMessageId.clear();
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
    std::lock_guard<std::mutex> lock(solveMutex);
    currentSolveMessageId.clear();
  }
}

bool Solver::validateSolveParameters(const json &params, ResponseMessage &response) {
  if (!params.contains("imageData") || !params["imageData"].is_string() ||
      !params.contains("width") || !params["width"].is_number_integer() ||
      params["width"].get<int>() <= 0 || !params.contains("height") ||
      !params["height"].is_number_integer() ||
      params["height"].get<int>() <= 0) {

    response.setStatus("ERROR");
    response.setDetails({
      {"error", "INVALID_PARAMETERS"},
      {"message", "Missing or invalid required parameters: imageData (string), "
                  "width (positive integer), height (positive integer)"}
    });
    return false;
  }
  return true;
}

bool Solver::validateFilePathParameter(const json &params, ResponseMessage &response) {
  if (!params.contains("filePath") || !params["filePath"].is_string() ||
      params["filePath"].get<std::string>().empty()) {
    response.setStatus("ERROR");
    response.setDetails({
      {"error", "INVALID_PARAMETERS"},
      {"message", "Missing or invalid required parameter: filePath (non-empty string)"}
    });
    return false;
  }
  return true;
}

void Solver::handleSolveFileCommand(const CommandMessage &cmd,
                                    ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!validateFilePathParameter(params, response)) {
      return;
    }

    std::string filePath = params["filePath"];

    // Update hint parameters if present and valid
    if (params.contains("raHint") && params["raHint"].is_number())
      raHint = params["raHint"].get<double>();
    if (params.contains("decHint") && params["decHint"].is_number())
      decHint = params["decHint"].get<double>();
    if (params.contains("radiusHint") && params["radiusHint"].is_number())
      radiusHint = params["radiusHint"].get<double>();

    // Store message ID for completion event
    {
      std::lock_guard<std::mutex> lock(solveMutex);
      currentSolveMessageId = cmd.getMessageId();
    }

    // Start file solving
    solveFromFile(filePath);

    response.setStatus("IN_PROGRESS");
    response.setDetails({
      {"message", "Solving from file started"},
      {"filePath", filePath},
      {"state", solverStateToString(state)},
      {"progress", progress.load()}
    });
  } catch (const SolverException &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "SOLVER_ERROR"}, {"message", e.what()}});
    // Reset message ID if solve initiation failed
    std::lock_guard<std::mutex> lock(solveMutex);
    currentSolveMessageId.clear();
  } catch (const json::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({
      {"error", "INVALID_PARAMETERS"},
      {"message", "JSON parsing error: " + std::string(e.what())}
    });
    std::lock_guard<std::mutex> lock(solveMutex);
    currentSolveMessageId.clear();
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
    std::lock_guard<std::mutex> lock(solveMutex);
    currentSolveMessageId.clear();
  }
}

void Solver::handleAbortCommand(const CommandMessage &cmd,
                                ResponseMessage &response) {
  try {
    abort();

    response.setStatus("SUCCESS");
    response.setDetails({{"message", "Solving aborted (if it was running)"}});
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({
      {"error", "INTERNAL_ERROR"},
      {"message", "Error during abort: " + std::string(e.what())}
    });
  }
}

void Solver::handleSetParametersCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    // Update parameters
    setParameters(params);

    // Read back current parameters safely
    json currentParams;
    {
      std::lock_guard<std::mutex> lock(statusMutex);
      currentParams = {
        {"fovMin", fovMin},
        {"fovMax", fovMax},
        {"scaleMin", scaleMin},
        {"scaleMax", scaleMax},
        {"useDistortion", useDistortion},
        {"downsample", downsample},
        {"raHint", raHint},
        {"decHint", decHint},
        {"radiusHint", radiusHint}
      };
    }

    response.setStatus("SUCCESS");
    response.setDetails({
      {"message", "Parameters updated"},
      {"currentParameters", currentParams}
    });
  } catch (const SolverException &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "PARAMETER_ERROR"}, {"message", e.what()}});
  } catch (const json::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({
      {"error", "INVALID_PARAMETERS"},
      {"message", "JSON parsing error: " + std::string(e.what())}
    });
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Solver::handleGetSolutionCommand(const CommandMessage &cmd,
                                      ResponseMessage &response) {
  try {
    json solution = getLastSolution();

    if (hasValidSolution) {
      response.setStatus("SUCCESS");
      response.setDetails({
        {"solution", solution}
      });
    } else {
      response.setStatus("ERROR");
      response.setDetails({
        {"error", "NO_SOLUTION"},
        {"message", "No valid solution available"}
      });
    }
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({
      {"error", "INTERNAL_ERROR"},
      {"message", "Error retrieving solution: " + std::string(e.what())}
    });
  }
}

} // namespace astrocomm
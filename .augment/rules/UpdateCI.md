---
type: "manual"
---

Based on the current CMake-based build system with vcpkg package management that we've been working with, update the CI/CD pipeline configuration to ensure comprehensive multi-platform support. Specifically:

1. **Platform Coverage**: Configure CI to build and test on Windows (MSVC and MinGW), Linux (Ubuntu/Debian with GCC and Clang), and macOS (with Xcode/Clang)

2. **Build System Integration**:

   - Ensure vcpkg is properly installed and configured on all platforms
   - Use the existing CMake configuration with proper feature flags (HYDROGEN_BUILD_TESTS=ON, HYDROGEN_ENABLE_WEBSOCKETS=ON, etc.)
   - Handle platform-specific dependency resolution

3. **Build Matrix**: Create build configurations for:

   - Debug and Release builds
   - Different compiler versions (GCC 9+, Clang 10+, MSVC 2019+)
   - Optional features enabled/disabled (WebSockets, Python bindings, compression, etc.)

4. **Testing Integration**:

   - Run the test suite on all platforms after successful builds
   - Generate and upload test coverage reports
   - Ensure all compilation errors we've been fixing are caught in CI

5. **Artifact Management**:

   - Build and package binaries for each platform
   - Upload build artifacts and test results
   - Cache vcpkg dependencies to speed up builds

6. **Configuration Files**: Update or create CI configuration files (GitHub Actions, GitLab CI, Jenkins, etc.) that reflect the current project structure with src/common, src/core, src/server, src/client_component, src/device_component, and apps directories.

The goal is to have a robust CI pipeline that catches build failures like the ones we've been systematically fixing (missing WebSocket support, interface mismatches, etc.) before they reach the main branch.

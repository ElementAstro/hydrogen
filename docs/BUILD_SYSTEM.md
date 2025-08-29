# Hydrogen Build System Documentation

## Overview

The Hydrogen project features a modern, modular CMake build system that supports multiple package managers (vcpkg, Conan) with automatic fallback to FetchContent. The build system is designed for maintainability, scalability, and cross-platform compatibility.

## Key Features

- **Modular Architecture**: Separate CMake modules for different concerns
- **Multi-Package Manager Support**: vcpkg, Conan, with FetchContent fallback
- **Feature Toggle System**: Conditional compilation based on available dependencies
- **Modern CMake Practices**: Target-based design with proper export/import
- **Cross-Platform Compatibility**: Windows, Linux, macOS support
- **Performance Optimized**: Parallel builds and dependency caching

## Quick Start

### Using vcpkg

1. Install vcpkg and set `VCPKG_ROOT` environment variable
2. Configure with CMake preset:
   ```bash
   cmake --preset=default
   cmake --build build/default
   ```

### Using Conan

1. Install Conan: `pip install conan`
2. Install dependencies and build:
   ```bash
   conan install . --build=missing
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
   cmake --build build
   ```

### Fallback Mode (No Package Manager)

```bash
cmake -S . -B build -DHYDROGEN_ALLOW_FETCHCONTENT=ON
cmake --build build
```

## Build Options

### Core Options

- `HYDROGEN_BUILD_SHARED_LIBS`: Build shared libraries (default: OFF)
- `HYDROGEN_BUILD_TESTS`: Build unit tests (default: ON)
- `HYDROGEN_BUILD_EXAMPLES`: Build example applications (default: ON)
- `HYDROGEN_BUILD_DOCS`: Build documentation (default: OFF)

### Feature Options

- `HYDROGEN_ENABLE_SSL`: Enable SSL/TLS support (default: ON)
- `HYDROGEN_ENABLE_COMPRESSION`: Enable compression support (default: ON)
- `HYDROGEN_ENABLE_PYTHON_BINDINGS`: Build Python bindings (default: OFF)
- `HYDROGEN_ENABLE_LOGGING`: Enable detailed logging (default: ON)

### Development Options

- `HYDROGEN_ENABLE_WARNINGS`: Enable compiler warnings (default: ON)
- `HYDROGEN_WARNINGS_AS_ERRORS`: Treat warnings as errors (default: OFF)
- `HYDROGEN_ENABLE_SANITIZERS`: Enable runtime sanitizers in Debug (default: OFF)
- `HYDROGEN_ENABLE_LTO`: Enable Link Time Optimization in Release (default: OFF)

## Package Manager Configuration

### vcpkg

The build system automatically detects vcpkg through:
1. `CMAKE_TOOLCHAIN_FILE` pointing to vcpkg.cmake
2. `VCPKG_ROOT` environment variable
3. `vcpkg.json` manifest file in project root

Features:
- Manifest mode with `vcpkg.json`
- CMake presets for different configurations
- Automatic triplet detection
- Feature-based dependency selection

### Conan

Conan integration supports both `conanfile.txt` and `conanfile.py`:
- Automatic toolchain detection
- CMakeDeps and CMakeToolchain generators
- Profile-based configuration
- Version management

### FetchContent Fallback

When package managers are unavailable:
- Automatic fallback to FetchContent
- Dependency caching in `.cmake-cache/`
- Git-based dependency resolution
- Version pinning for reproducible builds

## Architecture

### CMake Modules

- `HydrogenUtils.cmake`: Core utilities and helper functions
- `HydrogenPackageManagers.cmake`: Package manager detection and configuration
- `HydrogenConfig.cmake`: Build options and feature configuration
- `HydrogenCompiler.cmake`: Compiler and platform configuration
- `HydrogenDependencies.cmake`: Unified dependency management
- `HydrogenVersions.cmake`: Version management and conflict resolution
- `HydrogenFeatures.cmake`: Feature toggle system
- `HydrogenExport.cmake`: Target export and package configuration

### Component Structure

```
src/
├── core/                 # Core functionality
├── server_component/     # Server-side functionality
├── client_component/     # Client-side functionality
└── device_component/     # Device implementations
```

Each component has its own CMakeLists.txt with:
- Modern target configuration
- Proper dependency management
- Feature-based conditional compilation
- Export/import support

## Testing

### Automated Tests

Run the comprehensive test suite:

```bash
# Test vcpkg integration
./scripts/test-vcpkg-build.ps1

# Test Conan integration
./scripts/test-conan-build.ps1

# Test FetchContent fallback
./scripts/test-fetchcontent-fallback.ps1

# Test cross-platform compatibility
./scripts/test-cross-platform.ps1
```

### Manual Testing

```bash
# Configure with specific features
cmake -S . -B build \
  -DHYDROGEN_ENABLE_SSL=ON \
  -DHYDROGEN_ENABLE_PYTHON_BINDINGS=ON \
  -DHYDROGEN_BUILD_TESTS=ON

# Build and test
cmake --build build
ctest --test-dir build
```

## Troubleshooting

### Common Issues

1. **Package Manager Not Detected**
   - Ensure vcpkg or Conan is properly installed
   - Check environment variables (`VCPKG_ROOT`)
   - Verify toolchain file paths

2. **Dependency Resolution Failures**
   - Check package availability in chosen package manager
   - Verify version constraints in manifest files
   - Enable FetchContent fallback

3. **Feature Compilation Errors**
   - Check feature dependencies are available
   - Review feature configuration in CMake cache
   - Disable problematic features if not needed

### Debug Information

Enable verbose output for debugging:
```bash
cmake -S . -B build --verbose
cmake --build build --verbose
```

Check feature status:
```bash
cmake -S . -B build
grep "HYDROGEN_HAS_" build/CMakeCache.txt
```

## Migration Guide

### From Legacy Build System

The new build system maintains backward compatibility:

1. Old option names are automatically mapped:
   - `BUILD_PYTHON_BINDINGS` → `HYDROGEN_ENABLE_PYTHON_BINDINGS`
   - `BUILD_TESTS` → `HYDROGEN_BUILD_TESTS`

2. Existing target names are preserved:
   - `AstroComm::Core`, `AstroComm::Server`, etc.

3. Legacy convenience library `astrocomm` still available

### Recommended Migration Steps

1. Update build scripts to use new option names
2. Adopt package manager (vcpkg or Conan)
3. Use CMake presets for standardized configurations
4. Enable new features as needed

## Performance

### Build Time Improvements

- Parallel dependency resolution
- Incremental builds with proper dependency tracking
- Binary caching through package managers
- Optimized target dependencies

### Benchmarks

Typical build times (Release mode, 8-core system):
- Clean build with vcpkg: ~3-5 minutes
- Clean build with Conan: ~4-6 minutes
- Clean build with FetchContent: ~5-8 minutes
- Incremental build: ~10-30 seconds

## Contributing

### Adding New Features

1. Define feature in `HydrogenFeatures.cmake`
2. Add dependency requirements
3. Update component CMakeLists.txt files
4. Add feature documentation
5. Update test scripts

### Adding New Dependencies

1. Add to package manager manifest files
2. Update `HydrogenDependencies.cmake`
3. Add version constraints in `HydrogenVersions.cmake`
4. Test with all package managers

## Support

For build system issues:
1. Check this documentation
2. Review test scripts for examples
3. Enable verbose output for debugging
4. Check CMake cache for configuration details

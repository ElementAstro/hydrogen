# Hydrogen

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Build Status](https://github.com/hydrogen-project/hydrogen/workflows/CI/badge.svg)](https://github.com/hydrogen-project/hydrogen/actions)
[![Documentation](https://img.shields.io/badge/docs-hydrogen--project.github.io-blue)](https://hydrogen-project.github.io/hydrogen)

Modern astronomical device communication protocol and framework designed for seamless integration with astronomical equipment including cameras, focusers, filter wheels, telescopes, and more.

## 🌟 Features

- **Modern C++ Design**: Built with C++17/20 standards and modern CMake practices
- **Cross-Platform**: Supports Windows, macOS, and Linux
- **Modular Architecture**: Component-based design for maximum flexibility
- **Multiple Package Managers**: Support for both Conan and vcpkg
- **Python Bindings**: Full Python API for scripting and automation
- **Device Support**: Comprehensive support for astronomical devices:
  - Cameras and imaging devices
  - Focusers and focusing systems
  - Filter wheels and filters
  - Telescopes and mounts
  - Guiders and autoguiding
  - Rotators and field rotation
  - Switches and power management
  - Plate solvers and astrometry

## 🚀 Quick Start

### Prerequisites

- **C++ Compiler**: GCC 9+, Clang 10+, or MSVC 2019+
- **CMake**: Version 3.15 or higher
- **Package Manager**: Either Conan 2.0+ or vcpkg

### Building with vcpkg

```bash
# Clone the repository
git clone https://github.com/hydrogen-project/hydrogen.git
cd hydrogen

# Configure and build
cmake --preset default
cmake --build --preset default
```

### Building with Conan

```bash
# Install dependencies
conan install . --build=missing

# Configure and build
cmake --preset conan-default
cmake --build --preset conan-default
```

## 📖 Documentation

- **[Build System Guide](docs/BUILD_SYSTEM.md)**: Comprehensive build system documentation
- **[Implementation Summary](docs/IMPLEMENTATION_SUMMARY.md)**: Technical implementation details
- **[API Documentation](https://hydrogen-project.github.io/hydrogen)**: Complete API reference

## 🔧 Development

### Building with Tests

```bash
# Enable tests during configuration
cmake --preset default -DHYDROGEN_BUILD_TESTS=ON
cmake --build --preset default

# Run tests
ctest --preset default
```

### Python Bindings

```bash
# Enable Python bindings
cmake --preset default -DHYDROGEN_ENABLE_PYTHON_BINDINGS=ON
cmake --build --preset default
```

### Available CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `HYDROGEN_BUILD_TESTS` | `OFF` | Build unit tests |
| `HYDROGEN_ENABLE_PYTHON_BINDINGS` | `OFF` | Build Python bindings |
| `HYDROGEN_BUILD_EXAMPLES` | `ON` | Build example applications |
| `HYDROGEN_BUILD_BENCHMARKS` | `OFF` | Build performance benchmarks |
| `HYDROGEN_ENABLE_SSL` | `ON` | Enable SSL/TLS support |
| `HYDROGEN_ENABLE_COMPRESSION` | `OFF` | Enable compression support |

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details on how to get started.

## 📄 License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## 🔒 Security

For security concerns, please see our [Security Policy](SECURITY.md).

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/hydrogen-project/hydrogen/issues)
- **Discussions**: [GitHub Discussions](https://github.com/hydrogen-project/hydrogen/discussions)
- **Documentation**: [Project Documentation](https://hydrogen-project.github.io/hydrogen)
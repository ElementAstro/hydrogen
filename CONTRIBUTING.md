# Contributing to Hydrogen

Thank you for your interest in contributing to Hydrogen! This document provides guidelines and information for contributors.

## 🚀 Getting Started

### Prerequisites

- **C++ Compiler**: GCC 9+, Clang 10+, or MSVC 2019+
- **CMake**: Version 3.15 or higher
- **Package Manager**: Either Conan 2.0+ or vcpkg
- **Git**: For version control

### Development Setup

1. **Fork and Clone**
   ```bash
   git clone https://github.com/YOUR_USERNAME/hydrogen.git
   cd hydrogen
   ```

2. **Build with Development Options**
   ```bash
   # With vcpkg
   cmake --preset default -DHYDROGEN_BUILD_TESTS=ON -DHYDROGEN_BUILD_EXAMPLES=ON
   cmake --build --preset default
   
   # With Conan
   conan install . --build=missing
   cmake --preset conan-default -DHYDROGEN_BUILD_TESTS=ON
   cmake --build --preset conan-default
   ```

3. **Run Tests**
   ```bash
   ctest --preset default
   ```

## 📝 Code Style Guidelines

### C++ Standards
- Use **C++17** as the minimum standard
- Follow modern C++ best practices
- Use RAII and smart pointers
- Prefer `const` and `constexpr` where applicable

### Naming Conventions
- **Classes**: PascalCase (`DeviceManager`)
- **Functions/Methods**: camelCase (`connectDevice`)
- **Variables**: camelCase (`deviceId`)
- **Constants**: UPPER_SNAKE_CASE (`MAX_RETRY_COUNT`)
- **Files**: snake_case (`device_manager.h`)

### Code Organization
- Header files: `.h` extension
- Implementation files: `.cpp` extension
- One class per file pair
- Include guards or `#pragma once`
- Forward declarations when possible

### Documentation
- Use Doxygen-style comments for public APIs
- Document all public methods and classes
- Include usage examples for complex APIs

## 🔄 Development Workflow

### Branch Strategy
- `main`: Stable release branch
- `develop`: Integration branch for new features
- `feature/description`: Feature development branches
- `bugfix/description`: Bug fix branches

### Pull Request Process

1. **Create Feature Branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make Changes**
   - Write tests for new functionality
   - Ensure all tests pass
   - Follow code style guidelines
   - Update documentation as needed

3. **Commit Guidelines**
   - Use conventional commit format: `type(scope): description`
   - Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`
   - Example: `feat(camera): add exposure time validation`

4. **Submit Pull Request**
   - Fill out the PR template completely
   - Link related issues
   - Ensure CI passes
   - Request review from maintainers

## 🧪 Testing Guidelines

### Unit Tests
- Write tests for all new functionality
- Use Google Test framework
- Place tests in `tests/` directory
- Maintain >80% code coverage

### Integration Tests
- Test device communication protocols
- Verify cross-platform compatibility
- Test with real hardware when possible

### Performance Tests
- Benchmark critical paths
- Monitor memory usage
- Test with large datasets

## 🏗️ Build System

### CMake Best Practices
- Use modern CMake (3.15+)
- Prefer target-based approach
- Use generator expressions
- Support both Conan and vcpkg

### Adding Dependencies
- Update both `vcpkg.json` and `conanfile.py`
- Use version ranges appropriately
- Test with both package managers

## 🐛 Reporting Issues

### Bug Reports
- Use the bug report template
- Include minimal reproduction case
- Specify platform and compiler
- Attach relevant logs

### Feature Requests
- Use the feature request template
- Explain the use case clearly
- Consider implementation complexity
- Discuss alternatives

## 📋 Code Review Guidelines

### For Authors
- Keep PRs focused and small
- Write clear commit messages
- Respond to feedback promptly
- Update tests and documentation

### For Reviewers
- Be constructive and respectful
- Focus on code quality and maintainability
- Check for security implications
- Verify test coverage

## 🎯 Areas for Contribution

### High Priority
- Device driver implementations
- Cross-platform testing
- Documentation improvements
- Performance optimizations

### Good First Issues
- Code style improvements
- Test coverage expansion
- Documentation updates
- Example applications

## 📞 Getting Help

- **Questions**: Use GitHub Discussions
- **Issues**: Create GitHub Issues
- **Real-time**: Join our community chat
- **Documentation**: Check project docs first

## 📄 License

By contributing to Hydrogen, you agree that your contributions will be licensed under the GNU General Public License v3.0.

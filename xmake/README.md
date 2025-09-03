# Hydrogen XMake Modular Configuration

This directory contains the modular XMake build configuration for the Hydrogen project. The build system has been split into logical components to improve maintainability and organization.

## 📁 File Structure

```
xmake/
├── README.md           # This documentation file
├── libraries.lua       # Library target definitions
├── applications.lua    # Application target definitions
├── tests.lua          # Test target definitions
├── python.lua         # Python bindings configuration
└── tasks.lua          # Custom tasks and utilities
```

## 🔧 Component Overview

**Note**: Build options, global configuration, and dependencies are now embedded directly in the main `xmake.lua` file for simplicity. Only target definitions are modularized.

### `libraries.lua` - Library Targets
- **Purpose**: Defines all library targets for the Hydrogen project
- **Contains**: Common, Core, Server, Client, Device, and convenience libraries
- **Functions**: `create_all_libraries()`, `create_*_library()`

### `applications.lua` - Application Targets
- **Purpose**: Defines all executable application targets
- **Contains**: Server, client, and device simulator applications
- **Functions**: `create_all_applications()`, `create_application()`

### `tests.lua` - Test Configuration
- **Purpose**: Defines test targets and testing infrastructure
- **Contains**: Test utilities, component tests, integration tests, performance tests
- **Functions**: `create_all_tests()`, `create_test_target()`

### `python.lua` - Python Bindings
- **Purpose**: Configures Python bindings using pybind11
- **Contains**: Python module definition, installation helpers, development tasks
- **Functions**: `create_all_python_targets()`, `create_python_bindings()`

### `tasks.lua` - Custom Tasks
- **Purpose**: Defines custom build tasks and utilities
- **Contains**: Build tasks, development tasks, utility tasks, CI/CD tasks
- **Functions**: `create_all_tasks()`, various task definitions

## 🚀 Usage

The modular target definitions are automatically loaded by the main `xmake.lua` file:

```lua
-- Main xmake.lua includes target modules
includes("xmake/libraries.lua")
includes("xmake/applications.lua")
includes("xmake/tests.lua")
includes("xmake/python.lua")
includes("xmake/tasks.lua")
```

The main `xmake.lua` file contains all the configuration, options, and dependencies directly, while the modular files contain only the target definitions.

## 🔄 Benefits of Modular Structure

### **Maintainability**
- **Separation of Concerns**: Each file has a single, well-defined responsibility
- **Easier Navigation**: Find specific configuration quickly
- **Reduced Complexity**: Smaller, focused files are easier to understand

### **Scalability**
- **Easy Extension**: Add new components without modifying existing files
- **Parallel Development**: Multiple developers can work on different components
- **Version Control**: Cleaner diffs and easier conflict resolution

### **Reusability**
- **Component Reuse**: Individual modules can be reused in other projects
- **Selective Loading**: Load only needed components for specific use cases
- **Template Creation**: Use as templates for similar projects

### **Testing and Debugging**
- **Isolated Testing**: Test individual components independently
- **Easier Debugging**: Narrow down issues to specific modules
- **Incremental Development**: Develop and test one component at a time

## 🛠️ Customization

### Adding New Options
1. Edit the main `xmake.lua` file
2. Add the new option definition in the options section
3. Update the build summary function if needed

### Adding New Dependencies
1. Edit the main `xmake.lua` file
2. Add dependency in the dependencies section
3. Update target files to link the new dependency

### Adding New Targets
1. Edit appropriate target file (`libraries.lua`, `applications.lua`, `tests.lua`)
2. Add target definition directly in the file
3. The target will be automatically included

### Adding New Tasks
1. Edit `xmake/tasks.lua`
2. Add task definition directly in the file
3. The task will be automatically available

## 📋 Migration from Monolithic

The original monolithic `xmake.lua` has been backed up as `xmake-original.lua`. Key changes:

- **918 lines** → **~90 lines** main file + **~1800 lines** across 8 modules
- **Single file** → **9 focused files**
- **Mixed concerns** → **Clear separation of responsibilities**
- **Hard to navigate** → **Easy to find and modify specific functionality**

## 🔍 Function Reference

### Main Orchestration Functions
- `apply_global_configuration()` - Apply all global settings
- `configure_all_dependencies()` - Set up all dependencies
- `validate_options()` - Validate option combinations
- `create_all_libraries()` - Create all library targets
- `create_all_applications()` - Create all application targets
- `create_all_tests()` - Create all test targets
- `create_all_python_targets()` - Create Python bindings
- `create_all_tasks()` - Create custom tasks

### Utility Functions
- `print_options_summary()` - Display build options
- `configure_target_common()` - Apply common target settings
- `create_application()` - Helper for creating applications
- `create_test_target()` - Helper for creating tests
- `link_*_dependencies()` - Dependency linking helpers

## 🎯 Best Practices

1. **Keep Functions Focused**: Each function should have a single responsibility
2. **Use Descriptive Names**: Function and variable names should be self-documenting
3. **Group Related Functionality**: Keep related functions in the same file
4. **Document Changes**: Update this README when adding new components
5. **Test Incrementally**: Test each component as you develop it
6. **Follow Conventions**: Maintain consistent naming and structure patterns

This modular structure makes the Hydrogen XMake build system more maintainable, scalable, and easier to understand while preserving all original functionality.

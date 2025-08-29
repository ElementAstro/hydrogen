from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy
import os


class HydrogenConan(ConanFile):
    name = "hydrogen"
    version = "1.0.0"
    description = "Modern astronomical device communication protocol and framework"
    license = "MIT"
    author = "Hydrogen Project"
    url = "https://github.com/hydrogen-project/hydrogen"
    homepage = "https://hydrogen-project.github.io/hydrogen"
    topics = ("astronomy", "device-communication", "protocol", "framework")
    
    # Package configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tests": [True, False],
        "with_python_bindings": [True, False],
        "with_examples": [True, False],
        "with_benchmarks": [True, False],
        "with_ssl": [True, False],
        "with_compression": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tests": False,
        "with_python_bindings": False,
        "with_examples": True,
        "with_benchmarks": False,
        "with_ssl": True,
        "with_compression": True,
    }
    
    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "cmake/*", "include/*", "tests/*", "examples/*"
    
    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
    
    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
    
    def layout(self):
        cmake_layout(self)
    
    def requirements(self):
        # Core dependencies
        self.requires("boost/1.84.0")
        self.requires("nlohmann_json/3.11.3")
        
        # Optional dependencies based on features
        if self.options.with_ssl:
            self.requires("openssl/3.2.0")
        
        if self.options.with_compression:
            self.requires("zlib/1.3.1")
        
        # Web framework dependency
        self.requires("crow/1.2.0")
    
    def build_requirements(self):
        self.tool_requires("cmake/3.28.1")
        
        if self.options.with_tests:
            self.test_requires("gtest/1.14.0")
        
        if self.options.with_python_bindings:
            self.requires("pybind11/2.11.1")
        
        if self.options.with_benchmarks:
            self.test_requires("benchmark/1.8.3")
    
    def generate(self):
        # Generate CMake configuration
        deps = CMakeDeps(self)
        deps.generate()
        
        tc = CMakeToolchain(self)
        
        # Set CMake variables based on options
        tc.variables["HYDROGEN_BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["HYDROGEN_BUILD_TESTS"] = self.options.with_tests
        tc.variables["HYDROGEN_BUILD_EXAMPLES"] = self.options.with_examples
        tc.variables["HYDROGEN_BUILD_BENCHMARKS"] = self.options.with_benchmarks
        tc.variables["HYDROGEN_ENABLE_PYTHON_BINDINGS"] = self.options.with_python_bindings
        tc.variables["HYDROGEN_ENABLE_SSL"] = self.options.with_ssl
        tc.variables["HYDROGEN_ENABLE_COMPRESSION"] = self.options.with_compression
        
        # Set build type specific variables
        if self.settings.build_type == "Debug":
            tc.variables["HYDROGEN_ENABLE_WARNINGS"] = True
            tc.variables["HYDROGEN_ENABLE_SANITIZERS"] = True
        elif self.settings.build_type == "Release":
            tc.variables["HYDROGEN_ENABLE_LTO"] = True
        
        tc.generate()
    
    def build(self):
        from conan.tools.cmake import CMake
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        
        # Run tests if enabled
        if self.options.with_tests:
            cmake.test()
    
    def package(self):
        from conan.tools.cmake import CMake
        cmake = CMake(self)
        cmake.install()
        
        # Copy license
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
    
    def package_info(self):
        # Core library
        self.cpp_info.components["core"].libs = ["hydrogen_core"]
        self.cpp_info.components["core"].names["cmake_find_package"] = "Hydrogen"
        self.cpp_info.components["core"].names["cmake_find_package_multi"] = "Hydrogen"
        
        # Server component
        self.cpp_info.components["server"].libs = ["hydrogen_server"]
        self.cpp_info.components["server"].requires = ["core", "boost::boost", "nlohmann_json::nlohmann_json"]
        if self.options.with_ssl:
            self.cpp_info.components["server"].requires.append("openssl::openssl")
        
        # Client component
        self.cpp_info.components["client"].libs = ["hydrogen_client"]
        self.cpp_info.components["client"].requires = ["core", "boost::boost"]
        if self.options.with_ssl:
            self.cpp_info.components["client"].requires.append("openssl::openssl")
        
        # Device component
        self.cpp_info.components["device"].libs = ["hydrogen_device"]
        self.cpp_info.components["device"].requires = ["core", "boost::boost"]
        
        # Set global properties
        self.cpp_info.set_property("cmake_file_name", "Hydrogen")
        self.cpp_info.set_property("cmake_target_name", "Hydrogen::Hydrogen")
        
        # System libraries
        if self.settings.os == "Windows":
            self.cpp_info.system_libs.extend(["ws2_32", "mswsock"])
        elif self.settings.os == "Linux":
            self.cpp_info.system_libs.extend(["pthread", "dl"])
        elif self.settings.os == "Macos":
            self.cpp_info.frameworks.extend(["Foundation", "CoreFoundation"])
        
        # Compiler flags
        if self.settings.compiler == "gcc" or self.settings.compiler == "clang":
            self.cpp_info.cppflags.append("-std=c++17")
        
        # Define preprocessor macros
        if self.options.with_ssl:
            self.cpp_info.defines.append("HYDROGEN_HAS_SSL")
        if self.options.with_compression:
            self.cpp_info.defines.append("HYDROGEN_HAS_COMPRESSION")
        if self.options.shared:
            self.cpp_info.defines.append("HYDROGEN_SHARED")

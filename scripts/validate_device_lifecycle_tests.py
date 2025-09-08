#!/usr/bin/env python3
"""
Device Lifecycle Tests Validation Script

This script validates that the device lifecycle tests are properly configured
and can be built and executed with both CMake and XMake build systems.
"""

import os
import sys
import subprocess
import argparse
import json
from pathlib import Path
from typing import Dict, List, Optional, Tuple

class Colors:
    """ANSI color codes for terminal output"""
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    PURPLE = '\033[0;35m'
    CYAN = '\033[0;36m'
    WHITE = '\033[1;37m'
    NC = '\033[0m'  # No Color

class TestValidator:
    """Validates device lifecycle tests configuration and execution"""
    
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.results = {
            'file_checks': {},
            'build_checks': {},
            'test_execution': {},
            'overall_status': 'unknown'
        }
    
    def print_status(self, message: str, color: str = Colors.BLUE):
        """Print colored status message"""
        print(f"{color}[INFO]{Colors.NC} {message}")
    
    def print_success(self, message: str):
        """Print success message"""
        print(f"{Colors.GREEN}[SUCCESS]{Colors.NC} {message}")
    
    def print_warning(self, message: str):
        """Print warning message"""
        print(f"{Colors.YELLOW}[WARNING]{Colors.NC} {message}")
    
    def print_error(self, message: str):
        """Print error message"""
        print(f"{Colors.RED}[ERROR]{Colors.NC} {message}")
    
    def check_file_exists(self, file_path: Path, description: str) -> bool:
        """Check if a file exists and record result"""
        exists = file_path.exists()
        self.results['file_checks'][description] = {
            'path': str(file_path),
            'exists': exists
        }
        
        if exists:
            self.print_success(f"{description}: {file_path}")
        else:
            self.print_error(f"{description} not found: {file_path}")
        
        return exists
    
    def validate_file_structure(self) -> bool:
        """Validate that all required files exist"""
        self.print_status("Validating file structure...")
        
        required_files = [
            (self.project_root / "src/core/src/device/device_lifecycle.cpp", "Device lifecycle implementation"),
            (self.project_root / "src/core/include/hydrogen/core/device/device_lifecycle.h", "Device lifecycle header"),
            (self.project_root / "tests/core/device/test_device_lifecycle.cpp", "Device lifecycle tests"),
            (self.project_root / "tests/core/CMakeLists.txt", "Core tests CMake configuration"),
            (self.project_root / "xmake/tests.lua", "XMake tests configuration"),
            (self.project_root / "scripts/run_device_lifecycle_tests.sh", "Linux test runner script"),
            (self.project_root / "scripts/run_device_lifecycle_tests.bat", "Windows test runner script"),
            (self.project_root / "tests/core/device/README.md", "Test documentation")
        ]
        
        all_exist = True
        for file_path, description in required_files:
            if not self.check_file_exists(file_path, description):
                all_exist = False
        
        return all_exist
    
    def validate_test_content(self) -> bool:
        """Validate test file content and structure"""
        self.print_status("Validating test content...")
        
        test_file = self.project_root / "tests/core/device/test_device_lifecycle.cpp"
        if not test_file.exists():
            self.print_error("Test file does not exist")
            return False
        
        try:
            content = test_file.read_text()
            
            # Check for required includes
            required_includes = [
                '#include <gtest/gtest.h>',
                '#include <gmock/gmock.h>',
                '#include "hydrogen/core/device/device_lifecycle.h"'
            ]
            
            missing_includes = []
            for include in required_includes:
                if include not in content:
                    missing_includes.append(include)
            
            if missing_includes:
                self.print_error(f"Missing includes: {missing_includes}")
                return False
            
            # Check for test cases
            test_patterns = [
                'TEST_F(DeviceLifecycleTest,',
                'DeviceRegistrationAndUnregistration',
                'ValidStateTransitions',
                'InvalidStateTransitions',
                'ErrorHandlingAndRecovery',
                'ConcurrentOperations',
                'PerformanceAndScalability'
            ]
            
            missing_tests = []
            for pattern in test_patterns:
                if pattern not in content:
                    missing_tests.append(pattern)
            
            if missing_tests:
                self.print_error(f"Missing test patterns: {missing_tests}")
                return False
            
            # Count test cases
            test_count = content.count('TEST_F(DeviceLifecycleTest,')
            self.print_success(f"Found {test_count} test cases")
            
            if test_count < 20:
                self.print_warning(f"Expected at least 20 test cases, found {test_count}")
            
            return True
            
        except Exception as e:
            self.print_error(f"Error reading test file: {e}")
            return False
    
    def validate_cmake_configuration(self) -> bool:
        """Validate CMake configuration for device lifecycle tests"""
        self.print_status("Validating CMake configuration...")
        
        cmake_file = self.project_root / "tests/core/CMakeLists.txt"
        if not cmake_file.exists():
            self.print_error("CMake configuration file does not exist")
            return False
        
        try:
            content = cmake_file.read_text()
            
            required_elements = [
                'core_device_lifecycle_tests',
                'device/test_device_lifecycle.cpp',
                'gtest_discover_tests(core_device_lifecycle_tests)'
            ]
            
            missing_elements = []
            for element in required_elements:
                if element not in content:
                    missing_elements.append(element)
            
            if missing_elements:
                self.print_error(f"Missing CMake elements: {missing_elements}")
                return False
            
            self.print_success("CMake configuration is valid")
            return True
            
        except Exception as e:
            self.print_error(f"Error reading CMake file: {e}")
            return False
    
    def validate_xmake_configuration(self) -> bool:
        """Validate XMake configuration for device lifecycle tests"""
        self.print_status("Validating XMake configuration...")
        
        xmake_file = self.project_root / "xmake/tests.lua"
        if not xmake_file.exists():
            self.print_error("XMake configuration file does not exist")
            return False
        
        try:
            content = xmake_file.read_text()
            
            # Check that core_tests includes all core test files
            if 'add_files("../tests/core/**.cpp")' in content:
                self.print_success("XMake configuration includes device lifecycle tests")
                return True
            else:
                self.print_error("XMake configuration may not include device lifecycle tests")
                return False
            
        except Exception as e:
            self.print_error(f"Error reading XMake file: {e}")
            return False
    
    def try_cmake_build(self) -> bool:
        """Try to build tests with CMake"""
        self.print_status("Attempting CMake build...")
        
        build_dir = self.project_root / "build_validation"
        
        try:
            # Clean previous build
            if build_dir.exists():
                import shutil
                shutil.rmtree(build_dir)
            
            build_dir.mkdir()
            
            # Configure
            configure_cmd = [
                'cmake',
                '-B', str(build_dir),
                '-DHYDROGEN_BUILD_TESTS=ON',
                '-DHYDROGEN_BUILD_EXAMPLES=OFF',
                str(self.project_root)
            ]
            
            result = subprocess.run(configure_cmd, capture_output=True, text=True, cwd=self.project_root)
            
            if result.returncode != 0:
                self.print_error(f"CMake configure failed: {result.stderr}")
                return False
            
            # Build
            build_cmd = [
                'cmake',
                '--build', str(build_dir),
                '--target', 'core_device_lifecycle_tests',
                '--parallel'
            ]
            
            result = subprocess.run(build_cmd, capture_output=True, text=True, cwd=self.project_root)
            
            if result.returncode != 0:
                self.print_error(f"CMake build failed: {result.stderr}")
                return False
            
            # Check if executable exists
            test_exe = build_dir / "tests/core/core_device_lifecycle_tests"
            if not test_exe.exists():
                # Try Windows extension
                test_exe = build_dir / "tests/core/core_device_lifecycle_tests.exe"
            
            if test_exe.exists():
                self.print_success("CMake build successful")
                self.results['build_checks']['cmake'] = {'success': True, 'executable': str(test_exe)}
                return True
            else:
                self.print_error("CMake build completed but executable not found")
                return False
            
        except Exception as e:
            self.print_error(f"CMake build error: {e}")
            return False
    
    def try_xmake_build(self) -> bool:
        """Try to build tests with XMake"""
        self.print_status("Attempting XMake build...")
        
        try:
            # Configure
            configure_cmd = [
                'xmake', 'config',
                '--mode=debug',
                '--tests=y',
                '--examples=n'
            ]
            
            result = subprocess.run(configure_cmd, capture_output=True, text=True, cwd=self.project_root)
            
            if result.returncode != 0:
                self.print_error(f"XMake configure failed: {result.stderr}")
                return False
            
            # Build
            build_cmd = ['xmake', 'build', 'core_tests']
            
            result = subprocess.run(build_cmd, capture_output=True, text=True, cwd=self.project_root)
            
            if result.returncode != 0:
                self.print_error(f"XMake build failed: {result.stderr}")
                return False
            
            self.print_success("XMake build successful")
            self.results['build_checks']['xmake'] = {'success': True}
            return True
            
        except Exception as e:
            self.print_error(f"XMake build error: {e}")
            return False
    
    def try_test_execution(self) -> bool:
        """Try to execute the tests"""
        self.print_status("Attempting test execution...")
        
        # Try CMake test execution
        cmake_success = False
        if 'cmake' in self.results['build_checks'] and self.results['build_checks']['cmake']['success']:
            try:
                test_exe = Path(self.results['build_checks']['cmake']['executable'])
                if test_exe.exists():
                    # Run a quick test with limited output
                    result = subprocess.run([str(test_exe), '--gtest_filter=*Registration*', '--gtest_brief=1'], 
                                          capture_output=True, text=True, timeout=30)
                    
                    if result.returncode == 0:
                        self.print_success("CMake test execution successful")
                        cmake_success = True
                    else:
                        self.print_error(f"CMake test execution failed: {result.stderr}")
            except Exception as e:
                self.print_error(f"CMake test execution error: {e}")
        
        # Try XMake test execution
        xmake_success = False
        if 'xmake' in self.results['build_checks'] and self.results['build_checks']['xmake']['success']:
            try:
                result = subprocess.run(['xmake', 'test', 'core_tests'], 
                                      capture_output=True, text=True, timeout=60, cwd=self.project_root)
                
                if result.returncode == 0:
                    self.print_success("XMake test execution successful")
                    xmake_success = True
                else:
                    self.print_warning(f"XMake test execution had issues: {result.stderr}")
            except Exception as e:
                self.print_error(f"XMake test execution error: {e}")
        
        self.results['test_execution'] = {
            'cmake': cmake_success,
            'xmake': xmake_success
        }
        
        return cmake_success or xmake_success
    
    def generate_report(self) -> Dict:
        """Generate validation report"""
        self.print_status("Generating validation report...")
        
        # Calculate overall status
        file_checks_passed = all(check['exists'] for check in self.results['file_checks'].values())
        build_checks_passed = any(check.get('success', False) for check in self.results['build_checks'].values())
        test_execution_passed = any(self.results['test_execution'].values())
        
        if file_checks_passed and build_checks_passed and test_execution_passed:
            self.results['overall_status'] = 'success'
        elif file_checks_passed and build_checks_passed:
            self.results['overall_status'] = 'partial'
        else:
            self.results['overall_status'] = 'failed'
        
        return self.results
    
    def run_validation(self) -> bool:
        """Run complete validation"""
        self.print_status("Starting device lifecycle tests validation")
        self.print_status("=" * 50)
        
        # File structure validation
        file_structure_ok = self.validate_file_structure()
        
        # Test content validation
        test_content_ok = self.validate_test_content()
        
        # Build configuration validation
        cmake_config_ok = self.validate_cmake_configuration()
        xmake_config_ok = self.validate_xmake_configuration()
        
        # Build attempts
        cmake_build_ok = self.try_cmake_build()
        xmake_build_ok = self.try_xmake_build()
        
        # Test execution attempts
        test_execution_ok = self.try_test_execution()
        
        # Generate report
        report = self.generate_report()
        
        # Print summary
        print("\n" + "=" * 50)
        self.print_status("VALIDATION SUMMARY")
        print("=" * 50)
        
        if report['overall_status'] == 'success':
            self.print_success("✅ All validations passed!")
        elif report['overall_status'] == 'partial':
            self.print_warning("⚠️  Partial validation - some issues found")
        else:
            self.print_error("❌ Validation failed - significant issues found")
        
        print(f"\nFile Structure: {'✅' if file_structure_ok else '❌'}")
        print(f"Test Content: {'✅' if test_content_ok else '❌'}")
        print(f"CMake Config: {'✅' if cmake_config_ok else '❌'}")
        print(f"XMake Config: {'✅' if xmake_config_ok else '❌'}")
        print(f"CMake Build: {'✅' if cmake_build_ok else '❌'}")
        print(f"XMake Build: {'✅' if xmake_build_ok else '❌'}")
        print(f"Test Execution: {'✅' if test_execution_ok else '❌'}")
        
        return report['overall_status'] == 'success'

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description='Validate device lifecycle tests configuration')
    parser.add_argument('--project-root', type=Path, default=Path.cwd(),
                       help='Path to project root directory')
    parser.add_argument('--output', type=Path,
                       help='Output validation report to JSON file')
    parser.add_argument('--verbose', action='store_true',
                       help='Enable verbose output')
    
    args = parser.parse_args()
    
    # Validate project root
    if not (args.project_root / 'CMakeLists.txt').exists() and not (args.project_root / 'xmake.lua').exists():
        print(f"{Colors.RED}[ERROR]{Colors.NC} Not a valid Hydrogen project root: {args.project_root}")
        sys.exit(1)
    
    # Run validation
    validator = TestValidator(args.project_root)
    success = validator.run_validation()
    
    # Save report if requested
    if args.output:
        with open(args.output, 'w') as f:
            json.dump(validator.results, f, indent=2)
        print(f"\nValidation report saved to: {args.output}")
    
    # Exit with appropriate code
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()

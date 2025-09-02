#!/usr/bin/env python3
"""
Hydrogen Optimized Build Script
Comprehensive build automation with performance optimization and CI/CD integration
"""

import os
import sys
import subprocess
import argparse
import json
import time
import multiprocessing
import shutil
from pathlib import Path
from typing import Dict, List, Optional, Tuple

class HydrogenBuilder:
    """Optimized build system for Hydrogen project"""
    
    def __init__(self, source_dir: Path, build_dir: Path):
        self.source_dir = source_dir
        self.build_dir = build_dir
        self.start_time = time.time()
        self.build_stats = {}
        
    def detect_system_capabilities(self) -> Dict:
        """Detect system capabilities for optimization"""
        capabilities = {
            'cpu_cores': multiprocessing.cpu_count(),
            'memory_gb': self._get_memory_gb(),
            'has_ccache': shutil.which('ccache') is not None,
            'has_sccache': shutil.which('sccache') is not None,
            'has_ninja': shutil.which('ninja') is not None,
            'compiler': self._detect_compiler(),
        }
        
        print(f"🔍 System Capabilities:")
        print(f"  CPU Cores: {capabilities['cpu_cores']}")
        print(f"  Memory: {capabilities['memory_gb']} GB")
        print(f"  Compiler: {capabilities['compiler']}")
        print(f"  Build Tools: Ninja={capabilities['has_ninja']}, ccache={capabilities['has_ccache']}")
        
        return capabilities
    
    def _get_memory_gb(self) -> int:
        """Get system memory in GB"""
        try:
            if sys.platform == "linux":
                with open('/proc/meminfo', 'r') as f:
                    for line in f:
                        if line.startswith('MemTotal:'):
                            kb = int(line.split()[1])
                            return kb // (1024 * 1024)
            elif sys.platform == "darwin":
                result = subprocess.run(['sysctl', '-n', 'hw.memsize'], 
                                      capture_output=True, text=True)
                if result.returncode == 0:
                    bytes_mem = int(result.stdout.strip())
                    return bytes_mem // (1024 * 1024 * 1024)
            elif sys.platform == "win32":
                import psutil
                return psutil.virtual_memory().total // (1024 * 1024 * 1024)
        except:
            pass
        return 8  # Default fallback
    
    def _detect_compiler(self) -> str:
        """Detect available compiler"""
        compilers = ['clang++', 'g++', 'cl.exe']
        for compiler in compilers:
            if shutil.which(compiler):
                return compiler
        return 'unknown'
    
    def configure_build(self, preset: str, extra_args: List[str] = None) -> bool:
        """Configure build with optimizations"""
        print(f"🔧 Configuring build with preset: {preset}")
        
        capabilities = self.detect_system_capabilities()
        
        # Base configuration command
        cmd = ['cmake', '--preset', preset]
        
        # Add optimization flags based on system capabilities
        optimization_flags = self._get_optimization_flags(capabilities, preset)
        cmd.extend(optimization_flags)
        
        # Add extra arguments
        if extra_args:
            cmd.extend(extra_args)
        
        print(f"Configuration command: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(cmd, cwd=self.source_dir, check=True)
            print("✅ Configuration successful")
            return True
        except subprocess.CalledProcessError as e:
            print(f"❌ Configuration failed: {e}")
            return False
    
    def _get_optimization_flags(self, capabilities: Dict, preset: str) -> List[str]:
        """Get optimization flags based on system capabilities"""
        flags = []
        
        # Enable build optimizations
        flags.append('-DHYDROGEN_ENABLE_BUILD_OPTIMIZATIONS=ON')
        
        # Configure parallel builds
        parallel_jobs = max(1, capabilities['cpu_cores'] * 3 // 4)
        flags.append(f'-DCMAKE_BUILD_PARALLEL_LEVEL={parallel_jobs}')
        
        # Enable compiler caching
        if capabilities['has_ccache']:
            flags.append('-DHYDROGEN_ENABLE_CCACHE=ON')
        
        # Enable unity builds for large systems
        if capabilities['memory_gb'] >= 16 and capabilities['cpu_cores'] >= 8:
            flags.append('-DHYDROGEN_ENABLE_UNITY_BUILD=ON')
        
        # Enable LTO for release builds
        if 'release' in preset.lower():
            flags.append('-DHYDROGEN_ENABLE_LTO=ON')
        
        # Memory optimization for systems with limited RAM
        if capabilities['memory_gb'] < 8:
            flags.append('-DHYDROGEN_ENABLE_MEMORY_OPTIMIZATION=ON')
        
        return flags
    
    def build(self, preset: str, targets: List[str] = None, verbose: bool = False) -> bool:
        """Execute optimized build"""
        print(f"🔨 Building with preset: {preset}")
        
        capabilities = self.detect_system_capabilities()
        
        # Base build command
        cmd = ['cmake', '--build', '--preset', preset]
        
        # Add parallel build configuration
        parallel_jobs = max(1, capabilities['cpu_cores'] * 3 // 4)
        cmd.extend(['--parallel', str(parallel_jobs)])
        
        # Add targets if specified
        if targets:
            cmd.append('--target')
            cmd.extend(targets)
        
        # Add verbose flag if requested
        if verbose:
            cmd.append('--verbose')
        
        print(f"Build command: {' '.join(cmd)}")
        
        build_start = time.time()
        
        try:
            result = subprocess.run(cmd, cwd=self.source_dir, check=True)
            build_duration = time.time() - build_start
            
            self.build_stats['build_duration'] = build_duration
            self.build_stats['parallel_jobs'] = parallel_jobs
            
            print(f"✅ Build successful in {build_duration:.1f}s using {parallel_jobs} parallel jobs")
            return True
        except subprocess.CalledProcessError as e:
            print(f"❌ Build failed: {e}")
            return False
    
    def test(self, preset: str, test_types: List[str] = None, parallel: bool = True) -> bool:
        """Run optimized tests"""
        print(f"🧪 Running tests with preset: {preset}")
        
        capabilities = self.detect_system_capabilities()
        
        # Base test command
        cmd = ['ctest', '--preset', preset]
        
        # Configure parallel testing
        if parallel:
            test_jobs = max(1, capabilities['cpu_cores'] // 2)
            cmd.extend(['--parallel', str(test_jobs)])
        
        # Add test type filters
        if test_types:
            for test_type in test_types:
                cmd.extend(['--label-regex', test_type])
        
        # Add output options
        cmd.extend(['--output-on-failure', '--timeout', '300'])
        
        print(f"Test command: {' '.join(cmd)}")
        
        test_start = time.time()
        
        try:
            result = subprocess.run(cmd, cwd=self.source_dir, check=True)
            test_duration = time.time() - test_start
            
            self.build_stats['test_duration'] = test_duration
            
            print(f"✅ Tests passed in {test_duration:.1f}s")
            return True
        except subprocess.CalledProcessError as e:
            print(f"❌ Tests failed: {e}")
            return False
    
    def package(self, preset: str) -> bool:
        """Create optimized package"""
        print(f"📦 Creating package with preset: {preset}")
        
        cmd = ['cmake', '--build', '--preset', preset, '--target', 'package']
        
        try:
            result = subprocess.run(cmd, cwd=self.source_dir, check=True)
            print("✅ Package created successfully")
            return True
        except subprocess.CalledProcessError as e:
            print(f"❌ Package creation failed: {e}")
            return False
    
    def generate_performance_report(self) -> Dict:
        """Generate comprehensive performance report"""
        total_duration = time.time() - self.start_time
        
        report = {
            'timestamp': int(time.time()),
            'total_duration': total_duration,
            'build_stats': self.build_stats,
            'system_info': self.detect_system_capabilities(),
        }
        
        # Save report to file
        report_file = self.build_dir / 'performance_report.json'
        with open(report_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"📊 Performance report saved to: {report_file}")
        return report
    
    def cleanup_build_artifacts(self, keep_essentials: bool = True):
        """Clean up build artifacts to save space"""
        print("🧹 Cleaning up build artifacts...")
        
        cleanup_patterns = [
            '**/*.tmp',
            '**/*.log',
            '**/CMakeFiles/**/*.o',
            '**/CMakeFiles/**/*.obj',
            '**/.ninja_*',
        ]
        
        if not keep_essentials:
            cleanup_patterns.extend([
                '**/CMakeFiles',
                '**/Testing',
                '**/_deps',
            ])
        
        cleaned_count = 0
        for pattern in cleanup_patterns:
            for file_path in self.build_dir.glob(pattern):
                try:
                    if file_path.is_file():
                        file_path.unlink()
                        cleaned_count += 1
                    elif file_path.is_dir():
                        shutil.rmtree(file_path)
                        cleaned_count += 1
                except Exception as e:
                    print(f"Warning: Could not clean {file_path}: {e}")
        
        print(f"✅ Cleaned {cleaned_count} build artifacts")

def main():
    parser = argparse.ArgumentParser(description='Hydrogen Optimized Build Script')
    parser.add_argument('--preset', default='release', help='CMake preset to use')
    parser.add_argument('--configure-only', action='store_true', help='Only configure, do not build')
    parser.add_argument('--build-only', action='store_true', help='Only build, do not configure')
    parser.add_argument('--test', action='store_true', help='Run tests after building')
    parser.add_argument('--test-types', nargs='+', help='Test types to run (unit, integration, performance)')
    parser.add_argument('--package', action='store_true', help='Create package after building')
    parser.add_argument('--targets', nargs='+', help='Specific targets to build')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    parser.add_argument('--cleanup', action='store_true', help='Clean up build artifacts after completion')
    parser.add_argument('--source-dir', type=Path, default=Path.cwd(), help='Source directory')
    parser.add_argument('--build-dir', type=Path, help='Build directory (auto-detected if not specified)')
    
    args = parser.parse_args()
    
    # Determine build directory
    if args.build_dir:
        build_dir = args.build_dir
    else:
        build_dir = args.source_dir / 'build' / args.preset
    
    print(f"🚀 Hydrogen Optimized Build System")
    print(f"Source: {args.source_dir}")
    print(f"Build: {build_dir}")
    print(f"Preset: {args.preset}")
    print()
    
    builder = HydrogenBuilder(args.source_dir, build_dir)
    
    success = True
    
    # Configuration phase
    if not args.build_only:
        success = builder.configure_build(args.preset)
        if not success:
            sys.exit(1)
    
    if args.configure_only:
        print("✅ Configuration completed")
        sys.exit(0)
    
    # Build phase
    if success:
        success = builder.build(args.preset, args.targets, args.verbose)
    
    # Test phase
    if success and args.test:
        success = builder.test(args.preset, args.test_types)
    
    # Package phase
    if success and args.package:
        success = builder.package(args.preset)
    
    # Generate performance report
    report = builder.generate_performance_report()
    
    # Cleanup if requested
    if args.cleanup:
        builder.cleanup_build_artifacts()
    
    # Final status
    if success:
        total_time = time.time() - builder.start_time
        print(f"🎉 Build completed successfully in {total_time:.1f}s")
        sys.exit(0)
    else:
        print("❌ Build failed")
        sys.exit(1)

if __name__ == '__main__':
    main()

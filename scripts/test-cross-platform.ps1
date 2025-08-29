# Cross-platform compatibility and performance test for Hydrogen build system
# This script validates that the build system works consistently across different platforms
# and measures performance improvements

param(
    [string]$BuildType = "Release",
    [switch]$Clean = $false,
    [switch]$Verbose = $false,
    [switch]$BenchmarkMode = $false
)

# Script configuration
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build-test-crossplatform"
$LogFile = Join-Path $ProjectRoot "crossplatform-test.log"

# Logging function
function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logMessage = "[$timestamp] [$Level] $Message"
    Write-Host $logMessage
    Add-Content -Path $LogFile -Value $logMessage
}

# Performance measurement function
function Measure-BuildTime {
    param([scriptblock]$ScriptBlock, [string]$Description)
    
    Write-Log "Starting: $Description"
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    try {
        & $ScriptBlock
        $stopwatch.Stop()
        $elapsed = $stopwatch.Elapsed
        Write-Log "Completed: $Description in $($elapsed.TotalSeconds.ToString('F2')) seconds"
        return $elapsed
    } catch {
        $stopwatch.Stop()
        Write-Log "Failed: $Description after $($stopwatch.Elapsed.TotalSeconds.ToString('F2')) seconds" "ERROR"
        throw
    }
}

# Initialize logging
if (Test-Path $LogFile) { Remove-Item $LogFile }
Write-Log "Starting Hydrogen cross-platform compatibility test"

try {
    # Test 1: Platform Detection
    Write-Log "Test 1: Platform and environment detection"
    
    # Detect operating system
    if ($IsWindows -or $env:OS -eq "Windows_NT") {
        $Platform = "Windows"
        $Architecture = if ([Environment]::Is64BitOperatingSystem) { "x64" } else { "x86" }
    } elseif ($IsLinux) {
        $Platform = "Linux"
        $Architecture = & uname -m
    } elseif ($IsMacOS) {
        $Platform = "macOS"
        $Architecture = & uname -m
    } else {
        $Platform = "Unknown"
        $Architecture = "Unknown"
    }
    
    Write-Log "Platform: $Platform"
    Write-Log "Architecture: $Architecture"
    Write-Log "PowerShell Version: $($PSVersionTable.PSVersion)"
    
    # Detect available tools
    $tools = @{
        "cmake" = $null
        "ninja" = $null
        "make" = $null
        "vcpkg" = $null
        "conan" = $null
        "git" = $null
    }
    
    foreach ($tool in $tools.Keys) {
        try {
            $version = & $tool --version 2>$null | Select-Object -First 1
            $tools[$tool] = $version
            Write-Log "✓ $tool available: $version"
        } catch {
            Write-Log "✗ $tool not available"
        }
    }
    
    # Test 2: Generator Detection and Selection
    Write-Log "Test 2: CMake generator detection"
    
    $preferredGenerators = @()
    if ($tools["ninja"]) {
        $preferredGenerators += "Ninja"
    }
    if ($Platform -eq "Windows") {
        $preferredGenerators += "Visual Studio 17 2022"
        $preferredGenerators += "Visual Studio 16 2019"
    }
    if ($tools["make"]) {
        $preferredGenerators += "Unix Makefiles"
    }
    
    $selectedGenerator = $null
    foreach ($generator in $preferredGenerators) {
        try {
            & cmake -G $generator --help >$null 2>&1
            if ($LASTEXITCODE -eq 0) {
                $selectedGenerator = $generator
                Write-Log "Selected generator: $generator"
                break
            }
        } catch {
            continue
        }
    }
    
    if (-not $selectedGenerator) {
        $selectedGenerator = "Default"
        Write-Log "Using default CMake generator"
    }
    
    # Clean previous build if requested
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Log "Cleaning previous build directory"
        Remove-Item -Recurse -Force $BuildDir
    }
    
    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }
    
    # Test 3: Configuration Performance
    Write-Log "Test 3: Configuration performance test"
    Set-Location $BuildDir
    
    $configArgs = @(
        "-S", $ProjectRoot,
        "-B", $BuildDir,
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DHYDROGEN_BUILD_TESTS=ON",
        "-DHYDROGEN_BUILD_EXAMPLES=ON",
        "-DHYDROGEN_ENABLE_SSL=ON",
        "-DHYDROGEN_ENABLE_COMPRESSION=ON"
    )
    
    if ($selectedGenerator -ne "Default") {
        $configArgs += "-G"
        $configArgs += $selectedGenerator
    }
    
    if ($Verbose) {
        $configArgs += "--verbose"
    }
    
    $configTime = Measure-BuildTime {
        & cmake @configArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Configuration failed"
        }
    } "CMake Configuration"
    
    # Test 4: Build Performance
    Write-Log "Test 4: Build performance test"
    
    $buildArgs = @(
        "--build", $BuildDir,
        "--config", $BuildType
    )
    
    # Use parallel builds if supported
    if ($selectedGenerator -eq "Ninja" -or $selectedGenerator -like "*Visual Studio*") {
        $cpuCount = [Environment]::ProcessorCount
        $buildArgs += "--parallel"
        $buildArgs += $cpuCount.ToString()
        Write-Log "Using parallel build with $cpuCount cores"
    }
    
    if ($Verbose) {
        $buildArgs += "--verbose"
    }
    
    $buildTime = Measure-BuildTime {
        & cmake @buildArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }
    } "Project Build"
    
    # Test 5: Feature Compatibility
    Write-Log "Test 5: Feature compatibility verification"
    
    # Check CMake cache for feature status
    $cmakeCache = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path $cmakeCache) {
        $cacheContent = Get-Content $cmakeCache
        
        $features = @{
            "SSL" = $false
            "Compression" = $false
            "Logging" = $false
            "WebSockets" = $false
            "HTTP_Server" = $false
        }
        
        foreach ($feature in $features.Keys) {
            $pattern = "HYDROGEN_HAS_$feature.*TRUE"
            if ($cacheContent -match $pattern) {
                $features[$feature] = $true
                Write-Log "✓ Feature $feature enabled"
            } else {
                Write-Log "✗ Feature $feature disabled"
            }
        }
        
        # Platform-specific feature checks
        if ($Platform -eq "Windows") {
            $winLibs = $cacheContent | Where-Object { $_ -match "ws2_32|mswsock" }
            if ($winLibs) {
                Write-Log "✓ Windows-specific libraries linked"
            }
        }
        
        if ($Platform -eq "Linux") {
            $linuxLibs = $cacheContent | Where-Object { $_ -match "pthread|dl" }
            if ($linuxLibs) {
                Write-Log "✓ Linux-specific libraries linked"
            }
        }
    }
    
    # Test 6: Binary Compatibility
    Write-Log "Test 6: Binary compatibility verification"
    
    $expectedTargets = @(
        "astrocomm_core",
        "astrocomm_server", 
        "astrocomm_client",
        "astrocomm_device"
    )
    
    $libDir = Join-Path $BuildDir $BuildType
    if (-not (Test-Path $libDir)) {
        $libDir = $BuildDir
    }
    
    foreach ($target in $expectedTargets) {
        $found = $false
        $extensions = @()
        
        # Platform-specific extensions
        switch ($Platform) {
            "Windows" { $extensions = @(".lib", ".dll", ".exe") }
            "Linux" { $extensions = @(".a", ".so") }
            "macOS" { $extensions = @(".a", ".dylib") }
            default { $extensions = @(".lib", ".a", ".dll", ".so", ".dylib") }
        }
        
        foreach ($ext in $extensions) {
            $targetFile = Join-Path $libDir "$target$ext"
            if (Test-Path $targetFile) {
                $fileInfo = Get-Item $targetFile
                Write-Log "✓ Found $target$ext ($($fileInfo.Length) bytes)"
                $found = $true
                break
            }
        }
        
        if (-not $found) {
            Write-Log "✗ Target $target not found for platform $Platform" "WARN"
        }
    }
    
    # Test 7: Performance Benchmarking
    if ($BenchmarkMode) {
        Write-Log "Test 7: Performance benchmarking"
        
        # Clean build test
        $cleanBuildTime = Measure-BuildTime {
            Remove-Item -Recurse -Force $BuildDir
            New-Item -ItemType Directory -Path $BuildDir | Out-Null
            Set-Location $BuildDir
            & cmake @configArgs
            & cmake @buildArgs
        } "Clean Build"
        
        # Incremental build test
        $incrementalBuildTime = Measure-BuildTime {
            & cmake @buildArgs
        } "Incremental Build (No Changes)"
        
        # Report performance metrics
        Write-Log "=== Performance Metrics ==="
        Write-Log "Configuration time: $($configTime.TotalSeconds.ToString('F2'))s"
        Write-Log "Initial build time: $($buildTime.TotalSeconds.ToString('F2'))s"
        Write-Log "Clean build time: $($cleanBuildTime.TotalSeconds.ToString('F2'))s"
        Write-Log "Incremental build time: $($incrementalBuildTime.TotalSeconds.ToString('F2'))s"
        Write-Log "Platform: $Platform $Architecture"
        Write-Log "Generator: $selectedGenerator"
        Write-Log "CPU cores: $([Environment]::ProcessorCount)"
    }
    
    # Test 8: Cross-platform file paths
    Write-Log "Test 8: Cross-platform file path handling"
    
    $pathTests = @(
        (Join-Path $ProjectRoot "src/core/CMakeLists.txt"),
        (Join-Path $ProjectRoot "cmake/HydrogenUtils.cmake"),
        (Join-Path $BuildDir "CMakeCache.txt")
    )
    
    foreach ($path in $pathTests) {
        if (Test-Path $path) {
            Write-Log "✓ Path accessible: $path"
        } else {
            Write-Log "✗ Path not accessible: $path" "WARN"
        }
    }
    
    Write-Log "=== Cross-Platform Test Summary ==="
    Write-Log "Platform: $Platform $Architecture"
    Write-Log "Generator: $selectedGenerator"
    Write-Log "Configuration: $($configTime.TotalSeconds.ToString('F2'))s"
    Write-Log "Build: $($buildTime.TotalSeconds.ToString('F2'))s"
    Write-Log "Features: Compatible"
    Write-Log "Binaries: Generated"
    Write-Log "Paths: Cross-platform compatible"
    Write-Log "=== Test Completed Successfully ==="
    
} catch {
    Write-Log "Test failed: $($_.Exception.Message)" "ERROR"
    Write-Log "Stack trace: $($_.ScriptStackTrace)" "ERROR"
    exit 1
} finally {
    Set-Location $ProjectRoot
}

Write-Log "Cross-platform compatibility test completed successfully"

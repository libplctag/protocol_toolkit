# Protocol Toolkit - Build All Configurations Script (Windows PowerShell)
# This script builds and tests the Protocol Toolkit with various configurations
# to match what the CI/CD pipeline does.

param(
    [switch]$Clean = $false,
    [switch]$StaticOnly = $false,
    [switch]$Fast = $false,
    [switch]$All = $true,
    [switch]$TestsOnly = $false,
    [switch]$Help = $false
)

# Configuration
$BuildDir = "build"
$OriginalDir = Get-Location
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$ProjectRoot = Split-Path -Parent $ScriptDir

# Function to print colored output
function Write-Status {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Blue
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

# Function to display usage
function Show-Usage {
    Write-Host "Usage: .\build_all_configs.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Help           Show this help message"
    Write-Host "  -Clean          Clean build directory before building"
    Write-Host "  -StaticOnly     Run only static analysis"
    Write-Host "  -Fast           Run only baseline configuration (no sanitizers)"
    Write-Host "  -All            Run all configurations (default)"
    Write-Host "  -TestsOnly      Run tests on existing builds"
    Write-Host ""
    Write-Host "Configurations built:"
    Write-Host "  - baseline:     Standard build with static analysis"
    Write-Host "  - asan:         AddressSanitizer"
    Write-Host "  - ubsan:        UndefinedBehaviorSanitizer"
    Write-Host "  - static:       Static linking"
    Write-Host ""
    Write-Host "Note: MemorySanitizer and ThreadSanitizer are not available on Windows"
}

# Function to clean build directory
function Clean-Build {
    Write-Status "Cleaning build directory..."
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

# Function to build with specific configuration
function Build-Config {
    param(
        [string]$ConfigName,
        [string]$CMakeArgs,
        [bool]$RunTests
    )
    
    Write-Status "Building configuration: $ConfigName"
    
    # Create build subdirectory
    $BuildSubdir = Join-Path $BuildDir $ConfigName
    New-Item -ItemType Directory -Path $BuildSubdir -Force | Out-Null
    Set-Location $BuildSubdir
    
    try {
        # Configure
        Write-Status "Configuring $ConfigName..."
        $ConfigureArgs = @(
            $ProjectRoot,
            "-DCMAKE_BUILD_TYPE=Release",
            "-DPTK_BUILD_TESTS=ON",
            "-DPTK_ENABLE_STATIC_ANALYSIS=ON"
        )
        
        if ($CMakeArgs) {
            $ConfigureArgs += $CMakeArgs.Split(' ')
        }
        
        & cmake @ConfigureArgs
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Configuration failed for $ConfigName"
            return $false
        }
        
        # Build
        Write-Status "Building $ConfigName..."
        & cmake --build . --config Release --parallel
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed for $ConfigName"
            return $false
        }
        
        # Run tests if requested
        if ($RunTests) {
            Write-Status "Running tests for $ConfigName..."
            
            # Run test harness
            $TestHarness = Join-Path "bin" "Release" "test_harness.exe"
            if (Test-Path $TestHarness) {
                & $TestHarness
                if ($LASTEXITCODE -ne 0) {
                    Write-Error "Test harness failed for $ConfigName"
                    return $false
                }
            } else {
                Write-Warning "Test harness not found for $ConfigName"
            }
            
            # Run CTest
            & ctest --output-on-failure --parallel $env:NUMBER_OF_PROCESSORS --config Release
            if ($LASTEXITCODE -ne 0) {
                Write-Error "CTest failed for $ConfigName"
                return $false
            }
        }
        
        Write-Success "Configuration $ConfigName completed successfully"
        return $true
        
    } finally {
        Set-Location $OriginalDir
    }
}

# Function to run static analysis
function Run-StaticAnalysis {
    Write-Status "Running static analysis..."
    
    $BaselineDir = Join-Path $BuildDir "baseline"
    if (-not (Test-Path $BaselineDir)) {
        Write-Error "Baseline build not found. Run baseline build first."
        return
    }
    
    Set-Location $BaselineDir
    
    try {
        # Run cppcheck
        if (Get-Command cppcheck -ErrorAction SilentlyContinue) {
            Write-Status "Running cppcheck..."
            try {
                & cmake --build . --target cppcheck
                Write-Success "Cppcheck completed"
            } catch {
                Write-Warning "Cppcheck had issues (non-fatal)"
            }
        } else {
            Write-Warning "Cppcheck not found, skipping"
        }
        
        # Note: scan-build is not typically available on Windows
        Write-Warning "Clang Static Analyzer (scan-build) not available on Windows"
        
    } finally {
        Set-Location $OriginalDir
    }
}

# Function to check system requirements
function Test-Requirements {
    Write-Status "Checking system requirements..."
    
    # Check for required tools
    $MissingTools = @()
    
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        $MissingTools += "cmake"
    }
    
    if (-not (Get-Command msbuild -ErrorAction SilentlyContinue)) {
        $MissingTools += "msbuild (Visual Studio Build Tools)"
    }
    
    if ($MissingTools.Count -gt 0) {
        Write-Error "Missing required tools: $($MissingTools -join ', ')"
        Write-Error "Please install the missing tools and try again"
        exit 1
    }
    
    # Check for optional tools
    if (-not (Get-Command cppcheck -ErrorAction SilentlyContinue)) {
        Write-Warning "cppcheck not found - static analysis will be limited"
        Write-Warning "Install cppcheck from https://cppcheck.sourceforge.io/"
    }
    
    Write-Success "System requirements check completed"
}

# Main execution
function Main {
    Set-Location $ProjectRoot
    
    Write-Status "Protocol Toolkit Build Script (Windows)"
    Write-Status "Project root: $ProjectRoot"
    
    # Show help if requested
    if ($Help) {
        Show-Usage
        exit 0
    }
    
    # Check requirements
    Test-Requirements
    
    # Clean if requested
    if ($Clean) {
        Clean-Build
    }
    
    # Create build directory if it doesn't exist
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }
    
    # Handle tests-only mode
    if ($TestsOnly) {
        Write-Status "Running tests on existing builds..."
        
        $Configs = @("baseline", "asan", "ubsan", "static")
        foreach ($Config in $Configs) {
            $ConfigPath = Join-Path $BuildDir $Config
            $TestHarness = Join-Path $ConfigPath "bin" "Release" "test_harness.exe"
            
            if ((Test-Path $ConfigPath) -and (Test-Path $TestHarness)) {
                Write-Status "Running tests for $Config..."
                Set-Location $ConfigPath
                try {
                    & $TestHarness
                    if ($LASTEXITCODE -eq 0) {
                        Write-Success "Tests passed for $Config"
                    } else {
                        Write-Error "Tests failed for $Config"
                    }
                } finally {
                    Set-Location $OriginalDir
                }
            }
        }
        exit 0
    }
    
    # Handle static analysis only
    if ($StaticOnly) {
        # Build baseline for static analysis if it doesn't exist
        $BaselineDir = Join-Path $BuildDir "baseline"
        if (-not (Test-Path $BaselineDir)) {
            Build-Config "baseline" "" $false | Out-Null
        }
        Run-StaticAnalysis
        exit 0
    }
    
    # Build configurations
    $BuildFailed = $false
    
    if ($Fast) {
        # Fast mode - only baseline
        $BuildFailed = -not (Build-Config "baseline" "" $true)
        if (-not $BuildFailed) {
            Run-StaticAnalysis
        }
        
    } elseif ($All) {
        # All configurations
        
        # Baseline build
        $BuildFailed = -not (Build-Config "baseline" "" $true)
        
        # Sanitizer builds (only if baseline succeeded and on Windows)
        if (-not $BuildFailed) {
            # AddressSanitizer (limited support on Windows)
            if (-not (Build-Config "asan" "-DPTK_ENABLE_ASAN=ON" $true)) {
                $BuildFailed = $true
            }
            
            # UndefinedBehaviorSanitizer
            if (-not (Build-Config "ubsan" "-DPTK_ENABLE_UBSAN=ON" $true)) {
                $BuildFailed = $true
            }
            
            # Static build
            if (-not (Build-Config "static" "-DPTK_STATIC_BUILD=ON" $true)) {
                $BuildFailed = $true
            }
            
            Write-Warning "MemorySanitizer and ThreadSanitizer are not available on Windows"
        }
        
        # Run static analysis
        if (-not $BuildFailed) {
            Run-StaticAnalysis
        }
    }
    
    # Summary
    Write-Status "Build summary:"
    if ($BuildFailed) {
        Write-Error "Some builds failed. Check the output above for details."
        exit 1
    } else {
        Write-Success "All builds completed successfully!"
        
        # Show build artifacts
        Write-Status "Build artifacts:"
        Get-ChildItem -Path $BuildDir -Recurse -Include "test_harness.exe", "*.lib", "*.dll" | 
            Select-Object -First 10 | 
            ForEach-Object { Write-Host "  $($_.FullName)" }
        
        Write-Status "To run individual tests:"
        Write-Host "  .\$BuildDir\baseline\bin\Release\test_harness.exe"
        Write-Host "  .\$BuildDir\asan\bin\Release\test_harness.exe"
        Write-Host "  .\$BuildDir\ubsan\bin\Release\test_harness.exe"
        Write-Host "  etc..."
        
        exit 0
    }
}

# Run main function
Main
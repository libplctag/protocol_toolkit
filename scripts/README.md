# Protocol Toolkit Build Scripts

This directory contains scripts to build and test the Protocol Toolkit with various configurations, matching what the CI/CD pipeline does.

## Available Scripts

### `build_all_configs.sh` (Linux/macOS)
Bash script for building all configurations on Linux and macOS systems.

### `build_all_configs.ps1` (Windows)
PowerShell script for building all configurations on Windows systems.

## Quick Start

### Linux/macOS
```bash
# Make executable and run all configurations
chmod +x scripts/build_all_configs.sh
./scripts/build_all_configs.sh

# Fast mode (baseline only)
./scripts/build_all_configs.sh --fast

# Clean build
./scripts/build_all_configs.sh --clean
```

### Windows
```powershell
# Run all configurations
.\scripts\build_all_configs.ps1

# Fast mode (baseline only)
.\scripts\build_all_configs.ps1 -Fast

# Clean build
.\scripts\build_all_configs.ps1 -Clean
```

## Build Configurations

The scripts build the following configurations:

| Configuration | Description | Platforms |
|---------------|-------------|-----------|
| **baseline** | Standard build with static analysis | All |
| **asan** | AddressSanitizer (memory error detection) | All |
| **msan** | MemorySanitizer (uninitialized memory detection) | Linux/macOS (Clang only) |
| **ubsan** | UndefinedBehaviorSanitizer (undefined behavior detection) | All |
| **tsan** | ThreadSanitizer (data race detection) | Linux/macOS |
| **static** | Static linking build | All |

## Command Line Options

### Linux/macOS (`build_all_configs.sh`)
```bash
Usage: ./build_all_configs.sh [OPTIONS]

Options:
  -h, --help          Show help message
  -c, --clean         Clean build directory before building
  -s, --static-only   Run only static analysis
  -f, --fast          Run only baseline configuration (no sanitizers)
  -a, --all           Run all configurations (default)
  -t, --tests-only    Run tests on existing builds
```

### Windows (`build_all_configs.ps1`)
```powershell
Usage: .\build_all_configs.ps1 [OPTIONS]

Options:
  -Help               Show help message
  -Clean              Clean build directory before building
  -StaticOnly         Run only static analysis
  -Fast               Run only baseline configuration (no sanitizers)
  -All                Run all configurations (default)
  -TestsOnly          Run tests on existing builds
```

## Examples

### Development Workflow
```bash
# Quick test during development
./scripts/build_all_configs.sh --fast

# Full validation before commit
./scripts/build_all_configs.sh --clean --all

# Run tests on existing builds
./scripts/build_all_configs.sh --tests-only
```

### Static Analysis Only
```bash
# Run static analysis without rebuilding
./scripts/build_all_configs.sh --static-only
```

### CI/CD Simulation
```bash
# Simulate what CI/CD does
./scripts/build_all_configs.sh --clean --all
```

## Prerequisites

### Linux/macOS
- **Required:**
  - `cmake` (≥ 3.18)
  - `make` or `ninja`
  - `gcc` or `clang`
  - C11 compiler support

- **Optional (for full features):**
  - `cppcheck` - Static analysis
  - `scan-build` (clang-tools) - Clang static analyzer
  - `valgrind` - Memory debugging

### Windows
- **Required:**
  - `cmake` (≥ 3.18)
  - Visual Studio Build Tools or Visual Studio
  - MSVC compiler

- **Optional:**
  - `cppcheck` - Static analysis
  - Clang (for better sanitizer support)

## Installation Commands

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    clang \
    clang-tools \
    cppcheck \
    valgrind
```

### macOS
```bash
brew install cmake ninja cppcheck llvm
```

### Windows
```powershell
# Install via chocolatey
choco install cmake cppcheck

# Or download from:
# - CMake: https://cmake.org/download/
# - Cppcheck: https://cppcheck.sourceforge.io/
# - Visual Studio: https://visualstudio.microsoft.com/
```

## Output

The scripts create a `build/` directory with subdirectories for each configuration:

```
build/
├── baseline/           # Standard build
├── asan/              # AddressSanitizer build
├── msan/              # MemorySanitizer build (Linux/macOS only)
├── ubsan/             # UndefinedBehaviorSanitizer build
├── tsan/              # ThreadSanitizer build (Linux/macOS only)
└── static/            # Static linking build
```

Each subdirectory contains:
- `bin/test_harness` - Main test executable
- `lib/` - Built libraries
- Static analysis reports (when enabled)

## Test Execution

Tests are automatically run after each build. You can also run tests manually:

```bash
# Run specific configuration
./build/baseline/bin/test_harness

# Run all tests in a configuration
cd build/baseline
ctest --output-on-failure
```

## Integration with CI/CD

These scripts match the GitHub Actions workflow (`.github/workflows/ci.yml`) configurations:

- Same CMake flags
- Same sanitizer combinations
- Same test execution pattern
- Same static analysis tools

## Troubleshooting

### Common Issues

1. **Missing tools**: Install prerequisites listed above
2. **Permission errors**: Make sure script is executable (`chmod +x`)
3. **Build failures**: Check compiler and CMake versions
4. **Test failures**: Check that all dependencies are installed

### Platform-Specific Notes

- **MemorySanitizer**: Only works with Clang on Linux/macOS
- **ThreadSanitizer**: Not available on Windows
- **Static analysis**: Limited on Windows (cppcheck only)

### Getting Help

```bash
# Show help
./scripts/build_all_configs.sh --help

# Check system requirements
./scripts/build_all_configs.sh --fast  # Will check and warn about missing tools
```

## Advanced Usage

### Custom Build Types
```bash
# Debug build
BUILD_TYPE=Debug ./scripts/build_all_configs.sh

# RelWithDebInfo build
BUILD_TYPE=RelWithDebInfo ./scripts/build_all_configs.sh
```

### Parallel Builds
The scripts automatically detect the number of CPU cores and use parallel builds. You can override this:

```bash
# Force specific number of parallel jobs
CMAKE_BUILD_PARALLEL_LEVEL=8 ./scripts/build_all_configs.sh
```

### Environment Variables
- `BUILD_TYPE`: CMake build type (Debug, Release, RelWithDebInfo)
- `CMAKE_BUILD_PARALLEL_LEVEL`: Number of parallel build jobs
- `CMAKE_GENERATOR`: CMake generator (Unix Makefiles, Ninja, etc.)

## Contributing

When adding new build configurations:
1. Update both scripts (bash and PowerShell)
2. Update this README
3. Update the CI/CD workflow (`.github/workflows/ci.yml`)
4. Test on multiple platforms
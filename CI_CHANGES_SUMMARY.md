# GitHub CI/CD Pipeline Changes Summary

## Overview
Updated the GitHub Actions workflow to provide comprehensive testing across multiple platforms, architectures, and testing methodologies as requested.

## Key Changes Made

### 1. **Trigger Changes**
- **Before**: Triggered on `prerelease` branch
- **After**: Triggered on `dev` branch for all check-ins (push and pull requests)

### 2. **Ubuntu LTS Version**
- **Before**: Ubuntu 22.04 LTS
- **After**: Ubuntu 20.04 LTS (one LTS version back from current 22.04)

### 3. **Architecture Support Expansion**

#### Ubuntu Latest (24.04)
- **Before**: x86_64, aarch64
- **After**: x86_64, x86 (32-bit), aarch64

#### Ubuntu LTS (20.04)
- **Before**: x86_64, aarch64
- **After**: x86_64, x86 (32-bit), armv6, armv7hf, aarch64

#### Windows
- **Before**: x64, x86, ARM64
- **After**: x64, x86 (32-bit), ARM64 (no changes - already correct)

#### macOS
- **Before**: x86_64, arm64
- **After**: x86_64, arm64 (no changes - already correct)

### 4. **New Parallel Testing Jobs**

#### Valgrind Testing Job
- **Platform**: Ubuntu Latest
- **Architectures**: x86_64, x86 (32-bit)
- **Test Types**: Sequential, Parallel
- **Features**:
  - Full leak detection with origin tracking
  - Custom suppression file for false positives
  - Parallel execution of individual test executables

#### Sanitizer Stress Testing Job
- **Platform**: Ubuntu Latest
- **Sanitizers**: ASAN, MSAN, UBSAN, TSAN
- **Test Types**: Sequential, Parallel
- **Features**:
  - Dedicated sanitizer environment variables
  - Parallel execution of comprehensive tests
  - Artifact upload for failure analysis
  - MSAN excluded from parallel testing (performance)

### 5. **Cross-Compilation Support**

#### ARM6 (armv6)
- **Compiler**: arm-linux-gnueabi-gcc
- **Tests**: Cross-compilation only (no native execution)
- **Sanitizers**: None (limited support)

#### ARM7 Hard Float (armv7hf)
- **Compiler**: arm-linux-gnueabihf-gcc
- **Tests**: Cross-compilation only (no native execution)
- **Sanitizers**: None (limited support)

#### 32-bit x86
- **Compiler**: clang with -m32 flag
- **Tests**: Native execution supported
- **Sanitizers**: ASAN, UBSAN (MSAN excluded)

### 6. **Test Execution Strategy**

#### Native vs Cross-Compilation
- **Native execution**: x86_64, x86 (32-bit) on Ubuntu/Windows, both architectures on macOS
- **Cross-compilation only**: ARM6, ARM7hf, ARM64 on Ubuntu/Windows

#### Parallel Testing
- **Sequential**: Single test harness execution
- **Parallel**: Individual test executables run concurrently using `xargs -P $(nproc)`

### 7. **Enhanced Sanitizer Configuration**

#### Environment Variables
- **ASAN**: `detect_leaks=1:abort_on_error=1`
- **MSAN**: `abort_on_error=1`
- **UBSAN**: `abort_on_error=1`
- **TSAN**: `abort_on_error=1`

#### Exclusion Matrix
- **MSAN**: Only on x86_64 (Clang requirement)
- **ARM6/ARM7hf**: No sanitizers (limited support)
- **ARM64**: No MSAN (architecture limitation)

### 8. **Artifact Management**

#### Valgrind Suppressions
- Created `scripts/valgrind.supp` for false positive suppression
- Covers glibc, pthread, OpenSSL, and PTK-specific suppressions

#### Failure Artifacts
- Sanitizer logs uploaded on failure
- Testing directory artifacts preserved
- 7-day retention for debugging

### 9. **Job Dependencies**
- Updated test summary to include new parallel testing jobs
- Proper dependency chain for artifact collection
- Always-run policy for summary generation

## Job Matrix Summary

| Job Name | Platform | Architectures | Sanitizers | Test Types |
|----------|----------|---------------|------------|------------|
| ubuntu-latest | Ubuntu 24.04 | x86_64, x86, aarch64 | ASAN, MSAN, UBSAN, TSAN | Standard |
| ubuntu-lts | Ubuntu 20.04 | x86_64, x86, armv6, armv7hf, aarch64 | ASAN, UBSAN | Standard |
| windows | Windows Latest | x64, x86, ARM64 | ASAN | Standard |
| macos | macOS Latest | x86_64, arm64 | ASAN, UBSAN | Standard |
| valgrind-testing | Ubuntu Latest | x86_64, x86 | None | Sequential, Parallel |
| sanitizer-stress-testing | Ubuntu Latest | x86_64 | ASAN, MSAN, UBSAN, TSAN | Sequential, Parallel |

## Total Test Matrix Size

### Before Changes
- **Jobs**: 4 main jobs
- **Configurations**: ~25 total combinations
- **Architectures**: 6 architectures across all platforms

### After Changes
- **Jobs**: 6 main jobs
- **Configurations**: ~45 total combinations
- **Architectures**: 9 architectures across all platforms
- **Test Types**: Standard + Sequential + Parallel

## Performance Impact

### Parallel Execution
- Individual test executables run concurrently
- Utilizes all available CPU cores (`$(nproc)`)
- Reduced total execution time for comprehensive testing

### Resource Optimization
- Cross-compilation builds without test execution
- Sanitizer exclusions for unsupported architectures
- Artifact cleanup with appropriate retention

## Testing Coverage

### Architecture Coverage
- **x86_64**: Full support with all sanitizers and tools
- **x86 (32-bit)**: Ubuntu, Windows with ASAN/UBSAN + Valgrind
- **ARM64**: Cross-compilation on Ubuntu/Windows, native on macOS
- **ARMv6**: Ubuntu cross-compilation (embedded systems)
- **ARMv7 Hard Float**: Ubuntu cross-compilation (ARM boards)

### Tool Coverage
- **Sanitizers**: All major sanitizers across supported platforms
- **Valgrind**: Memory leak detection with origin tracking
- **Static Analysis**: Clang Static Analyzer + Cppcheck
- **Parallel Testing**: Stress testing with concurrent execution

## Files Modified

1. **`.github/workflows/ci.yml`** - Main CI/CD workflow
2. **`scripts/valgrind.supp`** - Valgrind suppressions (new)
3. **`CI_CHANGES_SUMMARY.md`** - This summary document (new)

## Usage

The updated pipeline automatically triggers on:
- Any push to the `dev` branch
- Any pull request targeting the `dev` branch

No manual intervention required - all configurations run automatically with comprehensive reporting in the GitHub Actions summary.

## Benefits

1. **Comprehensive Architecture Support**: Covers embedded (ARM6/7) to server (x86_64) architectures
2. **Enhanced Error Detection**: Multiple sanitizers + Valgrind for maximum coverage
3. **Parallel Testing**: Faster feedback through concurrent test execution
4. **Cross-Platform Validation**: Consistent behavior across Ubuntu, Windows, macOS
5. **Failure Analysis**: Detailed artifact collection for debugging
6. **Scalable Design**: Easy to add new architectures or sanitizers
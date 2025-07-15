#!/bin/bash

# Protocol Toolkit - Build All Configurations Script
# This script builds and tests the Protocol Toolkit with various configurations
# to match what the CI/CD pipeline does.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
ORIGINAL_DIR=$(pwd)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to clean build directory
clean_build() {
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
}

# Function to build with specific configuration
build_config() {
    local config_name="$1"
    local cmake_args="$2"
    local run_tests="$3"
    
    print_status "Building configuration: $config_name"
    
    # Create build subdirectory
    local build_subdir="${BUILD_DIR}/${config_name}"
    mkdir -p "$build_subdir"
    cd "$build_subdir"
    
    # Configure
    print_status "Configuring $config_name..."
    if ! cmake "$PROJECT_ROOT" \
        -DCMAKE_BUILD_TYPE=Release \
        -DPTK_BUILD_TESTS=ON \
        -DPTK_ENABLE_STATIC_ANALYSIS=ON \
        $cmake_args; then
        print_error "Configuration failed for $config_name"
        cd "$ORIGINAL_DIR"
        return 1
    fi
    
    # Build
    print_status "Building $config_name..."
    if ! cmake --build . --parallel $(nproc 2>/dev/null || echo 4); then
        print_error "Build failed for $config_name"
        cd "$ORIGINAL_DIR"
        return 1
    fi
    
    # Run tests if requested
    if [ "$run_tests" = "true" ]; then
        print_status "Running tests for $config_name..."
        
        # Run test harness
        if [ -f "bin/test_harness" ]; then
            if ! ./bin/test_harness; then
                print_error "Test harness failed for $config_name"
                cd "$ORIGINAL_DIR"
                return 1
            fi
        else
            print_warning "Test harness not found for $config_name"
        fi
        
        # Run CTest
        if ! ctest --output-on-failure --parallel $(nproc 2>/dev/null || echo 4); then
            print_error "CTest failed for $config_name"
            cd "$ORIGINAL_DIR"
            return 1
        fi
    fi
    
    cd "$ORIGINAL_DIR"
    print_success "Configuration $config_name completed successfully"
}

# Function to run static analysis
run_static_analysis() {
    print_status "Running static analysis..."
    
    cd "${BUILD_DIR}/baseline"
    
    # Run cppcheck
    if command -v cppcheck >/dev/null 2>&1; then
        print_status "Running cppcheck..."
        if make cppcheck 2>/dev/null; then
            print_success "Cppcheck completed"
        else
            print_warning "Cppcheck had issues (non-fatal)"
        fi
    else
        print_warning "Cppcheck not found, skipping"
    fi
    
    # Run scan-build
    if command -v scan-build >/dev/null 2>&1; then
        print_status "Running scan-build..."
        if make scan-build 2>/dev/null; then
            print_success "Scan-build completed"
        else
            print_warning "Scan-build had issues (non-fatal)"
        fi
    else
        print_warning "Scan-build not found, skipping"
    fi
    
    cd "$ORIGINAL_DIR"
}

# Function to check system requirements
check_requirements() {
    print_status "Checking system requirements..."
    
    # Check for required tools
    local missing_tools=()
    
    if ! command -v cmake >/dev/null 2>&1; then
        missing_tools+=("cmake")
    fi
    
    if ! command -v make >/dev/null 2>&1; then
        missing_tools+=("make")
    fi
    
    if ! command -v gcc >/dev/null 2>&1 && ! command -v clang >/dev/null 2>&1; then
        missing_tools+=("gcc or clang")
    fi
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        print_error "Missing required tools: ${missing_tools[*]}"
        print_error "Please install the missing tools and try again"
        exit 1
    fi
    
    # Check for optional tools
    if ! command -v cppcheck >/dev/null 2>&1; then
        print_warning "cppcheck not found - static analysis will be limited"
    fi
    
    if ! command -v scan-build >/dev/null 2>&1; then
        print_warning "scan-build not found - install clang-tools for full static analysis"
    fi
    
    if ! command -v valgrind >/dev/null 2>&1; then
        print_warning "valgrind not found - memory checking will be limited"
    fi
    
    print_success "System requirements check completed"
}

# Function to display usage
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -c, --clean         Clean build directory before building"
    echo "  -s, --static-only   Run only static analysis"
    echo "  -f, --fast          Run only baseline configuration (no sanitizers)"
    echo "  -a, --all           Run all configurations (default)"
    echo "  -t, --tests-only    Run tests on existing builds"
    echo ""
    echo "Configurations built:"
    echo "  - baseline:     Standard build with static analysis"
    echo "  - asan:         AddressSanitizer"
    echo "  - msan:         MemorySanitizer (Clang only)"
    echo "  - ubsan:        UndefinedBehaviorSanitizer"
    echo "  - tsan:         ThreadSanitizer"
    echo "  - static:       Static linking"
    echo ""
}

# Parse command line arguments
CLEAN=false
STATIC_ONLY=false
FAST=false
ALL=true
TESTS_ONLY=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -s|--static-only)
            STATIC_ONLY=true
            ALL=false
            shift
            ;;
        -f|--fast)
            FAST=true
            ALL=false
            shift
            ;;
        -a|--all)
            ALL=true
            shift
            ;;
        -t|--tests-only)
            TESTS_ONLY=true
            ALL=false
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Main execution
main() {
    cd "$PROJECT_ROOT"
    
    print_status "Protocol Toolkit Build Script"
    print_status "Project root: $PROJECT_ROOT"
    
    # Check requirements
    check_requirements
    
    # Clean if requested
    if [ "$CLEAN" = true ]; then
        clean_build
    fi
    
    # Create build directory if it doesn't exist
    mkdir -p "$BUILD_DIR"
    
    # Handle tests-only mode
    if [ "$TESTS_ONLY" = true ]; then
        print_status "Running tests on existing builds..."
        
        for config in baseline asan msan ubsan tsan static; do
            if [ -d "${BUILD_DIR}/${config}" ] && [ -f "${BUILD_DIR}/${config}/bin/test_harness" ]; then
                print_status "Running tests for $config..."
                cd "${BUILD_DIR}/${config}"
                if ./bin/test_harness; then
                    print_success "Tests passed for $config"
                else
                    print_error "Tests failed for $config"
                fi
                cd "$ORIGINAL_DIR"
            fi
        done
        
        exit 0
    fi
    
    # Handle static analysis only
    if [ "$STATIC_ONLY" = true ]; then
        # Build baseline for static analysis
        if [ ! -d "${BUILD_DIR}/baseline" ]; then
            build_config "baseline" "" false
        fi
        run_static_analysis
        exit 0
    fi
    
    # Build configurations
    local build_failed=false
    
    if [ "$FAST" = true ]; then
        # Fast mode - only baseline
        build_config "baseline" "" true || build_failed=true
        run_static_analysis
        
    elif [ "$ALL" = true ]; then
        # All configurations
        
        # Baseline build
        build_config "baseline" "" true || build_failed=true
        
        # Sanitizer builds (only if baseline succeeded)
        if [ "$build_failed" = false ]; then
            # AddressSanitizer
            if command -v clang >/dev/null 2>&1; then
                build_config "asan" "-DPTK_ENABLE_ASAN=ON -DCMAKE_C_COMPILER=clang" true || build_failed=true
            else
                build_config "asan" "-DPTK_ENABLE_ASAN=ON" true || build_failed=true
            fi
            
            # MemorySanitizer (Clang only)
            if command -v clang >/dev/null 2>&1; then
                build_config "msan" "-DPTK_ENABLE_MSAN=ON -DCMAKE_C_COMPILER=clang" true || build_failed=true
            else
                print_warning "Skipping MemorySanitizer (requires Clang)"
            fi
            
            # UndefinedBehaviorSanitizer
            build_config "ubsan" "-DPTK_ENABLE_UBSAN=ON" true || build_failed=true
            
            # ThreadSanitizer
            build_config "tsan" "-DPTK_ENABLE_TSAN=ON" true || build_failed=true
            
            # Static build
            build_config "static" "-DPTK_STATIC_BUILD=ON" true || build_failed=true
        fi
        
        # Run static analysis
        run_static_analysis
    fi
    
    # Summary
    print_status "Build summary:"
    if [ "$build_failed" = true ]; then
        print_error "Some builds failed. Check the output above for details."
        exit 1
    else
        print_success "All builds completed successfully!"
        
        # Show build artifacts
        print_status "Build artifacts:"
        find "$BUILD_DIR" -name "test_harness" -o -name "*.a" -o -name "*.so" | head -10
        
        print_status "To run individual tests:"
        echo "  ./${BUILD_DIR}/baseline/bin/test_harness"
        echo "  ./${BUILD_DIR}/asan/bin/test_harness"
        echo "  ./${BUILD_DIR}/ubsan/bin/test_harness"
        echo "  etc..."
        
        exit 0
    fi
}

# Run main function
main "$@"
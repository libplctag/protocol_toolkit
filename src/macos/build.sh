#!/bin/bash

# Build script for Protocol Toolkit macOS using CMake
set -e

# Configuration
BUILD_TYPE=${BUILD_TYPE:-Release}
BUILD_TESTS=${BUILD_TESTS:-ON}
INSTALL_PREFIX=${INSTALL_PREFIX:-/usr/local}
VERBOSE=${VERBOSE:-OFF}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Protocol Toolkit macOS Build Script${NC}"
echo -e "${GREEN}===================================${NC}"

# Check if we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo -e "${RED}Error: This build script is designed for macOS only${NC}"
    exit 1
fi

# Check for required tools
command -v cmake >/dev/null 2>&1 || {
    echo -e "${RED}Error: cmake is required but not installed${NC}"
    echo "Install with: brew install cmake"
    exit 1
}

# Create build directory
BUILD_DIR="build"
if [[ "$1" == "clean" ]]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}Clean complete${NC}"
    exit 0
fi

echo -e "${YELLOW}Creating build directory: $BUILD_DIR${NC}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DBUILD_TESTS="$BUILD_TESTS" \
    -DCMAKE_VERBOSE_MAKEFILE="$VERBOSE"

# Build
echo -e "${YELLOW}Building...${NC}"
cmake --build . --config "$BUILD_TYPE" --parallel $(sysctl -n hw.ncpu)

# Run tests if enabled
if [[ "$BUILD_TESTS" == "ON" ]]; then
    echo -e "${YELLOW}Running tests...${NC}"
    ctest --output-on-failure
fi

echo -e "${GREEN}Build complete!${NC}"
echo ""
echo "Built artifacts:"
echo "  Library: $BUILD_DIR/libprotocol_toolkit_macos.a"
echo "  Example: $BUILD_DIR/example_tcp_client"
if [[ "$BUILD_TESTS" == "ON" ]]; then
    echo "  Tests: $BUILD_DIR/tests/"
fi

echo ""
echo "To install, run:"
echo "  cd $BUILD_DIR && sudo cmake --install ."
echo ""
echo "To run the example:"
echo "  ./$BUILD_DIR/example_tcp_client"

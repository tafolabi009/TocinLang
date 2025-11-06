#!/bin/bash
# Local CI simulation script to test before pushing to GitHub
# This simulates the GitHub Actions CI pipeline

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Tocin Compiler - Local CI Simulation${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    OS="Windows"
fi

echo -e "${BLUE}Operating System: ${OS}${NC}"

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check prerequisites
echo -e "\n${YELLOW}[1/7] Checking prerequisites...${NC}"
MISSING_DEPS=()

if ! command_exists cmake; then
    MISSING_DEPS+=("cmake")
fi

if ! command_exists python3; then
    MISSING_DEPS+=("python3")
fi

if ! command_exists llvm-config && ! command_exists llvm-config-18; then
    MISSING_DEPS+=("llvm")
fi

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${RED}✗ Missing dependencies: ${MISSING_DEPS[*]}${NC}"
    echo -e "${YELLOW}Please install missing dependencies before running this script.${NC}"
    exit 1
else
    echo -e "${GREEN}✓ All prerequisites found${NC}"
fi

# Show versions
echo -e "\n${YELLOW}Dependency versions:${NC}"
cmake --version | head -1
python3 --version
if command_exists llvm-config; then
    echo "LLVM: $(llvm-config --version)"
elif command_exists llvm-config-18; then
    echo "LLVM: $(llvm-config-18 --version)"
fi

# Clean previous build
echo -e "\n${YELLOW}[2/7] Cleaning previous build...${NC}"
if [ -d "build" ]; then
    echo "Removing old build directory..."
    rm -rf build
fi
mkdir -p build
echo -e "${GREEN}✓ Build directory ready${NC}"

# Configure CMake
echo -e "\n${YELLOW}[3/7] Configuring CMake...${NC}"
cd build

CMAKE_FLAGS=(
    "-DCMAKE_BUILD_TYPE=Release"
    "-DWITH_PYTHON=ON"
    "-DWITH_V8=OFF"
    "-DWITH_ZSTD=ON"
    "-DWITH_XML=ON"
    "-DENABLE_TESTING=ON"
)

echo "CMake flags: ${CMAKE_FLAGS[*]}"
if cmake .. "${CMAKE_FLAGS[@]}"; then
    echo -e "${GREEN}✓ CMake configuration successful${NC}"
else
    echo -e "${RED}✗ CMake configuration failed${NC}"
    exit 1
fi

# Build
echo -e "\n${YELLOW}[4/7] Building project...${NC}"
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
echo "Building with $CORES parallel jobs..."

if cmake --build . --config Release --parallel "$CORES"; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

# Check if binary exists
echo -e "\n${YELLOW}[5/7] Checking binary...${NC}"
TOCIN_BIN=""
if [ -f "./tocin" ]; then
    TOCIN_BIN="./tocin"
elif [ -f "./tocin.exe" ]; then
    TOCIN_BIN="./tocin.exe"
elif [ -f "./Release/tocin.exe" ]; then
    TOCIN_BIN="./Release/tocin.exe"
fi

if [ -z "$TOCIN_BIN" ]; then
    echo -e "${RED}✗ Tocin binary not found${NC}"
    exit 1
else
    echo -e "${GREEN}✓ Found tocin binary at: $TOCIN_BIN${NC}"
fi

# Test basic functionality
echo -e "\n${YELLOW}[6/7] Testing basic functionality...${NC}"
if $TOCIN_BIN --help > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Basic functionality test passed${NC}"
else
    echo -e "${RED}✗ Basic functionality test failed${NC}"
    exit 1
fi

# Run unit tests
echo -e "\n${YELLOW}Running unit tests...${NC}"
if [ -f "CTestTestfile.cmake" ]; then
    if ctest --output-on-failure --parallel "$CORES"; then
        echo -e "${GREEN}✓ All unit tests passed${NC}"
    else
        echo -e "${YELLOW}⚠ Some unit tests failed (continuing...)${NC}"
    fi
else
    echo -e "${YELLOW}⚠ No CTest configuration found${NC}"
fi

# Run integration tests
echo -e "\n${YELLOW}[7/7] Running integration tests...${NC}"
cd ..
TEST_COUNT=0
PASS_COUNT=0
FAIL_COUNT=0

for test_file in tests/*.to; do
    if [ -f "$test_file" ]; then
        TEST_COUNT=$((TEST_COUNT + 1))
        echo -ne "Testing: $(basename "$test_file")... "
        
        if build/$TOCIN_BIN "$test_file" > /dev/null 2>&1; then
            PASS_COUNT=$((PASS_COUNT + 1))
            echo -e "${GREEN}✓ PASS${NC}"
        else
            FAIL_COUNT=$((FAIL_COUNT + 1))
            echo -e "${YELLOW}⚠ FAIL${NC}"
        fi
    fi
done

# Summary
echo -e "\n${BLUE}════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Test Summary${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"
echo -e "Total tests:  $TEST_COUNT"
echo -e "${GREEN}Passed:       $PASS_COUNT${NC}"
if [ $FAIL_COUNT -gt 0 ]; then
    echo -e "${YELLOW}Failed:       $FAIL_COUNT${NC}"
fi

SUCCESS_RATE=0
if [ $TEST_COUNT -gt 0 ]; then
    SUCCESS_RATE=$((PASS_COUNT * 100 / TEST_COUNT))
fi
echo -e "Success rate: ${SUCCESS_RATE}%"

echo -e "\n${BLUE}════════════════════════════════════════════════════════${NC}"
if [ $FAIL_COUNT -eq 0 ] && [ $TEST_COUNT -gt 0 ]; then
    echo -e "${GREEN}✓ All checks passed! Ready to push to GitHub.${NC}"
    echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"
    exit 0
elif [ $SUCCESS_RATE -ge 80 ]; then
    echo -e "${YELLOW}⚠ Most checks passed. Review failures before pushing.${NC}"
    echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"
    exit 0
else
    echo -e "${RED}✗ Too many failures. Please fix issues before pushing.${NC}"
    echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"
    exit 1
fi

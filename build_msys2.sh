#!/bin/bash

set -e

echo "Building Tocin Compiler with MSYS2 MinGW64..."
echo "Using MSYS2 installation at: D:/Downloads/msys64"
echo

# Clean any existing build files from root directory
echo "Cleaning existing build files..."
rm -f CMakeCache.txt Makefile CPackConfig.cmake CPackSourceConfig.cmake cmake_install.cmake cmake_uninstall.cmake
rm -rf CMakeFiles

# Clean and recreate build directory
echo "Cleaning build directory..."
rm -rf build
mkdir -p build
cd build

# Initialize log file path relative to current build dir
LOG_FILE=./build.log

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_COLOR_MAKEFILE=ON 2>&1 | tee "$LOG_FILE"

# Verify Makefile exists in build directory
if [ ! -f Makefile ]; then
	echo "Error: Makefile not found in build directory!" | tee -a "$LOG_FILE"
	echo "Checking if CMake generated files in parent directory..." | tee -a "$LOG_FILE]"
	if [ -f ../Makefile ]; then
		echo "Found Makefile in parent directory, using that instead..." | tee -a "$LOG_FILE"
		cd ..
		LOG_FILE=./build.log
	else
		echo "No Makefile found! CMake configuration may have failed." | tee -a "$LOG_FILE"
		exit 1
	fi
fi

# Build the project
echo "Building project..."
CORES=$(command -v nproc >/dev/null 2>&1 && nproc || echo 4)
mingw32-make --output-sync=target -j"$CORES" SHELL=sh 2>&1 | tee -a "$LOG_FILE"

echo "Build finished. Full log: $LOG_FILE"

# Test the compiler
echo "Testing compiler..." | tee -a "$LOG_FILE"
if [ -f tocin.exe ]; then
	echo "Compiler built successfully!" | tee -a "$LOG_FILE"
	./tocin.exe --help | sed 's/\r$//' | tee -a "$LOG_FILE"
	echo
	echo "Creating test file..." | tee -a "$LOG_FILE"
	echo 'print("Hello from Tocin!");' > test.to
	echo "Running test..." | tee -a "$LOG_FILE"
	./tocin.exe test.to --jit | sed 's/\r$//' | tee -a "$LOG_FILE"
elif [ -f ../tocin.exe ]; then
	echo "Compiler built successfully in parent directory!" | tee -a "$LOG_FILE"
	../tocin.exe --help | sed 's/\r$//' | tee -a "$LOG_FILE"
	echo
	echo "Creating test file..." | tee -a "$LOG_FILE"
	echo 'print("Hello from Tocin!");' > test.to
	echo "Running test..." | tee -a "$LOG_FILE"
	../tocin.exe test.to --jit | sed 's/\r$//' | tee -a "$LOG_FILE"
else
	echo "Compiler executable not found!" | tee -a "$LOG_FILE]"
	echo "Searching for tocin.exe..." | tee -a "$LOG_FILE]"
	find .. -name "tocin.exe" -type f 2>/dev/null | tee -a "$LOG_FILE" || echo "No tocin.exe found anywhere" | tee -a "$LOG_FILE"
	exit 1
fi

echo
echo "Build process completed successfully!" 
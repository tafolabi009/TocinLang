#!/bin/bash

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

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

# Verify Makefile exists in build directory
if [ ! -f Makefile ]; then
    echo "Error: Makefile not found in build directory!"
    echo "Checking if CMake generated files in parent directory..."
    if [ -f ../Makefile ]; then
        echo "Found Makefile in parent directory, using that instead..."
        cd ..
    else
        echo "No Makefile found! CMake configuration may have failed."
        exit 1
    fi
fi

# Build the project
echo "Building project..."
mingw32-make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Test the compiler
echo "Testing compiler..."
if [ -f tocin.exe ]; then
    echo "Compiler built successfully!"
    ./tocin.exe --help
    echo
    echo "Creating test file..."
    echo 'print("Hello from Tocin!");' > test.to
    echo "Running test..."
    ./tocin.exe test.to --jit
elif [ -f ../tocin.exe ]; then
    echo "Compiler built successfully in parent directory!"
    ../tocin.exe --help
    echo
    echo "Creating test file..."
    echo 'print("Hello from Tocin!");' > test.to
    echo "Running test..."
    ../tocin.exe test.to --jit
else
    echo "Compiler executable not found!"
    echo "Searching for tocin.exe..."
    find .. -name "tocin.exe" -type f 2>/dev/null || echo "No tocin.exe found anywhere"
    exit 1
fi

echo
echo "Build process completed successfully!" 
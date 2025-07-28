#!/bin/bash

echo "Building Tocin Compiler with MSYS2 MinGW64..."
echo "Using MSYS2 installation at: D:/Downloads/msys64"
echo

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
else
    echo "Compiler executable not found!"
    exit 1
fi

echo
echo "Build process completed successfully!" 
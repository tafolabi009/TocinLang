#!/bin/bash

# Clean build directory
rm -rf build/*

# Configure with CMake
cmake -G "MinGW Makefiles" -S . -B build

# Build
cmake --build build

echo "Build complete. The executable is located at: build/tocin.exe" 

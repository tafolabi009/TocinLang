#!/bin/bash

# Exit on error
set -e

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Create packages
cpack -G "NSIS;DEB;RPM;TGZ;ZIP"

# Move packages to dist directory
mkdir -p ../dist
mv *.exe *.deb *.rpm *.tar.gz *.zip ../dist/

echo "Packages created successfully in the dist directory!" 
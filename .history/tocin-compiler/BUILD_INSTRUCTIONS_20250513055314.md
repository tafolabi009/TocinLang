# Tocin Compiler Build Instructions
# Building the Tocin Compiler

This document explains how to build the Tocin compiler on different platforms and environments.

## Prerequisites

The Tocin compiler has the following dependencies:

- A C++17 compatible compiler (MSVC, GCC, or Clang)
- LLVM libraries (optional but recommended)
- CMake 3.16 or higher (optional)

## Building on Windows

### Option 1: Using the provided build scripts

If you don't have CMake installed, you can use the build scripts provided:

#### PowerShell (Recommended for Windows)

1. Navigate to the project root directory in PowerShell
2. Create a build directory: `mkdir build`
3. Move to the build directory: `cd build`
4. Run the build script: `..\build.ps1`

#### Batch File

1. Navigate to the project root directory in Command Prompt
2. Create a build directory: `mkdir build`
3. Move to the build directory: `cd build`
4. Run the build script: `..\build.bat`

### Option 2: Using CMake

If you have CMake installed, you can use it to generate build files:

1. Navigate to the project root directory
2. Create a build directory: `mkdir build`
3. Move to the build directory: `cd build`
4. Run CMake to generate build files: `cmake .. -G "MinGW Makefiles"` (or your preferred generator)
5. Build the project: `cmake --build .`

## Installation Options

### MSYS2/MinGW (Recommended for Windows)

MSYS2 provides an easy way to install GCC, LLVM, and other dependencies on Windows:

1. Download and install MSYS2 from [https://www.msys2.org/](https://www.msys2.org/)
2. Open MSYS2 MINGW64 terminal
3. Update the package database: `pacman -Syu`
4. Install the required packages:
   ```
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-llvm mingw-w64-x86_64-cmake
   ```
5. Add the MSYS2 bin directory to your PATH (usually `C:\msys64\mingw64\bin`)

### Visual Studio

Visual Studio Community Edition provides the MSVC compiler:

1. Download and install from [https://visualstudio.microsoft.com/vs/community/](https://visualstudio.microsoft.com/vs/community/)
2. During installation, select "Desktop development with C++"
3. Optionally, install the "LLVM tools for Windows" in the Visual Studio Installer

## Troubleshooting

### Missing LLVM Headers

If you encounter errors related to missing LLVM headers (like `llvm/Support/Host.h`), 
the compiler will automatically use fallback implementations from the `llvm_shim.h` file.
However, for full functionality, we recommend installing LLVM properly.

### Common Issues

1. **"Command not found" errors**: Ensure the compiler is installed and in your PATH.
   For PowerShell, you can check the location with `Get-Command g++` or `Get-Command cl`.

2. **Missing libraries at runtime**: Ensure that any required DLLs are in your PATH or
   in the same directory as the executable.

3. **Compilation errors with LLVM**: Make sure the LLVM include directories are properly set.
   The build scripts attempt to find them in common locations.

## Manual Compilation

If the build scripts don't work for your environment, you can manually compile the project:

### With GCC/MinGW

```
g++ -std=c++17 -I../src -IC:/msys64/mingw64/include -IC:/msys64/mingw64/include/llvm src/main.cpp src/error/error_handler.cpp src/lexer/lexer.cpp src/lexer/token.cpp -o tocin.exe
```

### With MSVC

```
cl.exe /std:c++17 /EHsc /MD /I../src /IC:/msys64/mingw64/include /IC:/msys64/mingw64/include/llvm src/main.cpp src/error/error_handler.cpp src/lexer/lexer.cpp src/lexer/token.cpp /link /OUT:tocin.exe
```

## Contributing

When adding new files to the project, make sure to update the CMakeLists.txt file or 
the build scripts to include them in the compilation process. 

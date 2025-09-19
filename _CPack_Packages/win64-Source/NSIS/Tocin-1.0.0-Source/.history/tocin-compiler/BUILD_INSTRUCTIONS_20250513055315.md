# Tocin Compiler Build Instructions

## Overview

This document provides instructions for building the Tocin compiler, including solutions to common LLVM linking issues.

## Environment Setup

The Tocin compiler has been tested with the following environment:

- Windows 10/11 with MSYS2 MinGW64
- GCC/G++ version 15.1.0 (or later)
- LLVM version 20.1.3 (via MSYS2 package)
- CMake 3.16 or higher

## Installation of Required Packages

1. Install MSYS2 from [https://www.msys2.org/](https://www.msys2.org/)

2. Open MSYS2 MinGW64 terminal and update the system:
   ```
   pacman -Syu
   ```

3. Install required packages:
   ```
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make
   pacman -S mingw-w64-x86_64-llvm mingw-w64-x86_64-clang
   pacman -S mingw-w64-x86_64-zlib mingw-w64-x86_64-zstd mingw-w64-x86_64-libxml2
   pacman -S mingw-w64-x86_64-libffi mingw-w64-x86_64-v8
   ```

## Build Options

### Option 1: Using CMake

1. Configure the project:
   ```
   cmake -G "MinGW Makefiles" -S . -B build
   ```

2. Build the project:
   ```
   cmake --build build
   ```

### Option 2: Using the Direct Build Script

We've included a PowerShell build script that compiles the project directly, bypassing CMake:

```
.\build_with_llvm.ps1
```

This script handles compilation and linking with the appropriate LLVM libraries.

## Troubleshooting LLVM Linking Issues

The most common issue when building the Tocin compiler is related to LLVM library linking. Here are solutions for common problems:

### Missing LLVM Libraries

**Problem:** Linker can't find LLVM component libraries.

**Solution:** 
The MSYS2 MinGW environment typically provides a single consolidated LLVM library instead of individual component libraries. The CMakeLists.txt has been updated to use the single library:

```cmake
# Use a single LLVM library instead of individual components
set(llvm_libs LLVM)
```

### LLVM Version Mismatch

**Problem:** Compiler errors about missing LLVM symbols or incompatible types.

**Solution:**
Ensure you're using the correct LLVM version for your environment. Check the version with:
```
llvm-config --version
```

### Testing LLVM Integration

To test if your LLVM integration works correctly, try compiling this minimal test program:

```cpp
// test_llvm.cpp
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <iostream>

int main() {
    llvm::LLVMContext context;
    auto module = std::make_unique<llvm::Module>("test", context);
    std::cout << "Module created successfully" << std::endl;
    return 0;
}
```

Compile with:
```
g++ test_llvm.cpp -o test_llvm -I<path_to_llvm_include> -lLLVM -L<path_to_llvm_lib>
```

### Dynamic Library Path Issues

**Problem:** Executable builds but crashes when run.

**Solution:**
Ensure the LLVM DLLs are in your PATH or in the same directory as the executable:
```
copy C:\msys64\mingw64\bin\libLLVM.dll build\
```

## Advanced: Manual Compilation and Linking

If all else fails, you can compile and link the Tocin compiler manually:

1. Compile all source files:
   ```
   g++ -c src/main.cpp -Isrc -I<llvm_include_path> -std=c++17
   g++ -c src/compiler/compiler.cpp -Isrc -I<llvm_include_path> -std=c++17
   g++ -c src/codegen/ir_generator.cpp -Isrc -I<llvm_include_path> -std=c++17
   # ... repeat for other source files
   ```

2. Link all object files with LLVM:
   ```
   g++ *.o -o tocin.exe -L<llvm_lib_path> -lLLVM -lz -lstdc++ -lws2_32 -ladvapi32
   ```

## Frequently Asked Questions

### Q: How do I find where LLVM is installed?

**A:** In MSYS2 MinGW64, LLVM is typically installed in:
- Include files: `C:/msys64/mingw64/include`
- Library files: `C:/msys64/mingw64/lib`

### Q: How can I check which LLVM libraries are available?

**A:** Run this command to list LLVM libraries:
```
ls -la C:/msys64/mingw64/lib/libLLVM*
```

### Q: What if I need to use individual LLVM component libraries?

**A:** If you must use individual components, you'll need to extract them from the combined library or compile LLVM from source with the appropriate configuration.

# Build Instructions for Tocin Compiler

## Prerequisites

1. **CMake** (3.15 or newer)
2. A C++ compiler with C++17 support:
   - GCC 7.3 or newer
   - Clang 6.0 or newer
   - MSVC 19.14 (Visual Studio 2017) or newer
3. **LLVM 15.0 or newer** with development packages
4. **Python 3.8 or newer** with development headers
5. **V8 JavaScript engine** libraries and headers (optional for JavaScript FFI)
6. **libffi** development package (for foreign function interface)
7. **zlib** development package
8. **libxml2** development package

## Environment Setup

### Windows with MSYS2/MinGW64

1. Install MSYS2 from https://www.msys2.org/
2. Open MSYS2 MinGW64 shell and run:
   ```bash
   pacman -Syu
   pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake
   pacman -S mingw-w64-x86_64-llvm mingw-w64-x86_64-clang
   pacman -S mingw-w64-x86_64-python mingw-w64-x86_64-python-pip
   pacman -S mingw-w64-x86_64-libffi mingw-w64-x86_64-zlib mingw-w64-x86_64-libxml2
   pacman -S mingw-w64-x86_64-v8
   ```

3. Verify LLVM installation:
   ```bash
   llvm-config --version
   ```

### Linux

```bash
sudo apt update
sudo apt install build-essential cmake
sudo apt install llvm-dev clang
sudo apt install python3-dev
sudo apt install libffi-dev zlib1g-dev libxml2-dev
# Install V8 if needed
```

## Building the Compiler

### Basic Build

1. Clone the repository:
   ```bash
   git clone https://github.com/username/tocin-compiler.git
   cd tocin-compiler
   ```

2. Configure with CMake:
   ```bash
   cmake -G "MinGW Makefiles" -S . -B build  # For Windows with MSYS2/MinGW
   # OR
   cmake -S . -B build  # For Linux or other environments
   ```

3. Build the compiler:
   ```bash
   cmake --build build
   ```

4. Run tests:
   ```bash
   ctest --test-dir build
   ```

### Troubleshooting IR Generator Issues

The IR generator component has several dependencies on LLVM libraries and can be sensitive to version mismatches. Here are some common issues and solutions:

#### 1. LLVM API Compatibility

If you encounter errors related to LLVM API calls like:
- `getPointerElementType()` not being available
- `CreateArrayMalloc` not being found
- `getBasicBlockList()` being private

**Solution:**
- These are usually due to LLVM API changes between versions
- The IR generator in this codebase has been updated for LLVM 15.0+
- If using an older LLVM version, you may need to adapt the code to match your LLVM version

#### 2. Build Errors with the IR Generator

If you're having issues building specifically with the IR generator:

1. Try building just the IR generator to isolate issues:
   ```bash
   cmake --build build --target ir_generator_direct
   ```

2. Check LLVM include paths:
   ```bash
   # For Windows/MSYS2
   echo %INCLUDE%
   # For Linux
   echo $CPLUS_INCLUDE_PATH
   ```

3. If needed, specify LLVM paths explicitly:
   ```bash
   cmake -G "MinGW Makefiles" -S . -B build -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm
   ```

#### 3. Alternative Build Options

If you continue to have issues with LLVM integration:

1. Build without the IR generator:
   ```bash
   cmake -G "MinGW Makefiles" -S . -B build -DBUILD_IR_GENERATOR=OFF
   ```

2. Use a Docker container with a known-good environment:
   ```bash
   # See Dockerfile in the dev-tools directory
   docker build -t tocin-compiler-build .
   docker run -v $(pwd):/src tocin-compiler-build
   ```

## Documentation

For more detailed information about the compiler architecture and components, see:

- `docs/ARCHITECTURE.md` - Overall design and architecture
- `docs/IR_GENERATOR.md` - Details about the IR generation process
- `IR_GENERATOR_STATUS.md` - Current status of the IR generator implementation

## Common Issues

### LLVM Error: No such file or directory

This usually means CMake couldn't find the LLVM installation. Make sure LLVM is installed and that the environment variables are set correctly.

### Cannot find -lLLVM

This indicates a linking problem with LLVM libraries. Try:
```bash
llvm-config --libs
```
And make sure these libraries are in the linker path.

### Mismatch between IR generator and AST structure

The IR generator must match the AST structure exactly. If you've made changes to the AST classes, you'll need to update the IR generator visitor methods to match.

\# Building Tocin

This guide covers how to build the Tocin compiler and runtime on all major platforms, enable/disable optional features, and troubleshoot common build issues.

## Prerequisites
- CMake 3.15+
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- LLVM 11.0 or newer
- Python 3.6+ (for Python FFI, optional)
- Node.js 12+ and V8 (for JavaScript FFI, optional)
- zstd, zlib, libffi, libxml2 (optional, for extra features)

## Basic Build (All Platforms)
```bash
git clone https://github.com/yourusername/tocin-compiler.git
cd tocin-compiler
mkdir build && cd build
cmake ..
cmake --build .
```

## Enabling/Disabling Features
You can enable or disable optional features using CMake flags:
```bash
cmake -DWITH_PYTHON=ON -DWITH_V8=OFF -DWITH_ZSTD=ON -DWITH_XML=OFF ..
```
- `WITH_PYTHON`: Enable Python FFI (default ON)
- `WITH_V8`: Enable JavaScript (V8) FFI (default ON)
- `WITH_ZSTD`: Enable zstd compression (default ON)
- `WITH_XML`: Enable XML support (default ON)

## Platform Notes
### Windows
- Use PowerShell or Git Bash for best results.
- Ensure LLVM, Python, and Node.js are in your PATH.
- If using MSYS2, install dependencies via pacman.

### Linux/macOS
- Use your system package manager to install dependencies (e.g., `apt`, `brew`).
- For FFI, ensure Python/Node.js are installed and development headers are available.

## Troubleshooting
- **LLVM not found:** Set `LLVM_DIR` to your LLVM CMake config path.
- **Python/V8 not found:** Ensure development headers are installed and in your PATH.
- **Linker errors:** Check that all required libraries are installed and accessible.
- **Feature disabled:** Use `cmake -DWITH_FEATURE=ON` to enable, or check CMake output for warnings.

## Cleaning and Rebuilding
```bash
rm -rf build
mkdir build && cd build
cmake ..
cmake --build .
```

## Installing and Uninstalling

After building, you can install Tocin system-wide or to a custom prefix:

```bash
# Install to the default prefix (./build/install)
cmake --install .

# Install to a custom prefix
cmake --install . --prefix=/usr/local

# Uninstall (from the build directory)
cmake --build . --target uninstall
```

- The install target copies the compiler, headers, stdlib, and docs to the prefix.
- The uninstall target removes all installed files and directories.

## Advanced CMake Options
- `-DCMAKE_INSTALL_PREFIX=...` : Set custom install location
- `-DCMAKE_BUILD_TYPE=Release|Debug` : Choose build type
- `-DBUILD_SHARED_LIBS=ON|OFF` : Build shared/static libraries
- `-DCMAKE_SKIP_RPATH=ON|OFF` : Control RPATH usage

These options can be passed to `cmake ..` during configuration.

## Using CMake Presets and IDE Integration

- The project provides a `CMakePresets.json` for fast, consistent setup.
- Supported by VSCode, CLion, and the CMake CLI.

### Command Line
```bash
cmake --preset=default
cmake --build --preset=default
cmake --install --preset=default
```

### VSCode/CLion
- Open the project folder. The IDE will detect CMakePresets and offer to configure/build with them.
- Switch between debug/release/ci builds easily.
- Intellisense and code navigation work out of the box.

### Troubleshooting
- If the IDE does not detect presets, ensure you are using CMake 3.19+ and the latest IDE version.
- You can always fall back to manual `cmake ..` configuration if needed.

See also: [README.md](../README.md), [BENCHMARKS.md](../BENCHMARKS.md) 
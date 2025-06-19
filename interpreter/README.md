# Tocin Programming Language Interpreter

## Copyright Notice
Copyright (c) 2024 Genovo Technologies. All rights reserved.

This software was created by Afolabi Oluwatosin for Genovo Technologies. It is proprietary software and may not be distributed, modified, or used without explicit permission from Genovo Technologies.

## Features
- Modern programming language interpreter
- High-performance execution
- Comprehensive standard library
- Cross-platform support
- Advanced optimization techniques

## Building from Source

### Prerequisites
- CMake 3.10 or higher
- C++17 compatible compiler
- LLVM
- Boost libraries
- MSYS2 (for Windows builds)

### Windows Build Instructions
1. Install MSYS2 from https://www.msys2.org/
2. Run the setup script:
   ```bash
   ./setup_msys.bat
   ```
3. The build artifacts will be available in the `dist` directory

### Linux Build Instructions
1. Install dependencies:
   ```bash
   sudo apt-get install cmake g++ libllvm-dev libboost-all-dev
   ```
2. Build the project:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

### macOS Build Instructions
1. Install dependencies using Homebrew:
   ```bash
   brew install cmake llvm boost
   ```
2. Build the project:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

## Project Structure
```
interpreter/
├── src/           # Source files
├── include/       # Header files
├── resources/     # Resources (icons, etc.)
└── tests/         # Test files
```

## License
This software is proprietary and confidential. Unauthorized copying, distribution, or use is strictly prohibited.

## Contact
For support or licensing inquiries, please contact:
- Email: support@genovotech.com
- Website: https://genovotech.com 
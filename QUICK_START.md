# Tocin Compiler - Quick Start Guide

## 🚀 Get Started in 5 Minutes

Welcome to Tocin! This guide will help you get up and running with the Tocin programming language compiler quickly.

## 📋 Prerequisites

Before you begin, make sure you have the following installed:

- **CMake** (3.16 or later)
- **C++ Compiler** (GCC, Clang, or MSVC)
- **LLVM** (20.x or later) - Optional but recommended for optimizations

### Windows Users
```bash
# Install Visual Studio Build Tools or MinGW
# Download CMake from https://cmake.org/download/
```

### macOS Users
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install CMake and LLVM via Homebrew
brew install cmake llvm
```

### Linux Users
```bash
# Ubuntu/Debian
sudo apt-get install cmake build-essential llvm-dev

# CentOS/RHEL
sudo yum install cmake gcc-c++ llvm-devel

# Arch Linux
sudo pacman -S cmake gcc llvm
```

## 🔧 Building the Compiler

### Option 1: Automatic Build (Recommended)

Run the standalone build script:

```bash
# Windows
build_standalone.bat

# Linux/macOS
./build_standalone.sh
```

### Option 2: Manual Build

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build . --config Release
```

## ✅ Verify Installation

After building, you should have the following executables in the `dist/` directory:

- `tocin.exe` (Windows) or `tocin` (Linux/macOS) - The main compiler
- `tocin-repl.exe` (Windows) or `tocin-repl` (Linux/macOS) - Interactive REPL

Test the installation:

```bash
# Test the compiler
./dist/tocin.exe --version

# Test the REPL
./dist/tocin-repl.exe
```

## 🎯 Your First Tocin Program

Create a file called `hello.to`:

```tocin
// Your first Tocin program
def main() -> int {
    println("Hello, Tocin!");
    return 0;
}
```

Compile and run it:

```bash
# Compile the program
./dist/tocin.exe hello.to -o hello

# Run the executable
./hello
```

## 📚 Learning Tocin

### Basic Syntax

```tocin
// Variables and types
let x: int = 42;
let y: float = 3.14;
let message: string = "Hello, World!";
let flag: bool = true;

// Functions
def add(a: int, b: int) -> int {
    return a + b;
}

// Control flow
if (x > 40) {
    println("x is large");
} else {
    println("x is small");
}

// Loops
for (let i: int = 0; i < 10; i = i + 1) {
    println(i);
}

// Classes
class Point {
    property x: float;
    property y: float;
    
    def constructor(x: float, y: float) {
        self.x = x;
        self.y = y;
    }
    
    def distance(other: Point) -> float {
        let dx = self.x - other.x;
        let dy = self.y - other.y;
        return Math.sqrt(dx * dx + dy * dy);
    }
}
```

### Advanced Features

```tocin
// Traits
trait Printable {
    fn print(self);
}

impl Printable for Point {
    fn print(self) {
        println("Point(" + self.x + ", " + self.y + ")");
    }
}

// Error handling
def safeDivide(a: int, b: int) -> Option<int> {
    if (b == 0) {
        return None;
    }
    return Some(a / b);
}

// Concurrency
let channel: Channel<int> = new Channel<int>();

go {
    for (let i: int = 0; i < 5; i = i + 1) {
        channel.send(i);
    }
    channel.close();
};

for (let value in channel) {
    println(value);
}
```

## 🛠️ Development Tools

### VS Code Extension

Install the Tocin extension for VS Code:

1. Open VS Code
2. Go to Extensions (Ctrl+Shift+X)
3. Search for "Tocin"
4. Install the extension

### Language Server Protocol

The Tocin LSP provides:

- Syntax highlighting
- Code completion
- Error detection
- Go to definition
- Find references
- Rename symbol

### REPL (Interactive Mode)

Start the interactive REPL:

```bash
./dist/tocin-repl.exe
```

Example REPL session:

```tocin
>>> let x = 42
>>> let y = x * 2
>>> println(y)
84
>>> def factorial(n) { if n <= 1 { 1 } else { n * factorial(n - 1) } }
>>> factorial(5)
120
```

## 📖 Documentation

- **[Language Basics](docs/03_Language_Basics.md)** - Core language features
- **[Standard Library](docs/04_Standard_Library.md)** - Built-in libraries
- **[Advanced Topics](docs/05_Advanced_Topics.md)** - Advanced language features
- **[Examples](examples/)** - Sample programs and demos

## 🧪 Testing

Run the test suite:

```bash
# Run all tests
./dist/tocin.exe tests/test_compiler_integration.to

# Run specific test categories
./dist/tocin.exe tests/test_basic_syntax.to
./dist/tocin.exe tests/test_advanced_features.to
```

## 🚀 Performance

Tocin is designed for high performance:

- **Compilation Speed**: Fast compilation with LLVM backend
- **Runtime Performance**: Near-native performance
- **Memory Safety**: Zero-cost abstractions with safety guarantees
- **Concurrency**: Efficient goroutines and channels

## 🤝 Getting Help

### Community Resources

- **GitHub Issues**: Report bugs and request features
- **Discussions**: Ask questions and share ideas
- **Documentation**: Comprehensive guides and references
- **Examples**: Sample code and tutorials

### Common Issues

**Build Errors**
```bash
# If CMake is not found
# Install CMake from https://cmake.org/download/

# If LLVM is not found
# Install LLVM development packages
```

**Runtime Errors**
```bash
# Check if all dependencies are installed
# Verify the executable is in your PATH
# Run with verbose output for debugging
./dist/tocin.exe --verbose your_program.to
```

## 🎯 Next Steps

1. **Explore Examples**: Check out the `examples/` directory
2. **Read Documentation**: Start with the language basics
3. **Join Community**: Contribute to the project
4. **Build Something**: Create your own Tocin programs

## 📈 Performance Tips

- Use type annotations for better performance
- Leverage the ownership system for memory safety
- Use concurrency for I/O-bound operations
- Profile your code with the built-in profiler

## 🔧 Configuration

Create a `tocin.json` configuration file:

```json
{
    "compiler": {
        "optimization": "O2",
        "target": "native",
        "warnings": "all"
    },
    "formatting": {
        "indent": 4,
        "lineLength": 100
    }
}
```

## 🎉 Congratulations!

You're now ready to start programming with Tocin! The language combines the safety of Rust, the expressiveness of Kotlin, and the performance of C++.

Happy coding! 🚀 
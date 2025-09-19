# Tocin Programming Language Compiler

Tocin is a modern, statically-typed programming language designed for clarity, performance, and interoperability. This repository contains the Tocin compiler implementation.

## Features

- Strong static typing with type inference
- First-class functions and closures
- Pattern matching
- Coroutines and async/await syntax
- Comprehensive standard library
- Interoperability with C, Python, and JavaScript
- Optimized LLVM-based code generation
- REPL for interactive development

## Getting Started

### Prerequisites

- CMake 3.15+
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- LLVM 11.0 or newer
- Python 3.6+ (for Python FFI)
- Node.js 12+ (for JavaScript FFI)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/yourusername/tocin-compiler.git
cd tocin-compiler

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build .
```

### Running the Compiler

```bash
# Compile a Tocin source file
./tocin source.to -o output

# Run the compiled program via JIT
./tocin source.to --jit

# Start the REPL
./tocin
```

## Language Syntax

### Variables and Constants

```
let x: int = 42;
const PI: float = 3.14159;
```

### Functions

```
def add(a: int, b: int) -> int {
    return a + b;
}

// Lambda function
let multiply = lambda (x: int, y: int) -> int { x * y };
```

### Control Flow

```
if x > 0 {
    print("Positive");
} elif x < 0 {
    print("Negative");
} else {
    print("Zero");
}

while condition {
    // statements
}

for i in range(10) {
    // statements
}
```

### Classes

```
class Point {
    let x: float;
    let y: float;
    
    def init(x: float, y: float) {
        self.x = x;
        self.y = y;
    }
    
    def distance(other: Point) -> float {
        let dx = self.x - other.x;
        let dy = self.y - other.y;
        return sqrt(dx * dx + dy * dy);
    }
}
```

### Pattern Matching

```
match value {
    case 0 { print("Zero"); }
    case 1 { print("One"); }
    case _ { print("Other"); }
}
```

### Asynchronous Programming

```
async def fetch_data(url: string) -> string {
    // asynchronous implementation
}

async def process() {
    let data = await fetch_data("https://example.com");
    print(data);
}
```

## Command Line Options

- `--help`: Display help message
- `--compress`: Compress source code before compilation
- `--serialize-ast`: Serialize AST to XML
- `--dump-ir`: Dump LLVM IR to stdout
- `-O0`, `-O1`, `-O2`, `-O3`: Set optimization level (default: -O2)
- `-o <file>`: Write output to file
- `-c`: Generate object file
- `-S`: Generate assembly file
- `--jit`: Run the program using JIT compilation

## Project Structure

- `src/lexer/`: Lexical analysis
- `src/parser/`: Syntactic analysis and AST construction
- `src/ast/`: Abstract Syntax Tree definitions
- `src/type/`: Type system and type checking
- `src/codegen/`: LLVM IR code generation
- `src/compiler/`: Compilation pipeline
- `src/error/`: Error handling
- `src/ffi/`: Foreign Function Interface
- `src/runtime/`: Runtime library support

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the LICENSE.txt file for details. 

# Tocin Programming Language Compiler

Tocin is a modern, statically-typed programming language designed for clarity, performance, concurrency, and world-class interoperability. This repository contains the Tocin compiler and standard library.

## Features

- Strong static typing with type inference
- Traits (interfaces with default methods)
- LINQ-style collection operations
- Null safety (safe call, Elvis operator, not-null assertion)
- Pattern matching
- Coroutines, async/await, and concurrency primitives (channels, select)
- Comprehensive standard library (math, string, web, ML, etc.)
- Interoperability with C, Python, and JavaScript (FFI)
- Optimized LLVM-based code generation
- REPL for interactive development
- Robust error handling and diagnostics
- Automated test suite and coverage

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

#### Platform Notes
- **Windows:** Use PowerShell or Git Bash. Ensure LLVM and Python are in your PATH.
- **Linux/macOS:** Standard build tools required. For FFI, ensure Python/Node.js are installed.

### Running the Compiler

```bash
# Compile a Tocin source file
./tocin source.to -o output

# Run the compiled program via JIT
./tocin source.to --jit

# Start the REPL
./tocin
```

## Language Highlights

### Traits
```to
trait Printable { fn print(self); }
struct Point { x: int, y: int }
impl Printable for Point { fn print(self) { print(self.x, self.y); } }
let p = Point { x: 1, y: 2 };
p.print();
```

### LINQ
```to
let numbers = [1, 2, 3, 4, 5];
let evens = numbers.where(x => x % 2 == 0);
let squares = numbers.select(x => x * x);
```

### Null Safety
```to
let s: string? = null;
let len = s?.length;
let value = s ?: "default";
```

### FFI
```to
let py_result = ffi.python.call("len", [[1,2,3]]);
let js_result = ffi.javascript.call("eval", ["1+2+3"]);
```

### Concurrency
```to
let ch: channel<int> = new channel<int>();
go(worker_function());
ch <- 42;
let value: int = <-ch;
select {
    case ch <- 100: print("Sent 100");
    case value := <-ch: print("Received", value);
    default: print("No ops ready");
}
```

## Testing

### Running All Tests
```bash
# (After building) Run all tests in the tests/ directory
# Example (PowerShell):
Get-ChildItem tests/*.to | ForEach-Object { ./tocin $_.FullName }
```
- Each test should print output or errors. Negative tests are commented for manual/CI review.
- See `tests/` for templates covering traits, FFI, LINQ, null safety, error handling, and stdlib.

### Adding New Tests
- Add a `.to` file to `tests/`.
- Use comments to indicate expected errors for negative tests.
- Run all tests as above.

## Troubleshooting
- **Build errors:** Ensure all prerequisites are installed and in your PATH.
- **FFI errors:** Ensure Python/Node.js are installed and enabled in CMake.
- **Test failures:** Check for recent changes or platform-specific issues.
- **Need help?** Open an issue or see the docs/ directory for more.

## Contributing

Contributions are welcome! Please:
- Read `CONTRIBUTING.md` for guidelines and code style.
- Add/expand tests for new features.
- Document new APIs and modules.
- Use clear commit messages and PR descriptions.

## Project Structure
- `src/lexer/`, `src/parser/`, `src/ast/`, `src/type/`, `src/codegen/`, `src/compiler/`, `src/error/`, `src/ffi/`, `src/runtime/`: Core compiler and runtime
- `stdlib/`: Standard library modules
- `tests/`: Automated and manual tests
- `examples/`: Real-world usage and demos
- `docs/`: Language and API documentation

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Continuous Integration (CI/CD)

Tocin uses GitHub Actions for automated CI/CD:
- **Build matrix:** Windows, Linux, and macOS
- **CMake build:** Ensures all platforms build successfully
- **Automated tests:** Runs all tests in `tests/` on every PR and push
- **Coverage:** Collects and uploads code coverage reports
- **Artifacts:** Uploads build and test artifacts for inspection

All pull requests and pushes to main branches are automatically tested. See [CONTRIBUTING.md](CONTRIBUTING.md) for more details on CI/CD and how to contribute safely. 

# Tocin Interpreter Guide

## Overview

The Tocin interpreter provides a powerful, Python-like experience with advanced features including JIT compilation, concurrent execution, and comprehensive error handling. This guide explains how to use the interpreter effectively and how it compares to other popular interpreters.

## Interpreter Features

### 1. Interactive REPL Mode

Start the interpreter in REPL mode for interactive development:

```bash
./tocin
```

The REPL supports:
- Multi-line input with automatic indentation detection
- Command history and editing
- Tab completion (if supported by terminal)
- Inline documentation with `help(function_name)`
- Expression evaluation with automatic result printing

### 2. Script Execution

Execute Tocin scripts directly:

```bash
./tocin script.to
```

Options:
- `-O0`, `-O1`, `-O2`, `-O3`: Optimization levels
- `--jit`: Enable JIT compilation for performance
- `--ast`: Print AST for debugging
- `--ir`: Print LLVM IR for inspection
- `--time`: Show execution time statistics

### 3. Output Format

The Tocin interpreter provides clear, informative output similar to Python but with enhancements:

#### Expression Results
```to
# Tocin REPL
> 2 + 3
5

> "Hello, " + "World!"
"Hello, World!"

> [1, 2, 3].map(x => x * 2)
[2, 4, 6]
```

#### Function Definitions
```to
> def greet(name: string) -> string {
    return "Hello, " + name;
  }
Function 'greet' defined successfully

> greet("Alice")
"Hello, Alice"
```

#### Error Messages
```to
> 1 / 0
RuntimeError: Division by zero
  at <stdin>:1:3
  
> nonexistent_var
NameError: Variable 'nonexistent_var' is not defined
  at <stdin>:1:1
```

## Comparison with Python Interpreter

### Similarities
- Interactive REPL with immediate feedback
- Clear error messages with line numbers
- Expression evaluation and auto-printing
- Multi-line input support

### Enhancements Over Python

#### 1. **Built-in JIT Compilation**
```bash
# Tocin with JIT
./tocin --jit compute_heavy.to

# Python (requires PyPy or manual optimization)
python compute_heavy.py
```

The Tocin JIT compiler automatically optimizes hot code paths using LLVM, providing performance comparable to compiled languages.

#### 2. **Static Type Safety with Inference**
```to
# Tocin - Type errors caught before execution
let x: int = 42;
x = "hello";  // Compile-time error: Type mismatch

# Python - Type errors only at runtime
x = 42
x = "hello"  # No error until variable is used
```

#### 3. **Built-in Concurrency Primitives**
```to
# Tocin - Native goroutines
go {
    print("Running concurrently");
}

# Python - Requires threading/asyncio
import threading
threading.Thread(target=lambda: print("Running concurrently")).start()
```

#### 4. **Enhanced Error Messages**
Tocin provides detailed stack traces with context:

```
Error: Type mismatch in function call
  Expected: int
  Received: string
  at main.to:15:8
  in function 'calculate'
  called from main.to:23:5
  
Suggestion: Convert the string to int using int.parse()
```

#### 5. **Performance Monitoring**
```bash
./tocin --time script.to

# Output includes:
Performance Statistics:
  Total execution time: 1.234s
  JIT compilation time: 0.123s
  Function calls: 1,523
  Memory allocations: 45,678
```

## Advanced Features

### Memory Management

The interpreter includes a generational garbage collector:

```to
// Objects are automatically managed
let large_array = Array.new(1000000);
// Memory is reclaimed when no longer needed
```

### Optimization Passes

Multiple optimization levels are available:

- **-O0**: No optimization (fastest compilation, useful for debugging)
- **-O1**: Basic optimizations (constant folding, dead code elimination)
- **-O2**: Advanced optimizations (function inlining, loop unrolling)
- **-O3**: Aggressive optimizations (all of the above plus profile-guided optimization)

### Static Analysis

The interpreter performs static analysis before execution:

```to
// Detects unreachable code
def example() {
    return 42;
    print("This will never execute");  // Warning: Unreachable code
}

// Detects potential null pointer dereferences
let x: string? = null;
print(x.length);  // Error: Possible null reference
```

## Integration with Compiler

The interpreter seamlessly works with the compiler:

### Compile and Execute
```bash
# Compile to binary
./tocin -o myapp myapp.to

# Execute directly
./myapp
```

### Mixed Mode
```bash
# Interpret with JIT for testing
./tocin --jit myapp.to

# Then compile for production
./tocin -O3 -o myapp myapp.to
```

## Best Practices

### 1. Use Type Annotations
```to
// Good: Clear types for better performance and error detection
def calculate(x: int, y: int) -> int {
    return x + y;
}

// Avoid: Type inference works but is slower
def calculate(x, y) {
    return x + y;
}
```

### 2. Enable JIT for Long-Running Scripts
```bash
# For scripts with loops or recursive functions
./tocin --jit -O2 script.to
```

### 3. Use Concurrency for I/O-Bound Tasks
```to
// Good: Non-blocking I/O
async def fetch_data(url: string) -> string {
    return await http.get(url);
}

// Use goroutines for CPU-bound parallel work
def process_large_dataset(data: Array<int>) {
    let chunks = data.chunk(1000);
    for chunk in chunks {
        go {
            process_chunk(chunk);
        }
    }
}
```

### 4. Profile Before Optimizing
```bash
./tocin --time --profile script.to
```

## Debugging

### Interactive Debugging
```to
// Set breakpoints in code
debug.breakpoint();

// Inspect variables
debug.print_locals();

// Step through execution
debug.step();
```

### Verbose Error Mode
```bash
./tocin --verbose --stack-trace script.to
```

## Performance Tips

### 1. **Leverage JIT Compilation**
The JIT compiler optimizes frequently executed code paths automatically.

### 2. **Use Appropriate Data Structures**
```to
// Fast: Specialized collections
let numbers = IntArray.new(1000);

// Slower: Generic arrays
let numbers = Array.new(1000);
```

### 3. **Minimize Allocations**
```to
// Good: Reuse objects
let buffer = String.with_capacity(1000);
for item in items {
    buffer.clear();
    buffer.append(item.to_string());
    process(buffer);
}

// Avoid: Creating new strings in loop
for item in items {
    process(item.to_string());  // Allocates each iteration
}
```

### 4. **Use Parallel Processing**
```to
// Process data in parallel
let results = data
    .parallel()
    .map(x => expensive_computation(x))
    .collect();
```

## Troubleshooting

### Common Issues

#### 1. "JIT compilation failed"
- Check LLVM installation
- Ensure sufficient memory
- Try lower optimization level

#### 2. "Stack overflow"
- Check for infinite recursion
- Increase stack size if needed: `--stack-size=16M`

#### 3. "Type inference failed"
- Add explicit type annotations
- Check for circular type dependencies

## Future Enhancements

The interpreter will continue to evolve with:
- **Language Server Protocol (LSP)**: IDE integration with autocomplete and refactoring
- **Interactive Debugger**: Step-through debugging with breakpoints
- **Profiler Integration**: Visual performance profiling tools
- **Remote Execution**: Run interpreter on remote machines
- **Notebook Support**: Jupyter-style notebook interface

## Conclusion

The Tocin interpreter combines the ease of use of Python with the performance of compiled languages, providing a superior development experience for modern applications. Its advanced features like JIT compilation, built-in concurrency, and comprehensive error handling make it ideal for both rapid prototyping and production deployment.

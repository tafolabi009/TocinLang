# Advanced Features Examples

This directory contains examples demonstrating the advanced features of the Tocin compiler, including the recently completed interpreter and FFI implementations.

## Recent Additions

### JIT Compilation
The Tocin interpreter now includes JIT (Just-In-Time) compilation support using LLVM:
- Functions can be compiled to native code at runtime
- Optimized execution for performance-critical code
- Seamless integration with interpreted code

### Goroutines
Concurrent execution is supported through goroutine-style threading:
- Launch concurrent tasks with `go` statements
- Non-blocking execution
- Thread-safe error handling

### JavaScript FFI
Complete Foreign Function Interface for JavaScript:
- Module loading and management
- Object and array operations
- Promise and async/await support
- Variable management
- Type conversion infrastructure

## Example Programs

### 1. Goroutine Example (goroutine_example.to)

```tocin
# Example demonstrating concurrent execution with goroutines

def worker(id: int, ch: channel<string>):
    for i in range(5):
        ch <- "Worker " + str(id) + " iteration " + str(i)
        sleep(100)  # Sleep 100ms

def main():
    let ch = channel<string>()
    
    # Launch multiple goroutines
    go worker(1, ch)
    go worker(2, ch)
    go worker(3, ch)
    
    # Collect results
    for i in range(15):
        let msg = <-ch
        print(msg)
    
    return 0
```

### 2. JavaScript FFI Example (js_ffi_example.to)

```tocin
# Example demonstrating JavaScript FFI

import ffi.javascript as js

def main():
    # Initialize JavaScript FFI
    if not js.initialize():
        print("Failed to initialize JavaScript FFI")
        return 1
    
    # Set a global variable
    js.setVariable("myVar", 42)
    let value = js.getVariable("myVar")
    print("JavaScript variable: " + str(value))
    
    # Create and manipulate an array
    let arr = js.createArray([1, 2, 3, 4, 5])
    print("Array length: " + str(js.getArrayLength(arr)))
    
    # Create an object
    let obj = js.createObject({
        "name": "Tocin",
        "version": "1.0",
        "year": 2024
    })
    
    if js.hasProperty(obj, "name"):
        let name = js.getProperty(obj, "name")
        print("Object name: " + str(name))
    
    # Work with promises
    let promise = js.createPromise()
    js.resolvePromise(promise, "Success!")
    let result = js.awaitPromise(promise)
    print("Promise result: " + str(result))
    
    js.finalize()
    return 0
```

### 3. JIT Compilation Example (jit_example.to)

```tocin
# Example demonstrating JIT compilation

# This function will be JIT compiled for better performance
@jit_compile
def fibonacci(n: int) -> int:
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

# This function will be JIT compiled
@jit_compile
def factorial(n: int) -> int:
    if n <= 1:
        return 1
    return n * factorial(n - 1)

def main():
    print("Computing Fibonacci numbers with JIT compilation...")
    
    for i in range(20):
        let result = fibonacci(i)
        print("fib(" + str(i) + ") = " + str(result))
    
    print("\nComputing factorials with JIT compilation...")
    
    for i in range(10):
        let result = factorial(i)
        print(str(i) + "! = " + str(result))
    
    return 0
```

### 4. Combined Example (advanced_example.to)

```tocin
# Example combining multiple advanced features

import ffi.javascript as js
import ffi.python as py
import ffi.cpp as cpp

# Async function with JIT compilation
@jit_compile
async def fetchData(url: string) -> Result<string, string>:
    try:
        let response = await http.get(url)
        return Ok(response.body)
    catch error:
        return Err("Failed to fetch: " + error.message)

# Worker function to run in goroutine
def processData(data: string, resultCh: channel<string>):
    # Use JavaScript to process JSON
    let parsed = js.eval("JSON.parse('" + data + "')")
    
    # Use Python for data analysis
    py.eval("import statistics")
    let stats = py.call("statistics.mean", parsed["values"])
    
    resultCh <- "Mean: " + str(stats)

def main():
    # Initialize FFI systems
    js.initialize()
    py.initialize()
    cpp.initialize()
    
    # Create communication channel
    let resultCh = channel<string>()
    
    # Fetch data asynchronously
    let dataResult = await fetchData("https://api.example.com/data")
    
    match dataResult:
        case Ok(data):
            # Process data in goroutine
            go processData(data, resultCh)
            
            # Wait for result
            let result = <-resultCh
            print("Processing result: " + result)
            
        case Err(error):
            print("Error: " + error)
    
    # Cleanup
    js.finalize()
    py.finalize()
    cpp.finalize()
    
    return 0
```

## Feature Status

| Feature | Status | Notes |
|---------|--------|-------|
| JIT Compilation | ✅ Implemented | Basic IR generation working |
| Goroutines | ✅ Implemented | Using std::thread |
| JavaScript FFI | ✅ Implemented | Framework ready, needs V8 for full features |
| Python FFI | ✅ Implemented | Working with Python C API |
| C++ FFI | ✅ Implemented | Using dlopen/dlsym |
| Async/Await | ⚠️ Partial | Framework exists |
| Promises | ✅ Implemented | In JavaScript FFI |
| Channels | ⚠️ Partial | Basic implementation exists |

## Building and Running Examples

### Compile an example
```bash
./build/tocin examples/goroutine_example.to -o goroutine_example
```

### Run with JIT
```bash
./build/tocin examples/jit_example.to --jit
```

### With optimization
```bash
./build/tocin examples/advanced_example.to -O3 -o advanced_example
```

### Enable specific features
```bash
./build/tocin examples/js_ffi_example.to --enable-javascript -o js_example
```

## Testing

Run the test suite to verify all features:
```bash
python3 test_interpreter_completion.py
```

## Documentation

For more details on the implementation, see:
- `INTERPRETER_COMPLETION.md` - Detailed completion summary
- `COMPLETION_SUMMARY.md` - Overall project completion status
- `IMPROVEMENTS.md` - Feature improvements and enhancements
- `README.md` - General project information

## Contributing

When adding new examples:
1. Use clear, well-commented code
2. Demonstrate one or two features per example
3. Include error handling
4. Test the example before committing
5. Update this README with the new example

## Notes

- Some features require external dependencies (V8, Python, etc.)
- JIT compilation is automatic for frequently-called functions
- Goroutines are OS threads, not lightweight green threads
- FFI features gracefully degrade when dependencies are unavailable

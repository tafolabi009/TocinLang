# Integration Guide for Advanced Features

This document explains how to integrate and use the newly implemented advanced features in your Tocin projects.

## Table of Contents
1. [Building with Advanced Features](#building-with-advanced-features)
2. [V8 JavaScript Integration](#v8-javascript-integration)
3. [Advanced Optimizations](#advanced-optimizations)
4. [Lightweight Goroutine Scheduler](#lightweight-goroutine-scheduler)
5. [Examples](#examples)
6. [Performance Tuning](#performance-tuning)

## Building with Advanced Features

### Prerequisites

#### V8 JavaScript Engine
**Linux (Debian/Ubuntu):**
```bash
sudo apt-get install libv8-dev
```

**macOS:**
```bash
brew install v8
```

**Windows (vcpkg):**
```bash
vcpkg install v8
```

### CMake Configuration

Update your `CMakeLists.txt` to include the new features:

```cmake
# Enable advanced features
option(WITH_V8 "Enable V8 JavaScript Engine" ON)
option(WITH_ADVANCED_OPT "Enable advanced optimizations" ON)
option(WITH_LIGHTWEIGHT_SCHEDULER "Enable lightweight goroutine scheduler" ON)

# Add source directories
add_subdirectory(src/v8_integration)
add_subdirectory(src/compiler)
add_subdirectory(src/runtime)

# Link libraries
target_link_libraries(tocin PRIVATE 
    v8_integration
    advanced_optimizations
    lightweight_scheduler
)
```

### Build Commands

```bash
mkdir build
cd build

# Build with all features
cmake -DWITH_V8=ON -DWITH_ADVANCED_OPT=ON -DWITH_LIGHTWEIGHT_SCHEDULER=ON ..
make -j$(nproc)

# Or build without V8 (if not available)
cmake -DWITH_V8=OFF -DWITH_ADVANCED_OPT=ON -DWITH_LIGHTWEIGHT_SCHEDULER=ON ..
make -j$(nproc)
```

## V8 JavaScript Integration

### Basic Usage in Tocin

```to
// Import JavaScript FFI module
import ffi.javascript;

// Execute JavaScript code
let result = ffi.javascript.eval("2 + 3 * 4");
print("Result: " + result);

// Define JavaScript function
ffi.javascript.eval("""
    function factorial(n) {
        return n <= 1 ? 1 : n * factorial(n - 1);
    }
""");

// Call JavaScript function from Tocin
let fact = ffi.javascript.call("factorial", [10]);
print("Factorial(10) = " + fact);
```

### Advanced V8 Usage in C++

```cpp
#include "v8_integration/v8_runtime.h"

using namespace tocin::v8_integration;

void useV8Runtime() {
    V8Runtime runtime;
    
    if (!runtime.initialize()) {
        std::cerr << "Failed to initialize V8: " 
                  << runtime.getLastError() << "\n";
        return;
    }
    
    // Execute JavaScript
    auto result = runtime.executeCode(R"(
        const arr = [1, 2, 3, 4, 5];
        arr.map(x => x * 2).reduce((a, b) => a + b, 0);
    )");
    
    if (runtime.hasError()) {
        std::cerr << "Error: " << runtime.getLastError() << "\n";
    } else {
        std::cout << "Result: " << result.asInt32() << "\n";
    }
    
    // Register Tocin function for JavaScript
    runtime.registerFunction("tocinFunction", 
        [](const std::vector<ffi::FFIValue>& args) {
            // Your Tocin logic here
            return ffi::FFIValue(42);
        }
    );
    
    runtime.shutdown();
}
```

## Advanced Optimizations

### Using the Optimization Pipeline

```cpp
#include "compiler/advanced_optimizations.h"

using namespace tocin::optimization;

void optimizeModule(llvm::Module* module) {
    // Create optimization pipeline
    AdvancedOptimizationPipeline pipeline;
    
    // Configure optimizations
    pipeline.setOptimizationLevel(3);  // 0-3
    pipeline.enablePGO(false);         // Profile-guided optimization
    pipeline.enableIPO(true);          // Interprocedural optimization
    pipeline.enablePolyhedral(true);   // Loop optimizations
    pipeline.enableLTO(true);          // Link-time optimization
    
    // Run optimization
    pipeline.optimize(module);
    
    // Get statistics
    auto stats = pipeline.getStats();
    std::cout << "Optimization completed in " 
              << stats.optimizationTimeMs << "ms\n";
    std::cout << "Functions inlined: " 
              << stats.ipoStats.inlinedFunctions << "\n";
    std::cout << "Loops vectorized: " 
              << stats.loopStats.vectorizedLoops << "\n";
}
```

### Compiler Integration

Add to your compiler class:

```cpp
#include "compiler/advanced_optimizations.h"

class Compiler {
private:
    std::unique_ptr<optimization::AdvancedOptimizationPipeline> optimizer_;
    
public:
    Compiler() {
        optimizer_ = std::make_unique<optimization::AdvancedOptimizationPipeline>();
    }
    
    void compile(const std::string& source, int optLevel) {
        // ... parse and generate IR ...
        
        // Apply advanced optimizations
        optimizer_->setOptimizationLevel(optLevel);
        optimizer_->optimize(module.get());
        
        // ... generate code ...
    }
};
```

## Lightweight Goroutine Scheduler

### Basic Usage in Tocin

```to
// Import concurrency module
import runtime.concurrency;

// Launch goroutines
go {
    print("Goroutine 1");
}

go {
    print("Goroutine 2");
}

// Launch goroutine with parameters
func processData(id: int, data: string) {
    print("Processing " + data + " in goroutine " + id);
}

for i in 0..1000 {
    go processData(i, "data" + i);
}
```

### Advanced Usage in C++

```cpp
#include "runtime/lightweight_scheduler.h"

using namespace tocin::runtime;

void useLightweightScheduler() {
    // Create scheduler with 8 worker threads
    LightweightScheduler scheduler(8);
    
    // Configure scheduler
    scheduler.setFiberStackSize(8192);  // 8KB per fiber
    scheduler.start();
    
    // Launch goroutines
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 1000000; i++) {
        scheduler.go([&counter, i]() {
            counter++;
            // Do work
        });
    }
    
    // Or use singleton
    auto& globalScheduler = LightweightScheduler::instance();
    globalScheduler.go([]() {
        // Work
    });
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Get statistics
    auto stats = scheduler.getStats();
    std::cout << "Completed: " << stats.completedFibers << "\n";
    std::cout << "Average time: " << stats.averageFiberTimeMs << "ms\n";
    
    scheduler.stop();
}
```

### Integration with Existing Runtime

```cpp
// In your runtime initialization
void Runtime::initialize() {
    // Start global scheduler
    auto& scheduler = LightweightScheduler::instance();
    scheduler.setMaxWorkers(std::thread::hardware_concurrency());
    scheduler.start();
}

// In your go statement handler
void Runtime::executeGoStatement(ast::GoStatement* stmt) {
    auto& scheduler = LightweightScheduler::instance();
    
    scheduler.go([stmt, this]() {
        // Execute the statement in a fiber
        this->execute(stmt->body);
    });
}
```

## Examples

### Complete Integration Example

```cpp
#include "v8_integration/v8_runtime.h"
#include "compiler/advanced_optimizations.h"
#include "runtime/lightweight_scheduler.h"

class AdvancedTocinRuntime {
private:
    std::unique_ptr<v8_integration::V8Runtime> v8_;
    std::unique_ptr<optimization::AdvancedOptimizationPipeline> optimizer_;
    LightweightScheduler& scheduler_;
    
public:
    AdvancedTocinRuntime() 
        : scheduler_(LightweightScheduler::instance()) {
        
        // Initialize V8
        v8_ = std::make_unique<v8_integration::V8Runtime>();
        v8_->initialize();
        
        // Initialize optimizer
        optimizer_ = std::make_unique<optimization::AdvancedOptimizationPipeline>();
        optimizer_->setOptimizationLevel(3);
        
        // Start scheduler
        scheduler_.start();
    }
    
    void executeJavaScript(const std::string& code) {
        // Execute in a goroutine
        scheduler_.go([this, code]() {
            auto result = v8_->executeCode(code);
            if (v8_->hasError()) {
                std::cerr << "JS Error: " << v8_->getLastError() << "\n";
            }
        });
    }
    
    void compileOptimized(llvm::Module* module) {
        optimizer_->optimize(module);
    }
    
    ~AdvancedTocinRuntime() {
        scheduler_.stop();
        v8_->shutdown();
    }
};
```

### Tocin Language Example

```to
// Complete example using all advanced features

import ffi.javascript;
import runtime.concurrency;

// Use JavaScript for complex calculations
ffi.javascript.eval("""
    function complexCalculation(data) {
        return data.map(x => Math.sin(x) * Math.cos(x))
                   .reduce((a, b) => a + b, 0);
    }
""");

// Process data concurrently
func processDataset(dataset: []float) -> float {
    let results: []float = [];
    
    // Launch goroutine for each chunk
    for chunk in dataset.chunks(1000) {
        go {
            let jsResult = ffi.javascript.call("complexCalculation", [chunk]);
            results.append(jsResult);
        }
    }
    
    // Wait for all results
    sleep(1000);
    
    return results.sum();
}

// Main program
func main() {
    let data = generate_random_data(1000000);
    let result = processDataset(data);
    print("Final result: " + result);
}
```

## Performance Tuning

### V8 Optimization Tips

1. **Warm up the V8 engine**: Execute code multiple times for JIT optimization
2. **Reuse contexts**: Don't create new contexts frequently
3. **Minimize type conversions**: Batch operations when possible

### Compiler Optimization Tips

1. **Use PGO for production**: Profile your application and rebuild with PGO
2. **Enable LTO**: For final release builds
3. **Tune optimization level**: Start with -O2, use -O3 for performance-critical code

### Scheduler Optimization Tips

1. **Match workers to CPU cores**: Use `std::thread::hardware_concurrency()`
2. **Tune fiber stack size**: Larger stacks for deep recursion, smaller for many fibers
3. **Batch small operations**: Don't create goroutines for tiny tasks

### Memory Optimization

```cpp
// Configure for high concurrency
scheduler.setMaxWorkers(16);
scheduler.setFiberStackSize(4096);  // 4KB - can support millions of fibers

// For lower concurrency but more stack space
scheduler.setMaxWorkers(4);
scheduler.setFiberStackSize(16384);  // 16KB
```

## Troubleshooting

### V8 Not Found
```
CMake Error: Could not find V8
```
**Solution**: Install V8 or build with `-DWITH_V8=OFF`

### Scheduler Crashes
**Issue**: Stack overflow in fibers
**Solution**: Increase fiber stack size:
```cpp
scheduler.setFiberStackSize(8192);  // 8KB or more
```

### Optimization Failures
**Issue**: Module corruption during optimization
**Solution**: Reduce optimization level or disable specific passes:
```cpp
pipeline.setOptimizationLevel(2);  // Instead of 3
pipeline.enablePolyhedral(false);  // If polyhedral causes issues
```

## Testing

Run comprehensive tests:
```bash
cd build

# V8 tests
./tests/v8_integration/test_v8

# Optimization tests
./tests/optimization/test_optimizations

# Scheduler tests
./tests/scheduler/test_scheduler

# Benchmarks
./benchmarks/advanced_features_bench
```

## Further Reading

- [V8 Integration Roadmap](V8_INTEGRATION_ROADMAP.md)
- [Advanced Features Documentation](ADVANCED_FEATURES.md)
- [Architecture Overview](ARCHITECTURE.md)
- [Performance Guide](PERFORMANCE.md)

## Support

For issues, questions, or contributions:
- Open an issue on GitHub
- Check documentation in `docs/`
- Review test files for examples

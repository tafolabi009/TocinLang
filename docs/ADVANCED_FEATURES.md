# Advanced Features Implementation Guide

This document describes the newly implemented advanced features in the Tocin compiler.

## V8 JavaScript Engine Integration

### Overview
The V8 integration provides full JavaScript code execution capability within Tocin programs, enabling seamless interoperability between Tocin and JavaScript.

### Location
- **Header**: `src/v8_integration/v8_runtime.h`
- **Implementation**: `src/v8_integration/v8_runtime.cpp`
- **Tests**: `tests/v8_integration/test_v8_runtime.cpp`

### Features
- JavaScript code execution
- Bidirectional type conversion (Tocin ↔ V8)
- Function calling between Tocin and JavaScript
- Error handling and reporting
- Module loading (in progress)

### Usage Example
```cpp
#include "v8_integration/v8_runtime.h"

using namespace tocin::v8_integration;

int main() {
    V8Runtime runtime;
    if (runtime.initialize()) {
        // Execute JavaScript code
        auto result = runtime.executeCode("2 + 3");
        
        // Call JavaScript function
        runtime.executeCode("function add(a, b) { return a + b; }");
        auto sum = runtime.callFunction("add", {FFIValue(10), FFIValue(20)});
        
        runtime.shutdown();
    }
    return 0;
}
```

### Configuration
Enable V8 integration during build:
```bash
cmake -DWITH_V8=ON ..
```

## Advanced Optimizations

### Overview
Comprehensive optimization pipeline including Profile-Guided Optimization (PGO), Interprocedural Optimization (IPO), Polyhedral loop transformations, and Whole-Program Optimization (WPO).

### Location
- **Header**: `src/compiler/advanced_optimizations.h`
- **Implementation**: `src/compiler/advanced_optimizations.cpp`
- **Tests**: `tests/optimization/test_advanced_optimizations.cpp`

### Components

#### 1. Profile-Guided Optimization (PGO)
Collects runtime profiling data to guide optimizations based on actual usage patterns.

```cpp
PGOManager pgo;
pgo.enableProfiling(module);
pgo.applyPGO(module);
```

#### 2. Interprocedural Optimization (IPO)
Performs optimizations across function boundaries including:
- Function inlining
- Devirtualization
- Constant propagation

```cpp
InterproceduralOptimizer ipo;
ipo.optimizeCallGraph(module);
ipo.performInlining(module, 225);
```

#### 3. Polyhedral Loop Optimization
Advanced loop transformations:
- Loop fusion
- Loop tiling for cache optimization
- Loop interchange
- Automatic vectorization
- Parallel loop detection

```cpp
PolyhedralOptimizer poly;
poly.applyLoopTiling(module, 64);
poly.applyVectorization(module);
```

#### 4. Whole-Program Optimization (WPO)
Link-time optimization and global optimizations:
- Dead code elimination
- Global value numbering
- Global variable optimization

```cpp
WholeProgramOptimizer wpo;
wpo.addModule(module);
wpo.setOptimizationLevel(3);
wpo.optimize();
```

### Optimization Pipeline
Orchestrates all optimization techniques:

```cpp
AdvancedOptimizationPipeline pipeline;
pipeline.setOptimizationLevel(3);
pipeline.enablePGO(true);
pipeline.enableIPO(true);
pipeline.enablePolyhedral(true);
pipeline.enableLTO(true);
pipeline.optimize(module);

auto stats = pipeline.getStats();
std::cout << "Optimization time: " << stats.optimizationTimeMs << "ms\n";
```

## Lightweight Goroutine Scheduler

### Overview
Fiber-based goroutine scheduler supporting millions of concurrent goroutines with ~4KB stack size per fiber instead of ~1MB for OS threads.

### Location
- **Header**: `src/runtime/lightweight_scheduler.h`
- **Implementation**: `src/runtime/lightweight_scheduler.cpp`
- **Tests**: `tests/scheduler/test_lightweight_scheduler.cpp`

### Features
- Fiber-based cooperative multitasking
- Work-stealing queue for load balancing
- Configurable number of worker threads
- Configurable fiber stack size
- Comprehensive statistics tracking

### Usage Example
```cpp
#include "runtime/lightweight_scheduler.h"

using namespace tocin::runtime;

int main() {
    // Create scheduler with 4 worker threads
    LightweightScheduler scheduler(4);
    scheduler.start();
    
    // Launch goroutines
    for (int i = 0; i < 10000; i++) {
        scheduler.go([i]() {
            std::cout << "Goroutine " << i << "\n";
        });
    }
    
    // Or use the singleton
    auto& globalScheduler = LightweightScheduler::instance();
    globalScheduler.go([]() {
        // Do work
    });
    
    // Get statistics
    auto stats = scheduler.getStats();
    std::cout << "Active fibers: " << stats.activeFibers << "\n";
    std::cout << "Completed: " << stats.completedFibers << "\n";
    
    scheduler.stop();
    return 0;
}
```

### Architecture

#### Fiber
Lightweight execution context with small stack:
- ~4KB stack size (configurable)
- Cooperative scheduling
- Minimal overhead

#### Work-Stealing Queue
Lock-free queue for efficient task distribution:
- Owner can push/pop from bottom
- Other workers can steal from top
- Optimal load balancing

#### Worker Threads
OS threads that execute fibers:
- Configurable number of workers
- Automatic work stealing
- Statistics tracking per worker

### Configuration
```cpp
scheduler.setMaxWorkers(8);           // Set number of workers
scheduler.setFiberStackSize(8192);    // Set stack size per fiber
```

## Testing

### Running Tests

#### V8 Integration Tests
```bash
cd tests/v8_integration
g++ -std=c++17 test_v8_runtime.cpp ../../src/v8_integration/v8_runtime.cpp \
    -I../../src -lv8 -lpthread -o test_v8
./test_v8
```

#### Optimization Tests
```bash
cd tests/optimization
g++ -std=c++17 test_advanced_optimizations.cpp \
    ../../src/compiler/advanced_optimizations.cpp \
    -I../../src $(llvm-config --cxxflags --ldflags --libs) -o test_opt
./test_opt
```

#### Scheduler Tests
```bash
cd tests/scheduler
g++ -std=c++17 test_lightweight_scheduler.cpp \
    ../../src/runtime/lightweight_scheduler.cpp \
    -I../../src -lpthread -o test_sched
./test_sched
```

### Test Coverage
- V8 Integration: 10 comprehensive tests
- Advanced Optimizations: 10 optimization pass tests
- Lightweight Scheduler: 10 concurrency tests

## Performance Considerations

### V8 Integration
- Object caching for frequently used V8 objects
- Context reuse when possible
- Efficient type conversion between Tocin and V8

### Optimizations
- PGO can improve performance by 20-40%
- IPO reduces function call overhead
- Polyhedral optimizations improve loop performance by 2-10x
- LTO can reduce binary size by 10-30%

### Goroutine Scheduler
- 4KB per fiber vs 1MB per OS thread = 256x memory efficiency
- Support for millions of concurrent goroutines
- Work stealing achieves optimal load balancing
- Low scheduling overhead (~microseconds per context switch)

## Future Enhancements

### V8 Integration
- [ ] ES module system support
- [ ] Async/await bridge
- [ ] npm package integration
- [ ] Chrome DevTools integration
- [ ] WebAssembly support

### Optimizations
- [ ] Machine learning-based optimization selection
- [ ] GPU kernel optimization
- [ ] Advanced alias analysis
- [ ] Speculative optimization

### Scheduler
- [ ] Priority-based scheduling
- [ ] CPU affinity support
- [ ] NUMA-aware scheduling
- [ ] Preemptive scheduling option

## Contributing

When adding new features:
1. Follow the existing code structure
2. Add comprehensive tests
3. Update documentation
4. Run all tests before submitting
5. Provide usage examples

## License

See LICENSE file in the root directory.

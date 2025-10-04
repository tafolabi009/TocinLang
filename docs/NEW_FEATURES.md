# New Features Implementation Guide

This document describes the newly implemented advanced features in the Tocin compiler and runtime system.

## Overview

This implementation adds four major enhancements to the Tocin compiler:

1. **ES Module System Support** - Full ECMAScript module loading and execution
2. **Async/Await Bridge** - Seamless integration between Tocin and JavaScript promises
3. **Priority-Based Scheduling** - Advanced task scheduling with priority levels
4. **NUMA-Aware Scheduling** - Optimized scheduling for multi-socket systems

## 1. ES Module System Support

### Description

The V8 integration now supports loading and executing ECMAScript modules, enabling modern JavaScript module syntax with import/export statements.

### Key Features

- **Module Loading**: Load ES6 modules from file system
- **Module Caching**: Efficient caching of compiled modules
- **Import/Export**: Full support for import and export statements
- **Module Resolution**: Automatic module dependency resolution

### API Reference

```cpp
#include "v8_integration/v8_runtime.h"

using namespace tocin::v8_integration;

// Initialize V8 runtime
V8Runtime runtime;
runtime.initialize();

// Load an ES module
bool success = runtime.loadESModule("./mymodule.mjs", "mymodule");

// Import the module
ffi::FFIValue module = runtime.importModule("mymodule");

// Export values to JavaScript
runtime.exportValue("myValue", ffi::FFIValue(42));

runtime.shutdown();
```

### Example Module (mymodule.mjs)

```javascript
// mymodule.mjs
export function greet(name) {
    return `Hello, ${name}!`;
}

export const PI = 3.14159;

export class Calculator {
    add(a, b) {
        return a + b;
    }
}
```

### Usage from Tocin

```cpp
// Load and use the module
V8Runtime runtime;
runtime.initialize();

// Load module
runtime.loadESModule("./mymodule.mjs", "mymodule");

// Call exported function
runtime.executeCode("import { greet } from 'mymodule'; greet('World');");

runtime.shutdown();
```

## 2. Async/Await Bridge

### Description

Provides a bridge between Tocin's concurrency model and JavaScript's Promise-based async operations.

### Key Features

- **Promise Creation**: Create JavaScript promises from Tocin
- **Await Mechanism**: Wait for JavaScript promises to resolve
- **Timeout Support**: Optional timeouts for async operations
- **Error Handling**: Proper rejection and error propagation

### API Reference

```cpp
#include "v8_integration/v8_runtime.h"

using namespace tocin::v8_integration;

V8Runtime runtime;
runtime.initialize();

// Create a promise
auto result = runtime.createPromise([](auto resolve, auto reject) {
    // Simulate async operation
    std::this_thread::sleep_for(std::chrono::seconds(1));
    resolve(ffi::FFIValue("Success!"));
});

// Execute JavaScript that returns a promise
runtime.executeCode("const myPromise = Promise.resolve(42);");

// Await the promise (with optional timeout)
auto asyncResult = runtime.awaitPromise("myPromise", 5000); // 5 second timeout

if (asyncResult.isResolved) {
    std::cout << "Promise resolved with value" << std::endl;
} else if (asyncResult.isRejected) {
    std::cout << "Promise rejected: " << asyncResult.error << std::endl;
}

runtime.shutdown();
```

### Example: Async Operations

```cpp
// Create a promise that resolves after delay
auto createDelayedPromise = [&runtime](int ms, const std::string& value) {
    return runtime.createPromise([ms, value](auto resolve, auto reject) {
        std::thread([ms, value, resolve]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            resolve(ffi::FFIValue(value));
        }).detach();
    });
};

// Create multiple promises
auto p1 = createDelayedPromise(100, "First");
auto p2 = createDelayedPromise(200, "Second");

// Await both (implement Promise.all pattern)
```

## 3. Priority-Based Scheduling

### Description

The lightweight scheduler now supports priority-based task scheduling, allowing critical tasks to be executed before lower-priority tasks.

### Key Features

- **Priority Levels**: 5 priority levels (Critical, High, Normal, Low, Background)
- **Priority Queues**: Efficient priority-based task queuing
- **Priority-Aware Work Stealing**: Workers prefer high-priority tasks when stealing
- **Dynamic Priority**: Priority can be adjusted dynamically

### API Reference

```cpp
#include "runtime/lightweight_scheduler.h"

using namespace tocin::runtime;

// Get scheduler instance
auto& scheduler = LightweightScheduler::instance();
scheduler.start();

// Schedule tasks with different priorities
scheduler.goWithPriority(Fiber::Priority::Critical, []() {
    std::cout << "Critical task - runs first" << std::endl;
});

scheduler.goWithPriority(Fiber::Priority::Normal, []() {
    std::cout << "Normal task - runs after critical" << std::endl;
});

scheduler.goWithPriority(Fiber::Priority::Background, []() {
    std::cout << "Background task - runs when idle" << std::endl;
});

// Wait for all tasks to complete
scheduler.waitAll();
scheduler.stop();
```

### Priority Levels

| Priority | Value | Description |
|----------|-------|-------------|
| Critical | 0 | Highest priority, immediate execution |
| High | 1 | Important tasks, executed quickly |
| Normal | 2 | Default priority for regular tasks |
| Low | 3 | Lower priority, may be delayed |
| Background | 4 | Lowest priority, runs when idle |

### Example: Task Prioritization

```cpp
auto& scheduler = LightweightScheduler::instance();
scheduler.start();

// Critical system task
scheduler.goWithPriority(Fiber::Priority::Critical, []() {
    performSystemMaintenance();
});

// User interaction task
scheduler.goWithPriority(Fiber::Priority::High, []() {
    handleUserInput();
});

// Background processing
scheduler.goWithPriority(Fiber::Priority::Background, []() {
    processLogFiles();
});

scheduler.waitAll();
```

## 4. NUMA-Aware Scheduling

### Description

Optimizes task scheduling for Non-Uniform Memory Access (NUMA) architectures, improving performance on multi-socket systems.

### Key Features

- **NUMA Topology Detection**: Automatic detection of NUMA nodes
- **CPU Affinity**: Pin workers to specific CPUs
- **Node-Aware Distribution**: Distribute workers across NUMA nodes
- **Performance Optimization**: Reduce inter-node memory access latency

### API Reference

```cpp
#include "runtime/lightweight_scheduler.h"

using namespace tocin::runtime;

// Create scheduler with NUMA awareness
LightweightScheduler scheduler(std::thread::hardware_concurrency());

// Enable NUMA awareness
scheduler.enableNUMAAwareness(true);

// Manually set worker affinity (optional)
scheduler.setWorkerAffinity(0, 0, 0);  // Worker 0 on CPU 0, NUMA node 0
scheduler.setWorkerAffinity(1, 1, 0);  // Worker 1 on CPU 1, NUMA node 0
scheduler.setWorkerAffinity(2, 16, 1); // Worker 2 on CPU 16, NUMA node 1

scheduler.start();

// Schedule tasks - they will be distributed across NUMA nodes
scheduler.go([]() {
    // Task automatically scheduled to optimal NUMA node
    processData();
});

scheduler.waitAll();
scheduler.stop();
```

### NUMA Architecture Support

The scheduler automatically detects NUMA topology on:

- **Linux**: Reads from `/sys/devices/system/node`
- **Windows**: Uses `GetLogicalProcessorInformationEx`
- **Other Platforms**: Defaults to single-node configuration

### Performance Tips

1. **Enable NUMA Awareness**: Call `enableNUMAAwareness(true)` for multi-socket systems
2. **Worker Count**: Use worker count equal to physical cores
3. **Memory Allocation**: Allocate memory on the same NUMA node as the worker
4. **Data Locality**: Keep related tasks on the same NUMA node

### Example: NUMA-Optimized Processing

```cpp
auto& scheduler = LightweightScheduler::instance();
scheduler.enableNUMAAwareness(true);
scheduler.start();

// Get statistics to see NUMA configuration
auto stats = scheduler.getStats();
std::cout << "NUMA Nodes: " << stats.numNUMANodes << std::endl;
std::cout << "Workers: " << stats.totalWorkers << std::endl;

// Schedule data processing tasks
// High-priority tasks go to NUMA node 0
scheduler.goWithPriority(Fiber::Priority::High, []() {
    processHighPriorityData();
});

// Normal tasks distributed across all nodes
for (int i = 0; i < 100; i++) {
    scheduler.go([i]() {
        processDataChunk(i);
    });
}

scheduler.waitAll();
```

## Combined Usage Example

Here's an example combining all four features:

```cpp
#include "v8_integration/v8_runtime.h"
#include "runtime/lightweight_scheduler.h"

int main() {
    // Initialize V8 runtime
    tocin::v8_integration::V8Runtime runtime;
    runtime.initialize();
    
    // Initialize scheduler with NUMA awareness
    auto& scheduler = tocin::runtime::LightweightScheduler::instance();
    scheduler.enableNUMAAwareness(true);
    scheduler.start();
    
    // Load ES module
    runtime.loadESModule("./worker.mjs", "worker");
    
    // Schedule high-priority JavaScript execution
    scheduler.goWithPriority(tocin::runtime::Fiber::Priority::High, [&runtime]() {
        // Execute JavaScript code
        auto result = runtime.executeCode(
            "import { processData } from 'worker'; processData([1,2,3]);"
        );
    });
    
    // Create async operation with promise
    auto asyncResult = runtime.createPromise([](auto resolve, auto reject) {
        // Simulate async work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        resolve(ffi::FFIValue("Async complete"));
    });
    
    // Schedule background task
    scheduler.goWithPriority(tocin::runtime::Fiber::Priority::Background, []() {
        std::cout << "Background cleanup" << std::endl;
    });
    
    // Wait for all tasks
    scheduler.waitAll();
    
    // Cleanup
    scheduler.stop();
    runtime.shutdown();
    
    return 0;
}
```

## Performance Considerations

### V8 Integration

- Module loading has one-time compilation cost
- Keep modules cached for repeated use
- Minimize context switches between Tocin and JavaScript

### Async/Await

- Promises have overhead, use for I/O-bound operations
- Prefer Tocin's lightweight fibers for CPU-bound tasks
- Set appropriate timeouts to avoid indefinite waits

### Priority Scheduling

- Too many priority levels can increase overhead
- Use Normal priority for most tasks
- Reserve Critical for time-sensitive operations

### NUMA Awareness

- Greatest benefit on systems with 2+ sockets
- Negligible overhead on single-socket systems
- Monitor NUMA statistics to verify optimization

## Troubleshooting

### V8 Module Loading Fails

**Problem**: Module fails to load with compilation error

**Solution**: 
- Check module syntax is valid ES6
- Verify file path is correct
- Ensure module has proper import/export statements

### Promise Never Resolves

**Problem**: `awaitPromise` times out

**Solution**:
- Check promise executor calls resolve/reject
- Verify no deadlocks in async code
- Increase timeout value if needed

### Poor Priority Scheduling Performance

**Problem**: High-priority tasks delayed

**Solution**:
- Verify scheduler is started: `scheduler.start()`
- Check worker count is adequate
- Monitor with `getStats()` to see queue depth

### NUMA Not Detected

**Problem**: `numNUMANodes` returns 1 on multi-socket system

**Solution**:
- On Linux: Check `/sys/devices/system/node` exists
- On Windows: Verify running on Windows Server
- May need administrator privileges

## Build Configuration

To use these features, ensure the following CMake options:

```bash
cmake -DWITH_V8=ON \        # Enable V8 integration (optional)
      -DWITH_ASYNC=ON \      # Enable async/await (enabled by default)
      -DCMAKE_BUILD_TYPE=Release \
      ..
```

The scheduler features are always available and don't require special build flags.

## Testing

Run the test suites to verify the implementations:

```bash
# Build tests
cd build
make

# Run scheduler tests
./tests/scheduler/test_lightweight_scheduler

# Run V8 integration tests (if V8 enabled)
./tests/v8_integration/test_v8_runtime
```

## See Also

- [V8 Integration Roadmap](V8_INTEGRATION_ROADMAP.md)
- [Advanced Features Documentation](ADVANCED_FEATURES.md)
- [Runtime System Documentation](04_Runtime_System.md)
- [Performance Optimization Guide](PERFORMANCE_OPTIMIZATION.md)

## Version Information

- **Implementation Date**: January 2025
- **Tocin Version**: 1.0+
- **V8 Version**: 11.0+ (if enabled)
- **Supported Platforms**: Linux, Windows, macOS

## License

Same as Tocin compiler - see LICENSE file.

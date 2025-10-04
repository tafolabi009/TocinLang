// Performance Benchmarks for Advanced Features

#include "../../src/v8_integration/v8_runtime.h"
#include "../../src/compiler/advanced_optimizations.h"
#include "../../src/runtime/lightweight_scheduler.h"
#include <iostream>
#include <chrono>
#include <vector>

using namespace std::chrono;

class BenchmarkTimer {
public:
    BenchmarkTimer(const std::string& name) : name_(name) {
        start_ = high_resolution_clock::now();
    }
    
    ~BenchmarkTimer() {
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start_);
        std::cout << name_ << ": " << duration.count() << "ms\n";
    }
    
private:
    std::string name_;
    high_resolution_clock::time_point start_;
};

// V8 Integration Benchmarks
void benchmarkV8Execution() {
    std::cout << "\n=== V8 JavaScript Execution Benchmarks ===\n";
    
#ifdef WITH_V8
    using namespace tocin::v8_integration;
    V8Runtime runtime;
    runtime.initialize();
    
    // Benchmark 1: Simple arithmetic
    {
        BenchmarkTimer timer("V8: 10,000 arithmetic operations");
        for (int i = 0; i < 10000; i++) {
            runtime.executeCode("2 + 3 * 4 - 1");
        }
    }
    
    // Benchmark 2: Function calls
    runtime.executeCode("function fib(n) { return n <= 1 ? n : fib(n-1) + fib(n-2); }");
    {
        BenchmarkTimer timer("V8: 100 fibonacci(15) calls");
        for (int i = 0; i < 100; i++) {
            runtime.callFunction("fib", {tocin::ffi::FFIValue(15)});
        }
    }
    
    // Benchmark 3: Array operations
    {
        BenchmarkTimer timer("V8: 1,000 array operations");
        for (int i = 0; i < 1000; i++) {
            runtime.executeCode("[1,2,3,4,5].map(x => x * 2).reduce((a,b) => a + b, 0)");
        }
    }
    
    runtime.shutdown();
#else
    std::cout << "V8 support not enabled. Skipping V8 benchmarks.\n";
#endif
}

// Optimization Pipeline Benchmarks
void benchmarkOptimizations() {
    std::cout << "\n=== Optimization Pipeline Benchmarks ===\n";
    
    // Create test modules
    std::vector<std::unique_ptr<llvm::Module>> modules;
    llvm::LLVMContext context;
    
    for (int i = 0; i < 10; i++) {
        auto module = std::make_unique<llvm::Module>("bench_module", context);
        modules.push_back(std::move(module));
    }
    
    using namespace tocin::optimization;
    
    // Benchmark 1: IPO
    {
        InterproceduralOptimizer ipo;
        BenchmarkTimer timer("IPO: 10 modules");
        for (auto& module : modules) {
            ipo.optimizeCallGraph(module.get());
        }
    }
    
    // Benchmark 2: Polyhedral optimization
    {
        PolyhedralOptimizer poly;
        BenchmarkTimer timer("Polyhedral: 10 modules");
        for (auto& module : modules) {
            poly.analyzeLoops(module.get());
            poly.applyVectorization(module.get());
        }
    }
    
    // Benchmark 3: Full pipeline
    {
        AdvancedOptimizationPipeline pipeline;
        pipeline.setOptimizationLevel(3);
        pipeline.enableIPO(true);
        pipeline.enablePolyhedral(true);
        
        BenchmarkTimer timer("Full pipeline: 10 modules");
        for (auto& module : modules) {
            pipeline.optimize(module.get());
        }
    }
}

// Goroutine Scheduler Benchmarks
void benchmarkScheduler() {
    std::cout << "\n=== Lightweight Scheduler Benchmarks ===\n";
    
    using namespace tocin::runtime;
    
    // Benchmark 1: Launch many goroutines
    {
        LightweightScheduler scheduler(8);
        scheduler.start();
        
        std::atomic<int> counter{0};
        const int numGoroutines = 100000;
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < numGoroutines; i++) {
            scheduler.go([&counter]() {
                counter++;
            });
        }
        
        // Wait for completion
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        
        std::cout << "Launched " << numGoroutines << " goroutines in " 
                  << duration.count() << "ms\n";
        std::cout << "Rate: " << (numGoroutines * 1000 / duration.count()) 
                  << " goroutines/sec\n";
        std::cout << "Completed: " << counter.load() << "\n";
        
        scheduler.stop();
    }
    
    // Benchmark 2: Concurrent work
    {
        LightweightScheduler scheduler(8);
        scheduler.start();
        
        std::atomic<int> sum{0};
        const int numGoroutines = 10000;
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < numGoroutines; i++) {
            scheduler.go([&sum, i]() {
                sum += i;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            });
        }
        
        // Wait for completion
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        
        std::cout << "\nConcurrent work with " << numGoroutines << " goroutines: "
                  << duration.count() << "ms\n";
        
        auto stats = scheduler.getStats();
        std::cout << "Completed fibers: " << stats.completedFibers << "\n";
        
        scheduler.stop();
    }
    
    // Benchmark 3: Work stealing efficiency
    {
        LightweightScheduler scheduler(4);
        scheduler.start();
        
        std::atomic<int> counter{0};
        const int numGoroutines = 50000;
        
        auto start = high_resolution_clock::now();
        
        // Create unbalanced workload
        for (int i = 0; i < numGoroutines; i++) {
            scheduler.go([&counter, i]() {
                if (i % 10 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(500));
                }
                counter++;
            });
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        
        std::cout << "\nWork-stealing with unbalanced load: " << duration.count() << "ms\n";
        std::cout << "Processed: " << counter.load() << " / " << numGoroutines << "\n";
        
        scheduler.stop();
    }
}

// Memory efficiency comparison
void benchmarkMemoryEfficiency() {
    std::cout << "\n=== Memory Efficiency Comparison ===\n";
    
    using namespace tocin::runtime;
    
    const size_t fiberStackSize = 4096;  // 4KB
    const size_t threadStackSize = 1048576;  // 1MB (typical OS thread)
    
    const int numFibers = 1000000;  // 1 million
    
    size_t fiberMemory = numFibers * fiberStackSize;
    size_t threadMemory = numFibers * threadStackSize;
    
    std::cout << "Memory for " << numFibers << " concurrent tasks:\n";
    std::cout << "  Fibers (4KB each): " << (fiberMemory / 1024 / 1024) << " MB\n";
    std::cout << "  Threads (1MB each): " << (threadMemory / 1024 / 1024) << " MB\n";
    std::cout << "  Memory savings: " << (threadMemory / fiberMemory) << "x\n";
}

int main() {
    std::cout << "=== Tocin Advanced Features Benchmarks ===\n";
    std::cout << "Running comprehensive performance tests...\n";
    
    benchmarkV8Execution();
    benchmarkOptimizations();
    benchmarkScheduler();
    benchmarkMemoryEfficiency();
    
    std::cout << "\n=== Benchmarks Complete ===\n";
    return 0;
}

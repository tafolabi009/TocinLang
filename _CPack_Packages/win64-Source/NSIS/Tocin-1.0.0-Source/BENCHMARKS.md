# Tocin Benchmarks

This directory contains sample programs for benchmarking the Tocin compiler and runtime. Use these to measure compilation time, runtime performance, and the efficiency of language features.

## How to Run Benchmarks

1. **Build the compiler in release mode:**
   ```bash
   mkdir build && cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   cmake --build .
   ```
2. **Run a benchmark:**
   ```bash
   ./tocin ../benchmarks/benchmark_compile_large.to
   ./tocin ../benchmarks/benchmark_runtime_linq.to --jit
   ./tocin ../benchmarks/benchmark_runtime_concurrency.to --jit
   ./tocin ../benchmarks/benchmark_runtime_ffi.to --jit
   ```
   - Use `time` or your platform's timing tool to measure execution time.

## Interpreting Results
- **Compilation time:** How long it takes to compile large or complex programs.
- **Runtime performance:** How fast key features (LINQ, concurrency, FFI) execute.
- **Compare results** before and after optimizations, or against other languages.

## Adding New Benchmarks
- Add a `.to` file to the `benchmarks/` directory.
- Focus on a specific feature or pattern (e.g., recursion, trait dispatch, stdlib usage).
- Document the purpose and expected behavior in comments at the top of the file.

## Sample Output
```
Total: 2692537
LINQ result: 1125000000
Concurrency total: 2499950000
FFI total: 400000
```

## Best Practices
- Run benchmarks on a quiet system for consistent results.
- Use release builds for accurate performance measurement.
- Profile and optimize hot spots based on benchmark results.

See also: [Performance Best Practices](docs/PERFORMANCE.md) 
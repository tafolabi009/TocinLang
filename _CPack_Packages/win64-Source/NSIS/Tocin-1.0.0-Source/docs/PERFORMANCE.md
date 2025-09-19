# Performance Best Practices in Tocin

Writing fast, efficient Tocin code is easy with the right techniques. Here are some tips and best practices for getting the best performance from your programs.

## General Tips
- **Prefer immutable data** where possible for easier reasoning and optimization.
- **Use built-in types and stdlib functions**—they are highly optimized.
- **Minimize allocations** in tight loops (reuse objects, use preallocated lists).
- **Avoid deep recursion** unless tail call optimization is guaranteed.

## Collections and LINQ
- **Chain LINQ operations** efficiently; avoid unnecessary intermediate collections.
- **Use `where` before `select`** to filter early and reduce work.
- **Prefer `for` loops** for simple, hot-path iteration.

## Traits and Dispatch
- **Use traits for shared behavior, not for data.**
- **Avoid excessive trait bounds** in hot code paths.

## Null Safety
- **Check for null only when necessary.**
- **Use non-nullable types** for performance-critical fields.

## Concurrency
- **Use channels and goroutines** for parallelism, but avoid oversubscription.
- **Batch work** to reduce synchronization overhead.

## FFI
- **Minimize FFI calls** in hot loops (prefer native Tocin code for tight loops).
- **Batch FFI operations** when possible.

## Compiler and Build
- **Build in release mode** (`-O2`/`-O3`) for best performance.
- **Profile your code** using the provided benchmarks and system profilers.

## Troubleshooting
- **Unexpected slowness:** Profile to find bottlenecks.
- **High memory usage:** Check for unnecessary allocations or large data structures.
- **FFI slowdowns:** Minimize cross-language calls in performance-critical code.

See also: [BENCHMARKS.md](../../BENCHMARKS.md) 
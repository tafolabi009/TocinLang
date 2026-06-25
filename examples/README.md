# Tocin examples

These programs compile and run with the current compiler. Run any of them with:

```bash
./build/tocin examples/hello.to --run                 # JIT-execute
./build/tocin examples/hello.to -o hello && ./hello   # native binary
```

| File | Demonstrates |
|------|--------------|
| `hello.to` | The basics: `main`, `println` |
| `fib.to` | Recursion, type inference, range `for`, formatted output |
| `fizzbuzz.to` | `if/elif/else`, `for`, modulo |
| `point.to` | Classes: fields, methods, `self`, construction, mutation |
| `strings.to` | String concatenation and formatting |

## `roadmap/`

The programs under [`roadmap/`](roadmap/) showcase the language's intended
direction (traits, generics, concurrency, GUI/ML/web stdlib, ...). They use
syntax and features that are **not yet implemented** and therefore do not compile
today — see the [Roadmap section of the main README](../README.md#roadmap). They
are kept as design targets, not as working code.

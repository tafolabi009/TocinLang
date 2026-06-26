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
| `first_class_functions.to` | Closures, capturing, functions as values |
| `expr_evaluator.to` | Recursive-descent arithmetic evaluator (cursor class) |
| `linked_list.to` | A recursive data structure |
| `adt_interpreter.to` | **Algebraic data types** + exhaustive `match` (a recursive AST) |
| `stack_vm.to` | **Capstone:** a bytecode stack VM — ADTs, `match`, vectors, coercion |
| `json_parser.to` | A JSON parser written in Tocin (ADTs + recursion + vectors) |
| `tuples.to` | **Tuples** and multiple return values; destructuring |
| `iterators.to` | The **iterator protocol** (`next(self) -> Option`) |
| `generators.to` | **Generators** (`yield`) — finite sequences driven by `for x in gen()` |
| `byref_closures.to` | **By-reference closures** — a closure mutates a captured local |
| `async_await.to` | **async/await** — async functions and awaiting their results |
| `tcp_echo.to` | **TCP networking** + `go` goroutines (a loopback server/client) |
| `operator_overload.to` | Operator overloading via dunder methods |
| `raii_destructor.to` | Deterministic cleanup with `__del__` + `defer` |
| `borrow_check.to` | The opt-in `--borrow-check` move + `&`/`&mut` borrow analysis |
| `freestanding_kernel.to` | `--freestanding` systems code (no libc/GC/runtime) |
| `defer_cleanup.to` | `defer` (LIFO cleanup at function return) |
| `concurrency.to` | `go` goroutines, typed channels, `select` |

## `roadmap/`

The programs under [`roadmap/`](roadmap/) showcase the language's intended
direction (traits, generics, concurrency, GUI/ML/web stdlib, ...). They use
syntax and features that are **not yet implemented** and therefore do not compile
today — see the [Roadmap section of the main README](../README.md#roadmap). They
are kept as design targets, not as working code.

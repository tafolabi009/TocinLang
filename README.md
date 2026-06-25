# Tocin — a statically-typed, LLVM-compiled programming language

[![CI Status](https://github.com/tafolabi009/tocin-compiler/workflows/CI/badge.svg)](https://github.com/tafolabi009/tocin-compiler/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-blue)](https://github.com/tafolabi009/tocin-compiler)

> Tocin is a statically-typed, compiled programming language with type inference,
> classes, and an LLVM backend. Programs can be **JIT-executed** for fast iteration
> or **compiled ahead-of-time to native executables**.

This README describes what the compiler **actually does today**. Longer-term,
aspirational features are collected in the [Roadmap](#roadmap) so it is always
clear what is implemented versus planned.

## Highlights (implemented and tested)

- **Compiles to native code via LLVM 18** — every example below produces a real
  ELF/Mach-O/PE executable, or can be JIT-executed in-process.
- **Type inference** — variable and function return types are inferred when not
  annotated; a real type checker reports genuine type errors.
- **Functions** — parameters, recursion, mutual recursion, and use-before-definition
  (declarations are order-independent via two-pass code generation).
- **Control flow** — `if / elif / else`, `while`, range-based `for i in a..b`, and
  `match` / `case` / `default`.
- **Error handling** — `throw`, `try` / `catch (e)` / `finally`; exceptions unwind
  across function calls (setjmp/longjmp runtime) in both JIT and native builds.
- **Null safety** — safe navigation `a?.b`, elvis/coalescing `a ?: b`, and
  force-unwrap `a!!` over nullable references.
- **Option / Result** — `Some`/`None`/`Ok`/`Err` with destructuring `match`
  patterns (`case Some(x)`, `case Err(e)`).
- **Enums** — `enum Color { Red, Green, Blue }` with auto-incrementing integer
  constants, usable bare or qualified (`Color.Red`).
- **Classes / structs** — fields, methods with `self`, construction, field read and
  mutation, and method calls.
- **Traits & impl** — `trait` interfaces and `impl [Trait for] Type` blocks with
  per-type method dispatch.
- **Generics** — `def f<T>(...)` functions and `class C<T>` classes, both
  monomorphized per concrete type (type arguments inferred at the call/constructor
  site).
- **Arrays** — array literals, indexing (read/write), `len`, iteration, and arrays
  as reference-typed function parameters.
- **Concurrency** — `go f(args)` goroutines (real OS threads), typed `channel<T>`
  send/receive, and `select` over multiple channels, in both JIT and native builds.
- **Modules** — `import a.b.c` / `import "path"` resolves and merges other `.to`
  files; a small standard library ships in `stdlib/std/` (math, list, LINQ-style
  collection ops).
- **Macros** — function-like `macro name(params) { body }` expanded at the token
  level before parsing; invoked as `name!(args)` with precedence-safe, composable
  expansion.
- **C FFI** — call C library functions via `extern def name(...) -> T;`, resolved
  by the JIT and the native linker ([docs/ffi.md](docs/ffi.md)).
- **Strings** — string literals and concatenation with `+`.
- **Math builtins** — `sqrt`, `pow`, `abs`, `min`, `max`, `floor`, trig, and more.
- **Formatted output** — `print` / `println`, including `println("x = {}", x)`.

## Quick start

### Prerequisites

- LLVM 18 (development package, e.g. `llvm-18-dev` on Debian/Ubuntu)
- CMake ≥ 3.16 and a C++20 compiler (GCC 11+, Clang 14+)
- `zlib`, `libffi` (and optionally `libzstd`, `libxml2`, Python 3 for FFI)

On Debian/Ubuntu:

```bash
sudo apt-get install -y llvm-18-dev libffi-dev zlib1g-dev libzstd-dev libxml2-dev cmake ninja-build
```

### Build

```bash
git clone https://github.com/tafolabi009/tocin-compiler.git
cd tocin-compiler
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DWITH_V8=OFF -DLLVM_DIR=$(llvm-config-18 --cmakedir)
cmake --build build -j
```

This produces `build/tocin`.

### Hello, Tocin

```tocin
// hello.to
def main() {
    println("Hello from Tocin!");
    return 0;
}
```

```bash
./build/tocin hello.to --run          # JIT-compile and run immediately
./build/tocin hello.to -o hello       # compile to a native executable
./hello
```

## Language tour (all of this runs today)

```tocin
// Type inference: the return type of `add` is inferred as int.
def add(a: int, b: int) {
    return a + b;
}

// Recursion.
def fib(n: int) -> int {
    if n < 2 { return n; }
    return fib(n - 1) + fib(n - 2);
}

// Range-based loops and formatted printing.
def main() {
    let sum = 0;
    for i in 1..101 {
        sum = sum + i;
    }
    println("sum 1..100 = {}", sum);     // 5050
    println("fib(10) = {}", fib(10));    // 55

    // Strings concatenate with +.
    let who = "world";
    println("hello, " + who);

    return add(2, 3);                    // process exit code = 5
}
```

Classes with methods and `self`:

```tocin
class Point {
    x: int;
    y: int;
    def sum(self) -> int { return self.x + self.y; }
    def scaled(self, k: int) -> int { return (self.x + self.y) * k; }
}

def main() {
    let p = Point(3, 4);
    p.x = 10;                            // field mutation
    println("sum   = {}", p.sum());      // 14
    println("scale = {}", p.scaled(2));  // 28
    return p.sum();
}
```

Generics, traits, arrays, and the standard library:

```tocin
import std.list;                          // listSum, listMax, ...

def max<T>(a: T, b: T) -> T {             // monomorphized per type
    if a > b { return a; }
    return b;
}

trait Shape { def area(self) -> int; }
struct Square { side: int; }
impl Shape for Square {
    def area(self) -> int { return self.side * self.side; }
}

def main() {
    let nums = [3, 1, 4, 1, 5, 9];        // array literal
    println("len={} sum={} max={}", len(nums), listSum(nums), listMax(nums));
    println("max(2, 7) = {}", max(2, 7)); // generic
    let s = Square(5);
    println("area = {}", s.area());       // trait method = 25
    return s.area();
}
```

Generic classes (`class C<T>`), monomorphized per type argument:

```tocin
class Box<T> {
    value: T;
    def get(self) -> T { return self.value; }
    def set(self, v: T) { self.value = v; }
}

def main() {
    let bi = Box(42);                     // Box<int>
    bi.set(100);
    let bf = Box(1.5);                    // Box<float> — a separate instantiation
    println("{} {}", bi.get(), bf.get()); // 100 1.5
    return bi.get();
}
```

Goroutines and channels:

```tocin
def worker(ch: channel<int>, n: int) { ch <- n * n; }

def main() {
    let ch = channel<int>();
    for i in 1..6 { go worker(ch, i); }   // 5 goroutines (OS threads)
    let total = 0;
    for i in 0..5 { total = total + <-ch; }
    println("sum of squares = {}", total); // 55
    return total;
}
```

Function-like macros (`name!(args)`), expanded before parsing:

```tocin
macro square(x) { x * x }
macro hypot2(a, b) { square!(a) + square!(b) }   // macros can use macros

def main() {
    println("{}", square!(2 + 3));   // 25 — arguments are auto-parenthesized
    return hypot2!(3, 4);            // 25
}
```

Error handling with `throw` / `try` / `catch` / `finally`:

```tocin
def divide(a: int, b: int) -> int {
    if b == 0 { throw 1; }                 // unwinds to the nearest catch
    return a / b;
}

def main() {
    try {
        let r = divide(10, 0);
        println("never printed: {}", r);
    } catch (e) {
        println("error code {}", e);       // error code 1
    } finally {
        println("done");                   // always runs
    }
    return 0;
}
```

## How it works

```
Source (.to)
   → Lexer        (tokens, indentation/brace blocks, string + numeric literals)
   → Parser       (recursive-descent → AST)
   → Type checker (two-pass: hoist signatures, then infer & check bodies)
   → IR generator (LLVM IR; two-pass so declaration order doesn't matter)
   → { JIT (ORCv2 LLJIT)  |  AOT (TargetMachine → object → system linker) }
```

The driver (`src/main.cpp`) drives this pipeline. `--run` JIT-executes `main`
in-process; `-o <file>` selects output by extension (`.ll` IR, `.s` assembly,
`.o` object, anything else → a linked native executable).

## Project structure

```
tocin-compiler/
├── src/
│   ├── lexer/       # Tokenizer
│   ├── parser/      # Recursive-descent parser → AST
│   ├── ast/         # AST and type node definitions
│   ├── type/        # Type checker and (in-progress) advanced type features
│   ├── codegen/     # LLVM IR generation
│   ├── compiler/    # Compilation context, optimization, (in-progress) macros
│   ├── runtime/     # Scheduler, async, concurrency support (in-progress)
│   ├── ffi/         # Python / C / JavaScript FFI (see Roadmap)
│   └── error/       # Diagnostics
├── stdlib/          # Standard-library modules written in Tocin (in-progress)
├── tests/           # Unit tests and .to integration programs
├── examples/        # Example programs
└── docs/            # Language and design documentation
```

## Testing

```bash
# tocin_runtime_shared lets the .to runner (lli) resolve the __tocin_* runtime
# symbols used by concurrency and exception programs.
cmake --build build --target tocin tocin_tests tocin_runtime_shared
ctest --test-dir build --output-on-failure       # C++ unit tests
bash scripts/run_to_tests.sh                      # .to integration programs
```

## Roadmap

These features are planned or partially scaffolded but **not yet fully working
end-to-end**. They are listed honestly so expectations match reality:

- **Explicit generic type arguments** — generic functions and classes infer their
  type arguments from the call/constructor today; turbofish-style annotations
  (`Box<int>(...)`) and generic types in parameter/field signatures are pending.
- **Higher-order LINQ** — `where`/`select`/`aggregate` ship today as stdlib
  functions over int lists (`import std.linq`); arbitrary lambda predicates/maps
  need first-class function values (pending).
- **Nullable-type flow analysis** — the `?.`/`?:`/`!!` operators work today;
  static tracking of which references are nullable is pending.
- **`Option` / `Result` ergonomics** — `Some`/`None`/`Ok`/`Err` and `case`
  destructuring work today; exhaustiveness checking, `?` propagation and typed
  payloads (the bound value is an int slot) are pending.
- **`async` / `await`** — goroutines, channels and `select` work; structured
  async is pending.
- **Typed exceptions** — `throw` / `catch` carry an integer code today; throwing and
  binding arbitrary typed payloads (e.g. error objects) is pending.
- **Python / JavaScript FFI** — C FFI works via `extern`; deep CPython and V8
  integration is gated behind build flags (V8 is not bundled — it is not
  installable from standard package managers, so CI builds with `-DWITH_V8=OFF`).
- **Hygienic / AST macros** — function-like token macros work today (`name!(...)`);
  hygiene (auto-renaming macro-introduced bindings) and statement/item macros that
  expand to multiple declarations are future work.
- **WebAssembly target**, **package manager**, and an **interactive debugger**
  (behind their respective CMake flags).
- **Advanced optimization** — the standard LLVM `-O0..-O3` pipelines are wired and
  working today (e.g. constant folding); profile-guided / interprocedural /
  polyhedral passes in `src/compiler/advanced_optimizations.cpp` are not yet on
  the default path.

Contributions toward any of these are very welcome.

## Technical stack

- **Compiler**: C++20
- **Backend**: LLVM 18 (IR generation, ORCv2 JIT, TargetMachine AOT, optimization passes)
- **Build**: CMake ≥ 3.16
- **Optional**: Python 3 (FFI), libffi, zstd, libxml2

## License

MIT License — see [LICENSE](LICENSE).

## Contributing

Contributions are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md). When adding a
language feature, please include a `.to` test under `tests/` demonstrating it.

## Author

**Afolabi Oluwatosin**

- Website: [https://folabi.me](https://folabi.me)
- LinkedIn: [linkedin.com/in/tafolabi009](https://www.linkedin.com/in/tafolabi009)
- Email: [tafolabi009@gmail.com](mailto:tafolabi009@gmail.com)
- GitHub: [@tafolabi009](https://github.com/tafolabi009)

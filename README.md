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
- **Control flow** — `if / elif / else`, `while`, and range-based `for i in a..b`.
- **Classes / structs** — fields, methods with `self`, construction, field read and
  mutation, and method calls.
- **Strings** — string literals and concatenation with `+`.
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
cmake --build build --target tocin tocin_tests
ctest --test-dir build --output-on-failure       # C++ unit tests
bash scripts/run_to_tests.sh                      # .to integration programs
```

## Roadmap

These features are partially scaffolded in the source tree and/or planned, but are
**not yet fully working end-to-end**. They are listed here honestly so expectations
match reality:

- **Traits & `impl` blocks** — AST/parsing groundwork exists; resolution and codegen pending.
- **Generics** — generic syntax is recognized; monomorphization is pending.
- **Collections & LINQ** — arrays/lists with indexing, and `where`/`select`/`aggregate`.
- **Pattern matching** — `match` with destructuring patterns.
- **Null safety** — `?.`, `?:`, `!!` operators and nullable-type flow analysis.
- **`Option` / `Result`** — types are defined; exhaustiveness checking and ergonomics pending.
- **Concurrency** — `go`, channels, and `select` parse today; a fiber scheduler exists in
  `src/runtime` but is not yet wired to codegen. `async` / `await`.
- **FFI** — Python (CPython) and C/C++ (`dlopen`) backends are largely implemented;
  JavaScript is a stub. V8 is gated behind `-DWITH_V8=ON` and is **not** bundled
  (it is not installable from standard package managers), so CI builds with `-DWITH_V8=OFF`.
- **Macros** — a macro engine exists but is not yet invoked by the compile pipeline.
- **WebAssembly target**, **package manager**, and an **interactive debugger**
  (behind their respective CMake flags).
- **Advanced optimization** — the standard LLVM `-O0..-O3` pipelines are wired and
  working today (e.g. constant folding); profile-guided/interprocedural/polyhedral
  passes in `src/compiler/advanced_optimizations.cpp` are not yet on the default path.

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

# Tocin — a statically-typed, LLVM-compiled programming language

[![CI Status](https://github.com/tafolabi009/TocinLang/workflows/CI/badge.svg)](https://github.com/tafolabi009/TocinLang/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-blue)](https://github.com/tafolabi009/TocinLang)

> Tocin is a statically-typed, compiled programming language with type inference,
> classes, and an LLVM backend. Programs can be **JIT-executed** for fast iteration
> or **compiled ahead-of-time to native executables**.

This README describes what the compiler **actually does today**. Longer-term,
aspirational features are collected in the [Roadmap](#roadmap) so it is always
clear what is implemented versus planned.

## Install

Self-contained, per-user installers (compiler + runtime + standard library +
docs + examples + uninstaller, added to your `PATH`) — see
**[INSTALL.md](INSTALL.md)** for all platforms and options.

```bash
# Linux / macOS
curl -fsSL https://raw.githubusercontent.com/tafolabi009/TocinLang/master/installer/get-tocin.sh | sh
```
```powershell
# Windows (PowerShell)
irm https://raw.githubusercontent.com/tafolabi009/TocinLang/master/installer/install.ps1 | iex
```

The one-liners fetch a published GitHub Release. To build a package locally and
publish one, see [installer/README.md](installer/README.md).

## Highlights (implemented and tested)

- **Compiles to native code via LLVM (18–22)** — every example below produces a
  real ELF/Mach-O/PE executable, or can be JIT-executed in-process.
- **Type inference** — variable and function return types are inferred when not
  annotated; a real type checker reports genuine type errors.
- **Strict, readable diagnostics** — type checking is strict by default: unknown
  names, wrong argument counts (builtins included), and type mismatches stop
  compilation with rustc-style errors — the offending source line, a caret
  underline, colors on terminals, `did you mean 'sqrt'?` suggestions — and a
  final `N errors generated.` summary. `--permissive` downgrades them to
  warnings if you need the old behavior.
- **Functions** — parameters, recursion, mutual recursion, and use-before-definition
  (declarations are order-independent via two-pass code generation).
- **First-class functions & closures** — functions are values: pass them as
  arguments (`f: (int) -> int`), store them, return them, and call them
  indirectly; `lambda (x: int) -> int  x * 2` lambdas capture enclosing locals
  (**reads snapshot by value; writes share the cell**, so `counter = counter + 1`
  inside a lambda is visible outside) and can be returned from a function
  (escaping read-captures with independent state). Nested `def` helpers are
  also supported.
- **Control flow** — `if / elif / else`, `while`, range-based `for i in a..b`,
  `for v in arr`, `break` / `continue`, and `match` / `case` / `default`.
- **Operators** — full arithmetic with automatic int→float promotion, compound
  assignment (`+= -= *= /= %=`), bitwise & shifts (`& | ^ << >> ~`), logical
  (`&&`/`and`, `||`/`or`, `!`), and string equality by value (`s == "x"`).
  Integer literals support `0x`/`0o`/`0b` and `_` separators.
- **Ternary & default parameters** — conditional expressions
  (`let m = a > b ? a : b;`) and default parameter values
  (`def pad(s: string, width: int = 8)`), filled in per call site.
- **Error handling** — `throw`, `try` / `catch (e)` / `finally`; exceptions unwind
  across function calls (setjmp/longjmp runtime) in both JIT and native builds.
- **Runtime traps** — integer division/modulo by zero and out-of-bounds indexing
  never silently corrupt state: they abort with a located message, e.g.
  `panic: integer division by zero at app.to:4:16`.
- **Null safety** — safe navigation `a?.b`, elvis/coalescing `a ?: b`, and
  force-unwrap `a!!` over nullable references.
- **Option / Result** — `Some`/`None`/`Ok`/`Err` with destructuring `match`
  patterns (`case Some(x)`, `case Err(e)`).
- **Enums** — `enum Color { Red, Green, Blue }` with auto-incrementing integer
  constants, usable bare or qualified (`Color.Red`).
- **Algebraic enums (tagged unions)** — variants with typed payloads, including
  recursive ones: `enum Expr { Num(int), Add(Expr, Expr) }`. Destructure with
  `match` (`case Add(a, b)`), and the compiler **enforces exhaustiveness** — a
  match that misses a variant (with no `default:`) is a compile error. This is
  the canonical way to build an AST; see `examples/adt_interpreter.to`.
- **Tuples & multiple return** — `(a, b)` tuple literals, `def f() -> (int, int)`
  returning several values, positional access `t.0`/`t.1`, and destructuring
  `let (q, r) = divmod(a, b);`. A destructured literal keeps each element's type.
- **Classes / structs** — fields, methods with `self`, construction, field read and
  mutation, and method calls.
- **Traits & impl** — `trait` interfaces and `impl [Trait for] Type` blocks with
  per-type method dispatch.
- **Trait objects (dynamic dispatch)** — a value typed as a trait (a parameter,
  a `let`, or an element of `list<Trait>`) carries its concrete type and
  dispatches method calls virtually, so one collection can hold many concrete
  types: `let xs: list<Shape> = [Circle(5), Rect(2,3)]; for s in xs { s.area() }`.
  See `examples/trait_objects.to`.
- **Generic trait bounds** — `def f<T: Bound>(...)` is checked: a type argument
  that doesn't implement `Bound` is a compile error, and trait methods on the
  bounded parameter dispatch to the concrete type.
- **Generics** — `def f<T>(...)` functions and `class C<T>` classes, both
  monomorphized per concrete type (type arguments inferred at the call/constructor
  site).
- **Arrays** — array literals, indexing (read/write), `len`, iteration, and arrays
  as reference-typed function parameters.
- **Dynamic collections** — growable `vector` (`vecNew`/`vecPush`/`vecGet`/`vecLen`/…)
  and `hashmap` with int **and string** keys (`mapPutStr`/`mapGetStr`/…) — the
  building blocks for symbol tables and real data structures.
- **Concurrency** — `go f(args)` goroutines (real OS threads), typed `channel<T>`
  send/receive, and `select` over multiple channels, in both JIT and native builds.
- **Async/await** — `async def` + `await` compile and run with correct (eager)
  semantics and compose in expressions and across async calls; pair with
  goroutines + channels for real parallelism (a suspending M:N scheduler is
  designed but not wired in — see docs/async-scheduler-design.md).
- **Networking & system services** — TCP sockets (`tcpListen`/`tcpAccept`/
  `tcpConnect`/`tcpSend`/`tcpRecv`/`tcpClose`) for clients and concurrent servers,
  wall-clock + monotonic **time** (`timeSec`/`timeMs`/`monoNanos`/`sleepMs`),
  **hashing** (FNV-1a `hashStr`/`hashBytes`, splitmix64 `hashInt`), seeded
  **random** (`randSeed`/`randInt`/`randRange`), and `envGet`/`sysExit`. See
  `examples/tcp_echo.to`.
- **Modules & standard library** — `import a.b.c` / `import "path"` resolves and
  merges other `.to` files. The bundled stdlib spans **34 modules across 13
  domains**: `std` (math, strings, sequences, functional `map`/`filter`/`fold`
  with function-value callbacks, JSON, a test framework), `data` (algorithms +
  classic data structures), `math` (stats, linear algebra, geometry, calculus),
  `ml` (neural networks, deep learning, computer vision, and **TEN — Temporal
  Eigenstate Networks**), plus `web`, `net`, `database`, `game`, `gui`, `audio`,
  `embedded`, `scripting`, and `pkg`. Every module compiles and runs under
  `tocin --run` and is exercised by the JIT test suite (`tests/jit/`).
- **Macros** — function-like `macro name(params) { body }` expanded at the token
  level before parsing; invoked as `name!(args)` with precedence-safe, composable
  expansion.
- **C FFI** — call C library functions via `extern def name(...) -> T;`, resolved
  by the JIT and the native linker ([docs/ffi.md](docs/ffi.md)).
- **Strings** — literals, `+` concatenation, and a char-level library: `strLen`,
  `charAt`, `substring`, `strEq`/`strCmp`, `indexOfChar`, `intToStr`, `strToInt`,
  `charToStr`.
- **File I/O** — `readFile`, `writeFile`, `appendFile`, `readLine`.
- **Math & conversions** — `sqrt`, `pow`, `abs`, `min`, `max`, `floor`, trig, plus
  `intToFloat`/`floatToInt`/`floatToStr`/`strToFloat` and char predicates
  (`isDigit`, `isAlpha`, `isSpace`, `toUpperChar`, …) for lexers/parsers.
- **Garbage collection** — heap memory (arrays, strings, closures, collections,
  boxes) is reclaimed automatically via the Boehm collector; no manual frees
  needed, so long-running programs don't leak. `free`/`vecFree`/`mapFree` remain
  for eager release.
- **Systems / low-level** — raw memory (`alloc`, `memcpy`, `memset`, `ptrAdd`,
  `loadByte`/`storeByte`/`loadInt`/`storeInt`), **volatile MMIO accessors**
  (`volatileLoad8/16/32/64`, `volatileStore8/16/32/64` — never elided or merged,
  even at `-O3`), memory **`fence()`**, and **inline assembly with operands and
  constraints** — `asm("lea 100($1), $0", "=r,r", x)` returns a value, and
  clobbers work too (`asm("rdtsc", "={ax},~{dx}")`) — for VCS-style tools and
  OS/kernel work. See `tests/jit/kernel_primitives.to`.
- **`switch`** — C-style `switch`/`case`/`default` (alias of `match`).
- **`defer`** — `defer <statement>` runs cleanup at function return (LIFO, every
  return path); pairs with `free`/`vecFree` for deterministic resource release.
- **Operator overloading** — classes define `__add__`, `__eq__`, `__lt__`, … as
  ordinary methods; binary operators on instances dispatch to them.
- **RAII destructors** — a class with `__del__(self)` runs it automatically when
  a constructor-initialized local leaves scope (LIFO, every return path),
  for deterministic cleanup on top of the GC.
- **Freestanding / kernel mode** — `--freestanding` emits a relocatable object
  with no libc/GC/runtime (links with `-nostdlib`), for OS/kernel/bare-metal;
  inline `asm` and raw memory remain available.
- **Opt-in borrow checker** — `--borrow-check` adds Rust-like compile-time move /
  use-after-move enforcement on owned (class) values, on top of the GC's
  always-on safety. Off by default (instances alias freely under GC).
- **Native performance** — default `-O2`; `-O3 --native` adds CPU-specific
  vectorization (POPCNT/AVX), whole-program internalization, and alias-aware
  buffer optimization. On a 12-kernel C/C++/Rust comparison, Tocin's geomean
  **ties Rust** (~1.4× of C overall) and it is outright fastest on 5 of the 12
  kernels (e.g. `sqrtsum`, ~2× faster than the C version); `matmul` and
  `levenshtein` remain the known gaps.
- **Project tooling** — `tocin new <name>` scaffolds a project, `tocin check
  file.to` typechecks without generating code (fast pre-commit/CI gate), and
  `tocin doc file.to` emits Markdown API docs from signatures and the `//`
  comment blocks above declarations.
- **Formatted output** — `print` / `println`, including `println("x = {}", x)`.

## Documentation

Four guides cover the language from first program to compiler internals. Every
code snippet in them has been compiled and run against the in-tree compiler.

| Guide | Audience | Contents |
|---|---|---|
| [docs/tutorial.md](docs/tutorial.md) | New users | A from-scratch, example-driven walkthrough — install, first program, then each feature with runnable code. Read this to **learn to write Tocin**. |
| [docs/language-reference.md](docs/language-reference.md) | All users | The complete reference: grammar, types, operators & precedence, control flow, classes/traits/generics, error handling, concurrency, FFI, and the built-in function reference. |
| [docs/stdlib-reference.md](docs/stdlib-reference.md) | All users | Every built-in and standard-library function with signatures, semantics, and examples — strings, collections, math, file I/O, and more. |
| [docs/tocin-for-ai.md](docs/tocin-for-ai.md) | LLMs / tooling | A dense, precise spec of the *real* implemented language (verified feature set, ABI, codegen model, exact gotchas) so an AI can write correct Tocin on the first try. |

## Quick start

### Prerequisites

- LLVM 18–22 development package (18 is the Linux CI baseline, e.g.
  `llvm-18-dev` on Debian/Ubuntu; MSYS2's LLVM 22 works on Windows)
- CMake ≥ 3.16 and a C++20 compiler (GCC 11+, Clang 14+)
- `zlib`, `libffi` (and optionally `libzstd`, `libxml2`, Python 3 for FFI)

On Debian/Ubuntu:

```bash
sudo apt-get install -y llvm-18-dev libffi-dev zlib1g-dev libzstd-dev libxml2-dev cmake ninja-build
```

### Build

```bash
git clone https://github.com/tafolabi009/TocinLang.git
cd TocinLang
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
`.o` object, anything else → a linked native executable). Executables link
through the system C compiler when one is available — or through the **bundled
`ld.lld` + static link recipe** shipped by the installer packages, so an
installed Tocin builds native binaries on machines with **no compiler toolchain
at all** ([docs/native-linking.md](docs/native-linking.md)).

## Project structure

```
tocin-compiler/
├── src/
│   ├── lexer/       # Tokenizer
│   ├── parser/      # Recursive-descent parser → AST
│   ├── ast/         # AST and type node definitions
│   ├── type/        # Type checker (strict by default) and advanced type features
│   ├── codegen/     # LLVM IR generation
│   ├── compiler/    # Compilation context, optimization pipeline, macros
│   ├── runtime/     # Scheduler, async, concurrency, GC runtime
│   ├── ffi/         # C / Python / JavaScript FFI (see Roadmap for scope)
│   └── error/       # Diagnostics (caret rendering, suggestions)
├── stdlib/          # The standard library — 34 Tocin modules, all runnable
├── installer/       # Native installers: NSIS .exe, .deb, .run, .pkg/.dmg
├── tests/           # C++ unit tests, lli programs (cases/), JIT suites (jit/)
├── examples/        # Example programs
└── docs/            # Language and design documentation
```

## Testing

```bash
# tocin_runtime_shared lets the .to runner (lli) resolve the __tocin_* runtime
# symbols used by concurrency and exception programs.
cmake --build build --target tocin tocin_tests tocin_runtime_shared
ctest --test-dir build --output-on-failure       # C++ unit tests
bash scripts/run_to_tests.sh                      # .to integration programs (lli)
bash tests/run_stdlib_tests.sh                    # JIT runtime + stdlib suites
```

## Roadmap

These features are planned or partially scaffolded but **not yet fully working
end-to-end**. They are listed honestly so expectations match reality:

- **Explicit generic type arguments** — generic functions and classes infer their
  type arguments from the call/constructor today; turbofish-style annotations
  (`Box<int>(...)`) and generic types in parameter/field signatures are pending.
- **Generic higher-order stdlib** — `std.functional` ships real
  `map`/`filter`/`fold`/`zip` with function-value callbacks, and `std.linq`
  ships allocation-light reductions, both over int lists; generalizing these
  helpers over arbitrary element types is pending.
- **Nullable-type flow analysis** — the `?.`/`?:`/`!!` operators work today;
  static tracking of which references are nullable is pending.
- **`Option` / `Result` ergonomics** — `Some`/`None`/`Ok`/`Err` and `case`
  destructuring work today; exhaustiveness checking, `?` propagation and typed
  payloads (the bound value is an int slot) are pending.
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

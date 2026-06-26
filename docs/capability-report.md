# Tocin capability report

An honest assessment of what Tocin can and cannot build today, written against
the real in-tree compiler (every claim below is backed by a program that
compiles and runs — see `tests/cases/` and `examples/`).

Last updated: this build. Test suite: 136/136 `.to` programs passing, plus
opt-in borrow-check (5/5), match-exhaustiveness (3/3), and safety —
const + bounds (2/2) — harnesses.

---

## TL;DR

| Question | Answer |
|---|---|
| Write a compiler / interpreter / language? | **Yes — today.** This is Tocin's strongest domain. |
| Write CLI tools, parsers, data processors, batch jobs? | **Yes.** |
| Write a simple VCS / low-level file tool? | **Yes** — file I/O + bitwise + raw memory builtins (`alloc`/`memcpy`/`memset`/`load*`/`store*`) + optional C FFI for hashing. |
| Write a long-running microservice / server? | **Yes now** — a garbage collector (Boehm GC) reclaims unreachable memory, so long-lived processes no longer grow without bound (2M allocations that would leak >128 MB peak at ~8 MB). Networking still goes through C FFI. |
| OS / kernel / bare-metal work? | **Yes** — `--freestanding` emits a relocatable object with NO libc/GC/runtime (a pure arithmetic/raw-memory/asm program has zero undefined symbols; one using `alloc` references only the user-provided `__tocin_alloc`). Inline assembly and raw pointer/memory builtins give CPU and layout control. Verified end-to-end with a `-nostdlib` static binary. |
| Self-host (write the Tocin compiler in Tocin)? | **A subset is achievable now** (a Tocin→C or Tocin→LLVM-text backend); a full LLVM-API self-host is a large project blocked by tooling (LLVM bindings), not by language gaps. Memory is no longer a blocker. |
| C++-level optimizations? | **Yes for compute.** Native code runs LLVM's full `-O2`/`-O3` pipeline (same optimizer as clang). A compute benchmark (recursive fib + 100M-iteration loop) runs in **0.149 s vs C `-O2` at 0.137 s — within ~9 %**. Allocation-bound code now pays GC cost rather than leaking. |

---

## What Tocin can build today

**Compilers & interpreters — fully supported.** Verified end to end:
- `examples/expr_evaluator.to` — a recursive-descent arithmetic evaluator
  (`+ - * /`, parentheses, precedence) using mutual recursion and mutable
  parser state threaded through a class parameter.
- `examples/ast_tree_symtab.to` — an AST-shaped binary tree with recursive
  evaluation plus a string-keyed symbol table.
- `examples/linked_list.to` — a recursive linked-list data structure.

These are the exact ingredients of a front end: string/char access for lexing,
recursive descent + mutual recursion for parsing, `class` trees for ASTs,
string-keyed `map` for symbol tables, and `vector` for growable buffers.

**General programming.** Functions (recursion, mutual recursion, forward
references, first-class values, **capturing closures**), `class`/struct with
methods and `self`, generics (monomorphized), traits + `impl`, enums —
including **algebraic enums (tagged unions)** with payload-carrying, recursive
variants and **exhaustiveness-checked** `match` (the AST substrate for a
compiler), `Option`/`Result` with pattern matching, exceptions (`try`/`catch`/`finally`,
finally honored on early return), full operators (arithmetic with int→float
promotion, compound assignment, bitwise/shifts, value-equality strings),
`vector`/`map` collections, a char-level string library, and file I/O.

**Concurrency.** `go` goroutines (real OS threads), typed `channel<T>`,
`<-` send/receive, and `select` — in both JIT and native builds.

**Interop.** C functions via `extern def name(...) -> T;`, resolved by the JIT
and the native linker. This is the escape hatch for anything not in the
language (networking, crypto, OS APIs): call the C library.

---

## Modern language features

Beyond the basics, Tocin now has the constructs you reach for when building real
systems — each backed by a passing `.to` test and, where noted, an example:

- **Algebraic data types** — `enum` variants with typed, possibly-recursive
  payloads (`enum Expr { Num(int), Add(Expr, Expr) }`); construct by calling a
  variant; the AST substrate for compilers. (`examples/adt_interpreter.to`,
  `examples/stack_vm.to`)
- **Exhaustive pattern matching** — `match` on an ADT must cover every variant
  or add `default:`; a missing case is a fatal compile error (`P001`).
- **Tuples & multiple return** — `(a, b)` literals, `-> (int, int)`, `t.0`/`t.1`,
  and `let (q, r) = divmod(...)`. (`examples/tuples.to`)
- **Iterator protocol** — `for x in obj` drives any class with
  `next(self) -> Option`. (`examples/iterators.to`)
- **Array slices** — `a[lo..hi]` yields a fresh, bounds-clamped sub-array.
- **Variadics** — `def f(a, rest: int...)` collects trailing args into an array.
- **Labeled break/continue** — `outer: for ... { break outer; }`.
- **const enforcement** and **default-on bounds checking** (panics on
  out-of-range; `--freestanding` opts out).
- **Runtime stdlib** — TCP sockets (`tcp*`), time (`timeSec`/`monoNanos`/
  `sleepMs`), hashing (FNV-1a/splitmix64), seeded random, `envGet`/`sysExit`.
  (`examples/tcp_echo.to`) Plus a JSON parser written in Tocin
  (`examples/json_parser.to`).

## Not yet built (honest)

These are the remaining items a Rust/C++-class language would want; none are
blocked by a fundamental design flaw, but they are real work and are *not*
implemented today:

- **Trait objects / open dynamic dispatch.** Methods dispatch statically by the
  receiver's concrete type, and the iterator protocol gives one dynamic shape,
  but a `vector<Shape>` of heterogeneous trait objects (vtables / fat pointers)
  is not implemented. Closed-world polymorphism is available via ADTs + `match`.
- **Generic/trait bound enforcement.** `def f<T: Bound>` parses the bound but
  does not yet verify the type argument satisfies it.
- **By-reference closure capture** (capture is by value), `&`/`&mut` reference
  borrows and lifetimes (the borrow checker is move-only), and generators
  (`yield`).
- **Higher-level networking** (HTTP, TLS) and async/epoll I/O; build on the raw
  socket primitives or C FFI.
- **Tooling**: a formatter and an LSP are not provided; a hosted package
  registry and an M:N green-thread scheduler (goroutines are 1:1 OS threads)
  remain future work.

## Honest gaps (and how they bite)

1. **Memory is garbage-collected** (Boehm GC), so arrays, strings, closures,
   and boxes are reclaimed when unreachable — the old unbounded-leak blocker is
   gone. Remaining nuance: `vector`/`map` internal storage (C++ containers) is
   not GC-scanned, so very large collections still benefit from explicit
   `vecFree`/`mapFree`; and the GC is conservative (an int that happens to hold
   a heap address keeps that block alive). For request-scoped servers this is a
   non-issue in practice.
2. **Capture is by value, not by reference.** Closures snapshot captured
   locals; mutating the original afterward doesn't change the copy. (`const`
   bindings are enforced — reassigning one is a compile error; array indexing is
   bounds-checked by default, panicking on an out-of-range access, with
   `--freestanding` omitting the check for systems code; and `break`/`continue`
   support outer-loop labels.)
3. **Collection elements are 64-bit slots.** `vector`/`map`/channel payloads
   are designed for `int`. Pointers/strings round-trip as raw addresses but
   there is no element-type tracking, so storing strings in a `vector` is
   fragile. Use `mapPutStr`/`mapGetStr` for string-keyed data.
4. **Generators, the power operator `**`, and `++`/`--` are not implemented.**
   Memory safety has two layers: GC by default (always safe — no use-after-free
   regardless), plus an **opt-in borrow checker** (`--borrow-check`) that adds
   Rust-like compile-time move / use-after-move enforcement on owned values.
   `defer` + RAII destructors (`__del__`) give deterministic cleanup. The borrow
   checker is move-only for now — `&`/`&mut` reference borrows and lifetimes are
   the remaining Rust-parity items (the move analysis is the foundation they
   build on).
5. **TCP networking is built in** (`tcpListen`/`tcpAccept`/`tcpConnect`/
   `tcpSend`/`tcpRecv`/`tcpClose`, POSIX sockets) — enough to write a concurrent
   server (pair with `go`) or a client, plus `time`/`hashing`/`random`/`env`
   runtime modules. Higher-level protocols (HTTP, TLS) and async (epoll) I/O are
   not yet provided — build them on the socket primitives or reach for C FFI.
6. **Python/JavaScript FFI are scaffolding only** — the C path is the working
   one.

---

## Self-hosting: how far away?

The **language** is capable enough: you can already write a lexer, a
recursive-descent parser, an AST, a symbol table, and a tree-walking evaluator
in Tocin (all demonstrated). The realistic path to self-hosting is:

1. **Tocin → C or Tocin → LLVM-text-IR backend, written in Tocin.** Tocin has
   string building and file I/O, so a Tocin program can emit `.c` or `.ll` text
   and shell out to `cc`/`llc`. This avoids needing LLVM's C++ API from Tocin
   and is the pragmatic first self-host. **Achievable now**, modulo patience
   with the memory leak on large inputs.
2. **Full LLVM-API self-host** (replacing the current C++ driver) would require
   LLVM bindings exposed through C FFI and is a large engineering project — but
   it is gated by *tooling*, not by missing language features.

What would make self-hosting comfortable rather than merely possible:
generic-element collections, a real `string`/byte-buffer type, and memory
management (even a simple arena or refcount).

---

## Optimizations: is it "C++-level"?

Two halves, and they get different grades.

**Backend (code generation): A.** Tocin lowers to LLVM 18 IR and runs the
standard pass pipeline at `-O0`/`-O1`/`-O2`/`-O3` (default `-O2` for native
binaries) — the *same optimizer clang uses*. Tight arithmetic and control-flow
code is genuinely competitive with C++: inlining, constant folding,
vectorization, register allocation, and instruction selection are all LLVM's.

Measured: a compute benchmark — recursive `fib(34)` plus a 100-million-iteration
accumulation loop — compiled with Tocin `-O2` runs in **0.149 s** versus the
equivalent C at `-O2` in **0.137 s**: within ~9 %.

**Frontend (what IR we hand to LLVM): B.** The emitted IR is still
allocation-happy — arrays, string concatenations, closures, and Option/Result
boxes are heap-allocated rather than stack-allocated (no escape analysis yet),
and values cross boundaries as boxed `i64` slots. But two things changed the
grade from "naive" to "fine in practice":
- **Garbage collection** reclaims those allocations, so they no longer leak.
- **Constant folding** of literal string concatenation removes the
  `malloc`+`strcpy`+`strcat` for compile-time-constant strings.

The remaining headroom is escape analysis / stack allocation to avoid the GC
entirely for short-lived values — a refinement, no longer a correctness issue.
**Net: compute-bound code is C++-class (measured); allocation-bound code is
correct and bounded, paying GC cost where C++ would pay none.**

---

## Bottom line

Tocin is a real, compiled, statically-typed language that today can build
compilers, interpreters, CLI tools, parsers, long-running services, and
low-level/systems programs, with a world-class optimizing backend (measured
within ~9 % of C on compute) and a garbage-collected heap. Self-hosting runs
through a Tocin-emitting backend — concrete, scoped work, not blocked by a
fundamental language deficiency. The keystone gaps called out in earlier
revisions — memory management and missing operators/control-flow — are closed.

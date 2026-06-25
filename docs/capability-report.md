# Tocin capability report

An honest assessment of what Tocin can and cannot build today, written against
the real in-tree compiler (every claim below is backed by a program that
compiles and runs — see `tests/cases/` and `examples/`).

Last updated: this build. Test suite: 108/108 `.to` programs passing.

---

## TL;DR

| Question | Answer |
|---|---|
| Write a compiler / interpreter / language? | **Yes — today.** This is Tocin's strongest domain. |
| Write CLI tools, parsers, data processors, batch jobs? | **Yes.** |
| Write a simple VCS / low-level file tool? | **Yes**, using file I/O + bitwise + (optionally) C FFI for hashing. |
| Write a long-running microservice / server? | **Not yet** — no garbage collection, so long-lived processes leak memory. Fine for run-and-exit programs. |
| Self-host (write the Tocin compiler in Tocin)? | **A subset is achievable now** (a Tocin→C or Tocin→LLVM-text backend); a full LLVM-API self-host is a large project, blocked by tooling (LLVM bindings) and memory management, not by language gaps. |
| C++-level optimizations? | **Backend yes, frontend no.** Native code runs through LLVM's full -O2/-O3 pipeline (same optimizer as clang). But the IR the front end emits is allocation-heavy and never frees, so allocation-bound code is not yet C++-competitive. |

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
methods and `self`, generics (monomorphized), traits + `impl`, enums,
`Option`/`Result` with pattern matching, exceptions (`try`/`catch`/`finally`,
finally honored on early return), full operators (arithmetic with int→float
promotion, compound assignment, bitwise/shifts, value-equality strings),
`vector`/`map` collections, a char-level string library, and file I/O.

**Concurrency.** `go` goroutines (real OS threads), typed `channel<T>`,
`<-` send/receive, and `select` — in both JIT and native builds.

**Interop.** C functions via `extern def name(...) -> T;`, resolved by the JIT
and the native linker. This is the escape hatch for anything not in the
language (networking, crypto, OS APIs): call the C library.

---

## Honest gaps (and how they bite)

1. **No garbage collection / automatic free.** Arrays, strings produced by
   concatenation, closures, `vector`/`map`, and `Option`/`Result` boxes are
   `malloc`'d and never freed (manual `vecFree`/`mapFree` exist). A program
   that runs and exits is fine; a server that runs for hours will grow without
   bound. **This is the single biggest blocker for microservices.**
2. **Capture is by value, not by reference.** Closures snapshot captured
   locals; mutating the original afterward doesn't change the copy. No labeled
   `break`/`continue`.
3. **Collection elements are 64-bit slots.** `vector`/`map`/channel payloads
   are designed for `int`. Pointers/strings round-trip as raw addresses but
   there is no element-type tracking, so storing strings in a `vector` is
   fragile. Use `mapPutStr`/`mapGetStr` for string-keyed data.
4. **`switch`, `defer`, ownership (`move`/`borrow`), generators, the power
   operator `**`, and `++`/`--` are not implemented.** Use `match`/`case` for
   `switch`; there is no borrow checker (so no Rust-style compile-time memory
   safety).
5. **No standard networking / HTTP / async-I/O library.** Reach for C FFI.
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

**Frontend (what IR we hand to LLVM): C.** The emitted IR is naive:
- Heap-allocates (`malloc`) for arrays, string concatenations, closures, and
  Option/Result boxes — and never frees. No escape analysis, so a closure or
  array that never leaves a function is still heap-allocated rather than put on
  the stack.
- Values cross boundaries as boxed `i64` slots; opaque pointers discard element
  type information.
- String concatenation does `malloc`+`strcpy`+`strcat` per `+`.

LLVM cleans up a great deal (dead code, redundant loads, scalar replacement
where it can prove safety), but it cannot remove the allocations or invent
frees. **Net: optimized compute-bound code is C++-class; allocation-bound code
is not, until the front end learns stack allocation, escape analysis, and
either frees or a GC.**

---

## Bottom line

Tocin is a real, compiled, statically-typed language that today can build
compilers, interpreters, CLI tools, parsers, and run-and-exit programs, with a
world-class optimizing backend. The path to "production microservices" runs
through memory management; the path to "self-hosting" runs through a
Tocin-emitting backend plus those same memory improvements. Neither is blocked
by a fundamental language deficiency — both are concrete, scoped engineering
work on top of a working core.

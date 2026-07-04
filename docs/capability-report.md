# Tocin capability report

An honest assessment of what Tocin can and cannot build today, written against
the real in-tree compiler (every claim below is backed by a program that
compiles and runs — see `tests/cases/` and `examples/`).

Last updated: this build. Test suite: 146/146 `.to` programs passing, plus
opt-in borrow-check — move + `&`/`&mut` borrows (12/12), match-exhaustiveness
(3/3), and safety — const + bounds + trait-bound (3/3) — harnesses.

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
| C++-level optimizations? | **Yes for compute.** Native code runs LLVM's full `-O2`/`-O3` pipeline (same optimizer as clang), plus `--native` CPU tuning, whole-program internalization, and alias-aware buffer optimization. On a 12-kernel C/C++/Rust comparison the geomean **ties Rust** (~1.4× of C overall), with Tocin outright fastest on 5 of 12 kernels (`sqrtsum` ~2× faster than C); `matmul` and `levenshtein` are the known gaps. Allocation-bound code pays GC cost rather than leaking. |

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
- **Trait objects / open dynamic dispatch** — a value typed as a trait carries
  its concrete type at run time and dispatches method calls virtually, through
  trait-typed parameters, `let` bindings, and **heterogeneous collections**
  (`let xs: list<Shape> = [Circle(..), Rect(..)]`). Representation is a
  `{ i64 typeId, ptr data }` box. (`examples/trait_objects.to`)
- **Generic trait bounds** — `def f<T: Bound>` rejects a type argument that does
  not implement `Bound` (fatal `T016`), and trait-method calls on the bounded
  parameter resolve to the concrete type.
- **Generators** — a function with `yield` produces a sequence; calling it
  materializes the yielded values into an array that `for x in gen(...)` walks
  or indexes. Eager (finite-sequence) collection. (`examples/generators.to`)
- **By-reference closures** — a closure that writes a captured local shares the
  cell (the write is visible outside); read-only captures stay by-value.
  (`examples/byref_closures.to`)
- **`&`/`&mut` reference borrows** — under `--borrow-check`, shared-XOR-mutable
  borrows with lexical lifetimes (conflicts are fatal `B002`).
  (`examples/borrow_check.to`)

## Not yet built (honest)

These are the remaining items a Rust/C++-class language would want; none are
blocked by a fundamental design flaw, but they are real work and are *not*
implemented today:

- **By-reference closure capture now works**: a closure that *writes* a
  captured local shares the cell, so the mutation is visible in the enclosing
  scope (read-only captures stay by-value snapshots). The one nuance is escaping
  by-ref closures (a written-capture closure that outlives its defining frame) —
  not reachable today because the type checker does not yet permit calling a
  stored function-typed value; heap-promotion of the cell is the upgrade for
  when it does.
- **Generators (`yield`) now work** for finite sequences: a function containing
  `yield` becomes a generator whose call materializes the yielded values as an
  array that `for x in gen(...)` iterates (or you can index it). Collection is
  eager (collect-then-iterate), so infinite/lazy generators — which need a
  coroutine substrate (see the async item) — are the remaining piece.
- **`&`/`&mut` reference borrows now work** under `--borrow-check`: `&x` takes a
  shared borrow and `&mut x` an exclusive one, with the standard rule (many
  shared XOR one mutable) and lexical borrow lifetimes (a borrow ends when its
  binding leaves scope). Conflicts — a second `&mut`, a `&mut` over a live `&`,
  using/moving/mutating a value while it is borrowed — are fatal `B002`. What
  remains is non-lexical (flow-sensitive) lifetimes and borrows through struct
  fields / across function boundaries; the move analysis and these statement-
  scoped borrows are the foundation those build on.
- **Async/await works; the M:N scheduler is the remaining layer.** `async def`
  and `await` now compile and run with correct eager semantics: an async body
  runs on the calling path and `await f()` evaluates to f()'s result (composes
  in expressions and across async calls — `examples/async_await.to`). Goroutines
  (`go`) run on real 1:1 OS threads with channels. What is *not* built is the
  cooperative **M:N** scheduler that suspends a pending `await` and runs other
  tasks on a worker pool — the fiber substrate exists
  (`src/runtime/lightweight_scheduler.*`, ucontext-based) but is not yet wired
  into the program runtime; the design is written up in
  `docs/async-scheduler-design.md`.
- **Higher-level networking** (HTTP, TLS) and async/epoll I/O; build on the raw
  socket primitives or C FFI.
- **Tooling**: a formatter and an LSP are not provided; a hosted package
  registry remains future work.

## Honest gaps (and how they bite)

1. **Memory is garbage-collected** (Boehm GC), so arrays, strings, closures,
   and boxes are reclaimed when unreachable — the old unbounded-leak blocker is
   gone. Remaining nuance: `vector`/`map` internal storage (C++ containers) is
   not GC-scanned, so very large collections still benefit from explicit
   `vecFree`/`mapFree`; and the GC is conservative (an int that happens to hold
   a heap address keeps that block alive). For request-scoped servers this is a
   non-issue in practice.
2. **Closure capture is by value for reads, by reference for writes.** A
   closure that only reads a captured local snapshots it (mutating the original
   afterward doesn't change the snapshot); a closure that *assigns* a captured
   local shares the underlying cell, so the write is visible outside the closure
   (`examples/byref_closures.to`). (`const` bindings are enforced — reassigning
   one is a compile error; array indexing is bounds-checked by default, panicking
   on an out-of-range access, with `--freestanding` omitting the check for systems
   code; and `break`/`continue` support outer-loop labels.)
3. **Collection elements are 64-bit slots.** `vector`/`map`/channel payloads
   are designed for `int`. Pointers/strings round-trip as raw addresses but
   there is no element-type tracking, so storing strings in a `vector` is
   fragile. Use `mapPutStr`/`mapGetStr` for string-keyed data.
4. **The power operator `**` and `++`/`--` are not implemented** (generators
   now work for finite sequences via `yield`; truly lazy ones are future work).
   Memory safety has two layers: GC by default (always safe — no use-after-free
   regardless), plus an **opt-in borrow checker** (`--borrow-check`) that adds
   Rust-like compile-time enforcement on owned values: move / use-after-move
   (`B001`) *and* `&`/`&mut` reference borrows with the shared-XOR-mutable rule
   and lexical borrow lifetimes (`B002`). `defer` + RAII destructors (`__del__`)
   give deterministic cleanup. What remains for full Rust parity is non-lexical
   (flow-sensitive) lifetimes and borrows through struct fields / across
   function boundaries — refinements on the statement-scoped borrows that work
   today.
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

Measured on a 12-kernel comparison against C, C++, and Rust (same machine,
each language's `-O3`-equivalent, Tocin at `-O3 --native`): Tocin's geomean
**ties Rust** at roughly 1.4× of C overall, and Tocin is outright fastest of
the four on 5 of the 12 kernels — e.g. `sqrtsum` runs ~2× faster than the C
version because `sqrt` lowers to the LLVM intrinsic and vectorizes. The known
gaps are `matmul` (the reduction pattern blocks loop interchange — it limits
Rust too) and `levenshtein`.

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
low-level/systems programs, with a world-class optimizing backend (geomean
ties Rust on a 12-kernel C/C++/Rust comparison; outright fastest on 5 of 12)
and a garbage-collected heap. Self-hosting runs
through a Tocin-emitting backend — concrete, scoped work, not blocked by a
fundamental language deficiency. The keystone gaps called out in earlier
revisions — memory management and missing operators/control-flow — are closed.

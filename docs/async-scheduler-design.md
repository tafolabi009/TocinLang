# Async M:N scheduler — design

This document scopes the one concurrency item Tocin does **not** yet ship: a
cooperative **M:N** scheduler that multiplexes many `async` tasks onto a small
pool of OS worker threads, suspending a task at `await` when its result is not
ready and running other tasks meanwhile.

It is written so the work can be picked up as a focused, de-risked project. The
language surface (`async def`, `await`) already works today with **eager**
semantics (an async body runs on the calling path; `await f()` is f()'s result).
This design replaces the eager lowering with a suspend/resume one **without
changing observable single-result semantics** — only adding concurrency.

## What already exists

- **`async`/`await` front end** — parsed, type-checked, and lowered (currently
  as ordinary synchronous calls; `await` is identity over a ready value).
- **Goroutines + channels** — `go f(args)` packs args into a heap struct and
  spawns a real OS thread (`__tocin_go`, `src/runtime/concurrency_runtime.cpp`);
  `channel<T>`, `<-`, and `select` work in JIT and native builds.
- **A fiber substrate** — `src/runtime/lightweight_scheduler.{h,cpp}`: a `Fiber`
  with a `ucontext`-based stack (POSIX `makecontext`/`swapcontext`; Windows
  fibers), a `WorkStealingQueue`, `Worker` threads, and a `LightweightScheduler`
  with work-stealing and NUMA hooks. It compiles into the **compiler** today but
  is **not linked into the program runtime** (`tocin_runtime`) and is not driven
  by generated code.

The gap is therefore *integration*, not invention: wire the existing fiber
scheduler into the runtime and lower `async`/`await` onto it.

## Target model

Stackful coroutines on a fixed worker pool (the Go model), because the fiber
substrate is already stackful:

- A **task** is a `Fiber` running one `async` call's body on its own ~16–64 KB
  stack. `go`/spawning an async function enqueues a task.
- `P` **workers** (default `min(nproc, ...)`) each run a loop pulling ready tasks
  from their queue, stealing from peers when idle.
- `await fut`:
  - fast path — if `fut` is already resolved, take the value and continue (no
    switch), preserving today's eager behavior for ready results;
  - slow path — register the current fiber as `fut`'s waiter and
    `swapcontext` back to the worker scheduler loop. When `fut` resolves, its
    completer marks the waiter Ready and pushes it to a worker queue; the worker
    later `swapcontext`s back in, and `await` returns the value.
- A **Future** is `{ state, value, waiters, mutex }`; an async task's return
  resolves its own future and wakes waiters.

## Work items (in dependency order)

1. **Link the scheduler into the runtime.** Add `lightweight_scheduler.cpp` (and
   a new `async_runtime.cpp`) to the `tocin_runtime` / `tocin_runtime_shared`
   targets in `CMakeLists.txt`. Today only `concurrency_runtime.cpp` is linked
   into programs.
2. **C ABI bridge** (`extern "C"`, the established `__tocin_*` pattern — define
   in the runtime, dispatch by name in `visitCallExpr`, register in `main.cpp`):
   - `__tocin_sched_start()/_shutdown()` — boot/drain the worker pool.
   - `void* __tocin_future_new()`, `__tocin_future_resolve(f, i64)`,
     `i64 __tocin_future_await(f)` — await parks the current fiber if pending.
   - `__tocin_spawn(fnptr, argpack)` — enqueue an async body as a task (reuse the
     `go` arg-packing thunk in `visitGoStmt`).
3. **Lower `async`/`await` onto the bridge.**
   - An `async def` compiles to a body function returning into a future plus a
     thin entry that `__tocin_spawn`s it and returns the future handle.
   - `await e` → `__tocin_future_await(e)` (replacing today's identity lowering
     in `visitAwaitExpr`). Keep the eager fast path inside the runtime so
     single-await programs behave identically.
4. **GC × fibers (correctness-critical).** Boehm GC must scan every fiber stack
   as roots, or live values get collected mid-suspend. Register each fiber stack
   with `GC_register_my_thread` / a `GC_stack_base`, or allocate fiber stacks
   from GC memory and push them as explicit roots. This is the subtlest item and
   needs its own stress test (allocate-heavy tasks across many suspends).
5. **Blocking-call offload.** A task that makes a blocking syscall (file/socket)
   would stall its worker. Minimum: document it. Better: a small blocking-IO
   thread pool, or hand off to non-blocking epoll/kqueue (ties into the
   higher-level-networking item).
6. **Cancellation + structured concurrency** (optional v2): a `select`/timeout
   that cancels a pending await; scope-bound task groups.

## Testing

- **Semantics unchanged**: every existing async test (`tests/cases/async_*.to`)
  must still pass under the new lowering.
- **Real interleaving**: two async tasks that `await` on each other's futures
  must complete (would deadlock under a 1:1 model with fewer workers than
  tasks) — proves M:N suspend/resume.
- **Scale**: spawn 100k tasks that each await once; assert completion and a
  worker count ≪ task count (proves M:N, not 1:1).
- **GC stress**: allocation-heavy tasks across many suspensions, run under GC,
  assert no use-after-free and bounded memory.

## Why it is deferred (honesty)

Items 1–4 are individually tractable but collectively a subsystem, and item 4
(GC interacting with hand-switched stacks) is genuinely subtle — a wrong cut
there yields rare, hard-to-debug corruption. Shipping a half-working scheduler
would regress the currently-green concurrency story, so it is staged here as
scoped, testable work rather than rushed in. The eager `async`/`await` that
ships today is correct for the common single-result case; this design is the
path to making it *concurrent*.

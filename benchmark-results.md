# Where does Tocin rank? — 12 kernels, 8 languages

**Verdict: Tocin is in the compiled "big-boys" tier — the C / C++ / Rust
cluster, not the Python / Node tier.** With native-CPU codegen it ranks **#4 of
8** overall at a **1.47x** geomean slowdown, statistically tied with Rust (1.42x)
and within striking distance of C (1.39x) and C++ (1.30x). The next tier down —
Go (2.00x), Java (2.19x) — is a clear gap below it. Tocin is ~3x closer to #1
than it is to Go.

> **What changed.** An earlier run had Tocin at 2.08x because its AOT path
> compiled for *generic* x86-64 and its optimizer ran with no host-CPU info, so
> it never emitted `POPCNT`, never auto-vectorized with AVX, and the middle-end
> couldn't fold the popcount idiom. That report predicted: *"give the AOT path
> the host CPU… that alone should collapse popcount (~11x → ~1x) and very likely
> move Tocin into the C/C++/Rust cluster."* This is that change (`--native`),
> measured. Popcount went **24.2 ms → 0.4 ms** (now hardware `POPCNT` /
> `VPOPCNTDQ`), and the overall score dropped **2.08x → 1.47x.** Prediction
> confirmed.

Every kernel is algorithmically identical in all 8 languages, and the harness
cross-verifies each kernel's integer checksum across every language before any
time is reported. **All 12 checksums agree.** Tocin built the best way it offers:
**AOT native, full LLVM `-O3`, `--native`, multi-file**, fast data path (raw
`alloc`+`loadInt/storeInt`, which lower to inline `load/store`).

Machine: Intel Xeon (Sapphire Rapids) shared cloud VM. Best-of-3 (Python
best-of-1). Tocin `-O3 --native` AOT · C/C++ `-O3 -march=native` · Rust
`opt-level=3 target-cpu=native` · Go `go build` · Java HotSpot · Node/V8 ·
CPython 3.11.

## Overall ranking

Score = geometric mean, across all 12 kernels, of (language's time / the fastest
language's time on that kernel). 1.00 = fastest on everything; lower is better.

| Rank | Language | Score | Tier |
|---|---|---|---|
| 1 | C++ | 1.30x | native compiled |
| 2 | C | 1.39x | native compiled |
| 3 | Rust | 1.42x | native compiled |
| **4** | **Tocin** | **1.47x** | **native compiled** |
| 5 | Go | 2.00x | compiled + GC/runtime |
| 6 | Java | 2.19x | JIT |
| 7 | Node | 3.32x | JIT |
| 8 | Python | 64.65x | interpreted |

The top four are separated by 0.17; the gap from Tocin to Go is 0.53. Tocin is
inside the leading cluster, not adjacent to it.

## Per-kernel times (ms, lower is better)

| Kernel | category | tocin | c | cpp | rust | go | java | node | python |
|---|---|---|---|---|---|---|---|---|---|
| fib | recursion | 20.1 | 15.3 | 15.3 | 24.7 | 42.7 | 34.0 | 66.0 | 803 |
| nqueens | backtracking | 499 | 486 | 490 | 530 | 576 | 487 | 727 | 21184 |
| sieve | memory scan | 56.1 | 48.6 | 50.5 | 52.6 | 49.9 | 52.7 | 65.5 | 1342 |
| mandelbrot | float | **153** | 185 | 185 | 169 | 139 | 144 | 143 | 5073 |
| matmul | int matrix | 14.8 | 5.1 | 4.1 | 15.3 | 20.4 | 16.9 | 20.3 | 1080 |
| collatz | integer loop | **147** | 215 | 219 | 164 | 205 | 213 | 1178 | 5979 |
| popcount | bit manip | **0.4** | 2.3 | 2.2 | 0.4 | 26.8 | 25.5 | 41.1 | 2115 |
| sqrtsum | float / sqrt | 16.9 | 18.6 | 18.7 | 16.4 | 15.0 | 29.0 | 14.9 | 1244 |
| digitsum | int->string | 167 | 63 | 33 | 54 | 43 | 33 | 46 | 453 |
| quicksort | sorting | **80.4** | 91.4 | 91.5 | 86.0 | 78.9 | 110 | 128 | 1367 |
| levenshtein | dynamic prog | 11.2 | 4.9 | 4.9 | 11.7 | 8.9 | 16.2 | 19.9 | 534 |
| rollhash | hashing | 88.0 | 89.7 | 89.7 | 86.3 | 71.0 | 91.1 | 346 | 1210 |

**Bold = Tocin beats all of C, C++ and Rust on that kernel** (mandelbrot,
collatz, popcount, quicksort). On popcount it ties Rust for fastest of all eight.

## Reading it

**Top-tier (9 of 12 kernels, within ~1.3x of the best compiled language, often
beating C/C++/Rust):** fib, nqueens, sieve, mandelbrot, collatz, popcount,
sqrtsum, quicksort, rollhash. On tight integer/float loops, bit manipulation and
raw-memory work it rides LLVM and lands right next to — or ahead of —
C/C++/Rust.

**Still trailing — and why:**
- **digitsum 5.1x off Java.** `intToStr` allocates a GC string per number. This
  is allocation-bound, not CPU-bound, so `--native` doesn't touch it — the
  string/conversion runtime + GC is Tocin's genuinely softest area and the next
  thing worth optimizing (small-integer fast path, reusable buffer).
- **matmul 3.6x off C++.** `--native` barely moved this (15.8 → 14.8 ms). The
  inner loop walks `b` column-wise (stride `n`), which the vectorizer can't turn
  into contiguous AVX loads without a loop interchange / tiling pass the compiler
  doesn't run yet. The ceiling here is a codegen feature, not a CPU flag.
- **levenshtein 2.3x off C.** Byte-array DP with per-cell min; reasonable but not
  yet vectorized.

So the remaining gap is two fixable compiler items (string-alloc churn; matmul
loop interchange), not anything fundamental about the language.

## Bottom line

Tocin is **not** in the interpreted/JIT tier (Node 3.3x, Python 65x). It is a
real native-compiled language sitting **with C, C++ and Rust** — a 1.47x geomean
that is statistically tied with Rust, and on pure compute it already beats all
three on a third of the kernels. The headline weakness from the previous run
(no hardware POPCNT, no AVX targeting) is fixed: `tocin file.to -O3 --native -o app`
now tunes codegen for the host CPU end-to-end (both the LLVM middle-end and the
backend).

### Reproduce
```
tocin runner.to -O3 --native -o runner   # the Tocin build under test
bash run_all.sh                           # builds all 8, cross-verifies, tabulates
```

### Caveats
Single shared-VM run, ~15% noise; orderings inside ~1.3x are ties. Kernels are
simple and identical across languages, so no language uses its own libraries.
`--native` binaries are tuned for the build host's CPU and are not portable to
older CPUs (omit `--native` for a portable generic-x86-64 binary).

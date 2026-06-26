# Tocin vs 7 languages — cross-language microbenchmarks

Tocin built the **best way the language offers**: AOT native compilation (`tocin
runner.to -O3 -o runner`), full LLVM `-O3`, multi-file (a `runner` importing six
kernel modules + a timing harness), and the fast data path — raw memory
(`alloc` + `loadInt/storeInt/loadByte/storeByte`, which lower to inline
`getelementptr`+`load/store`, **no** growable-vector runtime calls).

Every kernel is **algorithmically identical** in all eight languages and the
harness **cross-verifies the checksum** of each kernel across every language
before reporting any time. They all agree:

| kernel | checksum | | kernel | checksum |
|---|---|---|---|---|
| fib(35) | 9227465 | | mandelbrot | 149817 |
| nqueens(13) | 73712 | | matmul(256) trace | 156140380 |
| sieve(10^7) = pi(10^7) | 664579 | | collatz(<10^6) max len | 524 |

## Results — wall time per kernel (ms, lower is better)

Machine: Intel Xeon @ 2.10 GHz, 4 cores (shared cloud VM). Best-of-3 per kernel
(CPython best-of-1 — no JIT to warm). Tocin `-O3` AOT · C/C++ `-O3
-march=native` · Rust `opt-level=3 target-cpu=native` · Go `go build` · Java
HotSpot · Node 22 / V8 · CPython 3.11.

| Benchmark | **tocin** | c | cpp | rust | go | java | node | python | Tocin / best peer* |
|---|---|---|---|---|---|---|---|---|---|
| fib         | **21.2** | 15.3 | 15.3 | 18.6 | 42.7 | 34.4 | 83.7 | 922 | 1.39x |
| nqueens     | **491**  | 474 | 472 | 447 | 581 | 601 | 868 | 19940 | 1.10x |
| sieve       | **53.5** | 49.9 | 49.2 | 55.6 | 55.6 | 54.9 | 71.0 | 1255 | 1.09x |
| mandelbrot  | **171**  | 185 | 186 | 139 | 163 | 182 | 173 | 4642 | 1.24x |
| matmul      | **16.0** | 5.2 | 4.4 | 13.0 | 23.5 | 21.7 | 26.3 | 968 | 3.65x |
| collatz     | **160**  | 219 | 221 | 135 | 235 | 272 | 1465 | 5378 | 1.19x |

\* Tocin time / the fastest non-Python peer.

## What this shows (honestly)

- **Tocin is a genuinely fast compiled language**, not "fast for a young
  language." On 5 of 6 kernels it is within **1.1-1.4x** of the fastest of C,
  C++, Rust, Go, and Java.
- **It beats gcc/clang `-O3 -march=native` on two kernels**: collatz (160 ms vs
  C 219 / C++ 221) and mandelbrot (171 ms vs C 185 / C++ 186). Rust — also LLVM —
  leads both, so this is LLVM's tight-loop codegen showing through; Tocin rides
  it. Tocin also beats Go, Java, and Node on most kernels.
- **One clear weakness: matmul, 3.65x off C++.** gcc/clang auto-vectorize the
  integer inner loop (SIMD); Tocin's scalar `load i64` does not. A contributing
  detail found while building this: Tocin emits `align 4` on its 8-byte
  `loadInt/storeInt`, which discourages LLVM's vectorizer — emitting `align 8`
  (the buffers *are* 8-byte aligned) is a concrete, free codegen win to chase.
- **15-90x faster than CPython** everywhere, as expected of native code.
- **AOT `-O3` is ~5x faster than the JIT path.** A prior run of the same fib(35)
  via `--run` was ~114 ms; here AOT `-O3` is ~21 ms. "Use the best optimization
  the language offers" mattered.

### Caveats
Single shared-VM run; treat ~15% as noise. Orderings inside ~1.3x (e.g.
fib/sieve/nqueens among the C-tier) are effectively ties. Kernels are
deliberately simple and identical, so no language uses its own fancy libraries.
matmul is the one kernel with a real, explained gap.

## Reproduce
```
bash run_all.sh      # builds all 8 with best opts, runs, cross-checks, tabulates
```

# Query-Style Operations (LINQ)

Tocin's query facilities are ordinary functions, not extension methods:
**method-style chaining (`xs.where(...).select(...)`) is not part of the
language.** Two modules cover the same ground over `list<int>`:

- `std.linq` — reductions/aggregations and `*Into` transforms that take plain
  parameters (thresholds, scale factors, operation codes);
- `std.functional` — higher-order `map`/`filter`/`fold` functions that take
  function values (named functions or lambdas).

Both follow the stdlib conventions: predicates return `int` `1`/`0`, and
imported names are called bare (no namespacing).

## std.linq

| Function | Description |
|---|---|
| `reduceSum(xs: list<int>) -> int` | Sum of the elements |
| `reduceProduct(xs: list<int>) -> int` | Product of the elements |
| `aggregate(xs: list<int>, seed: int, op: int) -> int` | Fold with a seed and an op code: `0` add, `1` multiply, `2` max, `3` min |
| `count(xs: list<int>) -> int` | Element count (`len`) |
| `countGreater(xs: list<int>, threshold: int) -> int` | How many elements exceed `threshold` |
| `indexOf(xs: list<int>, target: int) -> int` | First index of `target`, `-1` if absent |
| `allGreater(xs: list<int>, threshold: int) -> int` | `1` if every element exceeds `threshold` |
| `anyGreater(xs: list<int>, threshold: int) -> int` | `1` if any element exceeds `threshold` |
| `mapScaleInto(dst, src: list<int>, k: int) -> int` | Write `src[i] * k` into `dst`; returns the count |
| `mapAddInto(dst, src: list<int>, k: int) -> int` | Write `src[i] + k` into `dst`; returns the count |
| `filterGreaterInto(dst, src: list<int>, threshold: int) -> int` | Copy elements `> threshold` into `dst`; returns how many were written |

The `*Into` transforms write into a caller-allocated destination list at least
as long as the source, instead of returning a fresh list. Verified example:

```tocin
import std.linq;

def main() {
    let xs = [3, 1, 4, 1, 5, 9, 2, 6];
    println("sum {}  product {}", reduceSum(xs), reduceProduct(xs));
    println("max {}  over3 {}", aggregate(xs, xs[0], 2), countGreater(xs, 3));
    let dst = [0, 0, 0, 0, 0, 0, 0, 0];
    let n = filterGreaterInto(dst, xs, 3);
    println("kept {}  first {}", n, dst[0]);
    return 0;
}
```

```text
sum 31  product 6480
max 9  over3 4
kept 4  first 4
```

## std.functional

These take function-typed parameters, so the transformation itself is an
argument — a top-level `def` or a `lambda` (which captures enclosing locals
by value):

| Function | Description |
|---|---|
| `mapInts(xs: list<int>, f: (int) -> int) -> list<int>` | New list of `f(x)` for each element |
| `filterInts(xs: list<int>, pred: (int) -> int) -> list<int>` | New list of elements where `pred(x) != 0` |
| `foldInts(xs: list<int>, init: int, f: (int, int) -> int) -> int` | Left fold: `f(acc, x)` |
| `zipWith(a: list<int>, b: list<int>, f: (int, int) -> int) -> list<int>` | Element-wise combine of two lists |
| `anyInt(xs, pred)` / `allInt(xs, pred)` | `1` if any / every element satisfies `pred` |
| `countWhere(xs: list<int>, pred: (int) -> int) -> int` | How many elements satisfy `pred` |
| `findFirst(xs: list<int>, pred: (int) -> int, dflt: int) -> int` | First matching element, or `dflt` |
| `takeWhile(xs, pred)` / `dropWhile(xs, pred)` | Longest matching prefix / everything after it |
| `rangeList(start: int, end: int) -> list<int>` | Half-open integer range `[start, end)` |
| `reversed(xs: list<int>) -> list<int>` | Reversed copy |
| `concatInts(a: list<int>, b: list<int>) -> list<int>` | Concatenated copy |

Verified example with lambdas:

```tocin
import std.functional;

def main() {
    let xs = rangeList(1, 6);                                   // [1, 2, 3, 4, 5]
    let sq = mapInts(xs, lambda (x: int) -> int x * x);
    println("{} {} {}", sq[0], sq[4], len(sq));
    let odd = filterInts(xs, lambda (x: int) -> int x % 2);
    println("{}", foldInts(odd, 0, lambda (a: int, b: int) -> int a + b));
    return 0;
}
```

```text
1 25 5
9
```

## Limitations

- Both modules operate on `list<int>` today. For float data use the
  `list<float>` functions in `math.stats` (`mean`, `median`, ...) and
  `math.stats_advanced`.
- There is no chaining and no lazy evaluation: compose by nesting calls or
  binding intermediate `let`s, as above.
- `std.functional`'s `countWhere` clashes with a same-named function in
  `database.database`; do not import both modules into one program.

See also: [language-reference.md](language-reference.md) (lambdas and function
types), [stdlib-reference.md](stdlib-reference.md) (builtins and the
`std.linq` walkthrough), [04_Standard_Library.md](04_Standard_Library.md).

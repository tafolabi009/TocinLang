# Tocin Standard Library Guide

A practical guide to finding, importing, and testing against the standard
library. For a module-by-module tour with function tables see
[04_Standard_Library.md](04_Standard_Library.md); for the built-in functions
that need no import see [stdlib-reference.md](stdlib-reference.md).

## Finding modules

The library is plain Tocin source under `stdlib/`, organized into domain
directories. The import path is the file path: `import data.algorithms;`
loads `data/algorithms.to`.

```text
stdlib/
├── std/          math.to  list.to  functional.to  linq.to  strings.to
│                 strseq.to  json.to  testing.to
├── data/         algorithms.to  structures.to  collections.to
├── math/         basic.to  linear.to  geometry.to  stats.to
│                 stats_advanced.to  differential.to
├── ml/           neural_network.to  deep_learning.to  computer_vision.to  ten.to
├── web/          http.to  websocket.to
├── net/          advanced.to
├── database/     database.to
├── game/         engine.to  graphics.to  shader.to
├── gui/          core.to  widgets.to
├── audio/        audio.to
├── embedded/     gpio.to
├── scripting/    automation.to
└── pkg/          manager.to
```

Each file opens with a comment describing its scope, and every public function
carries a doc comment — `grep "^def " stdlib/std/strings.to` is a quick way to
see a module's API.

## Import mechanics

```tocin
import std.testing;        // dotted path  -> std/testing.to
import "std/testing";      // string path, same file
```

For each import the compiler searches, in order:

1. the directory of the importing file,
2. `$TOCIN_PATH`, if set,
3. the compiled-in standard-library path.

The installers generate a `tocin` launcher that points `TOCIN_PATH` at the
installed `stdlib/`, so imports work out of the box. From a source checkout,
set it explicitly:

```sh
TOCIN_PATH=/path/to/TocinLang/stdlib ./build/tocin program.to --run
```

An import merges the file's top-level declarations into the program (each file
is loaded at most once, and a module's own imports load first). **There is no
namespacing and no selective import**: after `import std.math;` you call
`gcd(12, 18)` — never `std.math.gcd(...)` or `math.gcd(...)`.

### The naming rule

Because every imported name lands in one global scope, stdlib functions are
named to be globally unique, usually with a module or structure prefix:
`listSum`, `strTrim`, `jsonParse`, `heapPush`, `matMul`, `tenScan`. Constants
follow the same rule (`PI`, `GEO_PI`, `JSON_ARRAY`, `WS_HOVERED`).

A few short names do repeat across unrelated domains:

| Name | Defined in |
|---|---|
| `countWhere` | `std/functional.to` and `database/database.to` |
| `mix` | `audio/audio.to` and `game/shader.to` |
| `step` | `game/engine.to` and `game/shader.to` |
| `fract` | `math/basic.to` and `game/shader.to` (identical definitions) |

Importing two modules that define the same name with different signatures
into one program is not supported — it typically fails to compile. Keep such
pairs in separate programs.

### API conventions

- **Booleans are `int`**: predicates return `1`/`0` (`isPrime(13)` returns
  `1`), and boolean parameters take `1`/`0`.
- **Handles are `int`**: raw-buffer structures (`heapNew`, `tableNew`,
  `worldNew`, `layoutNew`, ...) return an address; pass it to the module's
  functions and release it with the matching `*Free`.
- Bulk float APIs write into **caller-allocated `list<float>` buffers**
  instead of returning fresh lists (`softmax(src, dst)`,
  `linearRegression(x, y, out)`).

## Writing tests with std.testing

`std.testing` is a minimal test harness written in Tocin. It keeps global
pass/fail counters so checks read naturally, and `testSummary()` returns the
process exit code (`0` if every check passed, `1` otherwise):

| Function | Description |
|---|---|
| `testBegin() -> int` | Reset the pass/fail counters |
| `check(name: string, cond: int) -> int` | Assert a condition (`1`/`0` or any comparison) |
| `checkEq(name: string, got: int, want: int) -> int` | Assert integer equality; prints both values on mismatch |
| `checkStrEq(name: string, got: string, want: string) -> int` | Assert string equality (by value) |
| `testSummary() -> int` | Print the summary line; return `0` if all passed, else `1` |

A complete test file:

```tocin
import std.testing;
import std.math;

def main() -> int {
    testBegin();
    check("13 is prime", isPrime(13));
    checkEq("gcd", gcd(12, 18), 6);
    checkStrEq("strings", "to" + "cin", "tocin");
    return testSummary();     // exit 0 if all checks passed, 1 otherwise
}
```

```text
$ TOCIN_PATH=stdlib ./build/tocin my_test.to --run
  ok   13 is prime
  ok   gcd
  ok   strings
3 passed, 0 failed
```

Because `main`'s return value is the process exit code, any script or CI job
can treat a nonzero exit as failure with no extra plumbing.

## How the stdlib test suite runs

Tests that need the full runtime (vectors, maps, strings, `alloc`, module
imports, `std.testing`) live in `tests/jit/` and run under the JIT via
`tests/run_stdlib_tests.sh`:

```sh
./tests/run_stdlib_tests.sh              # runs every tests/jit/*.to
./tests/run_stdlib_tests.sh some/dir     # or another directory of .to files
```

The runner executes each file with `tocin FILE.to --run`, defaulting
`TOCIN_PATH` to the repository's `stdlib/`. A file **passes when it exits 0**
— unless it declares an expected exit code in a comment:

```tocin
// expect: 7
```

in which case the exit code must equal that number. Files built on
`std.testing` need no annotation: `return testSummary();` already exits
nonzero when any check fails, so they self-report. The existing
`tests/jit/stdlib_*.to` files are good templates for new module tests.

## Which module do I need?

| Task | Import | Key functions |
|---|---|---|
| Integer math (gcd, primes, factorial) | `std.math` | `gcd`, `lcm`, `factorial`, `isPrime` |
| Sum / min / max of an int list | `std.list` | `listSum`, `listMin`, `listMax`, `listContains` |
| Map / filter / fold with lambdas | `std.functional` | `mapInts`, `filterInts`, `foldInts`, `rangeList` |
| Query-style reductions | `std.linq` | `reduceSum`, `aggregate`, `countGreater` |
| Trim, pad, parse strings | `std.strings` | `strTrim`, `strPadLeft`, `strParseIntOr` |
| Split / join / replace strings | `std.strseq` | `splitChar`, `joinStr`, `replaceAll` |
| Parse or emit JSON | `std.json` | `jsonParse`, `jsonGetString`, `jsonStringify` |
| Assert and report in tests | `std.testing` | `check`, `checkEq`, `testSummary` |
| Sort and search int lists | `data.algorithms` | `sort`, `binarySearch`, `argMax` |
| Stack, queue, set, counter | `data.structures` | `stackPush`, `enqueue`, `setAdd`, `countAdd` |
| Heap, union-find, BST, deque, bitset | `data.collections` | `heapPush`, `ufUnion`, `bstPut`, `pushBack` |
| Float helpers and constants | `math.basic` | `lerp`, `clampf`, `hypot`, `PI` |
| Matrices and vectors | `math.linear` | `matMul`, `vecDot`, `vecNorm` |
| 2-D/3-D geometry | `math.geometry` | `dist2`, `cross2`, `circleArea` |
| Statistics and regression | `math.stats`, `math.stats_advanced` | `mean`, `stddev`, `correlation`, `linearRegression` |
| Derivatives, integrals, roots | `math.differential` | `derivative`, `integrateSimpson`, `newtonRoot` |
| Neural-network numerics | `ml.neural_network`, `ml.deep_learning` | `denseForward`, `trainStep`, `argmax`, `accuracy` |
| Image processing | `ml.computer_vision` | `threshold`, `boxBlur`, `sobel`, `resize` |
| Sequence models (TEN) | `ml.ten` | `tenEigenInit`, `tenScan`, `tenLayerForward` |
| Serve HTTP / build responses | `web.http` | `httpRoute`, `buildResponse`, `serveLoop` |
| Make HTTP requests | `net.advanced` | `httpGet`, `httpPost`, `responseBody` |
| WebSocket frames | `web.websocket` | `writeFrame`, `frameOpcode`, `unmaskPayload` |
| In-memory KV store / tables | `database.database` | `kvPut`, `tableInsert`, `selectWhere`, `columnSum` |
| Entities, framebuffers, shading | `game.engine`, `game.graphics`, `game.shader` | `spawnEntity`, `drawLine`, `smoothstep` |
| Widget layout math | `gui.core`, `gui.widgets` | `buttonState`, `layoutRow`, `progressFill` |
| Audio synthesis / DSP | `audio.audio` | `genSine`, `applyEnvelope`, `lowpass`, `midiToFreq` |
| Memory-mapped GPIO | `embedded.gpio` | `pinMode`, `digitalWrite`, `digitalRead` |
| Templating and shell strings | `scripting.automation` | `renderTemplate`, `buildCommand`, `shellQuote` |
| Semantic versioning | `pkg.manager` | `compareVersions`, `satisfiesCaret`, `bestMatch` |

# Tocin Standard Library

The Tocin standard library is a set of 34 modules written in pure Tocin,
shipped as source under `stdlib/`. Every module builds on the compiler's
built-in functions (`println`, `len`, `sqrt`, `vecNew`, `mapNew`, `alloc`,
`tcpListen`, ...), which are always available with no import — see
[stdlib-reference.md](stdlib-reference.md) for that list. Every function name
and example in this document was verified against the in-tree source and, for
complete programs, by running them with `build/tocin FILE.to --run`.

## Imports and `TOCIN_PATH`

A dotted import maps to a relative file path; a string import is used as-is
(the trailing `.to` is added if absent):

```tocin
import std.math;          // resolves to std/math.to
import data.algorithms;   // resolves to data/algorithms.to
import "std/math";        // same file, string form
```

For each import the compiler searches, in order:

1. the directory of the importing file,
2. the directory named by the **`TOCIN_PATH`** environment variable, if set,
3. the compiled-in standard-library path.

The installers create a launcher that sets `TOCIN_PATH` to the installed
`stdlib/` automatically. When running from a source checkout, set it yourself:

```sh
TOCIN_PATH=/path/to/TocinLang/stdlib ./build/tocin program.to --run
```

**There is no namespacing.** An imported file's top-level declarations are
merged into the program, so you call imported functions by their bare names
(`gcd(12, 18)`, never `std.math.gcd(...)`). Stdlib names are prefixed by
module or data structure (`listSum`, `strTrim`, `heapPush`, `tenScan`) to keep
them unique. A few short names do repeat across distant domains (`countWhere`,
`mix`, `step`); importing two modules that define the same name into one
program is not supported — see [STDLIB_GUIDE.md](STDLIB_GUIDE.md).

Two conventions run through the whole library:

- **Booleans are `int`**: predicates return `1` (true) or `0` (false).
- **Handles are `int`**: structures built on raw buffers (`heapNew`,
  `tableNew`, `worldNew`, ...) return an `int` address; pass it to the
  structure's functions and release it with the matching `*Free`.

| Domain | Modules |
|---|---|
| `std` | `std/math`, `std/list`, `std/functional`, `std/linq`, `std/strings`, `std/strseq`, `std/json`, `std/testing` |
| `data` | `data/algorithms`, `data/structures`, `data/collections` |
| `math` | `math/basic`, `math/linear`, `math/geometry`, `math/stats`, `math/stats_advanced`, `math/differential` |
| `ml` | `ml/neural_network`, `ml/deep_learning`, `ml/computer_vision`, `ml/ten` |
| `web`, `net` | `web/http`, `web/websocket`, `net/advanced` |
| `database` | `database/database` |
| `game` | `game/engine`, `game/graphics`, `game/shader` |
| `gui` | `gui/core`, `gui/widgets` |
| `audio` | `audio/audio` |
| `embedded` | `embedded/gpio` |
| `scripting` | `scripting/automation` |
| `pkg` | `pkg/manager` |

## std — core utilities

The `std` modules cover everyday programming: integer math (`std.math`),
`list<int>` helpers (`std.list`), higher-order functions over int lists
(`std.functional`), query-style reductions (`std.linq`, see
[LINQ.md](LINQ.md)), string cleanup and parsing (`std.strings`), splitting and
joining (`std.strseq`), a JSON parser/serializer (`std.json`), and a test
harness (`std.testing`, see [STDLIB_GUIDE.md](STDLIB_GUIDE.md)).

| Function | Module | Description |
|---|---|---|
| `gcd(a: int, b: int) -> int` | `std.math` | Greatest common divisor |
| `isPrime(n: int) -> int` | `std.math` | Primality test |
| `listSum(xs: list<int>) -> int` | `std.list` | Sum of the elements |
| `listContains(xs: list<int>, target: int) -> int` | `std.list` | Membership test |
| `mapInts(xs: list<int>, f: (int) -> int) -> list<int>` | `std.functional` | Apply a function value to every element |
| `foldInts(xs: list<int>, init: int, f: (int, int) -> int) -> int` | `std.functional` | Left fold with a binary function |
| `reduceSum(xs: list<int>) -> int` | `std.linq` | Sum reduction |
| `strTrim(s: string) -> string` | `std.strings` | Strip surrounding whitespace |
| `strParseIntOr(s: string, fallback: int) -> int` | `std.strings` | Parse an integer, with fallback |
| `splitChar(s: string, delim: int) -> vector` | `std.strseq` | Split on a character code |
| `joinStr(parts: vector, sep: string) -> string` | `std.strseq` | Join a vector of strings |
| `jsonParse(s: string) -> int` | `std.json` | Parse JSON text into a node handle |

`std.json` follows the handle convention: `jsonParse` returns a node you read
with `jsonType`, `jsonAsInt`, `jsonAsString`, `jsonArrayGet`,
`jsonObjectGet`, or the shortcut accessors `jsonGetInt(v, key, dflt)` and
`jsonGetString(v, key, dflt)`; `jsonStringify(v)` serializes a node back to
text.

```tocin
import std.strseq;
import std.strings;

def main() {
    let parts = splitChar("alpha,beta,gamma", 44);   // ',' is char code 44
    println("{}", joinStr(parts, " | "));
    println("{}", strTrim("   padded   "));
    println("{}", strParseIntOr("42", 0) + strParseIntOr("oops", -1));
    return 0;
}
```

```text
alpha | beta | gamma
padded
41
```

## data — algorithms and containers

`data.algorithms` sorts and searches `list<int>` (quicksort, binary search,
arg-min/max, counting). `data.structures` wraps the `vector`/`map` builtins
into a stack, FIFO queue, integer set, and frequency counter.
`data.collections` builds classic structures on raw buffers: a binary
min-heap, union-find, bitset, ring buffer, string list, binary search tree,
and a double-ended queue.

| Function | Module | Description |
|---|---|---|
| `sort(xs: list<int>)` | `data.algorithms` | In-place quicksort |
| `binarySearch(xs: list<int>, target: int) -> int` | `data.algorithms` | Index in a sorted list, `-1` if absent |
| `argMax(xs: list<int>) -> int` | `data.algorithms` | Index of the largest element |
| `stackPush(s, x)` / `stackPop(s)` | `data.structures` | LIFO stack over a `vector` |
| `enqueue(q, x)` / `dequeue(q)` | `data.structures` | FIFO queue |
| `setAdd(s, x)` / `setContains(s, x)` | `data.structures` | Integer set over a `map` |
| `countAdd(c, x)` / `countOf(c, x)` | `data.structures` | Frequency counter |
| `heapPush(h, x)` / `heapPop(h)` | `data.collections` | Binary min-heap (priority queue) |
| `ufUnion(uf, a, b)` / `ufConnected(uf, a, b)` | `data.collections` | Union-find over disjoint sets |
| `bstPut(t, key, value)` / `bstGet(t, key, dflt)` | `data.collections` | Binary search tree map |
| `pushBack(d, x)` / `popFront(d)` | `data.collections` | Double-ended queue |
| `bitSet(bs, i)` / `bitGet(bs, i)` | `data.collections` | Fixed-size bitset |

```tocin
import data.algorithms;
import data.structures;

def main() {
    let xs = [9, 4, 7, 1, 4];
    sort(xs);
    println("{} {} {}", xs[0], xs[4], binarySearch(xs, 7));
    let s = stackNew();
    stackPush(s, 10);
    stackPush(s, 20);
    println("{} {}", stackPop(s), stackSize(s));
    return 0;
}
```

```text
1 9 3
20 1
```

## math — numerics

`math.basic` adds constants (`PI`, `TAU`, `E`, `SQRT2`, `EPSILON`) and float
and integer helpers on top of the math builtins. `math.linear` works on flat
row-major matrices stored in `list<float>`; `math.geometry` provides 2-D and
3-D vector math on scalar components; `math.stats` and `math.stats_advanced`
cover descriptive statistics through regression and the normal distribution;
`math.differential` does numeric calculus on function values.

| Function | Module | Description |
|---|---|---|
| `lerp(a: float, b: float, t: float) -> float` | `math.basic` | Linear interpolation |
| `clampf(x, lo, hi) -> float` | `math.basic` | Clamp a float to a range |
| `hypot(a: float, b: float) -> float` | `math.basic` | `sqrt(a*a + b*b)` without intermediate overflow |
| `ipow(base: int, n: int) -> int` | `math.basic` | Integer power |
| `matMul(a, b, dst, r, m, p)` | `math.linear` | Matrix product of flat row-major matrices |
| `vecDot(a, b) -> float` / `vecNorm(a) -> float` | `math.linear` | Dot product / Euclidean norm |
| `dist2(ax, ay, bx, by) -> float` | `math.geometry` | Distance between 2-D points |
| `circleArea(r: float) -> float` | `math.geometry` | Area of a circle |
| `mean(xs)` / `median(xs)` / `stddev(xs)` | `math.stats` | Descriptive statistics over `list<float>` |
| `correlation(x, y) -> float` | `math.stats_advanced` | Pearson correlation |
| `linearRegression(x, y, out)` | `math.stats_advanced` | Least squares; writes slope to `out[0]`, intercept to `out[1]` |
| `derivative(f: (float) -> float, x: float) -> float` | `math.differential` | Numeric derivative of a function value |

`math.differential` also offers `integrateTrapezoid`, `integrateSimpson`,
`newtonRoot`, `bisectRoot`, and `eulerIntegrate` — all taking
`(float) -> float` function values (named functions or lambdas).

```tocin
import math.stats;
import math.linear;

def main() {
    let xs = [2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0];
    println("mean {}  stddev {}", mean(xs), stddev(xs));
    let a = [1.0, 2.0, 2.0];
    println("norm {}", vecNorm(a));
    return 0;
}
```

```text
mean 5  stddev 2
norm 3
```

## ml — machine learning

`ml.neural_network` implements the numeric core of a multilayer perceptron:
activations, a dense forward pass, softmax, MSE and cross-entropy losses, and
`trainStep` — one in-place SGD step of a one-hidden-layer sigmoid MLP that
returns the pre-update loss. `ml.deep_learning` adds evaluation metrics,
weight initialization, and learning-rate schedules; `ml.computer_vision`
processes grayscale images stored in raw byte buffers.

| Function | Module | Description |
|---|---|---|
| `sigmoid(x)` / `reluf(x)` / `tanhf(x)` | `ml.neural_network` | Activation functions |
| `softmax(src, dst)` | `ml.neural_network` | Softmax over a `list<float>` |
| `denseForward(w, b, x, out, outN, inN)` | `ml.neural_network` | Dense layer: `out = W*x + b` |
| `mseLoss(pred, target)` / `crossEntropy(pred, target)` | `ml.neural_network` | Loss functions |
| `trainStep(w1, b1, w2, b2, x, y, h, o, inN, hidN, outN, lr) -> float` | `ml.neural_network` | One SGD step of a 2-layer sigmoid MLP |
| `argmax(v: list<float>) -> int` | `ml.deep_learning` | Index of the largest score |
| `accuracy(preds, labels, rows, classes) -> float` | `ml.deep_learning` | Classification accuracy |
| `heScale(fanIn)` / `xavierScale(fanIn)` / `initWeights(w, scale, seed)` | `ml.deep_learning` | Weight initialization |
| `lrExpDecay(lr0, decay, epoch)` / `lrStepDecay(lr0, factor, stepEpochs, epoch)` | `ml.deep_learning` | Learning-rate schedules |
| `threshold` / `boxBlur` / `sobel` / `resize` | `ml.computer_vision` | Grayscale image operations on byte buffers |

### ml.ten — Temporal Eigenstate Networks

`ml.ten` is an implementation of **Temporal Eigenstate Networks (TEN)**: a
sequence model that replaces attention with a spectral recurrence. Each layer
projects the (layer-normed) input to K complex eigenstate drives, evolves them
with a diagonal complex recurrence `c_k(t) = lambda_k * c_k(t-1) + beta_k(t)`
where each `lambda_k = mag_k * e^(i*freq_k)` is a learned per-head complex
eigenvalue, mixes the eigenstates with per-head block-diagonal coupling, and
reconstructs the hidden state through a gated output projection plus a SiLU
MLP, both with residual connections. The result is an exact `O(T*K)` causal
convolution instead of `O(T^2)` attention. All tensors are flat row-major
`list<float>`, batch size 1, with caller-owned weights.

Key entry points: `tenEigenInit(mag, freq, K)` parameterizes the eigenvalues;
`tenScan(br, bi, mag, freq, cr, ci, T, K)` runs the complex recurrence
(`tenScanRegions` is a variant reading both drive halves from one buffer);
`tenMix(cr, ci, coupling, scratch, T, K, heads)` applies the per-head
coupling; `tenLayerNorm(x, out, T, D)` is the pre-norm;
`tenLayerForward(...)` chains the full layer; and
`tenClosedFormReal`/`tenClosedFormImag` give the analytic impulse response
used to validate the scan.

```tocin
import ml.ten;

def main() {
    let mag = [0.0, 0.0];
    let freq = [0.0, 0.0];
    tenEigenInit(mag, freq, 2);                       // K = 2 eigenstates
    let br = [1.0, 1.0, 0.0, 0.0, 0.0, 0.0];          // impulse at t = 0 (T x K)
    let bi = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    let cr = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    let ci = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    tenScan(br, bi, mag, freq, cr, ci, 3, 2);         // T = 3 steps
    println("state 0: {} {} {}", cr[0], cr[2], cr[4]);
    return 0;
}
```

```text
state 0: 1 0.0474259 0.00224921
```

## web and net — HTTP and WebSocket

`web.http` parses request lines (`httpMethod`, `httpPath`, `httpRoute`),
builds HTTP/1.1 responses, and runs a blocking single-threaded server on the
`tcp*` builtins where the handler is a `(string) -> string` function value.
`web.websocket` encodes and decodes WebSocket frames in raw buffers.
`net.advanced` is the client side: URL parsing and one-shot `httpGet`/
`httpPost` requests.

| Function | Module | Description |
|---|---|---|
| `httpMethod(req)` / `httpPath(req)` | `web.http` | Parse the request line |
| `httpRoute(req, method, path) -> int` | `web.http` | Does the request match method + path? |
| `buildResponse(status, contentType, body) -> string` | `web.http` | Build a full HTTP/1.1 response |
| `ok(body)` / `okJson(body)` / `notFound()` | `web.http` | Response shorthands |
| `serve(port)` / `serveOnce(listenFd, handler)` / `serveLoop(listenFd, handler, count)` | `web.http` | Blocking server; handler is `(string) -> string` |
| `writeFrame(dst, opcode, src, len, masked, maskKey)` | `web.websocket` | Encode a WebSocket frame |
| `frameOpcode(frame)` / `framePayloadLen(frame)` / `framePayloadOffset(frame)` | `web.websocket` | Decode a frame header |
| `unmaskPayload(frame)` | `web.websocket` | Unmask a client frame in place |
| `httpGet(url)` / `httpPost(url, contentType, body)` | `net.advanced` | One-shot HTTP client requests |
| `urlHost(url)` / `urlPort(url)` / `urlPath(url)` | `net.advanced` | URL parsing |
| `responseStatus(resp)` / `responseBody(resp)` | `net.advanced` | Split a raw HTTP response |

## database — in-memory storage

`database.database` provides a string key/value store over the `map` builtins
and a typed-row table engine (fixed column count, `int` cells) with insert,
scan, select, and aggregation — the relational core, with no disk or SQL
parser. Note: it defines `countWhere(t, col, value)`, which clashes with
`std.functional`'s `countWhere`; do not import both modules into one program.

| Function | Description |
|---|---|
| `kvNew()` / `kvPut(db, key, value)` / `kvGet(db, key, dflt)` / `kvHas(db, key)` | String key/value store |
| `tableNew(ncols, capacityRows) -> int` | Create a table handle |
| `tableInsert(t, row: list<int>)` | Append a row |
| `tableGet(t, row, col)` / `tableSet(t, row, col, value)` | Cell access |
| `selectWhere(t, col, value, out)` / `selectGreater(t, col, threshold, out)` | Collect matching row indices into a `vector` |
| `columnSum(t, col)` / `columnMax(t, col)` / `columnMin(t, col)` | Column aggregates |
| `countWhere(t, col, value) -> int` | Count rows with `row[col] == value` |
| `tableDelete(t, row)` / `tableFree(t)` | Remove a row / release the table |

## game — entities, framebuffers, shading math

`game.engine` is a small entity world (positions and velocities stored
fixed-point in one buffer) with spawning, integration steps, forces, and AABB
collision tests. `game.graphics` draws into a raw RGBA framebuffer:
rectangles, lines (Bresenham), and circles. `game.shader` collects
GLSL-style shading math (`smoothstep`, `mix`, reflection, Lambert and
Blinn-Phong lighting, color packing). `game.shader` reuses the names `step`,
`mix`, and `fract` — import it separately from `game.engine`, `audio.audio`,
and `math.basic`.

| Function | Module | Description |
|---|---|---|
| `worldNew(cap)` / `spawnEntity(w, x, y, vx, vy)` | `game.engine` | Create a world / add an entity |
| `step(w, dt)` / `applyForce(w, ax, ay, dt)` | `game.engine` | Integrate positions / accelerate everything |
| `entitiesCollide(w, e1, e2, size)` / `aabbOverlap(...)` | `game.engine` | Collision tests |
| `fixedSteps(frameTime, fixedDt) -> int` | `game.engine` | Fixed-timestep step count |
| `createFramebuffer(width, height) -> int` | `game.graphics` | Allocate an RGBA framebuffer |
| `setPixel` / `fillRect` / `drawLine` / `fillCircle` / `clear` | `game.graphics` | Drawing primitives |
| `smoothstep(edge0, edge1, x)` / `mix(a, b, t)` | `game.shader` | Interpolation curves |
| `lambert(...)` / `blinnPhong(...)` | `game.shader` | Diffuse / specular lighting terms |
| `packColor(r, g, b, a) -> int` / `unpackChannel(color, c)` | `game.shader` | RGBA8 color packing |
| `luminance(r, g, b) -> float` | `game.shader` | Perceptual brightness |

## gui — layout and widget math

`gui.core` supplies rectangle tests, a vertical layout cursor, and
immediate-mode widget state: `buttonState` folds mouse position and button
into hover/pressed/active flags (`WS_HOVERED`, `WS_PRESSED`, `WS_ACTIVE`),
and `sliderValue` maps a mouse position onto a value range. `gui.widgets`
computes text metrics, alignment, progress-bar fill, scrollbar thumb geometry,
grid cells, and flex item sizes — the math layer a renderer plugs into.

| Function | Module | Description |
|---|---|---|
| `rectContains(x, y, w, h, px, py)` / `rectsOverlap(...)` | `gui.core` | Hit tests |
| `layoutNew(x, y, spacing)` / `layoutRow(l, h)` | `gui.core` | Vertical layout cursor |
| `buttonState(x, y, w, h, mx, my, mouseDown)` / `buttonClicked(state)` | `gui.core` | Immediate-mode button state |
| `sliderValue(trackX, trackW, mx, minV, maxV)` / `sliderKnobX(...)` | `gui.core` | Slider position math |
| `textWidth(n)` / `textHeight()` | `gui.widgets` | Monospace text metrics |
| `progressFill(trackW, fraction)` | `gui.widgets` | Progress-bar fill width |
| `scrollThumbLen(...)` / `scrollThumbPos(...)` | `gui.widgets` | Scrollbar thumb geometry |
| `gridCell(...)` / `flexItemSize(total, n, gap)` / `wrapLines(charCount, widthChars)` | `gui.widgets` | Grid, flex, and wrapping layout |

## audio — synthesis and DSP

`audio.audio` generates and processes sample buffers (`list<float>`):
oscillators, gain staging, mixing, clipping, attack/release envelopes,
metering, a one-pole lowpass, and MIDI note conversion.

| Function | Description |
|---|---|
| `genSine(buf, freq, sampleRate, amp)` / `genSquare(...)` / `genSaw(...)` | Fill a buffer with an oscillator waveform |
| `gain(buf, g)` | Scale all samples |
| `mix(dst, src, level)` | Mix `src` into `dst` at a level |
| `clip(buf, limit)` | Hard-clip samples to `[-limit, limit]` |
| `applyEnvelope(buf, attack, release)` | Linear attack/release envelope |
| `rms(buf)` / `peak(buf)` | Level metering |
| `normalize(buf, target)` | Scale so the peak hits `target` |
| `lowpass(buf, a)` | One-pole lowpass filter in place |
| `midiToFreq(note) -> float` | MIDI note number to frequency |

## embedded — memory-mapped GPIO

`embedded.gpio` drives a memory-mapped GPIO register block through the
volatile access builtins (`volatileLoad32`/`volatileStore32`/`fence`), so
reads and writes are never optimized away or reordered — the pattern a
bare-metal or RTOS driver needs (pair with `--freestanding`). Register
offsets (`GPIO_DIR`, `GPIO_OUT`, `GPIO_IN`, `GPIO_SET`, `GPIO_CLR`) follow a
common MMIO layout; adapt `base` to your SoC.

| Function | Description |
|---|---|
| `pinMode(base, pin, output)` | Configure a pin as input or output |
| `digitalWrite(base, pin, high)` | Drive a pin via the atomic set/clear registers |
| `digitalRead(base, pin) -> int` | Read a pin level |
| `toggle(base, pin)` | Flip an output pin |
| `writePort(base, value)` / `readPort(base)` | Whole-port access |
| `barrier()` | Memory fence between device accesses |

## scripting — text automation

`scripting.automation` is string plumbing for scripts and code generation:
`{{key}}` template rendering, shell command assembly with quoting,
environment expansion, and `key=value` config-line parsing. Actual process
spawning belongs behind FFI.

| Function | Description |
|---|---|
| `renderTemplate(tmpl, keys, vals) -> string` | Replace every `{{key}}` placeholder |
| `buildCommand(program, args) -> string` | Assemble a command line, double-quoting arguments with spaces |
| `shellQuote(s) -> string` | POSIX single-quote a string for the shell |
| `expandVar(name) -> string` | Read an environment variable |
| `repeatStr(s, count) -> string` | Repeat a string |
| `configKey(line)` / `configValue(line)` | Split a `key=value` line |

## pkg — semantic versioning

`pkg.manager` implements semver parsing and constraint matching for package
tooling: split versions into fields, compare them, and test caret/tilde
ranges.

| Function | Description |
|---|---|
| `major(v)` / `minor(v)` / `patch(v)` | Extract version fields from `"1.2.3"` |
| `versionCode(v) -> int` | Encode a version as one comparable integer |
| `compareVersions(a, b) -> int` | Three-way comparison |
| `versionEq(a, b)` / `versionGt(a, b)` / `versionLt(a, b)` | Comparison predicates |
| `satisfiesCaret(version, base)` | `^base` range test (left-most non-zero field fixed) |
| `satisfiesTilde(version, base)` | `~base` range test (patch-level changes only) |
| `satisfies(version, base, op)` | Range test by operator code |
| `bestMatch(candidates, baseCode, op) -> int` | Pick the best matching version code |

## Built-in functions

Everything above is ordinary Tocin source layered on the runtime builtins —
`print`/`println`, `len`, math (`sqrt`, `sin`, `pow`, ...), strings
(`strLen`, `substr`, `startsWith`, ...), dynamic collections (`vecNew`,
`mapNew`, ...), raw memory (`alloc`, `loadInt`, `storeByte`, ...), file I/O,
and TCP sockets. These need no import and are documented with verified
signatures in [stdlib-reference.md](stdlib-reference.md).

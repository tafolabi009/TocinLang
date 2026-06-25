# Tocin Standard Library & Builtins Reference

This is the reference for Tocin's built-in functions and bundled standard-library
modules. Every signature, return value, and edge case below was verified by
compiling and running a snippet with:

```sh
./build/tocin FILE.to --run
```

A program's entry point is `def main()`; its `return` value becomes the process
exit code. Examples below show `main` only where the behavior depends on it.

## How to read this document

- **Builtins** are recognized by name directly in the code generator
  (`src/codegen/ir_generator.cpp`); their runtime behavior lives in
  `src/runtime/concurrency_runtime.cpp` (`__tocin_*` functions). They are always
  available with no `import`.
- **Stdlib modules** are ordinary Tocin source under `stdlib/std/*.to`. Bring
  them in with `import std.<name>;` (the compiler maps `std.math` to
  `std/math.to` and finds it on the bundled stdlib path).
- **Booleans are `int`**: `0` is false, `1` is true. Many builtins and all
  bundled stdlib predicates return `0`/`1`.

---

## Integer literals

Integer literals may be written in decimal or with a base prefix, and digit
groups may be separated with underscores (`_`) for readability in any base:

| Form | Example | Value |
|---|---|---|
| Decimal | `42`, `1_000_000` | 42, 1000000 |
| Hex (`0x`) | `0xFF`, `0xDE_AD` | 255, 57005 |
| Octal (`0o`) | `0o17` | 15 |
| Binary (`0b`) | `0b1010`, `0b1111_0000` | 10, 240 |

```to
def main() {
    println("{} {} {} {}", 0xFF, 0o17, 0b1010, 1_000_000); // 255 15 10 1000000
    return 0;
}
```

The underscores are ignored by the lexer; they only group digits. Compound
assignment (`+= -= *= /= %=`) and the bitwise operators (`& | ^ << >> ~`) work on
integers as expected.

---

## Memory & ABI notes (read first)

These properties recur throughout and are not repeated in every entry:

- **64-bit slots.** Dynamic collection elements, channel values, and
  `Option`/`Result` payloads are all stored as a single 64-bit slot. Integers go
  in directly; doubles are bit-cast into the slot; pointers/strings are stored as
  their address (an `i64`). There is no per-slot type tag.
- **Storing a pointer/string in a vector or map is limited.** The address
  round-trips as an `i64`, so reading it back gives you an integer, not a usable
  string — the language exposes no way to cast that integer back to a pointer.
  Use these collections for integer data (and booleans-as-int).
- **Strings are NUL-terminated `char*`.** Every string-returning builtin returns
  a *fresh* `malloc`'d buffer that never aliases its input.
- **Leaks.** There is no garbage collector. String results (`substring`,
  `intToStr`, `charToStr`, `+` concatenation, `readFile`, `readLine`, ...) and
  collection handles (`vecNew`, `mapNew`) are heap-allocated and leak unless you
  free them. Vectors/maps have explicit `vecFree`/`mapFree`; strings have no free
  builtin, so string-heavy loops leak by design.
- **`len` is for arrays only** (see below); use `strLen` for strings.

---

## I/O & formatting

| Function | Signature | Description |
|---|---|---|
| `print` | `print(...)` | Print arguments with no trailing newline. |
| `println` | `println(...)` | Print arguments followed by a newline. |
| `readLine` | `readLine() -> string` | Read one line from stdin (newline stripped). |

`print`/`println` support **two calling styles**:

1. **Format style** — the *first argument is a string literal containing `{}`*.
   Each `{}` is replaced, left to right, by the remaining arguments. `\n`, `\t`,
   `\r` escapes inside the literal are honored.
2. **Sequential style** — otherwise, every argument is printed in turn with no
   separator.

How each type prints (the placeholder/value adapts to the argument's type):

- **int** → decimal (`%lld`).
- **string / pointer** → the string (`%s`).
- **float/double** → `%g` (compact: `1e+06`, `0.0001`, `2.5`, `3.14159`).

Verified example:

```to
def main() {
    print("no newline ");
    println("hi");                                  // -> "no newline hi"
    println("int={} str={} float={}", 7, "abc", 3.5); // int=7 str=abc float=3.5
    println(42);                                    // 42        (sequential)
    println(3.14159);                               // 3.14159
    println("x={}, y={}, sum={}", 10, 20, 30);      // x=10, y=20, sum=30
    return 0;
}
```

`readLine` (verified with `printf 'Tocin\n' | ./build/tocin file.to --run`):

```to
def main() {
    let name = readLine();        // reads "Tocin"
    println("hello {}", name);    // hello Tocin
    println("len={}", strLen(name)); // len=5  (newline is stripped)
    return 0;
}
```

**Edge cases / gotchas**

- The format string must be a **literal**. A `string` *variable* containing `{}`
  is NOT treated as a format — its `{}` is printed literally and any extra
  arguments are appended sequentially:
  `let f = "x={}"; println(f, 5);` prints `x={}5`.
- **Too few arguments** for the placeholders: the leftover `{}` is printed
  verbatim. `println("a={} b={}", 1);` prints `a=1 b={}`.
- A literal `%` in a format string is emitted safely (escaped to `%%`).
- `readLine` returns a fresh buffer; at EOF with no input it returns an empty
  string. The result leaks (no free builtin for strings).

---

## `len` — array length only

| Function | Signature | Description |
|---|---|---|
| `len` | `len(a: array) -> int` | Number of elements in a fixed array literal. |

`len` reads the 64-bit length stored in the array header (offset 0). It is for
**arrays only** (e.g. `[1, 2, 3]` and `list<int>` parameters).

```to
def main() {
    let a = [10, 20, 30, 40];
    println("len={}", len(a));   // len=4
    return len(a);               // exit code 4
}
```

**Do NOT call `len` on a string.** A string is a `char*` to raw bytes with no
length header, so `len(someString)` reinterprets the first 8 bytes of text as an
integer and returns garbage (e.g. `len("hello")` printed
`7308216773875885416` in testing). Likewise, `len` on a string returned by
`readFile` gives garbage. Use `strLen` for strings (and `vecLen`/`mapLen` for
dynamic collections).

---

## Math builtins

Unary `double -> double` (integer arguments are auto-promoted to `double`):

`sqrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log2`, `log10`,
`floor`, `ceil`, `round`, `fabs`

Plus:

| Function | Signature | Description |
|---|---|---|
| `pow` | `pow(base, exp) -> double` | `base` raised to `exp` (both promoted to double). |
| `abs` | `abs(x) -> typeof x` | Absolute value; integer in → integer out, float in → float out. |
| `min` | `min(a, b) -> typeof a` | Smaller of two **same-type** numbers. |
| `max` | `max(a, b) -> typeof a` | Larger of two **same-type** numbers. |

> **Shadowing:** these names shadow any same-named `extern` declaration. The
> compiler intercepts the call and emits the math intrinsic / inline comparison
> directly (for the libm ones it calls the C library function of the same name),
> so you cannot override them with your own `extern`.

Verified example:

```to
def main() {
    println("sqrt(2)={}",   sqrt(2.0));    // 1.41421
    println("sqrt(9 int)={}", sqrt(9));    // 3        (int promoted)
    println("sin(0)={}",    sin(0.0));     // 0
    println("atan(1)={}",   atan(1.0));    // 0.785398
    println("exp(1)={}",    exp(1.0));     // 2.71828
    println("log2(8)={}",   log2(8.0));    // 3
    println("log10(1000)={}", log10(1000.0)); // 3
    println("floor(3.7)={}", floor(3.7));  // 3
    println("ceil(3.2)={}",  ceil(3.2));   // 4
    println("round(3.5)={}", round(3.5));  // 4
    println("fabs(-4.5)={}", fabs(-4.5));  // 4.5
    println("pow(2,10)={}",  pow(2, 10));  // 1024   (ints promoted)
    println("abs(-5)={}",    abs(-5));     // 5
    println("abs(-2.5)={}",  abs(-2.5));   // 2.5
    println("min(3,7)={}",   min(3, 7));   // 3
    println("max(2.5,3.5)={}", max(2.5, 3.5)); // 3.5
    return 0;
}
```

**Edge cases / gotchas**

- **`log` does NOT work as a function.** `log` is a reserved keyword
  (`TokenType::LOG`), so `log(x)` is a *parse error* ("Expected expression"),
  even though the codegen lists it. Use `log2`/`log10`, or compute a natural log
  as `log2(x) / log2(2.718281828...)`. (`log2` and `log10` are fine — they are
  not keywords.)
- **`min`/`max` require both arguments to be the same numeric type.** A mixed
  call like `max(3, 2.5)` (int + float) produces an invalid-IR compile error
  (`C002`). Make both `int` or both `float` (e.g. `max(3.0, 2.5)`).
- `abs` preserves the operand type (no promotion to double); `pow` always returns
  a `double` and promotes integer arguments.
- The unary functions promote integer arguments to `double` automatically.

---

## Strings

Strings are NUL-terminated `char*`. Index/length operations are **byte**-based
(no UTF-8 codepoint awareness). Functions that return a string return a fresh
`malloc`'d buffer (it never aliases the input), which leaks unless your program
exits. `NULL` inputs are tolerated by the runtime.

**Equality compares by value.** For strings, `==` and `!=` compare the *contents*
(byte-for-byte), not the pointer addresses — so `"hel" + "lo" == "hello"` is true.
Use plain `==`/`!=` for simple equality; `strEq` is still available and does the
same thing, while `strCmp` is for *ordering* (less/equal/greater). (Value
comparison applies only to strings; other pointer types — class instances, null —
still compare by identity.)

**Compound assignment works on strings.** `s += "x"` appends (it is sugar for
`s = s + "x"`, allocating a fresh concatenated buffer).

| Function | Signature | Description |
|---|---|---|
| `strLen` | `strLen(s: string) -> int` | Length in bytes (0 for null). |
| `charAt` | `charAt(s: string, i: int) -> int` | Byte value at index `i`; `-1` if out of range. |
| `substring` | `substring(s: string, start: int, len: int) -> string` | Up to `len` bytes from `start`; bounds clamped. |
| `strEq` | `strEq(a: string, b: string) -> int` | `1` if equal, else `0`. (Or just use `==`.) |
| `strCmp` | `strCmp(a: string, b: string) -> int` | `-1`/`0`/`1` (lexicographic, like C `strcmp` normalized). Use for **ordering**. |
| `indexOfChar` | `indexOfChar(s: string, c: int) -> int` | Index of first byte equal to `c`, else `-1`. |
| `intToStr` | `intToStr(n: int) -> string` | Decimal text of `n` (fresh buffer). |
| `strToInt` | `strToInt(s: string) -> int` | Parse leading integer; `0` if not numeric. |
| `charToStr` | `charToStr(c: int) -> string` | 1-byte string holding byte `c` (fresh buffer). |
| `==` / `!=` (operator) | `string == string -> int` | Value (content) equality / inequality; `1`/`0`. |
| `+` (operator) | `string + string -> string` | Concatenation (fresh buffer). |
| `+=` (operator) | `s += string` | Append in place (`s = s + ...`). |

Verified example:

```to
def main() {
    let s = "hello";
    println("strLen={}",      strLen(s));         // 5
    println("charAt(0)={}",   charAt(s, 0));      // 104  ('h')
    println("charAt(4)={}",   charAt(s, 4));      // 111  ('o')
    println("charAt(5)={}",   charAt(s, 5));      // -1   (out of range)
    println("charAt(-1)={}",  charAt(s, -1));     // -1
    println("substring(1,3)={}",   substring(s, 1, 3));   // ell
    println("substring(0,100)={}", substring(s, 0, 100)); // hello (clamped)
    println("substring(3,99)={}",  substring(s, 3, 99));  // lo
    println("strEq(hello,hello)={}", strEq(s, "hello"));  // 1
    println("strEq(hello,world)={}", strEq(s, "world"));  // 0
    println("strCmp(a,b)={}", strCmp("a", "b"));  // -1
    println("strCmp(b,a)={}", strCmp("b", "a"));  // 1
    println("strCmp(a,a)={}", strCmp("a", "a"));  // 0
    println("indexOfChar('l')={}", indexOfChar(s, 108)); // 2
    println("indexOfChar('z')={}", indexOfChar(s, 122)); // -1
    println("intToStr(1234)={}",  intToStr(1234)); // 1234
    println("intToStr(-9)={}",    intToStr(-9));   // -9
    println("strToInt(\"567\")={}", strToInt("567")); // 567
    println("strToInt(\"abc\")={}", strToInt("abc")); // 0
    println("charToStr(65)={}", charToStr(65));    // A
    let cat = "foo" + "bar" + "!";
    println("concat={}", cat);                     // foobar!

    let built = "hel" + "lo";
    println("eq by value={}", strEq(built, "hello")); // 1
    if built == "hello" { println("== works"); }   // == works   (value comparison)
    if built != "world" { println("!= works"); }   // != works
    if intToStr(42) == "42" { println("intToStr=="); } // intToStr==

    let s = "foo";
    s += "bar";                                    // compound append
    s += "!";
    println("appended={} len={}", s, strLen(s));   // appended=foobar! len=7
    return 0;
}
```

**Edge cases / gotchas**

- **`==` / `!=` on strings compare by value** (contents), not pointer identity,
  so `("a"+"b") == "ab"` is `1`. Use plain `==`/`!=` for equality; `strEq` is the
  same and `strCmp` is for ordering. (Only strings get value comparison; other
  pointer types still compare by address.)
- **`+=` appends to a string** (`s += "x"` ⇒ `s = s + "x"`), allocating a fresh
  buffer each time (these accumulate / leak in a loop).
- `charAt` returns a **byte value** (0–255), not a 1-char string. Out-of-range
  (including negative) returns `-1`. Use `charToStr(charAt(s, i))` to get a
  string.
- `substring` **clamps**: `start` is pinned to `[0, len]`, `len` to
  `[0, remaining]`. It always returns a valid (possibly empty) fresh string.
- `strCmp` returns exactly `-1`, `0`, or `1` (the raw C `strcmp` result is
  normalized).
- `strToInt` uses C `atoll` semantics: it parses a leading optionally-signed
  integer and returns `0` for non-numeric input (`"abc"` → `0`).
- `intToStr`, `charToStr`, `substring`, and `+` each allocate a fresh buffer
  (these results leak; there is no string-free builtin).
- **Concatenation `+` requires both operands to be strings.** `"n=" + 5`
  (string + int) is a compile error (`T006`: operator `+` cannot be applied to
  `string` and `int`). Convert first: `"n=" + intToStr(5)` → `n=5`.

---

## Dynamic collections

Two growable containers behind **opaque handles**. A handle is a pointer; when a
handle is passed to a user function, annotate the parameter as `v: vector` or
`m: map`. Elements/keys/values are 64-bit slots (integers stored directly;
see the Memory & ABI note about storing pointers/strings).

These leak unless you call the matching `*Free`.

### Vector (`std::vector<int64_t>`)

| Function | Signature | Description |
|---|---|---|
| `vecNew` | `vecNew() -> vector` | Allocate a new empty vector. |
| `vecPush` | `vecPush(v: vector, x: int)` | Append `x`. |
| `vecGet` | `vecGet(v: vector, i: int) -> int` | Element at `i`; `0` if out of range. |
| `vecSet` | `vecSet(v: vector, i: int, x: int)` | Overwrite element `i`; no-op if out of range. |
| `vecLen` | `vecLen(v: vector) -> int` | Number of elements. |
| `vecPop` | `vecPop(v: vector) -> int` | Remove and return the last element; `0` if empty. |
| `vecFree` | `vecFree(v: vector)` | Free the vector. |

Verified example:

```to
def main() {
    let v = vecNew();
    vecPush(v, 10);
    vecPush(v, 20);
    vecPush(v, 30);
    println("vecLen={}",   vecLen(v));    // 3
    println("vecGet(0)={}", vecGet(v, 0)); // 10
    println("vecGet(99)={}", vecGet(v, 99)); // 0   (out of range)
    println("vecGet(-1)={}", vecGet(v, -1)); // 0
    vecSet(v, 1, 99);
    println("vecGet(1)={}", vecGet(v, 1)); // 99
    vecSet(v, 50, 7);                      // out of range -> no-op
    println("vecLen={}",   vecLen(v));    // 3   (unchanged)
    println("vecPop={}",   vecPop(v));    // 30
    println("vecLen={}",   vecLen(v));    // 2
    vecFree(v);

    let empty = vecNew();
    println("pop empty={}", vecPop(empty)); // 0
    vecFree(empty);
    return 0;
}
```

Passing a handle to a function (parameter typed as `vector`):

```to
def total(v: vector) -> int {
    let s = 0;
    let i = 0;
    while i < vecLen(v) { s = s + vecGet(v, i); i = i + 1; }
    return s;
}
def main() {
    let v = vecNew();
    vecPush(v, 1); vecPush(v, 2); vecPush(v, 3);
    println("total={}", total(v)); // total=6
    vecFree(v);
    return 0;
}
```

### Map (one handle holds two independent namespaces)

A single map holds **two independent dictionaries**: an integer-keyed one and a
string-keyed one. `mapPut`/`mapGet`/`mapHas` use integer keys; the `*Str`
variants use string keys. The two namespaces do not collide — integer key `65`
and string key `"65"` are different entries. `mapLen` returns the combined count.

| Function | Signature | Description |
|---|---|---|
| `mapNew` | `mapNew() -> map` | Allocate a new empty map. |
| `mapPut` | `mapPut(m: map, k: int, v: int)` | Set int-keyed entry `k = v`. |
| `mapGet` | `mapGet(m: map, k: int) -> int` | Int-keyed value; `0` if key missing. |
| `mapHas` | `mapHas(m: map, k: int) -> int` | `1` if int key present, else `0`. |
| `mapPutStr` | `mapPutStr(m: map, k: string, v: int)` | Set string-keyed entry `k = v`. |
| `mapGetStr` | `mapGetStr(m: map, k: string) -> int` | String-keyed value; `0` if key missing. |
| `mapHasStr` | `mapHasStr(m: map, k: string) -> int` | `1` if string key present, else `0`. |
| `mapLen` | `mapLen(m: map) -> int` | Total entries (int-keyed + string-keyed). |
| `mapFree` | `mapFree(m: map)` | Free the map. |

Verified example:

```to
def main() {
    let m = mapNew();
    mapPut(m, 1, 100);
    mapPut(m, 2, 200);
    println("mapGet(1)={}",  mapGet(m, 1));   // 100
    println("mapGet(99)={}", mapGet(m, 99));  // 0   (missing)
    println("mapHas(1)={}",  mapHas(m, 1));   // 1
    println("mapHas(99)={}", mapHas(m, 99));  // 0

    mapPutStr(m, "alpha", 11);
    mapPutStr(m, "beta", 22);
    println("mapGetStr(alpha)={}",   mapGetStr(m, "alpha"));   // 11
    println("mapGetStr(missing)={}", mapGetStr(m, "missing")); // 0
    println("mapHasStr(beta)={}",    mapHasStr(m, "beta"));    // 1
    println("mapLen={}", mapLen(m));   // 4   (2 int + 2 string)

    mapPut(m, 1, 999);                 // overwrite
    println("mapGet(1)={}", mapGet(m, 1)); // 999

    // int key 65 and string key "65" are independent:
    mapPut(m, 65, 1);
    mapPutStr(m, "65", 2);
    println("int 65 -> {}", mapGet(m, 65));     // 1
    println("str 65 -> {}", mapGetStr(m, "65")); // 2

    mapFree(m);
    return 0;
}
```

**Edge cases / gotchas**

- `vecGet` out of range (including negative) → `0`; `vecSet` out of range →
  silent no-op (does not grow the vector). `vecPop` on empty → `0`.
- `mapGet`/`mapGetStr` on a missing key → `0` (indistinguishable from a stored
  `0`; use `mapHas`/`mapHasStr` to check presence).
- Re-`Put` overwrites the existing value.
- Storing a string/pointer as an element/value stores its address as an `i64`;
  reading it back gives an integer you can't convert back to a string. Keep these
  collections integer-valued.

---

## File I/O

Strings are NUL-terminated `char*`. Read results are fresh `malloc`'d buffers
owned by the caller (they leak). Write/append return the number of bytes written,
or `-1` on error.

| Function | Signature | Description |
|---|---|---|
| `readFile` | `readFile(path: string) -> string` | Whole file as a string; empty string on error. |
| `writeFile` | `writeFile(path: string, contents: string) -> int` | Truncate+write; bytes written, or `-1`. |
| `appendFile` | `appendFile(path: string, contents: string) -> int` | Append; bytes written, or `-1`. |
| `readLine` | `readLine() -> string` | One line from stdin (see I/O section). |

Verified example (paths under a writable scratch dir):

```to
def main() {
    let path = "/tmp/tocin_io.txt";
    let w = writeFile(path, "line1\n");
    println("wrote bytes={}", w);          // wrote bytes=6
    let a = appendFile(path, "line2\n");
    println("appended bytes={}", a);       // appended bytes=6
    let c = readFile(path);
    print(c);                              // line1\nline2\n
    println("strLen={}", strLen(c));       // strLen=12

    let missing = readFile("/no/such/file.txt");
    println("missing strLen={}", strLen(missing)); // 0   (empty string)

    let bad = writeFile("/no/such/dir/x.txt", "data");
    println("bad write={}", bad);          // -1
    return 0;
}
```

**Edge cases / gotchas**

- A failed `readFile` (missing file, seek/read error) returns an **empty
  string**, never null. Inspect it with `strLen` — do **not** use `len` (it would
  read garbage from the string bytes).
- `writeFile`/`appendFile` return `-1` if the file can't be opened (e.g. bad
  directory) or the write is short; otherwise the byte count.
- `writeFile` truncates; `appendFile` adds to the end.
- Files are opened in binary mode; bytes are written/read verbatim (no newline
  translation).

---

## Concurrency builtins

Goroutines run on OS threads; the runtime joins all spawned goroutines at program
exit. Channel values travel as 64-bit slots (ints, bit-cast floats, or pointer
addresses).

| Construct | Form | Description |
|---|---|---|
| Channel type | `channel<T>` | Type of a channel carrying `T`. |
| Channel create | `channel<T>()` | Create a new (unbounded) channel. |
| Send | `ch <- value;` | Send `value` into channel `ch`. |
| Receive | `<-ch` | Receive a value, blocking until one is available. |
| Spawn | `go f(args);` | Run `f(args)` on a new goroutine. |
| Select | `select { case v = <-ch: { ... } default: { ... } }` | Wait on multiple channel receives. |

Notes on semantics:

- The channel is unbounded; **send never blocks** (it enqueues and wakes a
  waiter). **Receive blocks** until a value is present.
- `go` requires a *direct call to a known function*: `go worker(ch, i);`.
  Arguments are evaluated in the spawning goroutine and passed by value.
- `select` tries each `case` with a non-blocking receive and runs the first ready
  one. With a `default` it runs that when no case is ready (non-blocking). Without
  a `default` it parks briefly and retries (blocking) until a case is ready.

Verified example (goroutines + channel; from `examples/concurrency.to`):

```to
def worker(ch: channel<int>, n: int) {
    ch <- n * n;            // send this worker's result
}
def main() {
    let ch = channel<int>();
    for i in 1..6 { go worker(ch, i); }   // spawn 5 goroutines
    let total = 0;
    for i in 0..5 { total = total + <-ch; } // collect 5 results
    println("sum of squares 1..5 = {}", total); // 55
    return total;
}
```

Verified `select` example (from `examples/select.to`):

```to
def worker(ch: channel<int>, n: int) { ch <- n; }
def main() {
    let fast = channel<int>();
    let slow = channel<int>();
    go worker(fast, 10);

    let got = 0;
    select {                           // blocking: waits for a ready case
        case v = <-fast: { got = v; }
        case v = <-slow: { got = v; }
    }
    println("received {}", got);       // received 10

    select {                           // non-blocking: nothing ready -> default
        case v = <-slow: { println("unexpected {}", v); }
        default: { println("nothing ready"); }  // nothing ready
    }
    return got;
}
```

**Edge cases / gotchas**

- A blocking `<-ch` with no sender will hang forever; a blocking `select` (no
  `default`) with no ready case will spin-park forever.
- Receiving from an empty channel inside `select` does not block — that's what
  makes `default` (non-blocking) selection possible.
- Values are 64-bit slots: a `channel<int>` is the natural, fully-supported case.

---

## Option / Result builtins

Constructors for optional/fallible values. Each wraps one payload into a tagged
heap object; deconstruct with `match` and constructor patterns.

| Constructor | Signature | Tag | Meaning |
|---|---|---|---|
| `Some` | `Some(x)` | present (1) | Option holding a value. |
| `None` | `None` | empty (0) | Empty option (a nil/null literal — note: no parentheses). |
| `Ok` | `Ok(v)` | success (1) | Result success holding a value. |
| `Err` | `Err(e)` | error (0) | Result failure holding an error/value. |

`Some(x)` and `Ok(v)` share the "present/success" tag; `Err(e)` is the
"error" tag; `None` is the empty/nil case. Match patterns bind the inner payload.

Verified example (from `examples/option_result.to`):

```to
def safeDiv(a: int, b: int) -> Result {
    if b == 0 { return Err(1); }   // error code 1
    return Ok(a / b);
}
def main() {
    match Some(42) {
        case Some(x): { println("got {}", x); }   // got 42
        case None:    { println("nothing"); }
    }
    match safeDiv(100, 4) {
        case Ok(v):  { println("ok: {}", v); }     // ok: 25
        case Err(e): { println("error code {}", e); }
    }
    match safeDiv(100, 0) {
        case Ok(v):  { println("ok: {}", v); }
        case Err(e): { println("error code {}", e); } // error code 1
    }
    return 0;
}
```

**Edge cases / gotchas**

- `None` is written without parentheses (it is the empty/nil case, not a call).
- Payloads are 64-bit slots, so `Some`/`Ok`/`Err` of an integer is the
  fully-supported case (same slot caveats as collections apply to pointers).
- The idiomatic way to consume them is `match` with `case Some(x)` / `case None`
  / `case Ok(v)` / `case Err(e)` patterns, which check the tag and bind the
  payload.

---

## Stdlib module: `std.math`

```to
import std.math;
```

Integer/number utilities written in Tocin. (Float math like `sqrt`/`pow` are
builtins — see the Math section.) Booleans are returned as `int` (`0`/`1`).

| Function | Signature | Description |
|---|---|---|
| `gcd` | `gcd(a: int, b: int) -> int` | Greatest common divisor (Euclid). |
| `lcm` | `lcm(a: int, b: int) -> int` | Least common multiple, `(a / gcd(a,b)) * b`. |
| `factorial` | `factorial(n: int) -> int` | `n!` (product of `2..n`; returns `1` for `n <= 1`). |
| `clamp` | `clamp(x: int, lo: int, hi: int) -> int` | Constrain `x` to `[lo, hi]`. |
| `isPrime` | `isPrime(n: int) -> int` | `1` if `n` is prime, else `0` (`n < 2` → `0`). |

Verified example:

```to
import std.math;
def main() {
    println("gcd(48,36)={}",   gcd(48, 36));      // 12
    println("lcm(4,6)={}",     lcm(4, 6));        // 12
    println("factorial(5)={}", factorial(5));     // 120
    println("clamp(15,0,10)={}", clamp(15, 0, 10)); // 10
    println("clamp(-3,0,10)={}", clamp(-3, 0, 10)); // 0
    println("clamp(5,0,10)={}",  clamp(5, 0, 10));  // 5
    println("isPrime(7)={}", isPrime(7));         // 1
    println("isPrime(8)={}", isPrime(8));         // 0
    println("isPrime(1)={}", isPrime(1));         // 0
    return 0;
}
```

**Edge cases:** `factorial(0)`/`factorial(1)` → `1`; `isPrime` returns `0` for
`n < 2`. `lcm`/`gcd` assume non-pathological inputs (no overflow checks).

---

## Stdlib module: `std.list`

```to
import std.list;
```

Algorithms over `list<int>` (fixed array literals). These use `len(...)`
internally, so they operate on arrays.

| Function | Signature | Description |
|---|---|---|
| `listSum` | `listSum(xs: list<int>) -> int` | Sum of all elements. |
| `listMax` | `listMax(xs: list<int>) -> int` | Maximum element. |
| `listMin` | `listMin(xs: list<int>) -> int` | Minimum element. |
| `listContains` | `listContains(xs: list<int>, target: int) -> int` | `1` if `target` present, else `0`. |
| `listReverse` | `listReverse(xs: list<int>)` | Reverse in place (mutates `xs`). |

Verified example:

```to
import std.list;
def main() {
    let xs = [5, 2, 9, 1, 7];
    println("listSum={}",  listSum(xs));   // 24
    println("listMax={}",  listMax(xs));   // 9
    println("listMin={}",  listMin(xs));   // 1
    println("contains(9)={}", listContains(xs, 9)); // 1
    println("contains(3)={}", listContains(xs, 3)); // 0
    listReverse(xs);                       // in place
    println("{} {} {} {} {}", xs[0], xs[1], xs[2], xs[3], xs[4]); // 7 1 9 2 5
    return 0;
}
```

**Edge cases:** `listMax`/`listMin` read `xs[0]` first, so they assume a
**non-empty** list. `listReverse` mutates the caller's array in place (it returns
nothing).

---

## Stdlib module: `std.linq`

```to
import std.linq;
```

LINQ-style operations over `list<int>`. Tocin has no first-class lambdas yet, so
transforms come in two flavors: **reductions/aggregations** that return a scalar,
and **`*Into` transforms** that write results into a caller-allocated
destination list (the destination must be at least as long as the source).
Booleans are `int` (`0`/`1`).

| Function | Signature | Description |
|---|---|---|
| `reduceSum` | `reduceSum(xs: list<int>) -> int` | Sum of elements (seed 0). |
| `reduceProduct` | `reduceProduct(xs: list<int>) -> int` | Product of elements (seed 1). |
| `aggregate` | `aggregate(xs: list<int>, seed: int, op: int) -> int` | Fold with seed; `op` = 0 add, 1 multiply, 2 max, 3 min. |
| `count` | `count(xs: list<int>) -> int` | Number of elements (= `len(xs)`). |
| `countGreater` | `countGreater(xs: list<int>, threshold: int) -> int` | How many elements are `> threshold`. |
| `indexOf` | `indexOf(xs: list<int>, target: int) -> int` | Index of first `target`, else `-1`. |
| `allGreater` | `allGreater(xs: list<int>, threshold: int) -> int` | `1` if every element `> threshold`, else `0`. |
| `anyGreater` | `anyGreater(xs: list<int>, threshold: int) -> int` | `1` if any element `> threshold`, else `0`. |
| `mapScaleInto` | `mapScaleInto(dst: list<int>, src: list<int>, k: int) -> int` | `dst[i] = src[i] * k`; returns count written. |
| `mapAddInto` | `mapAddInto(dst: list<int>, src: list<int>, k: int) -> int` | `dst[i] = src[i] + k`; returns count written. |
| `filterGreaterInto` | `filterGreaterInto(dst: list<int>, src: list<int>, threshold: int) -> int` | Copy elements `> threshold` into `dst`; returns count written. |

Verified example:

```to
import std.linq;
def main() {
    let xs = [3, 1, 4, 1, 5, 9, 2, 6];
    println("reduceSum={}",     reduceSum(xs));            // 31
    println("reduceProduct={}", reduceProduct([1,2,3,4])); // 24
    println("aggregate add 100={}", aggregate(xs, 100, 0)); // 131
    println("aggregate max 0={}",   aggregate(xs, 0, 2));   // 9
    println("count={}",         count(xs));               // 8
    println("countGreater(3)={}", countGreater(xs, 3));   // 4
    println("indexOf(5)={}",    indexOf(xs, 5));          // 4
    println("indexOf(99)={}",   indexOf(xs, 99));         // -1
    println("allGreater(0)={}", allGreater(xs, 0));       // 1
    println("allGreater(3)={}", allGreater(xs, 3));       // 0
    println("anyGreater(8)={}", anyGreater(xs, 8));       // 1
    println("anyGreater(100)={}", anyGreater(xs, 100));   // 0

    let dst = [0, 0, 0, 0, 0, 0, 0, 0];
    let n = mapScaleInto(dst, xs, 10);
    println("scale n={} dst0={} dst1={}", n, dst[0], dst[1]); // n=8 dst0=30 dst1=10

    let dst2 = [0, 0, 0, 0, 0, 0, 0, 0];
    mapAddInto(dst2, xs, 1);
    println("add dst0={} dst1={}", dst2[0], dst2[1]);     // 4 2

    let dst3 = [0, 0, 0, 0, 0, 0, 0, 0];
    let m = filterGreaterInto(dst3, xs, 3);
    println("filter count={} first={} second={}", m, dst3[0], dst3[1]); // 4 4 5
    return 0;
}
```

**Edge cases / gotchas**

- `*Into` functions write into `dst` and require `len(dst) >= len(src)` (for
  filters, `len(dst)` must be at least the number of matches). They do not
  allocate; you provide the destination array.
- `aggregate` with `op` 2 (max) / 3 (min) compares against `seed`, so choose a
  seed that won't dominate real data (e.g. seed `0` for max of positives).
- `indexOf` returns `-1` when not found; `countGreater`/`allGreater`/`anyGreater`
  test strictly greater-than (`>`), not `>=`.

---

## Quick keyword caveat

Several builtin-looking names are actually reserved keywords and cannot be used
as identifiers or called. The one that bites math code is **`log`** — use
`log2`/`log10` instead. Other reserved words include `channel`, `select`, `go`,
`where`, `error`, `debug`, `trace`, etc.; avoid them as variable/function names.

# Getting Started with Tocin

Welcome to **Tocin** — a statically-typed, compiled programming language with an
LLVM backend. Tocin reads like a friendly scripting language but compiles to
fast native code: you get type inference, classes, generics, traits, pattern
matching, channels and goroutines, a small standard library, and direct C
interop.

This tutorial takes you from zero to writing real Tocin. Every code example
below has been compiled and run with the actual `tocin` compiler, and the output
shown is the real output. Work through it top to bottom and you'll be productive
by the end.

---

## 1. Building and running

The compiler lives at `build/tocin`. (If you haven't built it yet, see
`docs/BUILDING.md`.) There are two ways to run a program:

```bash
# JIT-compile and run immediately (great while developing):
./build/tocin program.to --run

# Compile to a standalone native executable, then run it:
./build/tocin program.to -o program
./program
```

A few useful facts about the CLI:

- `--run` (also spelled `--jit`) executes `main()` in-process via the JIT.
- `-o <file>` selects the output format by extension: `.ll` for LLVM IR, `.s`
  for assembly, `.o` for an object file, and anything else (like `program`)
  produces a native executable.
- The value your `main()` returns becomes the process exit code. So
  `return 0;` means success, and `return 7;` makes the program exit with
  status 7.
- `-O0`..`-O3` set the optimization level (default `-O2`). Run `./build/tocin
  --help` to see all options.

Throughout this tutorial we'll use `--run`.

---

## 2. Hello, World

Create a file `hello.to`:

```tocin
def main() {
    println("Hello, World!");
    return 0;
}
```

Run it:

```bash
./build/tocin hello.to --run
```

Output:

```
Hello, World!
```

Every program starts at `def main()`. `println` writes a line to standard
output; `print` is the same without the trailing newline. We'll meet `{}`
placeholders in a moment.

---

## 3. Variables and types

Declare a variable with `let`. The type is **inferred** from the initializer, or
you can annotate it explicitly with `let name: Type = ...`.

The built-in scalar types are `int` (64-bit signed), `float`, `bool`, `string`,
and `void`. You can also build arrays with literals like `[1, 2, 3]`.

```tocin
def main() {
    let count = 42;              // inferred int
    let pi: float = 3.14159;     // annotated float
    let name = "Tocin";          // string
    let ready = true;            // bool
    let nums = [10, 20, 30];     // array literal

    println("count = {}", count);
    println("pi    = {}", pi);
    println("name  = {}", name);
    println("ready = {}", ready);
    println("nums[1] = {}, len = {}", nums[1], len(nums));
    return 0;
}
```

Output:

```
count = 42
pi    = 3.14159
name  = Tocin
ready = 1
nums[1] = 20, len = 3
```

In `println`, each `{}` is replaced, in order, by the following arguments.

> **Note:** `bool` values print as `1` (true) and `0` (false), not as the words
> `true`/`false`. Tocin represents booleans as integers, and several library
> functions return `1`/`0` to mean yes/no.

Use `len(...)` to get the length of an **array** (we'll see `strLen` for strings
and `vecLen` for vectors later — they are different functions).

---

## 4. Functions

Define a function with `def name(params) -> ReturnType { ... }`. The return type
is optional — if you omit it, Tocin infers it. Functions may be recursive, and a
function may call another defined later in the file.

```tocin
def add(a: int, b: int) -> int {
    return a + b;
}

// Return type omitted -> inferred.
def square(x: int) {
    return x * x;
}

def factorial(n: int) -> int {
    if n <= 1 { return 1; }
    return n * factorial(n - 1);
}

def main() {
    println("add(3, 4)     = {}", add(3, 4));
    println("square(9)     = {}", square(9));
    println("factorial(5)  = {}", factorial(5));
    return 0;
}
```

Output:

```
add(3, 4)     = 7
square(9)     = 81
factorial(5)  = 120
```

---

## 5. Control flow

Tocin has `if` / `elif` / `else`, `while`, `for ... in` over a range, and
`match`. Conditions don't need parentheses, but blocks always use braces.

```tocin
def classify(n: int) -> int {
    if n < 0 {
        println("{} is negative", n);
    } elif n == 0 {
        println("zero");
    } else {
        println("{} is positive", n);
    }
    return 0;
}

def main() {
    classify(-3);
    classify(0);
    classify(7);

    // while loop
    let i = 0;
    while i < 3 {
        println("while i = {}", i);
        i = i + 1;
    }

    // for over a half-open range a..b  (b is excluded)
    for j in 1..4 {
        println("for j = {}", j);
    }

    // match on a value
    let code = 2;
    match code {
        case 1: { println("one"); }
        case 2: { println("two"); }
        default: { println("many"); }
    }
    return 0;
}
```

Output:

```
-3 is negative
zero
7 is positive
while i = 0
while i = 1
while i = 2
for j = 1
for j = 2
for j = 3
two
```

Two things to remember: `for i in a..b` iterates `a, a+1, ..., b-1` (the upper
bound is **excluded**), and a `match` uses `case VALUE: { ... }` arms with an
optional `default: { ... }`.

The classic FizzBuzz puts these together:

```tocin
def main() {
    for i in 1..21 {
        if i % 15 == 0 {
            println("FizzBuzz");
        } elif i % 3 == 0 {
            println("Fizz");
        } elif i % 5 == 0 {
            println("Buzz");
        } else {
            println("{}", i);
        }
    }
    return 0;
}
```

The first few lines of output:

```
1
2
Fizz
4
Buzz
Fizz
```

---

## 6. Collections: vectors and maps

Array literals (`[1, 2, 3]`) are fixed sequences you index with `[]`. When you
want a **growable** list or a **key/value map**, use the built-in collection
functions.

### Vectors (growable lists)

```tocin
def sumVec(v: vector) -> int {
    let total = 0;
    for i in 0..vecLen(v) {
        total = total + vecGet(v, i);
    }
    return total;
}

def main() {
    let v = vecNew();
    vecPush(v, 5);
    vecPush(v, 10);
    vecPush(v, 15);
    println("len   = {}", vecLen(v));
    println("v[1]  = {}", vecGet(v, 1));

    vecSet(v, 1, 100);          // overwrite element 1
    println("after set v[1] = {}", vecGet(v, 1));

    let last = vecPop(v);       // remove & return the last element
    println("popped = {}", last);
    println("sum    = {}", sumVec(v));
    return 0;
}
```

Output:

```
len   = 3
v[1]  = 10
after set v[1] = 100
popped = 15
sum    = 105
```

The full vector API: `vecNew()`, `vecPush(v, x)`, `vecGet(v, i)`,
`vecSet(v, i, x)`, `vecLen(v)`, `vecPop(v)`.

> **Important gotcha:** a vector is an *opaque handle*. When you pass one to a
> function, annotate the parameter with the special type name `vector` (as in
> `sumVec(v: vector)` above). Maps use the type name `map` the same way. Don't
> try to write `len(v)` on a vector — use `vecLen(v)`.

### Maps (key/value)

Maps come in two flavors depending on the key type: integer keys
(`mapPut`/`mapGet`/`mapHas`) and string keys
(`mapPutStr`/`mapGetStr`/`mapHasStr`). The `*Has` functions return `1` if the
key is present, `0` otherwise.

```tocin
def main() {
    // Integer-keyed map
    let ages = mapNew();
    mapPut(ages, 1, 30);
    mapPut(ages, 2, 42);
    println("ages[1] = {}", mapGet(ages, 1));
    println("has key 2? {}", mapHas(ages, 2));
    println("has key 9? {}", mapHas(ages, 9));

    // String-keyed map
    let scores = mapNew();
    mapPutStr(scores, "alice", 95);
    mapPutStr(scores, "bob", 88);
    println("alice = {}", mapGetStr(scores, "alice"));
    println("has bob? {}", mapHasStr(scores, "bob"));
    return 0;
}
```

Output:

```
ages[1] = 30
has key 2? 1
has key 9? 0
alice = 95
has bob? 1
```

---

## 7. Strings

Concatenate strings with `+`. For everything else there's a small set of string
builtins. The most important thing to remember: **string length is `strLen`, not
`len`.**

```tocin
def main() {
    let greeting = "Hello, " + "Tocin" + "!";   // concatenation with +
    println(greeting);
    println("length = {}", strLen(greeting));    // strLen, NOT len, for strings

    println("char at 7 = {}", charAt(greeting, 7));        // ASCII code of 'T'
    println("substring = {}", substring(greeting, 7, 5));  // start=7, length=5

    // Numbers <-> strings
    let s = intToStr(2026);
    println("intToStr -> {}", s);
    println("strToInt -> {}", strToInt("100") + 1);

    // Build a string in a loop
    let acc = "";
    let i = 0;
    while i < 4 {
        acc = acc + intToStr(i) + "-";
        i = i + 1;
    }
    println("built = {}", acc);
    return 0;
}
```

Output:

```
Hello, Tocin!
length = 13
char at 7 = 84
substring = Tocin
intToStr -> 2026
strToInt -> 101
built = 0-1-2-3-
```

A couple of notes:

- `charAt(s, i)` returns the **character code** (an `int`), e.g. `84` for `'T'`.
  Use `charToStr(code)` to turn a code back into a one-character string.
- `substring(s, start, length)` takes a start index and a *length*, not an end
  index.
- Other handy string builtins include `strEq(a, b)` and `indexOfChar(s, code)`.

> **Gotcha:** calling `len("hello")` does **not** return `5` — `len` is for
> arrays, and on a string it produces a meaningless number. Always use
> `strLen("hello")`.

---

## 8. Classes

A `class` groups fields and methods. Methods take `self` as their first
parameter. Construct an instance by calling the class name like a function; read
and write fields with `.`.

```tocin
class Point {
    x: int;
    y: int;

    def sum(self) -> int {
        return self.x + self.y;
    }

    def shift(self, dx: int, dy: int) {
        self.x = self.x + dx;
        self.y = self.y + dy;
    }
}

def main() {
    let p = Point(3, 4);                 // construct
    println("sum = {}", p.sum());        // 7

    p.x = 10;                            // write a field directly
    println("p.x = {}", p.x);            // 10

    p.shift(1, 1);                       // call a mutating method
    println("after shift, sum = {}", p.sum());   // 10+1 + 4+1 = 16
    return 0;
}
```

Output:

```
sum = 7
p.x = 10
after shift, sum = 16
```

Constructor arguments fill the fields in declaration order, so `Point(3, 4)`
sets `x = 3` and `y = 4`.

### Structs, traits, and `impl`

Tocin also has `struct` (data with fields) plus `trait`s (interfaces) and `impl`
blocks that attach methods to a type. This is a clean way to share behavior
across types:

```tocin
trait Shape {
    def area(self) -> int;
}

struct Square { side: int; }
impl Shape for Square {
    def area(self) -> int { return self.side * self.side; }
}

struct Rect { w: int; h: int; }
impl Shape for Rect {
    def area(self) -> int { return self.w * self.h; }
}

def main() {
    let s = Square(5);
    let r = Rect(3, 4);
    println("square area = {}", s.area());   // 25
    println("rect area   = {}", r.area());   // 12
    return 0;
}
```

Output:

```
square area = 25
rect area   = 12
```

You can also write an `impl Type { ... }` block (without `for Trait`) to add
"inherent" methods directly to a struct.

---

## 9. Generics

Functions and classes can be parameterized over a type with `<T>`. The type
argument is **inferred** from how you call/construct, so you rarely write it
explicitly.

```tocin
// Generic function: T is inferred from the argument.
def maxOf<T>(a: T, b: T) -> T {
    if a > b { return a; }
    return b;
}

// Generic class: a fresh version is generated per concrete type argument.
class Box<T> {
    value: T;
    def get(self) -> T { return self.value; }
    def set(self, v: T) { self.value = v; }
}

def main() {
    println("maxOf(3, 9)   = {}", maxOf(3, 9));

    let bi = Box(42);                 // Box<int>, T inferred from 42
    bi.set(100);
    println("box(int)      = {}", bi.get());

    let bf = Box(2.5);                // Box<float>, a separate instantiation
    println("box(float)    = {}", bf.get());
    return 0;
}
```

Output:

```
maxOf(3, 9)   = 9
box(int)      = 100
box(float)    = 2.5
```

A generic class with several type parameters works the same way:
`class Pair<A, B> { first: A; second: B; ... }`, constructed as `Pair(10, 20)`.

---

## 10. Errors: throw / try / catch / finally

Signal an error with `throw`, which carries an integer code and unwinds to the
nearest enclosing `catch`. A `finally` block runs on both the normal and the
caught paths.

```tocin
def divide(a: int, b: int) -> int {
    if b == 0 {
        throw 1;            // throw an integer error code
    }
    return a / b;
}

def main() {
    try {
        let r = divide(10, 0);
        println("result = {}", r);            // not reached
    } catch (e) {
        println("caught error code {}", e);   // caught error code 1
    }

    // finally always runs
    try {
        println("10 / 2 = {}", divide(10, 2));
    } finally {
        println("cleanup done");
    }
    return 0;
}
```

Output:

```
caught error code 1
10 / 2 = 5
cleanup done
```

The thrown value propagates out of called functions until something catches it,
so you can throw deep in a call stack and handle it higher up.

---

## 11. Option, Result, and pattern matching

For errors you'd rather handle as values, Tocin has the familiar `Option` and
`Result` shapes:

- `Some(x)` / `None` — a value that may be absent.
- `Ok(v)` / `Err(e)` — a success or a failure.

You take them apart with `match`, using **constructor patterns** that bind the
inner payload:

```tocin
def safeDiv(a: int, b: int) -> Result {
    if b == 0 { return Err(1); }
    return Ok(a / b);
}

def main() {
    // Option: Some(x) or None
    match Some(42) {
        case Some(x): { println("got {}", x); }
        case None:    { println("nothing"); }
    }

    // Result: Ok(v) or Err(e)
    match safeDiv(100, 4) {
        case Ok(v):  { println("ok: {}", v); }
        case Err(e): { println("err: {}", e); }
    }
    match safeDiv(100, 0) {
        case Ok(v):  { println("ok: {}", v); }
        case Err(e): { println("err: {}", e); }
    }
    return 0;
}
```

Output:

```
got 42
ok: 25
err: 1
```

`case Some(x):` only matches the `Some` case and binds its payload to `x`; the
same goes for `Ok(v)` and `Err(e)`.

---

## 12. Null safety

Class instances (references) can be **null**, written `None`. Three operators
make working with possibly-null references concise and safe:

- `a?.b` — *safe navigation*: read `b` only if `a` is non-null; otherwise the
  result is `0`/null instead of crashing.
- `a ?: b` — *elvis / null-coalescing*: `a` if it's non-null, else `b`.
- `a!!` — *force-unwrap*: assert `a` is non-null (the program aborts if it
  isn't).

```tocin
class Box {
    v: int;
    def get(self) -> int { return self.v; }
}

def main() {
    let present: Box = Box(42);
    let missing: Box = None;          // None is the null value

    // ?:  supplies a fallback when the left side is null
    let a = present ?: Box(0);
    let b = missing ?: Box(7);
    println("elvis: {} {}", a.get(), b.get());      // 42 7

    // ?.  reads a field only if the base is non-null, else yields 0/null
    println("safe:  {} {}", present?.v, missing?.v); // 42 0

    // !!  asserts non-null
    println("force: {}", present!!.get());           // 42
    return 0;
}
```

Output:

```
elvis: 42 7
safe:  42 0
force: 42
```

---

## 13. First-class functions and lambdas

Functions are values: you can pass them to other functions, store them, and
return them. A function type is written `(int) -> int` (argument types, then
`->`, then the return type). An anonymous function is a `lambda`:

```tocin
lambda (x: int) -> int  x * 2
```

The body after the return type is a single expression.

```tocin
def inc(x: int) -> int { return x + 1; }
def trip(x: int) -> int { return x * 3; }

// A function-typed parameter has type (int) -> int.
def apply(f: (int) -> int, x: int) -> int {
    return f(x);
}

// Return a function value.
def chooser(positive: int) -> (int) -> int {
    if positive > 0 { return trip; }
    return inc;
}

def main() {
    println("apply(inc, 5)  = {}", apply(inc, 5));   // 6

    // A local initialized by a CALL needs an explicit function-type annotation.
    let f: (int) -> int = chooser(1);
    println("chooser(1)(10) = {}", f(10));           // 30

    // A lambda passed inline:
    println("lambda square  = {}", apply(lambda (x: int) -> int x * x, 9));  // 81
    return 0;
}
```

Output:

```
apply(inc, 5)  = 6
chooser(1)(10) = 30
lambda square  = 81
```

You can combine functions and collections — here a higher-order function folds a
lambda over a vector:

```tocin
def mapSum(f: (int) -> int, v: vector) -> int {
    let total = 0;
    for i in 0..vecLen(v) { total = total + f(vecGet(v, i)); }
    return total;
}

def main() {
    let v = vecNew();
    vecPush(v, 1); vecPush(v, 2); vecPush(v, 3);
    println("sum of squares = {}", mapSum(lambda (x: int) -> int x * x, v)); // 14
    return 0;
}
```

Output:

```
sum of squares = 14
```

Two limitations worth knowing up front:

> **Gotcha 1 — annotate function locals initialized by a call.** When a local
> variable gets its value from a *function call that returns a function*, you
> must annotate it: `let f: (int) -> int = chooser(1);`. Without the annotation
> the compiler reports `error [T006]: Called value is not a function`. (When the
> initializer is a plain function name or a lambda, inference is fine, but the
> explicit annotation always works and is the safe habit.)
>
> **Gotcha 2 — lambdas don't capture.** A lambda can use only its own
> parameters; it cannot reference local variables from the surrounding function.
> Pass the data you need in as arguments instead.

---

## 14. Concurrency: goroutines and channels

Start a concurrent task with `go f(args);`. Communicate between tasks with a
typed channel: create one with `channel<T>()`, send with `ch <- value;`, and
receive with `<-ch`.

```tocin
def worker(ch: channel<int>, n: int) {
    ch <- n * n;          // send the square into the channel
}

def main() {
    let ch = channel<int>();

    // Spawn 5 goroutines with `go`.
    for i in 1..6 {
        go worker(ch, i);
    }

    // Receive 5 results with <-ch.
    let total = 0;
    for i in 0..5 {
        total = total + <-ch;
    }
    println("sum of squares 1..5 = {}", total);   // 1+4+9+16+25 = 55
    return 0;
}
```

Output:

```
sum of squares 1..5 = 55
```

### Selecting over multiple channels

`select` waits on several channel operations at once and runs the first case
that becomes ready. With a `default:` arm it becomes non-blocking.

```tocin
def producer(ch: channel<int>) { ch <- 42; }

def main() {
    let ch = channel<int>();
    go producer(ch);

    // Blocking select: wait until a channel is ready.
    let got = 0;
    select {
        case v = <-ch: { got = v; }
    }
    println("received {}", got);          // received 42

    // Non-blocking select: run default if nothing is ready.
    let other = channel<int>();
    select {
        case v = <-other: { println("got {}", v); }
        default:          { println("nothing ready"); }
    }
    return 0;
}
```

Output:

```
received 42
nothing ready
```

---

## 15. File I/O

The standard builtins let you read and write whole files, append to them, and
read a line. Paths are resolved relative to the current working directory.

```tocin
def main() {
    let n = writeFile("tocin_demo.txt", "first line\n");
    println("wrote {} bytes", n);
    appendFile("tocin_demo.txt", "second line\n");

    let contents = readFile("tocin_demo.txt");
    print(contents);                 // prints both lines
    return 0;
}
```

Output:

```
wrote 11 bytes
first line
second line
```

`writeFile(path, text)` returns the number of bytes written (here `11` for
`"first line\n"`). Related builtins: `readFile(path)`, `appendFile(path, text)`,
and `readLine()` (reads one line from standard input).

---

## 16. Modules and the standard library

Pull in another file with `import a.b;`, which loads `a/b.to`. Tocin looks for
the module next to the importing file first, then in the standard library. The
standard library ships these modules:

- `std.math` — integer/number utilities (`gcd`, `lcm`, `factorial`, `clamp`,
  `isPrime`). Note that `sqrt`, `pow`, `abs`, `min`, `max`, `floor`, and the
  trig functions are *builtins* and need no import.
- `std.list` — list algorithms (`listSum`, `listMax`, `listMin`,
  `listContains`, `listReverse`).
- `std.linq` — LINQ-style operations over `list<int>`: reductions like
  `reduceSum`, `reduceProduct`, `aggregate`, `count`, `countGreater`,
  `indexOf`, and `*Into` transforms that write results into a destination list.

```tocin
import std.math;
import std.linq;

def main() {
    println("gcd(48, 36)  = {}", gcd(48, 36));        // 12
    println("isPrime(13)  = {}", isPrime(13));         // 1
    println("factorial(5) = {}", factorial(5));        // 120

    let xs = [3, 1, 4, 1, 5, 9];
    println("sum          = {}", reduceSum(xs));        // 23
    println("> 3 count    = {}", countGreater(xs, 3));  // 3 (4, 5, 9)
    return 0;
}
```

Output:

```
gcd(48, 36)  = 12
isPrime(13)  = 1
factorial(5) = 120
sum          = 23
> 3 count    = 3
```

After importing a module, its functions are available directly by name. (Because
`std.linq` predates lambdas, its transforms come in two forms: reductions that
return a scalar, and `*Into` functions that write into a list you pass in.)

---

## 17. Macros

A `macro` is expanded at the token level *before* parsing. Define one with
`macro name(params) { body }` and invoke it with a `!`: `name!(args)`. Each
argument is auto-parenthesized when substituted, and so is the whole expansion,
so macros respect operator precedence and compose cleanly.

```tocin
macro square(x) { x * x }
macro hypot2(a, b) { square!(a) + square!(b) }

def main() {
    println("square(5)     = {}", square!(5));       // 25
    println("square(2 + 3) = {}", square!(2 + 3));   // 25 (args are parenthesized)
    println("hypot2(3, 4)  = {}", hypot2!(3, 4));    // 25 (macros can call macros)
    return 0;
}
```

Output:

```
square(5)     = 25
square(2 + 3) = 25
hypot2(3, 4)  = 25
```

A macro can also wrap a whole statement. For example,
`macro greet(name) { println("hi {}", name) }` lets you write `greet!("Tocin");`.

---

## 18. C FFI

Call C library functions directly by declaring them with `extern def` and no
body. Under `--run` the JIT resolves them from the running process (libc/libm),
and native builds link them normally.

```tocin
extern def labs(x: int) -> int;                 // libc long abs
extern def atoi(s: string) -> int;              // parse int from a string
extern def hypot(x: float, y: float) -> float;  // libm

def main() {
    println("labs(-42)    = {}", labs(-42));        // 42
    println("atoi(\"123\")  = {}", atoi("123"));      // 123
    println("hypot(3, 4)  = {}", hypot(3.0, 4.0));   // 5
    return 0;
}
```

Output:

```
labs(-42)    = 42
atoi("123")  = 123
hypot(3, 4)  = 5
```

Tocin's types map to the obvious C types: `int` is a 64-bit integer, `float` is
a C `double`, and `string` is a C string (`char *`).

---

## A note on honesty and limitations

Everything in this tutorial was compiled and run with the real `tocin` compiler,
and the output blocks are the actual output. As you explore, keep these
practical limitations in mind:

- `bool` prints as `1`/`0`, and many library predicates return `1`/`0` rather
  than `true`/`false`.
- Strings use `strLen`/`charAt`/`substring` — `len` is only for arrays, and
  using it on a string yields a garbage number.
- Collection handles are opaque: annotate parameters with the type names
  `vector` and `map`, and use the `vec*`/`map*` functions to work with them.
- Lambdas are **non-capturing** — pass everything they need as arguments.
- A function-typed local initialized by a *call* needs an explicit annotation,
  e.g. `let f: (int) -> int = makeFn();`.

When in doubt, do what this tutorial did: write the smallest program that
exercises the feature, run it with `--run`, and check the output.

---

## Where to go next

You can now write real Tocin: variables and functions, control flow,
collections and strings, classes with traits and generics, error handling with
`throw`/`try` and with `Option`/`Result`, null-safe references, first-class
functions, concurrency, files, modules, macros, and C interop.

To go deeper:

- **Language reference** — the full grammar and semantics:
  [`docs/language-reference.md`](language-reference.md).
- **Standard library reference** — every module and builtin:
  [`docs/stdlib-reference.md`](stdlib-reference.md).

You may also find these existing guides useful: `docs/03_Language_Basics.md`,
`docs/04_Standard_Library.md`, `docs/STDLIB_GUIDE.md`,
`docs/LANGUAGE_FEATURES.md`, and the runnable programs in the `examples/`
directory (try `./build/tocin examples/fib.to --run`).

Happy hacking!

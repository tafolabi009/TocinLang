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

### Integer literals: hex, octal, binary, and digit separators

Integers can be written in several bases, and you may use underscores anywhere
inside a number as digit separators to make long values easier to read. They are
purely cosmetic — `1_000_000` is exactly `1000000`.

```tocin
def main() {
    let hex = 0xFF;          // hexadecimal -> 255
    let oct = 0o17;          // octal       -> 15
    let bin = 0b1010;        // binary      -> 10
    let big = 1_000_000;     // underscores are ignored
    println("{} {} {} {}", hex, oct, bin, big);
    return 0;
}
```

Output:

```
255 15 10 1000000
```

Underscores work in any base (e.g. `0b1010_1010`, `0xDE_AD_BE_EF`).

### Mixing `int` and `float` in arithmetic

When an expression mixes an `int` and a `float`, the `int` is automatically
promoted to `float`, and the whole expression is a `float`. The same promotion
happens when you assign an `int` to a `float` variable. You never need an
explicit conversion for this.

```tocin
def main() {
    let a = 3.0 + 2;      // 2 is promoted -> 5.0 (a float)
    let b = 10 / 4.0;     // float division -> 2.5
    let c = 2 * 3.5;      // -> 7.0
    let d: float = 5;     // int 5 stored as 5.0
    println("{} {} {} {}", a, b, c, d);
    return 0;
}
```

Output:

```
5 2.5 7 5
```

> **Note:** a `float` whose value is a whole number prints without a decimal
> point (`5.0` shows as `5`), while a fractional value like `2.5` prints with
> one. Either way the value is a `float`. This is why `10 / 4.0` gives `2.5`
> but plain integer `10 / 4` would give `2` (integer division).

### Compound assignment: `+= -= *= /= %=`

Instead of writing `x = x + 1`, you can update a variable in place with a
compound assignment operator. These work on plain variables, on array elements,
and — for `+=` — on strings (where it concatenates).

```tocin
def main() {
    let a = 10;
    a += 5;            // a = a + 5  -> 15
    a -= 3;            // -> 12
    a *= 2;            // -> 24
    a /= 4;            // -> 6
    a %= 4;            // -> 2
    println("a = {}", a);

    let arr = [1, 2, 3];
    arr[1] += 10;      // update an array element in place -> 12
    println("arr[1] = {}", arr[1]);

    let s = "hello";
    s += " world";     // string concatenation
    println("s = {}", s);
    return 0;
}
```

Output:

```
a = 2
arr[1] = 12
s = hello world
```

### Bitwise operators

Tocin has the usual C-style bitwise operators on integers: `&` (and), `|` (or),
`^` (xor), `<<` (left shift), `>>` (right shift), and the unary `~` (bitwise
NOT). They follow C precedence (for example `&` binds tighter than `|`, and the
shifts bind tighter than `+`).

```tocin
def main() {
    println("and = {}", 12 & 10);   // 8
    println("or  = {}", 12 | 10);   // 14
    println("xor = {}", 12 ^ 10);   // 6
    println("shl = {}", 1 << 4);    // 16
    println("shr = {}", 256 >> 2);  // 64
    println("not = {}", ~0);        // -1
    return 0;
}
```

Output:

```
and = 8
or  = 14
xor = 6
shl = 16
shr = 64
not = -1
```

A common use is treating an integer as a set of bit flags:

```tocin
def main() {
    let READ  = 1 << 0;            // 1
    let WRITE = 1 << 1;            // 2
    let EXEC  = 1 << 2;            // 4
    let perms = READ | WRITE;      // combine flags
    println("perms     = {}", perms);              // 3
    println("can read? {}", (perms & READ) != 0);  // 1
    println("can exec? {}", (perms & EXEC) != 0);  // 0
    return 0;
}
```

Output:

```
perms     = 3
can read? 1
can exec? 0
```

(For the full precedence ordering of every operator, see the language
reference.)

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

### Nested functions

You can define a `def` *inside* another function. This is handy for a small
helper that is only meaningful within its parent.

```tocin
def main() {
    def dbl(x: int) -> int { return x * 2; }
    println("dbl(21) = {}", dbl(21));   // 42
    return 0;
}
```

Output:

```
dbl(21) = 42
```

A nested `def` is an ordinary function that just happens to be written inside
another — it **cannot capture** its parent's local variables. If you need to
reference surrounding locals, use a capturing `lambda` instead (see
[First-class functions and lambdas](#13-first-class-functions-and-lambdas)).

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

### Breaking out of and skipping loop iterations

Inside any loop you can use `break` to stop the loop early and `continue` to
skip the rest of the current iteration and move on to the next one. Both work in
`while`, `for ... in` over a range, and `for ... in` over an array, and they
always affect the innermost enclosing loop.

```tocin
def main() {
    // Search a list and stop as soon as we find the value.
    let nums = [4, 8, 15, 16, 23, 42];
    let target = 16;
    let foundAt = -1;
    for i in 0..len(nums) {
        if nums[i] == target {
            foundAt = i;
            break;                  // leave the loop immediately
        }
    }
    println("found {} at index {}", target, foundAt);

    // Sum only the odd numbers, skipping evens with `continue`.
    let total = 0;
    for i in 0..10 {
        if i % 2 == 0 { continue; } // jump to the next iteration
        total = total + i;
    }
    println("sum of odds 0..9 = {}", total);
    return 0;
}
```

Output:

```
found 16 at index 3
sum of odds 0..9 = 25
```

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

### Comparing strings with `==` and `!=`

You can compare two strings for equality directly with `==` and `!=`, and they
compare **by value** (by contents), not by memory address. This works for string
literals, parameters, the results of `+` concatenation, and the values returned
by string builtins like `intToStr`.

```tocin
def greet(name: string) -> int {
    if name == "Alice" { return 1; }
    return 0;
}

def main() {
    println("alice? {}", greet("Alice"));      // 1
    println("bob?   {}", greet("Bob"));        // 0

    let a = "hello";
    let b = "hel" + "lo";                       // a different object, same text
    println("concat equal? {}", a == b);        // 1 (compared by value)
    println("not world?    {}", a != "world");  // 1

    println("intToStr(42) == \"42\"? {}", intToStr(42) == "42");  // 1
    return 0;
}
```

Output:

```
alice? 1
bob?   0
concat equal? 1
not world?    1
intToStr(42) == "42"? 1
```

> **Note:** value comparison applies to strings. Other reference values (class
> instances, or `None`) still compare by identity with `==` — i.e. whether they
> are the *same* object — so you can use `obj == None` to test for null.

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

A `finally` block runs even when the `try` or `catch` body **returns early** —
not only when control falls off the end of the block. This makes `finally` a
reliable place for cleanup that must happen no matter how the block exits:

```tocin
def attempt() -> int {
    try {
        throw 5;
    } catch (e) {
        return 1;            // early return out of catch...
    } finally {
        println("cleanup");  // ...but this still runs first
    }
    return 0;
}

def main() {
    println("result = {}", attempt());
    return 0;
}
```

Output:

```
cleanup
result = 1
```

The `finally` block executes, printing `cleanup`, and only then does the early
`return 1` take effect.

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

    // Storing a returned function in a local. An explicit function-type
    // annotation is optional here, but it documents the intent:
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

### Capturing closures

A lambda can use the local variables of the function it is defined in, not just
its own parameters — this makes it a *closure*. The capture is **by value**: the
lambda takes a snapshot of each captured variable at the moment it is created.
Changing the original variable afterwards does not change what the lambda saw.

```tocin
def main() {
    let n = 5;
    let add = lambda (x: int) -> int x + n;   // captures n = 5
    println("add(10) = {}", add(10));         // 15

    n = 100;                                   // change the original n
    println("still   = {}", add(10));          // still 15 (snapshot of 5)
    return 0;
}
```

Output:

```
add(10) = 15
still   = 15
```

Because the snapshot lives with the lambda, a closure can be **returned** from a
function and keep working after that function has finished — each one carries its
own captured state:

```tocin
def makeAdder(n: int) -> (int) -> int {
    return lambda (x: int) -> int x + n;       // captures this call's n
}

def main() {
    let add10  = makeAdder(10);
    let add100 = makeAdder(100);
    println("add10(1)  = {}", add10(1));        // 11
    println("add100(1) = {}", add100(1));       // 101
    return 0;
}
```

Output:

```
add10(1)  = 11
add100(1) = 101
```

A few things worth knowing:

> **Capture is by value, not by reference.** A closure can read the snapshot it
> captured, but it cannot reach back and mutate the caller's variable, and
> later changes to that variable are not seen by the closure. If you need shared
> mutable state, pass it explicitly (for example a `vector` handle).
>
> **A function-typed local may be annotated, but doesn't have to be.** Storing a
> returned function in `let f = chooser(1);` works with or without the
> `: (int) -> int` annotation; the annotation is just documentation.

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
- A `float` whose value is whole prints without a decimal point (`5.0` as `5`).
- Strings use `strLen`/`charAt`/`substring` — `len` is only for arrays, and
  using it on a string yields a garbage number.
- Collection handles are opaque: annotate parameters with the type names
  `vector` and `map`, and use the `vec*`/`map*` functions to work with them.
- Lambdas **capture by value** — they snapshot the surrounding locals they use,
  so mutating the original afterwards (or trying to mutate it from inside the
  lambda) has no effect. For shared mutable state, pass a handle explicitly.
- Nested `def` functions **cannot capture** their parent's locals; use a
  `lambda` when you need capture.
- There is **no automatic memory management (no GC)** yet: heap allocations such
  as concatenated strings, array literals, closures, vectors, and maps are not
  freed automatically, so long-running programs leak. Free vectors and maps
  manually with `vecFree`/`mapFree` when you can.
- `switch` and `defer` are **not** implemented — use `match` / `case` for
  multi-way branching. There is also no `**` power operator and no `++`/`--`;
  use `pow`/`x * x` and `x += 1` / `x -= 1` instead.

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

# Tocin Language Reference

This is the authoritative reference for the Tocin programming language as
implemented by the compiler in this repository. Tocin is a **statically typed,
compiled** language that lowers to **LLVM 18** IR and can either be JIT-executed
or compiled to a native object/executable.

Every nontrivial construct documented here has been verified by compiling and
running a snippet with the in-tree compiler. Where a feature is lexed but not
actually supported by the parser/codegen, it is called out explicitly. See
[Limitations / not yet supported](#20-limitations--not-yet-supported).

**Compiler binary:** `build/tocin`
**Run a program:** `./build/tocin FILE.to --run`

```tocin
// hello.to
def main() {
    println("Hello from Tocin!");
    return 0;
}
```

```text
$ ./build/tocin hello.to --run
Hello from Tocin!
```

The process exit code is the integer returned by `main` (see
[§19](#19-compilation--execution)).

---

## Table of contents

1.  [Lexical structure](#1-lexical-structure)
2.  [Types](#2-types)
3.  [Operators & precedence](#3-operators--precedence)
4.  [Statements](#4-statements)
5.  [Functions](#5-functions)
6.  [Classes](#6-classes)
7.  [Traits & impl](#7-traits--impl)
8.  [Generics](#8-generics)
9.  [Enums](#9-enums)
10. [Pattern matching](#10-pattern-matching)
11. [Error handling](#11-error-handling)
12. [Null safety](#12-null-safety)
13. [Option / Result](#13-option--result)
14. [Concurrency](#14-concurrency)
15. [Macros](#15-macros)
16. [Modules](#16-modules)
17. [FFI](#17-ffi)
18. [Memory model & ABI](#18-memory-model--abi)
19. [Compilation & execution](#19-compilation--execution)
20. [Limitations / not yet supported](#20-limitations--not-yet-supported)
21. [Built-in function reference](#21-built-in-function-reference)

---

## 1. Lexical structure

### Comments

```tocin
// line comment
/* block comment */
# line comment (Python style)
## block comment (Python style) ##
```

The canonical, idiomatic comment is `//`. Block comments do not nest.

### Identifiers

Identifiers start with a letter or `_` and continue with letters, digits, or
`_` (`[A-Za-z_][A-Za-z0-9_]*`). Identifiers are case-sensitive.

### Literals

| Literal | Examples | Notes |
|---|---|---|
| Integer | `0`, `42`, `1000`, `0xFF`, `0b1010`, `0o17` | Hex (`0x`), binary (`0b`), octal (leading `0` + nonzero digit). Always typed `int` (i64). Suffixes `u`/`l`/`L` are accepted and ignored. |
| Float | `3.14`, `1.5`, `1e9`, `2.0e-3`, `1.5f` | Typed `float` (f64). A decimal point requires a digit on both sides (`1.0`, not `1.`). The `f`/`F` suffix marks a float. |
| String | `"hello"`, `'hi'`, `"a\nb\t\"c\""` | Double or single quotes. Escapes: `\n \t \r \\ \" \'` plus Unicode `\u{...}`. A `char*` (NUL-terminated) at runtime. |
| Boolean | `true`, `false` | Type `bool`. |
| Null | `None` | The single null/empty value. Also serves as the `None` constructor in pattern matching and the absence value for references and `Option`/`Result`. |

```tocin
def main() {
    let i = 0xFF;          // 255
    let f = 1.5e1;         // 15.0
    let s = "tab\there";
    let b = true;
    let n = None;
    println("{} {} {} {}", i, f, s, b);  // 255 15 tab	here 1
    return 0;
}
```

> Note: in formatted output, `bool` prints as `1`/`0` and `float` prints with
> `%g` (so `15.0` prints as `15`). See [println](#21-built-in-function-reference).

### Keywords

The lexer reserves a large keyword set. Keywords that are **active** in the
current language (used by the parser/codegen) are:

```
let const def async await class struct enum trait impl
if elif else while for in return match case default
import lambda new delete try catch finally throw
go select channel and or true false None void self
extern  break continue   (break/continue are reserved but NOT yet implemented)
```

The following are reserved by the lexer but **not** wired into the grammar/
codegen and are best avoided as identifiers: `from switch interface pub priv
static final abstract virtual override super null undefined typeof instanceof
as is where yield generator coroutine spawn join mutex lock unlock atomic
volatile move borrow constexpr inline export module package namespace using
with defer panic recover assert debug trace log warn error fatal`.

`macro` is **not** a reserved keyword — it is recognized as an ordinary
identifier by a token-level preprocessing pass (see [§15](#15-macros)).

---

## 2. Types

### Built-in scalar types

| Tocin | LLVM | Description |
|---|---|---|
| `int` (alias `i64`) | `i64` | 64-bit signed integer. The default integer type. |
| `i32`, `i16`, `i8` | `i32`/`i16`/`i8` | Sized integers (mainly for FFI). |
| `float` (alias `f64`) | `double` | 64-bit IEEE float. The default float type. |
| `f32` (alias `float32`) | `float` | 32-bit IEEE float. |
| `bool` | `i1` | `true` / `false`. |
| `string` (alias `str`) | `char*` (ptr) | Immutable NUL-terminated byte string. |
| `void` | `void` | No value; the implicit return type of a function with no `return`. `None` as a type annotation is also treated as void. |

### Aggregate / reference types

| Form | Meaning |
|---|---|
| `[a, b, c]` | **Array/list literal.** A heap block laid out as `[i64 length][elem 0][elem 1]…`. Indexed with `a[i]`; length via `len(a)`. |
| `list<T>`, `array<T>` (and `List`/`Array`) | Array type annotations. Represented as an opaque pointer to the array block. |
| `{ k: v, ... }` | **Dictionary literal.** Constructs a dict value (see limitations on dict ergonomics). |
| `channel<T>` | A concurrency channel handle (opaque pointer). Constructed with `channel<T>()`. See [§14](#14-concurrency). |
| `(A, B) -> R` | **Function type.** A value that can be called; lowered to a function pointer. |
| `ClassName` / `StructName` | A user-defined class/struct instance, passed by reference (opaque pointer). |
| `C<T>` | A generic type instantiated at `T` (e.g. `Box<int>`). |
| `Option`, `Result` | Tagged single-payload wrappers; see [§13](#13-option--result). |

```tocin
class Point { x: int; y: int; }

def apply(f: (int) -> int, x: int) -> int { return f(x); }

def main() {
    let xs: list<int> = [1, 2, 3];   // array type annotation
    let p = Point(1, 2);             // class instance
    let ch = channel<int>();         // channel
    println("{} {}", xs[0], len(xs));// 1 3
    return 0;
}
```

### `let` / type inference vs explicit annotation

A local is declared with `let` (or `const`):

```tocin
let name: Type = expr;   // explicit type
let name = expr;         // inferred from the initializer's type
let name: Type;          // no initializer (only valid WITH a type)
```

Rules (verified):

* `let x = 5;` infers `x: int`. `let f = 1.5;` infers `float`.
* `let x;` with **no type and no initializer** is an error
  (`Cannot infer type for variable 'x' without initializer`).
* `let x: int = 5;` annotates explicitly. An initializer whose type does not
  match the annotation is an error unless it is a numeric widen/narrow that the
  compiler can cast (int↔int, float↔float).
* `const` is parsed and behaves like `let` for codegen (no enforced
  immutability today).

There is a special rule for **function-typed locals initialized from a call** —
see [§5](#5-functions).

---

## 3. Operators & precedence

### Supported operators

| Category | Operators | Notes |
|---|---|---|
| Arithmetic | `+` `-` `*` `/` `%` | Integer ops on `int`, floating ops on `float`. `/` on two ints is truncating integer division. `%` follows C semantics (sign of dividend). |
| Comparison | `==` `!=` `<` `<=` `>` `>=` | Result type `bool`. On `int`/`float` they compare values; on strings/references `==`/`!=` compare **identity (pointer)**, not contents — use `strEq` for string content equality. |
| Logical | `and` / `&&`, `or` / `\|\|` | `and`/`&&` are the same operator; `or`/`\|\|` likewise. |
| Unary | `-x`, `!x` | Numeric negation; boolean (and integer) logical-not. |
| Null safety | `?.`, `?:`, `!!` | Safe navigation, elvis/coalesce, force-unwrap. See [§12](#12-null-safety). |
| Coalesce | `??` | Synonym for `?:`. |
| Channel | `<-` | `ch <- v` sends; `<-ch` receives. See [§14](#14-concurrency). |
| Grouping | `( … )` | Parentheses override precedence. |
| Call / access | `f(...)`, `a.b`, `a[i]` | Function call, member access, indexing. |

### Actual precedence chain (lowest → highest binding)

This is the precedence as implemented by the recursive-descent parser
(`parser.cpp`). Higher rows bind tighter.

| Level | Operators | Associativity |
|---|---|---|
| 1 (loosest) | `=` (assignment) | right |
| 2 | `?:`, `??` (elvis / coalesce) | left |
| 3 | `or` / `\|\|` | left |
| 4 | `and` / `&&` | left |
| 5 | `==`, `!=` | left |
| 6 | `<`, `<=`, `>`, `>=` | left |
| 7 | `+`, `-` | left |
| 8 | `*`, `/`, `%` | left |
| 9 (prefix) | unary `-`, `!`, `<-` (receive), `await`, `new`, `delete` | right |
| 10 (tightest) | call `f()`, member `.`, safe member `?.`, index `[]`, force-unwrap `!!` | left (postfix) |

Verified consequences:

```tocin
2 + 3 * 4        // 14  (* tighter than +)
(2 + 3) * 4      // 20
10 - 2 - 3       // 5   (left associative)
1 + 2 == 3       // true (1) — comparison looser than +
1 < 2 and 3 < 4  // true (1)
1 > 2 or 3 < 4   // true (1)
```

> Comparisons are non-chaining: write `a < b and b < c`, not `a < b < c` (the
> latter parses as `(a < b) < c` and is not meaningful).

> **Not implemented as operators:** `**` (power), bitwise `& | ^ ~ << >>`, and
> the compound-assignments `+= -= *= /= %=` are recognized by the lexer but are
> **not accepted by the expression parser** and cause parse errors. Use the
> `pow(...)` builtin instead of `**`, and `x = x + 1` instead of `x += 1`.
> See [§20](#20-limitations--not-yet-supported).

---

## 4. Statements

Statements appear inside function/method bodies. Most statements end with `;`;
block-introducing statements (`if`, `while`, `for`, `match`, …) use `{ … }`.

### Variable declaration

```tocin
let total = 0;
let name: string = "Tocin";
const LIMIT = 100;
```

### Assignment

Assignment is an expression (used as a statement here). Valid targets are a
variable, a field (`obj.field`), or an array element (`arr[i]`).

```tocin
total = total + 1;
p.x = 10;          // field
arr[2] = 99;       // element
```

### `if` / `elif` / `else`

```tocin
if i % 15 == 0 {
    println("FizzBuzz");
} elif i % 3 == 0 {
    println("Fizz");
} elif i % 5 == 0 {
    println("Buzz");
} else {
    println("{}", i);
}
```

Braces are mandatory around every branch body. Conditions may be `bool`, or any
`int`/`float`/pointer value (nonzero / non-null is true).

### `while`

```tocin
let i = 0;
while i < 5 {
    i = i + 1;
}
```

### `for i in a..b` (range)

```tocin
for i in 0..15 {            // i = 0,1,...,14  (end exclusive)
    println("fib({}) = {}", i, fib(i));
}
```

`a..b` is a half-open integer range (`a` inclusive, `b` exclusive). The loop
variable is an `int` local scoped to the loop. The range bounds may be any
integer expressions.

There is also a `for v in <array>` form that iterates an array's elements, but
the range form is the well-exercised, recommended idiom.

### `return`

```tocin
return;            // from a void function
return expr;       // return a value
```

### Expression statements & blocks

Any expression followed by `;` is a statement (commonly a function call):

```tocin
println("hi");
```

A bare `{ … }` block introduces a new scope.

### `break` / `continue` — **not implemented**

`break` and `continue` are reserved keywords but the parser does **not** accept
them; using either inside a loop produces a parse error (and the offending
statement is dropped). Use a boolean guard / restructured condition instead.

---

## 5. Functions

### Declaration

```tocin
def add(a: int, b: int) -> int {
    return a + b;
}
```

* Parameters are `name: Type`, comma-separated. Parameter types are required.
* The return type may use `->` **or** `:`. Example: `def f(x: int): int { … }`.

### Return-type inference

If you omit the return type entirely, it is inferred from the first `return`
statement found in the body (void if none):

```tocin
def add(a: int, b: int) {   // inferred -> int
    return a + b;
}
```

### Recursion & forward references

Functions may call themselves and may call functions declared later in the file
(top-level functions are resolved in two passes):

```tocin
def isEven(n: int) -> bool {
    if n == 0 { return true; }
    return isOdd(n - 1);          // defined below
}
def isOdd(n: int) -> bool {
    if n == 0 { return false; }
    return isEven(n - 1);
}
```

### First-class functions, function-typed params & returns

Functions are values. You can pass them, return them, and store them:

```tocin
def inc(x: int) -> int { return x + 1; }
def trip(x: int) -> int { return x * 3; }

def apply(f: (int) -> int, x: int) -> int { return f(x); }

def chooser(big: int) -> (int) -> int {   // returns a function
    if big > 0 { return trip; }
    return inc;
}

def main() {
    println("{}", apply(inc, 5));   // 6
    return 0;
}
```

### Lambdas

```tocin
lambda (x: int) -> int x * x
```

A lambda has a parenthesized parameter list, an optional `-> ReturnType`, and a
**single expression** body (no block). Lambdas are most useful passed directly
to higher-order functions:

```tocin
println("{}", apply(lambda (x: int) -> int x * x, 9));  // 81
```

> Lambdas are **non-capturing**: the body can use its parameters and globals,
> but it does not close over surrounding locals. See
> [§20](#20-limitations--not-yet-supported).

### The function-typed-local rule

A local that holds a function value and is **initialized from a call** must be
given an explicit function-type annotation, so the compiler can recover the
call signature for the later indirect call:

```tocin
let f: (int) -> int = chooser(1);   // OK — annotation supplies the signature
println("{}", f(10));               // 30
```

Without the annotation the call through the local fails:

```tocin
let f = chooser(1);   // ERROR at the call site: "Called value is not a function"
println("{}", f(10));
```

(When the initializer is itself a function name or another already-annotated
function variable, the signature is propagated and no annotation is needed —
e.g. `let g = inc;` works because `inc`'s signature is known.)

---

## 6. Classes

A class has fields and methods. Fields use `name: Type;` (a leading `let`/`const`
is also accepted). Methods are `def` with a leading `self` receiver.

```tocin
class Point {
    x: int;
    y: int;
    def sum(self) -> int { return self.x + self.y; }
    def manhattan(self, other: int) -> int {
        return (self.x + self.y) - other;
    }
}
```

* **Construction:** call the class name with positional arguments matching the
  fields in declaration order: `Point(3, 4)`.
* **Field access / mutation:** `p.x`, and `p.x = 10;`.
* **`self`:** the receiver inside a method. Methods may call other methods on
  `self` (`self.doubled()`).
* `struct` is an exact synonym for `class`.

```tocin
def main() {
    let p = Point(3, 4);
    println("{}", p.sum());   // 7
    p.x = 10;                 // mutate a field
    println("{}", p.sum());   // 14
    return p.sum();           // exit 14
}
```

Class instances are reference values (opaque pointers). A class-typed variable
may hold `None` (see [§12](#12-null-safety)).

Generic classes (`class C<T>`) are covered in [§8](#8-generics).

---

## 7. Traits & impl

A `trait` declares method signatures (bodies optional). An `impl Trait for Type`
provides the methods for a type; an `impl Type` block adds **inherent** methods
to a type.

```tocin
trait Shape {
    def area(self) -> int;        // signature only
}

class Square {
    side: int;
    def area(self) -> int { return self.side * self.side; }
}

impl Shape for Square {
    def area(self) -> int { return self.side * self.side; }
}

// Inherent methods (no trait):
impl Square {
    def perimeter(self) -> int { return self.side * 4; }
}

def main() {
    let s = Square(5);
    println("{}", s.area());        // 25
    println("{}", s.perimeter());   // 20
    return 0;
}
```

Notes / current behavior:

* Traits are essentially a way to declare and group method signatures. `impl`
  blocks attach concrete methods to the named type; calls are resolved
  statically by receiver type.
* Trait *bounds* on generics are parsed (`<T: SomeTrait>`) but **ignored** —
  there is no constraint checking or dynamic dispatch through trait objects.
  See [TRAITS.md](TRAITS.md) for the broader design intent.

---

## 8. Generics

Generics are realized by **monomorphization**: a separate concrete copy of the
generic function/class is generated for each set of type arguments actually
used. Type arguments are **inferred** from the call/constructor — there is no
explicit "turbofish" syntax for supplying them.

### Generic functions

```tocin
def identity<T>(x: T) -> T { return x; }

def main() {
    println("{}", identity(42));    // 42   (T = int)
    println("{}", identity(3.5));   // 3.5  (T = float; separate instance)
    return 0;
}
```

Each distinct argument type produces its own specialization. Type parameters
that cannot be inferred from arguments default to `int` (i64).

### Generic classes

```tocin
class Box<T> {
    value: T;
    def get(self) -> T { return self.value; }
    def set(self, v: T) { self.value = v; }
}

class Pair<A, B> {
    first: A;
    second: B;
    def sum(self) -> A { return self.first + self.second; }
}

def main() {
    let bi = Box(42);        // Box<int> inferred from the constructor argument
    bi.set(100);
    println("{}", bi.get()); // 100

    let bf = Box(1.5);       // Box<float> — a distinct monomorphized instance
    println("{}", bf.get()); // 1.5

    let p = Pair(20, 22);
    println("{}", p.sum());  // 42
    return 0;
}
```

The concrete type argument is inferred from the constructor call; each
instantiation is a separate type behind the scenes.

---

## 9. Enums

An `enum` defines named integer constants. Members auto-increment from `0`; an
explicit `= value` (optionally negative) resets the counter.

```tocin
enum Color { Red, Green, Blue }                 // 0, 1, 2
enum Code  { Ok = 0, NotFound = 404, Err = -1 }

def main() {
    println("{}", Green);     // 1
    println("{}", NotFound);  // 404
    println("{}", Err);       // -1
    return 0;
}
```

Enum members are plain `int` constants in scope by their bare name. There are
no associated payloads (for tagged unions, use `Option`/`Result` and pattern
matching — [§10](#10-pattern-matching), [§13](#13-option--result)).

---

## 10. Pattern matching

`match` selects a branch by testing the scrutinee against each `case`. Every
case body is a `{ … }` block. An optional `default` runs if no case matches.

### Value-equality patterns

```tocin
def classify(n: int) -> int {
    match n {
        case 0: { return 100; }
        case 1: { return 200; }
        default: { return 999; }
    }
    return -1;
}
```

The pattern is an expression compared for equality with the scrutinee
(integers/floats by value; other values by identity).

### Constructor patterns (`Some`/`None`/`Ok`/`Err`)

`match` recognizes the `Option`/`Result` constructors specially: it checks the
tag and **binds the inner payload**.

```tocin
match Some(42) {
    case Some(x): { println("got {}", x); }   // got 42
    case None:    { println("nothing"); }
}

match safeDiv(100, n) {
    case Ok(v):  { println("ok: {}", v); }
    case Err(e): { println("error code {}", e); }
}
```

* `Some(x)` / `Ok(v)` match a non-null wrapper with tag 1 and bind the payload.
* `Err(e)` matches a non-null wrapper with tag 0 and binds the payload.
* `None` matches the null value.
* The bound name (`x`, `v`, `e`) must be a single identifier. The payload is
  bound as an `i64` slot (see [§13](#13-option--result) / [§18](#18-memory-model--abi)).

Cases are tested top-to-bottom; the first match wins. Match is a statement (it
does not yield a value); produce results via `return` or by assigning inside
the case bodies.

---

## 11. Error handling

Tocin has exceptions built on `throw` + `try`/`catch`/`finally`, implemented
with `setjmp`/`longjmp` and a thread-local handler stack.

```tocin
def divide(a: int, b: int) -> int {
    if b == 0 {
        throw 1;          // unwinds to the nearest catch, carrying code 1
    }
    return a / b;
}

def main() {
    try {
        let r = divide(10, 0);
        println("{}", r);            // not reached
    } catch (e) {
        println("error code {}", e); // error code 1
    } finally {
        println("done");             // always runs
    }
    return 0;
}
```

### Semantics (verified)

* **`throw expr;`** evaluates `expr`, normalizes it to a 64-bit payload, and
  unwinds to the nearest enclosing `catch`. The payload is conceptually an
  integer error code (an integer is the supported, well-behaved case — see the
  gap below).
* **`try { … } catch (e) { … }`** runs the body; if a `throw` unwinds into it,
  control transfers to the catch block with `e` bound to the integer payload.
  The catch variable is optional: `catch { … }` or `catch e { … }` are also
  accepted.
* **`finally { … }`** runs on **all** paths: normal completion of the try body,
  after a caught exception, and on the re-throw path.
* **`try`/`finally` with no `catch`** runs the finally block and then
  **re-throws** to the next enclosing handler:

```tocin
try {
    try {
        throw 9;
    } finally {
        println("inner finally");   // runs
    }
} catch (e) {
    println("outer caught {}", e);  // outer caught 9
}
```

* At least one of `catch` / `finally` must follow a `try`.

### Documented gaps

* The thrown payload is treated as a **64-bit integer slot**. Throwing
  non-integer values (strings, objects) is not modeled as a typed exception:
  the value is reinterpreted as an `i64`, and `e` in the catch is an `int`.
  Use integer error codes.
* There is no exception type hierarchy, no rethrow keyword (`throw;`), and no
  pattern-matching on exception types — a `catch` catches everything.
* If a thrown exception reaches the top with no handler, behavior is a runtime
  abort (no stack trace). Wrap fallible code in `try`.

See [ERROR_HANDLING.md](ERROR_HANDLING.md) for additional discussion.

---

## 12. Null safety

Reference values (class instances, `Option`/`Result`, strings) can be `None`
(a null pointer). Three operators make working with possibly-null references
concise:

| Operator | Name | Behavior |
|---|---|---|
| `a?.field` | Safe navigation | Reads `field` only when `a` is non-null; yields `0`/null when `a` is null (no trap). |
| `a ?: b` | Elvis / coalesce | Yields `a` when it is non-null, otherwise `b`. (`??` is a synonym.) |
| `a!!` | Force-unwrap | Asserts `a` is non-null; the value passes through. (Intended to trap on null.) |

```tocin
class Box {
    v: int;
    def get(self) -> int { return self.v; }
}

def main() {
    let present: Box = Box(42);
    let missing: Box = None;          // a null reference

    let b = missing ?: Box(7);        // missing is null -> Box(7)
    println("{}", b.get());           // 7

    println("{} {}", present?.v, missing?.v);  // 42 0  (safe nav)
    println("{}", present!!.get());            // 42    (force-unwrap)
    return 0;
}
```

`None` is also the empty case of `Option`/`Result` and the "no value" used in
pattern matching ([§10](#10-pattern-matching)).

See [NULL_SAFETY.md](NULL_SAFETY.md) for more.

---

## 13. Option / Result

`Option` and `Result` are tagged single-payload wrappers used for fallible
computations. They share one runtime representation.

### Construction

| Constructor | Meaning | Tag |
|---|---|---|
| `Some(x)` | Option holding `x` | 1 |
| `None` | Empty Option (the null value) | — (null) |
| `Ok(v)` | Successful Result holding `v` | 1 |
| `Err(e)` | Failed Result holding `e` | 0 |

`Some(x)`, `Ok(v)`, `Err(e)` each allocate a small `{ i64 tag, i64 payload }`
record on the heap and return a pointer; `None` is the null pointer. The payload
is stored in a 64-bit slot.

### Use

Construct them as return values and **destructure with `match`**:

```tocin
def safeDiv(a: int, b: int) -> Result {
    if b == 0 {
        return Err(1);      // error code 1
    }
    return Ok(a / b);
}

def describe(n: int) -> int {
    match safeDiv(100, n) {
        case Ok(v):  { println("ok: {}", v); return v; }
        case Err(e): { println("error code {}", e); return -1; }
    }
    return 0;
}

def find(x: int) -> Option {
    if x > 0 { return Some(x); }
    return None;
}

def main() {
    match find(5) {
        case Some(v): { println("some {}", v); }   // some 5
        case None:    { println("none"); }
    }
    describe(4);     // ok: 25
    describe(0);     // error code 1
    return 0;
}
```

Use `Option`/`Result` as the declared return type (they are interchangeable at
the representation level). Because the payload is an `i64` slot, integer
payloads are the cleanly-supported case; see [§18](#18-memory-model--abi) and
[OPTION_RESULT_TYPES.md](OPTION_RESULT_TYPES.md).

---

## 14. Concurrency

Tocin offers Go-style concurrency: `go` to spawn, typed channels for
communication, and `select` to multiplex.

### `go` — spawn a goroutine

```tocin
go worker(ch, i);
```

`go` runs a **function call** on a new OS thread. The callee must be a known
named function; its arguments are evaluated in the spawning thread, packed onto
the heap, and unpacked by a generated thunk in the new thread.

### Channels

* Create: `channel<T>()` (the `<T>` is parsed; the runtime channel is untyped
  and moves 64-bit slots).
* **Send:** `ch <- value;`
* **Receive:** `<-ch` (an expression yielding the next value).

Sends and receives synchronize the producing and consuming threads.

```tocin
def worker(ch: channel<int>, n: int) {
    ch <- n * n;                  // send this worker's result
}

def main() {
    let ch = channel<int>();
    for i in 1..6 {
        go worker(ch, i);         // 5 goroutines
    }
    let total = 0;
    for i in 0..5 {
        total = total + <-ch;     // collect 5 results
    }
    println("sum of squares 1..5 = {}", total);  // 55
    return total;
}
```

You can bind a received value to a local: `let x = <-ch;`.

### `select`

`select` waits on multiple channel receives and runs the case whose channel
becomes ready first. A `default` case makes the select **non-blocking** (it runs
immediately if no channel is ready); without a `default`, `select` blocks.

```tocin
let fast = channel<int>();
let slow = channel<int>();
go worker(fast, 10);

let got = 0;
select {
    case v = <-fast: { got = v; }   // bind the received value to v
    case v = <-slow: { got = v; }
}
println("received {}", got);        // received 10

select {
    case v = <-slow: { println("unexpected {}", v); }
    default: { println("nothing ready"); }   // runs: nothing is ready
}
```

Each receive case is `case v = <-ch: { … }` (the `v =` binding is optional).
The values moved through channels are 64-bit slots (see
[§18](#18-memory-model--abi)). See [CONCURRENCY.md](CONCURRENCY.md) for more.

---

## 15. Macros

Tocin has **function-like, token-level macros** that are expanded by a
preprocessing pass *before* parsing. Disable with `--no-macros`.

### Definition & invocation

```tocin
macro square(x) { x * x }
macro hypot2(a, b) { square!(a) + square!(b) }

def main() {
    println("{}", square!(5));        // 25
    println("{}", square!(2 + 3));    // 25  (precedence-safe)
    println("{}", hypot2!(3, 4));     // 25  (macros may call macros)
    return hypot2!(3, 4);
}
```

* **Definition:** `macro NAME(params) { body }` where `params` is a
  comma-separated list of identifiers. Definitions are removed from the token
  stream.
* **Invocation:** `NAME!(args)`. Arguments are token sequences split on
  top-level commas; bracket/paren/brace nesting is respected.
* **Expansion:** each parameter occurrence in the body is replaced by its
  argument **wrapped in parentheses**, and the whole expansion is wrapped in
  parentheses too. This makes macros compose like expressions and respect
  operator precedence (e.g. `square!(2 + 3)` expands to `((2 + 3) * (2 + 3))`).
* **Nesting / composition:** expansion repeats to a fixed point (up to 128
  rounds), so macros can invoke other macros.
* The argument count must match the parameter count, or expansion is a fatal
  error.

### Limitations (hygiene)

* Substitution is purely **textual at the token level** — macros are **not
  hygienic**. A parameter name is matched by identifier text; there is no
  renaming of macro-introduced names, and an argument that mentions a name used
  inside the body can collide. Keep macros small and expression-shaped.
* Macros operate on tokens, not the AST: they cannot introspect types or
  generate declarations conditionally.

---

## 16. Modules

A program is split across files and combined with `import`. Imports are resolved
and the imported file's top-level declarations are **merged** into the program
(textual inclusion, deduplicated), so imported names are available globally.

### Syntax

```tocin
import std.linq;          // dotted path  -> std/linq.to
import a.b.c;             // -> a/b/c.to
import "relative/path";   // string path  -> relative/path.to
```

A dotted path `a.b.c` maps to the relative file `a/b/c.to`. A string path is
used as-is. The trailing `.to` is added automatically if absent. The `;` is
optional.

```tocin
import std.linq;

def main() {
    let xs = [3, 1, 4, 1, 5];
    println("{}", reduceSum(xs));   // 14
    return 0;
}
```

### Resolution order

For each import, the compiler searches, in order:

1. The **directory of the importing file** (relative import).
2. **`$TOCIN_PATH`** (an environment variable naming a search root), if set.
3. The compiled-in **standard-library path** (`stdlib/`, e.g. `std/linq.to`).

The first existing file wins. An imported module's own imports are processed
first (so transitive dependencies load), and each file is loaded at most once.
There is no symbol namespacing or selective import — every top-level name from
an imported file becomes globally visible.

---

## 17. FFI

Tocin can call C functions directly. (Python and JavaScript FFI are scaffolded
but **not functional** today.) The full reference is [ffi.md](ffi.md); the
essentials:

### Declaring and calling C functions

Declare an external C function with `extern def` (a `def` with no body), then
call it like any Tocin function:

```tocin
extern def labs(x: int) -> int;                 // C long abs
extern def atoi(s: string) -> int;              // parse int
extern def putchar(c: int) -> int;              // write a byte
extern def hypot(x: float, y: float) -> float;  // libm

def main() {
    println("{}", labs(-42));        // 42
    println("{}", atoi("123"));      // 123
    println("{}", hypot(3.0, 4.0));  // 5
    putchar(72); putchar(105); putchar(10);   // "Hi\n"
    return labs(-7);                 // exit 7
}
```

* Under `--run` (JIT), the symbol is resolved from the running process (linked
  against libc/libm), so the C standard library is available out of the box.
* For native builds (`-o`), the external symbol is linked by the C toolchain.

### Type mapping

| Tocin | C / LLVM |
|---|---|
| `int` | 64-bit integer (`i64`) |
| `i32` `i16` `i8` | sized integers |
| `float` | `double` |
| `f32` | `float` |
| `bool` | `i1` |
| `string` | `char*` |
| `void` (no `-> T`) | `void` |

### Builtin-shadowing gotcha

A set of names is intercepted by the compiler as **builtins** and will **not**
reach the FFI path even if you declare them `extern`: `sqrt sin cos tan asin
acos atan exp log log2 log10 floor ceil round fabs pow abs min max len print
println`. To exercise the real FFI path, use other symbols (e.g. `labs`, `atoi`,
`getenv`, `putchar`, `hypot`, `toupper`). Only `extern`-declared functions
resolve; calling an undeclared C function is a compile error. Note that `int` is
64-bit, so declaring a C `int`-returning function as `-> int` relies on the
x86-64 SysV ABI — use `-> i32` if you need an exact C `int`.

---

## 18. Memory model & ABI

* **Heap allocation.** Class instances, `Option`/`Result` records, array/list
  literals, channels, and dynamic collections live on the heap (via `malloc` /
  runtime allocators). Class/array/channel values are passed around as **opaque
  pointers**.
* **No automatic memory management.** There is no garbage collector and no
  automatic destruction; heap allocations are **leaked** for the program
  lifetime unless an explicit free builtin is called (`vecFree`, `mapFree`).
  `new`/`delete` are parsed but allocate stack-locally and are not a general
  heap-management facility. For typical short-lived programs the leak is benign.
* **Arrays.** An array literal `[a, b, c]` is a flat heap block laid out as
  `[i64 length][elem 0][elem 1]…`. `len(arr)` reads the length word; `arr[i]`
  computes `base + 8 + i*sizeof(elem)`. The element type is tracked from the
  literal / annotation so indexing loads the right width.
* **64-bit slot ABI.** Channels, `Option`/`Result` payloads, the dynamic
  collection builtins (`vec*`, `map*`), and thrown exception values all move
  values through a uniform **64-bit slot**:
  - integers are sign-extended/truncated to `i64`,
  - `float`/`f64` are bit-cast to `i64` (and `f32` is promoted to `f64` first),
  - pointers (strings, objects) are `ptrtoint`-ed to `i64`.
  On receipt the slot is reinterpreted to the expected type. This is why these
  facilities are cleanest with `int` payloads, and why a value round-tripped
  through them must be used at a consistent type.
* **Strings** are immutable `char*` (NUL-terminated). String concatenation with
  `+` produces a new string; `==` on strings compares **pointers**, so use
  `strEq` for content comparison.

---

## 19. Compilation & execution

```text
tocin [options] FILE.to
```

| Option | Effect |
|---|---|
| `--run` (alias `--jit`) | JIT-compile and run immediately. The program's exit code is `main`'s return value. |
| `-o <file>` | Write output. The extension selects the format: `.ll` = LLVM IR, `.s` = assembly, `.o` = object file, anything else = **native executable**. |
| `-O0` / `-O1` / `-O2` / `-O3` | Optimization level. **Default is `-O2`.** |
| `--dump-ir` | Print the generated LLVM IR to stdout. |
| `--target <native\|wasm>` | Compilation target (native is the supported path). |
| `--no-ffi`, `--no-concurrency`, `--no-advanced`, `--no-macros`, `--no-async` | Disable the corresponding feature/pass. |
| `--help` | Show usage. |

Examples:

```bash
./build/tocin examples/hello.to --run                 # JIT run
./build/tocin examples/hello.to -o hello && ./hello   # native executable
./build/tocin examples/fib.to  -o fib.ll -O3          # emit optimized LLVM IR
./build/tocin examples/point.to --run; echo $?        # prints 14 (main's return)
```

The exit code equals the integer returned by `main` (truncated to the OS's
8-bit exit status by the shell, as usual).

---

## 20. Limitations / not yet supported

Known gaps in the current implementation (verified against the compiler):

* **`break` / `continue`** are reserved but **not parsed**; using them is a
  parse error. ([§4](#4-statements))
* **Power `**`, bitwise `& | ^ ~ << >>`, and compound assignments
  `+= -= *= /= %=`** are lexed but **not accepted by the expression parser**.
  Use `pow()` and explicit `x = x + 1`. ([§3](#3-operators--precedence))
* **Lambdas are non-capturing** and have **single-expression bodies** only;
  they do not close over surrounding locals. ([§5](#5-functions))
* **A function-typed local initialized from a call requires an explicit
  function-type annotation.** ([§5](#5-functions))
* **`Option`/`Result` and channel payloads are 64-bit slots** — typed/object
  payloads are reinterpreted as `i64`; integer payloads are the supported case.
  ([§13](#13-option--result), [§18](#18-memory-model--abi))
* **Exceptions carry an integer payload only**; there is no exception type
  hierarchy, no typed `catch`, and no bare `throw;` rethrow.
  ([§11](#11-error-handling))
* **Trait bounds on generics are ignored**; there is no constraint checking and
  no trait-object dynamic dispatch. Generics have **no explicit type-argument
  syntax** (turbofish) — types are inferred. ([§7](#7-traits--impl),
  [§8](#8-generics))
* **`string ==` compares identity, not contents** — use `strEq`.
  ([§3](#3-operators--precedence), [§18](#18-memory-model--abi))
* **No garbage collection**; most heap allocations leak for the process
  lifetime. ([§18](#18-memory-model--abi))
* **`const` is not enforced** (behaves like `let`). ([§2](#2-types))
* **Dictionary literals** construct a value, but there is no rich, ergonomic
  dict API at the language level; the `map*` builtins are the practical
  key/value store. ([§21](#21-built-in-function-reference))
* **Python / JavaScript FFI are not functional** — only the C path works.
  ([§17](#17-ffi))
* Many lexer keywords (`switch`, `interface`, `defer`, `yield`, `async`/`await`
  beyond parsing, etc.) are reserved but not implemented end-to-end; avoid them
  as identifiers and don't rely on them.

---

## 21. Built-in function reference

These functions are recognized directly by the compiler/runtime (no import
needed unless noted). Names marked *(reserved)* also shadow any `extern` of the
same name (see [§17](#17-ffi)).

### I/O & formatting

| Function | Description |
|---|---|
| `print(...)` *(reserved)* | Print without a trailing newline. |
| `println(...)` *(reserved)* | Print with a trailing newline. |

Two calling styles for `print`/`println`:

* **Format style:** the first argument is a string literal containing `{}`
  placeholders, followed by values: `println("x = {}, y = {}", x, y)`. Each
  `{}` is filled by the next argument (`%lld` for ints/bools, `%g` for floats,
  `%s` for strings). Escapes like `\n`, `\t` are honored in the literal.
* **Sequential style:** `print(a, b, c)` prints each argument in turn with no
  separators.

```tocin
println("{} {} {}", 1, 2.5, "x");  // 1 2.5 x
print("a"); print("b"); println("c");  // abc
```

### Math *(all reserved)*

| Function | Signature | Notes |
|---|---|---|
| `sqrt sin cos tan asin acos atan exp log log2 log10 floor ceil round fabs` | `(float) -> float` | Unary libm functions; integer args are promoted to float. |
| `pow(x, y)` | `(float, float) -> float` | `x` to the power `y`. Use this instead of `**`. |
| `abs(x)` | numeric → same type | Absolute value (int or float). |
| `min(a, b)` / `max(a, b)` | numeric → same type | Two-argument min/max. |

```tocin
println("{}", pow(2.0, 10.0));  // 1024
println("{}", sqrt(16.0));      // 4
println("{}", max(3, 9));       // 9
```

### Arrays *(reserved: `len`)*

| Function | Description |
|---|---|
| `len(arr)` | Length of an array literal value. |

Arrays also support `arr[i]` (read) and `arr[i] = v` (write).

### Dynamic vector (heap, 64-bit slots)

`vecNew()`, `vecPush(v, x)`, `vecGet(v, i)`, `vecSet(v, i, x)`, `vecLen(v)`,
`vecPop(v)`, `vecFree(v)`.

```tocin
let v = vecNew();
vecPush(v, 5); vecPush(v, 7);
println("{} {}", vecLen(v), vecGet(v, 1));  // 2 7
```

### Hash map (heap, 64-bit slots)

Integer keys: `mapNew()`, `mapPut(m, k, v)`, `mapGet(m, k)`, `mapHas(m, k)`,
`mapLen(m)`, `mapFree(m)`.
String keys: `mapPutStr(m, k, v)`, `mapGetStr(m, k)`, `mapHasStr(m, k)`.

```tocin
let m = mapNew();
mapPut(m, 1, 100); mapPut(m, 2, 200);
println("{} {}", mapGet(m, 2), mapHas(m, 3));  // 200 0
```

### Strings

`strLen(s)`, `charAt(s, i)`, `substring(s, a, b)`, `strEq(a, b)`,
`strCmp(a, b)`, `indexOfChar(s, c)`, `intToStr(n)`, `strToInt(s)`,
`charToStr(c)`. String concatenation uses `+`.

```tocin
let s = "hello";
println("{} {} {}", strLen(s), charAt(s, 1), strEq("a", "a"));  // 5 101 1
```

### File I/O

`readFile(path)` → string, `writeFile(path, content)`, `appendFile(path, content)`,
`readLine()` → string (reads a line from stdin).

### LINQ-style collection ops (require `import std.linq;`)

Reductions and transforms over arrays, e.g. `reduceSum(xs)`, `reduceProduct(xs)`,
`aggregate(xs, seed, op)`, `countGreater(xs, n)`, `mapScaleInto(dst, src, k)`,
`filterGreaterInto(dst, src, n)`. `*Into` variants write into a destination
array and return a count; reductions return a scalar.

```tocin
import std.linq;
def main() {
    let xs = [3, 1, 4, 1, 5];
    println("{}", reduceSum(xs));   // 14
    return 0;
}
```

See [LINQ.md](LINQ.md) and [STDLIB_GUIDE.md](STDLIB_GUIDE.md) for the broader
standard library.

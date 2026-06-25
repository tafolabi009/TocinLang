# Tocin for AI — Expert Spec

A single self-contained reference that makes an LLM an instant expert in **Tocin**, able to write correct, idiomatic, compiling code on the first try. Every snippet here has been compiled and run with the reference toolchain. Where the language has sharp edges, this document is blunt about them: read the **GOTCHAS** section before writing anything non-trivial.

> Toolchain used to verify: `/home/user/TocinLang/build/tocin`. Run a file with
> `./build/tocin FILE.to --run` (JIT + execute). The process exit code is the
> integer returned by `main`. Source of truth: `src/lexer/lexer.cpp`,
> `src/lexer/token.h`, `src/parser/parser.cpp`, `src/codegen/ir_generator.cpp`,
> `src/main.cpp`, plus `examples/*.to`, `tests/cases/*.to`, `stdlib/std/*.to`.

---

## 1. Mental model (read this first)

Tocin is a **statically typed, ahead-of-time compiled** language that lowers to **LLVM IR** and then to native code (or runs via an LLVM JIT with `--run`). The surface syntax is **C-like with braces and semicolons**, but flavored with Python/Rust/Kotlin keywords (`def`, `let`, `match`, `trait`, `impl`, `lambda`, `elif`, `?:`, `?.`, `!!`).

Semantics are **value semantics for scalars** (`int`, `float`, `bool` live in registers/stack slots) and **heap handles (opaque pointers) for everything else** — class/struct instances, strings (`char*`), arrays, dynamic collections (`vector`/`map`), channels, and `Option`/`Result` boxes. Heap memory is **garbage-collected** (Boehm GC, when the runtime is built with it — the default): unreachable allocations are reclaimed automatically, so long-running programs do not leak. `vecFree`/`mapFree`/`free` remain available for eager release of large buffers, but are optional.

The compiler is **pragmatic, not a full type system**. Type inference is local and shallow: a `let` with an initializer infers the variable's LLVM type from that initializer's value; function parameters and return types are explicitly annotated (or the return type is inferred from the body). Mixed int/float arithmetic **does** auto-promote the int operand to `float` (e.g. `3.0 + 2` → `5.0`; `let x: float = 5;` → `5.0`). Lambdas **do** capture enclosing locals **by value** (a snapshot), and the resulting closures can be returned/escape with independent state. `break` and `continue` **work** in every loop form. Some keywords still exist in the lexer but are **not implemented** in the parser/codegen — notably most ownership keywords (`switch`, `defer`, `break`, and `continue` are all implemented). Treat the verified feature set in this document as the real language.

Key runtime facts:
- `int` = 64-bit signed (`i64`). `float` = 64-bit (`double`). `bool` = `i1`.
- Heap payload slots (collection elements, channel messages, `Option`/`Result` payloads, thrown values, `map` keys/values) are all **64-bit slots**. Store integers freely; pointers/strings stored as elements are bit-cast to/from `i64` and are usable but limited (see GOTCHAS).
- `None` / `nil` is literally the **null pointer**.

---

## 2. Compact grammar (derived from `parser.cpp`, verified)

Notation: `*` = zero-or-more, `?` = optional, `|` = alternation, `UPPER` = token, lowercase = rule. Whitespace/indentation is **not significant** — blocks are `{ ... }`, statements end with `;`.

```
program        ::= declaration*

declaration    ::= varDecl | funcDecl | externDecl | classDecl | enumDecl
                 | traitDecl | implDecl | importStmt | statement

varDecl        ::= ("let" | "const") IDENT (":" type)? ("=" expression)? ";"
funcDecl       ::= ("def" | "async" "def") IDENT typeParams? "(" params ")" retType? "{" block "}"
externDecl     ::= "extern" "def" IDENT typeParams? "(" params ")" retType? ";"     // no body
classDecl      ::= ("class" | "struct") IDENT typeParams? "{" classMember* "}"
classMember    ::= varDecl | funcDecl | IDENT ":" type ("=" expression)? ";"?       // field or method
enumDecl       ::= "enum" IDENT "{" (IDENT ("=" "-"? INT)? ","?)* "}"
traitDecl      ::= "trait" IDENT typeParams? "{" methodSig* "}"                      // method bodies optional
implDecl       ::= "impl" (IDENT "for")? IDENT typeParams? "{" funcDecl* "}"
importStmt     ::= "import" (STRING | IDENT ("." IDENT)*) ";"?

typeParams     ::= "<" IDENT (":" type)? ("," IDENT (":" type)?)* ">"               // constraint parsed, ignored
params         ::= ( ("self" (":" type)?) | (IDENT ":" type) ) ("," ...)*           // every non-self param MUST have a type
retType        ::= ("->" | ":") type

statement      ::= ifStmt | whileStmt | forStmt | "{" block "}" | returnStmt
                 | matchStmt | switchStmt | goStmt | selectStmt | tryStmt | throwStmt
                 | "break" ";"? | "continue" ";"? | "defer" statement | exprStmt
switchStmt     ::= "switch" expression "{" ("case" expression ":" "{" block "}")* ("default" ":" "{" block "}")? "}"  // alias of match
ifStmt         ::= "if" expression "{" block "}" ("elif" expression "{" block "}")* ("else" "{" block "}")?
whileStmt      ::= "while" expression "{" block "}"
forStmt        ::= "for" IDENT (":" type)? "in" expression (".." expression)? "{" block "}"
returnStmt     ::= "return" expression? ";"
matchStmt      ::= "match" expression "{" ("case" expression ":" "{" block "}")* ("default" ":" "{" block "}")? "}"
goStmt         ::= "go" callExpr ";"
selectStmt     ::= "select" "{" ("case" (IDENT "=")? "<-" expr ":" "{" block "}")* ("default" ":" "{" block "}")? "}"
tryStmt        ::= "try" "{" block "}" ("catch" ("(" IDENT? ")" | IDENT)? "{" block "}")? ("finally" "{" block "}")?
throwStmt      ::= "throw" expression ";"
exprStmt       ::= expression ";"

type           ::= "(" (type ("," type)*)? ")" "->" type            // function type
                 | (IDENT | "channel") ("<" type ("," type)* ">")?  // simple or generic
                 | IDENT ("|" type)*                                // union type (parsed; treated as opaque ptr)

expression     ::= assignment
assignment     ::= elvis (("="|"+="|"-="|"*="|"/="|"%=") assignment)?   // target: var | obj.field | arr[i]
elvis          ::= or (("?:"|"??") or)*
or             ::= and ("||" and)*
and            ::= bitOr ("&&" bitOr)*
bitOr          ::= bitXor ("|" bitXor)*
bitXor         ::= bitAnd ("^" bitAnd)*
bitAnd         ::= equality ("&" equality)*
equality       ::= comparison (("=="|"!=") comparison)*
comparison     ::= shift (("<"|"<="|">"|">=") shift)*
shift          ::= term (("<<"|">>") term)*
term           ::= factor (("+"|"-") factor)*
factor         ::= unary (("*"|"/"|"%") unary)*
unary          ::= ("!"|"-"|"~") unary | "await" unary | "new" newExpr | "delete" expr
                 | "<-" unary               // channel receive (prefix)
                 | call
call           ::= primary ( "(" args ")" | "." IDENT | "?." IDENT | "!!" | "[" expr "]" | "<-" expr )*
primary        ::= INT | FLOAT | STRING | "true" | "false" | "None" | IDENT
                 | "channel" ("<" type ">")? "(" ")"   // new channel
                 | "(" expression ")"                  // grouping
                 | "[" (expression ("," expression)*)? "]"            // array/list literal
                 | "{" (expression ":" expression ("," ...)*)? "}"    // dict literal (limited support)
                 | "lambda" "(" params ")" ("->" type)? expression    // lambda: body is ONE expression
```

**Operator precedence**, lowest to highest (this is the recursive-descent chain in `parser.cpp` and is authoritative):

1. `=` assignment, and compound assignment `+= -= *= /= %=` (right-assoc; compound desugars to `x = x OP y`)
2. `?:` `??` (elvis / null-coalescing)
3. `||` or `or` (logical OR — both spellings work)
4. `&&` or `and` (logical AND — both spellings work)
5. `|` (bitwise OR)
6. `^` (bitwise XOR)
7. `&` (bitwise AND)
8. `==` `!=`
9. `<` `<=` `>` `>=`
10. `<<` `>>` (bit shifts)
11. `+` `-`
12. `*` `/` `%`
13. unary `!` `-` `~`, `await`, `new`, `delete`, prefix `<-`
14. postfix: call `()`, member `.`, safe member `?.`, force-unwrap `!!`, index `[]`, channel send `<-`

> Both symbolic (`&&` `||`) and keyword (`and` `or`) forms of the logical operators work and are equivalent. Logical NOT is `!` only — the word `not` is **not** an operator. There is no ternary `?:` in the C sense; `?:` is the Kotlin-style elvis operator (null-coalescing). The power operator `**` and `++`/`--` are not implemented.

---

## 3. Keywords and operators (complete, with real meaning)

### Implemented keywords (safe to use)

| Keyword | Meaning |
|---|---|
| `def` | Define a function or method. |
| `let` | Declare a mutable local/field; type inferred from initializer or annotated. |
| `const` | Like `let` (compiles to the same thing; no enforced immutability). |
| `if` / `elif` / `else` | Conditional. Condition needs no parentheses; body needs braces. |
| `while` | While loop. |
| `for ... in` | Iterate a range `a..b` or index an array (`for i in 0..len(a)`). |
| `in` | Part of `for x in ...`. |
| `return` | Return from a function (optionally with a value). |
| `match` / `case` / `default` | Pattern match (int/float equality, and `Some/Ok/Err/None` constructor patterns). |
| `class` / `struct` | Define a record type with fields + methods. `struct` and `class` are identical. |
| `enum` | Integer-valued enum. |
| `trait` | Declare an interface (method signatures, optional default bodies). |
| `impl` | `impl Type { ... }` inherent methods, or `impl Trait for Type { ... }`. |
| `import` | Pull in another `.to` file/module (concatenates its top-level decls). |
| `extern` | `extern def name(...)->T;` declares a C function (resolved from the process/libm). |
| `true` / `false` | Boolean literals. |
| `None` | The null pointer; the empty `Option`; matches `case None:`. |
| `and` / `or` | Logical AND/OR. The symbolic forms `&&` / `||` also work and are equivalent. (NOT: use `!`; the word `not` is not an operator.) |
| `lambda` | Anonymous function value (body is a single expression). **Captures enclosing locals by value** (snapshot at creation); closures may be returned and escape. |
| `self` | Method receiver (first parameter). |
| `throw` / `try` / `catch` / `finally` | Exceptions (integer/handle payload via setjmp/longjmp). |
| `go` | Spawn a goroutine (OS thread): `go f(args);`. |
| `channel` | Channel type/constructor: `channel<int>()`. |
| `select` | Wait on multiple channel receives. |
| `<-` | Channel send (`ch <- v;`) and receive (`<-ch`). |
| `async` / `await` | Parsed; async functions get a wrapper. Prefer goroutines+channels for real concurrency. |
| `new` / `delete` | `new` allocates; rarely needed (constructors via `ClassName(...)`). |

### Lexer keywords that are NOT implemented (do NOT use — they will fail to compile)

`panic`, `recover`, `assert`, `yield`, `generator`, `coroutine`, `spawn`, `join`, `mutex`/`lock`/`unlock`, `atomic`, `volatile`, `move`/`borrow`, `constexpr`, `inline`, `export`, `module`, `namespace`, `package`, `using`, `with`, `super`, `as`, `is`, `instanceof`, `typeof`, `where`, `pub`/`priv`/`static`/`final`/`abstract`/`virtual`/`override`, `null`, `undefined`, `from`. Several of these are reserved words, so they also can't be used as identifiers. (`break`, `continue`, `switch`, and `defer` **are** implemented.)

> **`break`/`continue` are the most common trap.** They parse as expression statements and produce `error [S001]: Expected expression`. Restructure loops with a boolean flag or a `while` condition instead. **Use `None`, never `null`.**

### Operators

| Operator | Meaning | Notes |
|---|---|---|
| `+ - * /` | Arithmetic | `/` on ints truncates toward zero. `+` on two strings concatenates (heap-allocates). Mixed int/float auto-promotes the int to float (`3.0 + 2` → `5.0`, `10 / 4.0` → `2.5`). |
| `%` | Modulo (integers) | |
| `+= -= *= /= %=` | Compound assignment | Desugar to `x = x OP y`. Target: variable, `obj.field`, or `arr[i]`. `s += "x"` concatenates strings. |
| `== !=` | Equality | Ints, floats, and **strings by value** (`name == "Alice"` compares contents). Non-string pointers (class instances, `None`) compare by identity. |
| `< <= > >=` | Comparison | Ints and floats. |
| `&& \|\|` or `and` `or` | Logical | Both spellings work and are equivalent. Operands are truthy ints/bools. |
| `!` | Logical not | `!x` where x is int/bool. The word `not` is **not** an operator. |
| `& \| ^` | Bitwise AND/OR/XOR | Integer operands. |
| `<< >>` | Bit shifts | `>>` is arithmetic (sign-preserving). |
| `~` | Bitwise NOT (unary) | `~0` → `-1`. |
| unary `-` | Negation | int or float. |
| `=` | Assignment | Target: variable, `obj.field`, or `arr[i]`. |
| `a..b` | Range (exclusive of `b`) | Only in `for i in a..b`. `for i in 1..4` yields `1,2,3`. |
| `?:` / `??` | Elvis / null-coalescing | `a ?: b` = `a` if non-null else `b`. Short-circuits (b evaluated only if a is null). On a non-pointer `a`, result is always `a`. |
| `?.` | Safe member access | `a?.field` = `field` if `a` non-null, else a zero/null of the field type. |
| `!!` | Force-unwrap | `a!!` = `a` if non-null, else **calls `abort()`** (process dies). On a non-pointer, identity. |
| `<-` | Channel send/receive | `ch <- v` (send, statement/expression); `<-ch` (receive, prefix, yields the value). |
| `.` | Member access / method call | `obj.field`, `obj.method(args)`, `Enum.Member`. |
| `[]` | Index | Array element read/write: `a[i]`, `a[i] = v`. |
| `()` | Call | Function/method/constructor/lambda call. |

> **Not implemented:** the power operator `**` and the increment/decrement operators `++`/`--`. (Bitwise `& | ^ << >> ~` and compound assignment `+= -= *= /= %=` **are** implemented — see the rows above.) `|` is also used inside type annotations to write union types.
>
> **Integer literals** support `0x` hex, `0o` octal, `0b` binary, and `_` digit separators: `0xFF`, `0o17`, `0b1010`, `1_000_000`.

### Comments

```
// line comment
/* block comment, /* does NOT nest */
```
`//` and `/* */` are both supported. `//` inside a string literal is fine (`"http://x"`).

---

## 4. Types and inference

### Built-in type names (as written in annotations)

| Tocin name(s) | LLVM type | Notes |
|---|---|---|
| `int`, `i64` | `i64` | The default integer. |
| `i32`, `i16`, `i8` | sized ints | Rarely needed; mixing with `int` requires care. |
| `float`, `f64` | `double` | The default float. Literal `3.0` is a float. |
| `f32`, `float32` | `float` | |
| `bool` | `i1` | `true`/`false`. |
| `string`, `str` | `ptr` (`char*`) | NUL-terminated C string. |
| `void`, `None` | `void` | Used as a unit/return-nothing type. |
| `ClassName` / `StructName` | `ptr` | Heap handle to an instance. |
| `EnumName` | `i64` | Enum members are integer constants. |
| `Option`, `Result` | `ptr` | Heap box `{ i64 tag, i64 payload }`; `None` is null. |
| `list<T>`, `array<T>` | `ptr` | Fixed array literal handle `[i64 len][elem...]`. Use for typed array **params**. |
| `dict<K,V>` | `ptr` | Limited; prefer `map` builtins. |
| `channel<T>` | `ptr` | Channel handle; `T` is parsed but messages are i64 slots. |
| `vector`, `map` (any non-collection name) | `ptr` | Opaque handle — use these as **param annotations** for `vecNew()`/`mapNew()` handles. |
| `(A, B) -> R` | function pointer | First-class function type. |

### Inference rules (exact)

- **`let x = expr;`** — `x`'s type is the LLVM type of `expr`'s evaluated value. `let x = 5;` → `int`. `let x = 3.0;` → `float`. `let s = "hi";` → `string`. `let p = Point(1,2);` → `Point` handle.
- **`let x;`** (no initializer, no type) → **error** (`cannot infer type`). Either annotate or initialize.
- **`let x: T = expr;`** — `x` has the annotated type `T`; the initializer is cast if it's a compatible int↔int or float↔float, else a type-mismatch error.
- **Function parameters** must be annotated: `def f(a: int, b: string)`. (Exception: a leading `self` needs no type.)
- **Return type**: annotate with `-> T` or `: T`. If omitted, it is **inferred from the body's `return` expressions**. `def square(n: int) { return n * n; }` infers `-> int`.
- **Mixed int/float arithmetic auto-promotes the int operand to `float`.** `5 + 3.0` → `8.0`, `10 / 4.0` → `2.5`, `2 * 3.5` → `7.0`. A `let x: float = 5;` also promotes the int literal. (Two ints stay int: `5 / 2` → `2`, truncating.)
- **Comparisons/logical ops** produce `bool`.
- **String `+` String** → a new heap string (concatenation). `int + int` → `int`. `float + float` → `float`. `ptr + int` → pointer arithmetic (rarely wanted).

### Function types and first-class functions

A function type is written `(ParamTypes) -> ReturnType`, e.g. `(int) -> int`, `(int, int) -> int`, `() -> int`. A bare top-level function name used as a value **is** a function pointer.

```tocin
def inc(x: int) -> int { return x + 1; }
def apply(f: (int) -> int, x: int) -> int { return f(x); }      // function-typed parameter
def main() -> int {
    return apply(inc, 41);                                        // pass a function -> 42
}
```

> **Critical:** a local variable that *holds* a function and is later *called* must have an explicit function-type annotation, because the compiler recovers the callee signature from the declared type. `let f = picker();` then `f(6)` **fails** with "Called value is not a function". Write `let f: (int) -> int = picker();`. A function-typed **parameter** already carries its type, so no extra annotation is needed there.

---

## 5. Builtins and standard library

### 5.1 Compiler builtins (always available, no import)

These are recognized by name in codegen. Signatures and **return types** are exact. Most collection/map/string/channel payloads are 64-bit slots (`int`-compatible).

**I/O / formatting**
| Builtin | Signature | Behavior |
|---|---|---|
| `print(...)` | `print(fmt: string, args...)` or `print(a, b, ...)` | Print without trailing newline. Forwards to C `printf`. |
| `println(...)` | same | Print **with** trailing newline. |

Two calling styles:
- **Format style** — first arg is a string **literal containing `{}`**: each `{}` is replaced by the next argument. `println("x={}, y={}", x, y)`. Escapes `\n \t \r` work inside the literal; `%` is auto-escaped. Conversions are chosen by type: int→`%lld`, float/double→`%g`, pointer/string→`%s`.
- **Sequential style** — no `{}` in the first arg (or first arg isn't a literal): every argument is printed in order with no separators. `println(s); println(42);`.

> A `{}` is only treated as a placeholder when the **first** argument is a string **literal** that contains `{}`. `println(myStringVar)` prints the string as-is.

**Arrays (fixed literals)**
| Builtin | Signature | Returns |
|---|---|---|
| `len(arr)` | `len(array) -> int` | Length of an **array literal** (reads the i64 header). **Arrays only** — see GOTCHAS; for string length use `strLen`. |

**Math** (these **shadow** any same-named `extern`; the builtin wins)
| Builtin | Signature | Returns | Notes |
|---|---|---|---|
| `sqrt sin cos tan asin acos atan exp log log2 log10 floor ceil round fabs` | `(float)->float` | `float` | Unary libm. **Int args are auto-converted to float**, so `sqrt(16)` works and yields `4` (a float). |
| `pow(a, b)` | `(num, num)->float` | `float` | Int args auto-convert. `pow(2,10)`→`1024` (float). |
| `abs(x)` | `(int)->int` or `(float)->float` | same kind as `x` | Branch-free select. |
| `min(a, b)` / `max(a, b)` | two same-kind args | same kind | int or float. |

**Dynamic vector** (heap, growable; element/return slots are `i64`). Pass the handle around with a `vector` (or any non-collection name) param annotation.
| Builtin | Signature | Returns |
|---|---|---|
| `vecNew()` | `() -> vector` | new empty vector handle |
| `vecPush(v, x)` | `(vector, int) -> int` | pushes `x`; returns 0 |
| `vecGet(v, i)` | `(vector, int) -> int` | element at `i` |
| `vecSet(v, i, x)` | `(vector, int, int) -> int` | sets element; returns 0 |
| `vecLen(v)` | `(vector) -> int` | length |
| `vecPop(v)` | `(vector) -> int` | removes & returns last |
| `vecFree(v)` | `(vector) -> int` | frees the vector; returns 0 |

**Hash map** (heap; int-keyed and string-keyed variants; value slots are `i64`).
| Builtin | Signature | Returns |
|---|---|---|
| `mapNew()` | `() -> map` | new empty map |
| `mapPut(m, k, v)` | `(map, int, int) -> int` | int key → int value; returns 0 |
| `mapGet(m, k)` | `(map, int) -> int` | value (0 if absent) |
| `mapHas(m, k)` | `(map, int) -> int` | 1 if present else 0 |
| `mapPutStr(m, k, v)` | `(map, string, int) -> int` | string key → int value; returns 0 |
| `mapGetStr(m, k)` | `(map, string) -> int` | value (0 if absent) |
| `mapHasStr(m, k)` | `(map, string) -> int` | 1/0 |
| `mapLen(m)` | `(map) -> int` | entry count |
| `mapFree(m)` | `(map) -> int` | frees; returns 0 |

**Strings** (immutable `char*`; indices/codepoints are `i64`)
| Builtin | Signature | Returns |
|---|---|---|
| `strLen(s)` | `(string) -> int` | length in bytes (**use this, not `len`, for strings**) |
| `charAt(s, i)` | `(string, int) -> int` | byte/char code at `i` (e.g. `charAt("hello",2)` = `108` = 'l') |
| `substring(s, start, length)` | `(string, int, int) -> string` | substring of `length` chars from `start` |
| `strEq(a, b)` | `(string, string) -> int` | 1 if contents equal else 0 |
| `strCmp(a, b)` | `(string, string) -> int` | <0 / 0 / >0 (like C `strcmp`) |
| `indexOfChar(s, c)` | `(string, int) -> int` | first index of char code `c`, or -1 |
| `intToStr(n)` | `(int) -> string` | decimal string |
| `strToInt(s)` | `(string) -> int` | parse decimal |
| `charToStr(c)` | `(int) -> string` | 1-char string from a char code |

**File I/O** (paths are strings; content is a string)
| Builtin | Signature | Returns |
|---|---|---|
| `readFile(path)` | `(string) -> string` | whole file contents |
| `writeFile(path, content)` | `(string, string) -> int` | bytes written |
| `appendFile(path, content)` | `(string, string) -> int` | bytes appended |
| `readLine()` | `() -> string` | one line from stdin |

**Option / Result / channels**
| Builtin | Signature | Returns |
|---|---|---|
| `Some(x)` / `Ok(x)` | `(int-slot) -> Option/Result` | boxes a value with tag 1 |
| `Err(e)` | `(int-slot) -> Result` | boxes a value with tag 0 |
| `None` | — | the null pointer (empty Option) |
| `channel<T>()` | `() -> channel` | new channel handle |

### 5.2 Standard library modules (import to use)

These are `.to` files under `stdlib/std/`, resolved by `import std.<name>;`. Import search order: the importing file's directory, then `$TOCIN_PATH`, then the compiled-in stdlib path (`/home/user/TocinLang/stdlib`). They operate on `int` and `list<int>`.

- **`import std.math;`** — `gcd(a,b)`, `lcm(a,b)`, `factorial(n)`, `clamp(x,lo,hi)`, `isPrime(n)`. (Note: `abs/min/max/sqrt/pow` are compiler builtins, not in this module.)
- **`import std.list;`** — `listSum(xs)`, `listMax(xs)`, `listMin(xs)`, `listContains(xs,target)`, `listReverse(xs)` (mutates in place).
- **`import std.linq;`** — LINQ-style over `list<int>`: reductions `reduceSum`, `reduceProduct`, `count`, `countGreater(xs,t)`, `indexOf(xs,t)`, `allGreater(xs,t)`, `anyGreater(xs,t)`, `aggregate(xs,seed,op)` (op: 0=add,1=mul,2=max,3=min); and "into a destination list" transforms `mapScaleInto(dst,src,k)`, `mapAddInto(dst,src,k)`, `filterGreaterInto(dst,src,t)` (returns count written).

```tocin
import std.linq;
def main() -> int {
    let xs = [3, 1, 4, 1, 5, 9, 2, 6];
    return reduceSum(xs);          // 31
}
```

> An `import` simply concatenates the imported file's top-level declarations into your program (imports are transitive and de-duplicated by file path). There are no namespaces — imported names are global.

---

## 6. "How to write X" — verified cookbook

Every snippet below compiles and runs. Exit codes / outputs are noted.

### A function
```tocin
def add(a: int, b: int) -> int { return a + b; }
def main() -> int { return add(20, 22); }     // exit 42
```

### Recursion
```tocin
def fact(n: int) -> int { if n <= 1 { return 1; } return n * fact(n - 1); }
def main() -> int { return fact(5); }          // exit 120
```
Mutual recursion works too (functions may call functions defined later in the file — top-level functions are pre-declared).

### A class with fields and methods
```tocin
class Point {
    x: int;
    y: int;
    def sum(self) -> int { return self.x + self.y; }
    def shift(self, d: int) { self.x = self.x + d; }   // mutates a field; void return
}
def main() -> int {
    let p = Point(3, 4);        // constructor = ClassName(fieldValues in declaration order)
    p.shift(10);                // p.x becomes 13
    p.x = 100;                  // direct field assignment also works
    return p.sum();             // exit 110
}
```
- Fields are declared as `name: Type;` (or `let name: Type;`). The **implicit constructor** takes the fields **in declaration order**.
- Methods take `self` as the first parameter. Call as `obj.method(args)` (do not pass `self`).
- `struct` is a synonym for `class`.

### A generic function
```tocin
def maxOf<T>(a: T, b: T) -> T { if a > b { return a; } return b; }
def main() -> int { return maxOf(7, 42); }     // exit 42
```
Type arguments are **inferred from the call's argument types** and the function is monomorphized per concrete type. **There is no turbofish** — `maxOf<int>(...)` is misparsed as comparisons (`<` `>`). Just call `maxOf(7, 42)`.

### A generic class
```tocin
class Box<T> {
    value: T;
    def get(self) -> T { return self.value; }
    def set(self, v: T) { self.value = v; }
}
def main() -> int {
    let bi = Box(10);     // Box<int>, inferred from the constructor argument
    bi.set(42);
    let bf = Box(2.5);    // Box<float>: a separate, independently laid-out instantiation
    return bi.get();      // exit 42
}
```
The concrete type is inferred from the constructor argument. `Box<int>` and `Box<float>` get distinct struct layouts and methods.

### An enum
```tocin
enum Color { Red, Green, Blue }       // Red=0, Green=1, Blue=2
enum Code { A = 10, B = 20 }          // explicit values; next member continues from last+1
def main() -> int {
    return Red + Color.Blue + B;       // 0 + 2 + 20 = exit 22
}
```
Members are plain `int` constants. Use them bare (`Red`) or qualified (`Color.Blue`). Negative explicit values are allowed (`X = -1`).

### Error handling (throw / try / catch / finally)
```tocin
def divide(a: int, b: int) -> int {
    if b == 0 { throw 1; }            // throw carries an int (or any value, coerced to a 64-bit slot)
    return a / b;
}
def main() -> int {
    try {
        return divide(10, 0);          // not reached
    } catch (e) {                      // e is bound to the thrown value
        return e + 41;                 // exit 42
    }
}
```
- `throw expr;` unwinds (setjmp/longjmp) to the nearest enclosing `try`. The thrown value is normalized to a 64-bit slot.
- `catch` may bind the value: `catch (e) { ... }`, `catch e { ... }`, or omit it: `catch { ... }`.
- `finally` runs on normal completion and on the caught path. A `try` with **only** `finally` (no `catch`) re-throws to the next handler after running `finally`.
- A `try` must have at least one of `catch` / `finally`.

### Option / Result + match
```tocin
def safeDiv(a: int, b: int) -> Result {
    if b == 0 { return Err(1); }
    return Ok(a / b);
}
def main() -> int {
    match safeDiv(84, 2) {
        case Ok(v): { return v; }      // binds payload -> 42
        case Err(e): { return e; }
    }
    return -1;
}
```
And `Option`:
```tocin
def find(target: int) -> Option {
    if target == 42 { return Some(99); }
    return None;
}
def main() -> int {
    match find(42) {
        case Some(x): { return x; }    // exit 99
        case None: { return 0; }
    }
    return -1;
}
```
- `Some(x)`/`Ok(v)` are tag 1; `Err(e)` is tag 0; `None` is the null pointer.
- In `match`, the patterns `Some(x)`, `Ok(v)`, `Err(e)` check the tag and **bind** the inner payload; `None` checks for null.
- Payloads are 64-bit slots (store ints). Annotate such functions as returning `Option` or `Result`.

### Null safety
```tocin
class Box { v: int; def get(self) -> int { return self.v; } }
def main() -> int {
    let present: Box = Box(42);
    let missing: Box = None;            // a class-typed handle can be None (null)
    let a = present ?: Box(0);          // present is non-null -> present
    let b = missing ?: Box(7);          // missing is null -> Box(7)
    let s1 = present?.v;                 // 42 (safe read)
    let s2 = missing?.v;                // 0  (null base -> zero of field type)
    return a.get() + b.get() + s1 + s2 - 49;   // 42+7+42+0-49 = exit 42
    // present!!.v would also be 42; missing!!.v would abort the process.
}
```

### A vector (dynamic array)
```tocin
def main() -> int {
    let v = vecNew();
    vecPush(v, 10); vecPush(v, 20); vecPush(v, 12);
    let s = 0;
    for i in 0..vecLen(v) { s = s + vecGet(v, i); }
    return s;                            // exit 42
}
```

### A string-keyed map (symbol table)
```tocin
def main() -> int {
    let m = mapNew();
    mapPutStr(m, "a", 40);
    mapPutStr(m, "b", 2);
    if mapHasStr(m, "a") == 1 {
        return mapGetStr(m, "a") + mapGetStr(m, "b");   // exit 42
    }
    return 0;
}
```

### String processing (charAt / substring / intToStr / loop building)
```tocin
def main() -> int {
    let acc = "";
    let i = 0;
    while i < 4 { acc = acc + intToStr(i); i = i + 1; }   // build "0123"
    println("acc={} len={} char1={}", acc, strLen(acc), charAt(acc, 1));  // acc=0123 len=4 char1=49
    println("sub={}", substring(acc, 1, 2));               // sub=12
    return strToInt(acc);                                  // exit 123
}
```
- A space is char code `32`; `'0'` is `48`. There are **no character literals** — compare against the numeric code from `charAt`, or build strings with `charToStr`.
- String `==` compares pointers, not contents — use `strEq` for content equality.

### First-class functions and lambdas
```tocin
def sq(x: int) -> int { return x * x; }
def apply(f: (int) -> int, x: int) -> int { return f(x); }
def main() -> int {
    let g: (int) -> int = sq;                 // store a function in a typed local
    let h = apply(lambda (x: int) -> int x * 2, 20);   // lambda body is ONE expression -> 40
    return apply(g, 5) + h;                    // 25 + 40 = exit 65
}
```
- Lambda syntax: `lambda (params) -> ReturnType <single-expression>`. The body is **one expression**, not a block. The `-> ReturnType` is recommended (defaults to a null/None type if omitted).
- **Lambdas do not capture** enclosing locals. A lambda referencing an outer variable produces invalid IR. Pass everything it needs as parameters.

### Higher-order function over a collection
```tocin
def sq(x: int) -> int { return x * x; }
def mapSum(f: (int) -> int, v: vector) -> int {     // function param + collection param
    let total = 0;
    for i in 0..vecLen(v) { total = total + f(vecGet(v, i)); }
    return total;
}
def main() -> int {
    let v = vecNew(); vecPush(v, 1); vecPush(v, 2); vecPush(v, 3);
    return mapSum(sq, v);                            // 1+4+9 = exit 14
}
```

### Goroutines + channel
```tocin
def worker(ch: channel<int>, n: int) {
    ch <- n * n;                         // send a result into the channel
}
def main() -> int {
    let ch = channel<int>();
    for i in 1..6 { go worker(ch, i); }  // 5 goroutines (real OS threads)
    let total = 0;
    for i in 0..5 { total = total + <-ch; }   // receive 5 results
    return total;                        // 1+4+9+16+25 = exit 55
}
```

### select (wait on multiple channels)
```tocin
def producer(ch: channel<int>) { ch <- 42; }
def main() -> int {
    let ch = channel<int>();
    go producer(ch);
    let got = 0;
    select {
        case v = <-ch: { got = v; }      // blocking: waits until a value arrives
    }
    return got;                          // exit 42 (deterministic)
}
```
- `select` with no `default` **blocks** until a case is ready (deterministic). With a `default`, it is **non-blocking**: the default runs when nothing is ready. Avoid relying on a goroutine having already sent before a non-blocking `select` — that is a race.

### A macro
```tocin
macro square(x) { x * x }
macro sumsq(a, b) { square!(a) + square!(b) }   // macros may invoke other macros
def main() -> int {
    return square!(2 + 3);               // ((2+3)*(2+3)) = exit 25 — args auto-parenthesized
}
```
- `macro name(params) { body }` defines a token-level macro; invoke with `name!(args)`. Each argument is auto-parenthesized and the whole expansion is parenthesized, so precedence is preserved and macros compose. Expansion is a preprocessing pass before parsing. `macro` is **not** a reserved keyword in the lexer; it is handled by the macro preprocessor.

### Reading / writing a file
```tocin
def main() -> int {
    writeFile("out.txt", "hello world");
    let s = readFile("out.txt");
    println(s);                          // hello world
    return strLen(s);                    // exit 11
}
```
`appendFile(path, content)` appends. Paths are relative to the process's working directory.

### Calling C via extern (FFI)
```tocin
extern def labs(x: int) -> int;          // libc
extern def atoi(s: string) -> int;
extern def hypot(x: float, y: float) -> float;   // libm
def main() -> int {
    println("{}", hypot(3.0, 4.0));      // 5
    return labs(-10) + atoi("32");       // exit 42
}
```
- `extern def name(params) -> T;` declares a C function with **no body**. The JIT resolves it from the running process (libc/libm are available); native builds link it normally.
- Map C types: `int`↔`long`/`int`, `float`↔`double`, `string`↔`char*`.
- **Do not** re-declare `abs`, `min`, `max`, `sqrt`, `pow`, `sin`, ... as extern and expect C semantics — the compiler's builtins **shadow** those names.

### End-to-end: word frequency (map + strings, no `break`)
```tocin
def main() -> int {
    let text = "the cat sat on the mat the cat ran";
    let counts = mapNew();
    let n = strLen(text);
    let start = 0;
    let i = 0;
    while i <= n {
        let atEnd = 0;
        if i == n { atEnd = 1; }
        let isSpace = 0;
        if atEnd == 0 { if charAt(text, i) == 32 { isSpace = 1; } }   // 32 = space
        if isSpace == 1 {
            let word = substring(text, start, i - start);
            let prev = 0;
            if mapHasStr(counts, word) == 1 { prev = mapGetStr(counts, word); }
            mapPutStr(counts, word, prev + 1);
            start = i + 1;
        }
        if atEnd == 1 {
            let word = substring(text, start, i - start);
            let prev = 0;
            if mapHasStr(counts, word) == 1 { prev = mapGetStr(counts, word); }
            mapPutStr(counts, word, prev + 1);
        }
        i = i + 1;
    }
    println("the = {}", mapGetStr(counts, "the"));   // the = 3
    println("cat = {}", mapGetStr(counts, "cat"));   // cat = 2
    return mapGetStr(counts, "the");                 // exit 3
}
```
Note how the loop avoids `break`/`continue` (unsupported) by using boolean flags.

---

## 7. Idioms and conventions

- **Every program needs `def main()`.** Annotate it `-> int` and `return` an int; that int becomes the process exit code. `return 0;` for success.
- **Run with** `./build/tocin file.to --run`; compile to a binary with `-o name` (extension picks format: `.ll` IR, `.s` asm, `.o` object, otherwise an executable). `--dump-ir` prints LLVM IR. Default optimization is `-O2`.
- **Naming:** functions/variables/fields `lowerCamelCase`; types/classes/enums/traits `UpperCamelCase`; enum members `UpperCamelCase`. (Conventional, not enforced.)
- **Booleans as ints:** stdlib and idiomatic code represent truth as `int` `0`/`1` (e.g. `mapHasStr` returns `1`/`0`, `isPrime` returns `1`/`0`). Compare explicitly: `if mapHasStr(m, k) == 1 { ... }`.
- **`{}` formatting** is the normal way to print: `println("name={} age={}", name, age)`.
- **Type the few locals that need it:** function-valued locals that get called, and locals where you want a specific width or `Option`/`Result`/class type. Otherwise prefer inference.
- **Use `vector`/`map` builtins** for dynamic, growable data; use `[..]` array literals + `len`/`[]` for fixed-size sequences (great for passing as `list<int>` params to stdlib).
- **Pass collection handles** to helpers using an opaque annotation: `def f(v: vector)`, `def g(m: map)`. Pass fixed arrays as `def h(xs: list<int>)`.
- **Concurrency:** prefer `go` + `channel<int>()` + `<-` over `async`/`await`.
- **Memory is collected automatically** (GC); `vecFree`/`mapFree`/`free` are optional, for eager release of large buffers.

---

## 8. GOTCHAS / PITFALLS (read before writing — each is verified)

1. **`break` / `continue` work in every loop** (`for i in a..b`, `for v in arr`, `while`). They affect the **innermost** enclosing loop only — there are no labeled breaks. (`switch` aliases `match`; `defer` runs LIFO at function return. Still unimplemented: `panic`, `assert`, `yield`, ownership keywords — see §3.)
2. **Use `None`, not `null`.** `null` is a reserved word the parser doesn't accept as a value (`Expected expression`). The empty value is `None` (the null pointer).
3. **`len` is for array literals only.** `len("hello")` returns garbage (it reads the first 8 bytes of the string as an i64 "length"). For strings use **`strLen`**. `len` works on `[1,2,3]` and `list<int>` params; `vecLen` works on `vector` handles; `mapLen` on `map` handles.
4. **Mixed int/float arithmetic auto-promotes the int to float.** `5 + 3.0` → `8.0`; `10 / 4.0` → `2.5`. Two ints stay int and `/` truncates (`5 / 2` → `2`). To force float division of two ints, make one a float (`x * 1.0 / y`).
5. **Collection/map/channel handle parameters need an opaque annotation.** Untyped params are a parse error (every parameter must be `name: Type`). Annotate handles as `v: vector`, `m: map`, or `ch: channel<int>`. Any non-collection type name lowers to an opaque pointer, so `vector`/`map` are just conventions that happen to work.
6. **Lambdas capture enclosing locals BY VALUE** (a snapshot at creation). Mutating the original after capture does not change the captured copy, and a closure may be returned and outlive its defining scope with independent state. Captured variables are read-only inside the lambda for the purpose of affecting the outside. Lambda bodies are a **single expression**, not a block: `lambda (x: int) -> int x + n`.
7. **Function-valued locals are usually callable without an annotation.** `let f = inc; f(6)`, `let f = lambda (x: int) -> int x+1; f(6)`, and `let f = makeAdder(10); f(7)` all work — the signature is recovered from the right-hand side (a named function, a lambda, or a function whose declared return type is itself a function type). An explicit `let f: (int) -> int = ...` is only needed when the initializer is too opaque for that inference.
8. **Collection / Option / channel / thrown payloads are 64-bit slots.** They are designed for `int`. Pointers/strings stored *as elements* are bit-cast to/from `i64`; they round-trip as raw addresses but there is no element type tracking, so this is fragile (e.g. don't expect a string read back from a `vector` to format correctly without care). Prefer storing ints; for string-keyed lookups use the dedicated `mapPutStr`/`mapGetStr`.
9. **`None` is a null pointer.** `?:`, `?.`, `!!`, and `case None:` all hinge on null. `x!!` on a null value calls `abort()` and kills the process. `?:`/`?.` only do anything meaningful when the left side is a pointer type (class/Option/string); on a non-pointer they are effectively identity.
10. **Math builtins shadow same-named externs.** Declaring `extern def sqrt/abs/min/max/pow/...` will not give you the C function — the builtin handler intercepts the call. Pick different names if you truly need the libc version.
11. **Memory is garbage-collected** (Boehm GC). Class instances, arrays, string concatenations, `vector`/`map`, and `Option`/`Result` boxes are reclaimed when unreachable — no manual free required. For very large `vector`/`map` buffers, `vecFree`/`mapFree`/`free` still give eager release. The GC is conservative (an `int` holding a heap address keeps that block alive), which is what makes the `alloc`-returns-`int` low-level model safe.
12. **`finally` runs on early return** from `try` or `catch` (it unwinds through a finally stack), as well as on normal completion. Required cleanup in `finally` is honored even when a handler does `return e;`. (Re-throw propagation also runs finally before unwinding to the enclosing handler.)
13. **Strings are NUL-terminated `char*`.** No length prefix. `strLen` walks to the NUL. Embedded NULs truncate. There are no `char` literals — use integer char codes (`charAt` returns one; `charToStr` builds a 1-char string).
14. **`==` / `!=` on strings compares contents** (the compiler detects string-typed operands and routes to a value comparison). `name == "Alice"`, `intToStr(42) == "42"`, and `("a"+"b") == "ab"` all behave as expected. Non-string pointers (class instances, `None`) still compare by identity. `strEq(a,b)` remains available and returns `1`/`0`.
15. **No turbofish for generics.** `id<int>(x)` parses as `(id < int) > (x)` (comparisons → a bool) and fails type checking. Always let type args be inferred from arguments: `id(x)`.
16. **Integer `/` truncates** toward zero; there is no separate integer-vs-float division operator — the operand types decide. `7 / 2` = `3`; `7.0 / 2.0` = `3.5`.
17. **Nested functions (`def` inside `def`) work** but are **non-capturing** — they are lifted to module scope and see only their own parameters and globals, not the enclosing function's locals. If you need to capture, use a `lambda`. Two nested functions in different outer functions must not share a name (they share the module namespace).
18. **`a..b` ranges are exclusive of `b`** and only valid in `for ... in`. `for i in 0..n` visits `0..n-1`. There is no standalone range value.
19. **Compound assignment (`+= -= *= /= %=`) and bitwise (`& | ^ << >> ~`) ARE usable** — see §operators. Only the power operator `**` and the increment/decrement `++`/`--` are not implemented; write `x = x * x` and `i = i + 1` for those. (`|` also serves as the union separator inside type annotations.)
20. **The `{ key: value }` dict literal and `dict<K,V>` type have limited support.** For key/value data, prefer the `map` builtins (`mapNew`, `mapPut`/`mapGet`, `mapPutStr`/`mapGetStr`).
21. **`async`/`await` is thin.** It parses and wraps functions but is not a substitute for real concurrency — use goroutines + channels.
22. **Non-blocking `select` (with `default`) is racy** if you expect a just-spawned goroutine to have produced a value. For determinism, use a blocking `select` (omit `default`) or pre-seed the channel.
23. **Verifying exit codes:** the exit code is `main`'s return value. If you check `$?` in a shell, **don't pipe `tocin --run` through `head`/`tail`** — you'll read the pager's exit code, not the program's.

---

## 9. Capabilities and limitations summary

**Tocin can today:**
- Compile to native code via LLVM, or JIT-run directly (`--run`).
- Functions (incl. mutual recursion, forward references, inferred return types, **nested `def`** — non-capturing), first-class function values and function-typed parameters, and **capturing closures** (single-expression lambdas that capture enclosing locals by value and can escape their scope).
- Classes/structs with fields, methods, `self`, implicit positional constructors, direct field mutation.
- Generics: generic functions and generic classes, monomorphized by **inferred** type arguments.
- Traits with `impl Trait for Type` and inherent `impl Type` methods.
- Enums with integer values (auto and explicit, incl. negatives).
- Control flow: `if`/`elif`/`else`, `while`, `for ... in a..b`, `for v in arr`, `break`/`continue` (innermost loop), `match`/`switch` (value equality + `Some/Ok/Err/None` patterns with payload binding), and `defer <stmt>` (LIFO cleanup at function return).
- Exceptions: `throw` / `try` / `catch` / `finally` (setjmp-based, integer/handle payload).
- `Option`/`Result` boxes and null-safety operators `?:` `?.` `!!`.
- Fixed array literals (`[..]`, `len`, indexing/assignment) and dynamic `vector` + `map` (int- and string-keyed) builtins.
- A useful string library (`strLen`, `charAt`, `substring`, `intToStr`/`strToInt`, `strEq`/`strCmp`, `charToStr`, `indexOfChar`) and string `+` concatenation.
- File I/O (`readFile`/`writeFile`/`appendFile`/`readLine`).
- Math builtins (libm unaries, `pow`, `abs`, `min`, `max`) plus `std.math`/`std.list`/`std.linq` modules.
- Arithmetic with auto int→float promotion, compound assignment (`+= -= *= /= %=`), bitwise/shift (`& | ^ << >> ~`), string `==`/`!=` by value, and `0x`/`0o`/`0b`/`_` integer literals.
- Concurrency: `go` goroutines (OS threads), `channel<int>()`, `<-` send/receive, blocking & non-blocking `select`.
- C FFI via `extern def`.
- Token-level macros (`macro` / `name!(...)`).
- Modules via `import` (flat concatenation; transitive; std path search).
- `print`/`println` with `{}` formatting.

**Tocin cannot (yet):**
- `panic`/`recover` / `assert` / `yield` / generators / ownership (`move`/`borrow`) — reserved but unimplemented. (`switch` aliases `match`; `defer` is implemented.)
- Capture **by reference** (capture is by value/snapshot); lambda bodies that are blocks (a lambda body is one expression); labeled `break`/`continue`.
- Capture from nested `def` (those are non-capturing — use a lambda).
- The power operator `**` and `++`/`--`.
- Turbofish / explicit generic type arguments (let them be inferred).
- A real generic-element collection type (collection payloads are i64 slots; string elements are fragile — use `mapPutStr`/`mapGetStr`).
- Capture **by reference** in closures (capture is by value). Escape analysis / stack allocation (allocations are GC-managed, not stack-promoted).
- Namespaced imports, visibility modifiers (`pub`/`priv`/...), `as`/`is`/`instanceof`/`typeof` operators.
- Robust `dict` literals (use `map` builtins).

When in doubt: keep types monomorphic and explicit at boundaries, store ints in collections, capture by value (read-only) in lambdas or pass state as parameters, use `None` not `null`, use `strLen` not `len` for strings, and `--run` to confirm. Every construct documented here has been compiled and executed successfully.

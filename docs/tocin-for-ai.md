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
                 | ("let" | "const") "(" IDENT ("," IDENT)* ")" "=" expression ";"  // tuple destructuring
funcDecl       ::= ("def" | "async" "def") IDENT typeParams? "(" params ")" retType? "{" block "}"
externDecl     ::= "extern" "def" IDENT typeParams? "(" params ")" retType? ";"     // no body
classDecl      ::= ("class" | "struct") IDENT typeParams? "{" classMember* "}"
classMember    ::= varDecl | funcDecl | IDENT ":" type ("=" expression)? ";"?       // field or method
enumDecl       ::= "enum" IDENT "{" (enumVariant ","?)* "}"
enumVariant    ::= IDENT ( "(" type ("," type)* ")"     // algebraic variant: payload field types
                         | "=" "-"? INT )?              // plain variant: explicit integer value
traitDecl      ::= "trait" IDENT typeParams? "{" methodSig* "}"                      // method bodies optional
implDecl       ::= "impl" (IDENT "for")? IDENT typeParams? "{" funcDecl* "}"
importStmt     ::= "import" (STRING | IDENT ("." IDENT)*) ";"?

typeParams     ::= "<" IDENT (":" type)? ("," IDENT (":" type)?)* ">"               // constraint parsed, ignored
params         ::= ( ("self" (":" type)?) | (IDENT ":" type "..."?) ) ("," ...)*     // every non-self param MUST have a type; trailing "..." = variadic
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
call           ::= primary ( "(" args ")" | "." IDENT | "." INT | "?." IDENT | "!!" | "[" expr "]" | "[" expr ".." expr "]" | "<-" expr )*
                 // "." INT is tuple element access: t.0, t.1
                 // "[" lo ".." hi "]" is an array slice (fresh array of [lo, hi))
primary        ::= INT | FLOAT | STRING | "true" | "false" | "None" | IDENT
                 | "channel" ("<" type ">")? "(" ")"   // new channel
                 | "(" expression ")"                  // grouping
                 | "(" expression ("," expression)+ ")" // tuple literal
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
| `const` | Immutable binding: like `let`, but reassigning it is a compile error (T013). |
| `if` / `elif` / `else` | Conditional. Condition needs no parentheses; body needs braces. |
| `while` | While loop. |
| `for ... in` | Iterate a range `a..b`, an array (`for v in arr`), or any class instance with a `next(self) -> Option` method (the iterator protocol). |
| `in` | Part of `for x in ...`. |
| `return` | Return from a function (optionally with a value). |
| `match` / `case` / `default` | Pattern match: int/float equality, `Some/Ok/Err/None`, and algebraic-enum variant patterns `Circle(r)`/`Rect(w, h)`/`Empty` with field binding. Matches on an algebraic enum must be exhaustive (cover every variant or add `default:`). |
| `class` / `struct` | Define a record type with fields + methods. `struct` and `class` are identical. |
| `enum` | Integer enum, or an algebraic data type (tagged union) when any variant carries payload fields. |
| `trait` | Declare an interface (method signatures, optional default bodies). A value typed as a trait is a **trait object** (open dynamic dispatch). |
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

> **`break`/`continue` are fully implemented** in every loop (`for i in a..b`, `for v in arr`, `while`): unlabeled they affect the innermost loop, and a loop may carry a label (`outer: for ...`) so `break outer;` / `continue outer;` target it. **Use `None`, never `null`** (`null` is reserved and rejected as a value).

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

**Time** (epoch + monotonic)
| Builtin | Signature | Returns |
|---|---|---|
| `timeSec()` / `timeMs()` | `() -> int` | wall-clock seconds / milliseconds since the epoch |
| `monoNanos()` | `() -> int` | monotonic nanoseconds (for measuring durations) |
| `sleepMs(ms)` | `(int) -> int` | sleep the current goroutine; returns 0 |

**Hashing & random** (FNV-1a / splitmix64 / xorshift64*)
| Builtin | Signature | Returns |
|---|---|---|
| `hashStr(s)` | `(string) -> int` | 64-bit FNV-1a hash (stable across runs) |
| `hashBytes(ptr, n)` | `(int, int) -> int` | FNV-1a over `n` bytes at a raw address |
| `hashInt(x)` | `(int) -> int` | splitmix64 integer mix |
| `randSeed(n)` | `(int) -> int` | seed the per-thread generator; returns 0 |
| `randInt()` | `() -> int` | next non-negative pseudo-random int |
| `randRange(lo, hi)` | `(int, int) -> int` | pseudo-random int in `[lo, hi)` |

**TCP networking** (POSIX sockets; fds are ints). Pair with `go` for a concurrent server.
| Builtin | Signature | Returns |
|---|---|---|
| `tcpListen(port)` | `(int) -> int` | listening socket fd, or -1 |
| `tcpAccept(fd)` | `(int) -> int` | accepted client fd (blocks), or -1 |
| `tcpConnect(host, port)` | `(string, int) -> int` | connected socket fd, or -1 |
| `tcpSend(fd, s)` | `(int, string) -> int` | bytes sent, or -1 |
| `tcpRecv(fd)` | `(int) -> string` | bytes read (empty on EOF/error) |
| `tcpClose(fd)` | `(int) -> int` | closes the fd; returns 0 |

**Environment / process**
| Builtin | Signature | Returns |
|---|---|---|
| `envGet(name)` | `(string) -> string` | environment variable value, or "" |
| `sysExit(code)` | `(int) -> int` | terminate the process with `code` |

**Raw memory & systems** (addresses are plain `int`s; the load/store builtins lower to inline loads/stores — no runtime calls, so they optimize like C pointer code)
| Builtin | Signature | Behavior |
|---|---|---|
| `alloc(n)` | `(int) -> int` | GC-managed heap buffer of `n` bytes; returns its address |
| `free(p)` | `(int) -> int` | release (no-op under GC); returns 0 |
| `memcpy(dst, src, n)` / `memset(dst, byte, n)` | `(int, int, int) -> int` | C semantics; return `dst` |
| `ptrAdd(p, off)` | `(int, int) -> int` | address + byte offset |
| `loadByte(p, off)` / `storeByte(p, off, v)` | 2/3 ints | 8-bit load (zero-extended) / store (truncating) |
| `loadInt(p, off)` / `storeInt(p, off, v)` | 2/3 ints | 64-bit load / store |

**Kernel / MMIO primitives** (for drivers and OS work; all widths in bits, loads zero-extend, stores truncate)
| Builtin | Signature | Behavior |
|---|---|---|
| `volatileLoad8/16/32/64(p, off)` | `(int, int) -> int` | `volatile` load — never elided, merged, or reordered vs other volatile ops. Use for device registers. |
| `volatileStore8/16/32/64(p, off, v)` | `(int, int, int) -> int` | `volatile` store; returns 0 |
| `fence()` | `() -> int` | full sequentially-consistent hardware memory barrier |
| `asm("tmpl")` | template literal | raw side-effecting assembly, no operands (`asm("cli")`, `asm("hlt")`) |
| `asm(tmpl, constraints, args...)` | literals + ints | constrained assembly (LLVM/GCC syntax, AT&T dialect): at most one leading `"=..."` output, one constraint per input, `~{...}` clobbers. Returns the output operand (or 0). Examples: `let t = asm("rdtsc", "={ax},~{dx}")`, `let s = asm("lea 100($1), $0", "=r,r", x)`, `asm("outb %b1, %w0", "{dx},{ax}", port, val)` |

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
- **`import std.strings;`** — string processing built on the char builtins: `strTrim(s)` / `strTrimLeft` / `strTrimRight`, `strRepeat(s,n)`, `strPadLeft(s,width,padChar)` / `strPadRight`, `strReverse(s)`, `strCountChar(s,ch)`, `strLastIndexOf(s,ch)`, `strIsInt(s)`, `strParseIntOr(s,fallback)` (checked parse), `strEqualsIgnoreCase(a,b)`. All return strings or `int`; char args are ASCII codes.
- **`import std.testing;`** — a Tocin-native test harness: `testBegin()`, `check(name, cond)`, `checkEq(name, got, want)`, `checkStrEq(name, got, want)`, then `return testSummary();` (prints `N passed, M failed` and returns 0 if all passed, 1 otherwise — use it as `main`'s exit code).

Beyond `std.*`, these domain modules also compile and run (import by path, e.g. `import math.stats;`). Names are globally unique (Tocin has no namespaces yet, so no two modules define the same function). Each has a test in `tests/cases/stdlib_*.to`; run them all with `tests/run_stdlib_tests.sh`.
- **`import math.basic;`** — float helpers (`signf`, `clampf`, `lerp`, `hypot`, `cbrt`, `degToRad`/`radToDeg`, `approxEq`/`approxEqTol`) + int helpers (`iabs`, `ipow`, `isqrt`, `iclamp`).
- **`import math.stats;`** — `sum`/`mean`/`minv`/`maxv`/`spread`, `variance`/`stddev` (population) and `sampleVariance`/`sampleStddev`, `median`, `dot`, `covariance` over `list<float>`.
- **`import math.geometry;`** — 2D/3D `dot`/`length`/`dist`, `cross{X,Y,Z}`, `atan2f`, `angle2`, shape area/volume, `triangleArea2`.
- **`import math.linear;`** — dense linear algebra over flat row-major `list<float>`: `matMul`, `matTranspose`, `matVecMul`, `matTrace`, `vecDot`/`vecNorm`/`vecNormalize`.
- **`import math.differential;`** — numerical calculus over `(float)->float`: `derivative`, `integrateSimpson`/`integrateTrapezoid`, `newtonRoot`/`bisectRoot`, `eulerIntegrate`.
- **`import math.stats_advanced;`** — `correlation`, `linearRegression`, `zscoreNormalize`, `minMaxScale`, `normalPdf`/`normalCdf`, `erf`.
- **`import data.algorithms;`** — in-place `sort` (quicksort) / `insertionSort`, `binarySearch`/`linearSearch`, `isSorted`, `reverse`, `argMin`/`argMax`, `intSum`/`intProduct`/`countEq` over `list<int>`.
- **`import data.structures;`** — `Stack` (`stackNew`/`stackPush`/`stackPop`/…), FIFO `Queue` (`queueNew`/`enqueue`/`dequeue`), int `Set` and `Counter` over the map builtins; handles use the `vector`/`map` types.
- **`import embedded.gpio;`** — MMIO GPIO driver over the volatile primitives: `pinMode`, `digitalRead`/`digitalWrite`, `toggle`, `readPort`/`writePort`, `barrier`. Works under `--freestanding`.
- **`import audio;`** — DSP over `list<float>`: `genSine`/`genSquare`/`genSaw`, `gain`, `mix`, `clip`, `applyEnvelope`, `rms`/`peak`, `normalize`, `lowpass`, `midiToFreq`.
- **`import game.graphics;`** — software RGBA rasterizer over a raw framebuffer: `createFramebuffer`, `setPixel`/`getChannel`, `clear`, `fillRect`/`drawRect`, `drawLine` (Bresenham), `fillCircle`.
- **`import ml.neural_network;`** — feed-forward NN over flat `list<float>`: `sigmoid`/`relu`/`tanhf` (+ derivatives), `softmax`, `denseForward`, `mseLoss`/`crossEntropy`, and a backprop `trainStep` (learns XOR in the tests).
- **`import ml.deep_learning;`** — training utilities: `argmax`, `oneHot`, `accuracy`, `meanAbsError`, `rSquared`, `heScale`/`xavierScale`, `initWeights`, `lrExpDecay`/`lrStepDecay`.
- **`import ml.computer_vision;`** — 8-bit grayscale image ops over raw buffers: `threshold`, `invert`, `brighten`, `boxBlur`, `sobel`, `histogram`, `resize`.
- **`import web.http;`** — HTTP/1.1 helpers: `httpMethod`/`httpPath`/`httpRoute`, `buildResponse`/`ok`/`okJson`/`notFound`/`statusText`, and a `serve`/`serveOnce`/`serveLoop` server over the tcp builtins.
- **`import net.advanced;`** — HTTP client: `urlHost`/`urlPort`/`urlPath`, `httpGet`/`httpPost`, `responseStatus`/`responseBody`.
- **`import web.websocket;`** — RFC 6455 frame codec over byte buffers: `writeFrame`, `frameOpcode`/`framePayloadLen`/`framePayloadOffset`, `unmaskPayload`.

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

### Trait objects (dynamic dispatch) + bounds
```tocin
trait Shape { def area(self) -> int; }
class Circle { r: int; }
impl Shape for Circle { def area(self) -> int { return self.r * self.r * 3; } }
class Rect { w: int; h: int; }
impl Shape for Rect { def area(self) -> int { return self.w * self.h; } }

def total(xs: list<Shape>) -> int {     // heterogeneous list of trait objects
    let s = 0;
    for x in xs { s = s + x.area(); }   // virtual call per element
    return s;
}
def biggest<T: Shape>(a: T) -> int { return a.area(); }  // bounded generic

def main() -> int {
    let shapes: list<Shape> = [Circle(5), Rect(2, 3)];   // boxed as trait objects
    return total(shapes) + biggest(Circle(1));            // 81 + 3 = exit 84? (75+6+3)
}
```
A value whose static type is a trait (a parameter, a `let`, or a `list<Trait>` element) is a **trait object**: `{i64 typeId, ptr data}` boxed at the boundary, dispatched at run time to the concrete `Type_method`. `def f<T: Bound>` requires the type argument to implement `Bound` (else a `T016` compile error), and `x.method()` on a bounded `T` resolves to the concrete type.

### An enum
```tocin
enum Color { Red, Green, Blue }       // Red=0, Green=1, Blue=2
enum Code { A = 10, B = 20 }          // explicit values; next member continues from last+1
def main() -> int {
    return Red + Color.Blue + B;       // 0 + 2 + 20 = exit 22
}
```
Members are plain `int` constants. Use them bare (`Red`) or qualified (`Color.Blue`). Negative explicit values are allowed (`X = -1`).

### An algebraic enum (tagged union / sum type)
```tocin
enum Expr {                 // a variant with payload fields makes the enum an ADT
    Num(int),
    Add(Expr, Expr),        // recursive: fields can be the enum's own type
    Mul(Expr, Expr)
}
def eval(e: Expr) -> int {
    match e {               // must cover every variant (or add `default:`) — else P001
        case Num(n):    { return n; }
        case Add(a, b): { return eval(a) + eval(b); }
        case Mul(a, b): { return eval(a) * eval(b); }
    }
    return 0;
}
def main() -> int {
    return eval(Mul(Add(Num(2), Num(3)), Num(4)));   // (2+3)*4 = exit 20
}
```
Construct a variant by calling it (`Num(2)`, `Add(x, y)`); nullary variants are written bare (`Empty`). `match` binds each payload field to a single identifier, denormalized to its declared type. A value is a heap `[i64 tag][slots…]` buffer. This is the canonical way to build an AST — see `examples/adt_interpreter.to`.

### Tuples and multiple return values
```tocin
def divmod(a: int, b: int) -> (int, int) {   // a tuple return type
    return (a / b, a % b);                    // a tuple literal
}
def main() -> int {
    let (q, r) = divmod(17, 5);               // destructuring: q=3, r=2
    let t = (q, r, 100);
    return t.0 * 10 + t.1;                    // positional access: 32
}
```
A tuple `(a, b, …)` is a heap buffer of 64-bit slots. Destructuring a tuple **literal** (`let (x, y) = (e0, e1)`) is lossless — each name keeps its element's native type (ints, floats, strings, class refs). Destructuring the result of a call binds each name as a 64-bit slot (`int`/reference), matching the runtime ABI. Access elements positionally with `.0`, `.1`, …. Destructuring patterns infer types (no `: T` annotation on the pattern).

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
- **Lambdas capture enclosing locals by value** — a snapshot taken when the lambda is created. The captured copy is independent (mutating the outer variable afterward does not change it), and the closure may be **returned and escape** its defining scope carrying its own state. Capture is by value only; there is no capture by reference.

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
This loop uses boolean flags for illustration, but `break`/`continue` (including labeled `break outer;`) are implemented and would work here too.

---

## 7. Idioms and conventions

- **Every program needs `def main()`.** Annotate it `-> int` and `return` an int; that int becomes the process exit code. `return 0;` for success.
- **Run with** `./build/tocin file.to --run`; compile to a binary with `-o name` (extension picks format: `.ll` IR, `.s` asm, `.o` object, otherwise an executable). `--dump-ir` prints LLVM IR. Default optimization is `-O2`; use `-O3 --native` for maximum speed on the build machine (`--native` enables host-CPU features — POPCNT/AVX — and is not portable to older CPUs).
- **Type checking is strict by default:** undeclared identifiers, wrong argument counts, clear operator/return-type violations, and assignment to undeclared variables are compile ERRORS that block codegen (with `file:line:col` locations). `--permissive` prints them but compiles anyway (not recommended; kept for migration).
- **Whole-program optimization is automatic** when producing an executable or running with `--run`: everything except `main` is internalized, so unused functions are eliminated and cross-function inlining/specialization is unrestricted. Pure computations on compile-time-known inputs may be **folded at compile time** (like C/Rust at -O3) — benchmark with runtime-derived inputs.
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

1. **`break` / `continue` work in every loop** (`for i in a..b`, `for v in arr`, `while`). Unlabeled, they affect the **innermost** loop; a loop may carry a label (`outer: for ...`) and `break outer;` / `continue outer;` target it. (`switch` aliases `match`; `defer` runs LIFO at function return. Still unimplemented: `panic`, `assert`, `yield`, ownership keywords — see §3.)
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
24. **Module-level globals work.** A top-level `let`/`const` is a real global: mutable, shared across functions, and initialized before `main` (even non-constant initializers like `let buf = alloc(1024);` or computed values). Reads/writes from any function see the same storage. (Imported top-level declarations become globals too — §5.2.)
25. **Runtime panics, not UB.** Integer division/modulo by zero, out-of-bounds `arr[i]`, and force-unwrap `!!` of `None` abort with `panic: <reason> at file:line:col` (exit 134) instead of a silent crash. The div/mod check is free when the divisor is a literal. (Suppressed under `--freestanding`.)

---

## 9. Capabilities and limitations summary

**Tocin can today:**
- Compile to native code via LLVM (default `-O2`; use `-O3 --native` for maximum speed — in a 12-kernel/8-language benchmark Tocin lands in the C/C++/Rust cluster, ahead of Go/Java/Node), or JIT-run directly (`--run`).
- **Module-level global variables** (mutable, initialized before `main`), and **runtime panics** with located messages for division-by-zero, out-of-bounds indexing, and nil force-unwrap (instead of undefined behavior).
- **Freestanding / kernel mode** (`--freestanding`): emit a relocatable object with no libc/GC/runtime for OS/kernel/bare-metal. Only arithmetic, control flow, functions, raw memory (`alloc`/`memcpy`/`memset`/`ptrAdd`/`load*`/`store*`), **volatile MMIO access (`volatileLoad8/16/32/64`, `volatileStore8/16/32/64`), memory barriers (`fence()`)**, char predicates, and **inline assembly — both `asm("cli")` and the constrained form `asm(tmpl, constraints, args...)` for port I/O, MSRs, and control registers** — are available; `print`/strings/collections/file-I/O/channels are compile errors. The object exports `main`; link it with `-nostdlib` and provide `__tocin_alloc` if you use `alloc`.
- **Opt-in borrow checker** (`--borrow-check`, OFF by default): adds Rust-like move / use-after-move enforcement on owned (class/struct) values. Binding to another variable, passing by value, or returning a value MOVES it; using a moved value is a `B001` error; reassignment revives; copy types (int/float/bool/string) are never moved. WITHOUT the flag, class instances alias freely (GC-managed) — so only enable it for code you want move-checked. Move-only for now (no `&`/`&mut` borrows or lifetimes yet).
- Functions (incl. mutual recursion, forward references, inferred return types, **nested `def`** — non-capturing), first-class function values and function-typed parameters, and **capturing closures** (single-expression lambdas that capture enclosing locals by value and can escape their scope).
- Classes/structs with fields, methods, `self`, implicit positional constructors, direct field mutation, **operator overloading** (define `__add__`/`__sub__`/`__mul__`/`__div__`/`__mod__` and `__eq__`/`__ne__`/`__lt__`/`__le__`/`__gt__`/`__ge__` as methods — binary operators on instances dispatch to them), and **RAII destructors** (`__del__(self)` runs automatically when a constructor-initialized local leaves scope, LIFO, on every return path).
- Generics: generic functions and generic classes, monomorphized by **inferred** type arguments.
- Traits with `impl Trait for Type` and inherent `impl Type` methods.
- Enums with integer values (auto and explicit, incl. negatives).
- Control flow: `if`/`elif`/`else`, `while`, `for ... in a..b`, `for v in arr`, `break`/`continue` (innermost loop, or a named outer loop via `label: for ...` + `break label;`), `match`/`switch` (value equality + `Some/Ok/Err/None` + algebraic-enum variant patterns with payload binding, checked for exhaustiveness), and `defer <stmt>` (LIFO cleanup at function return).
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
- Capture **by reference** (capture is by value/snapshot); lambda bodies that are blocks (a lambda body is one expression).
- Capture from nested `def` (those are non-capturing — use a lambda).
- The power operator `**` and `++`/`--`.
- Turbofish / explicit generic type arguments (let them be inferred).
- A real generic-element collection type (collection payloads are i64 slots; string elements are fragile — use `mapPutStr`/`mapGetStr`).
- Capture **by reference** in closures (capture is by value). Escape analysis / stack allocation (allocations are GC-managed, not stack-promoted).
- Namespaced imports, visibility modifiers (`pub`/`priv`/...), `as`/`is`/`instanceof`/`typeof` operators.
- Robust `dict` literals (use `map` builtins).

When in doubt: keep types monomorphic and explicit at boundaries, store ints in collections, capture by value (read-only) in lambdas or pass state as parameters, use `None` not `null`, use `strLen` not `len` for strings, and `--run` to confirm. Every construct documented here has been compiled and executed successfully.

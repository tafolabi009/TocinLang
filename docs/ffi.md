# Foreign Function Interface (FFI)

Tocin can call C functions directly. Python and JavaScript FFI are scaffolded
but **not yet functional** — see [Status](#status) below.

## C FFI (working)

Declare an external C function with `extern def` (a function with no body), then
call it like any Tocin function:

```tocin
extern def labs(x: int) -> int;
extern def atoi(s: string) -> int;

def main() {
    println("{}", labs(-42));     // 42
    return atoi("7");             // 7
}
```

- **JIT (`--run`)**: the symbol is resolved from the running process, which is
  linked against libc/libm, so the C standard library is available out of the
  box.
- **Native (`-o out`)**: the external symbol is linked by the C toolchain like
  any other.

### Type mapping

| Tocin            | C / LLVM            |
|------------------|---------------------|
| `int`            | 64-bit integer (`i64`) |
| `i32` `i16` `i8` | sized integers      |
| `float`          | `double`            |
| `f32`            | `float`             |
| `bool`           | `i1`                |
| `string`         | `char*`             |
| `void` (no `-> T`)| `void`             |

### Gotchas

- **Reserved builtin names shadow externs.** A set of names is intercepted by
  the compiler as builtins and will *not* reach the FFI path even if you declare
  them `extern`: `sqrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`,
  `log`, `log2`, `log10`, `floor`, `ceil`, `round`, `fabs`, `pow`, `abs`, `min`,
  `max`, `len`, `print`, `println`. To exercise the real FFI path use other
  symbols (e.g. `labs`, `atoi`, `getenv`, `putchar`, `hypot`, `toupper`).
- **`int` is 64-bit.** Declaring a C `int`-returning function (e.g. `atoi`) as
  `-> int` (i64) works on the x86-64 SysV ABI; use `-> i32` if you need an exact
  C `int`.
- **Only declared functions resolve.** Calling an undeclared C function is a
  compile error; you must `extern def` it first.

See `examples/ffi.to` and `tests/cases/ffi_*.to` for runnable examples.

## Status

| FFI         | Status                                                            |
|-------------|------------------------------------------------------------------|
| **C**       | Working (JIT + native), as documented above.                     |
| **Python**  | Experimental scaffold behind `WITH_PYTHON`; no `import`-from-Python syntax is wired into the pipeline yet. |
| **JavaScript / V8** | Disabled. V8 is not available from standard package managers, so builds use `-DWITH_V8=OFF`. |

Contributions toward real Python/JS integration are welcome; the C path is the
supported FFI today.

# Error Handling in Tocin

Tocin reports errors at three levels: **compile-time diagnostics** (strict by
default), **runtime traps** (panics with source locations), and **language
error handling** (`try`/`catch`/`finally` and `Option`/`Result`).

## Compile-time diagnostics (strict by default)

The type checker is strict: unknown identifiers, wrong argument counts
(builtins included), type mismatches, assigning to a `const`, non-exhaustive
`match`es on algebraic enums, and (under `--borrow-check`) move/borrow
violations are **hard errors**. Diagnostics are rendered rustc-style — the
offending source line, a caret underline, colors on terminals (respects
`NO_COLOR`), and a `did you mean '…'?` suggestion when a similarly-spelled
name exists — ending with an `N errors generated.` summary:

```text
app.to:3:8: error [T013]: Cannot assign to constant 'X' (declared with `const`). Use `let` for a mutable binding.
    3 |     X = 6;
      |        ^
1 error generated.
```

Common error codes: `T002` unknown identifier, `T013` assignment to `const`,
`T016` unsatisfied generic trait bound, `P001` non-exhaustive `match`,
`B001`/`B002` move / borrow violations (opt-in `--borrow-check`).

- `tocin check file.to` runs just the checker (no codegen) and exits 0 when
  clean — a fast pre-commit / CI gate.
- `--permissive` downgrades type errors to warnings and compiles anyway (not
  recommended; strict is the supported mode).

## Runtime traps (panics)

Operations that would otherwise be undefined behavior abort with a **located
panic** instead of corrupting state:

```text
panic: integer division by zero at app.to:4:16
```

- Integer division or modulo by zero.
- Out-of-bounds array/string indexing (bounds checks are on by default;
  `--freestanding` omits them for systems code).
- Force-unwrapping a null reference with `x!!`.

A panic flushes pending output and aborts the process (exit code 134).

## Exceptions: `throw` / `try` / `catch` / `finally`

```tocin
def divide(a: int, b: int) -> int {
    if b == 0 { throw 1; }        // unwinds to the nearest catch
    return a / b;
}

def main() {
    try {
        println("{}", divide(10, 0));
    } catch (e) {
        println("error code {}", e);   // error code 1
    } finally {
        println("done");               // runs on every path
    }
    return 0;
}
```

Exceptions unwind across function calls in both JIT and native builds, and
`finally` runs on every path — including early `return`. The payload is an
**integer error code** (there is no exception type hierarchy or typed
`catch`). See the full semantics in
[language-reference.md §11](language-reference.md#11-error-handling).

## Recoverable errors: `Option` / `Result`

For expected failures, prefer returning `Option`/`Result` and destructuring
with `match` — no unwinding involved:

```tocin
def safeDiv(a: int, b: int) -> Result {
    if b == 0 { return Err(1); }
    return Ok(a / b);
}

match safeDiv(100, n) {
    case Ok(v):  { println("ok: {}", v); }
    case Err(e): { println("error code {}", e); }
}
```

## Best practices

- Keep the strict checker on; fix errors rather than reaching for
  `--permissive`.
- Use `Option`/`Result` for expected failures, exceptions for exceptional
  ones, and let traps catch genuine bugs loudly.
- Note that only the **C** FFI path is functional today — treat Python/JS FFI
  errors as "feature not available", not something to handle at runtime.

See also: [language-reference.md](language-reference.md) §11 (exceptions),
§13 (Option/Result), §19 (diagnostics and traps).

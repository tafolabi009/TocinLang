# Error Handling in Tocin

Tocin provides robust error handling at compile time and runtime, with clear diagnostics and actionable messages.

## Error Types
- **Compile-time errors:** Type errors, undefined variables, trait violations, etc.
- **Runtime errors:** Null dereference, FFI failures, assertion failures, etc.

## Reporting Errors
- Errors are reported with file, line, column, and error code.
- Use `errorHandler.reportError` in custom codegen or extensions.

## Example: Compile-Time Error
```to
let x: int = "hello"; // Type error: cannot assign string to int
```

## Example: Runtime Error
```to
let s: string? = null;
let len = s!.length; // Runtime error: not-null assertion failed
```

## Best Practices
- Fix all compile-time errors before running your program.
- Use null safety and type annotations to prevent runtime errors.
- Handle FFI errors gracefully and check for backend availability.

## Troubleshooting
- **Unclear error message:** Check the error code and docs for more info.
- **FFI error:** Ensure the backend (Python, JS, C++) is available and enabled.
- **Trait error:** Ensure all required methods are implemented.

See also: [Language Basics](03_Language_Basics.md), [Advanced Topics](05_Advanced_Topics.md) 
# Null Safety in Tocin

Null safety in Tocin helps you avoid null reference errors by making nullability explicit and providing safe operators for working with optional values.

## Nullable Types
Declare a nullable type using `?`:
```to
let s: string? = null;
```

## Safe Call Operator (`?.`)
Safely access a member or method:
```to
let len = s?.length;
```
- If `s` is null, `len` is null.

## Elvis Operator (`?:`)
Provide a default value if null:
```to
let value = s ?: "default";
```

## Not-Null Assertion (`!`)
Assert that a value is not null (throws if null):
```to
let force_len = s!.length; // Throws if s is null
```

## Best Practices
- Prefer safe call and Elvis operator for optional values.
- Use not-null assertion only when you are certain the value is not null.
- Use nullable types for fields or variables that may be absent.

## Troubleshooting
- **Safe call on non-nullable:** Only use `?.` on nullable types.
- **Not-null assertion failed:** Ensure the value is not null before using `!`.
- **Type mismatch:** Assigning null to a non-nullable type is an error.

See also: [Language Basics](03_Language_Basics.md), [Advanced Topics](05_Advanced_Topics.md) 
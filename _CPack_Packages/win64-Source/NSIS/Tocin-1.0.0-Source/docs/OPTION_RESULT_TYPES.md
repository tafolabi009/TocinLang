# Option and Result Types in Tocin

This document describes the Option and Result types in Tocin, which provide safe alternatives to null references and exceptions.

## Option Type

The `Option<T>` type represents a value that might be present or absent. It has two variants:
- `Some(value)`: Contains a value of type T
- `None`: Represents the absence of a value

### Declaring Option Types

```tocin
let name: Option<string> = Some("Alice");
let empty: Option<int> = None;
```

### Using Option Values

You can use pattern matching to safely access the value inside an Option:

```tocin
match name {
    Some(value) => println("Name: " + value),
    None => println("No name provided")
}
```

### The ? Operator

The `?` operator provides a convenient way to propagate `None` values:

```tocin
fn process_name(name: Option<string>) -> Option<string> {
    // If name is None, the function returns None immediately
    // If name is Some(value), value is extracted and assigned to x
    let x = name?;
    return Some(x + " processed");
}
```

## Result Type

The `Result<T, E>` type represents an operation that might succeed with a value of type T or fail with an error of type E. It has two variants:
- `Ok(value)`: Contains a success value of type T
- `Err(error)`: Contains an error of type E

### Declaring Result Types

```tocin
let success: Result<int, string> = Ok(42);
let failure: Result<int, string> = Err("Something went wrong");
```

### Using Result Values

Like Option, you can use pattern matching to handle both success and error cases:

```tocin
match divide(10.0, 2.0) {
    Ok(value) => println("Result: " + toString(value)),
    Err(error) => println("Error: " + error)
}
```

### The ? Operator with Results

The `?` operator works with Result types to propagate errors:

```tocin
fn calculate(a: float, b: float) -> Result<float, string> {
    // If divide returns Err, the function returns that error immediately
    // If divide returns Ok(value), value is extracted and assigned to result
    let result = divide(a, b)?;
    return Ok(result * 2.0);
}
```

## Benefits of Option and Result

1. **No Null References**: Eliminates null reference exceptions by making the absence of a value explicit
2. **Type Safety**: The compiler ensures you handle both cases
3. **Explicit Error Handling**: No unexpected exceptions; errors are part of the return type
4. **Chainable Operations**: The `?` operator allows for clean and concise error propagation

## When to Use Each Type

- Use `Option<T>` when a value might be absent, but this is not an error condition
- Use `Result<T, E>` when an operation might fail, and you need to communicate why it failed

## Conversion Between Types

You can convert between Option and Result types:

```tocin
// Convert an Option to a Result
fn option_to_result<T>(opt: Option<T>, error: E) -> Result<T, E> {
    match opt {
        Some(value) => Ok(value),
        None => Err(error)
    }
}

// Convert a Result to an Option
fn result_to_option<T, E>(res: Result<T, E>) -> Option<T> {
    match res {
        Ok(value) => Some(value),
        Err(_) => None
    }
}
```

## Best Practices

1. Prefer returning Option/Result over using null or throwing exceptions
2. Use descriptive error types that explain what went wrong
3. Handle all cases explicitly in your code
4. Use the `?` operator to simplify error propagation
5. Document when functions can return None/Err and why 

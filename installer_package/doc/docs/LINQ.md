# LINQ in Tocin

LINQ (Language Integrated Query) in Tocin provides expressive, chainable operations for working with collections. Inspired by C# and functional languages, LINQ makes data transformation and querying concise and readable.

## Basic Usage
```to
let numbers = [1, 2, 3, 4, 5];
let evens = numbers.where(x => x % 2 == 0);
let squares = numbers.select(x => x * x);
```

## Supported Operations
- `where(predicate)`: Filter elements
- `select(selector)`: Transform elements
- `order_by(key_selector)`: Sort elements
- `first()`, `last()`: Get first/last element
- `count()`: Number of elements
- `any(predicate)`, `all(predicate)`: Boolean checks
- `sum()`, `avg()`, `min()`, `max()`: Aggregates

## Chaining
LINQ operations can be chained for complex queries:
```to
let result = numbers.where(x => x > 2).select(x => x * 10).order_by(x => -x);
```

## Type Inference
- The type of each operation is inferred automatically.
- `select` infers the result type from the selector function.
- Aggregates return the appropriate type (e.g., `sum()` returns the element type).

## Best Practices
- Use LINQ for readable, declarative data processing.
- Avoid deeply nested or overly complex chains—break into steps if needed.
- Use type annotations for clarity in complex queries.

## Troubleshooting
- **Type errors:** Ensure selector/predicate functions have the correct signature.
- **Operation not supported:** Check that the collection type supports LINQ (e.g., list, array).
- **Null values:** Use null safety features to avoid runtime errors.

See also: [Language Basics](03_Language_Basics.md), [Standard Library](04_Standard_Library.md) 
# Traits in Tocin

Traits are a powerful feature in Tocin, inspired by Rust and Swift protocols. They allow you to define shared behavior (interfaces) that can be implemented by structs, classes, or other types. Traits can have default method implementations and support trait bounds for generics.

## Defining a Trait
```to
trait Printable {
    fn print(self);
}
```

## Implementing a Trait
```to
struct Point { x: int, y: int }
impl Printable for Point {
    fn print(self) {
        print("Point(" + self.x + ", " + self.y + ")");
    }
}
```

## Using Traits
```to
let p = Point { x: 1, y: 2 };
p.print();
```

## Default Methods
Traits can provide default implementations:
```to
trait Describable {
    fn describe(self) -> string {
        return "[no description]";
    }
}
```

## Super Traits
A trait can require other traits:
```to
trait Drawable: Printable {
    fn draw(self);
}
```

## Trait Bounds in Generics
```to
fn print_all<T: Printable>(items: list<T>) {
    for item in items { item.print(); }
}
```

## Best Practices
- Use traits to define shared behavior, not data.
- Prefer trait bounds in generics for flexible APIs.
- Use default methods for common behavior, but allow overrides.
- Use super traits to compose complex interfaces.

## Troubleshooting
- **Missing method implementation:** Ensure all required trait methods are implemented.
- **Type does not implement trait:** Check that your struct/class has an `impl` for the trait.
- **Ambiguous method:** If multiple traits provide the same method, use fully qualified syntax.

See also: [Language Basics](03_Language_Basics.md), [Advanced Topics](05_Advanced_Topics.md) 
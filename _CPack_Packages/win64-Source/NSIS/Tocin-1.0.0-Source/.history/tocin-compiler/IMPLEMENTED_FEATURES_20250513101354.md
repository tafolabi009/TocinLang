# Tocin Language Implemented Features

This document outlines the features that have been implemented in the Tocin compiler.

## 1. Ownership and Borrowing

Rust-like ownership system that provides memory safety without garbage collection.

```
// Example of ownership and borrowing
fn process(data: String) {
    // data is owned here
    print(data); // data is moved to print
    // data is no longer valid here
}

fn borrow_example(data: &String) {
    // data is borrowed immutably - can read but not modify
    print(data.length);
}

fn mutable_borrow(data: &mut String) {
    // data is borrowed mutably - can both read and modify
    data.append("more text");
}
```

## 2. Result and Option Types

Explicit error handling and optional values, similar to Rust.

```
// Option example
fn find_user(id: int): Option<User> {
    if (database.has(id)) {
        return Some(database.get(id));
    }
    return None;
}

// Result example
fn divide(a: float, b: float): Result<float, string> {
    if (b == 0.0) {
        return Err("Division by zero");
    }
    return Ok(a / b);
}

// Pattern matching with Result
let result = divide(10.0, 2.0);
match result {
    Ok(value) => print("Result: " + value),
    Err(message) => print("Error: " + message)
}
```

## 3. Goroutines and Channels

Go-like lightweight concurrency with message passing.

```
// Start a goroutine
fn process_data(data: []int) {
    // Process data concurrently
}

go process_data(large_array);  // Non-blocking, runs in background

// Channel example
let channel = make(Chan<string>);

// Send on channel
go fn() {
    for (i in 0..5) {
        channel <- "Message " + i; // Send to channel
    }
}();

// Receive from channel
for (i in 0..5) {
    let message = <- channel; // Blocks until message received
    print(message);
}

// Select statement for multi-channel operations
select {
    case msg <- channel1:
        print("Received from channel1: " + msg);
    case channel2 <- "hello":
        print("Sent to channel2");
    default:
        print("No channel operations ready");
}
```

## 4. Deferred Execution

Go-like defer statement for cleanup operations.

```
fn read_file(path: string): Result<string, Error> {
    let file = open(path)?;
    defer file.close(); // Will be executed when function exits
    
    // Read and process file
    // If any errors occur, file.close() still gets called
    return Ok(file.read_all());
}
```

## 5. Properties

C#-like properties for controlled access to class fields.

```
class Person {
    private _age: int;
    
    // Auto-property with private setter
    public Name: string { get; private set; }
    
    // Property with custom accessors
    public Age: int {
        get { return _age; }
        set {
            if (value < 0) {
                throw "Age cannot be negative";
            }
            _age = value;
        }
    }
    
    // Read-only property with calculated value
    public IsAdult: bool {
        get { return _age >= 18; }
    }
}
```

## 6. LINQ-style Operations

C#-like query operations for collections.

```
let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Filter even numbers and multiply by 2
let result = numbers
    .where(x => x % 2 == 0)
    .select(x => x * 2);
// result: [4, 8, 12, 16, 20]

// More complex queries
let users = get_users();
let active_admin_names = users
    .where(u => u.is_active && u.role == "admin")
    .orderBy(u => u.last_login)
    .select(u => u.name);
```

## 7. Null Safety

Kotlin/Swift-like null safety features.

```
// Nullable types with ?
let name: string? = get_optional_name();

// Safe call operator
let length = name?.length; // length is int?

// Elvis operator (null coalescing)
let display_name = name ?: "Unknown"; // If name is null, use "Unknown"

// Not-null assertion
let confirmed_name = name!; // Throws if name is null
```

## 8. Extension Functions

Kotlin-like extension functions.

```
// Extension function declaration
extension fn string.capitalize(): string {
    if (this.length == 0) return this;
    return this[0].toUpper() + this.slice(1);
}

// Using the extension function
let name = "john";
let capitalized = name.capitalize(); // "John"
```

## 9. Type Classes/Traits

Rust-like traits for polymorphism and constraints.

```
// Trait definition
trait Printable {
    fn to_string(): string;
    fn print() {
        println(this.to_string()); // Default implementation
    }
}

// Implementing a trait
class Person {
    public name: string;
    public age: int;
}

impl Printable for Person {
    fn to_string(): string {
        return "Person { name: " + this.name + ", age: " + this.age + " }";
    }
    // print() uses the default implementation
}

// Generic function with trait bounds
fn print_value<T: Printable>(value: T) {
    value.print();
}

// Using dynamic dispatch
fn process(item: dyn Printable) {
    item.print();
}
```

## 10. Move Semantics

C++-like move semantics for efficient resource management.

```
// Move example
let s1 = "Hello world".to_string();
let s2 = move(s1); // s1 is now invalid, s2 owns the string

// Implementing move semantics in classes
class Buffer {
    private data: []byte;
    
    // Move constructor
    constructor(other: Buffer&&) {
        this.data = move(other.data);
    }
    
    // Move assignment operator
    fn operator=(other: Buffer&&): Buffer& {
        if (this != &other) {
            this.data = move(other.data);
        }
        return *this;
    }
}
```

## Additional Features

The integration of these features creates a powerful combination that enables:

1. **Safe Concurrency**: Through ownership, borrowing, and channels
2. **Robust Error Handling**: With Result types and pattern matching
3. **Resource Safety**: Using defer and move semantics
4. **Zero-Cost Abstractions**: Where possible, these features compile to efficient code without runtime overhead
5. **Expressive Abstractions**: Including LINQ, properties, and traits

## Implementation Notes

Most features are implemented as AST nodes and supporting classes in the compiler. The features are designed to work together cohesively, with the `FeatureManager` class providing a unified interface to access all language features.

Each feature has dedicated type checking and semantics validation to ensure the correctness of programs using these features. 

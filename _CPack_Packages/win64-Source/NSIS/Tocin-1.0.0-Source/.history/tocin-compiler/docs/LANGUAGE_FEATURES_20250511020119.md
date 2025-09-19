# Tocin Language Features

This document provides an overview of the features implemented in the Tocin programming language.

## Type System

### Basic Types

- **int**: 64-bit signed integer
- **float**: 64-bit floating point number
- **float32**: 32-bit floating point number
- **bool**: Boolean value (true/false)
- **string**: UTF-8 encoded string
- **void**: Represents the absence of a value

### Advanced Types

#### Option Type

The `Option<T>` type represents a value that might be absent:

```tocin
let name: Option<string> = Some("Alice");
let empty: Option<int> = None;
```

#### Result Type

The `Result<T, E>` type represents an operation that might fail:

```tocin
let success: Result<int, string> = Ok(42);
let failure: Result<int, string> = Err("Something went wrong");
```

#### Generics

Tocin supports generic types and functions:

```tocin
fn identity<T>(value: T) -> T {
    return value;
}

// Generic struct
struct Pair<T, U> {
    first: T;
    second: U;
}
```

#### Traits

Traits define behavior that types can implement:

```tocin
trait ToString {
    fn toString() -> string;
}

impl ToString for int {
    fn toString() -> string {
        // Implementation
    }
}
```

## Pattern Matching

Tocin supports comprehensive pattern matching:

```tocin
match value {
    Some(x) => println("Got value: " + x),
    None => println("No value")
}

match result {
    Ok(value) => println("Success: " + value.toString()),
    Err(error) => println("Error: " + error)
}

// Destructuring
match point {
    Point { x: 0, y } => println("On y-axis at " + y.toString()),
    Point { x, y: 0 } => println("On x-axis at " + x.toString()),
    Point { x, y } => println("Point at " + x.toString() + ", " + y.toString())
}
```

## Concurrency

### Goroutines

Lightweight threads for concurrent execution:

```tocin
fn computeValue() -> int {
    // Some computation
    return 42;
}

// Start a goroutine
go computeValue();
```

### Channels

For communication between goroutines:

```tocin
// Create a channel
let messages = Channel<string>();

// Send a value
messages.send("Hello");

// Receive a value
let msg = messages.receive();
```

### Select

Wait on multiple channel operations:

```tocin
select {
    case v = <-channel1:
        println("Received from channel1: " + v);
    case channel2.send(value):
        println("Sent to channel2");
    case <-timeout(500):
        println("Timeout");
    default:
        println("No channel ready");
}
```

## Error Handling

Tocin uses the Result type for explicit error handling without exceptions:

```tocin
fn divide(a: float, b: float) -> Result<float, string> {
    if b == 0.0 {
        return Err("Division by zero");
    }
    return Ok(a / b);
}

// Using the ? operator for error propagation
fn calculate(a: float, b: float) -> Result<float, string> {
    let result = divide(a, b)?;  // Returns Err if divide returns Err
    return Ok(result * 2);
}
```

## Null Safety

Tocin avoids null reference errors using the Option type:

```tocin
// No null references allowed
let name: string = "Alice";  // Valid
let name: string = null;     // Compile error!

// Instead, use Option
let optionalName: Option<string> = Some("Alice");
let noName: Option<string> = None;

// Safe access using pattern matching
match optionalName {
    Some(name) => println("Hello, " + name),
    None => println("Hello, anonymous")
}

// Safe access using the ? operator
fn greet(optionalName: Option<string>) -> Option<string> {
    let name = optionalName?;  // Returns None if optionalName is None
    return Some("Hello, " + name);
}
```

## Deferred Execution

The `defer` statement ensures that a function call is performed when the current function returns:

```tocin
fn processFile(path: string) -> Result<string, string> {
    let file = open(path)?;
    defer file.close();  // Will be called when function returns

    // Process the file...
    return Ok(file.readToString());
}
```

## Properties

Syntactic sugar for getters and setters:

```tocin
class Person {
    private _name: string;
    
    // Property with getter and setter
    property name: string {
        get {
            return _name;
        }
        set(value) {
            _name = value;
        }
    }
    
    // Read-only property
    property initials: string {
        get {
            return _name.split(" ")
                .map(word => word[0])
                .join("");
        }
    }
}
```

## Extension Functions

Add methods to existing types without inheritance:

```tocin
// Add a method to the string type
extension string {
    fn capitalize() -> string {
        if self.length() == 0 {
            return self;
        }
        return self[0].toUpperCase() + self.substring(1);
    }
}

// Now you can use it
let name = "alice";
println(name.capitalize());  // Prints "Alice"
```

## Move Semantics

Efficient transfer of resources:

```tocin
// Resource type with move semantics
struct Buffer {
    data: Array<byte>;
    
    // Move constructor
    fn move(self) -> Buffer {
        return Buffer { data: self.data.move() };
    }
    
    // Access after move is a compile error
    fn use() {
        let buf1 = Buffer { data: [1, 2, 3] };
        let buf2 = buf1.move();
        
        buf1.data[0] = 5;  // Compile error: buf1 was moved!
    }
}
```

## Tooling and Library Support

### Standard Library

- IO Operations
- Collections (List, Dictionary, Set)
- Math Functions
- String Manipulation
- Network Operations
- File System Access

### Foreign Function Interface (FFI)

```tocin
// Call C functions
extern "C" {
    fn printf(format: string, ...): int;
}

// Call JavaScript (through V8)
extern "js" {
    fn alert(message: string): void;
}

// Call Python (through Python C API)
extern "python" {
    fn numpy_array(data: Array<float>): PyObject;
}
```

## Example Programs

See the `examples/` directory for demonstration programs showing these features in action:

- `option_result_demo.tc`: Shows Option and Result types
- `traits_demo.tc`: Demonstrates the trait system
- `concurrency_demo.tc`: Shows goroutines and channels
- `pattern_matching_demo.tc`: Shows pattern matching capabilities 

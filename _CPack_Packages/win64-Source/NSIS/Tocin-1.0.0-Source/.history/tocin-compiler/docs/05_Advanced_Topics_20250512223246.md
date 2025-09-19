# Advanced Topics in Tocin

This guide delves into the advanced features and techniques in the Tocin programming language. These topics build upon the fundamentals covered in the Language Basics and are designed for developers looking to leverage the full power of Tocin.

## Table of Contents

1. [Generics](#generics)
2. [Advanced Type System](#advanced-type-system)
3. [Metaprogramming](#metaprogramming)
4. [Memory Management](#memory-management)
5. [Concurrency Patterns](#concurrency-patterns)
6. [Advanced Error Handling](#advanced-error-handling)
7. [Interoperability](#interoperability)
8. [Performance Optimization](#performance-optimization)
9. [Testing and Debugging](#testing-and-debugging)
10. [Design Patterns in Tocin](#design-patterns-in-tocin)

## Generics

Generics in Tocin allow you to write flexible, reusable code that works with different types while maintaining type safety.

### Generic Functions

```tocin
// A generic function that works with any type
def identity<T>(value: T) -> T {
    return value;
}

// A function with multiple type parameters
def pair<T, U>(first: T, second: U) -> (T, U) {
    return (first, second);
}

// Using generic functions
let intValue = identity<int>(5);
let stringValue = identity<string>("hello");
let mixedPair = pair<int, string>(10, "ten");
```

### Generic Types

```tocin
// A generic class
class Box<T> {
    property value: T;
    
    def initialize(value: T) {
        self.value = value;
    }
    
    def getValue() -> T {
        return self.value;
    }
    
    def setValue(newValue: T) -> void {
        self.value = newValue;
    }
}

// Using generic types
let intBox = new Box<int>(42);
let stringBox = new Box<string>("Hello");
```

### Bounded Type Parameters

```tocin
// A type parameter bounded by an interface
def largest<T: Comparable>(a: T, b: T) -> T {
    if (a.greaterThan(b)) {
        return a;
    } else {
        return b;
    }
}

// Multiple bounds using intersection types
def processValue<T: Serializable & Cloneable>(value: T) -> T {
    // Process value
    return value.clone();
}
```

### Variance

```tocin
// Covariance - indicated with out
class Producer<out T> {
    def produce() -> T {
        // Implementation
    }
}

// Contravariance - indicated with in
class Consumer<in T> {
    def consume(value: T) -> void {
        // Implementation
    }
}

// Invariance - no modifier
class Container<T> {
    property value: T;
    
    def getValue() -> T {
        return self.value;
    }
    
    def setValue(newValue: T) -> void {
        self.value = newValue;
    }
}
```

## Advanced Type System

Tocin's type system includes several advanced features for precise type control.

### Union Types

```tocin
// A function that accepts either a string or an int
def processValue(value: string | int) -> void {
    if (value is string) {
        println("String value: " + value);
    } else {
        println("Integer value: " + value.toString());
    }
}

// Using union types
processValue("hello");  // Works with string
processValue(42);       // Works with int
```

### Intersection Types

```tocin
// Defining interfaces
interface Loggable {
    def log() -> void;
}

interface Serializable {
    def serialize() -> string;
}

// Function requiring an object that implements both interfaces
def process(obj: Loggable & Serializable) -> void {
    obj.log();
    let serialized = obj.serialize();
    // Process serialized data
}
```

### Type Aliases and Type Functions

```tocin
// Simple type alias
type UserID = int;

// Generic type alias
type Result<T> = { value: T, error: string? };

// Type function
type MapType<K, V> = Map<K, V>;
type StringMap<V> = MapType<string, V>;

// Using type aliases
let userId: UserID = 12345;
let result: Result<int> = { value: 42, error: null };
let nameMap: StringMap<int> = new Map<string, int>();
```

### Conditional Types

```tocin
// A conditional type
type NonNullable<T> = T extends null ? never : T;

// Using conditional types
type StringOrNull = string | null;
type NonNullString = NonNullable<StringOrNull>;  // Resolves to string
```

### Literal Types

```tocin
// String literal types
type Direction = "north" | "south" | "east" | "west";
let heading: Direction = "north";  // Valid
// let invalid: Direction = "up";  // Error: not a valid Direction

// Numeric literal types
type Dice = 1 | 2 | 3 | 4 | 5 | 6;
let roll: Dice = 4;  // Valid
// let invalid: Dice = 7;  // Error: not a valid Dice value
```

### Type Narrowing

```tocin
// Type narrowing with type guards
def isString(value: any): value is string {
    return typeof(value) == "string";
}

def processValue(value: any) -> void {
    if (isString(value)) {
        // Type narrowed to string
        println("Length: " + value.length.toString());
    } else {
        println("Not a string");
    }
}

// Type narrowing with pattern matching
def describe(value: any) -> string {
    match (value) {
        case s is string => return "String: " + s,
        case n is int if n > 0 => return "Positive number: " + n.toString(),
        case n is int if n < 0 => return "Negative number: " + n.toString(),
        case 0 => return "Zero",
        case true => return "True",
        case false => return "False",
        case null => return "Null",
        case _ => return "Unknown type"
    }
}
```

## Metaprogramming

Tocin provides metaprogramming capabilities for generating and manipulating code at compile time.

### Compile-Time Reflection

```tocin
// Getting type information at compile time
def getTypeName<T>() -> string {
    return TypeInfo<T>.name;
}

// Using compile-time reflection
println(getTypeName<int>());  // Outputs: int
println(getTypeName<string>());  // Outputs: string
```

### Macros

```tocin
// Define a macro for logging
macro log(message) {
    println("[" + __FILE__ + ":" + __LINE__ + "] " + message);
}

// Using the macro
log("An error occurred");
// Expands to: println("[file.to:10] An error occurred");
```

### Code Generation

```tocin
// A code generation annotation
@GenerateToString
class Person {
    property name: string;
    property age: int;
    
    def initialize(name: string, age: int) {
        self.name = name;
        self.age = age;
    }
    
    // The @GenerateToString annotation will generate this method:
    // def toString() -> string {
    //     return "Person{name=" + self.name + ", age=" + self.age.toString() + "}";
    // }
}
```

## Memory Management

Tocin offers several approaches to memory management, from automatic garbage collection to manual memory control.

### Ownership System

```tocin
// Function that takes ownership of a value
def consumeString(s: owned string) -> void {
    // s is owned by this function
    println(s);
    // s is automatically freed when the function returns
}

// Function that borrows a value
def borrowString(s: borrowed string) -> void {
    // s is borrowed, not owned
    println(s);
    // s is not freed when the function returns
}

// Using ownership
let name = "Alice";
borrowString(name);  // Borrows name, name is still valid
consumeString(name);  // Consumes name, name is no longer valid
// println(name);  // Error: name has been moved
```

### Reference Counting

```tocin
// Creating a reference-counted object
let rc = new Rc<string>("Hello");

// Sharing ownership
let rc2 = rc;  // Reference count increases

// Modifying the shared value
rc.value = "Hello, World!";
println(rc2.value);  // Outputs: Hello, World!

// Checking the reference count
println(rc.refCount());  // Outputs: 2

// When rc and rc2 go out of scope, the reference count decreases
// and the object is freed when it reaches zero
```

### Weak References

```tocin
// Creating a reference-counted object
let rc = new Rc<string>("Hello");

// Creating a weak reference
let weak = rc.asWeak();

// Using the weak reference
if (let strongRef = weak.upgrade()) {
    println(strongRef.value);  // Outputs: Hello
} else {
    println("Object has been deallocated");
}

// If all strong references are dropped, weak references can't be upgraded
rc = null;
if (let strongRef = weak.upgrade()) {
    println(strongRef.value);
} else {
    println("Object has been deallocated");  // This will be printed
}
```

### Manual Memory Management

```tocin
// Manually allocating memory
let buffer = Memory.allocate<uint8>(1024);  // Allocate 1024 bytes

// Using the allocated memory
buffer[0] = 65;  // ASCII 'A'
buffer[1] = 66;  // ASCII 'B'
buffer[2] = 67;  // ASCII 'C'

// Printing the values
println(String.fromCharCodes(buffer, 0, 3));  // Outputs: ABC

// Manually freeing memory
Memory.free(buffer);
```

## Concurrency Patterns

Tocin provides powerful concurrency primitives and patterns.

### Asynchronous Programming

```tocin
// Defining an async function
async def fetchData(url: string) -> string {
    // Simulate network request
    await delay(1000);
    return "Data from " + url;
}

// Using async/await
async def processData() -> void {
    let data = await fetchData("https://example.com/api");
    println("Received: " + data);
}

// Running an async function
coroutine.spawn(processData);

// Waiting for multiple async operations
async def fetchMultiple() -> Array<string> {
    let urls = ["https://example.com/api/1", "https://example.com/api/2"];
    let promises = urls.map(url => fetchData(url));
    return await Promise.all(promises);
}
```

### Actor Model

```tocin
// Define an actor
actor Counter {
    private var count: int = 0;
    
    def increment() -> void {
        count += 1;
    }
    
    def decrement() -> void {
        count -= 1;
    }
    
    def getCount() -> int {
        return count;
    }
}

// Using actors
let counter = new Counter();

// Spawn multiple coroutines that update the counter
for (let i = 0; i < 10; i++) {
    coroutine.spawn(() => {
        for (let j = 0; j < 100; j++) {
            counter.increment();
        }
    });
}

// Wait for all coroutines to finish
time.sleep(1000);

// Get the final count
let finalCount = await counter.getCount();
println("Final count: " + finalCount.toString());
```

### Mutex and Locks

```tocin
// Create a mutex
let mutex = new Mutex();
let sharedResource = 0;

// Function that safely updates the shared resource
def updateResource(value: int) -> void {
    mutex.lock();
    try {
        sharedResource += value;
    } finally {
        mutex.unlock();
    }
}

// Using async locks
async def asyncUpdate(value: int) -> void {
    using await mutex.asyncLock() {
        sharedResource += value;
    }
}
```

### Thread Pools

```tocin
// Create a thread pool
let pool = new ThreadPool(4);  // 4 worker threads

// Submit tasks to the pool
for (let i = 0; i < 10; i++) {
    let taskId = i;
    pool.submit(() => {
        println("Task " + taskId.toString() + " running on thread " + Thread.currentId().toString());
        time.sleep(100);
        return "Task " + taskId.toString() + " result";
    });
}

// Wait for all tasks to complete
pool.waitForAll();

// Shutdown the pool
pool.shutdown();
```

## Advanced Error Handling

Tocin provides sophisticated error handling mechanisms beyond basic try-catch.

### Result Monads

```tocin
// Function that returns a Result
def divide(a: int, b: int) -> Result<float, string> {
    if (b == 0) {
        return Result.ERROR("Division by zero");
    }
    return Result.OK(a / b);
}

// Chaining Result operations
let result = divide(10, 2)
    .map(value => value * 2)  // Transform the OK value
    .mapError(msg => "Error: " + msg);  // Transform the ERROR value

// Pattern matching on Result
match (result) {
    Result.OK(value) => println("Result: " + value.toString()),
    Result.ERROR(message) => println(message)
}

// Using flatMap for error propagation
def computeFormula(a: int, b: int, c: int) -> Result<float, string> {
    return divide(a, b).flatMap(quotient => {
        return divide(quotient, c);
    });
}
```

### Option Monads

```tocin
// Function that returns an Option
def findUser(id: int) -> Option<User> {
    let user = database.getUserById(id);
    if (user != null) {
        return Option.SOME(user);
    } else {
        return Option.NONE();
    }
}

// Chaining Option operations
let result = findUser(123)
    .map(user => user.name)  // Transform the SOME value
    .filter(name => name.length > 0)  // Filter based on a condition
    .getOrElse("Unknown user");  // Provide a default value

// Pattern matching on Option
match (findUser(123)) {
    Option.SOME(user) => println("Found user: " + user.name),
    Option.NONE => println("User not found")
}
```

### Custom Error Types

```tocin
// Define custom error types
enum AppError {
    NETWORK_ERROR(string),
    DATABASE_ERROR(string),
    VALIDATION_ERROR(Array<string>),
    UNKNOWN_ERROR
}

// Function that returns a custom error
def fetchUserData(id: int) -> Result<User, AppError> {
    if (!internet.isConnected()) {
        return Result.ERROR(AppError.NETWORK_ERROR("No internet connection"));
    }
    
    // Try to fetch user data
    try {
        let user = api.getUser(id);
        return Result.OK(user);
    } catch (e: DatabaseException) {
        return Result.ERROR(AppError.DATABASE_ERROR(e.message));
    } catch (e) {
        return Result.ERROR(AppError.UNKNOWN_ERROR);
    }
}

// Handling custom errors
let result = fetchUserData(123);
match (result) {
    Result.OK(user) => displayUser(user),
    Result.ERROR(AppError.NETWORK_ERROR(msg)) => showNetworkError(msg),
    Result.ERROR(AppError.DATABASE_ERROR(msg)) => showDatabaseError(msg),
    Result.ERROR(AppError.VALIDATION_ERROR(errors)) => showValidationErrors(errors),
    Result.ERROR(AppError.UNKNOWN_ERROR) => showGenericError()
}
```

## Interoperability

Tocin provides mechanisms for interoperating with code written in other languages.

### FFI (Foreign Function Interface)

```tocin
// Import a C function
@Foreign("C")
extern def printf(format: string, ...args: any[]) -> int;

// Use the imported C function
printf("Hello, %s!\n", "World");
```

### Binding to Dynamic Libraries

```tocin
// Load a dynamic library
let lib = DynamicLibrary.load("libexample.so");

// Get a function from the library
let funcPtr = lib.getFunction<(int, int) -> int>("add");

// Call the function
let result = funcPtr(5, 3);  // result = 8
```

### Exporting Tocin Functions

```tocin
// Export a Tocin function for use in other languages
@Export("add")
def add(a: int, b: int) -> int {
    return a + b;
}
```

### WebAssembly Integration

```tocin
// Compile to WebAssembly
@WasmExport
def fibonacci(n: int) -> int {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

// Import JavaScript functions
@WasmImport("console", "log")
extern def consoleLog(message: string) -> void;
```

## Performance Optimization

Tocin provides various ways to optimize performance.

### Compiler Optimizations

```tocin
// Suggest inlining for a small function
@Inline
def square(x: int) -> int {
    return x * x;
}

// Prevent inlining for a function
@NoInline
def complexFunction(x: int) -> int {
    // Complex implementation
}

// Optimize a hot loop
@Optimize("speed")
def hotLoop() -> int {
    let sum = 0;
    for (let i = 0; i < 1000000; i++) {
        sum += i;
    }
    return sum;
}
```

### SIMD Operations

```tocin
// Using SIMD (Single Instruction, Multiple Data) operations
import math.simd;

// Create SIMD vectors
let v1 = new Float32x4(1.0, 2.0, 3.0, 4.0);
let v2 = new Float32x4(5.0, 6.0, 7.0, 8.0);

// Perform SIMD operations
let sum = v1 + v2;  // Add four pairs of floats in parallel
let product = v1 * v2;  // Multiply four pairs of floats in parallel

// Extract individual lanes
println(sum.x);  // 6.0
println(product.y);  // 12.0
```

### Memory Optimization

```tocin
// Use a memory pool for frequent allocations
let pool = new MemoryPool<int>(1000);  // Pool with capacity for 1000 integers

// Allocate from the pool
let value1 = pool.allocate();
value1 = 42;

let value2 = pool.allocate();
value2 = 100;

// Release back to the pool
pool.release(value1);
pool.release(value2);

// Optimizing struct layout
@Packed
struct PackedData {
    a: uint8;
    b: uint8;
    c: uint16;
    d: uint32;
}

// Specifying alignment
@Align(16)
struct AlignedData {
    values: Array<float>;
}
```

## Testing and Debugging

Tocin provides comprehensive tools for testing and debugging.

### Unit Testing

```tocin
// Define a test
@Test
def testAddition() -> void {
    assert(1 + 1 == 2, "Addition failed");
    
    // Using the assert module
    Assert.equals(1 + 1, 2);
    Assert.notEquals(1 + 1, 3);
    Assert.isTrue(1 < 2);
    Assert.isFalse(1 > 2);
}

// Test fixture with setup and teardown
@TestFixture
class MathTests {
    property calculator: Calculator;
    
    @Setup
    def setup() -> void {
        calculator = new Calculator();
    }
    
    @Test
    def testAddition() -> void {
        Assert.equals(calculator.add(2, 3), 5);
    }
    
    @Test
    def testSubtraction() -> void {
        Assert.equals(calculator.subtract(5, 3), 2);
    }
    
    @Teardown
    def teardown() -> void {
        calculator = null;
    }
}
```

### Property-Based Testing

```tocin
import testing.property;

// Define a property test
@PropertyTest
def testReverseTwiceIsIdentity(input: string) -> void {
    let reversed = input.reversed();
    let reversedTwice = reversed.reversed();
    Assert.equals(input, reversedTwice);
}

// Run the property test with generated inputs
PropertyTest.run(testReverseTwiceIsIdentity, {
    iterations: 100,
    maxSize: 1000
});
```

### Debugging Tools

```tocin
// Debug logging
Debug.log("Current value: " + value.toString());

// Conditional debug logging
Debug.logIf(value < 0, "Value is negative: " + value.toString());

// Adding breakpoints in code
Debug.breakpoint();

// Measuring execution time
let timer = Debug.startTimer();
// Code to measure
let elapsed = Debug.endTimer(timer);
println("Execution time: " + elapsed.toString() + "ms");

// Memory profiling
Debug.startMemoryProfile();
// Code to profile
let memoryUsage = Debug.endMemoryProfile();
println("Memory used: " + memoryUsage.toString() + "KB");
```

## Design Patterns in Tocin

Tocin's features make it suitable for implementing various design patterns.

### Singleton Pattern

```tocin
class Singleton {
    private static instance: Singleton? = null;
    
    private def initialize() {
        // Private constructor
    }
    
    def static getInstance() -> Singleton {
        if (instance == null) {
            instance = new Singleton();
        }
        return instance;
    }
    
    def doSomething() -> void {
        println("Singleton is doing something");
    }
}

// Using the singleton
let instance = Singleton.getInstance();
instance.doSomething();
```

### Observer Pattern

```tocin
interface Observer<T> {
    def update(data: T) -> void;
}

class Subject<T> {
    private property observers: Array<Observer<T>> = [];
    
    def addObserver(observer: Observer<T>) -> void {
        observers.push(observer);
    }
    
    def removeObserver(observer: Observer<T>) -> void {
        let index = observers.indexOf(observer);
        if (index >= 0) {
            observers.splice(index, 1);
        }
    }
    
    def notifyObservers(data: T) -> void {
        for (let observer of observers) {
            observer.update(data);
        }
    }
}

// Example implementation
class WeatherStation extends Subject<{temperature: float, humidity: float}> {
    private property temperature: float = 0;
    private property humidity: float = 0;
    
    def setMeasurements(temperature: float, humidity: float) -> void {
        self.temperature = temperature;
        self.humidity = humidity;
        notifyObservers({temperature, humidity});
    }
}

class Display implements Observer<{temperature: float, humidity: float}> {
    def update(data: {temperature: float, humidity: float}) -> void {
        println("Temperature: " + data.temperature.toString() + "°C");
        println("Humidity: " + data.humidity.toString() + "%");
    }
}

// Using the pattern
let weatherStation = new WeatherStation();
let display = new Display();

weatherStation.addObserver(display);
weatherStation.setMeasurements(25.2, 60.0);
```

### Builder Pattern

```tocin
class Person {
    property name: string;
    property age: int;
    property address: string;
    property email: string;
    property phone: string;
    
    def initialize() {
        // Default values
        self.name = "";
        self.age = 0;
        self.address = "";
        self.email = "";
        self.phone = "";
    }
}

class PersonBuilder {
    private property person: Person;
    
    def initialize() {
        self.person = new Person();
    }
    
    def withName(name: string) -> PersonBuilder {
        self.person.name = name;
        return self;
    }
    
    def withAge(age: int) -> PersonBuilder {
        self.person.age = age;
        return self;
    }
    
    def withAddress(address: string) -> PersonBuilder {
        self.person.address = address;
        return self;
    }
    
    def withEmail(email: string) -> PersonBuilder {
        self.person.email = email;
        return self;
    }
    
    def withPhone(phone: string) -> PersonBuilder {
        self.person.phone = phone;
        return self;
    }
    
    def build() -> Person {
        return self.person;
    }
}

// Using the builder
let person = new PersonBuilder()
    .withName("Alice")
    .withAge(30)
    .withEmail("alice@example.com")
    .build();
```

This guide covers the advanced features of Tocin. As you become more comfortable with the language, incorporating these advanced techniques will help you write more powerful, efficient, and maintainable code. 

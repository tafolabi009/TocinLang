# Tocin Standard Library Documentation

## Overview

The Tocin standard library provides a comprehensive set of modules for various programming domains. All modules are written in pure Tocin and demonstrate the language's advanced features.

## Table of Contents

1. [Math](#math)
2. [Data Structures & Algorithms](#data-structures--algorithms)
3. [Web & Networking](#web--networking)
4. [Database](#database)
5. [Machine Learning](#machine-learning)
6. [Game Development](#game-development)
7. [GUI](#gui)
8. [Audio](#audio)
9. [Embedded Systems](#embedded-systems)
10. [Package Management](#package-management)
11. [Scripting & Automation](#scripting--automation)

## Math

### Module: `math/basic.to`

Provides fundamental mathematical operations and constants.

**Constants:**
```to
const PI: float = 3.14159265358979323846;
const E: float = 2.71828182845904523536;
const SQRT2: float = 1.41421356237309504880;
const INFINITY: float = 1.0 / 0.0;
const NAN: float = 0.0 / 0.0;
```

**Functions:**
- `abs(x: number) -> number` - Absolute value
- `sign(x: number) -> int` - Sign of number (-1, 0, 1)
- `floor(x: float) -> int` - Floor function
- `ceil(x: float) -> int` - Ceiling function
- `round(x: float) -> int` - Round to nearest integer
- `min(...args: Array<number>) -> number` - Minimum value
- `max(...args: Array<number>) -> number` - Maximum value
- `clamp(x, min_val, max_val: number) -> number` - Clamp value to range
- `sqrt(x: number) -> float` - Square root
- `pow(x, y: number) -> number` - Power function
- `log(x: number) -> float` - Natural logarithm
- `exp(x: number) -> float` - Exponential function
- `sin(x: number) -> float` - Sine function
- `cos(x: number) -> float` - Cosine function
- `tan(x: number) -> float` - Tangent function

**Example:**
```to
import math.basic;

let angle = math.basic.PI / 4;
let sine = math.basic.sin(angle);
let cosine = math.basic.cos(angle);

print(f"sin(π/4) = {sine}");  // 0.707...
print(f"cos(π/4) = {cosine}"); // 0.707...
```

### Module: `math/linear.to`

Linear algebra operations including vectors and matrices.

**Types:**
- `Vector2`, `Vector3`, `Vector4` - Vector types
- `Matrix2x2`, `Matrix3x3`, `Matrix4x4` - Matrix types

**Operations:**
- Vector addition, subtraction, scaling
- Dot product, cross product
- Matrix multiplication
- Matrix determinant, inverse, transpose

### Module: `math/geometry.to`

Geometric computations and shapes.

**Features:**
- Point, Line, Circle, Rectangle classes
- Distance calculations
- Intersection tests
- Area and perimeter computations

### Module: `math/stats.to`

Statistical functions and distributions.

**Functions:**
- `mean(data: Array<number>) -> float`
- `median(data: Array<number>) -> float`
- `mode(data: Array<number>) -> number`
- `variance(data: Array<number>) -> float`
- `stddev(data: Array<number>) -> float`
- `correlation(x, y: Array<number>) -> float`

### Module: `math/differential.to`

Differential calculus and numerical analysis.

**Features:**
- Numerical differentiation
- Numerical integration
- Differential equation solvers

## Data Structures & Algorithms

### Module: `data/structures.to`

Advanced data structures beyond built-in types.

**Structures:**
- `Stack<T>` - LIFO stack
- `Queue<T>` - FIFO queue
- `PriorityQueue<T>` - Priority-based queue
- `LinkedList<T>` - Doubly-linked list
- `BinaryTree<T>` - Binary tree
- `BinarySearchTree<T>` - BST
- `AVLTree<T>` - Self-balancing BST
- `HashMap<K, V>` - Hash table
- `Trie` - Prefix tree
- `Graph<T>` - Graph structure

**Example:**
```to
import data.structures;

let stack = Stack<int>.new();
stack.push(1);
stack.push(2);
stack.push(3);

while (!stack.is_empty()) {
    print(stack.pop()); // 3, 2, 1
}
```

### Module: `data/algorithms.to`

Common algorithms for sorting, searching, and more.

**Sorting:**
- `quicksort<T>(arr: Array<T>, compare: fn(T, T) -> int)`
- `mergesort<T>(arr: Array<T>, compare: fn(T, T) -> int)`
- `heapsort<T>(arr: Array<T>, compare: fn(T, T) -> int)`

**Searching:**
- `binary_search<T>(arr: Array<T>, target: T) -> int?`
- `linear_search<T>(arr: Array<T>, target: T) -> int?`

**Graph Algorithms:**
- `dijkstra(graph: Graph, start: int) -> Array<int>`
- `bellman_ford(graph: Graph, start: int) -> Array<int>`
- `floyd_warshall(graph: Graph) -> Matrix`
- `kruskal(graph: Graph) -> Graph`
- `prim(graph: Graph) -> Graph`

## Web & Networking

### Module: `web/http.to`

HTTP client and server functionality.

**Classes:**
- `Request` - HTTP request representation
- `Response` - HTTP response representation
- `Client` - HTTP client for making requests
- `Server` - HTTP server for handling requests

**Client Example:**
```to
import web.http;

async def fetch_data() {
    let client = http.Client.new();
    let response = await client.get("https://api.example.com/data");
    
    if (response.statusCode == 200) {
        let data = JSON.parse(response.body);
        return data;
    }
    
    throw Error(f"HTTP {response.statusCode}: {response.statusMessage}");
}
```

**Server Example:**
```to
import web.http;

let server = http.Server.new();

server.get("/", (req, res) => {
    return res.html("<h1>Welcome to Tocin!</h1>");
});

server.get("/api/users/:id", (req, res) => {
    let userId = req.params["id"];
    // Fetch user from database
    return res.json({ id: userId, name: "John Doe" });
});

await server.listen(8080);
print("Server running on http://localhost:8080");
```

### Module: `web/websocket.to`

WebSocket client and server for real-time communication.

**Features:**
- Full-duplex communication
- Binary and text message support
- Connection state management
- Automatic reconnection

**Example:**
```to
import web.websocket;

let ws = websocket.Client.new("ws://localhost:8080");

ws.on_message((message) => {
    print(f"Received: {message}");
});

ws.on_connect(() => {
    ws.send("Hello, server!");
});

await ws.connect();
```

### Module: `net/advanced.to`

Advanced networking features including TCP/UDP sockets, TLS, and protocols.

## Database

### Module: `database/database.to`

Database connectivity and ORM functionality.

**Features:**
- SQL query builder
- Connection pooling
- Transaction support
- ORM (Object-Relational Mapping)
- Support for PostgreSQL, MySQL, SQLite

**Example:**
```to
import database.database;

let db = database.connect("postgresql://localhost/mydb");

// Raw SQL
let users = await db.query("SELECT * FROM users WHERE age > $1", [18]);

// Query builder
let adults = await db.table("users")
    .where("age", ">", 18)
    .order_by("name")
    .get();

// ORM
class User {
    property id: int;
    property name: string;
    property email: string;
    property age: int;
}

let user = User.find(1);
user.age = 30;
await user.save();
```

## Machine Learning

### Module: `ml/neural_network.to`

Neural network implementation with training and inference.

**Features:**
- Layer types: Dense, Conv2D, MaxPool, Dropout, BatchNorm
- Activation functions: ReLU, Sigmoid, Tanh, Softmax
- Loss functions: MSE, CrossEntropy
- Optimizers: SGD, Adam, RMSprop
- Training utilities

**Example:**
```to
import ml.neural_network as nn;

let model = nn.Sequential([
    nn.Dense(784, 128, activation: "relu"),
    nn.Dropout(0.2),
    nn.Dense(128, 64, activation: "relu"),
    nn.Dense(64, 10, activation: "softmax")
]);

let optimizer = nn.Adam(learning_rate: 0.001);
let loss_fn = nn.CrossEntropyLoss();

// Training loop
for epoch in 1..100 {
    for (x_batch, y_batch) in train_loader {
        let predictions = model.forward(x_batch);
        let loss = loss_fn(predictions, y_batch);
        
        model.backward(loss);
        optimizer.step();
    }
    
    print(f"Epoch {epoch}: loss = {loss}");
}
```

### Module: `ml/deep_learning.to`

Advanced deep learning architectures and techniques.

**Features:**
- Pre-trained models (ResNet, VGG, etc.)
- Transfer learning utilities
- Data augmentation
- Model serialization

### Module: `ml/computer_vision.to`

Computer vision algorithms and utilities.

**Features:**
- Image processing (filters, transformations)
- Object detection
- Face recognition
- Feature extraction

## Game Development

### Module: `game/engine.to`

2D/3D game engine functionality.

**Features:**
- Entity-Component-System (ECS) architecture
- Scene management
- Input handling
- Physics simulation
- Collision detection

**Example:**
```to
import game.engine;

class Player {
    property position: Vector2;
    property velocity: Vector2;
    property sprite: Sprite;
    
    def update(delta_time: float) {
        self.position += self.velocity * delta_time;
        
        // Collision detection
        if (collides_with_enemy(self)) {
            self.health -= 10;
        }
    }
    
    def render(renderer: Renderer) {
        renderer.draw_sprite(self.sprite, self.position);
    }
}

let game = engine.Game.new(width: 800, height: 600);
let player = Player.new();

game.add_entity(player);
game.run(); // Start game loop
```

### Module: `game/graphics.to`

Graphics rendering utilities.

**Features:**
- 2D sprite rendering
- 3D mesh rendering
- Particle systems
- Lighting and shadows

### Module: `game/shader.to`

Shader programming support.

## GUI

### Module: `gui/core.to`

GUI framework core functionality.

**Features:**
- Window management
- Event handling
- Layout system
- Theme support

### Module: `gui/widgets.to`

Standard GUI widgets.

**Widgets:**
- Button, Label, TextBox, CheckBox, RadioButton
- ComboBox, ListBox, TreeView
- TabControl, Panel, GroupBox
- ProgressBar, Slider, ScrollBar

**Example:**
```to
import gui.core;
import gui.widgets;

let app = gui.Application.new();
let window = gui.Window.new(title: "My App", width: 400, height: 300);

let button = gui.widgets.Button.new(text: "Click Me");
button.on_click(() => {
    print("Button clicked!");
});

window.add(button);
app.run();
```

## Audio

### Module: `audio/audio.to`

Audio playback and processing.

**Features:**
- Audio file loading (WAV, MP3, OGG)
- Playback control
- Volume and pan control
- Audio effects (reverb, echo, etc.)
- Audio synthesis

**Example:**
```to
import audio.audio;

let music = audio.load("background_music.mp3");
music.set_volume(0.8);
music.set_loop(true);
music.play();

// Audio synthesis
let synth = audio.Synthesizer.new();
synth.set_waveform("sine");
synth.set_frequency(440); // A4 note
synth.play_for(1.0); // Play for 1 second
```

## Embedded Systems

### Module: `embedded/gpio.to`

GPIO (General Purpose Input/Output) control for embedded systems.

**Features:**
- Pin configuration (input/output, pull-up/down)
- Digital read/write
- PWM (Pulse Width Modulation)
- Interrupts

**Example:**
```to
import embedded.gpio;

// Configure LED pin
let led_pin = gpio.Pin.new(17, mode: gpio.OUTPUT);

// Blink LED
for i in 1..10 {
    led_pin.write(gpio.HIGH);
    sleep(500); // milliseconds
    led_pin.write(gpio.LOW);
    sleep(500);
}

// Read button with interrupt
let button_pin = gpio.Pin.new(27, mode: gpio.INPUT, pull: gpio.PULL_UP);
button_pin.on_interrupt(gpio.FALLING_EDGE, () => {
    print("Button pressed!");
});
```

## Package Management

### Module: `pkg/manager.to`

Package management functionality.

**Features:**
- Package installation
- Dependency resolution
- Version management
- Package registry integration

**Example:**
```to
import pkg.manager;

let pm = pkg.Manager.new();

// Install package
await pm.install("http_client", version: "^1.0.0");

// Update packages
await pm.update();

// List installed packages
let packages = pm.list();
for pkg in packages {
    print(f"{pkg.name} v{pkg.version}");
}
```

## Scripting & Automation

### Module: `scripting/automation.to`

Scripting and task automation utilities.

**Features:**
- File system operations
- Process execution
- System information
- Task scheduling

**Example:**
```to
import scripting.automation;

// File operations
let files = automation.list_files(".", pattern: "*.to");
for file in files {
    let content = automation.read_file(file);
    let lines = content.count("\n");
    print(f"{file}: {lines} lines");
}

// Process execution
let result = await automation.execute("ls", ["-la"]);
print(result.stdout);

// Task scheduling
automation.schedule_task(
    name: "backup",
    interval: automation.DAILY,
    at: "02:00",
    action: () => {
        automation.execute("backup.sh");
    }
);
```

## Usage Notes

### Importing Modules

```to
// Import entire module
import math.basic;
let result = math.basic.sqrt(16);

// Import with alias
import web.http as http;
let server = http.Server.new();

// Import specific items
from math.basic import sin, cos, PI;
let x = sin(PI / 2);
```

### Module Organization

All standard library modules follow the same structure:
1. Module documentation at the top
2. Type definitions
3. Constants
4. Functions and classes
5. Helper functions (usually private)

### Error Handling

All standard library functions use Tocin's error handling mechanisms:
- Throwing exceptions for exceptional conditions
- Returning `Option<T>` for operations that may fail
- Returning `Result<T, E>` for operations with typed errors

**Example:**
```to
import data.structures;

let tree = BinarySearchTree<int>.new();

// find() returns Option<int>
match tree.find(42) {
    Some(value) => print(f"Found: {value}"),
    None => print("Not found")
}

// This throws an exception if invalid
let result = tree.remove(42); // May throw if tree is empty
```

### Thread Safety

Most standard library types are thread-safe by default. Exceptions are documented in their respective modules.

### Performance Considerations

- Collection operations are optimized for common use cases
- Lazy evaluation is used where appropriate (LINQ operations)
- Memory allocation is minimized through object pooling
- JIT compilation optimizes hot paths automatically

## Contributing to Standard Library

To add a new module to the standard library:

1. Create the module file in the appropriate directory
2. Follow naming conventions (snake_case for files, PascalCase for types)
3. Add comprehensive documentation
4. Include usage examples
5. Write unit tests
6. Submit a pull request

## Future Additions

Planned standard library modules:
- Cryptography (`crypto/`)
- Compression (`compression/`)
- Regular expressions (`regex/`)
- JSON/XML parsing (`parsing/`)
- Logging (`logging/`)
- Testing framework (`testing/`)
- Benchmarking (`bench/`)

## License

The Tocin standard library is licensed under the MIT License, same as the compiler.

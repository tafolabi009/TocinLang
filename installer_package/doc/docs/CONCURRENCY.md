# Concurrency in Tocin

Tocin implements a Go-style concurrency model with lightweight threads (goroutines) and communication channels. This document explains the key concurrency features and how to use them effectively.

## Goroutines

Goroutines are lightweight threads managed by the Tocin runtime. They allow concurrent execution without the overhead of OS threads.

### Creating Goroutines

To start a function in a new goroutine, use the `go` keyword:

```tocin
// Define a function
fn sayHello(name: string) {
    println("Hello, " + name);
}

// Run it in a goroutine
go sayHello("World");

// Anonymous function in a goroutine
go func() {
    println("Running in a goroutine");
}();
```

Goroutines are multiplexed onto a small number of OS threads, similar to Go's approach. This allows thousands of goroutines to run efficiently.

## Channels

Channels provide a way for goroutines to communicate and synchronize. They implement a message-passing model where data can be sent from one goroutine to another.

### Creating Channels

You can create unbuffered or buffered channels:

```tocin
// Unbuffered channel of integers
let unbuffered = Channel<int>();

// Buffered channel with capacity 5
let buffered = Channel<string>(5);
```

### Sending and Receiving

Send and receive operations are the primary ways to interact with channels:

```tocin
// Send a value to a channel
channel.send(42);

// Receive a value from a channel
let value = channel.receive();

// Try to receive without blocking (returns Option<T>)
match channel.tryReceive() {
    Some(value) => println("Got: " + value.toString()),
    None => println("Channel empty or closed")
}
```

### Channel Behavior

- **Unbuffered Channels**: Send operations block until a receiver is ready. Receive operations block until a sender sends a value.
- **Buffered Channels**: Send operations block only when the buffer is full. Receive operations block only when the buffer is empty.
- **Closed Channels**: You can close a channel to indicate no more values will be sent. Receivers can still receive values that were sent before closing, but further receives will return `None`.

## Select Statement

The `select` statement allows you to wait on multiple channel operations simultaneously. It's similar to a switch statement but for channels:

```tocin
select {
    case v = <-channel1:
        println("Received from channel1: " + v);
    case channel2.send(value):
        println("Sent to channel2");
    case <-timeout(500):
        println("Timeout after 500ms");
    default:
        println("No channel ready");
}
```

### Select Behavior

- If multiple cases are ready, one is chosen randomly.
- If no case is ready, it blocks until one becomes ready.
- If a `default` case is present, it's executed if no other case is ready, making the select non-blocking.

## Timeouts

Tocin provides a timeout mechanism for time-limited operations:

```tocin
// Create a timeout channel that will send a value after 500ms
let timeoutChannel = timeout(500);

// Use with select
select {
    case v = <-dataChannel:
        println("Received data: " + v);
    case <-timeoutChannel:
        println("Operation timed out");
}
```

## Communication Patterns

### Fan-Out (Distribution)

```tocin
fn fanOut() {
    let work = Channel<int>(100);
    
    // Create 5 workers
    for i in 1..5 {
        go worker(i, work);
    }
    
    // Send work
    for i in 1..20 {
        work.send(i);
    }
    
    // Signal no more work
    work.close();
}

fn worker(id: int, work: Channel<int>) {
    for {
        match work.tryReceive() {
            Some(task) => println("Worker " + id.toString() + " processing " + task.toString()),
            None => break  // Channel closed, exit
        }
    }
}
```

### Fan-In (Collection)

```tocin
fn fanIn(input1: Channel<int>, input2: Channel<int>) -> Channel<int> {
    let merged = Channel<int>();
    
    // Start goroutine to collect from input1
    go func() {
        for {
            match input1.tryReceive() {
                Some(v) => merged.send(v),
                None => break
            }
        }
    }();
    
    // Start goroutine to collect from input2
    go func() {
        for {
            match input2.tryReceive() {
                Some(v) => merged.send(v),
                None => break
            }
        }
    }();
    
    return merged;
}
```

## Best Practices

1. **Don't communicate by sharing memory; share memory by communicating.**
   - Use channels to pass data between goroutines rather than using shared variables with locks.

2. **Keep critical sections small.**
   - If you do need to use locks, keep the critical sections (code protected by locks) as small as possible.

3. **Avoid creating too many goroutines.**
   - While goroutines are lightweight, creating millions of them can still consume significant resources.

4. **Always check for closed channels.**
   - Use `tryReceive()` and handle the `None` case when a channel might be closed.

5. **Use buffered channels for performance when appropriate.**
   - Buffered channels can improve performance by reducing the need for synchronization, but they can also hide design problems.

6. **Consider using worker pools for CPU-bound tasks.**
   - For CPU-bound tasks, limit the number of workers to the number of CPU cores. 

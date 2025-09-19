#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <stdexcept>

namespace runtime {

/**
 * A lightweight thread scheduler for managing goroutines.
 * This scheduler uses a worker pool to execute tasks in a
 * cooperative multitasking manner.
 */
class Scheduler {
public:
    using Task = std::function<void()>;

    Scheduler(size_t workerCount = std::thread::hardware_concurrency()) 
        : running(true), workerCount(workerCount) {
        
        // Start worker threads
        for (size_t i = 0; i < workerCount; ++i) {
            workers.emplace_back([this] { workerLoop(); });
        }
    }

    ~Scheduler() {
        // Signal all workers to stop
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            running = false;
        }
        queueCondition.notify_all();

        // Join all worker threads
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // Schedule a task to be executed by a worker
    void schedule(Task task) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!running) {
                throw std::runtime_error("Cannot schedule tasks on a stopped scheduler");
            }
            taskQueue.push(std::move(task));
        }
        queueCondition.notify_one();
    }

    // Create and schedule a new goroutine
    template<typename Func, typename... Args>
    void go(Func&& func, Args&&... args) {
        // Create a task that wraps the function and arguments
        auto task = [f = std::forward<Func>(func), 
                    args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            std::apply(f, args);
        };
        
        schedule(std::move(task));
    }

private:
    // Worker thread loop
    void workerLoop() {
        while (true) {
            Task task;
            
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCondition.wait(lock, [this] { 
                    return !taskQueue.empty() || !running; 
                });
                
                if (!running && taskQueue.empty()) {
                    return;  // Exit the worker thread
                }
                
                if (!taskQueue.empty()) {
                    task = std::move(taskQueue.front());
                    taskQueue.pop();
                }
            }
            
            if (task) {
                try {
                    task();  // Execute the task
                } catch (const std::exception& e) {
                    // Log the exception
                    std::cerr << "Exception in goroutine: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unknown exception in goroutine" << std::endl;
                }
            }
        }
    }

    std::atomic<bool> running;
    size_t workerCount;
    std::vector<std::thread> workers;
    std::queue<Task> taskQueue;
    std::mutex queueMutex;
    std::condition_variable queueCondition;
};

/**
 * A global scheduler instance for the application
 */
inline Scheduler& getGlobalScheduler() {
    static Scheduler scheduler;
    return scheduler;
}

/**
 * Function to launch a goroutine from Tocin code
 */
template<typename Func, typename... Args>
void go(Func&& func, Args&&... args) {
    getGlobalScheduler().go(std::forward<Func>(func), std::forward<Args>(args)...);
}

/**
 * A channel for communication between goroutines, similar to Go channels.
 * The channel can be buffered or unbuffered.
 */
template<typename T>
class Channel {
public:
    // Create an unbuffered channel
    Channel() : capacity(0), closed(false) {}
    
    // Create a buffered channel with specified capacity
    explicit Channel(size_t capacity) : capacity(capacity), closed(false) {}
    
    ~Channel() {
        close();
    }
    
    // Send a value to the channel
    // Blocks if the channel is full
    bool send(T value) {
        std::unique_lock<std::mutex> lock(mutex);
        
        if (closed) {
            return false;  // Cannot send to a closed channel
        }
        
        // If unbuffered or buffer full, wait for a receiver
        notFull.wait(lock, [this] { 
            return buffer.size() < capacity || receivers > 0 || closed; 
        });
        
        if (closed) {
            return false;
        }
        
        if (receivers > 0) {
            // Direct send to a waiting receiver
            temp = std::move(value);
            receivers--;
            notEmpty.notify_one();
        } else {
            // Add to buffer
            buffer.push(std::move(value));
            notEmpty.notify_one();
        }
        
        return true;
    }
    
    // Receive a value from the channel
    // Blocks if the channel is empty
    bool receive(T& value) {
        std::unique_lock<std::mutex> lock(mutex);
        
        if (buffer.empty() && closed) {
            return false;  // Cannot receive from an empty, closed channel
        }
        
        if (buffer.empty()) {
            // No value available, wait for a sender
            receivers++;
            notEmpty.wait(lock, [this] { 
                return !buffer.empty() || hasTemp || closed; 
            });
            
            if (hasTemp) {
                // Direct receive from a sender
                value = std::move(temp);
                hasTemp = false;
                notFull.notify_one();
                return true;
            }
            
            if (buffer.empty() && closed) {
                return false;
            }
        }
        
        // Get a value from the buffer
        value = std::move(buffer.front());
        buffer.pop();
        notFull.notify_one();
        
        return true;
    }
    
    // Close the channel
    void close() {
        std::lock_guard<std::mutex> lock(mutex);
        closed = true;
        notEmpty.notify_all();
        notFull.notify_all();
    }
    
    // Check if the channel is closed
    bool isClosed() const {
        std::lock_guard<std::mutex> lock(mutex);
        return closed;
    }
    
    // Check if the channel is empty
    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return buffer.empty() && !hasTemp;
    }

private:
    size_t capacity;
    std::queue<T> buffer;
    
    mutable std::mutex mutex;
    std::condition_variable notEmpty;
    std::condition_variable notFull;
    
    bool closed;
    size_t receivers = 0;
    
    // For direct sender-to-receiver transfer
    T temp;
    bool hasTemp = false;
};

/**
 * Implements the 'select' operation that allows waiting on multiple channels
 */
class Select {
public:
    Select() = default;
    
    // Add a channel to the select statement with a callback for when it's ready
    template<typename T>
    void addCase(Channel<T>& channel, std::function<void(T)> callback) {
        cases.emplace_back([&channel, callback = std::move(callback)]() -> bool {
            T value;
            if (channel.receive(value)) {
                callback(value);
                return true;
            }
            return false;
        });
    }
    
    // Add a default case that executes if no channel is ready
    void addDefault(std::function<void()> callback) {
        defaultCase = std::move(callback);
    }
    
    // Execute the select statement
    // Returns true if a case was executed, false otherwise
    bool execute() {
        // Try each case in a random order
        std::vector<size_t> indices(cases.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            indices[i] = i;
        }
        std::random_shuffle(indices.begin(), indices.end());
        
        for (size_t idx : indices) {
            if (cases[idx]()) {
                return true;
            }
        }
        
        // If no case is ready and there's a default case, execute it
        if (defaultCase) {
            defaultCase();
            return true;
        }
        
        return false;
    }
    
    // Block until one of the cases is ready
    void wait() {
        while (!execute()) {
            std::this_thread::yield();
        }
    }

private:
    std::vector<std::function<bool()>> cases;
    std::function<void()> defaultCase;
};

} // namespace runtime 

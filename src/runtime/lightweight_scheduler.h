#pragma once

#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <cstdint>

namespace tocin {
namespace runtime {

/**
 * @brief Lightweight Fiber/Coroutine implementation
 * 
 * Uses cooperative multitasking with ~4KB stack size instead of
 * OS threads (~1MB stack) to support millions of concurrent goroutines.
 */
class Fiber {
public:
    using FiberFunc = std::function<void()>;
    
    enum class State {
        Ready,      // Ready to run
        Running,    // Currently executing
        Suspended,  // Suspended, waiting for event
        Completed   // Execution finished
    };

    explicit Fiber(FiberFunc func, size_t stackSize = 4096);
    ~Fiber();

    // Fiber control
    void resume();
    void yield();
    void complete();

    // State management
    State getState() const { return state_; }
    bool isCompleted() const { return state_ == State::Completed; }
    uint64_t getId() const { return id_; }

private:
    uint64_t id_;
    FiberFunc func_;
    State state_;
    void* stack_;
    size_t stackSize_;
    void* context_;
    
    static uint64_t nextId_;
};

/**
 * @brief Work-stealing queue for efficient task distribution
 * 
 * Lock-free queue that allows workers to steal tasks from each other
 * for optimal load balancing.
 */
template<typename T>
class WorkStealingQueue {
public:
    WorkStealingQueue() : bottom_(0), top_(0) {}
    
    // Owner operations (bottom of queue)
    void push(T item);
    T pop();
    
    // Thief operations (top of queue)
    T steal();
    
    // Status
    bool isEmpty() const;
    size_t size() const;

private:
    std::vector<T> items_;
    std::atomic<size_t> bottom_;
    std::atomic<size_t> top_;
    std::mutex mutex_;
};

/**
 * @brief Worker thread that executes fibers
 */
class Worker {
public:
    explicit Worker(size_t id);
    ~Worker();

    // Worker control
    void start();
    void stop();
    void join();
    
    // Task management
    void addFiber(std::shared_ptr<Fiber> fiber);
    std::shared_ptr<Fiber> stealFiber();
    
    // Statistics
    struct WorkerStats {
        uint64_t fibersExecuted;
        uint64_t fibersStolen;
        uint64_t idleTimeMs;
        uint64_t busyTimeMs;
    };
    
    WorkerStats getStats() const { return stats_; }

private:
    void run();
    std::shared_ptr<Fiber> getNextFiber();
    
    size_t id_;
    std::unique_ptr<std::thread> thread_;
    WorkStealingQueue<std::shared_ptr<Fiber>> queue_;
    std::atomic<bool> running_;
    std::atomic<bool> stopping_;
    WorkerStats stats_;
};

/**
 * @brief Lightweight Goroutine Scheduler
 * 
 * Fiber-based scheduler supporting millions of concurrent goroutines
 * with work-stealing for optimal load balancing.
 */
class LightweightScheduler {
public:
    LightweightScheduler();
    explicit LightweightScheduler(size_t numWorkers);
    ~LightweightScheduler();

    // Scheduler control
    void start();
    void stop();
    void waitAll();
    
    // Goroutine creation
    template<typename Func, typename... Args>
    uint64_t go(Func&& func, Args&&... args);
    
    // Configuration
    void setMaxWorkers(size_t count);
    void setFiberStackSize(size_t size);
    
    // Statistics
    struct SchedulerStats {
        size_t totalWorkers;
        size_t activeFibers;
        size_t completedFibers;
        uint64_t totalExecutionTimeMs;
        double averageFiberTimeMs;
    };
    
    SchedulerStats getStats() const;
    
    // Singleton access
    static LightweightScheduler& instance();

private:
    void initialize(size_t numWorkers);
    void balanceLoad();
    
    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<size_t> nextWorker_;
    std::atomic<size_t> activeFibers_;
    std::atomic<size_t> completedFibers_;
    std::atomic<bool> running_;
    size_t fiberStackSize_;
    
    mutable std::mutex statsMutex_;
    std::condition_variable completionCV_;
};

// Template implementations

template<typename T>
void WorkStealingQueue<T>::push(T item) {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.push_back(item);
    bottom_.store(items_.size(), std::memory_order_release);
}

template<typename T>
T WorkStealingQueue<T>::pop() {
    size_t b = bottom_.load(std::memory_order_acquire) - 1;
    bottom_.store(b, std::memory_order_release);
    
    size_t t = top_.load(std::memory_order_acquire);
    
    if (b < t) {
        // Queue was empty
        bottom_.store(t, std::memory_order_release);
        return T();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (b >= items_.size()) {
        return T();
    }
    
    T item = items_[b];
    
    if (b == t) {
        // Last item - race with steal()
        if (!top_.compare_exchange_strong(t, t + 1)) {
            item = T(); // Lost race
        }
        bottom_.store(t + 1, std::memory_order_release);
    }
    
    return item;
}

template<typename T>
T WorkStealingQueue<T>::steal() {
    size_t t = top_.load(std::memory_order_acquire);
    size_t b = bottom_.load(std::memory_order_acquire);
    
    if (t >= b) {
        return T(); // Queue is empty
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (t >= items_.size()) {
        return T();
    }
    
    T item = items_[t];
    
    if (!top_.compare_exchange_strong(t, t + 1)) {
        return T(); // Lost race
    }
    
    return item;
}

template<typename T>
bool WorkStealingQueue<T>::isEmpty() const {
    return bottom_.load() <= top_.load();
}

template<typename T>
size_t WorkStealingQueue<T>::size() const {
    size_t b = bottom_.load();
    size_t t = top_.load();
    return b > t ? b - t : 0;
}

template<typename Func, typename... Args>
uint64_t LightweightScheduler::go(Func&& func, Args&&... args) {
    // Create fiber with bound function
    auto fiber = std::make_shared<Fiber>(
        [func = std::forward<Func>(func), 
         ...args = std::forward<Args>(args)]() mutable {
            func(args...);
        },
        fiberStackSize_
    );
    
    // Assign to next worker (round-robin)
    size_t workerIdx = nextWorker_.fetch_add(1) % workers_.size();
    workers_[workerIdx]->addFiber(fiber);
    
    activeFibers_.fetch_add(1);
    
    return fiber->getId();
}

} // namespace runtime
} // namespace tocin

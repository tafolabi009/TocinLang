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
    
    enum class Priority {
        Critical = 0,  // Highest priority
        High = 1,
        Normal = 2,    // Default priority
        Low = 3,
        Background = 4 // Lowest priority
    };

    explicit Fiber(FiberFunc func, size_t stackSize = 4096, Priority priority = Priority::Normal);
    ~Fiber();

    // Fiber control
    void resume();
    void yield();
    void complete();

    // State management
    State getState() const { return state_; }
    bool isCompleted() const { return state_ == State::Completed; }
    uint64_t getId() const { return id_; }
    Priority getPriority() const { return priority_; }
    void setPriority(Priority priority) { priority_ = priority; }

private:
    uint64_t id_;
    FiberFunc func_;
    State state_;
    Priority priority_;
    void* stack_;
    size_t stackSize_;
    void* context_;
    
    static uint64_t nextId_;
};

/**
 * @brief Work-stealing queue for efficient task distribution
 * 
 * Lock-free queue that allows workers to steal tasks from each other
 * for optimal load balancing. Now supports priority-based ordering.
 */
template<typename T>
class WorkStealingQueue {
public:
    WorkStealingQueue() : bottom_(0), top_(0) {}
    
    // Owner operations (bottom of queue)
    void push(T item);
    void pushPriority(T item, int priority); // Priority-aware push
    T pop();
    
    // Thief operations (top of queue)
    T steal();
    T stealPriority(int minPriority); // Steal with priority filter
    
    // Status
    bool isEmpty() const;
    size_t size() const;

private:
    struct PriorityItem {
        T item;
        int priority;
        size_t insertOrder;
    };
    
    std::vector<T> items_;
    std::vector<PriorityItem> priorityItems_;
    std::atomic<size_t> bottom_;
    std::atomic<size_t> top_;
    std::atomic<size_t> insertCounter_;
    std::mutex mutex_;
    bool usePriority_;
};

/**
 * @brief Worker thread that executes fibers
 * 
 * Enhanced with CPU affinity and NUMA awareness
 */
class Worker {
public:
    explicit Worker(size_t id, int numaNode = -1, int cpuAffinity = -1);
    ~Worker();

    // Worker control
    void start();
    void stop();
    void join();
    
    // Task management
    void addFiber(std::shared_ptr<Fiber> fiber);
    std::shared_ptr<Fiber> stealFiber();
    
    // NUMA and affinity
    void setCPUAffinity(int cpu);
    void setNUMANode(int node);
    int getNUMANode() const { return numaNode_; }
    int getCPUAffinity() const { return cpuAffinity_; }
    
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
    void applyAffinity();
    
    size_t id_;
    int numaNode_;
    int cpuAffinity_;
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
 * Enhanced with priority-based scheduling and NUMA awareness.
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
    
    template<typename Func, typename... Args>
    uint64_t goWithPriority(Fiber::Priority priority, Func&& func, Args&&... args);
    
    // Configuration
    void setMaxWorkers(size_t count);
    void setFiberStackSize(size_t size);
    void enableNUMAAwareness(bool enable);
    void setWorkerAffinity(size_t workerId, int cpu, int numaNode);
    
    // Statistics
    struct SchedulerStats {
        size_t totalWorkers;
        size_t activeFibers;
        size_t completedFibers;
        uint64_t totalExecutionTimeMs;
        double averageFiberTimeMs;
        size_t numNUMANodes;
    };
    
    SchedulerStats getStats() const;
    
    // Singleton access
    static LightweightScheduler& instance();

private:
    void initialize(size_t numWorkers);
    void balanceLoad();
    void detectNUMATopology();
    size_t selectWorkerForFiber(Fiber::Priority priority);
    
    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<size_t> nextWorker_;
    std::atomic<size_t> activeFibers_;
    std::atomic<size_t> completedFibers_;
    std::atomic<bool> running_;
    size_t fiberStackSize_;
    bool numaAware_;
    size_t numNUMANodes_;
    std::vector<size_t> numaNodeWorkers_; // Workers per NUMA node
    
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
void WorkStealingQueue<T>::pushPriority(T item, int priority) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t order = insertCounter_.fetch_add(1);
    priorityItems_.push_back({item, priority, order});
    
    // Sort by priority (lower value = higher priority), then by insertion order
    std::sort(priorityItems_.begin(), priorityItems_.end(),
        [](const PriorityItem& a, const PriorityItem& b) {
            if (a.priority != b.priority) return a.priority < b.priority;
            return a.insertOrder < b.insertOrder;
        });
    
    bottom_.store(priorityItems_.size(), std::memory_order_release);
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
    
    // Try priority items first
    if (!priorityItems_.empty()) {
        T item = priorityItems_.back().item;
        priorityItems_.pop_back();
        bottom_.store(priorityItems_.size(), std::memory_order_release);
        return item;
    }
    
    // Fall back to regular items
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
    
    // Steal from priority items first (highest priority)
    if (!priorityItems_.empty()) {
        T item = priorityItems_.front().item;
        priorityItems_.erase(priorityItems_.begin());
        top_.fetch_add(1);
        return item;
    }
    
    // Steal from regular items
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
T WorkStealingQueue<T>::stealPriority(int minPriority) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find highest priority item that meets minimum priority requirement
    for (auto it = priorityItems_.begin(); it != priorityItems_.end(); ++it) {
        if (it->priority <= minPriority) {
            T item = it->item;
            priorityItems_.erase(it);
            top_.fetch_add(1);
            return item;
        }
    }
    
    return T();
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
    return goWithPriority(Fiber::Priority::Normal, std::forward<Func>(func), std::forward<Args>(args)...);
}

template<typename Func, typename... Args>
uint64_t LightweightScheduler::goWithPriority(Fiber::Priority priority, Func&& func, Args&&... args) {
    // Create fiber with bound function and priority
    auto fiber = std::make_shared<Fiber>(
        [func = std::forward<Func>(func), 
         ...args = std::forward<Args>(args)]() mutable {
            func(args...);
        },
        fiberStackSize_,
        priority
    );
    
    // Select worker based on priority and NUMA awareness
    size_t workerIdx = selectWorkerForFiber(priority);
    workers_[workerIdx]->addFiber(fiber);
    
    activeFibers_.fetch_add(1);
    
    return fiber->getId();
}

} // namespace runtime
} // namespace tocin

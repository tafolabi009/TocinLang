#include "lightweight_scheduler.h"
#include <algorithm>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <ucontext.h>
#endif

namespace tocin {
namespace runtime {

// ============================================================================
// Fiber Implementation
// ============================================================================

uint64_t Fiber::nextId_ = 1;

Fiber::Fiber(FiberFunc func, size_t stackSize)
    : id_(nextId_++)
    , func_(std::move(func))
    , state_(State::Ready)
    , stack_(nullptr)
    , stackSize_(stackSize)
    , context_(nullptr) {
    
    // Allocate stack
    stack_ = malloc(stackSize_);
    if (!stack_) {
        throw std::bad_alloc();
    }
    
#ifndef _WIN32
    // Initialize context (POSIX)
    context_ = malloc(sizeof(ucontext_t));
    // TODO: Setup context with makecontext()
#endif
}

Fiber::~Fiber() {
    if (stack_) {
        free(stack_);
    }
    if (context_) {
        free(context_);
    }
}

void Fiber::resume() {
    if (state_ == State::Completed) {
        return;
    }
    
    state_ = State::Running;
    
    // Execute fiber function
    if (func_) {
        func_();
        complete();
    }
}

void Fiber::yield() {
    if (state_ == State::Running) {
        state_ = State::Suspended;
    }
}

void Fiber::complete() {
    state_ = State::Completed;
}

// ============================================================================
// Worker Implementation
// ============================================================================

Worker::Worker(size_t id)
    : id_(id)
    , running_(false)
    , stopping_(false) {
    stats_ = {0, 0, 0, 0};
}

Worker::~Worker() {
    stop();
    join();
}

void Worker::start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    stopping_.store(false);
    thread_ = std::make_unique<std::thread>(&Worker::run, this);
}

void Worker::stop() {
    stopping_.store(true);
}

void Worker::join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void Worker::addFiber(std::shared_ptr<Fiber> fiber) {
    queue_.push(fiber);
}

std::shared_ptr<Fiber> Worker::stealFiber() {
    auto fiber = queue_.steal();
    if (fiber) {
        stats_.fibersStolen++;
    }
    return fiber;
}

void Worker::run() {
    auto lastActivity = std::chrono::high_resolution_clock::now();
    bool wasIdle = false;
    
    while (running_.load() && !stopping_.load()) {
        auto fiber = getNextFiber();
        
        if (fiber) {
            // Execute fiber
            auto start = std::chrono::high_resolution_clock::now();
            fiber->resume();
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            stats_.busyTimeMs += duration.count();
            
            if (fiber->isCompleted()) {
                stats_.fibersExecuted++;
            } else {
                // Re-queue if not completed
                queue_.push(fiber);
            }
            
            wasIdle = false;
            lastActivity = end;
        } else {
            // Idle - track idle time
            if (!wasIdle) {
                wasIdle = true;
                lastActivity = std::chrono::high_resolution_clock::now();
            }
            
            // Brief sleep to avoid spinning
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            
            auto now = std::chrono::high_resolution_clock::now();
            auto idleDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastActivity);
            stats_.idleTimeMs += idleDuration.count();
            lastActivity = now;
        }
    }
    
    running_.store(false);
}

std::shared_ptr<Fiber> Worker::getNextFiber() {
    // Try to get from own queue first
    auto fiber = queue_.pop();
    if (fiber) {
        return fiber;
    }
    
    // If queue is empty, try work stealing is handled by scheduler
    return nullptr;
}

// ============================================================================
// LightweightScheduler Implementation
// ============================================================================

LightweightScheduler::LightweightScheduler()
    : LightweightScheduler(std::thread::hardware_concurrency()) {
}

LightweightScheduler::LightweightScheduler(size_t numWorkers)
    : nextWorker_(0)
    , activeFibers_(0)
    , completedFibers_(0)
    , running_(false)
    , fiberStackSize_(4096) {
    initialize(numWorkers);
}

LightweightScheduler::~LightweightScheduler() {
    stop();
}

void LightweightScheduler::initialize(size_t numWorkers) {
    if (numWorkers == 0) {
        numWorkers = 1;
    }
    
    workers_.reserve(numWorkers);
    for (size_t i = 0; i < numWorkers; ++i) {
        workers_.push_back(std::make_unique<Worker>(i));
    }
}

void LightweightScheduler::start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    
    // Start all workers
    for (auto& worker : workers_) {
        worker->start();
    }
    
    // Start load balancing thread
    std::thread([this]() {
        while (running_.load()) {
            balanceLoad();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();
}

void LightweightScheduler::stop() {
    running_.store(false);
    
    // Stop all workers
    for (auto& worker : workers_) {
        worker->stop();
    }
    
    // Wait for workers to finish
    for (auto& worker : workers_) {
        worker->join();
    }
}

void LightweightScheduler::waitAll() {
    std::unique_lock<std::mutex> lock(statsMutex_);
    completionCV_.wait(lock, [this]() {
        return activeFibers_.load() == 0;
    });
}

void LightweightScheduler::setMaxWorkers(size_t count) {
    if (running_.load()) {
        return; // Cannot change while running
    }
    
    workers_.clear();
    initialize(count);
}

void LightweightScheduler::setFiberStackSize(size_t size) {
    fiberStackSize_ = std::max(size, size_t(1024)); // Minimum 1KB
}

LightweightScheduler::SchedulerStats LightweightScheduler::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    SchedulerStats stats;
    stats.totalWorkers = workers_.size();
    stats.activeFibers = activeFibers_.load();
    stats.completedFibers = completedFibers_.load();
    
    uint64_t totalTime = 0;
    for (const auto& worker : workers_) {
        auto workerStats = worker->getStats();
        totalTime += workerStats.busyTimeMs;
    }
    
    stats.totalExecutionTimeMs = totalTime;
    stats.averageFiberTimeMs = stats.completedFibers > 0 
        ? static_cast<double>(totalTime) / stats.completedFibers 
        : 0.0;
    
    return stats;
}

void LightweightScheduler::balanceLoad() {
    // Find busiest and least busy workers
    if (workers_.size() < 2) {
        return;
    }
    
    // Simple work stealing: if a worker is idle, steal from busiest
    for (size_t i = 0; i < workers_.size(); ++i) {
        for (size_t j = 0; j < workers_.size(); ++j) {
            if (i != j) {
                auto fiber = workers_[j]->stealFiber();
                if (fiber) {
                    workers_[i]->addFiber(fiber);
                    break; // Only steal one per round
                }
            }
        }
    }
}

LightweightScheduler& LightweightScheduler::instance() {
    static LightweightScheduler instance;
    return instance;
}

} // namespace runtime
} // namespace tocin

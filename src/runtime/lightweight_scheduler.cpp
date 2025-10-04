#include "lightweight_scheduler.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <ucontext.h>
#include <pthread.h>
#include <sched.h>
#include <sys/stat.h>
#endif

namespace tocin {
namespace runtime {

// ============================================================================
// Fiber Implementation
// ============================================================================

uint64_t Fiber::nextId_ = 1;

Fiber::Fiber(FiberFunc func, size_t stackSize, Priority priority)
    : id_(nextId_++)
    , func_(std::move(func))
    , state_(State::Ready)
    , priority_(priority)
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

Worker::Worker(size_t id, int numaNode, int cpuAffinity)
    : id_(id)
    , numaNode_(numaNode)
    , cpuAffinity_(cpuAffinity)
    , running_(false)
    , stopping_(false) {
    stats_.fibersExecuted = 0;
    stats_.fibersStolen = 0;
    stats_.idleTimeMs = 0;
    stats_.busyTimeMs = 0;
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

void Worker::setCPUAffinity(int cpu) {
    cpuAffinity_ = cpu;
    if (thread_ && thread_->joinable()) {
        applyAffinity();
    }
}

void Worker::setNUMANode(int node) {
    numaNode_ = node;
}

void Worker::applyAffinity() {
#ifdef _WIN32
    if (cpuAffinity_ >= 0) {
        HANDLE thread = thread_->native_handle();
        DWORD_PTR mask = 1ULL << cpuAffinity_;
        SetThreadAffinityMask(thread, mask);
    }
#elif defined(__linux__)
    if (cpuAffinity_ >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpuAffinity_, &cpuset);
        pthread_setaffinity_np(thread_->native_handle(), sizeof(cpu_set_t), &cpuset);
    }
#endif
}

void Worker::run() {
    // Apply CPU affinity if set
    applyAffinity();
    
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
    , fiberStackSize_(4096)
    , numaAware_(false)
    , numNUMANodes_(0) {
    initialize(numWorkers);
}

LightweightScheduler::~LightweightScheduler() {
    stop();
}

void LightweightScheduler::initialize(size_t numWorkers) {
    if (numWorkers == 0) {
        numWorkers = 1;
    }
    
    // Detect NUMA topology
    detectNUMATopology();
    
    workers_.reserve(numWorkers);
    
    // If NUMA aware, distribute workers across NUMA nodes
    if (numaAware_ && numNUMANodes_ > 0) {
        size_t workersPerNode = (numWorkers + numNUMANodes_ - 1) / numNUMANodes_;
        for (size_t i = 0; i < numWorkers; ++i) {
            int numaNode = i / workersPerNode;
            int cpuAffinity = numaNode * workersPerNode + (i % workersPerNode);
            workers_.push_back(std::make_unique<Worker>(i, numaNode, cpuAffinity));
        }
    } else {
        for (size_t i = 0; i < numWorkers; ++i) {
            workers_.push_back(std::make_unique<Worker>(i, -1, -1));
        }
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
    stats.numNUMANodes = numNUMANodes_;
    
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

void LightweightScheduler::enableNUMAAwareness(bool enable) {
    if (running_.load()) {
        return; // Cannot change while running
    }
    numaAware_ = enable;
    
    // Re-initialize workers with NUMA awareness
    workers_.clear();
    initialize(std::thread::hardware_concurrency());
}

void LightweightScheduler::setWorkerAffinity(size_t workerId, int cpu, int numaNode) {
    if (workerId < workers_.size()) {
        workers_[workerId]->setCPUAffinity(cpu);
        workers_[workerId]->setNUMANode(numaNode);
    }
}

void LightweightScheduler::detectNUMATopology() {
#ifdef __linux__
    // On Linux, check /sys/devices/system/node for NUMA nodes
    numNUMANodes_ = 0;
    for (int i = 0; i < 256; ++i) {
        std::string path = "/sys/devices/system/node/node" + std::to_string(i);
        struct stat buffer;
        if (stat(path.c_str(), &buffer) == 0) {
            numNUMANodes_ = i + 1;
        } else {
            break;
        }
    }
    
    if (numNUMANodes_ == 0) {
        numNUMANodes_ = 1; // Default to single node
    }
#elif defined(_WIN32)
    // On Windows, use GetLogicalProcessorInformationEx
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationNumaNode, nullptr, &length);
    if (length > 0) {
        std::vector<BYTE> buffer(length);
        if (GetLogicalProcessorInformationEx(RelationNumaNode, 
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), 
            &length)) {
            
            DWORD offset = 0;
            while (offset < length) {
                auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
                    buffer.data() + offset);
                if (info->Relationship == RelationNumaNode) {
                    numNUMANodes_ = std::max(numNUMANodes_, 
                        static_cast<size_t>(info->NumaNode.NodeNumber + 1));
                }
                offset += info->Size;
            }
        }
    }
    
    if (numNUMANodes_ == 0) {
        numNUMANodes_ = 1; // Default to single node
    }
#else
    numNUMANodes_ = 1; // Default for other systems
#endif
}

size_t LightweightScheduler::selectWorkerForFiber(Fiber::Priority priority) {
    if (!numaAware_ || numNUMANodes_ <= 1) {
        // Simple round-robin
        return nextWorker_.fetch_add(1) % workers_.size();
    }
    
    // NUMA-aware: prefer workers on the current NUMA node
    // For high-priority tasks, use workers on node 0
    if (priority <= Fiber::Priority::High) {
        // Find workers on NUMA node 0
        for (size_t i = 0; i < workers_.size(); ++i) {
            if (workers_[i]->getNUMANode() == 0) {
                return i;
            }
        }
    }
    
    // For normal/low priority, distribute across all nodes
    return nextWorker_.fetch_add(1) % workers_.size();
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

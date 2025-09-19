#pragma once

#include "../ast/ast.h"
#include "../error/error_handler.h"
#include "concurrency.h"
#include <memory>
#include <functional>
#include <future>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include <tuple>
#include <type_traits>

namespace runtime {

/**
 * @brief Async task state
 */
enum class TaskState {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
};

/**
 * @brief Async task result
 */
template<typename T>
struct TaskResult {
    TaskState state;
    T value;
    std::string error;
    std::chrono::steady_clock::time_point completionTime;
    
    TaskResult() : state(TaskState::PENDING) {}
    TaskResult(const T& val) : state(TaskState::COMPLETED), value(val) {}
    TaskResult(const std::string& err) : state(TaskState::FAILED), error(err) {}
};


/**
 * @brief Async task scheduler
 */
class AsyncScheduler {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
    size_t maxWorkers;
    
public:
    AsyncScheduler(size_t workerCount = std::thread::hardware_concurrency())
        : stop(false), maxWorkers(workerCount) {
        startWorkers();
    }
    
    ~AsyncScheduler() {
        stopWorkers();
    }
    
    /**
     * @brief Submit a task for execution
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> Future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        
        auto promise = std::make_shared<Promise<return_type>>();
        auto future = std::make_shared<Future<return_type>>(promise->getFuture());
        
        auto boundFunc = std::make_shared<std::decay_t<F>>(std::forward<F>(f));
        auto boundArgs = std::make_shared<std::tuple<std::decay_t<Args>...>>(std::forward<Args>(args)...);
        
        auto task = [promise, boundFunc, boundArgs]() mutable {
            try {
                if constexpr (std::is_void_v<return_type>) {
                    std::apply(*boundFunc, *boundArgs);
                    promise->resolve();
                } else {
                    auto result = std::apply(*boundFunc, *boundArgs);
                    promise->resolve(result);
                }
            } catch (const std::exception& e) {
                promise->reject(e.what());
            }
        };
        
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tasks.push(std::move(task));
        }
        condition.notify_one();
        
        return *future;
    }
    
    /**
     * @brief Create a delayed task
     */
    template<typename F, typename... Args>
    auto delay(std::chrono::milliseconds delay, F&& f, Args&&... args) -> Future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        
        auto promise = std::make_shared<Promise<return_type>>();
        auto future = std::make_shared<Future<return_type>>(promise->getFuture());
        
        auto boundFunc = std::make_shared<std::decay_t<F>>(std::forward<F>(f));
        auto boundArgs = std::make_shared<std::tuple<std::decay_t<Args>...>>(std::forward<Args>(args)...);
        
        auto task = [promise, boundFunc, boundArgs]() mutable {
            try {
                if constexpr (std::is_void_v<return_type>) {
                    std::apply(*boundFunc, *boundArgs);
                    promise->resolve();
                } else {
                    auto result = std::apply(*boundFunc, *boundArgs);
                    promise->resolve(result);
                }
            } catch (const std::exception& e) {
                promise->reject(e.what());
            }
        };
        
        std::thread([this, task = std::move(task), delay]() mutable {
            std::this_thread::sleep_for(delay);
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                tasks.push(std::move(task));
            }
            condition.notify_one();
        }).detach();
        
        return *future;
    }
    
    /**
     * @brief Wait for all tasks to complete
     */
    void waitForAll() {
        std::unique_lock<std::mutex> lock(queueMutex);
        condition.wait(lock, [this] { return tasks.empty(); });
    }
    
    /**
     * @brief Get number of pending tasks
     */
    size_t getPendingTaskCount() {
        std::lock_guard<std::mutex> lock(queueMutex);
        return tasks.size();
    }
    
private:
    void startWorkers() {
        for (size_t i = 0; i < maxWorkers; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) {
                            return;
                        }
                        if (!tasks.empty()) {
                            task = std::move(tasks.front());
                            tasks.pop();
                        }
                    }
                    if (task) {
                        task();
                    }
                }
            });
        }
    }
    
    void stopWorkers() {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
};

/**
 * @brief Coroutine context
 */
class CoroutineContext {
private:
    std::shared_ptr<AsyncScheduler> scheduler;
    std::function<void()> continuation;
    bool suspended;
    
public:
    CoroutineContext(std::shared_ptr<AsyncScheduler> sched) 
        : scheduler(sched), suspended(false) {}
    
    /**
     * @brief Suspend the coroutine
     */
    void suspend() {
        suspended = true;
    }
    
    /**
     * @brief Resume the coroutine
     */
    void resume() {
        if (suspended && continuation) {
            suspended = false;
            scheduler->submit(continuation);
        }
    }
    
    /**
     * @brief Set continuation function
     */
    void setContinuation(std::function<void()> cont) {
        continuation = cont;
    }
    
    /**
     * @brief Check if coroutine is suspended
     */
    bool isSuspended() const {
        return suspended;
    }
};

/**
 * @brief Async function wrapper
 */
template<typename T>
class AsyncFunction {
private:
    std::function<T()> func;
    std::shared_ptr<AsyncScheduler> scheduler;
    
public:
    AsyncFunction(std::function<T()> f, std::shared_ptr<AsyncScheduler> sched)
        : func(f), scheduler(sched) {}
    
    /**
     * @brief Execute the async function
     */
    Future<T> execute() {
        return scheduler->submit(func);
    }
    
    /**
     * @brief Execute with delay
     */
    Future<T> executeAfter(std::chrono::milliseconds delay) {
        return scheduler->delay(delay, func);
    }
};

/**
 * @brief Global async system
 */
class AsyncSystem {
private:
    static std::shared_ptr<AsyncScheduler> globalScheduler;
    static std::mutex schedulerMutex;
    
public:
    /**
     * @brief Initialize the global async system
     */
    static void initialize(size_t workerCount = std::thread::hardware_concurrency()) {
        std::lock_guard<std::mutex> lock(schedulerMutex);
        if (!globalScheduler) {
            globalScheduler = std::make_shared<AsyncScheduler>(workerCount);
        }
    }
    
    /**
     * @brief Get the global scheduler
     */
    static std::shared_ptr<AsyncScheduler> getScheduler() {
        std::lock_guard<std::mutex> lock(schedulerMutex);
        if (!globalScheduler) {
            initialize();
        }
        return globalScheduler;
    }
    
    /**
     * @brief Create an async function
     */
    template<typename T>
    static AsyncFunction<T> createAsync(std::function<T()> func) {
        return AsyncFunction<T>(func, getScheduler());
    }
    
    /**
     * @brief Await a future
     */
    template<typename T>
    static T await(Future<T>& future) {
        return future.get();
    }
    
    /**
     * @brief Await with timeout
     */
    template<typename T, typename Rep, typename Period>
    static T await(Future<T>& future, const std::chrono::duration<Rep, Period>& timeout) {
        return future.get(timeout);
    }
    
    /**
     * @brief Wait for all futures
     */
    template<typename... Futures>
    static void waitForAll(Futures&... futures) {
        (futures.get(), ...);
    }
    
    /**
     * @brief Wait for any future
     */
    template<typename T>
    static Future<T> waitForAny(std::vector<Future<T>>& futures) {
        // This would implement waiting for any future to complete
        // For now, return the first future
        return futures.empty() ? Future<T>(nullptr) : futures[0];
    }
};

// Static member definitions
std::shared_ptr<AsyncScheduler> AsyncSystem::globalScheduler = nullptr;

std::mutex AsyncSystem::schedulerMutex;

} // namespace runtime 
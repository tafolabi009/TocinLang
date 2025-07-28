#ifndef TOCIN_CONCURRENCY_H
#define TOCIN_CONCURRENCY_H

#include <future>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <optional>

namespace runtime {

// Forward declarations
template<typename T> class Channel;
template<typename T> class Future;
template<typename T> class Promise;
class Scheduler;

/**
 * @brief Thread-safe channel implementation for Tocin
 */
template<typename T>
class Channel {
private:
    std::queue<T> buffer;
    size_t capacity;
    std::mutex mutex;
    std::condition_variable not_full;
    std::condition_variable not_empty;
    std::atomic<bool> closed{false};

public:
    explicit Channel(size_t cap = 0) : capacity(cap) {}

    /**
     * @brief Send a value to the channel
     */
    bool send(const T& value) {
        std::unique_lock<std::mutex> lock(mutex);
        
        if (closed.load()) {
            return false;
        }

        // Wait if buffer is full (for bounded channels)
        if (capacity > 0) {
            not_full.wait(lock, [this] { return buffer.size() < capacity || closed.load(); });
        }

        if (closed.load()) {
            return false;
        }

        buffer.push(value);
        not_empty.notify_one();
        return true;
    }

    /**
     * @brief Receive a value from the channel
     */
    std::optional<T> receive() {
        std::unique_lock<std::mutex> lock(mutex);
        
        not_empty.wait(lock, [this] { return !buffer.empty() || closed.load(); });
        
        if (buffer.empty() && closed.load()) {
            return std::nullopt;
        }

        T value = buffer.front();
        buffer.pop();
        not_full.notify_one();
        return value;
    }

    /**
     * @brief Close the channel
     */
    void close() {
        std::unique_lock<std::mutex> lock(mutex);
        closed.store(true);
        not_empty.notify_all();
        not_full.notify_all();
    }

    /**
     * @brief Check if channel is closed
     */
    bool isClosed() const {
        return closed.load();
    }

    /**
     * @brief Get current buffer size
     */
    size_t size() const {
        std::unique_lock<std::mutex> lock(mutex);
        return buffer.size();
    }
};

/**
 * @brief Promise/Future implementation for async operations
 */
template<typename T>
class Promise {
private:
    std::shared_ptr<std::promise<T>> promise;
    std::shared_ptr<std::future<T>> future;

public:
    Promise() : promise(std::make_shared<std::promise<T>>()) {
        future = std::make_shared<std::future<T>>(promise->get_future());
    }

    /**
     * @brief Set the value of the promise
     */
    void setValue(const T& value) {
        promise->set_value(value);
    }

    /**
     * @brief Set an exception
     */
    void setException(const std::exception_ptr& e) {
        promise->set_exception(e);
    }

    /**
     * @brief Get the future
     */
    std::shared_ptr<std::future<T>> getFuture() const {
        return future;
    }
};

/**
 * @brief Future wrapper for easier async/await usage
 */
template<typename T>
class Future {
private:
    std::shared_ptr<std::future<T>> future;
    std::optional<T> cached_value;
    bool has_cached = false;

public:
    explicit Future(std::shared_ptr<std::future<T>> f) : future(f) {}

    /**
     * @brief Get the result (blocking)
     */
    T get() {
        if (!has_cached) {
            cached_value = future->get();
            has_cached = true;
        }
        return *cached_value;
    }

    /**
     * @brief Check if the future is ready
     */
    bool isReady() const {
        return future->wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    /**
     * @brief Wait for the future to complete
     */
    void wait() const {
        future->wait();
    }

    /**
     * @brief Wait for the future with timeout
     */
    template<typename Rep, typename Period>
    bool waitFor(const std::chrono::duration<Rep, Period>& timeout) const {
        return future->wait_for(timeout) == std::future_status::ready;
    }
};

/**
 * @brief Thread pool for managing goroutines
 */
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> should_stop{false};
    std::atomic<size_t> active_threads{0};

public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] { return should_stop.load() || !tasks.empty(); });
                        if (should_stop.load() && tasks.empty()) {
                            return;
                        }
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    active_threads++;
                    task();
                    active_threads--;
                }
            });
        }
    }

    /**
     * @brief Submit a task to the thread pool
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (should_stop.load()) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    /**
     * @brief Get number of active threads
     */
    size_t getActiveThreads() const {
        return active_threads.load();
    }

    /**
     * @brief Get number of queued tasks
     */
    size_t getQueuedTasks() const {
        std::unique_lock<std::mutex> lock(queue_mutex);
        return tasks.size();
    }

    /**
     * @brief Stop the thread pool
     */
    void stop() {
        should_stop.store(true);
        condition.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    ~ThreadPool() {
        stop();
    }
};

/**
 * @brief Scheduler for managing async tasks
 */
class Scheduler {
private:
    std::unique_ptr<ThreadPool> thread_pool;
    std::unordered_map<std::string, std::function<void()>> registered_tasks;
    std::mutex tasks_mutex;

public:
    Scheduler() : thread_pool(std::make_unique<ThreadPool>()) {}

    /**
     * @brief Launch a goroutine
     */
    template<typename F, typename... Args>
    void go(F&& f, Args&&... args) {
        thread_pool->submit(std::forward<F>(f), std::forward<Args>(args)...);
    }

    /**
     * @brief Create an async task
     */
    template<typename F, typename... Args>
    auto async(F&& f, Args&&... args) -> Future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        
        auto promise = std::make_shared<Promise<return_type>>();
        auto future = promise->getFuture();
        
        thread_pool->submit([promise, f = std::forward<F>(f), args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            try {
                auto result = std::apply(f, args_tuple);
                promise->setValue(result);
            } catch (...) {
                promise->setException(std::current_exception());
            }
        });
        
        return Future<return_type>(future);
    }

    /**
     * @brief Wait for all tasks to complete
     */
    void waitForAll() {
        // Simple implementation - in a real system you'd track all tasks
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

/**
 * @brief Select statement implementation
 */
template<typename... Channels>
class Select {
private:
    std::tuple<Channels&...> channels;
    std::function<void(size_t, typename std::tuple_element_t<0, std::tuple<Channels...>>::value_type)> onReceive;
    std::function<void(size_t)> onSend;

public:
    Select(std::tuple<Channels&...> chs, 
           std::function<void(size_t, typename Channels::value_type)> recv = nullptr,
           std::function<void(size_t)> send = nullptr)
        : channels(chs), onReceive(recv), onSend(send) {}

    /**
     * @brief Execute the select statement
     */
    int execute() {
        // Simple implementation - in a real system you'd use proper event handling
        size_t index = 0;
        std::apply([this, &index](auto&... ch) {
            // Try to receive from each channel
            (tryReceive(ch, index++) || ...);
        }, channels);
        return -1; // No channel ready
    }

private:
    template<typename Channel>
    bool tryReceive(Channel& ch, size_t idx) {
        auto value = ch.receive();
        if (value.has_value()) {
            if (onReceive) {
                onReceive(idx, *value);
            }
            return true;
        }
        return false;
    }
};

// Global scheduler instance
extern std::unique_ptr<Scheduler> global_scheduler;

/**
 * @brief Initialize the global scheduler
 */
void initializeScheduler();

/**
 * @brief Get the global scheduler
 */
Scheduler& getScheduler();

/**
 * @brief Launch a goroutine
 */
template<typename F, typename... Args>
void launchGoroutine(F&& f, Args&&... args) {
    getScheduler().go(std::forward<F>(f), std::forward<Args>(args)...);
}

/**
 * @brief Create an async task
 */
template<typename F, typename... Args>
auto createAsync(F&& f, Args&&... args) -> Future<decltype(f(args...))> {
    return getScheduler().async(std::forward<F>(f), std::forward<Args>(args)...);
}

/**
 * @brief Await a future
 */
template<typename T>
T await(Future<T>& future) {
    return future.get();
}

} // namespace runtime

#endif // TOCIN_CONCURRENCY_H

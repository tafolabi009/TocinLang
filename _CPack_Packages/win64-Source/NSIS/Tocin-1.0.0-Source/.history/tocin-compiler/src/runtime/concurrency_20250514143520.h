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
#include <future>
#include <optional>
#include <variant>
#include "../pch.h"
#include "../ast/ast.h"
#include "../type/type_checker.h"
#include "../error/error_handler.h"

namespace runtime
{

    /**
     * A lightweight thread scheduler for managing goroutines.
     * This scheduler uses a worker pool to execute tasks in a
     * cooperative multitasking manner.
     */
    class Scheduler
    {
    public:
        using Task = std::function<void()>;

        Scheduler(size_t workerCount = std::thread::hardware_concurrency())
            : running(true), workerCount(workerCount)
        {

            // Start worker threads
            for (size_t i = 0; i < workerCount; ++i)
            {
                workers.emplace_back([this]
                                     { workerLoop(); });
            }
        }

        ~Scheduler()
        {
            // Signal all workers to stop
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                running = false;
            }
            queueCondition.notify_all();

            // Join all worker threads
            for (auto &worker : workers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
        }

        // Schedule a task to be executed by a worker
        void schedule(Task task)
        {
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (!running)
                {
                    throw std::runtime_error("Cannot schedule tasks on a stopped scheduler");
                }
                taskQueue.push(std::move(task));
            }
            queueCondition.notify_one();
        }

        // Create and schedule a new goroutine
        template <typename Func, typename... Args>
        void go(Func &&func, Args &&...args)
        {
            // Create a task that wraps the function and arguments
            auto task = [f = std::forward<Func>(func),
                         args = std::make_tuple(std::forward<Args>(args)...)]() mutable
            {
                std::apply(f, args);
            };

            schedule(std::move(task));
        }

    private:
        // Worker thread loop
        void workerLoop()
        {
            while (true)
            {
                Task task;

                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    queueCondition.wait(lock, [this]
                                        { return !taskQueue.empty() || !running; });

                    if (!running && taskQueue.empty())
                    {
                        return; // Exit the worker thread
                    }

                    if (!taskQueue.empty())
                    {
                        task = std::move(taskQueue.front());
                        taskQueue.pop();
                    }
                }

                if (task)
                {
                    try
                    {
                        task(); // Execute the task
                    }
                    catch (const std::exception &e)
                    {
                        // Log the exception
                        std::cerr << "Exception in goroutine: " << e.what() << std::endl;
                    }
                    catch (...)
                    {
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
    inline Scheduler &getGlobalScheduler()
    {
        static Scheduler scheduler;
        return scheduler;
    }

    /**
     * Function to launch a goroutine from Tocin code
     */
    template <typename Func, typename... Args>
    void go(Func &&func, Args &&...args)
    {
        getGlobalScheduler().go(std::forward<Func>(func), std::forward<Args>(args)...);
    }

    // Forward declarations
    template <typename T>
    class Future;

    template <typename T>
    class Promise;

    /**
     * An asynchronous result that may be available in the future.
     * This provides the basis for the language's async/await mechanism.
     */
    template <typename T>
    class Future
    {
    public:
        using ValueType = T;

        Future() : state(std::make_shared<State>()) {}

        // Check if the future is ready
        bool isReady() const
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            return state->hasValue || state->hasError;
        }

        // Wait for the future to complete
        void wait() const
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            state->condition.wait(lock, [this]()
                                  { return state->hasValue || state->hasError; });
        }

        // Wait for the future with a timeout
        template <typename Rep, typename Period>
        bool waitFor(const std::chrono::duration<Rep, Period> &timeout) const
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            return state->condition.wait_for(lock, timeout, [this]()
                                             { return state->hasValue || state->hasError; });
        }

        // Get the result, throwing an exception on error
        T get()
        {
            wait();
            std::lock_guard<std::mutex> lock(state->mutex);

            if (state->hasError)
            {
                std::rethrow_exception(state->error);
            }

            return std::move(state->value);
        }

        // Get the result with a timeout, throwing an exception on error or timeout
        template <typename Rep, typename Period>
        T getFor(const std::chrono::duration<Rep, Period> &timeout)
        {
            if (!waitFor(timeout))
            {
                throw std::runtime_error("Future timed out");
            }

            std::lock_guard<std::mutex> lock(state->mutex);

            if (state->hasError)
            {
                std::rethrow_exception(state->error);
            }

            return std::move(state->value);
        }

        // Try to get the result without waiting, returns nullopt if not ready
        std::optional<T> tryGet()
        {
            std::lock_guard<std::mutex> lock(state->mutex);

            if (!state->hasValue)
            {
                return std::nullopt;
            }

            if (state->hasError)
            {
                std::rethrow_exception(state->error);
            }

            return std::move(state->value);
        }

        // Register a continuation to run when this future completes
        template <typename Func>
        auto then(Func &&func)
        {
            using ReturnType = std::invoke_result_t<Func, T>;

            auto promise = std::make_shared<Promise<ReturnType>>();
            auto future = promise->getFuture();

            // Set up the continuation
            std::lock_guard<std::mutex> lock(state->mutex);

            if (state->hasValue)
            {
                // Already has a value, execute immediately
                try
                {
                    if constexpr (std::is_void_v<ReturnType>)
                    {
                        func(state->value);
                        promise->setSuccess();
                    }
                    else
                    {
                        promise->setSuccess(func(state->value));
                    }
                }
                catch (...)
                {
                    promise->setError(std::current_exception());
                }
            }
            else if (state->hasError)
            {
                // Already has an error, propagate it
                promise->setError(state->error);
            }
            else
            {
                // Not ready yet, add to continuations
                state->continuations.push_back([promise, func = std::forward<Func>(func)](State &state)
                                               {
                    try {
                        if (state.hasError) {
                            promise->setError(state.error);
                        } else if constexpr (std::is_void_v<ReturnType>) {
                            func(state.value);
                            promise->setSuccess();
                        } else {
                            promise->setSuccess(func(state.value));
                        }
                    } catch (...) {
                        promise->setError(std::current_exception());
                    } });
            }

            return future;
        }

    public: // Making this public so it can be accessed by Promise<T> and Promise<void>
        struct State
        {
            mutable std::mutex mutex;
            mutable std::condition_variable condition;
            T value;
            std::exception_ptr error;
            bool hasValue = false;
            bool hasError = false;
            std::vector<std::function<void(State &)>> continuations;

            void runContinuations()
            {
                for (auto &continuation : continuations)
                {
                    getGlobalScheduler().schedule([cont = std::move(continuation),
                                                   state = this]()
                                                  { cont(*state); });
                }
                continuations.clear();
            }
        };

        std::shared_ptr<State> state;

        // Friend declarations to allow access to state
        friend class Promise<T>;
    };

    // Specialization for void futures
    template <>
    class Future<void>
    {
    public:
        using ValueType = void;

        Future() : state(std::make_shared<State>()) {}

        bool isReady() const
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            return state->isCompleted || state->hasError;
        }

        void wait() const
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            state->condition.wait(lock, [this]()
                                  { return state->isCompleted || state->hasError; });
        }

        template <typename Rep, typename Period>
        bool waitFor(const std::chrono::duration<Rep, Period> &timeout) const
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            return state->condition.wait_for(lock, timeout, [this]()
                                             { return state->isCompleted || state->hasError; });
        }

        void get()
        {
            wait();
            std::lock_guard<std::mutex> lock(state->mutex);

            if (state->hasError)
            {
                std::rethrow_exception(state->error);
            }
        }

        template <typename Rep, typename Period>
        void getFor(const std::chrono::duration<Rep, Period> &timeout)
        {
            if (!waitFor(timeout))
            {
                throw std::runtime_error("Future timed out");
            }

            std::lock_guard<std::mutex> lock(state->mutex);

            if (state->hasError)
            {
                std::rethrow_exception(state->error);
            }
        }

        template <typename Func>
        auto then(Func &&func)
        {
            using ReturnType = std::invoke_result_t<Func>;

            auto promise = std::make_shared<Promise<ReturnType>>();
            auto future = promise->getFuture();

            std::lock_guard<std::mutex> lock(state->mutex);

            if (state->isCompleted)
            {
                try
                {
                    if constexpr (std::is_void_v<ReturnType>)
                    {
                        func();
                        promise->setSuccess();
                    }
                    else
                    {
                        promise->setSuccess(func());
                    }
                }
                catch (...)
                {
                    promise->setError(std::current_exception());
                }
            }
            else if (state->hasError)
            {
                promise->setError(state->error);
            }
            else
            {
                state->continuations.push_back([promise, func = std::forward<Func>(func)](State &state)
                                               {
                    try {
                        if (state.hasError) {
                            promise->setError(state.error);
                        } else if constexpr (std::is_void_v<ReturnType>) {
                            func();
                            promise->setSuccess();
                        } else {
                            promise->setSuccess(func());
                        }
                    } catch (...) {
                        promise->setError(std::current_exception());
                    } });
            }

            return future;
        }

    public: // Making this public so Promise<void> can access it
        struct State
        {
            mutable std::mutex mutex;
            mutable std::condition_variable condition;
            std::exception_ptr error;
            bool isCompleted = false;
            bool hasError = false;
            std::vector<std::function<void(State &)>> continuations;

            void runContinuations()
            {
                for (auto &continuation : continuations)
                {
                    getGlobalScheduler().schedule([cont = std::move(continuation),
                                                   state = this]()
                                                  { cont(*state); });
                }
                continuations.clear();
            }
        };

        std::shared_ptr<State> state;

        // Friend declarations
        friend class Promise<void>;
    };

    /**
     * A promise that can be fulfilled with a value or an error.
     * This is used to create a future that can be completed later.
     */
    template <typename T>
    class Promise
    {
    public:
        Promise() : future(std::make_shared<typename Future<T>::State>()) {}

        // Get the future associated with this promise
        Future<T> getFuture()
        {
            Future<T> result;
            result.state = future;
            return result;
        }

        // Complete the promise with a value
        void setSuccess(T value)
        {
            std::lock_guard<std::mutex> lock(future->mutex);
            if (future->hasValue || future->hasError)
            {
                throw std::runtime_error("Promise already completed");
            }

            future->value = std::move(value);
            future->hasValue = true;
            future->condition.notify_all();
            future->runContinuations();
        }

        // Complete the promise with an error
        void setError(std::exception_ptr error)
        {
            std::lock_guard<std::mutex> lock(future->mutex);
            if (future->hasValue || future->hasError)
            {
                throw std::runtime_error("Promise already completed");
            }

            future->error = std::move(error);
            future->hasError = true;
            future->condition.notify_all();
            future->runContinuations();
        }

    private:
        std::shared_ptr<typename Future<T>::State> future;
    };

    // Specialization for void promises
    template <>
    class Promise<void>
    {
    public:
        Promise() : future(std::make_shared<Future<void>::State>()) {}

        Future<void> getFuture()
        {
            Future<void> result;
            result.state = future;
            return result;
        }

        void setSuccess()
        {
            std::lock_guard<std::mutex> lock(future->mutex);
            if (future->isCompleted || future->hasError)
            {
                throw std::runtime_error("Promise already completed");
            }

            future->isCompleted = true;
            future->condition.notify_all();
            future->runContinuations();
        }

        void setError(std::exception_ptr error)
        {
            std::lock_guard<std::mutex> lock(future->mutex);
            if (future->isCompleted || future->hasError)
            {
                throw std::runtime_error("Promise already completed");
            }

            future->error = std::move(error);
            future->hasError = true;
            future->condition.notify_all();
            future->runContinuations();
        }

    private:
        std::shared_ptr<Future<void>::State> future;
    };

    /**
     * Utility functions for async operations
     */

    // Creates a completed future with a value
    template <typename T>
    Future<T> makeReadyFuture(T value)
    {
        Promise<T> promise;
        promise.setSuccess(std::move(value));
        return promise.getFuture();
    }

    // Creates a completed void future
    inline Future<void> makeReadyFuture()
    {
        Promise<void> promise;
        promise.setSuccess();
        return promise.getFuture();
    }

    // Creates a failed future with an exception
    template <typename T, typename E>
    Future<T> makeExceptionalFuture(const E &exception)
    {
        Promise<T> promise;
        promise.setException(exception);
        return promise.getFuture();
    }

    // Run a function asynchronously and return a future
    template <typename Func, typename... Args>
    auto async(Func &&func, Args &&...args)
    {
        using ReturnType = std::invoke_result_t<Func, Args...>;
        using FutureType = Future<ReturnType>;

        auto promise = std::make_shared<Promise<ReturnType>>();
        auto future = promise->getFuture();

        getGlobalScheduler().schedule([promise,
                                       func = std::forward<Func>(func),
                                       args = std::make_tuple(std::forward<Args>(args)...)]() mutable
                                      {
            try {
                if constexpr (std::is_void_v<ReturnType>) {
                    std::apply(func, args);
                    promise->setSuccess();
                } else {
                    promise->setSuccess(std::apply(func, args));
                }
            } catch (...) {
                promise->setError(std::current_exception());
            } });

        return future;
    }

    // Wait for all futures to complete
    template <typename... Futures>
    auto whenAll(Futures &&...futures)
    {
        using TupleType = std::tuple<typename std::decay_t<Futures>::ValueType...>;

        auto promise = std::make_shared<Promise<TupleType>>();
        auto result = promise->getFuture();

        auto remainingCount = std::make_shared<std::atomic<size_t>>(sizeof...(Futures));
        auto resultsTuple = std::make_shared<TupleType>();

        auto collectResults = [promise, remainingCount, resultsTuple]()
        {
            if (--(*remainingCount) == 0)
            {
                try
                {
                    promise->setSuccess(std::move(*resultsTuple));
                }
                catch (...)
                {
                    promise->setError(std::current_exception());
                }
            }
        };

        auto processFuture = [collectResults](auto &tuple, size_t index, auto &future)
        {
            future.then([collectResults, &tuple, index](auto value)
                        {
                std::get<index>(tuple) = std::move(value);
                collectResults(); })
                .then([](auto &&) {}); // Prevent exceptions from continuations
        };

        std::apply([&](auto &&...args)
                   { ((processFuture(*resultsTuple, args, std::get<args>(std::forward_as_tuple(futures...)))), ...); }, std::make_index_sequence<sizeof...(Futures)>{});

        return result;
    }

    // Waits for the first future to complete and returns its index and value
    template <typename... Futures>
    auto whenAny(Futures &&...futures)
    {
        using ResultType = std::variant<typename std::decay_t<Futures>::ValueType...>;
        using ReturnType = std::pair<size_t, ResultType>;

        auto promise = std::make_shared<Promise<ReturnType>>();
        auto result = promise->getFuture();

        auto setOnce = std::make_shared<std::atomic<bool>>(false);

        auto processFuture = [promise, setOnce](size_t index, auto &future)
        {
            future.then([promise, setOnce, index](auto value)
                        {
                bool expected = false;
                if (setOnce->compare_exchange_strong(expected, true)) {
                    promise->setSuccess(ReturnType{index, std::move(value)});
                } })
                .then([](auto &&) {}); // Prevent exceptions from continuations
        };

        std::apply([&](auto &&...args)
                   { ((processFuture(args, std::get<args>(std::forward_as_tuple(futures...)))), ...); }, std::make_index_sequence<sizeof...(Futures)>{});

        return result;
    }

    /**
     * A channel for communication between goroutines, similar to Go channels.
     * The channel can be buffered or unbuffered.
     */
    template <typename T>
    class Channel
    {
    public:
        // Create an unbuffered channel
        Channel() : capacity(0), closed(false) {}

        // Create a buffered channel with specified capacity
        explicit Channel(size_t capacity) : capacity(capacity), closed(false) {}

        ~Channel()
        {
            close();
        }

        // Send a value to the channel
        // Blocks if the channel is full
        bool send(T value)
        {
            std::unique_lock<std::mutex> lock(mutex);

            if (closed)
            {
                return false; // Cannot send to a closed channel
            }

            // If unbuffered or buffer full, wait for a receiver
            notFull.wait(lock, [this]
                         { return buffer.size() < capacity || receivers > 0 || closed; });

            if (closed)
            {
                return false;
            }

            if (receivers > 0)
            {
                // Direct send to a waiting receiver
                temp = std::move(value);
                receivers--;
                notEmpty.notify_one();
            }
            else
            {
                // Add to buffer
                buffer.push(std::move(value));
                notEmpty.notify_one();
            }

            return true;
        }

        // Receive a value from the channel
        // Blocks if the channel is empty
        bool receive(T &value)
        {
            std::unique_lock<std::mutex> lock(mutex);

            if (buffer.empty() && closed)
            {
                return false; // Cannot receive from an empty, closed channel
            }

            if (buffer.empty())
            {
                // No value available, wait for a sender
                receivers++;
                notEmpty.wait(lock, [this]
                              { return !buffer.empty() || hasTemp || closed; });

                if (hasTemp)
                {
                    // Direct receive from a sender
                    value = std::move(temp);
                    hasTemp = false;
                    notFull.notify_one();
                    return true;
                }

                if (buffer.empty() && closed)
                {
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
        void close()
        {
            std::lock_guard<std::mutex> lock(mutex);
            closed = true;
            notEmpty.notify_all();
            notFull.notify_all();
        }

        // Check if the channel is closed
        bool isClosed() const
        {
            std::lock_guard<std::mutex> lock(mutex);
            return closed;
        }

        // Check if the channel is empty
        bool isEmpty() const
        {
            std::lock_guard<std::mutex> lock(mutex);
            return buffer.empty() && !hasTemp;
        }

    private:
        size_t capacity;
        std::queue<T> buffer;
        mutable std::mutex mutex;
        std::condition_variable notEmpty;
        std::condition_variable notFull;
        size_t receivers = 0;
        T temp;
        bool hasTemp = false;
        bool closed;
    };

    /**
     * A select statement for waiting on multiple channels.
     */
    class Select
    {
    public:
        Select() = default;

        // Add a case for receiving from a channel
        template <typename T>
        void addReceive(Channel<T> &channel, std::function<void(T)> callback)
        {
            cases.push_back([&channel, callback]()
                            {
                T value;
                if (channel.receive(value)) {
                    callback(std::move(value));
                    return true;
                }
                return false; });
        }

        // Add a case for sending to a channel
        template <typename T>
        void addSend(Channel<T> &channel, T value, std::function<void()> callback)
        {
            cases.push_back([&channel, value = std::move(value), callback]()
                            {
                if (channel.send(value)) {
                    callback();
                    return true;
                }
                return false; });
        }

        // Add a default case
        void addDefault(std::function<void()> callback)
        {
            defaultCase = std::move(callback);
        }

        // Execute the select statement
        bool execute()
        {
            // Try all cases once
            for (auto &c : cases)
            {
                if (c())
                {
                    return true;
                }
            }

            // If we have a default case, run it
            if (defaultCase)
            {
                defaultCase();
                return true;
            }

            // No case ready and no default, wait for a case to become ready
            std::mutex mutex;
            std::condition_variable cv;
            std::atomic<bool> done(false);

            // Spawn a task for each case
            for (auto &c : cases)
            {
                go([&done, &cv, c]()
                   {
                    if (!done.load() && c()) {
                        done.store(true);
                        cv.notify_one();
                    } });
            }

            // Wait for a case to complete
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&done]
                    { return done.load(); });

            return true;
        }

        // Wait for any case to become ready
        void wait()
        {
            execute();
        }

    private:
        std::vector<std::function<bool()>> cases;
        std::function<void()> defaultCase;
    };

    /**
     * @brief Describes a channel type
     *
     * A channel is a typed conduit through which you can send and receive values.
     * This class is used to represent channel types during compilation.
     */
    class ChannelType
    {
    public:
        static const std::string TYPE_NAME;

        /**
         * @brief Check if a type is a channel type
         *
         * @param type The type to check
         * @return true if the type is a channel
         */
        static bool isChannelType(ast::TypePtr type)
        {
            if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type))
            {
                return genericType->name == TYPE_NAME;
            }
            return false;
        }

        /**
         * @brief Create a channel type for the given element type
         *
         * @param elementType The type of values that can be sent/received on this channel
         * @return ast::TypePtr The channel type
         */
        static ast::TypePtr createChannelType(ast::TypePtr elementType)
        {
            return std::make_shared<ast::GenericType>(ast::DEFAULT_TOKEN, TYPE_NAME, std::vector<ast::TypePtr>{elementType});
        }

        /**
         * @brief Get the element type from a channel type
         *
         * @param channelType The channel type
         * @return ast::TypePtr The element type
         */
        static ast::TypePtr getElementType(ast::TypePtr channelType)
        {
            if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(channelType))
            {
                if (genericType->name == TYPE_NAME && !genericType->typeArguments.empty())
                {
                    return genericType->typeArguments[0];
                }
            }
            return nullptr;
        }
    };

    // Define the static const member
    const std::string ChannelType::TYPE_NAME = "Chan";

    /**
     * @brief Runtime features for goroutine support
     *
     * Provides methods for creating, managing, and analyzing goroutines.
     */
    class GoroutineSupport
    {
    public:
        GoroutineSupport(error::ErrorHandler &errorHandler)
            : errorHandler(errorHandler) {}

        /**
         * @brief Check if a function is eligible to be run as a goroutine
         *
         * @param function The function to check
         * @return true if the function can be used as a goroutine
         */
        bool canRunAsGoroutine(ast::FunctionStmt *function)
        {
            // All functions can be run as goroutines
            return true;
        }

        /**
         * @brief Analyze a goroutine launch to ensure it's valid
         *
         * @param function The function being launched as a goroutine
         * @param arguments The arguments passed to the function
         * @return true if the goroutine launch is valid
         */
        bool validateGoroutineLaunch(ast::ExprPtr function, const std::vector<ast::ExprPtr> &arguments)
        {
            if (!function)
            {
                errorHandler.reportError(
                    error::ErrorCode::INVALID_GOROUTINE,
                    "Invalid goroutine launch: null function expression",
                    error::ErrorSeverity::ERROR);
                return false;
            }

            if (auto callExpr = std::dynamic_pointer_cast<ast::CallExpr>(function))
            {
                // Handle call expression
                return true;
            }
            else if (auto varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(function))
            {
                // Handle variable expression
                return true;
            }
            else
            {
                errorHandler.reportError(
                    error::ErrorCode::INVALID_GOROUTINE,
                    "Invalid goroutine launch: expression must be a function call or reference",
                    error::ErrorSeverity::ERROR);
                return false;
            }
        }

    private:
        error::ErrorHandler &errorHandler;
    };

    /**
     * @brief Runtime features for channel operations
     *
     * Provides methods for creating and operating on channels.
     */
    class ChannelSupport
    {
    public:
        ChannelSupport(error::ErrorHandler &errorHandler)
            : errorHandler(errorHandler) {}

        bool validateChannelSend(ast::ExprPtr channel, ast::ExprPtr value, ast::TypePtr channelType, ast::TypePtr valueType)
        {
            if (!channel)
            {
                errorHandler.reportError(
                    error::ErrorCode::INVALID_CHANNEL_OPERATION,
                    "Invalid channel send: null channel expression",
                    error::ErrorSeverity::ERROR);
                return false;
            }

            if (!value)
            {
                errorHandler.reportError(
                    error::ErrorCode::INVALID_CHANNEL_OPERATION,
                    "Invalid channel send: null value expression",
                    error::ErrorSeverity::ERROR);
                return false;
            }

            if (!channelType || !valueType)
            {
                errorHandler.reportError(
                    error::ErrorCode::INVALID_CHANNEL_OPERATION,
                    "Invalid channel send: null type information",
                    error::ErrorSeverity::ERROR);
                return false;
            }
            return true;
        }

        bool validateChannelReceive(ast::ExprPtr channel, ast::TypePtr channelType)
        {
            if (!channel)
            {
                errorHandler.reportError(
                    error::ErrorCode::INVALID_CHANNEL_OPERATION,
                    "Invalid channel receive: null channel expression",
                    error::ErrorSeverity::ERROR);
                return false;
            }

            if (!channelType)
            {
                errorHandler.reportError(
                    error::ErrorCode::INVALID_CHANNEL_OPERATION,
                    "Invalid channel receive: null channel type",
                    error::ErrorSeverity::ERROR);
                return false;
            }
            return true;
        }

    private:
        error::ErrorHandler &errorHandler;
    };

    /**
     * @brief AST node for a goroutine launch expression
     *
     * Represents the 'go' keyword followed by a function call.
     */
    class GoExpr : public ast::Expression
    {
    public:
        GoExpr(const lexer::Token &token, ast::ExprPtr funcExpr,
               std::vector<ast::ExprPtr> arguments)
            : Expression(token), funcExpr(funcExpr), arguments(std::move(arguments)) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            visitor.visitGoExpr(static_cast<void *>(this));
        }

        virtual ast::TypePtr getType() const override
        {
            return nullptr; // Goroutine launch returns void
        }

        ast::ExprPtr getFuncExpr() const { return funcExpr; }
        const std::vector<ast::ExprPtr> &getArguments() const { return arguments; }

    private:
        ast::ExprPtr funcExpr;
        std::vector<ast::ExprPtr> arguments;
    };

    /**
     * @brief AST node for a channel send expression
     *
     * Represents sending a value on a channel (channel <- value).
     */
    class ChannelSendExpr : public ast::Expression
    {
    public:
        ChannelSendExpr(const lexer::Token &token, ast::ExprPtr channel, ast::ExprPtr value)
            : Expression(token), channel(channel), value(value) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            visitor.visitRuntimeChannelSendExpr(static_cast<void *>(this));
        }

        virtual ast::TypePtr getType() const override
        {
            return std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
        }

        ast::ExprPtr getChannel() const { return channel; }
        ast::ExprPtr getValue() const { return value; }

    private:
        ast::ExprPtr channel;
        ast::ExprPtr value;
    };

    /**
     * @brief AST node for a channel receive expression
     *
     * Represents receiving a value from a channel (<- channel).
     */
    class ChannelReceiveExpr : public ast::Expression
    {
    public:
        ChannelReceiveExpr(const lexer::Token &token, ast::ExprPtr channel)
            : Expression(token), channel(channel) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            visitor.visitRuntimeChannelReceiveExpr(this);
        }

        virtual ast::TypePtr getType() const override
        {
            return nullptr;
        }

        ast::ExprPtr getChannel() const { return channel; }

    private:
        ast::ExprPtr channel;
    };

    /**
     * @brief AST node for a select statement
     *
     * Represents a Go-like select statement for handling multiple channel operations.
     */
    class SelectStmt : public ast::Statement
    {
    public:
        struct SelectCase
        {
            enum class CaseType
            {
                SEND,
                RECEIVE,
                DEFAULT
            };

            CaseType type;
            ast::ExprPtr channel = nullptr;
            ast::ExprPtr value = nullptr;
            std::string variableName = "";
            ast::StmtPtr body = nullptr;
        };

        std::vector<SelectCase> cases;

        SelectStmt(const lexer::Token &token, std::vector<SelectCase> cases)
            : Statement(token), cases(std::move(cases)) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            visitor.visitRuntimeSelectStmt(this);
        }
    };

} // namespace runtime

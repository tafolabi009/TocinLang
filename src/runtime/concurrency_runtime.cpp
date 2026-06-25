// Tocin runtime: goroutines (OS threads), channels, and exception handling.
//
// These symbols are linked into the compiler/runtime and resolved by the JIT
// from the running process, and linked into native executables via the normal
// C toolchain. Values flowing through channels are passed as 64-bit slots
// (ints, bit-cast floats, or pointers), matching the codegen ABI.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    struct Channel
    {
        std::mutex m;
        std::condition_variable cv;
        std::deque<int64_t> q;
    };

    // Track spawned goroutines so the program can wait for them to finish.
    std::mutex g_threadsMutex;
    std::vector<std::thread> g_threads;
}

extern "C"
{
    // Allocate a new channel and return an opaque handle.
    void *__tocin_chan_new()
    {
        return new Channel();
    }

    // Send a 64-bit value into the channel and wake a waiting receiver.
    void __tocin_chan_send(void *handle, int64_t value)
    {
        if (!handle)
            return;
        auto *ch = static_cast<Channel *>(handle);
        {
            std::lock_guard<std::mutex> lock(ch->m);
            ch->q.push_back(value);
        }
        ch->cv.notify_one();
    }

    // Receive a 64-bit value, blocking until one is available.
    int64_t __tocin_chan_recv(void *handle)
    {
        if (!handle)
            return 0;
        auto *ch = static_cast<Channel *>(handle);
        std::unique_lock<std::mutex> lock(ch->m);
        ch->cv.wait(lock, [ch] { return !ch->q.empty(); });
        int64_t value = ch->q.front();
        ch->q.pop_front();
        return value;
    }

    // Non-blocking receive: if a value is available, dequeue it into *out and
    // return 1; otherwise return 0 without blocking. Used to implement `select`.
    int8_t __tocin_chan_try_recv(void *handle, int64_t *out)
    {
        if (!handle || !out)
            return 0;
        auto *ch = static_cast<Channel *>(handle);
        std::lock_guard<std::mutex> lock(ch->m);
        if (ch->q.empty())
            return 0;
        *out = ch->q.front();
        ch->q.pop_front();
        return 1;
    }

    // Briefly sleep to avoid a hot busy-wait in a blocking `select` poll loop.
    void __tocin_chan_park()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void __tocin_join_all();

    // Spawn a goroutine: run fn(arg) on a new OS thread, tracked for joining.
    void __tocin_go(void (*fn)(void *), void *arg)
    {
        if (!fn)
            return;
        // Ensure all goroutines are joined when the program exits.
        static std::once_flag atexitFlag;
        std::call_once(atexitFlag, [] { std::atexit(__tocin_join_all); });
        std::lock_guard<std::mutex> lock(g_threadsMutex);
        g_threads.emplace_back(fn, arg);
    }

    // Wait for all spawned goroutines to finish.
    void __tocin_join_all()
    {
        std::vector<std::thread> threads;
        {
            std::lock_guard<std::mutex> lock(g_threadsMutex);
            threads.swap(g_threads);
        }
        for (auto &t : threads)
            if (t.joinable())
                t.join();
    }
}

// ---------------------------------------------------------------------------
// Exception handling: setjmp/longjmp-based unwinding.
//
// Each `try` allocates a jmp_buf in its own frame, calls setjmp, and registers
// the buffer with __tocin_try_register. `throw` records a 64-bit value and
// longjmps back to the most recently registered handler. The handler stack is
// thread-local so goroutines have independent exception state.
// ---------------------------------------------------------------------------
namespace
{
    thread_local std::vector<std::jmp_buf *> g_handlerStack;
    thread_local int64_t g_excValue = 0;
}

extern "C"
{
    // Register a setjmp buffer as the innermost active exception handler.
    void __tocin_try_register(void *buf)
    {
        g_handlerStack.push_back(static_cast<std::jmp_buf *>(buf));
    }

    // Remove the innermost handler (try body completed without throwing).
    void __tocin_try_pop()
    {
        if (!g_handlerStack.empty())
            g_handlerStack.pop_back();
    }

    // Return the value carried by the most recently thrown exception.
    int64_t __tocin_exc_value()
    {
        return g_excValue;
    }

    // Throw: record the value and unwind to the nearest handler via longjmp.
    // With no active handler the exception is fatal.
    void __tocin_throw(int64_t value)
    {
        g_excValue = value;
        if (g_handlerStack.empty())
        {
            std::fprintf(stderr,
                         "Tocin: uncaught exception (value=%lld)\n",
                         static_cast<long long>(value));
            std::abort();
        }
        std::jmp_buf *buf = g_handlerStack.back();
        g_handlerStack.pop_back();
        std::longjmp(*buf, 1);
    }
}

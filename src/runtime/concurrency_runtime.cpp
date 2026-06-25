// Tocin concurrency runtime: goroutines (OS threads) and channels.
//
// These symbols are linked into the compiler/runtime and resolved by the JIT
// from the running process, and linked into native executables via the normal
// C toolchain. Values flowing through channels are passed as 64-bit slots
// (ints, bit-cast floats, or pointers), matching the codegen ABI.
#include <atomic>
#include <condition_variable>
#include <cstdint>
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

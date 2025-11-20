/**
 * Parallel Runtime for Tocin Compiler
 * Provides implementation of parallel execution primitives
 */

#include <cstdint>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <future>
#include <mutex>

namespace tocin {
namespace runtime {

// Thread pool for parallel execution
class ParallelRuntime {
private:
    static std::vector<std::thread> thread_pool_;
    static std::atomic<bool> initialized_;
    static std::mutex init_mutex_;
    static size_t num_threads_;
    
public:
    static void initialize(size_t num_threads = 0) {
        std::lock_guard<std::mutex> lock(init_mutex_);
        if (initialized_.load()) {
            return;
        }
        
        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) num_threads = 4;
        }
        
        num_threads_ = num_threads;
        initialized_.store(true);
    }
    
    static void shutdown() {
        std::lock_guard<std::mutex> lock(init_mutex_);
        initialized_.store(false);
    }
    
    static size_t get_num_threads() {
        return num_threads_;
    }
};

std::vector<std::thread> ParallelRuntime::thread_pool_;
std::atomic<bool> ParallelRuntime::initialized_(false);
std::mutex ParallelRuntime::init_mutex_;
size_t ParallelRuntime::num_threads_ = 0;

} // namespace runtime
} // namespace tocin

// C interface for LLVM-generated code
extern "C" {

/**
 * Parallel for loop implementation
 * @param start Starting index (inclusive)
 * @param end Ending index (exclusive)
 * @param body Function pointer to loop body, takes index as parameter
 */
void __tocin_parallel_for(int64_t start, int64_t end, void (*body)(int64_t)) {
    using namespace tocin::runtime;
    
    if (!ParallelRuntime::initialized_.load()) {
        ParallelRuntime::initialize();
    }
    
    if (!body || start >= end) {
        return;
    }
    
    size_t num_threads = ParallelRuntime::get_num_threads();
    int64_t range = end - start;
    
    // If range is too small, just execute serially
    if (range < static_cast<int64_t>(num_threads) * 2) {
        for (int64_t i = start; i < end; ++i) {
            body(i);
        }
        return;
    }
    
    // Divide work among threads
    std::vector<std::future<void>> futures;
    futures.reserve(num_threads);
    
    int64_t chunk_size = range / num_threads;
    int64_t remainder = range % num_threads;
    
    for (size_t thread_id = 0; thread_id < num_threads; ++thread_id) {
        int64_t thread_start = start + thread_id * chunk_size + std::min(static_cast<int64_t>(thread_id), remainder);
        int64_t thread_end = thread_start + chunk_size + (thread_id < remainder ? 1 : 0);
        
        if (thread_start >= end) {
            break;
        }
        
        futures.push_back(std::async(std::launch::async, [body, thread_start, thread_end]() {
            for (int64_t i = thread_start; i < thread_end; ++i) {
                body(i);
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
}

/**
 * Parallel for loop with step
 */
void __tocin_parallel_for_step(int64_t start, int64_t end, int64_t step, void (*body)(int64_t)) {
    if (!body || step == 0 || (step > 0 && start >= end) || (step < 0 && start <= end)) {
        return;
    }
    
    int64_t num_iterations = (end - start + step - (step > 0 ? 1 : -1)) / step;
    
    __tocin_parallel_for(0, num_iterations, [start, step, body](int64_t i) {
        body(start + i * step);
    });
}

/**
 * Initialize the parallel runtime explicitly
 */
void __tocin_parallel_init(int64_t num_threads) {
    tocin::runtime::ParallelRuntime::initialize(num_threads > 0 ? num_threads : 0);
}

/**
 * Shutdown the parallel runtime
 */
void __tocin_parallel_shutdown() {
    tocin::runtime::ParallelRuntime::shutdown();
}

} // extern "C"

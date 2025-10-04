// Lightweight Scheduler Tests for Tocin Compiler

#include "../../src/runtime/lightweight_scheduler.h"
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>

using namespace tocin::runtime;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running test: " #name "..."; \
    test_##name(); \
    std::cout << " PASSED\n"; \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        std::cerr << "Assertion failed: " #expr << "\n"; \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

TEST(scheduler_init) {
    LightweightScheduler scheduler(4);
    scheduler.start();
    auto stats = scheduler.getStats();
    ASSERT_EQ(stats.totalWorkers, 4);
    scheduler.stop();
}

TEST(single_goroutine) {
    LightweightScheduler scheduler(2);
    scheduler.start();
    std::atomic<int> counter{0};
    scheduler.go([&counter]() { counter++; });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(counter.load(), 1);
    scheduler.stop();
}

TEST(multiple_goroutines) {
    LightweightScheduler scheduler(4);
    scheduler.start();
    std::atomic<int> counter{0};
    for (int i = 0; i < 100; i++) {
        scheduler.go([&counter]() { counter++; });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ASSERT_EQ(counter.load(), 100);
    scheduler.stop();
}

int main() {
    std::cout << "=== Lightweight Scheduler Tests ===\n\n";
    RUN_TEST(scheduler_init);
    RUN_TEST(single_goroutine);
    RUN_TEST(multiple_goroutines);
    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}

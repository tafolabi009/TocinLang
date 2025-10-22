#include "concurrency.h"
#include <memory>

namespace runtime {

// Global scheduler instance
std::unique_ptr<Scheduler> global_scheduler = nullptr;

void initializeScheduler() {
    if (!global_scheduler) {
        global_scheduler = std::make_unique<Scheduler>();
    }
}

Scheduler& getScheduler() {
    if (!global_scheduler) {
        initializeScheduler();
    }
    return *global_scheduler;
}

} // namespace runtime

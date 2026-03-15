// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — background_scanner.cpp                                         ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/threading/background_scanner.hpp"

#include <algorithm>

namespace memsentry {

BackgroundScanner::~BackgroundScanner() {
    stop();
}

void BackgroundScanner::start(std::chrono::milliseconds interval,
                               ScanCallback callback)
{
    stop();  // Ensure no existing worker

    callback_ = std::move(callback);
    running_  = true;

#ifdef MEMSENTRY_HAS_JTHREAD
    // C++20 jthread: automatically joins on destruction, cooperative cancellation
    worker_ = std::jthread([this, interval](std::stop_token st) {
        scan_loop(st, interval);
    });
#else
    stop_requested_.store(false);
    worker_ = std::thread([this, interval]() {
        scan_loop(interval);
    });
#endif
}

void BackgroundScanner::stop() {
    if (!running_) return;

#ifdef MEMSENTRY_HAS_JTHREAD
    worker_.request_stop();
    if (worker_.joinable()) worker_.join();
#else
    stop_requested_.store(true);
    if (worker_.joinable()) worker_.join();
#endif

    running_ = false;
}

auto BackgroundScanner::is_running() const -> bool {
    return running_;
}

auto BackgroundScanner::last_statistics() const -> MemoryStatistics {
    std::lock_guard lock(stats_mutex_);
    return last_stats_;
}

void BackgroundScanner::scan_loop(
#ifdef MEMSENTRY_HAS_JTHREAD
    std::stop_token stop_token,
#endif
    std::chrono::milliseconds interval)
{
    auto& tracker = AllocationTracker::instance();

    while (true) {
#ifdef MEMSENTRY_HAS_JTHREAD
        // C++20 cooperative cancellation
        if (stop_token.stop_requested()) break;
#else
        if (stop_requested_.load()) break;
#endif

        // Collect statistics
        MemoryStatistics stats;
        stats.active_allocations    = tracker.active_allocation_count();
        stats.total_bytes_allocated = tracker.total_allocated_bytes();
        stats.peak_bytes_allocated  = tracker.peak_allocated_bytes();

        auto leaked = tracker.get_leaked_allocations();
        stats.leaked_allocations = leaked.size();

        std::size_t leaked_bytes = 0;
        for (const auto& record : leaked) {
            leaked_bytes += record.size;
        }
        stats.leaked_bytes = leaked_bytes;

        {
            std::lock_guard lock(stats_mutex_);
            last_stats_ = stats;
        }

        // Notify callback
        if (callback_) {
            callback_(stats);
        }

        // Sleep with periodic wake-up check
        auto wake_time = std::chrono::steady_clock::now() + interval;
        while (std::chrono::steady_clock::now() < wake_time) {
#ifdef MEMSENTRY_HAS_JTHREAD
            if (stop_token.stop_requested()) return;
#else
            if (stop_requested_.load()) return;
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

} // namespace memsentry

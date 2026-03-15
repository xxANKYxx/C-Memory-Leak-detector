// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — background_scanner.hpp                                         ║
// ║  std::jthread + std::stop_token background leak scanner                     ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/core/allocation_tracker.hpp"
#include "memsentry/reporting/report_types.hpp"

#include <chrono>
#include <functional>
#include <mutex>

#ifdef MEMSENTRY_HAS_JTHREAD
    #include <stop_token>
    #include <thread>
#else
    #include <atomic>
    #include <thread>
#endif

namespace memsentry {

// ─── Background Scanner ─────────────────────────────────────────────────────
// Periodically scans for anomalies using C++20 std::jthread with
// cooperative cancellation via std::stop_token.
class BackgroundScanner {
public:
    using ScanCallback = std::function<void(const MemoryStatistics&)>;

    BackgroundScanner() = default;
    ~BackgroundScanner();

    BackgroundScanner(const BackgroundScanner&) = delete;
    BackgroundScanner& operator=(const BackgroundScanner&) = delete;

    // Start the scanner with a given interval
    void start(std::chrono::milliseconds interval, ScanCallback callback = nullptr);

    // Stop the scanner (cooperative cancellation)
    void stop();

    [[nodiscard]] auto is_running() const -> bool;

    // Get the latest scan result
    [[nodiscard]] auto last_statistics() const -> MemoryStatistics;

private:
    void scan_loop(
#ifdef MEMSENTRY_HAS_JTHREAD
        std::stop_token stop_token,
#endif
        std::chrono::milliseconds interval
    );

#ifdef MEMSENTRY_HAS_JTHREAD
    std::jthread worker_;
#else
    std::thread worker_;
    std::atomic<bool> stop_requested_{false};
#endif

    ScanCallback callback_;
    mutable std::mutex stats_mutex_;
    MemoryStatistics last_stats_;
    bool running_ = false;
};

} // namespace memsentry

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — Modern C++20 Memory Leak Detector                              ║
// ║                                                                              ║
// ║  memsentry.hpp — Single public header / Façade                              ║
// ║                                                                              ║
// ║  Usage:                                                                      ║
// ║    #include <memsentry/memsentry.hpp>                                        ║
// ║                                                                              ║
// ║    int main() {                                                              ║
// ║        memsentry::MemSentryGuard guard;  // RAII: init on construct,         ║
// ║        // ... your code ...              //       report + shutdown on exit   ║
// ║    }                                                                         ║
// ║                                                                              ║
// ║  No macros needed — std::source_location captures call sites automatically. ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

// ── Core ─────────────────────────────────────────────────────────────────────
#include "memsentry/core/config.hpp"
#include "memsentry/core/allocation_record.hpp"
#include "memsentry/core/allocation_tracker.hpp"
#include "memsentry/core/allocator_hooks.hpp"
#include "memsentry/core/shadow_memory.hpp"

// ── Detectors ────────────────────────────────────────────────────────────────
#include "memsentry/detectors/detector_base.hpp"
#include "memsentry/detectors/leak_detector.hpp"
#include "memsentry/detectors/double_free_detector.hpp"
#include "memsentry/detectors/use_after_free_detector.hpp"
#include "memsentry/detectors/overflow_detector.hpp"
#include "memsentry/detectors/mismatch_detector.hpp"
#include "memsentry/detectors/uninit_detector.hpp"
#include "memsentry/detectors/invalid_free_detector.hpp"
#include "memsentry/detectors/cycle_detector.hpp"
#include "memsentry/detectors/fragmentation_analyzer.hpp"

// ── Capture ──────────────────────────────────────────────────────────────────
#include "memsentry/capture/stacktrace_capture.hpp"

// ── Reporting ────────────────────────────────────────────────────────────────
#include "memsentry/reporting/report_types.hpp"
#include "memsentry/reporting/report_engine.hpp"
#include "memsentry/reporting/console_reporter.hpp"
#include "memsentry/reporting/file_reporter.hpp"
#include "memsentry/reporting/statistics.hpp"

// ── Threading ────────────────────────────────────────────────────────────────
#include "memsentry/threading/thread_safe_map.hpp"
#include "memsentry/threading/lock_free_queue.hpp"
#include "memsentry/threading/background_scanner.hpp"

// ── Platform ─────────────────────────────────────────────────────────────────
#include "memsentry/platform/platform.hpp"

#include <memory>
#include <vector>

namespace memsentry {

// ═══════════════════════════════════════════════════════════════════════════════
// MemSentry — Main API
// ═══════════════════════════════════════════════════════════════════════════════

class MemSentry {
public:
    // ── Lifecycle ────────────────────────────────────────────────────────────
    static void init(Config cfg = Config::default_config());
    static void shutdown();

    // ── On-demand reporting ──────────────────────────────────────────────────
    static auto report() -> FullReport;
    static auto get_statistics() -> MemoryStatistics;

    // ── Detector access (for advanced usage) ─────────────────────────────────
    static auto leak_detector()          -> LeakDetector&;
    static auto double_free_detector()   -> DoubleFreeDetector&;
    static auto use_after_free_detector()-> UseAfterFreeDetector&;
    static auto overflow_detector()      -> OverflowDetector&;
    static auto mismatch_detector()      -> MismatchDetector&;
    static auto uninit_detector()        -> UninitDetector&;
    static auto invalid_free_detector()  -> InvalidFreeDetector&;
    static auto cycle_detector()         -> CycleDetector&;
    static auto fragmentation_analyzer() -> FragmentationAnalyzer&;

    // ── Report Engine ────────────────────────────────────────────────────────
    static auto report_engine() -> ReportEngine&;

    // ── Background Scanner ───────────────────────────────────────────────────
    static auto background_scanner() -> BackgroundScanner&;

private:
    struct State {
        Config                  config;
        LeakDetector            leak;
        DoubleFreeDetector      double_free;
        UseAfterFreeDetector    use_after_free;
        OverflowDetector        overflow;
        MismatchDetector        mismatch;
        UninitDetector          uninit;
        InvalidFreeDetector     invalid_free;
        CycleDetector           cycle;
        FragmentationAnalyzer   fragmentation;
        ReportEngine            report_engine;
        BackgroundScanner       scanner;
        bool                    initialized = false;
    };

    static auto state() -> State&;
};

// ═══════════════════════════════════════════════════════════════════════════════
// MemSentryGuard — RAII guard
// Constructor calls init(), destructor calls shutdown().
// ═══════════════════════════════════════════════════════════════════════════════

class MemSentryGuard {
public:
    explicit MemSentryGuard(Config cfg = Config::default_config()) {
        MemSentry::init(std::move(cfg));
    }

    ~MemSentryGuard() {
        auto report = MemSentry::report();
        MemSentry::report_engine().output(report);
        MemSentry::shutdown();
    }

    MemSentryGuard(const MemSentryGuard&) = delete;
    MemSentryGuard& operator=(const MemSentryGuard&) = delete;
    MemSentryGuard(MemSentryGuard&&) = delete;
    MemSentryGuard& operator=(MemSentryGuard&&) = delete;
};

} // namespace memsentry

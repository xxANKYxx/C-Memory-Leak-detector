// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — memsentry.cpp (main API implementation)                        ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
// This file is placed under src/core/ as the central initialization logic.

#include "memsentry/memsentry.hpp"

#include <chrono>
#include <iostream>

namespace memsentry {

auto MemSentry::state() -> State& {
    static State s;
    return s;
}

void MemSentry::init(Config cfg) {
    auto& s = state();
    s.config = std::move(cfg);

    // Configure the allocation tracker
    AllocationTracker::instance().configure(s.config);

    // Configure detectors based on config
    if (!s.config.enable_leak_detection)          s.leak.disable();
    if (!s.config.enable_double_free_detection)   s.double_free.disable();
    if (!s.config.enable_use_after_free)          s.use_after_free.disable();
    if (!s.config.enable_overflow_detection)       s.overflow.disable();
    if (!s.config.enable_mismatch_detection)       s.mismatch.disable();
    if (!s.config.enable_uninit_detection)         s.uninit.disable();
    if (!s.config.enable_invalid_free_detection)   s.invalid_free.disable();
    if (!s.config.enable_cycle_detection)          s.cycle.disable();
    if (!s.config.enable_fragmentation_analysis)   s.fragmentation.disable();

    // Set up the report engine based on config
    switch (s.config.report_format) {
        case ReportFormat::JSON:
            s.report_engine.set_reporter(std::make_unique<JsonReporter>());
            break;
        case ReportFormat::HTML:
            s.report_engine.set_reporter(std::make_unique<HtmlReporter>());
            break;
        case ReportFormat::Text:
        default:
            s.report_engine.set_reporter(std::make_unique<ConsoleReporter>());
            break;
    }

    // Default output sink: stderr
    s.report_engine.add_output_sink([](const std::string& output) {
        std::cerr << output;
    });

    // File output if configured
    if (s.config.report_output_path.has_value()) {
        auto path = s.config.report_output_path.value();
        s.report_engine.add_output_sink([path](const std::string& output) {
            FileReporter::write_to_file(output, path);
        });
    }

    // Start background scanner if interval > 0
    if (s.config.scanner_interval_ms > 0) {
        s.scanner.start(std::chrono::milliseconds(s.config.scanner_interval_ms));
    }

    // Enable global hooks (this activates tracking)
    enable_hooks();
    s.initialized = true;
}

void MemSentry::shutdown() {
    auto& s = state();
    if (!s.initialized) return;

    // Disable hooks first
    disable_hooks();

    // Stop background scanner
    s.scanner.stop();

    // Reset detectors
    s.leak.reset();
    s.double_free.reset();
    s.use_after_free.reset();
    s.overflow.reset();
    s.mismatch.reset();
    s.uninit.reset();
    s.invalid_free.reset();
    s.cycle.reset();
    s.fragmentation.reset();

    // Reset tracker and shadow memory
    AllocationTracker::instance().reset();
    ShadowMemory::instance().reset();

    s.initialized = false;
}

auto MemSentry::report() -> FullReport {
    auto& s = state();

    // Collect reports from all active detectors
    std::vector<DetectorReport> reports;

    if (s.leak.is_enabled())          reports.push_back(s.leak.generate_report());
    if (s.double_free.is_enabled())   reports.push_back(s.double_free.generate_report());
    if (s.use_after_free.is_enabled())reports.push_back(s.use_after_free.generate_report());
    if (s.overflow.is_enabled())      reports.push_back(s.overflow.generate_report());
    if (s.mismatch.is_enabled())      reports.push_back(s.mismatch.generate_report());
    if (s.uninit.is_enabled())        reports.push_back(s.uninit.generate_report());
    if (s.invalid_free.is_enabled())  reports.push_back(s.invalid_free.generate_report());
    if (s.cycle.is_enabled())         reports.push_back(s.cycle.generate_report());
    if (s.fragmentation.is_enabled()) reports.push_back(s.fragmentation.generate_report());

    // Collect statistics
    auto stats = StatisticsCollector::collect(
        AllocationTracker::instance(),
        s.fragmentation.is_enabled() ? &s.fragmentation : nullptr
    );

    return s.report_engine.generate(reports, stats);
}

auto MemSentry::get_statistics() -> MemoryStatistics {
    auto& s = state();
    return StatisticsCollector::collect(
        AllocationTracker::instance(),
        s.fragmentation.is_enabled() ? &s.fragmentation : nullptr
    );
}

auto MemSentry::leak_detector()           -> LeakDetector&          { return state().leak; }
auto MemSentry::double_free_detector()    -> DoubleFreeDetector&    { return state().double_free; }
auto MemSentry::use_after_free_detector() -> UseAfterFreeDetector&  { return state().use_after_free; }
auto MemSentry::overflow_detector()       -> OverflowDetector&      { return state().overflow; }
auto MemSentry::mismatch_detector()       -> MismatchDetector&      { return state().mismatch; }
auto MemSentry::uninit_detector()         -> UninitDetector&        { return state().uninit; }
auto MemSentry::invalid_free_detector()   -> InvalidFreeDetector&   { return state().invalid_free; }
auto MemSentry::cycle_detector()          -> CycleDetector&         { return state().cycle; }
auto MemSentry::fragmentation_analyzer()  -> FragmentationAnalyzer& { return state().fragmentation; }
auto MemSentry::report_engine()           -> ReportEngine&          { return state().report_engine; }
auto MemSentry::background_scanner()      -> BackgroundScanner&     { return state().scanner; }

} // namespace memsentry

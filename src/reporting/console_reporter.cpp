// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — console_reporter.cpp                                           ║
// ║  Human-readable text reporting with std::format                             ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/reporting/console_reporter.hpp"
#include "memsentry/reporting/statistics.hpp"

#include <sstream>

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

auto ConsoleReporter::format_report(const FullReport& report) -> std::string {
    std::ostringstream oss;

    // Header
    oss << "\n";
    oss << "╔══════════════════════════════════════════════════════════════════════╗\n";
    oss << "║                     MemSentry Analysis Report                       ║\n";
    oss << "╠══════════════════════════════════════════════════════════════════════╣\n";

#ifdef MEMSENTRY_HAS_FORMAT
    oss << std::format("║  Timestamp    : {:<52}║\n", report.timestamp);
    oss << std::format("║  Status       : {:<52}║\n",
                       report.all_passed() ? "✓ PASS" : "✗ FAIL");
    oss << std::format("║  Violations   : {:<52}║\n", report.total_violations());
#else
    oss << "║  Timestamp    : " << report.timestamp << "\n";
    oss << "║  Status       : " << (report.all_passed() ? "PASS" : "FAIL") << "\n";
    oss << "║  Violations   : " << report.total_violations() << "\n";
#endif
    oss << "╠══════════════════════════════════════════════════════════════════════╣\n";

    // Statistics
    oss << "║  MEMORY STATISTICS                                                  ║\n";
    oss << "╠══════════════════════════════════════════════════════════════════════╣\n";

    const auto& stats = report.statistics;
#ifdef MEMSENTRY_HAS_FORMAT
    oss << std::format("║  Total allocations   : {:<46}║\n", stats.total_allocations);
    oss << std::format("║  Total deallocations : {:<46}║\n", stats.total_deallocations);
    oss << std::format("║  Active allocations  : {:<46}║\n", stats.active_allocations);
    oss << std::format("║  Peak memory         : {:<46}║\n",
                       StatisticsCollector::format_bytes(stats.peak_bytes_allocated));
    oss << std::format("║  Current in use      : {:<46}║\n",
                       StatisticsCollector::format_bytes(stats.current_bytes_in_use));
    oss << std::format("║  Leaked              : {} allocations ({})          ║\n",
                       stats.leaked_allocations,
                       StatisticsCollector::format_bytes(stats.leaked_bytes));
    oss << std::format("║  Fragmentation index : {:.2f}                         ║\n",
                       stats.fragmentation_index);
#else
    oss << "║  Total allocations   : " << stats.total_allocations << "\n";
    oss << "║  Total deallocations : " << stats.total_deallocations << "\n";
    oss << "║  Active allocations  : " << stats.active_allocations << "\n";
    oss << "║  Peak memory         : " << StatisticsCollector::format_bytes(stats.peak_bytes_allocated) << "\n";
    oss << "║  Current in use      : " << StatisticsCollector::format_bytes(stats.current_bytes_in_use) << "\n";
    oss << "║  Leaked allocations  : " << stats.leaked_allocations << "\n";
    oss << "║  Fragmentation index : " << stats.fragmentation_index << "\n";
#endif
    oss << "╠══════════════════════════════════════════════════════════════════════╣\n";

    // Per-detector results
    for (const auto& det_report : report.detector_reports) {
        oss << "║  [" << det_report.detector_name << "] ";
        if (det_report.passed) {
            oss << "PASSED";
        } else {
#ifdef MEMSENTRY_HAS_FORMAT
            oss << std::format("FAILED — {} violation(s)", det_report.violation_count());
#else
            oss << "FAILED — " << det_report.violation_count() << " violation(s)";
#endif
        }
        oss << "\n";

        for (const auto& v : det_report.violations) {
            oss << "║    [" << severity_name(v.severity) << "] "
                << v.category << ": " << v.description << "\n";

            if (!v.source_location.empty()) {
                oss << "║      at " << v.source_location << "\n";
            }
            if (!v.stack_trace.empty()) {
                oss << "║      Stack trace:\n";
                // Indent each line of stack trace
                std::istringstream trace_stream(v.stack_trace);
                std::string trace_line;
                while (std::getline(trace_stream, trace_line)) {
                    if (!trace_line.empty()) {
                        oss << "║        " << trace_line << "\n";
                    }
                }
            }
        }
        oss << "╠──────────────────────────────────────────────────────────────────────╣\n";
    }

    oss << "╚══════════════════════════════════════════════════════════════════════╝\n\n";

    return oss.str();
}

} // namespace memsentry

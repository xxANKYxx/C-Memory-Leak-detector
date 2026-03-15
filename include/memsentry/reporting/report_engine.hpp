// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — report_engine.hpp                                              ║
// ║  Strategy pattern: pluggable report formatters (Text/JSON/HTML)             ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/reporting/report_types.hpp"
#include "memsentry/core/config.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace memsentry {

// ─── Reporter interface (Strategy pattern) ───────────────────────────────────
class IReporter {
public:
    virtual ~IReporter() = default;
    virtual auto format_report(const FullReport& report) -> std::string = 0;
    [[nodiscard]] virtual auto name() const -> std::string = 0;
};

// ─── Report Engine ───────────────────────────────────────────────────────────
// Combines multiple detector reports, applies statistics, and outputs via
// pluggable reporters (Observer pattern for multi-output).
class ReportEngine {
public:
    ReportEngine();

    // Strategy: set the active reporter
    void set_reporter(std::unique_ptr<IReporter> reporter);

    // Observer: add additional output sinks
    using OutputSink = std::function<void(const std::string&)>;
    void add_output_sink(OutputSink sink);

    // Generate the full report
    auto generate(const std::vector<DetectorReport>& detector_reports,
                  const MemoryStatistics& stats) -> FullReport;

    // Format and output the report
    void output(const FullReport& report);

    // Convenience: generate and output in one call
    void generate_and_output(const std::vector<DetectorReport>& detector_reports,
                              const MemoryStatistics& stats);

private:
    std::unique_ptr<IReporter>  reporter_;
    std::vector<OutputSink>     sinks_;
};

} // namespace memsentry

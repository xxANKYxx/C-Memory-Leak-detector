// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — report_engine.cpp                                              ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/reporting/report_engine.hpp"
#include "memsentry/reporting/console_reporter.hpp"

#include <chrono>
#include <sstream>

namespace memsentry {

ReportEngine::ReportEngine()
    : reporter_(std::make_unique<ConsoleReporter>()) {}

void ReportEngine::set_reporter(std::unique_ptr<IReporter> reporter) {
    reporter_ = std::move(reporter);
}

void ReportEngine::add_output_sink(OutputSink sink) {
    sinks_.push_back(std::move(sink));
}

auto ReportEngine::generate(const std::vector<DetectorReport>& detector_reports,
                              const MemoryStatistics& stats) -> FullReport
{
    FullReport report;
    report.detector_reports = detector_reports;
    report.statistics       = stats;

    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&time_t));
    report.timestamp = time_buf;

    // Summary
    std::ostringstream oss;
    oss << "MemSentry Report — " << report.timestamp << "\n";
    oss << "Total violations: " << report.total_violations() << "\n";
    oss << "Status: " << (report.all_passed() ? "PASS" : "FAIL") << "\n";
    report.summary = oss.str();

    return report;
}

void ReportEngine::output(const FullReport& report) {
    if (!reporter_) return;

    std::string formatted = reporter_->format_report(report);

    // Send to all output sinks (Observer pattern)
    for (const auto& sink : sinks_) {
        sink(formatted);
    }

    // Default: if no sinks, print to stderr
    if (sinks_.empty()) {
        std::fputs(formatted.c_str(), stderr);
    }
}

void ReportEngine::generate_and_output(
    const std::vector<DetectorReport>& detector_reports,
    const MemoryStatistics& stats)
{
    auto report = generate(detector_reports, stats);
    output(report);
}

} // namespace memsentry

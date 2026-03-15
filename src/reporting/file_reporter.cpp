// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — file_reporter.cpp                                              ║
// ║  JSON and HTML report formatters                                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/reporting/file_reporter.hpp"
#include "memsentry/reporting/statistics.hpp"

#include <fstream>
#include <sstream>

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

// ─── JSON Reporter ───────────────────────────────────────────────────────────
auto JsonReporter::format_report(const FullReport& report) -> std::string {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"timestamp\": \"" << report.timestamp << "\",\n";
    oss << "  \"status\": \"" << (report.all_passed() ? "PASS" : "FAIL") << "\",\n";
    oss << "  \"total_violations\": " << report.total_violations() << ",\n";

    // Statistics
    oss << "  \"statistics\": {\n";
    oss << "    \"total_allocations\": " << report.statistics.total_allocations << ",\n";
    oss << "    \"total_deallocations\": " << report.statistics.total_deallocations << ",\n";
    oss << "    \"active_allocations\": " << report.statistics.active_allocations << ",\n";
    oss << "    \"peak_bytes\": " << report.statistics.peak_bytes_allocated << ",\n";
    oss << "    \"current_bytes\": " << report.statistics.current_bytes_in_use << ",\n";
    oss << "    \"leaked_allocations\": " << report.statistics.leaked_allocations << ",\n";
    oss << "    \"leaked_bytes\": " << report.statistics.leaked_bytes << ",\n";

    char frag_buf[32];
    std::snprintf(frag_buf, sizeof(frag_buf), "%.4f", report.statistics.fragmentation_index);
    oss << "    \"fragmentation_index\": " << frag_buf << "\n";
    oss << "  },\n";

    // Detectors
    oss << "  \"detectors\": [\n";
    for (std::size_t d = 0; d < report.detector_reports.size(); ++d) {
        const auto& det = report.detector_reports[d];
        oss << "    {\n";
        oss << "      \"name\": \"" << det.detector_name << "\",\n";
        oss << "      \"passed\": " << (det.passed ? "true" : "false") << ",\n";
        oss << "      \"violations\": [\n";

        for (std::size_t v = 0; v < det.violations.size(); ++v) {
            const auto& viol = det.violations[v];
            oss << "        {\n";
            oss << "          \"severity\": \"" << severity_name(viol.severity) << "\",\n";
            oss << "          \"category\": \"" << viol.category << "\",\n";
            oss << "          \"description\": \"" << viol.description << "\",\n";

#ifdef MEMSENTRY_HAS_FORMAT
            oss << std::format("          \"address\": \"0x{:X}\",\n", viol.address);
#else
            char addr_buf[32];
            std::snprintf(addr_buf, sizeof(addr_buf), "0x%llX",
                          static_cast<unsigned long long>(viol.address));
            oss << "          \"address\": \"" << addr_buf << "\",\n";
#endif
            oss << "          \"size\": " << viol.size << ",\n";
            oss << "          \"source_location\": \"" << viol.source_location << "\"\n";
            oss << "        }" << (v + 1 < det.violations.size() ? "," : "") << "\n";
        }

        oss << "      ]\n";
        oss << "    }" << (d + 1 < report.detector_reports.size() ? "," : "") << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";

    return oss.str();
}

// ─── HTML Reporter ───────────────────────────────────────────────────────────
auto HtmlReporter::format_report(const FullReport& report) -> std::string {
    std::ostringstream oss;

    oss << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>MemSentry Analysis Report</title>
<style>
  body { font-family: 'Segoe UI', Tahoma, sans-serif; margin: 2rem; background: #1a1a2e; color: #e0e0e0; }
  h1 { color: #00d4ff; border-bottom: 2px solid #00d4ff; padding-bottom: 0.5rem; }
  h2 { color: #ff6b6b; }
  .stats { display: grid; grid-template-columns: repeat(3, 1fr); gap: 1rem; margin: 1rem 0; }
  .stat-card { background: #16213e; padding: 1rem; border-radius: 8px; border: 1px solid #0f3460; }
  .stat-label { font-size: 0.85rem; color: #888; }
  .stat-value { font-size: 1.5rem; font-weight: bold; color: #00d4ff; }
  .pass { color: #2ecc71; } .fail { color: #e74c3c; }
  .violation { background: #16213e; padding: 1rem; margin: 0.5rem 0; border-radius: 8px;
               border-left: 4px solid #e74c3c; }
  .severity-WARNING { border-left-color: #f39c12; }
  .severity-INFO { border-left-color: #3498db; }
  .severity-CRITICAL { border-left-color: #e74c3c; }
  .severity-ERROR { border-left-color: #e74c3c; }
  table { width: 100%; border-collapse: collapse; margin: 1rem 0; }
  th, td { padding: 0.5rem; text-align: left; border-bottom: 1px solid #2a2a4a; }
  th { background: #0f3460; color: #00d4ff; }
  pre { background: #0d0d1a; padding: 1rem; border-radius: 4px; overflow-x: auto; }
</style>
</head>
<body>
)";

    oss << "<h1>MemSentry Analysis Report</h1>\n";
    oss << "<p>Generated: " << report.timestamp << "</p>\n";
    oss << "<p>Status: <span class=\""
        << (report.all_passed() ? "pass\">PASS" : "fail\">FAIL")
        << "</span></p>\n";

    // Statistics cards
    oss << "<div class=\"stats\">\n";
    auto stat_card = [&](const char* label, const std::string& value) {
        oss << "<div class=\"stat-card\"><div class=\"stat-label\">" << label
            << "</div><div class=\"stat-value\">" << value << "</div></div>\n";
    };

    stat_card("Total Allocations", std::to_string(report.statistics.total_allocations));
    stat_card("Peak Memory", StatisticsCollector::format_bytes(report.statistics.peak_bytes_allocated));
    stat_card("Leaked", std::to_string(report.statistics.leaked_allocations) + " allocs");
    stat_card("Active", std::to_string(report.statistics.active_allocations));
    stat_card("Current In Use", StatisticsCollector::format_bytes(report.statistics.current_bytes_in_use));

    char frag_str[32];
    std::snprintf(frag_str, sizeof(frag_str), "%.2f", report.statistics.fragmentation_index);
    stat_card("Fragmentation", frag_str);
    oss << "</div>\n";

    // Detector results
    oss << "<h2>Detector Results</h2>\n";
    oss << "<table><tr><th>Detector</th><th>Status</th><th>Violations</th></tr>\n";
    for (const auto& det : report.detector_reports) {
        oss << "<tr><td>" << det.detector_name << "</td><td class=\""
            << (det.passed ? "pass\">PASS" : "fail\">FAIL")
            << "</td><td>" << det.violation_count() << "</td></tr>\n";
    }
    oss << "</table>\n";

    // Violation details
    for (const auto& det : report.detector_reports) {
        if (det.violations.empty()) continue;

        oss << "<h2>" << det.detector_name << " — Violations</h2>\n";
        for (const auto& v : det.violations) {
            oss << "<div class=\"violation severity-" << severity_name(v.severity) << "\">\n";
            oss << "<strong>[" << severity_name(v.severity) << "] "
                << v.category << "</strong><br>\n";
            oss << v.description << "<br>\n";
            if (!v.source_location.empty()) {
                oss << "<small>at " << v.source_location << "</small><br>\n";
            }
            if (!v.stack_trace.empty()) {
                oss << "<pre>" << v.stack_trace << "</pre>\n";
            }
            oss << "</div>\n";
        }
    }

    oss << "</body></html>\n";
    return oss.str();
}

// ─── File output helper ──────────────────────────────────────────────────────
void FileReporter::write_to_file(const std::string& content,
                                   const std::filesystem::path& path) {
    // Ensure parent directories exist (C++17 filesystem)
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(path);
    if (file.is_open()) {
        file << content;
    }
}

} // namespace memsentry

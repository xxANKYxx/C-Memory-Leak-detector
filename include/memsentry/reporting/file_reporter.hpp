// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — file_reporter.hpp                                              ║
// ║  File-based report output (JSON / HTML / Text)                              ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/reporting/report_engine.hpp"

#include <filesystem>

namespace memsentry {

// ─── JSON Reporter ───────────────────────────────────────────────────────────
class JsonReporter : public IReporter {
public:
    auto format_report(const FullReport& report) -> std::string override;
    [[nodiscard]] auto name() const -> std::string override { return "JsonReporter"; }
};

// ─── HTML Reporter ───────────────────────────────────────────────────────────
class HtmlReporter : public IReporter {
public:
    auto format_report(const FullReport& report) -> std::string override;
    [[nodiscard]] auto name() const -> std::string override { return "HtmlReporter"; }
};

// ─── File output helper ──────────────────────────────────────────────────────
class FileReporter {
public:
    static void write_to_file(const std::string& content,
                               const std::filesystem::path& path);
};

} // namespace memsentry

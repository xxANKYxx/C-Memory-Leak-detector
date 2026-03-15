// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — console_reporter.hpp                                           ║
// ║  Text-formatted report output to console (std::format based)                ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/reporting/report_engine.hpp"

namespace memsentry {

class ConsoleReporter : public IReporter {
public:
    auto format_report(const FullReport& report) -> std::string override;
    [[nodiscard]] auto name() const -> std::string override { return "ConsoleReporter"; }
};

} // namespace memsentry

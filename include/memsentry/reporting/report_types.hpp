// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — report_types.hpp — Structured report data                      ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace memsentry {

// ─── Severity levels ─────────────────────────────────────────────────────────
enum class Severity {
    Info,
    Warning,
    Error,
    Critical
};

[[nodiscard]] constexpr auto severity_name(Severity s) noexcept -> const char* {
    switch (s) {
        case Severity::Info:     return "INFO";
        case Severity::Warning:  return "WARNING";
        case Severity::Error:    return "ERROR";
        case Severity::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

// ─── A single violation found by a detector ──────────────────────────────────
struct DetectorViolation {
    Severity        severity    = Severity::Error;
    std::string     category;       // e.g., "MEMORY_LEAK", "DOUBLE_FREE"
    std::string     description;
    std::uintptr_t  address     = 0;
    std::size_t     size        = 0;
    std::string     stack_trace;    // formatted stack trace string
    std::string     source_location;
};

// ─── Report from a single detector ──────────────────────────────────────────
struct DetectorReport {
    std::string                     detector_name;
    std::vector<DetectorViolation>  violations;
    bool                            passed = true;  // true if no violations

    [[nodiscard]] auto violation_count() const -> std::size_t {
        return violations.size();
    }
};

// ─── Memory statistics ───────────────────────────────────────────────────────
struct MemoryStatistics {
    std::size_t total_allocations       = 0;
    std::size_t total_deallocations     = 0;
    std::size_t active_allocations      = 0;
    std::size_t total_bytes_allocated   = 0;
    std::size_t peak_bytes_allocated    = 0;
    std::size_t current_bytes_in_use    = 0;
    std::size_t leaked_allocations      = 0;
    std::size_t leaked_bytes            = 0;
    double      fragmentation_index     = 0.0;

    // Allocation histogram (size class → count)
    struct SizeClassBucket {
        std::size_t min_size;
        std::size_t max_size;
        std::size_t count;
    };
    std::vector<SizeClassBucket> histogram;
};

// ─── Full report combining all detectors ─────────────────────────────────────
struct FullReport {
    std::vector<DetectorReport> detector_reports;
    MemoryStatistics            statistics;
    std::string                 timestamp;
    std::string                 summary;

    [[nodiscard]] auto total_violations() const -> std::size_t {
        std::size_t total = 0;
        for (const auto& r : detector_reports) {
            total += r.violation_count();
        }
        return total;
    }

    [[nodiscard]] auto all_passed() const -> bool {
        for (const auto& r : detector_reports) {
            if (!r.passed) return false;
        }
        return true;
    }
};

} // namespace memsentry

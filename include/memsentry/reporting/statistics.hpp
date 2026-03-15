// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — statistics.hpp                                                 ║
// ║  Memory statistics aggregation and computation                              ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/reporting/report_types.hpp"
#include "memsentry/core/allocation_tracker.hpp"
#include "memsentry/detectors/fragmentation_analyzer.hpp"

namespace memsentry {

class StatisticsCollector {
public:
    [[nodiscard]] static auto collect(
        const AllocationTracker& tracker,
        const FragmentationAnalyzer* frag_analyzer = nullptr
    ) -> MemoryStatistics;

    [[nodiscard]] static auto format_statistics(const MemoryStatistics& stats)
        -> std::string;

    // Format bytes to human-readable (e.g., "1.5 MB")
    [[nodiscard]] static auto format_bytes(std::size_t bytes) -> std::string;
};

} // namespace memsentry

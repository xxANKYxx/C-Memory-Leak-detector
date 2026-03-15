// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — fragmentation_analyzer.hpp                                     ║
// ║  Analyzes heap fragmentation: histogram, fragmentation index, trends.       ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/detectors/detector_base.hpp"
#include "memsentry/reporting/report_types.hpp"

#include <map>
#include <mutex>

namespace memsentry {

class FragmentationAnalyzer : public DetectorMixin<FragmentationAnalyzer> {
public:
    FragmentationAnalyzer() = default;

    [[nodiscard]] static auto detector_name() -> std::string { return "FragmentationAnalyzer"; }

    void on_allocate(const AllocationEvent& evt);
    void on_deallocate(const DeallocationEvent& evt);
    void on_access(const AccessEvent& evt);

    // ── Analysis ─────────────────────────────────────────────────────────────
    [[nodiscard]] auto compute_fragmentation_index() const -> double;
    [[nodiscard]] auto compute_histogram() const -> std::vector<MemoryStatistics::SizeClassBucket>;
    [[nodiscard]] auto get_statistics() const -> MemoryStatistics;

    [[nodiscard]] auto generate_report() -> DetectorReport;
    void reset();

private:
    mutable std::mutex mutex_;

    // Size class buckets: [1-16], [17-64], [65-256], [257-1024], [1025-4096], [4097+]
    static constexpr std::size_t BUCKET_BOUNDARIES[] = {16, 64, 256, 1024, 4096};
    static constexpr std::size_t NUM_BUCKETS = 6;  // 5 boundaries + 1 overflow

    std::size_t bucket_counts_[NUM_BUCKETS] = {};

    // Track active allocations for fragmentation analysis
    std::map<std::uintptr_t, std::size_t> active_allocs_;  // sorted by address

    std::size_t total_allocations_   = 0;
    std::size_t total_deallocations_ = 0;
    std::size_t total_bytes_         = 0;
    std::size_t current_bytes_       = 0;
    std::size_t peak_bytes_          = 0;

    [[nodiscard]] auto bucket_for_size(std::size_t size) const -> std::size_t;
};

static_assert(MemoryDetector<FragmentationAnalyzer>);

} // namespace memsentry

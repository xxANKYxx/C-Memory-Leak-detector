// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — fragmentation_analyzer.cpp                                     ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/detectors/fragmentation_analyzer.hpp"

#include <algorithm>
#include <cmath>

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

void FragmentationAnalyzer::on_allocate(const AllocationEvent& evt) {
    std::lock_guard lock(mutex_);

    ++total_allocations_;
    total_bytes_ += evt.size;
    current_bytes_ += evt.size;
    peak_bytes_ = std::max(peak_bytes_, current_bytes_);

    active_allocs_[evt.address] = evt.size;
    ++bucket_counts_[bucket_for_size(evt.size)];
}

void FragmentationAnalyzer::on_deallocate(const DeallocationEvent& evt) {
    std::lock_guard lock(mutex_);

    auto it = active_allocs_.find(evt.address);
    if (it != active_allocs_.end()) {
        ++total_deallocations_;
        current_bytes_ -= it->second;
        active_allocs_.erase(it);
    }
}

void FragmentationAnalyzer::on_access(const AccessEvent& /*evt*/) {
    // Not used for fragmentation analysis
}

auto FragmentationAnalyzer::compute_fragmentation_index() const -> double {
    std::lock_guard lock(mutex_);

    if (active_allocs_.empty()) return 0.0;

    // Fragmentation index = 1 - (largest_contiguous_free / total_free)
    // For our tracking purposes, we measure allocation spread:
    // higher spread across address space = more fragmentation

    auto first_addr = active_allocs_.begin()->first;
    auto last_it    = std::prev(active_allocs_.end());
    auto last_addr  = last_it->first + last_it->second;

    std::size_t address_span = last_addr - first_addr;
    if (address_span == 0) return 0.0;

    // Compute total used bytes in the span
    std::size_t used = 0;
    for (const auto& [addr, size] : active_allocs_) {
        used += size;
    }

    // Fragmentation = 1 - (used / span)
    double frag = 1.0 - (static_cast<double>(used) / static_cast<double>(address_span));
    return std::clamp(frag, 0.0, 1.0);
}

auto FragmentationAnalyzer::compute_histogram() const
    -> std::vector<MemoryStatistics::SizeClassBucket>
{
    std::lock_guard lock(mutex_);

    std::vector<MemoryStatistics::SizeClassBucket> histogram;
    histogram.reserve(NUM_BUCKETS);

    std::size_t prev = 1;
    for (std::size_t i = 0; i < NUM_BUCKETS; ++i) {
        std::size_t max_size = (i < NUM_BUCKETS - 1) ? BUCKET_BOUNDARIES[i] : SIZE_MAX;
        histogram.push_back({
            .min_size = prev,
            .max_size = max_size,
            .count    = bucket_counts_[i]
        });
        if (i < NUM_BUCKETS - 1) {
            prev = BUCKET_BOUNDARIES[i] + 1;
        }
    }

    return histogram;
}

auto FragmentationAnalyzer::get_statistics() const -> MemoryStatistics {
    MemoryStatistics stats;
    stats.total_allocations     = total_allocations_;
    stats.total_deallocations   = total_deallocations_;
    stats.active_allocations    = active_allocs_.size();
    stats.total_bytes_allocated = total_bytes_;
    stats.peak_bytes_allocated  = peak_bytes_;
    stats.current_bytes_in_use  = current_bytes_;
    stats.fragmentation_index   = compute_fragmentation_index();
    stats.histogram             = compute_histogram();
    return stats;
}

auto FragmentationAnalyzer::generate_report() -> DetectorReport {
    std::lock_guard lock(mutex_);
    DetectorReport report;
    report.detector_name = name();

    double frag_index = compute_fragmentation_index();

    // Report high fragmentation as a warning
    if (frag_index > 0.5) {
        DetectorViolation violation;
        violation.severity = Severity::Warning;
        violation.category = "HIGH_FRAGMENTATION";

#ifdef MEMSENTRY_HAS_FORMAT
        violation.description = std::format(
            "Memory fragmentation index is {:.2f} (threshold: 0.50). "
            "{} active allocations spanning {} bytes",
            frag_index, active_allocs_.size(), current_bytes_
        );
#else
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Memory fragmentation index is %.2f (threshold: 0.50). "
            "%zu active allocations spanning %zu bytes",
            frag_index, active_allocs_.size(), current_bytes_);
        violation.description = buf;
#endif

        report.violations.push_back(std::move(violation));
    }

    report.passed = report.violations.empty();
    return report;
}

void FragmentationAnalyzer::reset() {
    std::lock_guard lock(mutex_);
    active_allocs_.clear();
    total_allocations_   = 0;
    total_deallocations_ = 0;
    total_bytes_         = 0;
    current_bytes_       = 0;
    peak_bytes_          = 0;
    std::fill(std::begin(bucket_counts_), std::end(bucket_counts_), 0);
    clear_violations();
}

auto FragmentationAnalyzer::bucket_for_size(std::size_t size) const -> std::size_t {
    for (std::size_t i = 0; i < NUM_BUCKETS - 1; ++i) {
        if (size <= BUCKET_BOUNDARIES[i]) return i;
    }
    return NUM_BUCKETS - 1;
}

} // namespace memsentry

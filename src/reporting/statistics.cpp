// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — statistics.cpp                                                 ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/reporting/statistics.hpp"

#include <sstream>

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

auto StatisticsCollector::collect(
    const AllocationTracker& tracker,
    const FragmentationAnalyzer* frag_analyzer) -> MemoryStatistics
{
    MemoryStatistics stats;

    stats.total_bytes_allocated = tracker.total_allocated_bytes();
    stats.peak_bytes_allocated  = tracker.peak_allocated_bytes();
    stats.active_allocations    = tracker.active_allocation_count();

    auto leaked = tracker.get_leaked_allocations();
    stats.leaked_allocations = leaked.size();

    std::size_t leaked_bytes = 0;
    for (const auto& record : leaked) {
        leaked_bytes += record.size;
    }
    stats.leaked_bytes = leaked_bytes;

    if (frag_analyzer) {
        auto frag_stats          = frag_analyzer->get_statistics();
        stats.total_allocations  = frag_stats.total_allocations;
        stats.total_deallocations= frag_stats.total_deallocations;
        stats.current_bytes_in_use = frag_stats.current_bytes_in_use;
        stats.fragmentation_index= frag_stats.fragmentation_index;
        stats.histogram          = frag_stats.histogram;
    }

    return stats;
}

auto StatisticsCollector::format_statistics(const MemoryStatistics& stats) -> std::string {
    std::ostringstream oss;

#ifdef MEMSENTRY_HAS_FORMAT
    oss << std::format("  Total allocations   : {}\n", stats.total_allocations);
    oss << std::format("  Total deallocations : {}\n", stats.total_deallocations);
    oss << std::format("  Active allocations  : {}\n", stats.active_allocations);
    oss << std::format("  Peak memory         : {}\n", format_bytes(stats.peak_bytes_allocated));
    oss << std::format("  Current in use      : {}\n", format_bytes(stats.current_bytes_in_use));
    oss << std::format("  Leaked              : {} allocations ({})\n",
                       stats.leaked_allocations, format_bytes(stats.leaked_bytes));
    oss << std::format("  Fragmentation index : {:.4f}\n", stats.fragmentation_index);
#else
    oss << "  Total allocations   : " << stats.total_allocations << "\n";
    oss << "  Total deallocations : " << stats.total_deallocations << "\n";
    oss << "  Active allocations  : " << stats.active_allocations << "\n";
    oss << "  Peak memory         : " << format_bytes(stats.peak_bytes_allocated) << "\n";
    oss << "  Current in use      : " << format_bytes(stats.current_bytes_in_use) << "\n";
    oss << "  Leaked              : " << stats.leaked_allocations << " allocations ("
        << format_bytes(stats.leaked_bytes) << ")\n";
    char frag_buf[32];
    std::snprintf(frag_buf, sizeof(frag_buf), "%.4f", stats.fragmentation_index);
    oss << "  Fragmentation index : " << frag_buf << "\n";
#endif

    // Histogram
    if (!stats.histogram.empty()) {
        oss << "\n  Allocation Size Histogram:\n";
        for (const auto& bucket : stats.histogram) {
            if (bucket.count == 0) continue;
#ifdef MEMSENTRY_HAS_FORMAT
            if (bucket.max_size == SIZE_MAX) {
                oss << std::format("    [{:>6}+]      : {} allocations\n",
                                   format_bytes(bucket.min_size), bucket.count);
            } else {
                oss << std::format("    [{:>6} - {:>6}] : {} allocations\n",
                                   format_bytes(bucket.min_size),
                                   format_bytes(bucket.max_size),
                                   bucket.count);
            }
#else
            if (bucket.max_size == SIZE_MAX) {
                oss << "    [" << format_bytes(bucket.min_size) << "+] : "
                    << bucket.count << " allocations\n";
            } else {
                oss << "    [" << format_bytes(bucket.min_size) << " - "
                    << format_bytes(bucket.max_size) << "] : "
                    << bucket.count << " allocations\n";
            }
#endif
        }
    }

    return oss.str();
}

auto StatisticsCollector::format_bytes(std::size_t bytes) -> std::string {
    constexpr std::size_t KB = 1024;
    constexpr std::size_t MB = 1024 * KB;
    constexpr std::size_t GB = 1024 * MB;

    char buf[64];
    if (bytes >= GB) {
        std::snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / static_cast<double>(GB));
    } else if (bytes >= MB) {
        std::snprintf(buf, sizeof(buf), "%.2f MB", static_cast<double>(bytes) / static_cast<double>(MB));
    } else if (bytes >= KB) {
        std::snprintf(buf, sizeof(buf), "%.2f KB", static_cast<double>(bytes) / static_cast<double>(KB));
    } else {
        std::snprintf(buf, sizeof(buf), "%zu B", bytes);
    }
    return std::string(buf);
}

} // namespace memsentry

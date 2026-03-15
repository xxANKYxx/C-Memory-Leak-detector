// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — leak_detector.hpp                                              ║
// ║  Detects allocations that were never freed (at shutdown or on-demand).       ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/detectors/detector_base.hpp"
#include "memsentry/core/allocation_tracker.hpp"

#include <mutex>

namespace memsentry {

class LeakDetector : public DetectorMixin<LeakDetector> {
public:
    LeakDetector() = default;

    [[nodiscard]] static auto detector_name() -> std::string { return "LeakDetector"; }

    void on_allocate(const AllocationEvent& evt);
    void on_deallocate(const DeallocationEvent& evt);
    void on_access(const AccessEvent& evt);

    [[nodiscard]] auto generate_report() -> DetectorReport;
    void reset();

private:
    mutable std::mutex mutex_;
    std::size_t allocation_count_   = 0;
    std::size_t deallocation_count_ = 0;
};

// Verify the concept is satisfied
static_assert(MemoryDetector<LeakDetector>, "LeakDetector must satisfy MemoryDetector concept");

} // namespace memsentry

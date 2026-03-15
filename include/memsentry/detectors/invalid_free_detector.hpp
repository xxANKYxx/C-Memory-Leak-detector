// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — invalid_free_detector.hpp                                      ║
// ║  Detects freeing pointers that were not returned by an allocator.           ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/detectors/detector_base.hpp"

#include <mutex>
#include <unordered_set>

namespace memsentry {

class InvalidFreeDetector : public DetectorMixin<InvalidFreeDetector> {
public:
    InvalidFreeDetector() = default;

    [[nodiscard]] static auto detector_name() -> std::string { return "InvalidFreeDetector"; }

    void on_allocate(const AllocationEvent& evt);
    void on_deallocate(const DeallocationEvent& evt);
    void on_access(const AccessEvent& evt);

    [[nodiscard]] auto generate_report() -> DetectorReport;
    void reset();

private:
    mutable std::mutex mutex_;
    std::unordered_set<std::uintptr_t> known_allocations_;
};

static_assert(MemoryDetector<InvalidFreeDetector>);

} // namespace memsentry

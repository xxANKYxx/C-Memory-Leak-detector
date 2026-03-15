// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — double_free_detector.hpp                                       ║
// ║  Detects freeing the same pointer more than once.                           ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/detectors/detector_base.hpp"

#include <mutex>
#include <unordered_set>

namespace memsentry {

class DoubleFreeDetector : public DetectorMixin<DoubleFreeDetector> {
public:
    DoubleFreeDetector() = default;

    [[nodiscard]] static auto detector_name() -> std::string { return "DoubleFreeDetector"; }

    void on_allocate(const AllocationEvent& evt);
    void on_deallocate(const DeallocationEvent& evt);
    void on_access(const AccessEvent& evt);

    [[nodiscard]] auto generate_report() -> DetectorReport;
    void reset();

private:
    mutable std::mutex mutex_;
    // Track which pointers have been freed: second free = violation
    std::unordered_set<std::uintptr_t> freed_pointers_;
};

static_assert(MemoryDetector<DoubleFreeDetector>);

} // namespace memsentry

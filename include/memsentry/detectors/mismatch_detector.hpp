// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — mismatch_detector.hpp                                          ║
// ║  Detects allocation/deallocation type mismatches (new/delete[], etc.)        ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/detectors/detector_base.hpp"

#include <mutex>
#include <unordered_map>

namespace memsentry {

class MismatchDetector : public DetectorMixin<MismatchDetector> {
public:
    MismatchDetector() = default;

    [[nodiscard]] static auto detector_name() -> std::string { return "MismatchDetector"; }

    void on_allocate(const AllocationEvent& evt);
    void on_deallocate(const DeallocationEvent& evt);
    void on_access(const AccessEvent& evt);

    [[nodiscard]] auto generate_report() -> DetectorReport;
    void reset();

private:
    mutable std::mutex mutex_;
    // Track allocation types: address → AllocationType
    std::unordered_map<std::uintptr_t, AllocationType> alloc_types_;
};

static_assert(MemoryDetector<MismatchDetector>);

} // namespace memsentry

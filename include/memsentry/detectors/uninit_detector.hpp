// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — uninit_detector.hpp                                            ║
// ║  Detects reads from uninitialized memory (allocated but never written).     ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/detectors/detector_base.hpp"
#include "memsentry/core/shadow_memory.hpp"

#include <mutex>

namespace memsentry {

class UninitDetector : public DetectorMixin<UninitDetector> {
public:
    UninitDetector() = default;

    [[nodiscard]] static auto detector_name() -> std::string { return "UninitDetector"; }

    void on_allocate(const AllocationEvent& evt);
    void on_deallocate(const DeallocationEvent& evt);
    void on_access(const AccessEvent& evt);

    [[nodiscard]] auto generate_report() -> DetectorReport;
    void reset();

private:
    mutable std::mutex mutex_;
};

static_assert(MemoryDetector<UninitDetector>);

} // namespace memsentry

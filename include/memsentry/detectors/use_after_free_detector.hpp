// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — use_after_free_detector.hpp                                    ║
// ║  Detects access to memory after it has been freed.                          ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/detectors/detector_base.hpp"

#include <mutex>
#include <unordered_map>

namespace memsentry {

class UseAfterFreeDetector : public DetectorMixin<UseAfterFreeDetector> {
public:
    UseAfterFreeDetector() = default;

    [[nodiscard]] static auto detector_name() -> std::string { return "UseAfterFreeDetector"; }

    void on_allocate(const AllocationEvent& evt);
    void on_deallocate(const DeallocationEvent& evt);
    void on_access(const AccessEvent& evt);

    [[nodiscard]] auto generate_report() -> DetectorReport;
    void reset();

private:
    mutable std::mutex mutex_;
    // Map of freed addresses → size (quarantine zone)
    std::unordered_map<std::uintptr_t, std::size_t> quarantine_;
};

static_assert(MemoryDetector<UseAfterFreeDetector>);

} // namespace memsentry

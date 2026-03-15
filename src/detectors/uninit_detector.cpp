// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — uninit_detector.cpp                                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/detectors/uninit_detector.hpp"

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

void UninitDetector::on_allocate(const AllocationEvent& /*evt*/) {
    // Shadow memory is set to Uninitialized by the allocator hooks
}

void UninitDetector::on_deallocate(const DeallocationEvent& /*evt*/) {
    // Nothing to do
}

void UninitDetector::on_access(const AccessEvent& evt) {
    if (!is_enabled()) return;

    // Only check reads — writes initialize the memory
    if (evt.is_write) {
        // Mark bytes as initialized
        auto& shadow = ShadowMemory::instance();
        shadow.set_range(evt.address, evt.size, ByteState::Initialized);
        return;
    }

    // Check if any byte in the accessed range is still uninitialized
    std::lock_guard lock(mutex_);
    auto& shadow = ShadowMemory::instance();

    for (std::size_t i = 0; i < evt.size; ++i) {
        auto state = shadow.get_state(evt.address + i);
        if (state == ByteState::Uninitialized) {
            DetectorViolation violation;
            violation.severity = Severity::Warning;
            violation.category = "UNINITIALIZED_READ";
            violation.address  = evt.address + i;
            violation.size     = 1;

#ifdef MEMSENTRY_HAS_FORMAT
            violation.description = std::format(
                "Read of uninitialized memory at 0x{:X} (offset +{} in access of {} bytes)",
                evt.address + i, i, evt.size
            );
#else
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "Read of uninitialized memory at 0x%llX (offset +%zu in access of %zu bytes)",
                static_cast<unsigned long long>(evt.address + i), i, evt.size);
            violation.description = buf;
#endif

            add_violation(std::move(violation));
            return;  // Report once per access, not per byte
        }
    }
}

auto UninitDetector::generate_report() -> DetectorReport {
    std::lock_guard lock(mutex_);
    DetectorReport report;
    report.detector_name = name();
    report.violations = violations();
    report.passed = violations().empty();
    return report;
}

void UninitDetector::reset() {
    std::lock_guard lock(mutex_);
    clear_violations();
}

} // namespace memsentry

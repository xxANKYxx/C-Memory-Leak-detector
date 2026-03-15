// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — use_after_free_detector.cpp                                    ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/detectors/use_after_free_detector.hpp"
#include "memsentry/core/allocation_tracker.hpp"

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

void UseAfterFreeDetector::on_allocate(const AllocationEvent& evt) {
    std::lock_guard lock(mutex_);
    // Remove from quarantine if re-allocated at same address
    quarantine_.erase(evt.address);
}

void UseAfterFreeDetector::on_deallocate(const DeallocationEvent& evt) {
    std::lock_guard lock(mutex_);
    // Add freed pointer to quarantine
    // Size is looked up from tracker — use 0 as fallback
    auto record = AllocationTracker::instance().find(evt.address);
    std::size_t size = record ? record->size : 0;
    quarantine_[evt.address] = size;
}

void UseAfterFreeDetector::on_access(const AccessEvent& evt) {
    std::lock_guard lock(mutex_);

    // Check if the accessed address falls within any quarantined (freed) region
    for (const auto& [freed_addr, freed_size] : quarantine_) {
        if (evt.address >= freed_addr && evt.address < freed_addr + freed_size) {
            DetectorViolation violation;
            violation.severity    = Severity::Critical;
            violation.category    = "USE_AFTER_FREE";
            violation.address     = evt.address;
            violation.size        = evt.size;

#ifdef MEMSENTRY_HAS_FORMAT
            violation.description = std::format(
                "Use-after-free: {} at 0x{:X} (freed block: 0x{:X}, {} bytes)",
                evt.is_write ? "write" : "read",
                evt.address,
                freed_addr,
                freed_size
            );
#else
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "Use-after-free: %s at 0x%llX (freed block: 0x%llX, %zu bytes)",
                evt.is_write ? "write" : "read",
                static_cast<unsigned long long>(evt.address),
                static_cast<unsigned long long>(freed_addr),
                freed_size);
            violation.description = buf;
#endif

            add_violation(std::move(violation));
            return;
        }
    }
}

auto UseAfterFreeDetector::generate_report() -> DetectorReport {
    std::lock_guard lock(mutex_);
    DetectorReport report;
    report.detector_name = name();
    report.violations = violations();
    report.passed = violations().empty();
    return report;
}

void UseAfterFreeDetector::reset() {
    std::lock_guard lock(mutex_);
    quarantine_.clear();
    clear_violations();
}

} // namespace memsentry

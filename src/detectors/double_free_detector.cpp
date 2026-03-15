// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — double_free_detector.cpp                                       ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/detectors/double_free_detector.hpp"

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

void DoubleFreeDetector::on_allocate(const AllocationEvent& evt) {
    std::lock_guard lock(mutex_);
    // If a pointer is reallocated at the same address, remove from freed set
    freed_pointers_.erase(evt.address);
}

void DoubleFreeDetector::on_deallocate(const DeallocationEvent& evt) {
    std::lock_guard lock(mutex_);

    if (freed_pointers_.contains(evt.address)) {
        // VIOLATION: this pointer was already freed
        DetectorViolation violation;
        violation.severity    = Severity::Critical;
        violation.category    = "DOUBLE_FREE";
        violation.address     = evt.address;

#ifdef MEMSENTRY_HAS_FORMAT
        violation.description = std::format(
            "Double free detected: pointer 0x{:X} was already freed",
            evt.address
        );
#else
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Double free detected: pointer 0x%llX was already freed",
            static_cast<unsigned long long>(evt.address));
        violation.description = buf;
#endif

        add_violation(std::move(violation));
    } else {
        freed_pointers_.insert(evt.address);
    }
}

void DoubleFreeDetector::on_access(const AccessEvent& /*evt*/) {
    // DoubleFreeDetector does not monitor access
}

auto DoubleFreeDetector::generate_report() -> DetectorReport {
    std::lock_guard lock(mutex_);
    DetectorReport report;
    report.detector_name = name();
    report.violations = violations();
    report.passed = violations().empty();
    return report;
}

void DoubleFreeDetector::reset() {
    std::lock_guard lock(mutex_);
    freed_pointers_.clear();
    clear_violations();
}

} // namespace memsentry

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — invalid_free_detector.cpp                                      ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/detectors/invalid_free_detector.hpp"

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

void InvalidFreeDetector::on_allocate(const AllocationEvent& evt) {
    std::lock_guard lock(mutex_);
    known_allocations_.insert(evt.address);
}

void InvalidFreeDetector::on_deallocate(const DeallocationEvent& evt) {
    std::lock_guard lock(mutex_);

    if (!known_allocations_.contains(evt.address)) {
        DetectorViolation violation;
        violation.severity = Severity::Critical;
        violation.category = "INVALID_FREE";
        violation.address  = evt.address;

#ifdef MEMSENTRY_HAS_FORMAT
        violation.description = std::format(
            "Invalid free: pointer 0x{:X} was never allocated by a tracked allocator",
            evt.address
        );
#else
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Invalid free: pointer 0x%llX was never allocated by a tracked allocator",
            static_cast<unsigned long long>(evt.address));
        violation.description = buf;
#endif

        add_violation(std::move(violation));
    } else {
        known_allocations_.erase(evt.address);
    }
}

void InvalidFreeDetector::on_access(const AccessEvent& /*evt*/) {
    // Not used for invalid free detection
}

auto InvalidFreeDetector::generate_report() -> DetectorReport {
    std::lock_guard lock(mutex_);
    DetectorReport report;
    report.detector_name = name();
    report.violations = violations();
    report.passed = violations().empty();
    return report;
}

void InvalidFreeDetector::reset() {
    std::lock_guard lock(mutex_);
    known_allocations_.clear();
    clear_violations();
}

} // namespace memsentry

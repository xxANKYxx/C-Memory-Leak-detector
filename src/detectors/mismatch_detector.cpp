// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — mismatch_detector.cpp                                          ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/detectors/mismatch_detector.hpp"

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

void MismatchDetector::on_allocate(const AllocationEvent& evt) {
    std::lock_guard lock(mutex_);
    alloc_types_[evt.address] = evt.type;
}

void MismatchDetector::on_deallocate(const DeallocationEvent& evt) {
    std::lock_guard lock(mutex_);

    auto it = alloc_types_.find(evt.address);
    if (it == alloc_types_.end()) return;

    // Convert DeallocationType to expected AllocationType
    auto alloc_type = it->second;

    if (!types_match(alloc_type, evt.type)) {
        DetectorViolation violation;
        violation.severity = Severity::Error;
        violation.category = "ALLOC_DEALLOC_MISMATCH";
        violation.address  = evt.address;

#ifdef MEMSENTRY_HAS_FORMAT
        violation.description = std::format(
            "Allocation/deallocation mismatch at 0x{:X}: "
            "allocated with '{}', freed with '{}' (expected '{}')",
            evt.address,
            allocation_type_name(alloc_type),
            deallocation_type_name(evt.type),
            expected_deallocation(alloc_type)
        );
#else
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "Allocation/deallocation mismatch at 0x%llX: "
            "allocated with '%s', freed with '%s' (expected '%s')",
            static_cast<unsigned long long>(evt.address),
            allocation_type_name(alloc_type),
            deallocation_type_name(evt.type),
            expected_deallocation(alloc_type));
        violation.description = buf;
#endif

        add_violation(std::move(violation));
    }

    alloc_types_.erase(it);
}

void MismatchDetector::on_access(const AccessEvent& /*evt*/) {
    // MismatchDetector does not monitor access
}

auto MismatchDetector::generate_report() -> DetectorReport {
    std::lock_guard lock(mutex_);
    DetectorReport report;
    report.detector_name = name();
    report.violations = violations();
    report.passed = violations().empty();
    return report;
}

void MismatchDetector::reset() {
    std::lock_guard lock(mutex_);
    alloc_types_.clear();
    clear_violations();
}

} // namespace memsentry
